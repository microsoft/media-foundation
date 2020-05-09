// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "CdmMediaSource.h"

namespace media
{

IFACEMETHODIMP CdmMediaSource::GetCharacteristics(DWORD *characteristics) noexcept
{
    return m_innerMFSource->GetCharacteristics(characteristics);
}

IFACEMETHODIMP CdmMediaSource::CreatePresentationDescriptor(IMFPresentationDescriptor **presentationDescriptorOut) noexcept
{
    return m_innerMFSource->CreatePresentationDescriptor(presentationDescriptorOut);
}

IFACEMETHODIMP CdmMediaSource::Start(IMFPresentationDescriptor *presentationDesc, const GUID *timeFormat, const PROPVARIANT *startPosition) noexcept
{
    return m_innerMFSource->Start(presentationDesc, timeFormat, startPosition);
}

IFACEMETHODIMP CdmMediaSource::Stop() noexcept
{
    return m_innerMFSource->Stop();
}

IFACEMETHODIMP CdmMediaSource::Pause() noexcept
{
    return m_innerMFSource->Pause();
}

IFACEMETHODIMP CdmMediaSource::Shutdown() noexcept
{
    {
        auto lock = m_itaMapLock.lock();
        m_itaMap.clear();
    }
    return m_innerMFSource->Shutdown();    
}

IFACEMETHODIMP CdmMediaSource::GetEvent(DWORD flags, IMFMediaEvent** eventOut) noexcept
{ 
    return m_innerMFSource->GetEvent(flags, eventOut);
}

IFACEMETHODIMP CdmMediaSource::BeginGetEvent(IMFAsyncCallback* callback, IUnknown* state) noexcept
{
    return m_innerMFSource->BeginGetEvent(callback, state);
}

IFACEMETHODIMP CdmMediaSource::EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** eventOut) noexcept
{ 
    return m_innerMFSource->EndGetEvent(result, eventOut);
}

IFACEMETHODIMP CdmMediaSource::QueueEvent(MediaEventType type, REFGUID extendedType, HRESULT status, const PROPVARIANT* value) noexcept
{ 
    return m_innerMFSource->QueueEvent(type, extendedType, status, value);
}

IFACEMETHODIMP CdmMediaSource::GetInputTrustAuthority(DWORD streamId, REFIID riid, IUnknown** objectOut) noexcept
try
{
    auto lock = m_itaMapLock.lock();
    winrt::com_ptr<IUnknown> itaUnknown;
    auto itaIter = m_itaMap.find(streamId);
    if (itaIter != m_itaMap.end())
    {
        itaUnknown = itaIter->second;
    }
    else
    {
        if (!m_cdmTrustedInput)
        {
            THROW_IF_FAILED(m_contentDecryptionModule->CreateTrustedInput(nullptr, 0, m_cdmTrustedInput.put()));
        }
        THROW_IF_FAILED(m_cdmTrustedInput->GetInputTrustAuthority(streamId, riid, itaUnknown.put()));
        m_itaMap[streamId] = itaUnknown;
    }
    itaUnknown.copy_to(objectOut);
    return S_OK;
}
CATCH_RETURN();

}