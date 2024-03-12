#pragma once

#include "pch.h"
#include <wrl\wrappers\corewrappers.h>
#include <mfapi.h>

using namespace Microsoft::WRL;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::CriticalSection;

class __declspec(uuid("F1BF74F0-65F4-4F47-925C-E8B564A9A87E")) BasicMFT :
    public RuntimeClass <
    RuntimeClassFlags< RuntimeClassType::WinRtClassicComMix >,
    IMFTransform >
{
public:
    IFACEMETHOD(RuntimeClassInitialize)() { return S_OK; }

    BasicMFT() {}

    // IMFTransform
    IFACEMETHODIMP GetStreamLimits(DWORD* pdwInputMinimum, DWORD* pdwInputMaximum, DWORD* pdwOutputMinimum, DWORD* pdwOutputMaximum);
    IFACEMETHODIMP GetStreamCount(DWORD* pcInputStreams, DWORD* pcOutputStreams);
    IFACEMETHODIMP GetInputStreamInfo(DWORD dwInputStreamID, MFT_INPUT_STREAM_INFO* pStreamInfo);
    IFACEMETHODIMP GetOutputStreamInfo(DWORD dwOutputStreamID, MFT_OUTPUT_STREAM_INFO* pStreamInfo);
    IFACEMETHODIMP GetAttributes(IMFAttributes** pAttributes) { return E_NOTIMPL; }
    IFACEMETHODIMP GetInputAvailableType(DWORD dwInputStreamID, DWORD dwTypeIndex, IMFMediaType** ppType) { return E_NOTIMPL; }
    IFACEMETHODIMP GetOutputAvailableType(DWORD dwOutputStreamID, DWORD dwTypeIndex, IMFMediaType** ppType) { return E_NOTIMPL; }
    IFACEMETHODIMP SetInputType(DWORD dwInputStreamID, IMFMediaType* pType, DWORD dwFlags);
    IFACEMETHODIMP SetOutputType(DWORD dwOutputStreamID, IMFMediaType* pType, DWORD dwFlags);
    IFACEMETHODIMP GetInputCurrentType(DWORD dwInputStreamID, IMFMediaType** ppType);
    IFACEMETHODIMP GetOutputCurrentType(DWORD dwOutputStreamID, IMFMediaType** ppType);
    IFACEMETHODIMP GetInputStatus(DWORD dwInputStreamID, DWORD* pdwFlags);
    IFACEMETHODIMP GetOutputStatus(DWORD* pdwFlags) { return E_NOTIMPL; }
    IFACEMETHODIMP ProcessMessage(MFT_MESSAGE_TYPE eMessage, ULONG_PTR ulParam);
    IFACEMETHODIMP ProcessInput(DWORD dwInputStreamID, IMFSample* pSample, DWORD dwFlags);
    IFACEMETHODIMP ProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount, MFT_OUTPUT_DATA_BUFFER* pOutputSamples, DWORD* pdwStatus);
    IFACEMETHODIMP GetOutputStreamAttributes(DWORD dwOutputStreamID, IMFAttributes** ppAttributes) { return E_NOTIMPL; }
    IFACEMETHODIMP GetStreamIDs(DWORD, DWORD*, DWORD, DWORD*) { return E_NOTIMPL; }
    IFACEMETHODIMP GetInputStreamAttributes(DWORD, IMFAttributes**) { return E_NOTIMPL; }
    IFACEMETHODIMP DeleteInputStream(DWORD) { return E_NOTIMPL; }
    IFACEMETHODIMP AddInputStreams(DWORD, DWORD*) { return E_NOTIMPL; }
    IFACEMETHODIMP SetOutputBounds(LONGLONG, LONGLONG) { return E_NOTIMPL; }
    IFACEMETHODIMP ProcessEvent(DWORD, IMFMediaEvent*) { return E_NOTIMPL; }

private:
    bool                        m_streamingInitialized = false;
    ComPtr<IMFSample>           m_sample;                 // Input sample.
    ComPtr<IMFMediaType>        m_inputType;              // Input media type.
    ComPtr<IMFMediaType>        m_outputType;             // Output media type.

    CriticalSection             m_critSec;
};