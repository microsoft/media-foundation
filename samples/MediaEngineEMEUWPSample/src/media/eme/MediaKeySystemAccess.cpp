// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <pch.h>
#include "MediaKeySystemAccess.h"

namespace media
{

namespace eme
{

MediaKeySystemAccess::MediaKeySystemAccess(CdmConfigurationProperties& cdmConfigProperties, IMFContentDecryptionModuleAccess* cdmAccess)
{
    m_cdmProperties = cdmConfigProperties.ConvertToPropertyStore();
    m_cdmAccess.copy_from(cdmAccess);
    winrt::com_ptr<IPropertyStore> configuration;
    THROW_IF_FAILED(cdmAccess->GetConfiguration(configuration.put()));
    m_keySystemConfiguration = MediaKeySystemConfiguration::FromPropertyStore(configuration);
}

wil::unique_cotaskmem_string MediaKeySystemAccess::keySystem()
{
    wil::unique_cotaskmem_string keySystem;
    THROW_IF_FAILED(m_cdmAccess->GetKeySystem(keySystem.put()));
    return std::move(keySystem);
}

const MediaKeySystemConfiguration& MediaKeySystemAccess::getConfiguration()
{
    return m_keySystemConfiguration;
}

std::shared_ptr<MediaKeys> MediaKeySystemAccess::createMediaKeys()
{
    winrt::com_ptr<IMFContentDecryptionModule> cdm;
    THROW_IF_FAILED(m_cdmAccess->CreateContentDecryptionModule(m_cdmProperties.get(), cdm.put()));
    return std::make_shared<MediaKeys>(cdm.get());
}

}

}