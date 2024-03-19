// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "..\..\ContentDecryptionModule01\ContentDecryption.h"

#include "MediaFoundationHelpers.h"

inline std::string ToString(MF_MEDIA_ENGINE_EVENT Value)
{
    static std::pair<MF_MEDIA_ENGINE_EVENT, char const* const> constexpr const g_Items[]
    {
        #define IDENFITIER(Name) std::make_pair(Name, #Name),
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_LOADSTART)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_PROGRESS)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_SUSPEND)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_ABORT)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_ERROR)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_EMPTIED)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_STALLED)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_PLAY)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_PAUSE)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_LOADEDDATA)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_WAITING)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_PLAYING)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_CANPLAY)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_SEEKING)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_SEEKED)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_TIMEUPDATE)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_ENDED)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_RATECHANGE)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_FORMATCHANGE)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_BALANCECHANGE)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_OPMINFO)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_RESOURCELOST)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED)
        IDENFITIER(MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE)
        #undef IDENFITIER
    };
    return ToString(g_Items, Value);
}

namespace media
{

// This implementation of IMFMediaEngineExtension is used to integrate a custom
// IMFMediaSource with the MediaEngine pipeline
class MediaEngineExtension : public winrt::implements<MediaEngineExtension, IMFMediaEngineExtension>
{
public:
    MediaEngineExtension(wil::com_ptr<IMFMediaSource> const& MediaSource) :
        m_MediaSource(MediaSource)
    {
    }

    void Shutdown()
    {
        TRACE(L"this %p\n", this);
        [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
        if(std::exchange(m_Shutdown, true))
            return;
        m_MediaSource.reset();
    }

// IMFMediaEngineExtension
    IFACEMETHOD(CanPlayType)([[maybe_unused]] BOOL AudioOnly, [[maybe_unused]] BSTR MimeType, MF_MEDIA_ENGINE_CANPLAY* Result) noexcept override
    {
        TRACE(L"this %p, AudioOnly %d, MimeType %ls\n", this, AudioOnly, MimeType ? MimeType : L"nullptr");
        WI_ASSERT(Result);
        *Result = MF_MEDIA_ENGINE_CANPLAY_NOT_SUPPORTED;
        return S_OK;
    }
    IFACEMETHOD(BeginCreateObject)([[maybe_unused]] BSTR Location, [[maybe_unused]] IMFByteStream* ByteStream, MF_OBJECT_TYPE Type, IUnknown** CancelCookie, IMFAsyncCallback* AsyncCallback, IUnknown* StateUnknown) noexcept override
    {
        TRACE(L"this %p, Location \"%ls\", ByteStream %p, Type %u\n", this, Location, ByteStream, Type);
        try
        {
            WI_ASSERT(AsyncCallback);
            wil::assign_null_to_opt_param(CancelCookie);
            RETURN_HR_IF(MF_E_UNEXPECTED, Type != MF_OBJECT_MEDIASOURCE);
            wil::com_ptr<IMFAsyncResult> AsyncResult;
            {
                [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
                THROW_HR_IF(MF_E_SHUTDOWN, m_Shutdown);
                THROW_HR_IF(MF_E_UNEXPECTED, !m_MediaSource);
                THROW_IF_FAILED(MFCreateAsyncResult(m_MediaSource.get(), AsyncCallback, StateUnknown, AsyncResult.put()));
                WI_ASSERT(!m_CreateMediaSourceActive);
                m_CreateMediaSourceActive = true;
            }
            THROW_IF_FAILED(AsyncResult->SetStatus(S_OK));
            THROW_IF_FAILED(AsyncCallback->Invoke(AsyncResult.get()));
        }
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(CancelObjectCreation)([[maybe_unused]] IUnknown* CancelCookie) noexcept override
    {
        TRACE(L"this %p\n", this);
        return E_NOTIMPL;
    }
    IFACEMETHOD(EndCreateObject)(IMFAsyncResult* AsyncResult, IUnknown** Object) noexcept override
    {
        TRACE(L"this %p\n", this);
        try
        {
            *Object = nullptr;
            [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
            auto CreateMediaSourceActive = std::exchange(m_CreateMediaSourceActive, false);
            THROW_HR_IF(MF_E_UNEXPECTED, !CreateMediaSourceActive);
            THROW_IF_FAILED(AsyncResult->GetStatus());
            THROW_IF_FAILED(AsyncResult->GetObject(Object));
        }
        CATCH_RETURN();
        return S_OK;
    }

private:
    mutable wil::srwlock m_DataMutex;
    wil::com_ptr<IMFMediaSource> m_MediaSource;
    bool m_Shutdown = false;
    bool m_CreateMediaSourceActive = false;
};

// Implements IMFContentProtectionManager
// (https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfcontentprotectionmanager)
// and winrt::Windows::Media::Protection::IMediaProtectionManager
// (https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.mediaprotectionmanager)
// required by IMFMediaEngineProtectedContent::SetContentProtectionManager in
// https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengineprotectedcontent-setcontentprotectionmanager.
class MediaEngineProtectionManager : public winrt::implements<MediaEngineProtectionManager, IMFContentProtectionManager, winrt::Windows::Media::Protection::IMediaProtectionManager> 
{
public:
    MediaEngineProtectionManager(std::shared_ptr<Eme::MediaKeys> const& mediaKeys) :
        m_ContentDecryptionModule(mediaKeys->ContentDecryptionModule)
    {
        // Use the PMP server from the CDM - this ensures that the media engine uses the same protected process as the CDM
        auto const cdmServices = m_ContentDecryptionModule.query<IMFGetService>();
        winrt::com_ptr<ABI::Windows::Media::Protection::IMediaProtectionPMPServer> abiPmpServer;
        THROW_IF_FAILED(cdmServices->GetService(MF_CONTENTDECRYPTIONMODULE_SERVICE, IID_PPV_ARGS(abiPmpServer.put())));
        winrt::Windows::Media::Protection::MediaProtectionPMPServer pmpServer{ nullptr };
        winrt::copy_from_abi(pmpServer, abiPmpServer.get());
        {
            // MFMediaEngine uses |pmpServerKey| to get the Protected Media Path (PMP) server used for playing protected content.
            // This is not currently documented in MSDN.
            winrt::hstring const pmpServerKey { L"Windows.Media.Protection.MediaProtectionPMPServer" };
            TRACE(L"this %p, pmpServerKey %ls\n", this, pmpServerKey.c_str());
            //using MapType = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, IInspectable>;
            //m_PropertySet.as<MapType>().Insert(pmpServerKey, pmpServer);
            m_PropertySet.Insert(pmpServerKey, pmpServer);
        }
    }

// IMFContentProtectionManager
    IFACEMETHOD(BeginEnableContent)(IMFActivate* enablerActivate, [[maybe_unused]] IMFTopology* topology, IMFAsyncCallback* callback, IUnknown* state) noexcept override
    {
        TRACE(L"this %p\n", this);
        try
        {
            wil::com_ptr<IUnknown> unknownObject;
            wil::com_ptr<IMFAsyncResult> asyncResult;
            THROW_IF_FAILED(MFCreateAsyncResult(nullptr, callback, state, asyncResult.put()));
            THROW_IF_FAILED(enablerActivate->ActivateObject(IID_PPV_ARGS(unknownObject.put())));

            // |enablerType| can be obtained from IMFContentEnabler
            // (https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfcontentenabler).
            // If not, try IMediaProtectionServiceRequest
            // (https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.imediaprotectionservicerequest).
            GUID enablerType = GUID_NULL;
            auto const contentEnabler = unknownObject.try_query<IMFContentEnabler>();
            if (contentEnabler)
            {
                THROW_IF_FAILED(contentEnabler->GetEnableType(&enablerType));
            }
            else
            {
                auto const serviceRequest = unknownObject.query<ABI::Windows::Media::Protection::IMediaProtectionServiceRequest>();
                THROW_IF_FAILED(serviceRequest->get_Type(&enablerType));
            }

            // Unhandled enabler types
            THROW_HR_IF(MF_E_REBOOT_REQUIRED, enablerType == MFENABLETYPE_MF_RebootRequired);
            THROW_HR_IF(MF_E_GRL_VERSION_TOO_LOW, enablerType == MFENABLETYPE_MF_UpdateRevocationInformation);
            THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_IMAGE_HASH), enablerType == MFENABLETYPE_MF_UpdateUntrustedComponent);
            // Other enabler types are handled by the CDM
            THROW_IF_FAILED(m_ContentDecryptionModule->SetContentEnabler(contentEnabler.get(), asyncResult.get()));
        } 
        CATCH_RETURN();
        return S_OK;
    }
    IFACEMETHOD(EndEnableContent)([[maybe_unused]] IMFAsyncResult* asyncResult) noexcept override
    {
        TRACE(L"this %p\n", this);
        return S_OK;
    }

// IMediaProtectionManager
    winrt::event_token ServiceRequested(winrt::Windows::Media::Protection::ServiceRequestedEventHandler const handler)
    {
        THROW_HR(E_NOTIMPL);
    }
    void ServiceRequested([[maybe_unused]] winrt::event_token const cookie)
    {
        THROW_HR(E_NOTIMPL);
    }
    winrt::event_token RebootNeeded([[maybe_unused]] winrt::Windows::Media::Protection::RebootNeededEventHandler const handler)
    {
        THROW_HR(E_NOTIMPL);
    }
    void RebootNeeded([[maybe_unused]] winrt::event_token const cookie)
    {
        THROW_HR(E_NOTIMPL);
    }
    winrt::event_token ComponentLoadFailed([[maybe_unused]] winrt::Windows::Media::Protection::ComponentLoadFailedEventHandler const handler)
    {
        THROW_HR(E_NOTIMPL);
    }
    void ComponentLoadFailed([[maybe_unused]] winrt::event_token const cookie)
    {
        THROW_HR(E_NOTIMPL);
    }
    winrt::Windows::Foundation::Collections::PropertySet Properties()
    {
        TRACE(L"this %p, m_PropertySet.Size() %u\n", this, m_PropertySet.Size());
        return m_PropertySet;
    }

private:
    wil::com_ptr<IMFContentDecryptionModule> const m_ContentDecryptionModule;
    winrt::Windows::Foundation::Collections::PropertySet m_PropertySet;
};

class __declspec(uuid("{4B134833-D245-4EEF-89F5-370DEE5D37DE}")) IMediaEnginePlayerSite : public IUnknown
{
public:
    virtual void HandleLoaded() = 0;
    virtual void HandleError(MF_MEDIA_ENGINE_ERR Error, HRESULT Result) = 0;
};

// This class handles creation and management of the MediaFoundation MediaEngine.
// - It uses the provided IMFMediaSource to feed media samples into the MediaEngine pipeline.
// - A DirectComposition surface is exposed to the application layer to incorporate video from the MediaEngine in its visual tree.
// - The application must provide a callback object (std::function) which is invoked after the media engine has loaded and the DirectComposition surface is available.
class MediaEnginePlayer : public winrt::implements<MediaEnginePlayer, IUnknown>
{
public:
    MediaEnginePlayer(IMediaEnginePlayerSite* Site, IMFMediaSource* mediaSource, std::shared_ptr<Eme::MediaKeys> mediaKeys) : 
        m_Site(Site)
    {
        TRACE(L"...\n");
        WI_ASSERT(m_Site);
        Initialize(mediaSource, mediaKeys);
    }

    void Initialize(IMFMediaSource* mediaSource, std::shared_ptr<Eme::MediaKeys> mediaKeys)
    {
        RunSyncInMTA([&]()
        {
            TRACE(L"...\n");
            InitializeVideo();
            CreateMediaEngine(mediaSource, mediaKeys);
        });
    }
    void Shutdown()
    {
        TRACE(L"...\n");
        RunSyncInMTA([&]()
        {
            TRACE(L"...\n");
            auto lock = m_lock.lock();
            THROW_IF_FAILED(m_MediaEngine->Shutdown());
        });
    }

    void Pause()
    {
        TRACE(L"...\n");
        RunSyncInMTA([&]()
        {
            TRACE(L"...\n");
            auto lock = m_lock.lock();
            THROW_IF_FAILED(m_MediaEngine->Pause());
        });
    }
    void StartPlayingFrom(uint64_t Time)
    {
        TRACE(L"...\n");
        RunSyncInMTA([&, Time]()
        {
            TRACE(L"...\n");
            auto lock = m_lock.lock();
            THROW_IF_FAILED(m_MediaEngine->SetCurrentTime(ConvertHnsToSeconds(Time)));
            THROW_IF_FAILED(m_MediaEngine->Play());
        });
    }
    void SetPlaybackRate(double playbackRate)
    {
        TRACE(L"...\n");
        RunSyncInMTA([&]()
        {
            TRACE(L"...\n");
            auto lock = m_lock.lock();
            THROW_IF_FAILED(m_MediaEngine->SetPlaybackRate(playbackRate));
        });
    }
    void SetVolume(float volume)
    {
        TRACE(L"...\n");
        RunSyncInMTA([&]()
        {
            TRACE(L"...\n");
            auto lock = m_lock.lock();
            THROW_IF_FAILED(m_MediaEngine->SetVolume(volume));
        });
    }
    uint64_t GetMediaTime() const
    {
        TRACE(L"...\n");
        uint64_t Result = 0;
        RunSyncInMTA([&]()
        {
            TRACE(L"...\n");
            auto lock = m_lock.lock();
            Result = ConvertSecondsToHns(m_MediaEngine->GetCurrentTime());
        });
        return Result;
    }

    HANDLE GetSurfaceHandle()
    {
        TRACE(L"...\n");
        HANDLE surfaceHandle = INVALID_HANDLE_VALUE;
        RunSyncInMTA([&]()
        {
            TRACE(L"...\n");
            surfaceHandle = m_dcompSurfaceHandle.get();
        });
        return surfaceHandle;
    }

    // Inform media engine of output window position & size changes
    void OnWindowUpdate(std::pair<uint32_t, uint32_t> WindowSize)
    {
        RunSyncInMTA([&]()
        {
            auto lock = m_lock.lock();
            m_WindowSize = WindowSize;
            if(m_MediaEngine)
            {
                RECT Position { 0, 0, static_cast<LONG>(WindowSize.first), static_cast<LONG>(WindowSize.second) };
                THROW_IF_FAILED(m_MediaEngine.query<IMFMediaEngineEx>()->UpdateVideoStream(nullptr, &Position, nullptr));
            }
        });
    }

  private:

    class MediaEngineNotify : public winrt::implements<MediaEngineNotify, IMFMediaEngineNotify>
    {
    public:
        MediaEngineNotify(winrt::com_ptr<MediaEnginePlayer> const& Owner) :
            m_Owner(Owner)
        {
        }

        void Shutdown()
        {
            TRACE(L"this %p\n", this);
            [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
            m_Shutdown = true;
            m_Owner = nullptr;
        }

    // IMFMediaEngineNotify
        IFACEMETHOD(EventNotify)(DWORD Code, DWORD_PTR Parameter1, DWORD Parameter2) noexcept override
        {
            TRACE(L"this %p, Code %hs, %p, %08x\n", this, ToString(static_cast<MF_MEDIA_ENGINE_EVENT>(Code)).c_str(), Parameter1, Parameter2);
            try
            {
                [[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
                RETURN_HR_IF(MF_E_SHUTDOWN, m_Shutdown);
                WI_ASSERT(m_Owner);
                switch(static_cast<MF_MEDIA_ENGINE_EVENT>(Code))
                {
                case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
                    {
                        media::MFPutWorkItem([&, StrongThis = get_strong()]() 
                        {
                            m_Owner->HandleLoaded();
                            m_Owner->m_Site->HandleLoaded();
                        });
                    }
                    break;
                case MF_MEDIA_ENGINE_EVENT_ERROR:
                    m_Owner->m_Site->HandleError(static_cast<MF_MEDIA_ENGINE_ERR>(Parameter1), static_cast<HRESULT>(Parameter2));
                    break;
                }
            }
            CATCH_RETURN();
            return S_OK;
        }

    private:
        mutable wil::srwlock m_DataMutex;
        winrt::com_ptr<MediaEnginePlayer> m_Owner;
        bool m_Shutdown = false;
    };

    void InitializeVideo()
    {
        m_dxgiDeviceManager = nullptr;
        THROW_IF_FAILED(MFLockDXGIDeviceManager(&m_deviceResetToken, m_dxgiDeviceManager.put()));
        UINT creationFlags = (D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS);
        D3D_FEATURE_LEVEL constexpr featureLevels[] { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
        wil::com_ptr<ID3D11Device> d3d11Device;
        wil::com_ptr<ID3D11DeviceContext> d3d11DeviceContext;
        THROW_IF_FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, creationFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, d3d11Device.put(), nullptr, d3d11DeviceContext.put()));
        d3d11DeviceContext.query<ID3D11Multithread>()->SetMultithreadProtected(TRUE);
        THROW_IF_FAILED(m_dxgiDeviceManager->ResetDevice(d3d11Device.get(), m_deviceResetToken));
    }
    void CreateMediaEngine(wil::com_ptr<IMFMediaSource> const& MediaSource, std::shared_ptr<Eme::MediaKeys> mediaKeys)
    {
        WI_ASSERT(MediaSource);

        m_platformRef.Startup();

        InitializeVideo();

        wil::com_ptr<IMFMediaEngineClassFactory> ClassFactory;
        THROW_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(ClassFactory.put())));

        m_Notify = winrt::make_self<MediaEngineNotify>(get_strong());

        wil::com_ptr<IMFAttributes> Attributes;
        THROW_IF_FAILED(MFCreateAttributes(Attributes.put(), 7));
        THROW_IF_FAILED(Attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, m_Notify.get()));
        THROW_IF_FAILED(Attributes->SetUINT32(MF_MEDIA_ENGINE_CONTENT_PROTECTION_FLAGS, MF_MEDIA_ENGINE_ENABLE_PROTECTED_CONTENT));
        THROW_IF_FAILED(Attributes->SetGUID(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE, MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE_IE_EDGE));
        THROW_IF_FAILED(Attributes->SetUINT32(MF_MEDIA_ENGINE_AUDIO_CATEGORY, AudioCategory_Media));
        if(m_dxgiDeviceManager != nullptr)
            THROW_IF_FAILED(Attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, m_dxgiDeviceManager.get()));

        wil::com_ptr<IMFMediaSource> ExtensionMediaSource;
        if(mediaKeys)
            ExtensionMediaSource = winrt::make_self<InputTrustAuthorityMediaSource>(MediaSource, mediaKeys->ContentDecryptionModule).get(); // Also exposes an IMFTrustedInput to the pipeline
        else
            ExtensionMediaSource = MediaSource; // Set the app-provided media source directly on the media engine extension
        m_Extension = winrt::make_self<MediaEngineExtension>(ExtensionMediaSource);
        THROW_IF_FAILED(Attributes->SetUnknown(MF_MEDIA_ENGINE_EXTENSION, m_Extension.get()));
        THROW_IF_FAILED(ClassFactory->CreateInstance(0, Attributes.get(), m_MediaEngine.put()));
        if(mediaKeys)
        {
            m_ProtectionManager = winrt::make_self<MediaEngineProtectionManager>(mediaKeys);
            THROW_IF_FAILED(m_MediaEngine.query<IMFMediaEngineProtectedContent>()->SetContentProtectionManager(m_ProtectionManager.get()));
            // NOTE: The call adds to the properties of the manager
        }

        THROW_IF_FAILED(m_MediaEngine.query<IMFMediaEngineEx>()->SetSource(wil::make_bstr(L"MediaEnginePlayer").get()));
    }

    void HandleLoaded()
    {
        auto lock = m_lock.lock();
        auto const MediaEngineEx = m_MediaEngine.query<IMFMediaEngineEx>();
        THROW_IF_FAILED(MediaEngineEx->EnableWindowlessSwapchainMode(true));
        OnWindowUpdate(m_WindowSize);
        THROW_IF_FAILED(MediaEngineEx->GetVideoSwapchainHandle(&m_dcompSurfaceHandle));
    }

    wil::com_ptr<IMediaEnginePlayerSite> m_Site;
    mutable wil::critical_section m_lock;
    MFPlatformRef m_platformRef;
    wil::com_ptr<IMFMediaEngine> m_MediaEngine;
    UINT m_deviceResetToken = 0;
    wil::com_ptr<IMFDXGIDeviceManager> m_dxgiDeviceManager;
    winrt::com_ptr<MediaEngineExtension> m_Extension;
    winrt::com_ptr<MediaEngineNotify> m_Notify;
    winrt::com_ptr<MediaEngineProtectionManager> m_ProtectionManager;
    wil::unique_handle m_dcompSurfaceHandle;
    std::pair<uint32_t, uint32_t> m_WindowSize = std::make_pair(640u, 480u);
};

} // namespace media