// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <pch.h>
#include "EmeFactory.h"

namespace media
{

namespace eme
{

EmeFactory::EmeFactory(CdmConfigurationProperties cdmConfigProperties)
{
    THROW_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(m_classFactory.put())));
    m_cdmConfigProperties = cdmConfigProperties;
}

std::shared_ptr<MediaKeySystemAccess> EmeFactory::requestMediaKeySystemAccess(const std::wstring& keySystem, std::vector<MediaKeySystemConfiguration>& configurations)
{
    // Use a cached CDM factory if client is querying for the same keysystem, otherwise create a new CDM factory
    if(!m_cdmFactory || m_cachedFactoryKeySystem != keySystem)
    {
        if(FAILED(LOG_IF_FAILED(m_classFactory->CreateContentDecryptionModuleFactory(keySystem.c_str(), IID_PPV_ARGS(m_cdmFactory.put())))))
        {
            return nullptr;
        }
        m_cachedFactoryKeySystem = keySystem;
    }

    if(!m_cdmFactory->IsTypeSupported(keySystem.c_str(), nullptr))
    {
        return nullptr;
    }
    winrt::com_ptr<IMFContentDecryptionModuleAccess> cdmAccess;
    std::vector<wil::com_ptr_t<IPropertyStore>> configurationPropertyStores;
    for(auto& config : configurations)
    {
        wil::com_ptr_t<IPropertyStore> propStore;
        propStore.attach(config.ConvertToPropertyStore().detach());
        configurationPropertyStores.push_back(propStore);
    }
    if(FAILED(LOG_IF_FAILED(m_cdmFactory->CreateContentDecryptionModuleAccess(keySystem.c_str(), configurationPropertyStores[0].addressof(), static_cast<DWORD>(configurationPropertyStores.size()), cdmAccess.put()))))
    {
        return nullptr;
    }
    
    return std::make_shared<MediaKeySystemAccess>(m_cdmConfigProperties, cdmAccess.get());
}

}

}