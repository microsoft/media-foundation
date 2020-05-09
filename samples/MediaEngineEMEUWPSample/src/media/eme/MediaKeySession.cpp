// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <pch.h>
#include "MediaKeySession.h"

namespace media
{

namespace eme
{

namespace
{
    class SessionCallbackHelper : public winrt::implements<SessionCallbackHelper,IMFContentDecryptionModuleSessionCallbacks>
    {
    public:
        SessionCallbackHelper(MediaKeySession::KeyMessageCB&& keyMessageCB, MediaKeySession::KeyStatusChangedCB&& keyStatusChangedCB) : m_keyMessageCB(keyMessageCB), m_keyStatusChangedCB(keyStatusChangedCB)
        {
            THROW_HR_IF(E_INVALIDARG, !m_keyMessageCB);
            THROW_HR_IF(E_INVALIDARG, !m_keyStatusChangedCB);
        }

        virtual ~SessionCallbackHelper()
        {
            DetachParent();
        }

        void DetachParent()
        {
            auto lock = m_lock.lock();
            m_keyMessageCB = nullptr;
            m_keyStatusChangedCB = nullptr;
        }

        // Session callback interface
        IFACEMETHODIMP KeyMessage(MF_MEDIAKEYSESSION_MESSAGETYPE messageType, const BYTE* message, DWORD messageSize, LPCWSTR destinationUrl) noexcept override
        try
        {
            auto lock = m_lock.lock();
            m_keyMessageCB(messageType, gsl::span<uint8_t>(const_cast<BYTE*>(message), static_cast<size_t>(messageSize)), destinationUrl);
            return S_OK;
        }
        CATCH_RETURN();

        IFACEMETHODIMP KeyStatusChanged() noexcept override
        try
        {
            auto lock = m_lock.lock();
            m_keyStatusChangedCB();
            return S_OK;
        }
        CATCH_RETURN();

    private:
        wil::critical_section m_lock;
        MediaKeySession::KeyMessageCB m_keyMessageCB;
        MediaKeySession::KeyStatusChangedCB m_keyStatusChangedCB;
    };
}

MediaKeySession::MediaKeySession(IMFContentDecryptionModule* cdm, MF_MEDIAKEYSESSION_TYPE sessionType)
{
    m_sessionCallbacks = winrt::make<SessionCallbackHelper>(
            std::bind(&MediaKeySession::OnKeyMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
            std::bind(&MediaKeySession::OnKeyStatusChanged, this));
    THROW_IF_FAILED(cdm->CreateSession(sessionType, m_sessionCallbacks.get(), m_cdmSession.put()));
}

MediaKeySession::~MediaKeySession()
{
    static_cast<SessionCallbackHelper*>(m_sessionCallbacks.get())->DetachParent();
}

wil::unique_cotaskmem_string MediaKeySession::sessionId()
{
    wil::unique_cotaskmem_string sessionIdStr;
    THROW_IF_FAILED(m_cdmSession->GetSessionId(sessionIdStr.put()));
    return sessionIdStr;
}

double MediaKeySession::expiration()
{
    double expirationValue = 0.0;
    THROW_IF_FAILED(m_cdmSession->GetExpiration(&expirationValue));
    return expirationValue;
}

std::vector<MediaKeyStatusPair> MediaKeySession::keyStatuses()
{
    wil::unique_cotaskmem_array_ptr<MFMediaKeyStatus> abiKeyStatuses;
    THROW_IF_FAILED(m_cdmSession->GetKeyStatuses(abiKeyStatuses.put(), abiKeyStatuses.size_address<UINT>()));
    std::vector<MediaKeyStatusPair> statusPairs(abiKeyStatuses.size());
    for(size_t i = 0; i < abiKeyStatuses.size(); i++)
    {
        std::vector<uint8_t> keyId(abiKeyStatuses[i].cbKeyId);
        memcpy(&keyId[0], abiKeyStatuses[i].pbKeyId, keyId.size());
        statusPairs[i] = std::make_tuple(std::move(keyId), abiKeyStatuses[i].eMediaKeyStatus);
    }
    return statusPairs;
}

bool MediaKeySession::load(const std::wstring& sessionId)
{
    BOOL loaded = FALSE;
    THROW_IF_FAILED(m_cdmSession->Load(sessionId.c_str(), &loaded));
    return !!loaded;
}

void MediaKeySession::onmessage(KeyMessageCB&& keyMessageCB)
{
    m_keyMessageCB = std::move(keyMessageCB);
}

void MediaKeySession::onkeystatuseschange(KeyStatusChangedCB&& keyStatusChangedCB)
{
    m_keyStatusChangedCB = std::move(keyStatusChangedCB);
}

void MediaKeySession::generateRequest(const std::wstring& initDataType, std::vector<uint8_t>& initData)
{
    THROW_IF_FAILED(m_cdmSession->GenerateRequest(initDataType.c_str(), &initData[0], static_cast<DWORD>(initData.size())));
}

void MediaKeySession::update(gsl::span<uint8_t> responseData)
{
    THROW_IF_FAILED(m_cdmSession->Update(responseData.data(), static_cast<DWORD>(responseData.size())));
}

void MediaKeySession::close()
{
    THROW_IF_FAILED(m_cdmSession->Close());
}

void MediaKeySession::remove()
{
    THROW_IF_FAILED(m_cdmSession->Remove());
}

// Private callback methods

void MediaKeySession::OnKeyMessage(MF_MEDIAKEYSESSION_MESSAGETYPE messageType, gsl::span<uint8_t> message, LPCWSTR destinationUrl)
{
    m_keyMessageCB(messageType, message, destinationUrl);
}

void MediaKeySession::OnKeyStatusChanged()
{
    m_keyStatusChangedCB();
}

}

}