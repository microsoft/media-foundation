// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "media/eme/MediaKeySession.h"

namespace media
{

namespace eme
{

// Based on the MediaKeys interface in the EME specification:
// https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys
class MediaKeys
{
public:
    MediaKeys(IMFContentDecryptionModule* cdm);
    std::shared_ptr<MediaKeySession> createSession(MF_MEDIAKEYSESSION_TYPE sessionType = MF_MEDIAKEYSESSION_TYPE_TEMPORARY);
    bool setServerCertificate(std::vector<uint8_t>& certificate);

    winrt::com_ptr<IMFContentDecryptionModule> GetCDM() { return m_cdm; }

private:
    winrt::com_ptr<IMFContentDecryptionModule> m_cdm;
};

}

}