// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "MediaEngineWrapper.h"
#include "MediaFoundationHelpers.h"
#include <Audioclient.h>
#include <d3d11.h>

using namespace Microsoft::WRL;

namespace media
{
    // Public methods
    HRESULT MediaEngineWrapper::RuntimeClassInitialize(IMFSourceReader* sourceReader,
        IMFDXGIDeviceManager* dxgiDeviceManager,
        std::function<void()> initializedCB, 
        std::function<void(MF_MEDIA_ENGINE_ERR, HRESULT)> errorCB, 
        std::function<void()> playbackEndedCB,
        uint32_t width, uint32_t height)
    {
        m_dxgiDeviceManager = dxgiDeviceManager;
        m_initializedCB = initializedCB;
        m_errorCB = errorCB;
        m_playbackEndedCB = playbackEndedCB;
        m_width = width;
        m_height = height;

        RunSyncInMTA([&]() {
            HRESULT hr = CreateMediaEngine(sourceReader);
            if (S_OK != hr)
                LOG_HR_MSG(hr, "CreateMediaEngine error (%d)", hr);
        });

        return S_OK;
    }

    void MediaEngineWrapper::StartPlayingFrom(uint64_t timeStamp)
    {
        RunSyncInMTA([&]() {
            auto lock = m_lock.lock();
            PROPVARIANT startTime = {};
            startTime.vt = VT_I8;
            startTime.hVal.QuadPart = timeStamp;
            m_sourceReader->SetCurrentPosition(GUID_NULL, startTime);

            const double timestampInSeconds = ConvertHnsToSeconds(timeStamp);
            m_mediaEngine->SetCurrentTime(timestampInSeconds);
            m_mediaEngine->Play();
        });
    }

    HANDLE MediaEngineWrapper::GetSurfaceHandle()
    {
        HANDLE surfaceHandle = INVALID_HANDLE_VALUE;
        RunSyncInMTA([&]() {
            surfaceHandle = m_dcompSurfaceHandle.get();
        });
        return surfaceHandle;
    }

    void MediaEngineWrapper::WindowUpdate(uint32_t width, uint32_t height)
    {
        RunSyncInMTA([&]() {
            auto lock = m_lock.lock();
            if (width != m_width || height != m_height)
            {
                m_width = width;
                m_height = height;
            }

            if (m_mediaEngine)
            {
                RECT destRect{ 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
                ComPtr<IMFMediaEngineEx> mediaEngineEx;
                m_mediaEngine.As(&mediaEngineEx);
                HRESULT hr = mediaEngineEx->UpdateVideoStream(nullptr, &destRect, nullptr);
                if (S_OK != hr)
                    LOG_HR_MSG(hr, "UpdateVideoStream error");
            }
        });
    }

    // Internal methods
    HRESULT MediaEngineWrapper::CreateMediaEngine(IMFSourceReader* sourceReader)
    {
        ComPtr<IMFMediaEngineClassFactory> classFactory;
        ComPtr<IMFAttributes> creationAttributes;

        RETURN_IF_FAILED(MakeAndInitialize<MediaEngineExtension>(&m_mediaEngineExtension));
        RETURN_IF_FAILED(MFCreateAttributes(&creationAttributes, 7));

        auto onLoadedCB = std::bind(&MediaEngineWrapper::OnLoaded, this);
        auto onErrorCB = std::bind(&MediaEngineWrapper::OnError, this, std::placeholders::_1, std::placeholders::_2);
        auto onPlaybackEndedCB = std::bind(&MediaEngineWrapper::OnPlaybackEnded, this);
        RETURN_IF_FAILED(MakeAndInitialize<MediaEngineNotifyImpl>(&m_mediaEngineNotifier, onLoadedCB, onErrorCB, onPlaybackEndedCB));
        RETURN_IF_FAILED(creationAttributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, m_mediaEngineNotifier.Get()));
        RETURN_IF_FAILED(creationAttributes->SetUINT32(MF_MEDIA_ENGINE_CONTENT_PROTECTION_FLAGS, MF_MEDIA_ENGINE_ENABLE_PROTECTED_CONTENT));
        
        // Should set a valid Application ID for production.
        GUID MediaEngineTelemetryApplicationId = {};
        MediaEngineTelemetryApplicationId.Data1 = 0;
        creationAttributes->SetGUID(MF_MEDIA_ENGINE_TELEMETRY_APPLICATION_ID, MediaEngineTelemetryApplicationId);

        RETURN_IF_FAILED(creationAttributes->SetUINT32(MF_MEDIA_ENGINE_AUDIO_CATEGORY, AudioCategory_Media));

        if (m_dxgiDeviceManager != nullptr)
            RETURN_IF_FAILED(creationAttributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, m_dxgiDeviceManager.Get()));

        RETURN_IF_FAILED(creationAttributes->SetUnknown(MF_MEDIA_ENGINE_EXTENSION, m_mediaEngineExtension.Get()));

        RETURN_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&classFactory)));
        RETURN_IF_FAILED(classFactory->CreateInstance(0, creationAttributes.Get(), &m_mediaEngine));

        RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationSourceWrapper>(&m_source, sourceReader));
        m_sourceReader = sourceReader;

        ComPtr<IUnknown> sourceUnknown;
        RETURN_IF_FAILED(m_source.As(&sourceUnknown));
        m_mediaEngineExtension->SetMediaSource(sourceUnknown.Get());
        
        // Set Custom source to Media engine
        if (!m_hasSetSource)
        {
            wil::unique_bstr source = wil::make_bstr(L"MFRendererSrc");
            RETURN_IF_FAILED(m_mediaEngine->SetSource(source.get()));
            m_hasSetSource = true;
        }
        else
        {
            RETURN_IF_FAILED(m_mediaEngine->Load());
        }
        return S_OK;
    }

    // Callback methods
    void MediaEngineWrapper::OnLoaded()
    {
        // Call asynchronously to prevent potential deadlock due to lock inversion
        // Ensure that the callback lambda holds a reference to this object to ensure that it isn't destroyed while there is a pending callback
        winrt::com_ptr<MediaEngineWrapper> ref;
        ref.copy_from(this);
        
        media::MFPutWorkItem([&, ref]() {
            {
                auto lock = m_lock.lock();
                // Put media engine into DCOMP mode (as opposed to frame server mode) and
                // obtain a handle to the DCOMP surface handle
                ComPtr<IMFMediaEngineEx> mediaEngineEx;
                m_mediaEngine.As(&mediaEngineEx);
                HRESULT hr = mediaEngineEx->EnableWindowlessSwapchainMode(true);
                if(S_OK != hr)
                    LOG_HR_MSG(hr, "EnableWindowlessSwapchainMode error");

                // If the wrapper has been notified of the actual window width / height, use the correct values, otherwise, use a default size of 640x480
                uint32_t width = m_width != 0 ? m_width : 640;
                uint32_t height = m_height != 0 ? m_height : 480;
                WindowUpdate(width, height);
                hr = mediaEngineEx->GetVideoSwapchainHandle(&m_dcompSurfaceHandle);
                if (S_OK != hr)
                    LOG_HR_MSG(hr, "GetVideoSwapchainHandle error");
            }
            m_initializedCB();
        });            
    }

    void MediaEngineWrapper::OnError(MF_MEDIA_ENGINE_ERR error, HRESULT hr)
    {
        if (m_errorCB)
            m_errorCB(error, hr);
    }

    void MediaEngineWrapper::OnPlaybackEnded()
    {
        m_mediaEngineNotifier->Shutdown();
        m_mediaEngineExtension->Shutdown();
        m_mediaEngine->Shutdown();
        if(m_playbackEndedCB)
            m_playbackEndedCB();
    }
} // namespace media