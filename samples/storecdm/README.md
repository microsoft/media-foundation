# clearkeyStoreCDM

This solution provides a sample for loading a Content Decryption Module (CDM) using a Universal Windows Platform (UWP) application. It also provides a UWP app for testing. 

## Sample Details

The solution contains two projects:
1. A native DLL which implements the [clearkey](https://www.w3.org/TR/encrypted-media/#clear-key) CDM
2. A UWP application which contains information on what DLL to load (currently the clearkeyDLL)

**Note** The Universal Windows app samples require Visual Studio 2019 to build and Windows 10 to execute.
 
To obtain information about Windows 10 development, go to the [Windows Dev Center](https://dev.windows.com)

To obtain information about Microsoft Visual Studio 2019 and the tools for developing Windows apps, go to [Visual Studio 2019](http://go.microsoft.com/fwlink/?LinkID=532422)
Note: Make sure to check Universal Windows App Development Tools during Visual Studio 2019 setup to install the Windows 10 SDK which is required for building this sample.

## System requirements

**Client:** Windows 10
**Feature:** store CDM

## Build the sample

1. Start Microsoft Visual Studio 2019 and select **File** \> **Open** \> **Project/Solution** to open the clearkeyStoreCDM solution
2. Right click on the UWP project and select **Publish** \> **Create App Packages...**
3. Follow the prompts for sideloading the app (ARM builds have not been tested, unchecking ARM build is recommended)
4. Build location will pop-up after build is complete if there are no errors

## Install the sample

1. Copy the whole App Package output directory to your windows 10 machine (VM is acceptable as well)
2. Navigate to the directory with the App Package and in an admin powershell window run **Add-AppDevPackage.ps1** 
3. Sign the native DLL located in **C:\Program Files\WindowsApps\\\*clearkeyStoreCDM***

## Test the sample

Publicly built apps should access the store CDM is by using the [CreateContentDecryptionModuleFactory](https://github.com/microsoft/media-foundation/blob/969f38b9fff9892f5d75bc353c72d213da807739/samples/MediaEngineEMEUWPSample/src/media/MediaEngineProtectedContentInterfaces.h#:~:text=virtual%20HRESULT%20STDMETHODCALLTYPE%20CreateContentDecryptionModuleFactory() API. This API was added to the [IMFMediaEngineClassFactory](https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nn-mfmediaengine-imfmediaengineclassfactory) interface in the 19041 version of the SDK.

## Debug the sample

Visual Studio remote debugger tools can be used to attach to the clearkeyDLL process for debugging on another machine.

