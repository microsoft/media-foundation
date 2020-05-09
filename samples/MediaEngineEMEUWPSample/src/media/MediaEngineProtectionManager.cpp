// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <pch.h>
#include "MediaEngineProtectionManager.h"

namespace media
{

MediaEngineProtectionManager::MediaEngineProtectionManager(std::shared_ptr<eme::MediaKeys> mediaKeys)
{
    m_contentDecryptionModule = mediaKeys->GetCDM();
    // Use the PMP server from the CDM - this ensures that the media engine uses the same protected process as the CDM
    winrt::com_ptr<IMFGetService> cdmServices = m_contentDecryptionModule.as<IMFGetService>();
    winrt::com_ptr<ABI::Windows::Media::Protection::IMediaProtectionPMPServer> abiPmpServer;
    THROW_IF_FAILED(cdmServices->GetService(MF_CONTENTDECRYPTIONMODULE_SERVICE, IID_PPV_ARGS(abiPmpServer.put())));
    winrt::Windows::Media::Protection::MediaProtectionPMPServer pmpServer{ nullptr };
    winrt::copy_from_abi(pmpServer, abiPmpServer.get());
    SetPMPServer(pmpServer);
}

MediaEngineProtectionManager::~MediaEngineProtectionManager() = default;

void MediaEngineProtectionManager::SetPMPServer(winrt::Windows::Media::Protection::MediaProtectionPMPServer pmpServer)
{
    using MapType = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, IInspectable>;
    auto propertyMap = m_propertySet.as<MapType>();

    // MFMediaEngine uses |pmpServerKey| to get the Protected Media Path (PMP) server used for playing protected content.
    // This is not currently documented in MSDN.
    winrt::hstring pmpServerKey{ L"Windows.Media.Protection.MediaProtectionPMPServer" };
    propertyMap.Insert(pmpServerKey, pmpServer);
}

HRESULT MediaEngineProtectionManager::BeginEnableContent(IMFActivate* enablerActivate, IMFTopology* /*topology*/, 
                                                         IMFAsyncCallback* callback, IUnknown* state) noexcept
try
{
    winrt::com_ptr<IUnknown> unknownObject;
    winrt::com_ptr<IMFAsyncResult> asyncResult;
    THROW_IF_FAILED(MFCreateAsyncResult(nullptr, callback, state, asyncResult.put()));
    THROW_IF_FAILED(enablerActivate->ActivateObject(IID_PPV_ARGS(unknownObject.put())));

    // |enablerType| can be obtained from IMFContentEnabler
    // (https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfcontentenabler).
    // If not, try IMediaProtectionServiceRequest
    // (https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.imediaprotectionservicerequest).
    GUID enablerType = GUID_NULL;
    winrt::com_ptr<IMFContentEnabler> contentEnabler = unknownObject.try_as<IMFContentEnabler>();
    if (contentEnabler)
    {
        THROW_IF_FAILED(contentEnabler->GetEnableType(&enablerType));
    }
    else
    {
        winrt::com_ptr<ABI::Windows::Media::Protection::IMediaProtectionServiceRequest>
            serviceRequest = unknownObject.as<ABI::Windows::Media::Protection::IMediaProtectionServiceRequest>();
        THROW_IF_FAILED(serviceRequest->get_Type(&enablerType));
    }

    // Unhandled enabler types
    THROW_HR_IF(MF_E_REBOOT_REQUIRED, enablerType == MFENABLETYPE_MF_RebootRequired);
    THROW_HR_IF(MF_E_GRL_VERSION_TOO_LOW, enablerType == MFENABLETYPE_MF_UpdateRevocationInformation);
    THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_IMAGE_HASH), enablerType == MFENABLETYPE_MF_UpdateUntrustedComponent);
    // Other enabler types are handled by the CDM
    THROW_IF_FAILED(m_contentDecryptionModule->SetContentEnabler(contentEnabler.get(), asyncResult.get()));
    return S_OK;
} CATCH_RETURN();

HRESULT MediaEngineProtectionManager::EndEnableContent(IMFAsyncResult* /*asyncResult*/) noexcept
{
    return S_OK;
}


// MediaEngineProtectionManager implementation
winrt::event_token MediaEngineProtectionManager::ServiceRequested(winrt::Windows::Media::Protection::ServiceRequestedEventHandler const handler)
{
    THROW_HR(E_NOTIMPL);
}

void MediaEngineProtectionManager::ServiceRequested(winrt::event_token const /*cookie*/)
{ 
    THROW_HR(E_NOTIMPL);
}

winrt::event_token MediaEngineProtectionManager::RebootNeeded(winrt::Windows::Media::Protection::RebootNeededEventHandler const handler)
{
    THROW_HR(E_NOTIMPL);
}

void MediaEngineProtectionManager::RebootNeeded(winrt::event_token const /*cookie*/)
{
    THROW_HR(E_NOTIMPL);
}

winrt::event_token MediaEngineProtectionManager::ComponentLoadFailed(winrt::Windows::Media::Protection::ComponentLoadFailedEventHandler const handler)
{
    THROW_HR(E_NOTIMPL);
}

void MediaEngineProtectionManager::ComponentLoadFailed(winrt::event_token const /*cookie*/)
{
    THROW_HR(E_NOTIMPL);
}

winrt::Windows::Foundation::Collections::PropertySet MediaEngineProtectionManager::Properties()
{
    return m_propertySet;
}

}  // namespace media