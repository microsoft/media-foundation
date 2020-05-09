**Important Note**

This sample leverages APIs that are not in the app API partition (see [this](src/media/MediaEngineProtectedContentInterfaces.h) file). The APIs in question are planned to be moved to the app API partition in the next release of the SDK (20H2), however until then you will not be able to submit an application to the Windows Store which uses these APIs.

# MediaEngineEMEUWPSample

This is a sample UWP application written in C++ which demonstrates protected Playready EME media playback using the MediaFoundation MediaEngine API and the WinRT composition APIs. This sample can run on all Windows SKUs including Xbox, Desktop, HoloLens and IoTCore.

![Alt text](docs/screenshot.jpg?raw=true "MediaEngineEMEUWPSample screenshot")

## Build instructions

1. Open MediaEngineEMEUWPSample.sln in Visual Studio 2019 or newer (ensure that you have version *19041* or newer of the Windows SDK installed).
2. Build solution

## Details

This sample is an extended version of the MediaEngineUWPSample which allows for playback of protected content using an API which aligns with the [Encrypted Media Extensions](https://w3c.github.io/encrypted-media/) specification. As with the MediaEngineUWPSample, this sample demonstrates how to achieve efficient media playback in a UWP application which needs low level control over UI rendering and potentially also needs to employ custom logic for fetching and demuxing media content. 

The following diagram shows the architecture of the application:
![Alt text](docs/architecture_diagram.png?raw=true "Application architecture diagram")