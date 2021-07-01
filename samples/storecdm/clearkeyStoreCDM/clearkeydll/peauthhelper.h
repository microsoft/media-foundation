//**@@@*@@@****************************************************
//
// Microsoft Windows MediaFoundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//**@@@*@@@****************************************************

#pragma once

#include <windows.h>
#include "storecdm_clearkey_private_import_types.h"

//
// These two methods along with the helper class declared below demonstrate an implementation of performing PEAuth, 
// i.e. verifying that this DLL is running inside mfpmp.exe, using standard Windows APIs.
//
// This implementation should NOT be used in a production CDM because it is trivial for an attacker to simply bypass the PEAuth checks.
//
// A production implementation of a CDM should not call ANY standard windows APIs during PEAuth.
// Instead, it would implement its own X.509 certificate parsing/verification and cryptography as well
// as using CDM-specific obfuscation technologies to ensure that this functionality is not tampered with at runtime
// (including tamper-prevention for both code and variables in memory on heap and stack).
//
HRESULT VerifyPEAuthUsingPEAuthHelper(
    _In_        IMFProtectedEnvironmentAccess *pCdm_clearkey_PEAuthHelper,
    _In_        BOOL                           fRequireTrustedKernel,
    _In_        BOOL                           fRequireTrustedUsermode,
    _In_        DWORD                          dwMinimumGRLVersionRequired,
    _In_        BOOL                           fBlockTestSignatures,
    _Inout_opt_ IMFAttributes                 *pIMFAttributesCDM );

HRESULT ResetPEAuthUsingPEAuthHelper(
    _In_ IMFProtectedEnvironmentAccess *pCdm_clearkey_PEAuthHelper );

class Cdm_clearkey_PEAuthHelper :
    public RuntimeClass<RuntimeClassFlags<ClassicCom>, ::IMFProtectedEnvironmentAccess, FtmBase>
{
public:
    Cdm_clearkey_PEAuthHelper();
    ~Cdm_clearkey_PEAuthHelper();
    HRESULT RuntimeClassInitialize();

    // IMFProtectedEnvironmentAccess
    STDMETHODIMP Call( _In_ UINT32 inputLength, _In_reads_bytes_( inputLength ) const BYTE *input, _In_ UINT32 outputLength, _Out_writes_bytes_( outputLength ) BYTE *output );
    STDMETHODIMP ReadGRL( _Out_ UINT32 *outputLength, _Outptr_result_bytebuffer_( *outputLength ) BYTE **output );

#if CLEARKEY_INCLUDE_PEAUTH_HANDSHAKE

private:
    void Reset();
    HRESULT Verify();
    HRESULT InitializeSessionKey();
    HRESULT VerifyState();
    HRESULT VerifyPEAuthCertificateChain(
        _Out_   CTByteArray &baPEAuthRSA4096PublicKey );
    HRESULT TransmitPEAuthMessage(
        _In_reads_bytes_( cbMsg )    const PEAUTH_MSG          *pMsg,
        _In_                               DWORD                cbMsg,
        _Inout_count_( cbRsp )             PEAUTH_MSG_RESPONSE *pRsp,
        _In_                               DWORD                cbRsp );

    HRESULT ParseGRLHeader( _Inout_count_( cbGRL ) BYTE *&pbGRL, _Inout_ UINT32 &cbGRL );
    HRESULT ParseGRLCoreEntries( _Inout_count_( cbGRL ) BYTE *&pbGRL, _Inout_ UINT32 &cbGRL );
    HRESULT ParseGRLExtEntries( _Inout_count_( cbGRL ) BYTE *&pbGRL, _Inout_ UINT32 &cbGRL );
    HRESULT ParseGRLRenewalEntries( _Inout_count_( cbGRL ) BYTE *&pbGRL, _Inout_ UINT32 &cbGRL );
    HRESULT ParseGRLSignature( _Inout_count_( cbGRL ) BYTE *&pbGRL, _Inout_ UINT32 &cbGRL, _Out_ CTByteArray &baSignature );
    HRESULT VerifyGRLCoreSignature();

    CriticalSection m_Lock;
    BOOL         m_fRequireTrustedKernel                = TRUE;
    BOOL         m_fRequireTrustedUsermode              = TRUE;
    DWORD        m_dwMinimumGRLVersionRequired          = 0;
    BOOL         m_fBlockTestSignatures                 = TRUE;
    BYTE        *m_pbGRL                                = nullptr;
    UINT32       m_cbGRL                                = 0;
    GRL_HEADER   m_oGRLHeader                           = { 0 };
    BOOL         m_fSessionKeyInitialized               = FALSE;
    BYTE         m_rgbSessionKeyClear[ AES_BLOCK_LEN ]  = { 0 };
    CTEntryArray<GRL_ENTRY>                 m_CoreEntries;
    CTEntryArray<GRL_EXTENSIBLE_ENTRY>      m_ExtEntries;
    CTEntryArray<GRL_RENEWAL_ENTRY>         m_RenewalEntries;
    CTByteArray                             m_baCoreSignature;
    CTByteArray                             m_baExtSignature;
    ComPtr<IMFProtectedEnvironmentAccess>   m_spIMFProtectedEnvironmentAccess;

#endif  //CLEARKEY_INCLUDE_PEAUTH_HANDSHAKE
};

