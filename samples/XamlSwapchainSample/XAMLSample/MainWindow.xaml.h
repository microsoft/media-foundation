#pragma once

#include "MainWindow.g.h"
#include "pch.h"

using Microsoft::WRL::ComPtr;

namespace winrt::XAMLSample::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnPanelLoaded(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        // Basic graphics
        ComPtr<ID3D11Device>           m_device;
        ComPtr<ID3D11DeviceContext>    m_deviceContext;
        ComPtr<ID3D11RenderTargetView> m_backbufferView;
        ComPtr<IDXGISwapChain1>        m_swapChain;

        ComPtr<IMFDXGIDeviceManager>   m_deviceMgr;

        ComPtr<IMFSourceReader>        m_sourceReader;

        HRESULT InitializeGraphics();
        HRESULT InitializeVideoProcessor(
            DWORD width, DWORD height, ID3D11VideoDevice* videoDevice, ID3D11VideoProcessorEnumerator** d3d11VideoProcEnum, ID3D11VideoProcessor** d3d11VideoProc, ID3D11VideoContext* d3d11VideoContext);
        HRESULT DoPresentationLoop();
        void PlayVideo();
    };
}

namespace winrt::XAMLSample::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
