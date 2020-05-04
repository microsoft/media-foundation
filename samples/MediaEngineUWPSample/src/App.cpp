// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "media/MediaEngineWrapper.h"
#include "media/MediaFoundationHelpers.h"

using namespace winrt;

using namespace Windows;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;

// Global Variables:
const wchar_t* c_testContentURL = L"http://distribution.bbb3d.renderfarming.net/video/mp4/bbb_sunflower_1080p_30fps_normal.mp4";

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

    // Composition members
    wil::critical_section m_compositionLock;
    winrt::Windows::Foundation::Size m_windowSize{};
    CompositionTarget m_target{ nullptr };
    Visual m_videoVisual{ nullptr };

    IFrameworkView CreateView()
    {
        return *this;
    }

    void Initialize(CoreApplicationView const &)
    {
        m_mfPlatform.Startup();

        // Create a source resolver to create an IMFMediaSource for the content URL.
        // This will create an instance of an inbuilt OS media source for playback.
        // An application can skip this step and instantiate a custom IMFMediaSource implementation instead.
        winrt::com_ptr<IMFSourceResolver> sourceResolver;
        THROW_IF_FAILED(MFCreateSourceResolver(sourceResolver.put()));
        constexpr uint32_t sourceResolutionFlags = MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_READ;
        MF_OBJECT_TYPE objectType = {};
        winrt::com_ptr<IMFMediaSource> mediaSource;
        THROW_IF_FAILED(sourceResolver->CreateObjectFromURL(c_testContentURL, sourceResolutionFlags, nullptr, &objectType, reinterpret_cast<IUnknown**>(mediaSource.put_void())));

        // Callbacks invoked by the media engine wrapper
        auto onInitialized = std::bind(&App::OnMediaInitialized, this);
        auto onError = std::bind(&App::OnMediaError, this, std::placeholders::_1, std::placeholders::_2);

        // Create and initialize the MediaEngineWrapper which manages media playback
        m_mediaEngineWrapper = winrt::make_self<media::MediaEngineWrapper>(onInitialized, onError, nullptr, nullptr, nullptr);
        m_mediaEngineWrapper->Initialize(mediaSource.get());
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

    void OnMediaError(MF_MEDIA_ENGINE_ERR error, HRESULT hr)
    {
        LOG_HR_MSG(hr, "MediaEngine error (%d)", error);
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
