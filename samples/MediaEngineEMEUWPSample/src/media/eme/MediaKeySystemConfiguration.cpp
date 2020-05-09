// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <pch.h>
#include "MediaKeySystemConfiguration.h"

namespace media
{

namespace eme
{

namespace
{
    wil::unique_prop_variant GenerateCapabilityProperty(const std::vector<MediaKeySystemMediaCapability>& capabilities)
    {
        auto capabilitiesArray = wil::make_unique_cotaskmem<PROPVARIANT[]>(capabilities.size());
        for(size_t i = 0; i < capabilities.size(); i++)
        {
            winrt::com_ptr<IPropertyStore> capabilityProperties;
            THROW_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(capabilityProperties.put())));
            // Populate property store with robustness and content type BSTR prop variants
            wil::unique_prop_variant robustness;
            robustness.vt = VT_BSTR;
            robustness.bstrVal = wil::make_bstr(capabilities[i].robustness.c_str()).release();
            THROW_IF_FAILED(capabilityProperties->SetValue(MF_EME_ROBUSTNESS, robustness));
            wil::unique_prop_variant contentType;
            contentType.vt = VT_BSTR;
            contentType.bstrVal = wil::make_bstr(capabilities[i].contentType.c_str()).release();
            THROW_IF_FAILED(capabilityProperties->SetValue(MF_EME_CONTENTTYPE, contentType));

            wil::unique_prop_variant capabilityProperty;
            capabilityProperty.vt = VT_UNKNOWN;
            winrt::com_ptr<IUnknown> capabilityPropertiesUnk = capabilityProperties.as<IUnknown>();
            capabilityProperty.punkVal = capabilityPropertiesUnk.detach();
            capabilitiesArray[i] = capabilityProperty.release();
        }
        wil::unique_prop_variant capabilitiesProperty;
        capabilitiesProperty.vt = VT_VECTOR | VT_VARIANT;
        capabilitiesProperty.capropvar.cElems = static_cast<ULONG>(capabilities.size());
        capabilitiesProperty.capropvar.pElems = capabilitiesArray.release();
        return std::move(capabilitiesProperty);
    }

    void GetCapabilitiesFromPropVar(const PROPVARIANT& capabilitiesProperty, std::vector<MediaKeySystemMediaCapability>& capabilities)
    {
        capabilities = std::vector<MediaKeySystemMediaCapability>(capabilitiesProperty.capropvar.cElems);
        for(size_t i = 0; i < capabilities.size(); i++)
        {
            PROPVARIANT& capabilityProperty = capabilitiesProperty.capropvar.pElems[i];
            winrt::com_ptr<IPropertyStore> capabilityPropertyStore;
            if(capabilityProperty.vt != VT_UNKNOWN || FAILED(capabilityProperty.punkVal->QueryInterface(IID_PPV_ARGS(capabilityPropertyStore.put()))))
            {
                continue;
            }

            wil::unique_prop_variant robustness;
            if(SUCCEEDED(capabilityPropertyStore->GetValue(MF_EME_ROBUSTNESS, &robustness)) &&
               robustness.vt == VT_BSTR)
            {
                capabilities[i].robustness = std::wstring(robustness.bstrVal);
            }

            wil::unique_prop_variant contentType;
            if(SUCCEEDED(capabilityPropertyStore->GetValue(MF_EME_CONTENTTYPE, &contentType)) &&
               contentType.vt == VT_BSTR)
            {
                capabilities[i].contentType = std::wstring(contentType.bstrVal);
            }
        }
    }
}

winrt::com_ptr<IPropertyStore> MediaKeySystemConfiguration::ConvertToPropertyStore()
{
    winrt::com_ptr<IPropertyStore> propertyStore;
    THROW_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(propertyStore.put())));

    // Init data types
    auto initDataTypeArray = wil::make_unique_cotaskmem<BSTR[]>(m_initDataTypes.size());
    for(size_t i = 0; i < m_initDataTypes.size(); i++)
    {
        initDataTypeArray[i] = wil::make_bstr(m_initDataTypes[i].c_str()).release();
    }
    wil::unique_prop_variant initDataTypes;
    initDataTypes.vt = VT_VECTOR | VT_BSTR;
    initDataTypes.cabstr.cElems = static_cast<ULONG>(m_initDataTypes.size());
    initDataTypes.cabstr.pElems = initDataTypeArray.release();
    THROW_IF_FAILED(propertyStore->SetValue(MF_EME_INITDATATYPES, initDataTypes));

    // Audio capabilities
    wil::unique_prop_variant audioCapabilities = GenerateCapabilityProperty(m_audioCapabilities);
    THROW_IF_FAILED(propertyStore->SetValue(MF_EME_AUDIOCAPABILITIES, audioCapabilities));

    // Video capabilities
    wil::unique_prop_variant videoCapabilities = GenerateCapabilityProperty(m_videoCapabilities);
    THROW_IF_FAILED(propertyStore->SetValue(MF_EME_VIDEOCAPABILITIES, videoCapabilities));

    // Distinctive ID
    wil::unique_prop_variant distinctiveId;
    distinctiveId.vt = VT_UI4;
    distinctiveId.ulVal = static_cast<DWORD>(m_distinctiveIdRequirement);
    THROW_IF_FAILED(propertyStore->SetValue(MF_EME_DISTINCTIVEID, distinctiveId));

    // Persistent state
    wil::unique_prop_variant persistentState;
    persistentState.vt = VT_UI4;
    persistentState.ulVal = static_cast<DWORD>(m_persistentStateRequirement);
    THROW_IF_FAILED(propertyStore->SetValue(MF_EME_PERSISTEDSTATE, persistentState));

    // Session types
    auto sessionTypeArray = wil::make_unique_cotaskmem<DWORD[]>(m_sessionTypes.size());
    for(size_t i = 0; i < m_sessionTypes.size(); i++)
    {
        sessionTypeArray[i] = m_sessionTypes[i];
    }
    wil::unique_prop_variant sessionTypes;
    sessionTypes.vt = VT_VECTOR | VT_UI4;
    sessionTypes.caul.cElems = static_cast<ULONG>(m_sessionTypes.size());
    sessionTypes.caul.pElems = sessionTypeArray.release();
    THROW_IF_FAILED(propertyStore->SetValue(MF_EME_SESSIONTYPES, sessionTypes));

    return propertyStore;
}

MediaKeySystemConfiguration MediaKeySystemConfiguration::FromPropertyStore(winrt::com_ptr<IPropertyStore> propertyStore)
{
    MediaKeySystemConfiguration configuration;
    
    // Init data types
    wil::unique_prop_variant initDataTypes;
    if(SUCCEEDED(propertyStore->GetValue(MF_EME_INITDATATYPES, &initDataTypes)) &&
       initDataTypes.vt == (VT_VECTOR | VT_BSTR))
    {
        configuration.m_initDataTypes.resize(initDataTypes.cabstr.cElems);
        for(size_t i = 0; i < initDataTypes.cabstr.cElems; i++)
        {
            configuration.m_initDataTypes[i] = std::wstring(initDataTypes.cabstr.pElems[i]);
        }
    }

    // Audio capabilities
    wil::unique_prop_variant audioCapabilities;
    if(SUCCEEDED(propertyStore->GetValue(MF_EME_AUDIOCAPABILITIES, &audioCapabilities)) &&
       audioCapabilities.vt == (VT_VECTOR | VT_VARIANT))
    {
        GetCapabilitiesFromPropVar(audioCapabilities, configuration.m_audioCapabilities);
    }

    // Video capabilities
    wil::unique_prop_variant videoCapabilities;
    if(SUCCEEDED(propertyStore->GetValue(MF_EME_VIDEOCAPABILITIES, &videoCapabilities)) &&
       videoCapabilities.vt == (VT_VECTOR | VT_VARIANT))
    {
        GetCapabilitiesFromPropVar(videoCapabilities, configuration.m_videoCapabilities);
    }

    // Distinctive ID
    wil::unique_prop_variant distinctiveId;
    if(SUCCEEDED(propertyStore->GetValue(MF_EME_DISTINCTIVEID, &distinctiveId)) &&
       distinctiveId.vt == VT_UI4)
    {
        configuration.m_distinctiveIdRequirement = static_cast<MF_MEDIAKEYS_REQUIREMENT>(distinctiveId.ulVal);
    }

    // Persistent state
    wil::unique_prop_variant persistentState;
    if(SUCCEEDED(propertyStore->GetValue(MF_EME_PERSISTEDSTATE, &persistentState)) &&
       persistentState.vt == VT_UI4)
    {
        configuration.m_persistentStateRequirement = static_cast<MF_MEDIAKEYS_REQUIREMENT>(persistentState.ulVal);
    }

    // Session types
    wil::unique_prop_variant sessionTypes;
    if(SUCCEEDED(propertyStore->GetValue(MF_EME_SESSIONTYPES, &sessionTypes)) &&
       sessionTypes.vt == (VT_VECTOR | VT_UI4))
    {
        configuration.m_sessionTypes = std::vector<MF_MEDIAKEYSESSION_TYPE>(sessionTypes.caul.cElems);
        for(size_t i = 0; i < sessionTypes.caul.cElems; i++)
        {
            configuration.m_sessionTypes[i] = static_cast<MF_MEDIAKEYSESSION_TYPE>(sessionTypes.caul.pElems[i]);
        }
    }

    return configuration;
}

}

}