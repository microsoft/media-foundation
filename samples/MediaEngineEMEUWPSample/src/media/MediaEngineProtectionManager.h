// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "eme/MediaKeys.h"
#include "media/MediaEngineProtectedContentInterfaces.h"

namespace media {

    // Implements IMFContentProtectionManager
    // (https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfcontentprotectionmanager)
    // and winrt::Windows::Media::Protection::IMediaProtectionManager
    // (https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.mediaprotectionmanager)
    // required by IMFMediaEngineProtectedContent::SetContentProtectionManager in
    // https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengineprotectedcontent-setcontentprotectionmanager.
    class MediaEngineProtectionManager
        : public winrt::implements<MediaEngineProtectionManager, IMFContentProtectionManager, winrt::Windows::Media::Protection::IMediaProtectionManager> {
    public:
        MediaEngineProtectionManager(std::shared_ptr<eme::MediaKeys> mediaKeys);
        ~MediaEngineProtectionManager() override;

        // IMFContentProtectionManager.
        IFACEMETHODIMP BeginEnableContent(IMFActivate* enablerActivate, IMFTopology* topology, IMFAsyncCallback* callback, 
                                          IUnknown* state) noexcept override;
        IFACEMETHODIMP EndEnableContent(IMFAsyncResult* asyncResult) noexcept override;

        // IMediaProtectionManager.
        // MFMediaEngine can query this interface to invoke Properties().
        winrt::event_token ServiceRequested(winrt::Windows::Media::Protection::ServiceRequestedEventHandler const handler);
        void ServiceRequested(winrt::event_token const cookie);
        winrt::event_token RebootNeeded(winrt::Windows::Media::Protection::RebootNeededEventHandler const handler);
        void RebootNeeded(winrt::event_token const cookie);
        winrt::event_token ComponentLoadFailed(winrt::Windows::Media::Protection::ComponentLoadFailedEventHandler const handler);
        void ComponentLoadFailed(winrt::event_token const cookie);
        winrt::Windows::Foundation::Collections::PropertySet Properties();

    protected:
        winrt::com_ptr<IMFContentDecryptionModule> m_contentDecryptionModule;
        winrt::Windows::Foundation::Collections::PropertySet m_propertySet;
        void SetPMPServer(winrt::Windows::Media::Protection::MediaProtectionPMPServer pmpServer);
    };

}