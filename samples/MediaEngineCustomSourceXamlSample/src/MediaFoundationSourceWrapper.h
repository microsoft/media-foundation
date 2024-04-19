#pragma once
#include "MediaFoundationStreamWrapper.h"

using namespace Microsoft::WRL;

namespace media {
    class MediaFoundationSourceWrapper
        : public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>, 
        IMFMediaSource,
        IMFGetService,
        IMFRateSupport,
        IMFRateControl>
    {
    public:
        MediaFoundationSourceWrapper();
        ~MediaFoundationSourceWrapper();

        HRESULT RuntimeClassInitialize(IMFSourceReader* sourceReader);

        // IMFMediaSource
        IFACEMETHODIMP GetCharacteristics(DWORD* characteristics) override;
        IFACEMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor** presentationDescriptorOut) override;
        IFACEMETHODIMP Start(IMFPresentationDescriptor* presentationDescriptor, const GUID* guidTimeFormat, const PROPVARIANT* startPosition) override;
        IFACEMETHODIMP Stop() override;
        IFACEMETHODIMP Pause() override;
        IFACEMETHODIMP Shutdown() override;

        // IMFMediaEventGenerator
        // Note: IMFMediaSource inherits IMFMediaEventGenerator.
        IFACEMETHODIMP GetEvent(DWORD flags, IMFMediaEvent** eventOut) override;
        IFACEMETHODIMP BeginGetEvent(IMFAsyncCallback* callback, IUnknown* state) override;
        IFACEMETHODIMP EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** eventOut) override;
        IFACEMETHODIMP QueueEvent(MediaEventType type, REFGUID extendedType, HRESULT status, const PROPVARIANT* value) override;

        // IMFGetService
        IFACEMETHODIMP GetService(REFGUID guidService, REFIID riid, LPVOID* result) override;

        // IMFRateSupport
        IFACEMETHODIMP GetSlowestRate(MFRATE_DIRECTION direction, BOOL supportsThinning, float* rate) override;
        IFACEMETHODIMP GetFastestRate(MFRATE_DIRECTION direction, BOOL supportsThinning, float* rate) override;
        IFACEMETHODIMP IsRateSupported(BOOL supportsThinning, float newRate, float* supportedRate) override;

        // IMFRateControl
        IFACEMETHODIMP SetRate(BOOL supportsThinning, float rate) override;
        IFACEMETHODIMP GetRate(BOOL* supportsThinning, float* rate) override;

        void CheckForEndOfPresentation();

    private:
        HRESULT SelectDefaultStreams(const DWORD streamDescCount, IMFPresentationDescriptor* presentationDescriptor);

        enum class State
        {
            kInitialized,
            kStarted,
            kStopped,
            kPaused,
            kShutdown
        } m_state = State::kInitialized;

        std::vector<ComPtr<MediaFoundationStreamWrapper>> m_mediaStreams;
        ComPtr<IMFMediaEventQueue> m_mediaEventQueue;
        ComPtr<IMFSourceReader> m_sourceReader;
        float m_currentRate = 0.0f;
        bool m_presentationEnded = false;
    };
}

