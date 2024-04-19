#pragma once
#include <mfapi.h>
#include <memory>
#include <queue>

using namespace Microsoft::WRL;

namespace media {
    class MediaFoundationStreamWrapper
        : public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>,
        IMFMediaStream>
    {
    public:
        MediaFoundationStreamWrapper();
        ~MediaFoundationStreamWrapper();

        HRESULT RuntimeClassInitialize(uint32_t sourceStreamId, uint32_t mediaStreamId, GUID streamType, IMFMediaSource* parentSource, IMFSourceReader* sourceReader);
        HRESULT QueueStartedEvent(const PROPVARIANT* startPosition);
        HRESULT QueueSeekedEvent(const PROPVARIANT* startPosition);
        HRESULT QueueStoppedEvent();
        HRESULT QueuePausedEvent();
        void SetSelected(bool selected);
        bool IsSelected();
        bool HasEnded();

        // IMFMediaStream implementation - it is in general running in MF threadpool thread.
        IFACEMETHODIMP GetMediaSource(IMFMediaSource** mediaSourceOut) override;
        IFACEMETHODIMP GetStreamDescriptor(IMFStreamDescriptor** streamDescriptorOut) override;
        IFACEMETHODIMP RequestSample(IUnknown* token) override;

        GUID StreamType() const;

        // IMFMediaEventGenerator implementation - IMFMediaStream derives from IMFMediaEventGenerator.
        IFACEMETHODIMP GetEvent(DWORD flags, IMFMediaEvent** eventOut) override;
        IFACEMETHODIMP BeginGetEvent(IMFAsyncCallback* callback, IUnknown* state) override;
        IFACEMETHODIMP EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** eventOut) override;
        IFACEMETHODIMP QueueEvent(MediaEventType type, REFGUID extendedType, HRESULT status, const PROPVARIANT* value) override;

    protected:
        HRESULT GenerateStreamDescriptor();
        HRESULT GetMediaType(IMFMediaType** mediaTypeOut);

        enum class State 
        {
            kInitialized,
            kStarted,
            kStopped,
            kPaused
        } m_state = State::kInitialized;

        uint32_t m_sourceStreamId;
        uint32_t m_mediaStreamId;
        GUID m_streamType;
        wil::critical_section m_lock;
        ComPtr<IMFMediaSource> m_parentSource;
        ComPtr<IMFSourceReader> m_sourceReader;
        ComPtr<IMFMediaEventQueue> m_mediaEventQueue;
        ComPtr<IMFStreamDescriptor> m_streamDescriptor;
        std::queue<ComPtr<IUnknown>> m_pendingSampleRequestTokens;
        bool m_selected = false;
        bool m_streamEnded = false;
    };
}
