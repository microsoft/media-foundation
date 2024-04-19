#pragma once

#include "MainWindow.g.h"
#include "pch.h"

#include "MediaEngineWrapper.h"
#include "MediaFoundationHelpers.h"

using Microsoft::WRL::ComPtr;

namespace winrt::MediaEngineCustomSourceXamlSample::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnPanelLoaded(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        // Basic graphics
        ComPtr<IMFDXGIDeviceManager>   m_deviceMgr;
        ComPtr<IMFSourceReader>        m_sourceReader;
        ComPtr<media::MediaEngineWrapper> m_mediaEngineWrapper;

        HRESULT InitializeDXGI();
        HRESULT SetupVideoVisual();
        void OnMediaInitialized();
        void OnMediaError(MF_MEDIA_ENGINE_ERR error, HRESULT hr);
        void OnPlaybackEnd();
        

        HANDLE m_videoSurfaceHandle = nullptr;
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel m_swapChainPanel{ nullptr };
    };
}

namespace winrt::MediaEngineCustomSourceXamlSample::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
