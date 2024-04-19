
#include "pch.h"
#include "MediaFoundationStreamWrapper.h"
#include "MediaFoundationSourceWrapper.h"

using namespace Microsoft::WRL;

namespace media
{
    MediaFoundationStreamWrapper::MediaFoundationStreamWrapper() = default;
    MediaFoundationStreamWrapper::~MediaFoundationStreamWrapper() = default;

    HRESULT MediaFoundationStreamWrapper::RuntimeClassInitialize(uint32_t sourceStreamId, uint32_t mediaStreamId, GUID streamType, IMFMediaSource* parentSource, IMFSourceReader* sourceReader)
    {
        m_streamType = streamType;
        m_sourceStreamId = sourceStreamId;
        m_mediaStreamId = mediaStreamId;
        m_sourceReader = sourceReader;
        m_parentSource = parentSource;
        RETURN_IF_FAILED(GenerateStreamDescriptor());
        RETURN_IF_FAILED(MFCreateEventQueue(&m_mediaEventQueue));
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::GetMediaSource(IMFMediaSource** mediaSourceOut)
    {
        auto lock = m_lock.lock();
        if (!m_parentSource)
            return MF_E_SHUTDOWN;

        RETURN_IF_FAILED(m_parentSource.CopyTo(mediaSourceOut));
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::GetStreamDescriptor(IMFStreamDescriptor** streamDescriptorOut)
    {
        if (!m_streamDescriptor)
            return MF_E_NOT_INITIALIZED;

        RETURN_IF_FAILED(m_streamDescriptor.CopyTo(streamDescriptorOut));
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::RequestSample(IUnknown* token) 
    {
        auto lock = m_lock.lock();
        // If token is nullptr, we still want to push it to represent a sample request from MF.
        m_pendingSampleRequestTokens.push(token);

        ComPtr<IMFSample> sample;
        DWORD flags;
        LONGLONG timestamp;
        DWORD actualStreamIndex;

        RETURN_IF_FAILED(m_sourceReader->ReadSample((DWORD)m_sourceStreamId, 0, &actualStreamIndex, &flags, &timestamp, &sample));
        if (actualStreamIndex != m_sourceStreamId)
            return E_FAIL;

        if( flags & MF_SOURCE_READERF_ENDOFSTREAM)
        {
            RETURN_IF_FAILED(m_mediaEventQueue->QueueEventParamUnk(MEEndOfStream, GUID_NULL, S_OK, nullptr));
            m_streamEnded = true;
            static_cast<MediaFoundationSourceWrapper*>(m_parentSource.Get())->CheckForEndOfPresentation();
        }       
        else
        {
            ComPtr<IUnknown> request_token = m_pendingSampleRequestTokens.front();
            if (request_token) 
                RETURN_IF_FAILED(sample->SetUnknown(MFSampleExtension_Token, request_token.Get()));

            RETURN_IF_FAILED(m_mediaEventQueue->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, sample.Get()));
            m_pendingSampleRequestTokens.pop();
        }
        return S_OK;
    }

    GUID MediaFoundationStreamWrapper::StreamType() const
    {
        return m_streamType;
    }

    HRESULT MediaFoundationStreamWrapper::GetEvent(DWORD flags, IMFMediaEvent** eventOut)
    {
        // Not tracing hr to avoid the noise from MF_E_NO_EVENTS_AVAILABLE.
        if (!m_mediaEventQueue)
            return E_FAIL;

        return m_mediaEventQueue->GetEvent(flags, eventOut);
    }

    HRESULT MediaFoundationStreamWrapper::BeginGetEvent(IMFAsyncCallback* callback, IUnknown* state) 
    {
        if (!m_mediaEventQueue)
            return E_FAIL;

        RETURN_IF_FAILED(m_mediaEventQueue->BeginGetEvent(callback, state));
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::EndGetEvent(IMFAsyncResult* result,
        IMFMediaEvent** eventOut)
    {
        if (!m_mediaEventQueue)
            return E_FAIL;

        RETURN_IF_FAILED(m_mediaEventQueue->EndGetEvent(result, eventOut));
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::QueueEvent(MediaEventType type, REFGUID extendedType, HRESULT status, const PROPVARIANT* value) 
    {
        if (!m_mediaEventQueue)
            return E_FAIL;

        RETURN_IF_FAILED(m_mediaEventQueue->QueueEventParamVar(type, extendedType, status, value));
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::GenerateStreamDescriptor() 
    {
        ComPtr<IMFMediaType> media_type;
        IMFMediaType** mediaTypes = &media_type;
        RETURN_IF_FAILED(GetMediaType(&media_type));
        RETURN_IF_FAILED(MFCreateStreamDescriptor(m_mediaStreamId, 1, mediaTypes, &m_streamDescriptor));
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::GetMediaType(IMFMediaType** mediaTypeOut)
    {
        m_sourceReader->GetCurrentMediaType(m_sourceStreamId, mediaTypeOut);
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::QueueStartedEvent(const PROPVARIANT* startPosition)
    {
        m_state = State::kStarted;
        RETURN_IF_FAILED(m_mediaEventQueue->QueueEventParamVar(MEStreamStarted, GUID_NULL, S_OK, startPosition));
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::QueueSeekedEvent(const PROPVARIANT* startPosition)
    {
        m_state = State::kStarted;
        RETURN_IF_FAILED(m_mediaEventQueue->QueueEventParamVar(MEStreamSeeked, GUID_NULL, S_OK, startPosition));
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::QueueStoppedEvent() 
    {
        m_state = State::kStopped;
        RETURN_IF_FAILED(m_mediaEventQueue->QueueEventParamVar(MEStreamStopped, GUID_NULL, S_OK, nullptr));
        return S_OK;
    }

    HRESULT MediaFoundationStreamWrapper::QueuePausedEvent() 
    {
        m_state = State::kPaused;
        RETURN_IF_FAILED(m_mediaEventQueue->QueueEventParamVar(MEStreamPaused, GUID_NULL, S_OK, nullptr));
        return S_OK;
    }

    void MediaFoundationStreamWrapper::SetSelected(bool selected) 
    {
        auto lock = m_lock.lock();
        m_selected = selected;
    }

    bool MediaFoundationStreamWrapper::IsSelected() 
    {
        auto lock = m_lock.lock();
        return m_selected;
    }

    bool MediaFoundationStreamWrapper::HasEnded()
    {
        auto lock = m_lock.lock();
        return m_streamEnded;
    }
}