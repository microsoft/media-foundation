// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "MediaFoundationHelpers.h"
#include "MediaEngineExtension.h"
#include "MediaFoundationSourceWrapper.h"
#include "MediaEngineNotifyImpl.h"

namespace media
{
    // This class handles creation and management of the MediaFoundation
    // MediaEngine.
    // - It uses the provided IMFMediaSource to feed media samples into the
    //   MediaEngine pipeline.
    class MediaEngineWrapper : public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>,
        IUnknown>
    {
    public:
        using ErrorCB = std::function<void(MF_MEDIA_ENGINE_ERR, HRESULT)>;

        MediaEngineWrapper() = default;
        ~MediaEngineWrapper() = default;

        HRESULT RuntimeClassInitialize(IMFSourceReader* sourceReader,
            IMFDXGIDeviceManager* dxgiDeviceManager,
            std::function<void()> initializedCB,
            std::function<void(MF_MEDIA_ENGINE_ERR, HRESULT)> errorCB,
            std::function<void()> playbackEndedCB,
            uint32_t width, uint32_t height);

        // Control various aspects of playback
        void StartPlayingFrom(uint64_t timeStamp);

        // Get a handle to a DCOMP surface for integrating into a visual tree
        HANDLE GetSurfaceHandle();

        // Inform media engine of output window position & size changes
        void WindowUpdate(uint32_t width, uint32_t height);

    private:
        HRESULT CreateMediaEngine(IMFSourceReader* sourceReader);
        void OnLoaded();
        void OnError(MF_MEDIA_ENGINE_ERR error, HRESULT hr);    
        void OnPlaybackEnded();

        wil::critical_section m_lock;
        std::function<void()> m_initializedCB;
        std::function<void()> m_playbackEndedCB;
        ErrorCB m_errorCB;

        ComPtr<IMFSourceReader> m_sourceReader;
        ComPtr<IMFMediaEngine> m_mediaEngine;
        ComPtr<IMFDXGIDeviceManager> m_dxgiDeviceManager;
        ComPtr<MediaEngineExtension> m_mediaEngineExtension;
        ComPtr<MediaFoundationSourceWrapper> m_source;
        ComPtr<MediaEngineNotifyImpl> m_mediaEngineNotifier;
        wil::unique_handle m_dcompSurfaceHandle;        

        uint32_t m_width = 0;
        uint32_t m_height = 0;
        bool m_hasSetSource = false;
    };

} // namespace media