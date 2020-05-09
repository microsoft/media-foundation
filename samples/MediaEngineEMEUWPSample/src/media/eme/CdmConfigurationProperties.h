// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

namespace media
{

namespace eme
{
    // Struct which encapsulates CDM configuration properties
    struct CdmConfigurationProperties
    {
        // Use this property to pass a storage location the CDM can use for initialization.
        // This storage location will also be used for content specific data if INPRIVATESTOREPATH is not set.
        // The app should not delete the store location to optimize performance of the CDM.
        std::wstring storagePath;
        // Use this property to pass a storage location the CDM can use for content specific data (e.g licenses)
        // The app should delete the store location after the CDM object has been released.
        std::wstring inPrivateStoragePath;

        winrt::com_ptr<IPropertyStore> ConvertToPropertyStore();
    };
}

}