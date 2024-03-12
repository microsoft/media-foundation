#include "pch.h"
#include "BasicMFT.h"


IFACEMETHODIMP_(HRESULT __stdcall) BasicMFT::GetStreamLimits(DWORD* pdwInputMinimum, DWORD* pdwInputMaximum, DWORD* pdwOutputMinimum, DWORD* pdwOutputMaximum)
{
    if ((pdwInputMinimum == nullptr) || (pdwInputMaximum == nullptr) || (pdwOutputMinimum == nullptr) || (pdwOutputMaximum == nullptr))
    {
        return E_POINTER;
    }

    // This MFT has a fixed number of streams.
    *pdwInputMinimum = 1;
    *pdwInputMaximum = 1;
    *pdwOutputMinimum = 1;
    *pdwOutputMaximum = 1;
    return S_OK;
}

IFACEMETHODIMP_(HRESULT __stdcall) BasicMFT::GetStreamCount(DWORD* pcInputStreams, DWORD* pcOutputStreams)
{
    if ((pcInputStreams == nullptr) || (pcOutputStreams == nullptr))
    {
        return E_POINTER;
    }

    // This MFT has a fixed number of streams.
    *pcInputStreams = 1;
    *pcOutputStreams = 1;
    return S_OK;
}

IFACEMETHODIMP BasicMFT::GetInputStreamInfo(DWORD dwInputStreamID, MFT_INPUT_STREAM_INFO* pStreamInfo)
{
    if (dwInputStreamID != 0)
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    if (pStreamInfo == nullptr)
    {
        return E_POINTER;
    }

    pStreamInfo->dwFlags = MFT_INPUT_STREAM_PROCESSES_IN_PLACE;

    return S_OK;
}

IFACEMETHODIMP BasicMFT::GetOutputStreamInfo(DWORD dwOutputStreamID, MFT_OUTPUT_STREAM_INFO* pStreamInfo)
{
    if (dwOutputStreamID != 0)
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    if (pStreamInfo == nullptr)
    {
        return E_POINTER;
    }

    pStreamInfo->dwFlags = MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;

    return S_OK;
}

IFACEMETHODIMP_(HRESULT __stdcall) BasicMFT::SetInputType(DWORD dwInputStreamID, IMFMediaType* pType, DWORD dwFlags)
{
    if (dwInputStreamID != 0)
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    RETURN_IF_FAILED(MFCreateMediaType(&m_inputType));
    RETURN_IF_FAILED(pType->CopyAllItems(m_inputType.Get()));
    return S_OK;
}

IFACEMETHODIMP_(HRESULT __stdcall) BasicMFT::SetOutputType(DWORD dwOutputStreamID, IMFMediaType* pType, DWORD dwFlags)
{
    if (dwOutputStreamID != 0)
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    RETURN_IF_FAILED(MFCreateMediaType(&m_outputType));
    RETURN_IF_FAILED(pType->CopyAllItems(m_outputType.Get()));
    return S_OK;
}

IFACEMETHODIMP_(HRESULT __stdcall) BasicMFT::GetInputCurrentType(DWORD dwInputStreamID, IMFMediaType** ppType)
{
    if (ppType == nullptr)
    {
        return E_POINTER;
    }

    if (dwInputStreamID != 0)
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    if (!m_inputType)
    {
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    return m_inputType->CopyAllItems(*ppType);
}

IFACEMETHODIMP_(HRESULT __stdcall) BasicMFT::GetOutputCurrentType(DWORD dwOutputStreamID, IMFMediaType** ppType)
{
    if (ppType == nullptr)
    {
        return E_POINTER;
    }

    if (dwOutputStreamID != 0)
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    if (!m_outputType)
    {
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    return m_outputType->CopyAllItems(*ppType);
}

IFACEMETHODIMP BasicMFT::GetInputStatus(DWORD dwInputStreamID, DWORD* pdwFlags)
{
    if (dwInputStreamID != 0)
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    if (m_inputType == nullptr || m_outputType == nullptr)
    {
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    *pdwFlags = m_sample == nullptr ? 1 : 0;

    return S_OK;
}

IFACEMETHODIMP_(HRESULT __stdcall) BasicMFT::ProcessMessage(MFT_MESSAGE_TYPE eMessage, ULONG_PTR ulParam)
{
    switch (eMessage)
    {
    case MFT_MESSAGE_COMMAND_FLUSH:
        // Flush the MFT.
        m_sample = nullptr;
        break;

    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        m_streamingInitialized = true;
        break;

    case MFT_MESSAGE_NOTIFY_END_STREAMING:
        m_streamingInitialized = false;
        break;

        // The remaining messages require no action
    case MFT_MESSAGE_SET_D3D_MANAGER:
    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
    case MFT_MESSAGE_COMMAND_DRAIN:
        break;
    }
    return S_OK;
}

IFACEMETHODIMP_(HRESULT __stdcall) BasicMFT::ProcessInput(DWORD dwInputStreamID, IMFSample* pSample, DWORD dwFlags)
{
    // Check input parameters.
    if (pSample == nullptr)
    {
        return E_POINTER;
    }

    if (dwInputStreamID != 0)
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    auto lock = m_critSec.Lock();

    // Check for valid media types.
    // The client must set input and output types before calling ProcessInput.
    // Check if an input sample is already queued.
    if (!m_inputType || !m_outputType || (m_sample != nullptr))
    {
        return MF_E_NOTACCEPTING;
    }

    // Cache the sample. We do the actual work in ProcessOutput.
    m_sample = pSample;
    return S_OK;
}

IFACEMETHODIMP_(HRESULT __stdcall) BasicMFT::ProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount, MFT_OUTPUT_DATA_BUFFER* pOutputSamples, DWORD* pdwStatus)
{
    if (dwFlags != 0 || cOutputBufferCount != 1)
    {
        return E_INVALIDARG;
    }

    if (pOutputSamples == nullptr || pdwStatus == nullptr)
    {
        return E_POINTER;
    }

    if (pOutputSamples->pSample != nullptr)
    {
        return E_INVALIDARG;
    }

    auto lock = m_critSec.Lock();

    if (m_sample == nullptr)
    {
        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }

    RETURN_IF_FAILED(m_sample.CopyTo(IID_PPV_ARGS(&pOutputSamples[0].pSample)));
    pOutputSamples[0].dwStatus = 0;

    m_sample = nullptr;

    return S_OK;
}
