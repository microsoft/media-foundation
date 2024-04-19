#include "pch.h"
#include "MediaEngineNotifyImpl.h"

using namespace Microsoft::WRL;

namespace media
{
    HRESULT MediaEngineNotifyImpl::RuntimeClassInitialize(std::function<void()> onLoadedCB, std::function<void(MF_MEDIA_ENGINE_ERR, HRESULT)> errorCB, std::function<void()> onPlaybackEndedCB)
    {
        m_onLoadedCB = onLoadedCB;
        m_errorCB = errorCB;
        m_onPlaybackEndedCB = onPlaybackEndedCB;

        return S_OK;
    }

    void MediaEngineNotifyImpl::Shutdown()
    {
        auto lock = m_lock.lock();
        m_detached = true;
        m_onLoadedCB = nullptr;
        m_errorCB = nullptr;
        m_onPlaybackEndedCB = nullptr;
    }

    // IMFMediaEngineNotify
    IFACEMETHODIMP MediaEngineNotifyImpl::EventNotify(DWORD eventCode, DWORD_PTR param1, DWORD param2)
    {
        auto lock = m_lock.lock();
        if (m_detached)
            return MF_E_SHUTDOWN;

        switch ((MF_MEDIA_ENGINE_EVENT)eventCode)
        {
        case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
            m_onLoadedCB();
            break;
        case MF_MEDIA_ENGINE_EVENT_ERROR:
            m_errorCB((MF_MEDIA_ENGINE_ERR)param1, (HRESULT)param2);
            break;
        case MF_MEDIA_ENGINE_EVENT_ENDED:
            m_onPlaybackEndedCB();
            break;
        default:
            break;
        }
        return S_OK;
    }
}