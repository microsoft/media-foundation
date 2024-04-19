#pragma once
#include <mfmediaengine.h>
#include <wrl.h>

namespace media
{
    class MediaEngineNotifyImpl
        : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
        IMFMediaEngineNotify>
    {
    public:
        MediaEngineNotifyImpl() = default;
        ~MediaEngineNotifyImpl() = default;

        HRESULT RuntimeClassInitialize(std::function<void()> onLoadedCB, std::function<void(MF_MEDIA_ENGINE_ERR, HRESULT)> errorCB, std::function<void()> onPlaybackEndedCB);
        void Shutdown();
        IFACEMETHODIMP EventNotify(DWORD eventCode, DWORD_PTR param1, DWORD param2);

    private:
        wil::critical_section m_lock;
        std::function<void()> m_onLoadedCB;
        std::function<void()> m_onPlaybackEndedCB;
        std::function<void(MF_MEDIA_ENGINE_ERR, HRESULT)> m_errorCB;
        bool m_detached = false;
    };

}

