//**@@@*@@@****************************************************
//
// Microsoft Windows MediaFoundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//**@@@*@@@****************************************************

#pragma once

#include <windows.h>
#include <mfidl.h>

class Cdm_clearkey_IMFContentEnabler final :
    public Microsoft::WRL::RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, IUnused, IMFContentEnabler, ChainInterfaces<::IPersistStream, ::IPersist>, IMFAttributes, CloakedIid<::IMFShutdown>, FtmBase>
{
    InspectableClass( RuntimeClass_org_w3_clearkey_ContentEnabler, BaseTrust );

public:
    Cdm_clearkey_IMFContentEnabler();
    ~Cdm_clearkey_IMFContentEnabler();
    HRESULT RuntimeClassInitialize();

    // IPersistStream
    STDMETHODIMP GetClassID( _Out_ CLSID *pclsid );
    STDMETHODIMP GetSizeMax( _Out_ ULARGE_INTEGER *pcbSize );
    STDMETHODIMP IsDirty();
    STDMETHODIMP Load( _In_ IStream *pStream );
    STDMETHODIMP Save( _Inout_ IStream *pStm, _In_ BOOL fClearDirty );

    // IMFContentEnabler
    STDMETHODIMP AutomaticEnable();
    STDMETHODIMP Cancel();
    STDMETHODIMP GetEnableData( _Outptr_result_bytebuffer_( *pcbData ) BYTE **ppbData, _Out_ DWORD *pcbData );
    STDMETHODIMP GetEnableType( _Out_ GUID *pType );
    STDMETHODIMP GetEnableURL(
        _Out_writes_bytes_( *pcchURL )  LPWSTR                 *ppwszURL,
        _Out_                           DWORD                  *pcchURL,
        _Inout_                         MF_URL_TRUST_STATUS    *pTrustStatus );
    STDMETHODIMP IsAutomaticSupported( _Out_ BOOL *pAutomatic );
    STDMETHODIMP MonitorEnable();

    // IMFShutdown
    STDMETHODIMP Shutdown( void );
    STDMETHODIMP GetShutdownStatus( _Out_ MFSHUTDOWN_STATUS *pStatus );

    // IMFAttributes
    CDM_clearkey_IMPLEMENT_IMFATTRIBUTES( m_spIMFAttributes );

private:
    CriticalSection m_Lock;
    ComPtr<IMFAttributes> m_spIMFAttributes;
    BOOL m_fShutdown = FALSE;

    Cdm_clearkey_IMFContentEnabler( const Cdm_clearkey_IMFContentEnabler &nonCopyable ) = delete;
    Cdm_clearkey_IMFContentEnabler& operator=( const Cdm_clearkey_IMFContentEnabler &nonCopyable ) = delete;
};

