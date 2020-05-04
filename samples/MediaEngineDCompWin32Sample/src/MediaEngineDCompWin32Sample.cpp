// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// MediaEngineDCompWin32Sample.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "ui/DirectCompositionWindow.h"
#include "ui/DirectCompositionLayer.h"
#include "media/MediaEngineWrapper.h"
#include "media/MediaFoundationHelpers.h"

using namespace Microsoft::WRL;

// Global Variables:
const wchar_t* c_testContentURL = L"http://distribution.bbb3d.renderfarming.net/video/mp4/bbb_sunflower_1080p_30fps_normal.mp4";

void MediaEnginePlaybackSample()
{
    // Ensure that MediaFoundation has been started up using the MFPlatformRef helper object
    media::MFPlatformRef mfPlatform;
    mfPlatform.Startup();

    // Create a source resolver to create an IMFMediaSource for the content URL.
    // This will create an instance of an inbuilt OS media source for playback.
    // An application can skip this step and instantiate a custom IMFMediaSource implementation instead.
    winrt::com_ptr<IMFSourceResolver> sourceResolver;
    THROW_IF_FAILED(MFCreateSourceResolver(sourceResolver.put()));
    constexpr uint32_t sourceResolutionFlags = MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_READ;
    MF_OBJECT_TYPE objectType = {};
    winrt::com_ptr<IMFMediaSource> mediaSource;
    THROW_IF_FAILED(sourceResolver->CreateObjectFromURL(c_testContentURL, sourceResolutionFlags, nullptr, &objectType, reinterpret_cast<IUnknown**>(mediaSource.put_void())));

    std::unique_ptr<ui::DirectCompositionWindow> directCompositionWindow;
    winrt::com_ptr<media::MediaEngineWrapper> mediaEngineWrapper;

    // Lambdas to handle callbacks

    // DirectCompositionWindow callback to inform app of size changes
    auto onWindowSizeChanged = [&](uint32_t width, uint32_t height) {
        // If the MediaEngineWrapper has been created, notify it of window size changes so that it can update the video surface size.
        if(mediaEngineWrapper)
        {
            mediaEngineWrapper->OnWindowUpdate(width, height);
        }
    };
    
    wil::slim_event windowClosed;
    // DirectCompositionWindow callback to inform app that window has been closed
    auto onWindowClosed = [&]()
    {
        windowClosed.SetEvent();
    };

    // MediaEngineWrapper initialization callback which is invoked once the media has been loaded and a DCOMP surface handle is available
    auto onInitialized = [&]() {
        // Create video visual and add it to the DCOMP tree
        winrt::com_ptr<IDCompositionDevice> dcompDevice;
        THROW_IF_FAILED(directCompositionWindow->GetDevice()->QueryInterface(IID_PPV_ARGS(dcompDevice.put())));
        std::shared_ptr<ui::DirectCompositionLayer> videoLayer =
            ui::DirectCompositionLayer::CreateFromSurface(dcompDevice.get(), mediaEngineWrapper->GetSurfaceHandle());
        winrt::com_ptr<IDCompositionVisual2> rootVisual;
        rootVisual.copy_from(directCompositionWindow->GetRootVisual());
        THROW_IF_FAILED(rootVisual->AddVisual(videoLayer->GetVisual(), TRUE, nullptr));
        directCompositionWindow->CommitUpdates();

        // Start playback
        mediaEngineWrapper->StartPlayingFrom(0);
    };

    auto onError = [&](MF_MEDIA_ENGINE_ERR error, HRESULT hr)
    {
        wchar_t message[100] = {};
        wsprintf(message, L"Playback failed. error=%d, hr=0x%x", error, hr);
        MessageBox(GetDesktopWindow(), message, nullptr, MB_OK | MB_ICONEXCLAMATION);
    };

    // Create a window to host the direct composition visual tree
    directCompositionWindow = std::make_unique<ui::DirectCompositionWindow>(/*parentWindow*/ nullptr, onWindowSizeChanged, onWindowClosed);
    directCompositionWindow->Initialize();

    directCompositionWindow->SetRect(0, 0, 640, 480);

    // Create and initialize the MediaEngineWrapper which manages media playback
    mediaEngineWrapper = winrt::make_self<media::MediaEngineWrapper>(onInitialized, onError, nullptr, nullptr, nullptr);
    mediaEngineWrapper->Initialize(mediaSource.get());

    // Keep thread alive until window is closed
    windowClosed.wait();
}

int APIENTRY wWinMain(_In_ HINSTANCE /*hInstance*/, _In_opt_ HINSTANCE /*hPrevInstance*/, _In_ LPWSTR /*lpCmdLine*/, _In_ int /*nCmdShow*/)
{
    MediaEnginePlaybackSample();
}
