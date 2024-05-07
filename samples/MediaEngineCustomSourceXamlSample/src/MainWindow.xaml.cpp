#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "MediaEngineWrapper.h"
#include "MediaFoundationHelpers.h"

using namespace Microsoft::WRL::Wrappers;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::UI::Xaml;

using namespace Microsoft::WRL;

HRESULT CreateSourceReader(
    LPCWSTR fileURL,
    IMFSourceReader** reader,
    DWORD* width,
    DWORD* height)
{
    ComPtr<IMFSourceReader> sourceReader;
    ComPtr<IMFMediaType> nativeMediaType;

    RETURN_IF_FAILED(MFCreateSourceReaderFromURL(fileURL, NULL, &sourceReader));

    RETURN_IF_FAILED(sourceReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &nativeMediaType));
    RETURN_IF_FAILED(MFGetAttributeSize(nativeMediaType.Get(), MF_MT_FRAME_SIZE, (UINT*)width, (UINT*)height));
   

    *reader = sourceReader.Detach();

    return S_OK;
}

namespace winrt::MediaEngineCustomSourceXamlSample::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        MFStartup(MF_VERSION);

        HRESULT hr = InitializeDXGI();
        if (S_OK != hr)
            LOG_HR_MSG(hr, "InitializeDXGI");
    }

    HRESULT MainWindow::InitializeDXGI()
    {
        m_deviceMgr = nullptr;
        UINT deviceResetToken = 0;
        RETURN_IF_FAILED(MFLockDXGIDeviceManager(&deviceResetToken, &m_deviceMgr));

        ComPtr<ID3D11Device> d3d11Device;
        UINT creationFlags = (D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT |
            D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS);
        constexpr D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                                          D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
                                                          D3D_FEATURE_LEVEL_9_1 };
        RETURN_IF_FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, creationFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            &d3d11Device, nullptr, nullptr));

        ComPtr<ID3D10Multithread> multithreadedDevice;
        RETURN_IF_FAILED(d3d11Device.As(&multithreadedDevice));
        multithreadedDevice->SetMultithreadProtected(TRUE);

        RETURN_IF_FAILED(m_deviceMgr->ResetDevice(d3d11Device.Get(), deviceResetToken));

        return S_OK;
    }

    void MainWindow::OnPanelLoaded(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        m_swapChainPanel = swapChainPanel();

        DWORD Height = 0;
        DWORD Width = 0;
        HRESULT hr = CreateSourceReader(MEDIA_FILE_NAME, &m_sourceReader, &Width, &Height);
        if (S_OK != hr)
            LOG_HR_MSG(hr, "CreateSourceReader");

        // Callbacks invoked by the media engine wrapper
        auto onInitialized = std::bind(&MainWindow::OnMediaInitialized, this);
        auto onError = std::bind(&MainWindow::OnMediaError, this, std::placeholders::_1, std::placeholders::_2);
        auto onPlaybackEnd = std::bind(&MainWindow::OnPlaybackEnd, this);

        MakeAndInitialize<media::MediaEngineWrapper>(&m_mediaEngineWrapper, m_sourceReader.Get(), m_deviceMgr.Get(), 
            onInitialized, onError, onPlaybackEnd, Width, Height);
    }

    HRESULT MainWindow::SetupVideoVisual()
    {
        m_videoSurfaceHandle = m_mediaEngineWrapper ? m_mediaEngineWrapper->GetSurfaceHandle() : NULL;

        if (m_videoSurfaceHandle != NULL)
        {
            // Panel interaction must be in UI thread.
            m_swapChainPanel.DispatcherQueue().TryEnqueue([=]()
                {
                    // Get backing native interface for SwapChainPanel.
                    auto panelNative{ m_swapChainPanel.as<ISwapChainPanelNative2>() };
                    
                    // Set Video Surface from Media Engine to Panel.
                    winrt::check_hresult(
                        panelNative->SetSwapChainHandle(m_videoSurfaceHandle)
                    );
                });
        }
        else
        {
            LOG_HR_MSG(E_FAIL, "Empty Surface Handle");
        }
        return S_OK;
    }

    void MainWindow::OnMediaInitialized()
    {
        // Create video visual and add it to the DCOMP tree
        SetupVideoVisual();

        // Start playback
        m_mediaEngineWrapper->StartPlayingFrom(0);       
    }

    void MainWindow::OnMediaError(MF_MEDIA_ENGINE_ERR error, HRESULT hr)
    {
        LOG_HR_MSG(hr, "MediaEngine error (%d)", error);
    }

    void MainWindow::OnPlaybackEnd()
    {
        MFShutdown();
    }
}
