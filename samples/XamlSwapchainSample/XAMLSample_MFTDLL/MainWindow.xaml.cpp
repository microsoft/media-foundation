#include "pch.h"
#include "MainWindow.xaml.h"
#include "Util.h"

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace Microsoft::WRL::Wrappers;
using namespace winrt::Windows::Media;
using namespace winrt::Microsoft::UI::Xaml;

using Microsoft::WRL::ComPtr;

#include <initguid.h>
// {F1BF74F0-65F4-4F47-925C-E8B564A9A87E}
DEFINE_GUID(CLSID_BasicMFTDLL,
    0xF1BF74F0, 0x65F4, 0x4F47, 0x92, 0x5c, 0xE8, 0xB5, 0x64, 0xA9, 0xA8, 0x7E);

const wchar_t* kMFTDllName = L"..\\BasicMFTDLL.dll";

wil::unique_hmodule g_mftModule;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

HRESULT CreateSourceReader(
    IUnknown* unkDeviceMgr,
    LPCWSTR fileURL,
    BOOL useSoftWareDecoder,
    GUID colorspace,
    IMFSourceReader** reader,
    DWORD* width,
    DWORD* height)
{
    ComPtr<IMFSourceReader> sourceReader;
    ComPtr<IMFMediaType> partialType;
    ComPtr<IMFMediaType> nativeMediaType;
    ComPtr<IMFAttributes> mfAttributes;

    if (unkDeviceMgr != nullptr)
    {
        RETURN_IF_FAILED(MFCreateAttributes(&mfAttributes, 1));
        RETURN_IF_FAILED(mfAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, unkDeviceMgr));
        if (useSoftWareDecoder)
        {
            RETURN_IF_FAILED(mfAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE));
            RETURN_IF_FAILED(mfAttributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, TRUE));
        }
        else
        {
            RETURN_IF_FAILED(mfAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE));
            RETURN_IF_FAILED(mfAttributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, FALSE));
        }
    }
    RETURN_IF_FAILED(MFCreateSourceReaderFromURL(fileURL, mfAttributes.Get(), &sourceReader));
    RETURN_IF_FAILED(sourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE));
    RETURN_IF_FAILED(sourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE));
    RETURN_IF_FAILED(MFCreateMediaType(&partialType));
    RETURN_IF_FAILED(partialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));

    RETURN_IF_FAILED(partialType->SetGUID(MF_MT_SUBTYPE, colorspace));
    RETURN_IF_FAILED(sourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, partialType.Get()));
    RETURN_IF_FAILED(sourceReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &nativeMediaType));
    RETURN_IF_FAILED(MFGetAttributeSize(nativeMediaType.Get(), MF_MT_FRAME_SIZE, (UINT*)width, (UINT*)height));

    *reader = sourceReader.Detach();

    return S_OK;
}


HRESULT ReadUnCompressedSample(
    IMFSourceReader* reader,
    IMFMediaBuffer** buffer,
    LONGLONG* start,
    LONGLONG* duration,
    DWORD* width,
    DWORD* height,
    bool* eos)
{
    ComPtr<IMFMediaType> mediaType;
    ComPtr<IMFSample> sample;
    DWORD streamIndex;
    DWORD flags;
    LONGLONG timestamp;
    RETURN_IF_FAILED(reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &timestamp, &sample));
    *eos = (flags & MF_SOURCE_READERF_ENDOFSTREAM) ? TRUE : FALSE;
    if (*eos)
    {
        return S_OK;
    }

    RETURN_IF_FAILED(sample->GetBufferByIndex(0, buffer));
    RETURN_IF_FAILED(sample->GetSampleTime(start));
    RETURN_IF_FAILED(sample->GetSampleDuration(duration));
    RETURN_IF_FAILED(reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &mediaType));
    RETURN_IF_FAILED(MFGetAttributeSize(mediaType.Get(), MF_MT_FRAME_SIZE, (UINT*)width, (UINT*)height));

    return S_OK;
}

namespace winrt::XAMLSample_MFTDLL::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        MFStartup(MF_VERSION);

        InitializeGraphics();
    }

    void MainWindow::OnPanelLoaded(IInspectable const&, RoutedEventArgs const&)
    {
        winrt::com_ptr<MainWindow> ref;

        util::MFPutWorkItem([&, ref]()
            {
                PlayVideo();
            });
    }

    HRESULT MainWindow::InitializeGraphics()
    {
        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0
        };

        D3D_FEATURE_LEVEL featureLevel;

        ComPtr<IDXGIDevice2> dxgiDevice;
        ComPtr<IDXGIAdapter> dxgiAdapter;
        ComPtr<IDXGIFactory2> dxgiFactory;
        DXGI_SWAP_CHAIN_DESC1 desc = { 0 };
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D10Multithread> multithread;
        D3D11_TEXTURE2D_DESC Desc;
        auto panelNative{ swapChainPanel().as<ISwapChainPanelNative>() };

        RETURN_IF_FAILED(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
            featureLevels,
            _countof(featureLevels),
            D3D11_SDK_VERSION,
            m_device.ReleaseAndGetAddressOf(),
            &featureLevel,
            m_deviceContext.ReleaseAndGetAddressOf()
        ));

        RETURN_IF_FAILED(m_device.As(&multithread));

        multithread->SetMultithreadProtected(TRUE);

        RETURN_IF_FAILED(m_device.As(&dxgiDevice));
        RETURN_IF_FAILED(dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter)));

        RETURN_IF_FAILED(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));

        desc.BufferCount = 2;
        desc.Width = 1920;
        desc.Height = 1080;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

        RETURN_IF_FAILED(dxgiFactory->CreateSwapChainForComposition(m_device.Get(), &desc, nullptr, m_swapChain.ReleaseAndGetAddressOf()));

        winrt::check_hresult(
            panelNative->SetSwapChain(m_swapChain.Get())
        );

        RETURN_IF_FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&texture)));
        RETURN_IF_FAILED(m_device->CreateRenderTargetView(texture.Get(), nullptr, m_backbufferView.ReleaseAndGetAddressOf()));
        m_deviceContext->OMSetRenderTargets(1, m_backbufferView.GetAddressOf(), nullptr);
        texture->GetDesc(&Desc);

        return S_OK;
    }

    HRESULT MainWindow::LoadMftFromDll(IMFTransform** mft)
    {
        // Get current module path
        wchar_t currentModulePath[MAX_PATH] = {};
        RETURN_LAST_ERROR_IF(GetModuleFileNameW(NULL, currentModulePath, ARRAYSIZE(currentModulePath)) == 0);
        std::wstring mftPath = std::filesystem::path(currentModulePath).replace_filename(kMFTDllName).wstring();
        g_mftModule = wil::unique_hmodule(LoadLibraryExW(mftPath.c_str(), NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
        RETURN_LAST_ERROR_IF(!g_mftModule.is_valid());
        OutputDebugString(L"[XAMLSample] Loaded dll.\n");

        typedef HRESULT(__stdcall* dllGetClassObjectPtr)(REFCLSID rclsid, REFIID riid, LPVOID FAR* ppv);
        dllGetClassObjectPtr getClassObject = (dllGetClassObjectPtr)::GetProcAddress(g_mftModule.get(), "DllGetClassObject");
        RETURN_LAST_ERROR_IF(!getClassObject);

        ComPtr<IClassFactory> classFactory;
        RETURN_IF_FAILED(getClassObject(CLSID_BasicMFTDLL, IID_PPV_ARGS(&classFactory)));
        RETURN_IF_FAILED(classFactory->CreateInstance(nullptr, IID_PPV_ARGS(mft)));
        OutputDebugString(L"[XAMLSample] Created instance of MFT from dll.\n");
        return S_OK;
    }

    HRESULT MainWindow::InitializeMFT()
    {
        //Init Variables
        GUID videoSubtype = MFVideoFormat_NV12;
        ComPtr<IMFMediaType> inputType;
        ComPtr<IMFMediaType> decoderOutType;

        MFT_REGISTER_TYPE_INFO outputInfo = { 0 };
        outputInfo.guidMajorType = MFMediaType_Video;
        outputInfo.guidSubtype = MFVideoFormat_ARGB32;

        //Insantiate MFT from dll
        LoadMftFromDll(&m_syncMFT);

        if (!m_syncMFT)
        {
            OutputDebugString(L"[XAMLSample] Failed to create MFT.\n");
            return E_FAIL;
        }

        OutputDebugString(L"[XAMLSample] Successfully created MFT.\n");

        RETURN_IF_FAILED(m_sourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &inputType));

        RETURN_IF_FAILED(m_syncMFT->SetInputType(0, inputType.Get(), 0));

        RETURN_IF_FAILED(MFCreateMediaType(&decoderOutType));
        RETURN_IF_FAILED(decoderOutType->SetGUID(MF_MT_SUBTYPE, outputInfo.guidSubtype));

        HRESULT hr = m_syncMFT->SetOutputType(0, decoderOutType.Get(), 0);
        
        // This check is not explicitly required for this example MFT, but is an example of a fallback path.
        if (FAILED(hr))
        {
            if (outputInfo.guidSubtype == MFVideoFormat_P010)
            {
                // Fallback to 8bits NV12
                RETURN_IF_FAILED(decoderOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12));
                RETURN_IF_FAILED(m_syncMFT->SetOutputType(0, decoderOutType.Get(), 0));
                hr = S_OK;
            }
            else
            {
                RETURN_IF_FAILED(hr);
            }
        }

        // Set messages to start streaming
        RETURN_IF_FAILED(m_syncMFT->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0));
        RETURN_IF_FAILED(m_syncMFT->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));

        OutputDebugString(L"[XAMLSample] MFT setup completed.\n");

        return S_OK;
    }

    HRESULT MainWindow::InitializeVideoProcessor(
        DWORD width,
        DWORD height,
        ID3D11VideoDevice* videoDevice,
        ID3D11VideoProcessorEnumerator** d3d11VideoProcEnum,
        ID3D11VideoProcessor** d3d11VideoProc,
        ID3D11VideoContext* d3d11VideoContext)
    {
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC vpcdesc = {};
        RECT rcDst = {};

        vpcdesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        vpcdesc.InputFrameRate.Numerator = 60;
        vpcdesc.InputFrameRate.Denominator = 1;
        vpcdesc.InputWidth = width;
        vpcdesc.InputHeight = height;
        vpcdesc.OutputFrameRate.Numerator = 60;
        vpcdesc.OutputFrameRate.Denominator = 1;
        vpcdesc.OutputWidth = 1920;
        vpcdesc.OutputHeight = 1080;
        vpcdesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
        RETURN_IF_FAILED(videoDevice->CreateVideoProcessorEnumerator(&vpcdesc, d3d11VideoProcEnum));
        RETURN_IF_FAILED(videoDevice->CreateVideoProcessor(*d3d11VideoProcEnum, 0, d3d11VideoProc));

        d3d11VideoContext->VideoProcessorSetStreamFrameFormat(*d3d11VideoProc, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
        rcDst.right = 1920;
        rcDst.bottom = 1080;
        d3d11VideoContext->VideoProcessorSetStreamDestRect(*d3d11VideoProc, 0, TRUE, &rcDst);

        D3D11_VIDEO_PROCESSOR_COLOR_SPACE streamColorSpace = {};
        streamColorSpace.Usage = 0;
        streamColorSpace.YCbCr_xvYCC = 1;
        streamColorSpace.RGB_Range = 1;
        streamColorSpace.YCbCr_Matrix = 1;
        d3d11VideoContext->VideoProcessorSetStreamColorSpace(*d3d11VideoProc, 0, &streamColorSpace);

        // Output color space
        D3D11_VIDEO_PROCESSOR_COLOR_SPACE outputColorSpace = {};
        outputColorSpace.Usage = 0;
        outputColorSpace.YCbCr_xvYCC = 1;
        outputColorSpace.RGB_Range = 1;
        outputColorSpace.YCbCr_Matrix = 1;
        d3d11VideoContext->VideoProcessorSetOutputColorSpace(*d3d11VideoProc, &outputColorSpace);
        d3d11VideoContext->VideoProcessorSetStreamAutoProcessingMode(*d3d11VideoProc, 0, FALSE/*enableVQ */);

        return S_OK;
    }

    HRESULT MainWindow::DriveMFT(
        IMFSample* inSample,
        IMFMediaBuffer** outBuffer)
    {
        bool hasOutputReady = false;
        MFT_OUTPUT_DATA_BUFFER mftOutputDataBuffer = { 0 };

        // Wait until the sample has finished processing.
        while (!hasOutputReady)
        {
            MFT_OUTPUT_STREAM_INFO outputStreamInfo = {};

            ComPtr<IMFSample> outSample;
            DWORD status = 0;
            memset(&mftOutputDataBuffer, 0, sizeof(mftOutputDataBuffer));

            // Setup the output buffer
            RETURN_IF_FAILED(m_syncMFT->GetOutputStreamInfo(0, &outputStreamInfo));
            if ((outputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0)
            {
                ComPtr<IMFMediaBuffer> outputBuffer;
                RETURN_IF_FAILED(MFCreateAlignedMemoryBuffer(outputStreamInfo.cbSize, outputStreamInfo.cbAlignment, &outputBuffer));
                RETURN_IF_FAILED(MFCreateSample(&outSample));
                RETURN_IF_FAILED(outSample->AddBuffer(outputBuffer.Get()));
                mftOutputDataBuffer.dwStreamID = 0;
                mftOutputDataBuffer.pSample = outSample.Detach();
            }
            else
            {
                mftOutputDataBuffer.pSample = nullptr;
            }
            // Check if the MFT has produced output, or is waiting for input
            HRESULT processOutputResult = m_syncMFT->ProcessOutput(0, 1, &mftOutputDataBuffer, &status);
            if (processOutputResult == MF_E_TRANSFORM_STREAM_CHANGE)
            {
                ComPtr<IMFMediaType> mediaType;
                RETURN_IF_FAILED(m_syncMFT->GetOutputAvailableType(0, 0, &mediaType));
                RETURN_IF_FAILED(m_syncMFT->SetOutputType(0, mediaType.Get(), 0));
            }
            else if (processOutputResult == MF_E_TRANSFORM_NEED_MORE_INPUT)
            {
                // Pass input to the MFT for processing.
                m_syncMFT->ProcessInput(0, inSample, 0);
            }
            else if (SUCCEEDED(processOutputResult))
            {
                // Pass the data to be rendered on the display.
                auto outputHR = mftOutputDataBuffer.pSample->GetBufferByIndex(0, outBuffer);
                if (FAILED(outputHR))
                {
                    OutputDebugString(L"[XAMLSample] Failed to get buffer.\n");
                }
                hasOutputReady = true;
            }

            SAFE_RELEASE(mftOutputDataBuffer.pSample);
            SAFE_RELEASE(mftOutputDataBuffer.pEvents);
        }

        SAFE_RELEASE(mftOutputDataBuffer.pSample);
        SAFE_RELEASE(mftOutputDataBuffer.pEvents);

        return S_OK;
    }

    void MainWindow::PlayVideo()
    {
        DoPresentationLoop();
    }

    HRESULT MainWindow::DoPresentationLoop()
    {
        float clearColor[] = { 0.39f, 0.58f, 0.929f, 0.0f };
        UINT resetToken = 0;
        DWORD width;
        DWORD height;
        HANDLE deviceHandle = NULL;
        ComPtr<ID3D11VideoDevice> videoDevice;
        ComPtr<ID3D11VideoProcessorEnumerator> d3d11VideoProcEnum;
        ComPtr<ID3D11VideoProcessor> d3d11VideoProc;
        ComPtr<ID3D11VideoContext> d3d11VideoContext;
        ComPtr<ID3D11DeviceContext> d3dDDevContext;
        ComPtr<ID3D11Device> d3dDevice;
        LONGLONG totalDecodeTime = 0;
        LONGLONG lastReportedTime = 0;
        LARGE_INTEGER performanceFreq = { 0 };
        LONGLONG decodeTime = 0;
        LONGLONG totalDecodedFrames = 0;
        LONGLONG lastReportedDecodedFrames = 0;
        double dperformanceFreq;

        QueryPerformanceFrequency(&performanceFreq), HRESULT_FROM_WIN32(GetLastError());
        dperformanceFreq = double(performanceFreq.QuadPart) / 1000.0;

        MFCreateDXGIDeviceManager(&resetToken, m_deviceMgr.ReleaseAndGetAddressOf());
        m_deviceMgr->ResetDevice(m_device.Get(), resetToken);

        RETURN_IF_FAILED(CreateSourceReader(m_deviceMgr.Get(), MEDIA_FILE_NAME, FALSE, MFVideoFormat_NV12, &m_sourceReader, &width, &height));

        m_deviceContext->ClearRenderTargetView(nullptr, clearColor);

        RETURN_IF_FAILED(m_deviceMgr->OpenDeviceHandle(&deviceHandle));
        m_device->GetImmediateContext(&d3dDDevContext);

        RETURN_IF_FAILED(InitializeMFT());

        RETURN_IF_FAILED(m_deviceMgr->GetVideoService(deviceHandle, __uuidof(ID3D11VideoDevice), (void**)(videoDevice.ReleaseAndGetAddressOf())));
        d3dDDevContext.As(&d3d11VideoContext);

        RETURN_IF_FAILED(InitializeVideoProcessor(width, height, videoDevice.Get(), &d3d11VideoProcEnum, &d3d11VideoProc, d3d11VideoContext.Get()));

        while (true)
        {
            // clear the window to a deep blue
            bool fEOS;
            LONGLONG start;
            LONGLONG duration;
            ComPtr<IMFDXGIBuffer> dxgiBuffer;
            ComPtr<ID3D11Texture2D> resourceTexture;
            ComPtr<IMFMediaBuffer> buffer;
            ComPtr<ID3D11Texture2D> backTexture;
            ComPtr<ID3D11VideoProcessorInputView> inputView;
            ComPtr<ID3D11VideoProcessorOutputView> outputView;
            UINT indexSrc;

            static const FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

            LARGE_INTEGER startTime = { 0 };
            LARGE_INTEGER endTime = { 0 };

            QueryPerformanceCounter(&startTime);

            // Read the next sample from the source
            RETURN_IF_FAILED(ReadUnCompressedSample(m_sourceReader.Get(), &buffer, &start, &duration, &width, &height, &fEOS));

            if (fEOS)
            {
                // Notify the MFT of End of stream to allow it to cleanup.
                RETURN_IF_FAILED(m_syncMFT->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0));
                RETURN_IF_FAILED(m_syncMFT->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0));
                break;
            }

            // MFT driving

            // Add the buffer from the source to a sample
            ComPtr<IMFSample> inSample;
            RETURN_IF_FAILED(MFCreateSample(&inSample));
            RETURN_IF_FAILED(inSample->AddBuffer(buffer.Get()));

            ComPtr<IMFMediaBuffer> outBuffer;

            RETURN_IF_FAILED(DriveMFT(inSample.Get(), &outBuffer));


            // Get the DXGI buffer from the output of the MFT
            outBuffer->QueryInterface(IID_PPV_ARGS(&dxgiBuffer));
            RETURN_IF_FAILED(dxgiBuffer->GetResource(IID_PPV_ARGS(&resourceTexture)));
            RETURN_IF_FAILED(dxgiBuffer->GetSubresourceIndex(&indexSrc));

            // convert NV12 decoded texture to backbuffer using video proc
            D3D11_VIDEO_PROCESSOR_STREAM vpStream = {};
            D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputdesc = {};
            D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc = {};

            RETURN_IF_FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backTexture)));

            // render decoder texture to backbuffer using video processor

            inputdesc.Texture2D.MipSlice = 0;
            inputdesc.Texture2D.ArraySlice = indexSrc;
            inputdesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;

            RETURN_IF_FAILED(videoDevice->CreateVideoProcessorInputView(resourceTexture.Get(),
                d3d11VideoProcEnum.Get(),
                &inputdesc,
                &inputView));

            outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2DARRAY;
            outputDesc.Texture2DArray.FirstArraySlice = 0;
            outputDesc.Texture2DArray.ArraySize = 1;
            outputDesc.Texture2DArray.MipSlice = 0;
            RETURN_IF_FAILED(videoDevice->CreateVideoProcessorOutputView(backTexture.Get(),
                d3d11VideoProcEnum.Get(),
                &outputDesc,
                &outputView));

            vpStream.Enable = TRUE;
            vpStream.pInputSurface = inputView.Get();

            RETURN_IF_FAILED(d3d11VideoContext->VideoProcessorBlt(d3d11VideoProc.Get(), outputView.Get(), 0, 1, &vpStream));

            RETURN_IF_FAILED(m_swapChain->Present(1, 0));

            // Stats logging
            QueryPerformanceCounter(&endTime);
            decodeTime = endTime.QuadPart - startTime.QuadPart;
            totalDecodeTime += decodeTime;
            totalDecodedFrames++;

            if (double(totalDecodeTime - lastReportedTime) > 1000.0 * dperformanceFreq)
            {
                wchar_t szOutputBuffer[1024];
                StringCchPrintfW(szOutputBuffer, _countof(szOutputBuffer), L"[XAMLSample] %d Frames were decoded in %f ms, at %f FPS\n", totalDecodedFrames - lastReportedDecodedFrames, double(totalDecodeTime - lastReportedTime) / dperformanceFreq, 1000.0 * (totalDecodedFrames - lastReportedDecodedFrames) * dperformanceFreq / (totalDecodeTime - lastReportedTime));
                OutputDebugStringW(szOutputBuffer);
                lastReportedTime = totalDecodeTime;
                lastReportedDecodedFrames = totalDecodedFrames;
            }
        }

        // Notify the MFT of End of stream to allow it to cleanup.
        RETURN_IF_FAILED(m_syncMFT->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0));
        RETURN_IF_FAILED(m_syncMFT->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0));

        wchar_t szOutputBuffer[1024];
        StringCchPrintfW(szOutputBuffer, _countof(szOutputBuffer), L"[XAMLSample] A total of %d Frames were decoded in %f ms, at %f FPS\n", totalDecodedFrames, double(totalDecodeTime) / dperformanceFreq, 1000.0 * totalDecodedFrames * dperformanceFreq / totalDecodeTime);
        OutputDebugStringW(szOutputBuffer);

        m_sourceReader = nullptr;
        MFShutdown();
        return S_OK;
    }
}
