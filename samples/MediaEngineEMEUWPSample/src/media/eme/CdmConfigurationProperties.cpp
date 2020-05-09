// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <pch.h>
#include "CdmConfigurationProperties.h"

#include "media/MediaEngineProtectedContentInterfaces.h"

namespace media
{

namespace eme
{

namespace
{
    void SetBstrProperty(IPropertyStore* propertyStore, const PROPERTYKEY& propertyKey, std::wstring& value)
    {
        wil::unique_bstr bstr = wil::make_bstr(value.c_str());
        wil::unique_prop_variant property;
        property.vt = VT_BSTR;
        property.bstrVal = bstr.release();
        propertyStore->SetValue(propertyKey, property);
    }
}

winrt::com_ptr<IPropertyStore> CdmConfigurationProperties::ConvertToPropertyStore()
{
    winrt::com_ptr<IPropertyStore> propertyStore;
    THROW_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(propertyStore.put())));

    // Storage path
    SetBstrProperty(propertyStore.get(), MF_EME_CDM_STOREPATH, storagePath);

    // Only set private storage path when specified
    if(inPrivateStoragePath.length() > 0)
    {
        SetBstrProperty(propertyStore.get(), MF_EME_CDM_INPRIVATESTOREPATH, inPrivateStoragePath);
    }

    return propertyStore;
}

}

}