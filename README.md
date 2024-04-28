## What is this repo?
A collection of Microsoft Media Foundation sample apps along with tooling and documentation. Our goal is to share code samples, documentation, our favorite tools, and tips and tricks for troubleshooting Media Foundation issues. 

## Samples
- [**(new)** MediaEngine CustomSource Xaml Sample](https://github.com/microsoft/media-foundation/tree/master/samples/MediaEngineCustomSourceXamlSample) - A sample C++ UWP application that shows an implementation of a Media pipeline using Media Engine with a "Custom Media Source". The custom source allows greater control in the data passed to Media Engine.
- [**(new)** Xaml Swapchain with MFT Sample](https://github.com/microsoft/media-foundation/tree/master/samples/XamlSwapchainSample) - A sample C++ UWP application that shows an implementation of a media pipeline using a XAML swapchain panel and an MFT (Media Foundation Transform). The sample gives an outline of how an MFT might be used in a wide range of scenarios such as decoding to ML effects on video frames.
- [MediaEngineUWP](https://github.com/microsoft/media-foundation/tree/master/samples/MediaEngineUWPSample) - A sample UWP C++/WinRT application which demonstrates media playback using the MediaFoundation MediaEngine API and the WinRT composition APIs.
- [MediaEngineEMEUWP](https://github.com/microsoft/media-foundation/tree/master/samples/MediaEngineEMEUWPSample) - A sample UWP application written in C++ which demonstrates protected Playready EME media playback using the MediaFoundation MediaEngine API and the WinRT composition APIs.
- [MediaEngineDCompWin32Sample](https://github.com/microsoft/media-foundation/tree/master/samples/MediaEngineDCompWin32Sample) - A sample native C++ Win32 application which demonstrates media playback using the MediaFoundation MediaEngine API and the DirectComposition API.
- [storeCDM](https://github.com/microsoft/media-foundation/tree/master/samples/storeCDM) - A UWP app which loads a native implementation of the [clearkey](https://www.w3.org/TR/encrypted-media/#clear-key) CDM.

## Documentation
- [Media Foundation SDK](https://docs.microsoft.com/en-us/windows/win32/medfound/microsoft-media-foundation-sdk)
- [Media Foundation Programming Guide](https://docs.microsoft.com/en-us/windows/win32/medfound/media-foundation-programming-guide)
- [Media Foundation Programming Reference](https://docs.microsoft.com/en-us/windows/win32/medfound/media-foundation-programming-reference)

## Tracing and Debugging
- [Capturing Media Foundation Traces](https://docs.microsoft.com/en-us/windows/win32/medfound/using-mftrace#interpreting-the-trace-results) 
- [Identifying Video Rendering Related Issues](./performanceTracing/README.md)

## Microsoft Tools
1. Media Experience Analyzer (MXA) - An advanced analysis tool used by Media experts to analyze Media Foundation performance traces.
    [![MXA Screenshot](./images/mxa_small.png)](./images/mxa.png)

    - Available for download packaged with the Windows ADK [here](https://docs.microsoft.com/en-us/windows-hardware/get-started/adk-install). You can opt to install only MXA using the installer.
    [![MXA ADK Installer](./images/adk_installer.png)](./images/adk_installer.png)
    - Microsofts Channel9 has produced a series of training videos for MXA available [here](./MXAVideoList.md)


2. [GPUView](https://docs.microsoft.com/en-us/windows-hardware/drivers/display/using-gpuview) - A development tool for determining the performance of the graphics processing unit (GPU) and CPU. It looks at performance with regard to direct memory access (DMA) buffer processing and all other video processing on the video hardware.
    - Available in the [Windows SDK](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk/)
    - Also available in the [Windows Performance Toolkit](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/)

    [![GPUView Screenshot](./images/gpuview_small.png)](./images/gpuview.png)

3. [TopoEdit](https://docs.microsoft.com/en-us/windows/win32/medfound/topoedit) - A visual tool for building and testing Media Foundation topologies.

    [![TopoEdit Screenshot](./images/topo_small.png)](./images/topo.png)

## Other useful links
- [Book: Developing Microsoft Media Foundation Applications](https://www.amazon.com/Developing-Microsoft-Foundation-Applications-Developer/dp/0735656592)

## Contributing

We're always looking for your help to fix bugs and improve the samples. Create a pull request, and we'll be happy to take a look.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
