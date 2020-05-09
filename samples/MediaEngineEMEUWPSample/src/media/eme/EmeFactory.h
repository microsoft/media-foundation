// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "media/eme/CdmConfigurationProperties.h"
#include "media/eme/MediaKeySystemAccess.h"
#include "media/eme/MediaKeySystemConfiguration.h"
#include "media/MediaEngineProtectedContentInterfaces.h"

namespace media
{
    namespace eme
    {
        // Class used to query for EME keysystem support and attempt creation of a MediaKeySystemAccess object for a specified keysystem
        class EmeFactory
        {
        public:
            EmeFactory(CdmConfigurationProperties cdmConfigProperties);
            virtual ~EmeFactory() = default;

            // Used to request creation of a MediaKeySystemAccess. Based on:
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
            // Returns: MediaKeySystemAccess on success, or nullptr if the keysystem or none of the specified configurations are supported
            std::shared_ptr<MediaKeySystemAccess> requestMediaKeySystemAccess(const std::wstring& keySystem, std::vector<MediaKeySystemConfiguration>& configurations);

        private:
            CdmConfigurationProperties m_cdmConfigProperties;
            winrt::com_ptr<IMFMediaEngineClassFactory4> m_classFactory;
            winrt::com_ptr<IMFContentDecryptionModuleFactory> m_cdmFactory;
            std::wstring m_cachedFactoryKeySystem;
        };
    }
}