// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "media/eme/EmeFactory.h"
#include "media/MediaEngineWrapper.h"
#include "media/MediaFoundationHelpers.h"
#include "TestContent.h"

using namespace winrt;

using namespace Windows;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;
using namespace Windows::Web::Http;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::Security::Cryptography;
using namespace Windows::Storage::Streams;

namespace abi
{
    using namespace ABI::Windows::UI::Composition;
}

namespace winrt
{
    using namespace Windows::UI::Composition;
}

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
    // Media members
    media::MFPlatformRef m_mfPlatform;
    winrt::com_ptr<media::MediaEngineWrapper> m_mediaEngineWrapper;

    // EME media members
    std::shared_ptr<media::eme::MediaKeySession> m_mediaKeySession;

    // Composition members
    wil::critical_section m_compositionLock;
    winrt::Windows::Foundation::Size m_windowSize{};
    CompositionTarget m_target{ nullptr };
    Visual m_videoVisual{ nullptr };

    IFrameworkView CreateView()
    {
        return *this;
    }

    void ReportError(HRESULT result, const char* message)
    {
        // An application would typically display an error to the user.
        // This sample just logs the error.
        LOG_HR_MSG(result, message);
    }

    // EME callbacks
    void OnEMEKeyMessage(MF_MEDIAKEYSESSION_MESSAGETYPE /*messageType*/, gsl::span<uint8_t> message, LPCWSTR destinationUrl)
    {
        // Parse challenge value and request headers from key message
        winrt::hstring messageString{ reinterpret_cast<wchar_t*>(const_cast<BYTE*>(message.data())), static_cast<winrt::hstring::size_type>(message.size() / sizeof(wchar_t)) };
        XmlDocument messageXml;
        messageXml.LoadXml(messageString);
        XmlNodeList challengeNodeList = messageXml.GetElementsByTagName(L"Challenge");

        if (challengeNodeList.Size() == 0)
        {
            ReportError(E_UNEXPECTED, "License acquisition failure (invalid challenge data)");
            THROW_HR(E_UNEXPECTED);
        }

        winrt::hstring challengeBase64 = challengeNodeList.GetAt(0).InnerText();
        
        // Extract header names/values
        XmlNodeList headerNameNodes = messageXml.GetElementsByTagName(L"name");
        XmlNodeList headerValueNodes = messageXml.GetElementsByTagName(L"value");

        if (headerNameNodes.Size() != headerValueNodes.Size())
        {
            ReportError(E_UNEXPECTED, "License acquisition failure (invalid challenge data)");
            THROW_HR(E_UNEXPECTED);
        }

        // Initialize HTTP POST request with headers and challenge data from key message
        HttpClient httpClient;
        IBuffer challengeBuffer = CryptographicBuffer::DecodeFromBase64String(challengeBase64);
        HttpBufferContent postContent(challengeBuffer);
        auto headers{ httpClient.DefaultRequestHeaders() };
        auto contentHeaders{ postContent.Headers() };

        for (uint32_t i = 0; i < headerNameNodes.Size(); i++)
        {
            winrt::hstring headerName = headerNameNodes.GetAt(i).InnerText();
            winrt::hstring headerValue = headerValueNodes.GetAt(i).InnerText();
            // Special handling required for the 'Content-Type' header
            if (std::wstring(headerName.c_str()) == std::wstring(L"Content-Type"))
            {
                contentHeaders.ContentType(Headers::HttpMediaTypeHeaderValue::Parse(headerValue));
            }
            else
            {
                headers.Insert(headerName, headerValue);
            }
        }

        IBuffer responseBuffer{};

        try
        {
            winrt::Windows::Foundation::Uri requestUri{ destinationUrl };
            HttpResponseMessage responseMessage = httpClient.PostAsync(requestUri, postContent).get();
            responseMessage.EnsureSuccessStatusCode();
            responseBuffer = responseMessage.Content().ReadAsBufferAsync().get();
        }
        catch (winrt::hresult_error const& ex)
        {
            ReportError(ex.code(), "License acquisition failure (invalid response)");
            throw ex;
        }

        // Pass response data to update() method on the MediaKeySession
        m_mediaKeySession->update(gsl::span<uint8_t>(responseBuffer.data(), responseBuffer.Length()));
    }

    void Initialize(CoreApplicationView const &)
    {
        m_mfPlatform.Startup();

        // Initialize playback on MTA MF work queue thread
        // Ensure that the callback lambda holds a reference to this app instance to prevent the instance from being destroyed prior to the lambda executing
        winrt::com_ptr<App> selfRef;
        selfRef.copy_from(this);
        media::MFPutWorkItem([&, selfRef]() {
            try
            {
                InitializePlayback();
            }
            catch(...)
            {
                ReportError(wil::ResultFromCaughtException(), "Playback initialization error");
            }
        });
    }

    void InitializePlayback()
    {
        // Setup EME
        media::eme::CdmConfigurationProperties configProperties = {};
        // Use the local cache folder which is not backed up / synced to the cloud for CDM storage
        configProperties.storagePath = Windows::Storage::ApplicationData::Current().LocalCacheFolder().Path().c_str();
        media::eme::EmeFactory emeFactory(configProperties);
        
        // Request access to the Playready key system.
        // This is equivalent to the following Javascript EME call:
        // Navigator.requestMediaKeySystemAccess("com.microsoft.playready.recommendation", [{ initDataTypes: ['cenc'], persistentState: 'optional', distinctiveIdentifier: 'required'])
        std::vector<media::eme::MediaKeySystemConfiguration> keySystemConfigs(1);
        keySystemConfigs[0].initDataTypes().push_back(L"cenc");
        keySystemConfigs[0].persistentState(MF_MEDIAKEYS_REQUIREMENT_OPTIONAL);
        keySystemConfigs[0].distinctiveId(MF_MEDIAKEYS_REQUIREMENT_REQUIRED);
        
        std::shared_ptr<media::eme::MediaKeySystemAccess> keySystemAccess = emeFactory.requestMediaKeySystemAccess(L"com.microsoft.playready.recommendation", keySystemConfigs);
        if(!keySystemAccess)
        {
            ReportError(MF_E_UNSUPPORTED_SERVICE, "Key system configuration not supported.");
            return;
        }

        // Create MediaKeys, MediaKeySession and setup the onmessage callback on the session (to handle license acquisition requests)
        std::shared_ptr<media::eme::MediaKeys> mediaKeys = keySystemAccess->createMediaKeys();
        m_mediaKeySession = mediaKeys->createSession();
        m_mediaKeySession->onmessage(std::bind(&App::OnEMEKeyMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // Asynchronously acquire license using hardcoded init data - usually an application would find this init data within the media container (e.g in the PSSH box of a MP4 file) or acquire it out-of-band (e.g from an adaptive streaming manifest)
        std::vector<uint8_t> initData(sizeof(TestContent::c_initData));
        memcpy(&initData[0], &TestContent::c_initData[0], initData.size());
        m_mediaKeySession->generateRequest(L"cenc", initData);

        // Create a source resolver to create an IMFMediaSource for the content URL.
        // This will create an instance of an inbuilt OS media source for playback.
        // An application can skip this step and instantiate a custom IMFMediaSource implementation instead.
        winrt::com_ptr<IMFSourceResolver> sourceResolver;
        THROW_IF_FAILED(MFCreateSourceResolver(sourceResolver.put()));
        constexpr uint32_t sourceResolutionFlags = MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_READ;
        MF_OBJECT_TYPE objectType = {};
        winrt::com_ptr<IMFMediaSource> mediaSource;
        std::wstring contentUri(TestContent::c_contentUrl);
        THROW_IF_FAILED(sourceResolver->CreateObjectFromURL(contentUri.c_str(), sourceResolutionFlags, nullptr, &objectType, reinterpret_cast<IUnknown**>(mediaSource.put_void())));

        // Callbacks invoked by the media engine wrapper
        auto onInitialized = std::bind(&App::OnMediaInitialized, this);
        auto onError = std::bind(&App::OnMediaError, this, std::placeholders::_1, std::placeholders::_2);

        // Create and initialize the MediaEngineWrapper which manages media playback
        m_mediaEngineWrapper = winrt::make_self<media::MediaEngineWrapper>(onInitialized, onError, nullptr, nullptr, nullptr);
        m_mediaEngineWrapper->Initialize(mediaSource.get(), mediaKeys);

    }

    // MediaEngineWrapper initialization callback which is invoked once the media has been loaded and a DCOMP surface handle is available
    void OnMediaInitialized()
    {
        // Create video visual and add it to the DCOMP tree
        SetupVideoVisual();

        // Start playback
        m_mediaEngineWrapper->StartPlayingFrom(0);
    }

    void SetupVideoVisual()
    {
        auto lock = m_compositionLock.lock();

        // Complete setting up video visual if we have a surface from the media engine and the visual tree has been initialized
        HANDLE videoSurfaceHandle = m_mediaEngineWrapper ? m_mediaEngineWrapper->GetSurfaceHandle() : NULL;

        if (!m_videoVisual && videoSurfaceHandle != NULL && m_target)
        {
            Compositor compositor = m_target.Compositor();

            // Create sprite visual for video
            SpriteVisual visual = compositor.CreateSpriteVisual();

            visual.Offset(
                {
                    0.0f,
                    0.0f,
                    0.0f,
                });

            // Use the ABI ICompositorInterop interface to create an ABI composition surface using the video surface handle from the media engine
            winrt::com_ptr<abi::ICompositorInterop> compositorInterop{ compositor.as<abi::ICompositorInterop>() };
            winrt::com_ptr<abi::ICompositionSurface> abiCompositionSurface;
            THROW_IF_FAILED(compositorInterop->CreateCompositionSurfaceForHandle(videoSurfaceHandle, abiCompositionSurface.put()));

            // Convert from ABI ICompositionSurface type to winrt CompositionSurface
            CompositionVisualSurface compositionSurface{ nullptr };
            winrt::copy_from_abi(compositionSurface, abiCompositionSurface.get());

            // Setup surface brush with surface from MediaEngineWrapper
            CompositionSurfaceBrush surfaceBrush{ compositor.CreateSurfaceBrush() };
            surfaceBrush.Surface(compositionSurface);
            visual.Brush(surfaceBrush);

            // Insert video visual intro tree
            m_videoVisual = visual;
            UpdateVideoSize();
            m_target.Root(m_videoVisual);
        }
    }

    void UpdateVideoSize()
    {
        auto lock = m_compositionLock.lock();

        // If the window has not been initialized yet, use a default size of 640x480
        const bool windowInitialized = m_windowSize.Width != 0 && m_windowSize.Height != 0;
        float width = windowInitialized ? m_windowSize.Width : 640;
        float height = windowInitialized ? m_windowSize.Height : 480;

        if (m_videoVisual)
        {
            m_videoVisual.Size({
                width,
                height
                });
        }

        if (m_mediaEngineWrapper)
        {
            // Call into media engine wrapper on MTA thread to resize the video surface
            media::RunSyncInMTA([&]() {
                m_mediaEngineWrapper->OnWindowUpdate(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
            });
        }
    }

    void OnMediaError(MF_MEDIA_ENGINE_ERR /*error*/, HRESULT hr)
    {
        ReportError(hr, "Media playback error.");
    }

    void Load(hstring const&)
    {
    }

    void Uninitialize()
    {
    }

    void Run()
    {
        CoreWindow window = CoreWindow::GetForCurrentThread();
        window.Activate();

        CoreDispatcher dispatcher = window.Dispatcher();
        dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
    }

    void OnWindowSizeChanged(IInspectable const&, WindowSizeChangedEventArgs const& args)
    {
        auto lock = m_compositionLock.lock();
        m_windowSize = args.Size();
        UpdateVideoSize();
    }

    void SetWindow(CoreWindow const & window)
    {
        auto lock = m_compositionLock.lock();
        Compositor compositor;
        m_target = compositor.CreateTargetForCurrentView();
        m_windowSize = { window.Bounds().Width, window.Bounds().Height };
        UpdateVideoSize();

        window.SizeChanged({ this, &App::OnWindowSizeChanged });

        // If the mediaengine has been initialized, setup the video visual
        SetupVideoVisual();
    }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoreApplication::Run(make<App>());
}
