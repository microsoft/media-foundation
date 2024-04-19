#include "pch.h"
#include "MediaFoundationSourceWrapper.h"

using namespace Microsoft::WRL;

namespace media
{
    MediaFoundationSourceWrapper::MediaFoundationSourceWrapper() = default;
    MediaFoundationSourceWrapper::~MediaFoundationSourceWrapper() = default;

    HRESULT MediaFoundationSourceWrapper::RuntimeClassInitialize(IMFSourceReader* sourceReader)
    {
        m_sourceReader = sourceReader;
        BOOL validStream = false;
        DWORD streamIndexIn = 0;
        DWORD mediaStreamIndex = 0;
        while (true)
        {
            validStream = false;
            HRESULT hr = m_sourceReader->GetStreamSelection(streamIndexIn, &validStream);
            if (MF_E_INVALIDSTREAMNUMBER == hr)
            {
                hr = S_OK;
                break;
            }

            if (validStream)
            {
                ComPtr<IMFMediaType> spMT;
                RETURN_IF_FAILED(m_sourceReader->GetCurrentMediaType(streamIndexIn, &spMT));
                GUID guidMajorType;
                RETURN_IF_FAILED(spMT->GetMajorType(&guidMajorType));
                if(guidMajorType == MFMediaType_Audio)
                {
                    ComPtr<MediaFoundationStreamWrapper> stream;
                    RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationStreamWrapper>(&stream, streamIndexIn, mediaStreamIndex, MFMediaType_Audio, this, sourceReader));
                    m_mediaStreams.push_back(stream);
                    mediaStreamIndex++;
                }
                else if (guidMajorType == MFMediaType_Video)
                {
                    ComPtr<MediaFoundationStreamWrapper> stream;
                    RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationStreamWrapper>(&stream, streamIndexIn, mediaStreamIndex, MFMediaType_Video, this, sourceReader));
                    m_mediaStreams.push_back(stream);
                    mediaStreamIndex++;
                }    
            }
            streamIndexIn++;
        }
        RETURN_IF_FAILED(MFCreateEventQueue(&m_mediaEventQueue));
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::GetCharacteristics(DWORD* characteristics)
    {
        if (m_state == State::kShutdown)
            return MF_E_SHUTDOWN;

        *characteristics = MFMEDIASOURCE_CAN_SEEK;
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::SelectDefaultStreams(const DWORD streamDescCount, IMFPresentationDescriptor* presentationDescriptor)
    {
        bool audioStreamSelected = false;
        bool videoStreamSelected = false;
        for (DWORD idx = 0; idx < streamDescCount; idx++)
        {
            ComPtr<IMFStreamDescriptor> streamDescriptor;
            BOOL selected;
            RETURN_IF_FAILED(presentationDescriptor->GetStreamDescriptorByIndex(idx, &selected, &streamDescriptor));
            if (selected)
                continue;

            DWORD streamId;
            RETURN_IF_FAILED(streamDescriptor->GetStreamIdentifier(&streamId));
            if (IsEqualGUID(m_mediaStreams[streamId]->StreamType(), MFMediaType_Audio) && !audioStreamSelected)
            {
                audioStreamSelected = true;
                RETURN_IF_FAILED(presentationDescriptor->SelectStream(idx));
            }
            if (IsEqualGUID(m_mediaStreams[streamId]->StreamType(), MFMediaType_Video) && !videoStreamSelected)
            {
                videoStreamSelected = true;
                RETURN_IF_FAILED(presentationDescriptor->SelectStream(idx));
            }
        }
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::CreatePresentationDescriptor(IMFPresentationDescriptor** presentationDescriptorOut)
    {
        if (m_state == State::kShutdown)
            return MF_E_SHUTDOWN;

        ComPtr<IMFPresentationDescriptor> presentationDescriptor;
        std::vector<ComPtr<IMFStreamDescriptor>> streamDescriptors;
        for (auto stream : m_mediaStreams)
        {
            ComPtr<IMFStreamDescriptor> streamDescriptor;
            RETURN_IF_FAILED(stream->GetStreamDescriptor(&streamDescriptor));
            streamDescriptors.push_back(streamDescriptor);
        }

        const DWORD streamDescCount = static_cast<DWORD>(streamDescriptors.size());
        RETURN_IF_FAILED(MFCreatePresentationDescriptor(streamDescCount, reinterpret_cast<IMFStreamDescriptor**>(streamDescriptors.data()), &presentationDescriptor));
        RETURN_IF_FAILED(SelectDefaultStreams(streamDescCount, presentationDescriptor.Get()));
        *presentationDescriptorOut = presentationDescriptor.Detach();
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::Start(IMFPresentationDescriptor* presentationDescriptor, const GUID* guidTimeFormat, const PROPVARIANT* startPosition)
    {
        if (m_state == State::kShutdown)
            return MF_E_SHUTDOWN;

        bool seeked = false;
        bool started = false;
        if (m_state == State::kInitialized || m_state == State::kStopped)
        {
            started = true;
        }
        else if (m_state == State::kPaused || m_state == State::kStarted)
        {
            if (startPosition->vt == VT_EMPTY)
                started = true;
            else
                seeked = true;
        }

        DWORD streamDescCount = 0;
        RETURN_IF_FAILED(presentationDescriptor->GetStreamDescriptorCount(&streamDescCount));
        for (DWORD i = 0; i < streamDescCount; i++)
        {
            ComPtr<IMFStreamDescriptor> streamDescriptor;
            BOOL selected;
            RETURN_IF_FAILED(presentationDescriptor->GetStreamDescriptorByIndex(i, &selected, &streamDescriptor));

            DWORD streamId;
            RETURN_IF_FAILED(streamDescriptor->GetStreamIdentifier(&streamId));

            // Error
            if (streamId >= m_mediaStreams.size())
                continue;

            ComPtr<MediaFoundationStreamWrapper> stream = m_mediaStreams[streamId];

            if (selected) 
            {
                MediaEventType event_type = MENewStream;
                if (stream->IsSelected()) 
                    event_type = MEUpdatedStream;

                ComPtr<IUnknown> unknown_stream;
                RETURN_IF_FAILED(stream.As(&unknown_stream));
                RETURN_IF_FAILED(m_mediaEventQueue->QueueEventParamUnk(event_type, GUID_NULL, S_OK, unknown_stream.Get()));

                // - If the source sends an MESourceStarted event, each media stream
                // sends an MEStreamStarted event. If the source sends an MESourceSeeked
                // event, each stream sends an MEStreamSeeked event.
                if (started) 
                {
                    if (seeked)
                        return E_FAIL;

                    RETURN_IF_FAILED(stream->QueueStartedEvent(startPosition));
                }
                else if (seeked) 
                {
                    RETURN_IF_FAILED(stream->QueueSeekedEvent(startPosition));
                }
            }
            stream->SetSelected(selected);
            m_state = State::kStarted;
        }

        if (started) 
        {
            if (seeked)
                return E_FAIL;

            ComPtr<IMFMediaEvent> mediaEvent;
            RETURN_IF_FAILED(MFCreateMediaEvent(MESourceStarted, GUID_NULL, S_OK, startPosition, &mediaEvent));
            RETURN_IF_FAILED(m_mediaEventQueue->QueueEvent(mediaEvent.Get()));
        }
        else if (seeked) 
        {
            RETURN_IF_FAILED(QueueEvent(MESourceSeeked, GUID_NULL, S_OK, startPosition));
        }
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::Stop()
    {
        if (m_state == State::kShutdown)
            return MF_E_SHUTDOWN;

        RETURN_IF_FAILED(QueueEvent(MESourceStopped, GUID_NULL, S_OK, nullptr));

        for (auto stream : m_mediaStreams) 
        {
            if (stream->IsSelected()) {
                stream->QueueStoppedEvent();
            }
        }

        m_state = State::kStopped;
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::Pause()
    {
        if (m_state == State::kShutdown)
            return MF_E_SHUTDOWN;

        RETURN_IF_FAILED(QueueEvent(MESourcePaused, GUID_NULL, S_OK, nullptr));

        for (auto stream : m_mediaStreams) {
            if (stream->IsSelected()) {
                stream->QueuePausedEvent();
            }

        }

        m_state = State::kPaused;
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::Shutdown()
    {
        m_state = State::kShutdown;
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::GetEvent(DWORD flags, IMFMediaEvent** eventOut)
    {
        return m_mediaEventQueue->GetEvent(flags, eventOut);
    }

    HRESULT MediaFoundationSourceWrapper::BeginGetEvent(IMFAsyncCallback* callback, IUnknown* state) 
    {
        RETURN_IF_FAILED(m_mediaEventQueue->BeginGetEvent(callback, state));
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** eventOut)
    {
        RETURN_IF_FAILED(m_mediaEventQueue->EndGetEvent(result, eventOut));
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::QueueEvent(MediaEventType type, REFGUID extendedType, HRESULT status, const PROPVARIANT* value)
    {
        RETURN_IF_FAILED(m_mediaEventQueue->QueueEventParamVar(type, extendedType, status, value));
        return S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::GetService(REFGUID guidService, REFIID riid, LPVOID* result) 
    {
        if (!IsEqualGUID(guidService, MF_RATE_CONTROL_SERVICE))
            return MF_E_UNSUPPORTED_SERVICE;

        return static_cast<IMFMediaSource*>(this)->QueryInterface(riid, result);
    }

    HRESULT MediaFoundationSourceWrapper::GetSlowestRate(MFRATE_DIRECTION direction, BOOL supportsThinning, float* rate) 
    {
        if (direction == MFRATE_REVERSE) 
            return MF_E_REVERSE_UNSUPPORTED;

        *rate = 0.0f;
        return m_state == State::kShutdown ? MF_E_SHUTDOWN : S_OK;
    }

    HRESULT MediaFoundationSourceWrapper::GetFastestRate(MFRATE_DIRECTION direction, BOOL supportsThinning, float* rate) 
    {
        if (direction == MFRATE_REVERSE) 
            return MF_E_REVERSE_UNSUPPORTED;

        *rate = 0.0f;
        HRESULT hr = MF_E_SHUTDOWN;
        if (m_state != State::kShutdown)
        {
            // Fastest rate should correspond to fastest rate limit of the source. The current sample uses 8x as an example.
            *rate = 8.0f;
            hr = S_OK;
        }
        return hr;
    }

    HRESULT MediaFoundationSourceWrapper::IsRateSupported(BOOL supportsThinning, float newRate, float* supportedRate) 
    {
        if (m_state == State::kShutdown)
            return MF_E_SHUTDOWN;

        if (supportedRate)
            *supportedRate = 0.0f;

        MFRATE_DIRECTION direction = (newRate >= 0) ? MFRATE_FORWARD : MFRATE_REVERSE;
        float fastestRate = 0.0f;
        float slowestRate = 0.0f;
        GetFastestRate(direction, supportsThinning, &fastestRate);
        GetSlowestRate(direction, supportsThinning, &slowestRate);

        HRESULT hr;
        if (supportsThinning)
        {
            // We do not support thinning right now. If Thin is supported, this
            // MediaSource is expected to drop all samples except ones marked as
            // MFSampleExtension_CleanPoint
            hr = MF_E_THINNING_UNSUPPORTED;
        }
        else if (newRate < slowestRate)
        {
            hr = MF_E_REVERSE_UNSUPPORTED;
        }
        else if (newRate > fastestRate)
        {
            hr = MF_E_UNSUPPORTED_RATE;
        }
        else 
        {
            // Supported
            hr = S_OK;
            if (supportedRate)
                *supportedRate = newRate;
        }
        return hr;
    }

    HRESULT MediaFoundationSourceWrapper::SetRate(BOOL supportsThinning, float rate) 
    {
        if (m_state == State::kShutdown)
            return MF_E_SHUTDOWN;

        HRESULT hr = IsRateSupported(supportsThinning, rate, &m_currentRate);
        if (SUCCEEDED(hr)) 
        {
            PROPVARIANT varRate;
            varRate.vt = VT_R4;
            varRate.fltVal = m_currentRate;
            hr = QueueEvent(MESourceRateChanged, GUID_NULL, S_OK, &varRate);
        }
        return hr;
    }

    HRESULT MediaFoundationSourceWrapper::GetRate(BOOL* supportsThinning, float* rate) 
    {
        *supportsThinning = FALSE;
        *rate = 0.0f;
        HRESULT hr = MF_E_SHUTDOWN;
        if (m_state != State::kShutdown)
        {
            hr = S_OK;
            *rate = m_currentRate;
        }
        return hr;
    }

    void MediaFoundationSourceWrapper::CheckForEndOfPresentation()
    {
        if (m_presentationEnded) {
            return;
        }

        bool presentationEnded = true;
        for (auto stream : m_mediaStreams) {
            if (!stream->HasEnded()) {
                presentationEnded = false;
                break;
            }
        }

        // Presentation end if both audio and video streams ended.
        m_presentationEnded = presentationEnded;
        if (m_presentationEnded) 
        {
            QueueEvent(MEEndOfPresentation, GUID_NULL, S_OK, nullptr);
        }
    }
}