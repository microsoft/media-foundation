// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <pch.h>
#include "MediaKeys.h"

namespace media
{

namespace eme
{

MediaKeys::MediaKeys(IMFContentDecryptionModule* cdm)
{
    m_cdm.copy_from(cdm);
}

std::shared_ptr<MediaKeySession> MediaKeys::createSession(MF_MEDIAKEYSESSION_TYPE sessionType)
{
    return std::make_shared<MediaKeySession>(m_cdm.get(), sessionType);
}

bool MediaKeys::setServerCertificate(std::vector<uint8_t>& certificate)
{
    return SUCCEEDED(m_cdm->SetServerCertificate(&certificate[0], static_cast<DWORD>(certificate.size())));
}

}

}