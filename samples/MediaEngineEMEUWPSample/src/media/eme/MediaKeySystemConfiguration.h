// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "media/MediaEngineProtectedContentInterfaces.h"

namespace media
{

namespace eme
{

// This struct represents a MediaKeySystemMediaCapability dictionary, as defined in the EME specification:
// https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemmediacapability
struct MediaKeySystemMediaCapability
{
    std::wstring contentType;
    std::wstring robustness;
};

// This class represents a MediaKeySystemConfiguration dictionary, as defined in the EME specification:
// https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#mediakeysystemconfiguration-dictionary
class MediaKeySystemConfiguration
{
public:
    MediaKeySystemConfiguration() = default;
    ~MediaKeySystemConfiguration() = default;

    static MediaKeySystemConfiguration FromPropertyStore(winrt::com_ptr<IPropertyStore> propertyStore);
    winrt::com_ptr<IPropertyStore> ConvertToPropertyStore();

    void label(std::wstring&& label) { m_label = std::move(label); }
    const std::wstring& label() { return m_label; }

    void initDataTypes(std::vector<std::wstring>&& initDataTypes) { m_initDataTypes = std::move(initDataTypes); }
    std::vector<std::wstring>& initDataTypes() { return m_initDataTypes; }

    void audioCapabilities(std::vector<MediaKeySystemMediaCapability>&& audioCapabilities) { m_audioCapabilities = std::move(audioCapabilities); }
    std::vector<MediaKeySystemMediaCapability>& audioCapabilities() { return m_audioCapabilities; }

    void videoCapabilities(std::vector<MediaKeySystemMediaCapability>&& videoCapabilities) { m_videoCapabilities = std::move(videoCapabilities); }
    std::vector<MediaKeySystemMediaCapability>& videoCapabilities() { return m_videoCapabilities; }

    void distinctiveId(MF_MEDIAKEYS_REQUIREMENT requirement) { m_distinctiveIdRequirement = requirement; }
    MF_MEDIAKEYS_REQUIREMENT distinctiveId() { return m_distinctiveIdRequirement; }

    void persistentState(MF_MEDIAKEYS_REQUIREMENT requirement) { m_persistentStateRequirement = requirement; }
    MF_MEDIAKEYS_REQUIREMENT persistentState() { return m_persistentStateRequirement; }

    void sessionTypes(std::vector<MF_MEDIAKEYSESSION_TYPE>&& sessionTypes) { m_sessionTypes = std::move(sessionTypes); }
    const std::vector<MF_MEDIAKEYSESSION_TYPE>& sessionTypes() { return m_sessionTypes; }

private:
    std::wstring m_label;
    std::vector<std::wstring> m_initDataTypes;
    std::vector<MediaKeySystemMediaCapability> m_audioCapabilities;
    std::vector<MediaKeySystemMediaCapability> m_videoCapabilities;
    MF_MEDIAKEYS_REQUIREMENT m_distinctiveIdRequirement;
    MF_MEDIAKEYS_REQUIREMENT m_persistentStateRequirement;
    std::vector<MF_MEDIAKEYSESSION_TYPE> m_sessionTypes;
    
};

}

}