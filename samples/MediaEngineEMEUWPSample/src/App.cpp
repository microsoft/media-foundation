// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "..\..\ContentDecryptionModule01\ContentDecryption.h"

#include "MediaEnginePlayer.h"
#include "MediaFoundationHelpers.h"

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

// Sample fragmented MP4 CBCS Protected Content
wchar_t constexpr const g_Location[] = 
    L"https://test.playready.microsoft.com/media/test/cbcs/bbb.main.30fps.1920x1080.h264.aac.cbcs.sameKeys.mp4";

// Sample init data with PlayReady PSSH box
// Contains the following PlayReady Header: 
// <WRMHEADER xmlns="http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader" version="4.3.0.0"><DATA><LA_URL>https://test.playready.microsoft.com/core/rightsmanager.asmx?cfg=(playenablers:(786627d8-c2a6-44be-8f88-08ae255b01a7),sl:150,ck:W31bfVt9W31bfVt9W31bfQ==,ckt:AES128BitCBC)</LA_URL><PROTECTINFO><KIDS><KID ALGID="AESCBC" VALUE="AAAAEAAQABAQABAAAAAAAQ=="></KID></KIDS></PROTECTINFO></DATA></WRMHEADER>
constexpr uint8_t g_InitializationData[]
{
    0x00, 0x00, 0x03, 0x54, 0x70, 0x73, 0x73, 0x68, 0x00, 0x00, 0x00, 0x00, 0x9a, 0x04, 0xf0, 0x79, 0x98, 0x40, 0x42, 0x86, 0xab, 0x92, 0xe6, 0x5b, 0xe0, 0x88, 0x5f, 0x95, 0x00, 0x00, 0x03, 0x34, 0x34, 0x03, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x2a, 0x03, 0x3c, 0x00, 0x57, 0x00, 0x52, 0x00, 0x4d, 0x00, 0x48, 0x00, 0x45, 0x00, 0x41, 0x00, 0x44, 0x00, 0x45, 0x00, 0x52, 0x00, 0x20, 0x00, 0x78, 0x00, 0x6d, 0x00, 0x6c, 0x00, 0x6e, 0x00, 0x73, 0x00, 0x3d, 0x00, 0x22, 0x00, 0x68, 0x00, 0x74, 0x00, 0x74, 0x00, 0x70, 0x00, 0x3a, 0x00, 0x2f, 0x00, 0x2f, 0x00, 0x73, 0x00, 0x63, 0x00, 0x68, 0x00, 0x65, 0x00, 0x6d, 0x00, 0x61, 0x00, 0x73, 0x00, 0x2e, 0x00, 0x6d, 0x00, 0x69, 0x00, 0x63, 0x00, 0x72, 0x00, 0x6f, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x66, 0x00, 0x74, 0x00, 0x2e, 0x00, 0x63, 0x00, 0x6f, 0x00, 0x6d, 0x00, 0x2f, 0x00, 0x44, 0x00, 0x52, 0x00, 0x4d, 0x00, 0x2f, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00, 0x37, 0x00, 0x2f, 0x00, 0x30, 0x00, 0x33, 0x00, 0x2f, 0x00, 0x50, 0x00, 0x6c, 0x00, 0x61, 0x00, 0x79, 0x00, 0x52, 0x00, 0x65, 0x00, 0x61, 0x00, 0x64, 0x00, 0x79, 0x00, 0x48, 0x00, 0x65, 0x00, 0x61, 0x00, 0x64, 0x00, 0x65, 0x00, 0x72, 0x00, 0x22, 0x00, 0x20, 0x00, 0x76, 0x00, 0x65, 0x00, 0x72, 0x00, 0x73, 0x00, 0x69, 0x00, 0x6f, 0x00, 0x6e, 0x00, 0x3d, 0x00, 0x22, 0x00, 0x34, 0x00, 0x2e, 0x00, 0x33, 0x00, 0x2e, 0x00, 0x30, 0x00, 0x2e, 0x00, 0x30, 0x00, 0x22, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x44, 0x00, 0x41, 0x00, 0x54, 0x00, 0x41, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x4c, 0x00, 0x41, 0x00, 0x5f, 0x00, 0x55, 0x00, 0x52, 0x00, 0x4c, 0x00, 0x3e, 0x00, 0x68, 0x00, 0x74, 0x00, 0x74, 0x00, 0x70, 0x00, 0x73, 0x00, 0x3a, 0x00, 0x2f, 0x00, 0x2f, 0x00, 0x74, 0x00, 0x65, 0x00, 0x73, 0x00, 0x74, 0x00, 0x2e, 0x00, 0x70, 0x00, 0x6c, 0x00, 0x61, 0x00, 0x79, 0x00, 0x72, 0x00, 0x65, 0x00, 0x61, 0x00, 0x64, 0x00, 0x79, 0x00, 0x2e, 0x00, 0x6d, 0x00, 0x69, 0x00, 0x63, 0x00, 0x72, 0x00, 0x6f, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x66, 0x00, 0x74, 0x00, 0x2e, 0x00, 0x63, 0x00, 0x6f, 0x00, 0x6d, 0x00, 0x2f, 0x00, 0x63, 0x00, 0x6f, 0x00, 0x72, 0x00, 0x65, 0x00, 0x2f, 0x00, 0x72, 0x00, 0x69, 0x00, 0x67, 0x00, 0x68, 0x00, 0x74, 0x00, 0x73, 0x00, 0x6d, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0x72, 0x00, 0x2e, 0x00, 0x61, 0x00, 0x73, 0x00, 0x6d, 0x00, 0x78, 0x00, 0x3f, 0x00, 0x63, 0x00, 0x66, 0x00, 0x67, 0x00, 0x3d, 0x00, 0x28, 0x00, 0x70, 0x00, 0x6c, 0x00, 0x61, 0x00, 0x79, 0x00, 0x65, 0x00, 0x6e, 0x00, 0x61, 0x00, 0x62, 0x00, 0x6c, 0x00, 0x65, 0x00, 0x72, 0x00, 0x73, 0x00, 0x3a, 0x00, 0x28, 0x00, 0x37, 0x00, 0x38, 0x00, 0x36, 0x00, 0x36, 0x00, 0x32, 0x00, 0x37, 0x00, 0x64, 0x00, 0x38, 0x00, 0x2d, 0x00, 0x63, 0x00, 0x32, 0x00, 0x61, 0x00, 0x36, 0x00, 0x2d, 0x00, 0x34, 0x00, 0x34, 0x00, 0x62, 0x00, 0x65, 0x00, 0x2d, 0x00, 0x38, 0x00, 0x66, 0x00, 0x38, 0x00, 0x38, 0x00, 0x2d, 0x00, 0x30, 0x00, 0x38, 0x00, 0x61, 0x00, 0x65, 0x00, 0x32, 0x00, 0x35, 0x00, 0x35, 0x00, 0x62, 0x00, 0x30, 0x00, 0x31, 0x00, 0x61, 0x00, 0x37, 0x00, 0x29, 0x00, 0x2c, 0x00, 0x73, 0x00, 0x6c, 0x00, 0x3a, 0x00, 0x31, 0x00, 0x35, 0x00, 0x30, 0x00, 0x2c, 0x00, 0x63, 0x00, 0x6b, 0x00, 0x3a, 0x00, 0x57, 0x00, 0x33, 0x00, 0x31, 0x00, 0x62, 0x00, 0x66, 0x00, 0x56, 0x00, 0x74, 0x00, 0x39, 0x00, 0x57, 0x00, 0x33, 0x00, 0x31, 0x00, 0x62, 0x00, 0x66, 0x00, 0x56, 0x00, 0x74, 0x00, 0x39, 0x00, 0x57, 0x00, 0x33, 0x00, 0x31, 0x00, 0x62, 0x00, 0x66, 0x00, 0x51, 0x00, 0x3d, 0x00, 0x3d, 0x00, 0x2c, 0x00, 0x63, 0x00, 0x6b, 0x00, 0x74, 0x00, 0x3a, 0x00, 0x41, 0x00, 0x45, 0x00, 0x53, 0x00, 0x31, 0x00, 0x32, 0x00, 0x38, 0x00, 0x42, 0x00, 0x69, 0x00, 0x74, 0x00, 0x43, 0x00, 0x42, 0x00, 0x43, 0x00, 0x29, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0x4c, 0x00, 0x41, 0x00, 0x5f, 0x00, 0x55, 0x00, 0x52, 0x00, 0x4c, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x50, 0x00, 0x52, 0x00, 0x4f, 0x00, 0x54, 0x00, 0x45, 0x00, 0x43, 0x00, 0x54, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x46, 0x00, 0x4f, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x4b, 0x00, 0x49, 0x00, 0x44, 0x00, 0x53, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x4b, 0x00, 0x49, 0x00, 0x44, 0x00, 0x20, 0x00, 0x41, 0x00, 0x4c, 0x00, 0x47, 0x00, 0x49, 0x00, 0x44, 0x00, 0x3d, 0x00, 0x22, 0x00, 0x41, 0x00, 0x45, 0x00, 0x53, 0x00, 0x43, 0x00, 0x42, 0x00, 0x43, 0x00, 0x22, 0x00, 0x20, 0x00, 0x56, 0x00, 0x41, 0x00, 0x4c, 0x00, 0x55, 0x00, 0x45, 0x00, 0x3d, 0x00, 0x22, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x45, 0x00, 0x41, 0x00, 0x41, 0x00, 0x51, 0x00, 0x41, 0x00, 0x42, 0x00, 0x41, 0x00, 0x51, 0x00, 0x41, 0x00, 0x42, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x51, 0x00, 0x3d, 0x00, 0x3d, 0x00, 0x22, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0x4b, 0x00, 0x49, 0x00, 0x44, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0x4b, 0x00, 0x49, 0x00, 0x44, 0x00, 0x53, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0x50, 0x00, 0x52, 0x00, 0x4f, 0x00, 0x54, 0x00, 0x45, 0x00, 0x43, 0x00, 0x54, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x46, 0x00, 0x4f, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0x44, 0x00, 0x41, 0x00, 0x54, 0x00, 0x41, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0x57, 0x00, 0x52, 0x00, 0x4d, 0x00, 0x48, 0x00, 0x45, 0x00, 0x41, 0x00, 0x44, 0x00, 0x45, 0x00, 0x52, 0x00, 0x3e, 0x00, 
};

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
    // Media members
    media::MFPlatformRef m_Platform;
    winrt::com_ptr<media::MediaEnginePlayer> m_Player;

    // EME media members
    std::shared_ptr<Eme::MediaKeySession> m_MediaKeySession;

    // Composition members
    mutable wil::critical_section m_compositionLock;
    winrt::Windows::Foundation::Size m_windowSize { };
    CompositionTarget m_target { nullptr };
    Visual m_videoVisual { nullptr };

    void ReportError(HRESULT Result, const char* Message)
    {
        LOG_HR_MSG(Result, Message);
    }

    class Site : public winrt::implements<Site, media::IMediaEnginePlayerSite>
    {
    public:
        Site(winrt::com_ptr<App> const& Owner) :
            m_Owner(Owner)
        {
        }

    // IMediaEnginePlayerSite
        void HandleLoaded() override
        {
            m_Owner->SetupVideoVisual();
            m_Owner->m_Player->StartPlayingFrom(0);
        }
        void HandleError([[maybe_unused]] MF_MEDIA_ENGINE_ERR MediaEngineError, HRESULT Result) override
        {
            LOG_HR_MSG(Result, "IMediaEnginePlayerSite::HandleError, MediaEngineError %u", MediaEngineError);
            m_Owner->ReportError(Result, "Media playback error");
        }

    private:
        winrt::com_ptr<App> m_Owner;
    };

    void HandleKeySessionMessage(MF_MEDIAKEYSESSION_MESSAGETYPE Type, std::vector<uint8_t> const& Data, std::wstring const& Location)
    {
        TRACE(L"Type %hs, Data.size() %zu, Location \"%ls\"\n", ToString(Type).c_str(), Data.size(), Location.c_str());

        // Parse challenge value and request headers from key message
        winrt::hstring messageString{ reinterpret_cast<wchar_t*>(const_cast<BYTE*>(Data.data())), static_cast<winrt::hstring::size_type>(Data.size() / sizeof(wchar_t)) };
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
                contentHeaders.ContentType(Headers::HttpMediaTypeHeaderValue::Parse(headerValue));
            else
                headers.Insert(headerName, headerValue);
        }

        IBuffer Response{};
        try
        {
            winrt::Windows::Foundation::Uri requestUri{ Location };
            HttpResponseMessage responseMessage = httpClient.PostAsync(requestUri, postContent).get();
            responseMessage.EnsureSuccessStatusCode();
            Response = responseMessage.Content().ReadAsBufferAsync().get();
        }
        catch (winrt::hresult_error const& ex)
        {
            ReportError(ex.code(), "License acquisition failure (invalid response)");
            throw ex;
        }
        TRACE(L"Type %hs, Response.Length() %u\n", ToString(Type).c_str(), Response.Length());

        // Pass response data to update() method on the MediaKeySession
        m_MediaKeySession->Update(std::vector<uint8_t>(Response.data(), Response.data() + Response.Length()));
    }
    void InitializePlayback()
    {
        TRACE(L"...\n");

        // Setup EME
        Eme::FactoryConfiguration Configuration { };
        // Use the local cache folder which is not backed up / synced to the cloud for CDM storage
        Configuration.StoragePath = Windows::Storage::ApplicationData::Current().LocalCacheFolder().Path().c_str();
        Eme::Factory EmeFactory(Configuration);
        
        // Request access to the Playready key system.
        // This is equivalent to the following Javascript EME call:
        // Navigator.requestMediaKeySystemAccess("com.microsoft.playready.recommendation", [{ initDataTypes: ['cenc'], persistentState: 'optional', distinctiveIdentifier: 'required'])
        std::vector<Eme::MediaKeySystemConfiguration> MediaKeySystemConfigurationVector(1);
        {
            auto& MediaKeySystemConfiguration = MediaKeySystemConfigurationVector[0];
            MediaKeySystemConfiguration.InitDataTypeVector.emplace_back(L"cenc");
            MediaKeySystemConfiguration.PersistentState = MF_MEDIAKEYS_REQUIREMENT_OPTIONAL;
            MediaKeySystemConfiguration.DistinctiveIdentifier = MF_MEDIAKEYS_REQUIREMENT_REQUIRED;
        }
        auto const KeySystemAccess = EmeFactory.RequestMediaKeySystemAccess(L"com.microsoft.playready.recommendation", MediaKeySystemConfigurationVector);
        THROW_HR_IF_MSG(MF_E_UNSUPPORTED_SERVICE, !KeySystemAccess, "Key system configuration not supported.");

        // Create MediaKeys, MediaKeySession and setup the onmessage callback on the session (to handle license acquisition requests)
        auto const MediaKeys = KeySystemAccess->CreateMediaKeys();
        m_MediaKeySession = MediaKeys->CreateSession();
        m_MediaKeySession->OnMessage = [&] (MF_MEDIAKEYSESSION_MESSAGETYPE Type, std::vector<uint8_t> const& Data, std::wstring const& Location) { HandleKeySessionMessage(Type, Data, Location); };
        m_MediaKeySession->GenerateRequest(L"cenc", std::vector<uint8_t> { g_InitializationData, g_InitializationData + std::size(g_InitializationData) });

        // Create a source resolver to create an IMFMediaSource for the content URL.
        // This will create an instance of an inbuilt OS media source for playback.
        // An application can skip this step and instantiate a custom IMFMediaSource implementation instead.
        wil::com_ptr<IMFSourceResolver> SourceResolver;
        THROW_IF_FAILED(MFCreateSourceResolver(SourceResolver.put()));
        MF_OBJECT_TYPE ObjectType;
        wil::com_ptr<IMFMediaSource> MediaSource;
        THROW_IF_FAILED(SourceResolver->CreateObjectFromURL(g_Location, MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_READ, nullptr, &ObjectType, MediaSource.put_unknown()));

        m_Player = winrt::make_self<media::MediaEnginePlayer>(winrt::make_self<Site>(get_strong()).get(), MediaSource.get(), MediaKeys);
    }
    void SetupVideoVisual()
    {
        auto lock = m_compositionLock.lock();

        if (m_videoVisual)
            return;
        // Complete setting up video visual if we have a surface from the media engine and the visual tree has been initialized
        if(!m_Player)
            return;
        auto const videoSurfaceHandle = m_Player->GetSurfaceHandle();
        if(!videoSurfaceHandle)
            return;
        if (!m_target)
            return;

        TRACE(L"...\n");
        Compositor compositor = m_target.Compositor();

        // Create sprite visual for video
        SpriteVisual visual = compositor.CreateSpriteVisual();
        visual.Offset({ 0.0f, 0.0f, 0.0f, });

        // Use the ABI ICompositorInterop interface to create an ABI composition surface using the video surface handle from the media engine
        winrt::com_ptr<abi::ICompositorInterop> compositorInterop { compositor.as<abi::ICompositorInterop>() };
        winrt::com_ptr<abi::ICompositionSurface> abiCompositionSurface;
        THROW_IF_FAILED(compositorInterop->CreateCompositionSurfaceForHandle(videoSurfaceHandle, abiCompositionSurface.put()));

        CompositionVisualSurface compositionSurface { nullptr };
        winrt::copy_from_abi(compositionSurface, abiCompositionSurface.get());
        CompositionSurfaceBrush surfaceBrush { compositor.CreateSurfaceBrush() };
        surfaceBrush.Surface(compositionSurface);
        visual.Brush(surfaceBrush);

        m_videoVisual = visual;
        UpdateVideoSize();
        m_target.Root(m_videoVisual);
    }
    void UpdateVideoSize()
    {
        auto lock = m_compositionLock.lock();
        auto WindowSize = m_windowSize;
        if (m_videoVisual)
            m_videoVisual.Size({ WindowSize.Width, WindowSize.Height });
        if(!m_Player)
            return;
        media::RunSyncInMTA([&]() 
        {
            m_Player->OnWindowUpdate(std::make_pair(static_cast<uint32_t>(WindowSize.Width), static_cast<uint32_t>(WindowSize.Height)));
        });
    }

// IFrameworkViewSource
    IFrameworkView CreateView()
    {
        TRACE(L"this %p\n", this);
        return *this;
    }

// IFrameworkView
    void Initialize(CoreApplicationView const&)
    {
        TRACE(L"this %p\n", this);
        m_Platform.Startup();
        media::MFPutWorkItem([&, StrongThis = get_strong()]
        {
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
    void SetWindow(CoreWindow const& Window)
    {
        TRACE(L"this %p\n", this);
        auto lock = m_compositionLock.lock();
        Compositor compositor;
        m_target = compositor.CreateTargetForCurrentView();
        m_windowSize = { Window.Bounds().Width, Window.Bounds().Height };
        UpdateVideoSize();
        Window.SizeChanged([&] (IInspectable const&, WindowSizeChangedEventArgs const& Args)
        { 
            auto lock = m_compositionLock.lock();
            m_windowSize = Args.Size();
            UpdateVideoSize();
        });
        SetupVideoVisual();
    }
    void Load(hstring const& EntryPoint)
    {
        TRACE(L"this %p, EntryPoint %ls\n", this, EntryPoint.c_str());
    }
    void Run()
    {
        TRACE(L"this %p\n", this);
        CoreWindow Window = CoreWindow::GetForCurrentThread();
        Window.Activate();
        CoreDispatcher dispatcher = Window.Dispatcher();
        dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
    }
    void Uninitialize()
    {
        TRACE(L"this %p\n", this);
    }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoreApplication::Run(make<App>());
}
