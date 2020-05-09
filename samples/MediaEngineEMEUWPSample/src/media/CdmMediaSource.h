#pragma once

#include "eme/MediaKeys.h"

namespace media
{

// A custom MF media source wrapper which supports protected playback using a provided ContentDecryptionModule
// Demuxing is performed by the inner source provided by the application
class CdmMediaSource
    : public winrt::implements<CdmMediaSource, IMFMediaSource, IMFTrustedInput>
{
  public:
    CdmMediaSource(std::shared_ptr<eme::MediaKeys> mediaKeys, IMFMediaSource* innerSource)
    {
        m_contentDecryptionModule = mediaKeys->GetCDM();
        m_innerMFSource.copy_from(innerSource);
    }
    ~CdmMediaSource() override = default;

    // IMFMediaSource
    IFACEMETHODIMP GetCharacteristics(DWORD* characteristics) noexcept override;
    IFACEMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor** presentationDescriptorOut) noexcept override;
    IFACEMETHODIMP Start(IMFPresentationDescriptor* presentationDescriptor, const GUID* guidTimeFormat, 
                         const PROPVARIANT* startPosition) noexcept override;
    IFACEMETHODIMP Stop() noexcept override;
    IFACEMETHODIMP Pause() noexcept override;
    IFACEMETHODIMP Shutdown() noexcept override;

    // IMFMediaEventGenerator
    // Note: IMFMediaSource inherits IMFMediaEventGenerator.
    IFACEMETHODIMP GetEvent(DWORD flags, IMFMediaEvent** eventOut) noexcept override;
    IFACEMETHODIMP BeginGetEvent(IMFAsyncCallback* callback, IUnknown* state) noexcept override;
    IFACEMETHODIMP EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** eventOut) noexcept override;
    IFACEMETHODIMP QueueEvent(MediaEventType type, REFGUID extendedType, HRESULT status, const PROPVARIANT* value) noexcept override;

    // IMFTrustedInput
    IFACEMETHODIMP GetInputTrustAuthority(DWORD streamId, REFIID riid, IUnknown** objectOut) noexcept override;

  private:
    wil::critical_section m_itaMapLock;
    winrt::com_ptr<IMFContentDecryptionModule> m_contentDecryptionModule;
    winrt::com_ptr<IMFTrustedInput> m_cdmTrustedInput;
    winrt::com_ptr<IMFMediaSource> m_innerMFSource;
    std::map<DWORD, winrt::com_ptr<IUnknown>> m_itaMap;
};

} // namespace media