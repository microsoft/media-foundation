// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "media/eme/CdmConfigurationProperties.h"
#include "media/eme/MediaKeySystemConfiguration.h"
#include "media/eme/MediaKeys.h"

namespace media
{
    namespace eme
    {
        // Based on the MediaKeySystemAccess interface in the EME specification:
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#mediakeysystemaccess-interface
        class MediaKeySystemAccess
        {
        public:
            MediaKeySystemAccess(CdmConfigurationProperties& cdmConfigProperties, IMFContentDecryptionModuleAccess* cdmAccess);

            wil::unique_cotaskmem_string keySystem();
            const MediaKeySystemConfiguration& getConfiguration();
            std::shared_ptr<MediaKeys> createMediaKeys();

        private:
            winrt::com_ptr<IPropertyStore> m_cdmProperties;
            winrt::com_ptr<IMFContentDecryptionModuleAccess> m_cdmAccess;
            MediaKeySystemConfiguration m_keySystemConfiguration;
        };
    }
}