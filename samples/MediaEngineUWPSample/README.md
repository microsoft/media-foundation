# MediaEngineUWPSample

This is a sample UWP application written in C++ which demonstrates media playback using the MediaFoundation [MediaEngine](https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nn-mfmediaengine-imfmediaengine) API and the WinRT composition [visual layer](https://docs.microsoft.com/en-us/windows/uwp/composition/visual-layer) APIs.

This sample works on all Windows SKUs including Xbox, Desktop, HoloLens and IoTCore.

![Alt text](docs/screenshot.jpg?raw=true "MediaEngineUWPSample screenshot")

## Build instructions

1. Open MediaEngineUWPSample.sln in Visual Studio 2019 or newer (ensure that you have version 17134 or newer of the Windows SDK installed).
2. Build solution

## Details

The purpose of this sample is to demonstrate how to achieve efficient media playback in a UWP application which needs low level control over UI rendering and potentially also needs to employ custom logic for fetching and demuxing media content. 

The general architecture of such an application is shown below:
![Alt text](docs/architecture_diagram.png?raw=true "Application architecture diagram")

A simple way to achieve efficient media playback on Windows is to leverage the Media Foundation [MediaEngine](https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nn-mfmediaengine-imfmediaengine) API. The MediaEngine encapsulates a highly efficient Media Foundation pipeline which leverages hardware acceleration for video decoding, processing and rendering (when available). The pipeline also applies additional optimizations typically not employed by other media API implementations such as deep video frame queueing / batching, audio offload and the correct tagging of work using [MMCSS](https://docs.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service) for efficient CPU utilization without sacrificing quality.

The WinRT composition APIs can be leveraged to compose the video rendered by the MediaEngine within the application window. The application can render its UI elements using low level D3D APIs and present to a D3D11 swapchain. In order to integrate with the visual layer, this D3D11 swapchain needs to be a composition swapchain (created via IDXGIFactoryMedia::CreateSwapChainForCompositionSurfaceHandle) which can be associated with a composition surface. This composition surface is set on a CompositionSurfaceBrush which is subsequently associated with a CompositionVisual in the visual tree. The application manages the visual tree to control the position and layering of visual elements (including the video) within the window.