//**@@@*@@@****************************************************
//
// Microsoft Windows MediaFoundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//**@@@*@@@****************************************************

#include "stdafx.h"

#if CLEARKEY_INCLUDE_PEAUTH_HANDSHAKE
#include "peauthroots.h"

#define CLEARKEY_PEAUTH_ENCODING_TYPES (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING)
const LPSTR c_szGRLEKU      = (char*)"1.3.6.1.4.1.311.10.5.4";
const LPSTR c_szPEAuthEKU   = (char*)"1.3.6.1.4.1.311.10.5.7";
const LPSTR c_szPEKeyAlgRSA = (char*)"1.2.840.113549";

#define CLEARKEY_PEAUTH_MAX_RSA_OUTPUT_SIZE    512
#define PEAUTHHELPER_INIT_BUFFER_SIZE (5 * sizeof(DWORD))
#define GENERAL_GRL_DATA_SIZE_MAX 1048576

#define READ_FROM_PBGRL( _pbGRL_, _cbGRL_, _pbBuffer_, _cbBuffer_ )             \
    do {                                                                        \
        IF_FALSE_GOTO( (_cbBuffer_) <= (_cbGRL_), MF_E_GRL_INVALID_FORMAT );    \
        memcpy( (_pbBuffer_), (_pbGRL_), (_cbBuffer_) );                        \
        (_pbGRL_) += (_cbBuffer_);                                              \
        (_cbGRL_) -= (_cbBuffer_);                                              \
    } while( FALSE )

static const DWORD c_dwPEAuthVerify = 1;
static const DWORD c_dwPEAuthReset = 2;
static const DWORD c_dwRequireTrustedKernel = 1;
static const DWORD c_dwRequireTrustedUsermode = 1;
static const DWORD c_dwBlockTestSignatures = 1;

static void _ReverseMemCopy( _Out_writes_bytes_( cb ) BYTE *pbDest, _In_reads_bytes_( cb ) const BYTE *pbSource, DWORD cb )
{
    for( DWORD i = 0; i < cb; i++ )
    {
        pbDest[ cb - 1 - i ] = pbSource[ i ];
    }
}

static void _SerializeVerifyCallInput(
    _In_ BOOL  fRequireTrustedKernel,
    _In_ BOOL  fRequireTrustedUsermode,
    _In_ DWORD dwMinimumGRLVersionRequired,
    _In_ BOOL  fBlockTestSignatures,
    _Out_writes_bytes_( PEAUTHHELPER_INIT_BUFFER_SIZE ) BYTE rgbInput[ PEAUTHHELPER_INIT_BUFFER_SIZE ] )
{
    DWORD dw;

    BYTE *pb = rgbInput;
    dw = c_dwPEAuthVerify;
    memcpy( pb, &dw, sizeof( DWORD ) );
    pb += sizeof( DWORD );

    dw = fRequireTrustedKernel ? c_dwRequireTrustedKernel : 0;
    memcpy( pb, &dw, sizeof( DWORD ) );
    pb += sizeof( DWORD );

    dw = fRequireTrustedUsermode ? c_dwRequireTrustedUsermode : 0;
    memcpy( pb, &dw, sizeof( DWORD ) );
    pb += sizeof( DWORD );

    dw = dwMinimumGRLVersionRequired;
    memcpy( pb, &dw, sizeof( DWORD ) );
    pb += sizeof( DWORD );

    dw = fBlockTestSignatures ? c_dwBlockTestSignatures : 0;
    memcpy( pb, &dw, sizeof( DWORD ) );
    pb += sizeof( DWORD );
}

static void _DeserializeVerifyCallInput(
    _In_reads_bytes_( PEAUTHHELPER_INIT_BUFFER_SIZE - sizeof( DWORD ) ) const BYTE rgbInput[ PEAUTHHELPER_INIT_BUFFER_SIZE - sizeof( DWORD ) ],
    _Out_ BOOL  *pfRequireTrustedKernel,
    _Out_ BOOL  *pfRequireTrustedUsermode,
    _Out_ DWORD *pdwMinimumGRLVersionRequired,
    _Out_ BOOL  *pfBlockTestSignatures )
{
    const BYTE *pb = rgbInput;
    DWORD dw;

    memcpy( &dw, pb, sizeof( DWORD ) );
    pb += sizeof( DWORD );
    *pfRequireTrustedKernel = ( dw == c_dwRequireTrustedKernel );

    memcpy( &dw, pb, sizeof( DWORD ) );
    pb += sizeof( DWORD );
    *pfRequireTrustedUsermode = ( dw == c_dwRequireTrustedUsermode );

    memcpy( &dw, pb, sizeof( DWORD ) );
    pb += sizeof( DWORD );
    *pdwMinimumGRLVersionRequired = dw;

    memcpy( &dw, pb, sizeof( DWORD ) );
    pb += sizeof( DWORD );
    *pfBlockTestSignatures = ( dw == c_dwBlockTestSignatures );
}

static void _GenerateCertChainMessage( _In_ BOOL fRequireTrustedKernel, _Out_ PEAUTH_MSG *ppemsgCertChainMsg )
{
    ZeroMemory( ppemsgCertChainMsg, sizeof( PEAUTH_MSG ) );
    ppemsgCertChainMsg->Version = PEAUTHMSG_VERSION;
    ppemsgCertChainMsg->cbMsgSize = sizeof( PEAUTH_MSG );
    ppemsgCertChainMsg->Type = PEAUTH_MSG_CERTCHAIN;
    ppemsgCertChainMsg->SecurityLevel = fRequireTrustedKernel ? PEAUTH_MSG_HIGH_SECURITY : PEAUTH_MSG_LOW_SECURITY;
}

static HRESULT _GenerateSessionKeyMessage( _In_ BOOL fRequireTrustedKernel, _In_ CTByteArray &baTransientKeyEncrypted, _Out_ CTByteArray &baSessionKeyMessage )
{
    HRESULT hr = S_OK;
    PEAUTH_MSG *pemsgWeakRef = nullptr;

    IF_FAILED_GOTO( baSessionKeyMessage.SetSize( sizeof( PEAUTH_MSG ) + baTransientKeyEncrypted.GetSize() ) );
    pemsgWeakRef = (PEAUTH_MSG*)baSessionKeyMessage.P();

    ZeroMemory( pemsgWeakRef, sizeof( PEAUTH_MSG ) );

    pemsgWeakRef->Version = PEAUTHMSG_VERSION;
    pemsgWeakRef->cbMsgSize = baSessionKeyMessage.GetSize();
    pemsgWeakRef->Type = PEAUTH_MSG_ACQUIRESESSIONKEY;
    pemsgWeakRef->SecurityLevel = fRequireTrustedKernel ? PEAUTH_MSG_HIGH_SECURITY : PEAUTH_MSG_LOW_SECURITY;
    pemsgWeakRef->MsgBody.SessionMsg.ulEncryptedSessionKeyOffset = sizeof( PEAUTH_MSG );
    pemsgWeakRef->MsgBody.SessionMsg.cbEncryptedSessionkey = baTransientKeyEncrypted.GetSize();

    memcpy( baSessionKeyMessage.P() + sizeof( PEAUTH_MSG ), baTransientKeyEncrypted.P(), baTransientKeyEncrypted.GetSize() );

done:
    return hr;
}

static HRESULT _GenerateRandom( _Out_writes_bytes_( AES_BLOCK_LEN ) BYTE rgbRand[ AES_BLOCK_LEN ] )
{
    HRESULT     hr      = S_OK;
    NTSTATUS    status  = STATUS_SUCCESS;

    status = BCryptGenRandom( nullptr, rgbRand, AES_BLOCK_LEN, BCRYPT_USE_SYSTEM_PREFERRED_RNG );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

done:
    CLEARKEY_CONFIRM_NTSTATUS( status, hr );
    return hr;
}

static HRESULT _GenerateKernelInquiryMessage( _In_ BOOL fRequireTrustedKernel, _In_ DWORD dwGRLVersion, _Out_ PEAUTH_MSG *ppemsgKernelInquiryMsg )
{
    HRESULT hr = S_OK;

    ZeroMemory( ppemsgKernelInquiryMsg, sizeof( PEAUTH_MSG ) );
    ppemsgKernelInquiryMsg->Version = PEAUTHMSG_VERSION;
    ppemsgKernelInquiryMsg->cbMsgSize = sizeof( PEAUTH_MSG );
    ppemsgKernelInquiryMsg->Type = PEAUTH_MSG_STATEINQUIRY;
    ppemsgKernelInquiryMsg->SecurityLevel = fRequireTrustedKernel ? PEAUTH_MSG_HIGH_SECURITY : PEAUTH_MSG_LOW_SECURITY;
    ppemsgKernelInquiryMsg->MsgBody.KernelStateMsg.GRLVersion = dwGRLVersion;

    //
    // Generate a random nonce
    //
    IF_FAILED_GOTO( _GenerateRandom( ppemsgKernelInquiryMsg->MsgBody.KernelStateMsg.rgNonce ) );

done:
    return hr;
}

static HRESULT _VerifyTrustStatus( PCCERT_CHAIN_CONTEXT pChainContext )
{
    //
    // All PEAuth-related certificates are not in the trusted root certificate store nor do they have timestamps.
    // If the error status includes any problems BESIDES those, we don't trust the cert.
    //
    // Instead of the root being in the trusted root certificate store, we explicitly check the root's public key
    // against a fixed allow-list defined in peauthroots.h.
    //
    if( ( pChainContext->TrustStatus.dwErrorStatus & ~( CERT_TRUST_IS_NOT_TIME_VALID | CERT_TRUST_IS_UNTRUSTED_ROOT ) ) != 0 )
    {
        return MF_E_PE_UNTRUSTED;
    }
    return S_OK;
}

static HRESULT _VerifyBinarySigning( __in ULONG ulResponseFlag, __in BOOL fBlockTestSignatures )
{
    HRESULT hr                  = S_OK;
    ULONG   ulUserModeTrustBits = PEAUTH_USERMODE_TRUST_BITS( ulResponseFlag );

    //
    // The order of the following checks is important. These should be in the order of
    // ascending content access rights. This is because PEAuth will populate
    // the trust bits regardless of the hierarchy/importance of the roots. ITA needs
    // to make a decision based on all given trust bit flags
    //
    if( PEAUTH_IS_REAL_PMP_BETA_ROOT( ulUserModeTrustBits ) )
    {
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
    else if( PEAUTH_IS_MS_TEST_ROOT( ulUserModeTrustBits ) || PEAUTH_IS_TEST_PMP_BETA_ROOT( ulUserModeTrustBits ) )
    {
        IF_FALSE_GOTO( !fBlockTestSignatures, MF_E_HIGH_SECURITY_LEVEL_CONTENT_NOT_ALLOWED );
    }

done:
    return hr;
}

static HRESULT _VerifyPublicKeys(
    _In_        PCCERT_CHAIN_CONTEXT     pChainContext,
    _In_        BOOL                     fBlockTestSignatures,
    _In_opt_    GRL_HEADER              *pGRLHeader,
    _In_opt_    CTEntryArray<GRL_ENTRY> *pCoreEntries,
    _Out_opt_   CTByteArray             *pbaLeafPublicKey )
{
    HRESULT              hr                     = S_OK;
    BOOL                 fTrustedRoot           = FALSE;
    PCERT_SIMPLE_CHAIN   pChainWeakRef          = nullptr;
    PCCERT_CONTEXT       pRootCertWeakRef       = nullptr;
    BYTE                *pbRootSubjectWeakRef   = nullptr;
    DWORD                cbRootSubjectWeakRef   = 0;
    BYTE                *pbRootPubkeyWeakRef    = nullptr;
    DWORD                cbRootPubkeyWeakRef    = 0;
    BCRYPT_ALG_HANDLE    hSha1Alg               = nullptr;
    NTSTATUS             status                 = STATUS_SUCCESS;

    pChainWeakRef = pChainContext->rgpChain[ pChainContext->cChain - 1 ];
    pRootCertWeakRef = pChainWeakRef->rgpElement[ pChainWeakRef->cElement - 1 ]->pCertContext;
    pbRootSubjectWeakRef = pRootCertWeakRef->pCertInfo->Subject.pbData;
    cbRootSubjectWeakRef = pRootCertWeakRef->pCertInfo->Subject.cbData;
    pbRootPubkeyWeakRef = pRootCertWeakRef->pCertInfo->SubjectPublicKeyInfo.PublicKey.pbData;
    cbRootPubkeyWeakRef = pRootCertWeakRef->pCertInfo->SubjectPublicKeyInfo.PublicKey.cbData;

    //
    // Now we confirm that the root certificate is signed by a pubkey that we trust
    //
    for( DWORD iRoot = 0; iRoot < ARRAYSIZE( RootTable ) && !fTrustedRoot; iRoot++ )
    {
        const ROOT_ENTRY *pRootEntry = &RootTable[ iRoot ];

        if( pRootEntry->fIsTest && fBlockTestSignatures )
        {
            //
            // Skip test roots when we're blocking test signatures
            //
            continue;
        }

        if( cbRootSubjectWeakRef == pRootEntry->cbSubject && 0 == memcmp( pbRootSubjectWeakRef, pRootEntry->pbSubject, pRootEntry->cbSubject ) )
        {
            if( cbRootPubkeyWeakRef == pRootEntry->cbPubkey && 0 == memcmp( pbRootPubkeyWeakRef, pRootEntry->pbPubkey, pRootEntry->cbPubkey ) )
            {
                fTrustedRoot = TRUE;
            }
        }
    }
    IF_FALSE_GOTO( fTrustedRoot, MF_E_PE_UNTRUSTED );

    if( pGRLHeader != nullptr )
    {
        //
        // Now we confirm that none of the public keys in the certificate chain are revoked by the GRL
        //
        status = BCryptOpenAlgorithmProvider( &hSha1Alg, BCRYPT_SHA1_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0 );
        IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

        for( DWORD iPublicKey = 0; iPublicKey < pChainWeakRef->cElement; iPublicKey++ )
        {
            BYTE     rgbHash[ 20 ] = { 0 };
            BYTE    *pbNextPubkeyWeakRef = pChainWeakRef->rgpElement[ iPublicKey ]->pCertContext->pCertInfo->SubjectPublicKeyInfo.PublicKey.pbData;
            DWORD    cbNextPubkeyWeakRef = pChainWeakRef->rgpElement[ iPublicKey ]->pCertContext->pCertInfo->SubjectPublicKeyInfo.PublicKey.cbData;
            DWORD    ibRev = 0;

            status = BCryptHash( hSha1Alg, nullptr, 0, pbNextPubkeyWeakRef, cbNextPubkeyWeakRef, rgbHash, sizeof( rgbHash ) );
            IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

            ibRev = pGRLHeader->cRevokedUserBinaries + pGRLHeader->cRevokedKernelBinaries;

            for( DWORD iRevoked = 0; iRevoked < pGRLHeader->cRevokedCertificates; iRevoked++ )
            {
                GRL_ENTRY *pGRLEntry = nullptr;

                IF_FALSE_GOTO( pCoreEntries->GetAt( iRevoked + ibRev, &pGRLEntry ), MF_E_UNEXPECTED );

                if( 0 == memcmp( rgbHash, pGRLEntry, sizeof( rgbHash ) ) )
                {
                    //
                    // Hash of public key was in the GRL
                    //
                    IF_FAILED_GOTO( MF_E_COMPONENT_REVOKED );
                }
            }
        }
    }

    if( pbaLeafPublicKey != nullptr )
    {
        PCERT_PUBLIC_KEY_INFO pleafPubkeyWeakRef = &pChainWeakRef->rgpElement[ 0 ]->pCertContext->pCertInfo->SubjectPublicKeyInfo;

        if( 0 != memcmp( c_szPEKeyAlgRSA, pleafPubkeyWeakRef->Algorithm.pszObjId, strlen( c_szPEKeyAlgRSA ) ) )
        {
            //
            // PEAuth certs should always be using RSA for their public key
            //
            IF_FAILED_GOTO( MF_E_UNEXPECTED );
        }

        IF_FAILED_GOTO( pbaLeafPublicKey->Set( pleafPubkeyWeakRef->PublicKey.pbData, pleafPubkeyWeakRef->PublicKey.cbData ) );
    }

done:
    if( hSha1Alg != nullptr )
    {
        BCryptCloseAlgorithmProvider( hSha1Alg, 0 );
    }
    CLEARKEY_CONFIRM_NTSTATUS( status, hr );
    return hr;
}

static HRESULT _VerifyCertificateChain(
    _In_reads_bytes_( cbMessage ) const BYTE                    *pbMessage,
    _In_                                DWORD                    cbMessage,
    _In_z_                        const LPSTR                    pszUsageIdentifier,
    _In_                                BOOL                     fBlockTestSignatures,
    _In_opt_                            GRL_HEADER              *pGRLHeader,
    _In_opt_                            CTEntryArray<GRL_ENTRY> *pCoreEntries,
    _Out_opt_                           CTByteArray             *pbaLeafPublicKey )
{
    HRESULT              hr                 = S_OK;
    HCERTSTORE           hCertStorePEAuth   = nullptr;
    PCCERT_CONTEXT       pcertPEAuth        = nullptr;
    PCCERT_CHAIN_CONTEXT pChainContext      = nullptr;
    LPSTR rgpszUsageIdentifier[ 1 ] = { pszUsageIdentifier };
    CERT_CHAIN_PARA oChainParam;
    CERT_ENHKEY_USAGE eku;

    ZeroMemory( &oChainParam, sizeof( oChainParam ) );
    ZeroMemory( &eku, sizeof( eku ) );

    //
    // Parse the PEAuth certificate chain
    //
    hCertStorePEAuth = CryptGetMessageCertificates(
        CLEARKEY_PEAUTH_ENCODING_TYPES,
        NULL,   // cannot use nullptr
        CERT_STORE_CREATE_NEW_FLAG,
        pbMessage,
        cbMessage );
    if( hCertStorePEAuth == nullptr )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    //
    // Locate the leaf certificate with the given EKU
    //
    eku.cUsageIdentifier = 1;
    eku.rgpszUsageIdentifier = rgpszUsageIdentifier;
    pcertPEAuth = CertFindCertificateInStore(
        hCertStorePEAuth,
        CLEARKEY_PEAUTH_ENCODING_TYPES,
        CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG,
        CERT_FIND_ENHKEY_USAGE,
        &eku,
        nullptr );
    if( pcertPEAuth == nullptr )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    //
    // Verify the certificate chain based on that leaf
    //
    oChainParam.cbSize = sizeof( oChainParam );
    if( !CertGetCertificateChain(
        nullptr,
        pcertPEAuth,
        nullptr,
        hCertStorePEAuth,
        &oChainParam,
        CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL,
        nullptr,
        &pChainContext ) )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    IF_FAILED_GOTO( _VerifyTrustStatus( pChainContext ) );
    IF_FAILED_GOTO( _VerifyPublicKeys( pChainContext, fBlockTestSignatures, pGRLHeader, pCoreEntries, pbaLeafPublicKey ) );

done:
    if( pcertPEAuth != nullptr )
    {
        (void)CertFreeCertificateContext( pcertPEAuth );
    }
    if( pChainContext != nullptr )
    {
        (void)CertFreeCertificateChain( pChainContext );
    }
    if( hCertStorePEAuth != nullptr )
    {
        (void)CertCloseStore( hCertStorePEAuth, CERT_CLOSE_STORE_FORCE_FLAG );
    }
    return hr;
}

static HRESULT _VerifyResponseMessageStatus(
    _In_ BOOL fRequireTrustedKernel,
    _In_ BOOL fRequireTrustedUsermode,
    _In_ const PEAUTH_MSG_RESPONSE *pRsp )
{
    HRESULT hr              = S_OK;
    HRESULT hrStatus        = pRsp->ResponseBody.Status.hStatus;
    ULONG   ulResponseFlags = pRsp->ResponseBody.Status.ResponseFlag;

    IF_FALSE_GOTO( 0 == ( PEAUTH_REBOOT_REQUIRED & ulResponseFlags ), MF_E_REBOOT_REQUIRED );

    if( MF_E_PE_UNTRUSTED == hrStatus )
    {
        //
        // If the error code that we got indicates that the process is untrusted
        // and we do NOT have valid debug credentials, see if we can identify the
        // exact cause of the failure. If we have valid debug credentials, we ignore
        // MF_E_PE_UNTRUSTED and return success instead
        //
        if( 0 == ( PEAUTH_VERIFY_DEBUG_CREDENTIALS & ulResponseFlags ) )
        {
            ULONG ulResponseFlagsMasked = ulResponseFlags & ~PEAUTH_TRUSTBITS_MASK;

            IF_FALSE_GOTO( 0 == ( PEAUTH_ALL_PROCESS_RESTART_REQUIRED & ulResponseFlags ), MF_E_ALL_PROCESS_RESTART_REQUIRED );
            IF_FALSE_GOTO( 0 == ( PEAUTH_PROCESS_RESTART_REQUIRED & ulResponseFlags ), MF_E_PROCESS_RESTART_REQUIRED );

            if( 0 != ( ( PEAUTH_USER_MODE_UNSECURE | PEAUTH_USER_MODE_DEBUGGER ) & ulResponseFlags ) )
            {
                if( fRequireTrustedUsermode )
                {
                    IF_FAILED_GOTO( MF_E_PE_UNTRUSTED );
                }
            }

            //
            // If we reach this point:
            //  If we got ONLY these flags set: PEAUTH_PE_UNTRUSTED | PEAUTH_KERNEL_UNSECURE
            //   then the only reason that response message is a failure code is because of kernel insecure.
            //   We don't always care about that case - fail with MF_E_KERNEL_UNTRUSTED only if fRequireTrustedKernel.
            //  Else
            //   there's some additional reason (or other reason) that the response message is a failure code.
            //   We need to fail with MF_E_PE_UNTRUSTED in all such cases.
            //
            if( ulResponseFlagsMasked == ( PEAUTH_PE_UNTRUSTED | PEAUTH_KERNEL_UNSECURE ) )
            {
                if( fRequireTrustedKernel )
                {
                    IF_FAILED_GOTO( MF_E_KERNEL_UNTRUSTED );
                }
            }
            else
            {
                IF_FAILED_GOTO( MF_E_PE_UNTRUSTED );
            }
        }
    }
    else
    {
        IF_FAILED_GOTO( hrStatus );
    }

done:
    return hr;
}

static HRESULT _VerifyCMAC(
    _In_reads_bytes_( AES_BLOCK_LEN ) BYTE   pbKey[ AES_BLOCK_LEN ],
    _In_reads_bytes_( cbSigned )      BYTE  *pbSigned,
    _In_                              DWORD  cbSigned,
    _In_reads_bytes_( AES_BLOCK_LEN ) BYTE   pbSignature[ AES_BLOCK_LEN ] )
{
    BCRYPT_ALG_HANDLE            hAESCMACAlg            = nullptr;
    HRESULT                      hr                     = S_OK;
    NTSTATUS                     status                 = STATUS_SUCCESS;
    BYTE rgbAESCMAC[ AES_BLOCK_LEN ];

    status = BCryptOpenAlgorithmProvider( &hAESCMACAlg, BCRYPT_AES_CMAC_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0 );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

    status = BCryptHash( hAESCMACAlg, pbKey, AES_BLOCK_LEN, pbSigned, cbSigned, rgbAESCMAC, AES_BLOCK_LEN );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );
    IF_FALSE_GOTO( 0 == memcmp( rgbAESCMAC, pbSignature, AES_BLOCK_LEN ), MF_E_PE_UNTRUSTED );

done:
    if( hAESCMACAlg != nullptr )
    {
        BCryptCloseAlgorithmProvider( hAESCMACAlg, 0 );
    }
    CLEARKEY_CONFIRM_NTSTATUS( status, hr );
    return hr;
}

HRESULT Cdm_clearkey_PEAuthHelper::TransmitPEAuthMessage(
    _In_reads_bytes_( cbMsg )    const PEAUTH_MSG          *pMsg,
    _In_                               DWORD                cbMsg,
    _Inout_count_( cbRsp )             PEAUTH_MSG_RESPONSE *pRsp,
    _In_                               DWORD                cbRsp )
{
    HRESULT hr = S_OK;
    IF_FAILED_GOTO( m_spIMFProtectedEnvironmentAccess->Call( cbMsg, (const BYTE*)pMsg, cbRsp, (BYTE*)pRsp ) );
    IF_FALSE_GOTO( PEAUTHMSG_VERSION == pRsp->Version, MF_E_PEAUTH_UNTRUSTED );

done:
    return hr;
}

HRESULT Cdm_clearkey_PEAuthHelper::ParseGRLHeader( _Inout_count_( cbGRL ) BYTE *&pbGRL, _Inout_ UINT32 &cbGRL )
{
    HRESULT hr            = S_OK;
    DWORD   cbCoreSection = 0;

    READ_FROM_PBGRL( pbGRL, cbGRL, &m_oGRLHeader, sizeof( m_oGRLHeader ) );

    if( ( wcslen( m_oGRLHeader.wszIdentifier ) != wcslen( GRL_HEADER_IDENTIFIER ) )
     || ( 0 != wcscmp( m_oGRLHeader.wszIdentifier, GRL_HEADER_IDENTIFIER ) ) )
    {
        IF_FAILED_GOTO( MF_E_GRL_INVALID_FORMAT );
    }

    IF_FALSE_GOTO( GRL_FORMAT_MAJOR >= m_oGRLHeader.wFormatMajor, MF_E_GRL_UNRECOGNIZED_FORMAT );
    IF_FALSE_GOTO( GRL_FORMAT_MAJOR == m_oGRLHeader.wFormatMajor, MF_E_GRL_VERSION_TOO_LOW );

    IF_FALSE_GOTO( sizeof( GRL_HEADER ) <= m_oGRLHeader.cbExtensibleSectionOffset, MF_E_GRL_INVALID_FORMAT );

    SAFE_ADD_DWORD( m_oGRLHeader.cRevokedKernelBinaries, m_oGRLHeader.cRevokedUserBinaries, &cbCoreSection );
    SAFE_ADD_DWORD( cbCoreSection, m_oGRLHeader.cRevokedCertificates, &cbCoreSection );
    SAFE_ADD_DWORD( cbCoreSection, m_oGRLHeader.cTrustedRoots, &cbCoreSection );
    SAFE_MULT_DWORD( cbCoreSection, sizeof( GRL_ENTRY ), &cbCoreSection );

    IF_FALSE_GOTO( m_oGRLHeader.cbExtensibleSectionOffset - sizeof( GRL_HEADER ) == cbCoreSection, MF_E_GRL_INVALID_FORMAT );

    IF_FALSE_GOTO( m_oGRLHeader.cbRenewalSectionOffset >= m_oGRLHeader.cbExtensibleSectionOffset, MF_E_GRL_INVALID_FORMAT );
    IF_FALSE_GOTO( m_oGRLHeader.cExtensibleEntries * sizeof( GRL_EXTENSIBLE_ENTRY ) == m_oGRLHeader.cbRenewalSectionOffset - m_oGRLHeader.cbExtensibleSectionOffset, MF_E_GRL_INVALID_FORMAT );

done:
    return hr;
}

HRESULT Cdm_clearkey_PEAuthHelper::ParseGRLCoreEntries( _Inout_count_( cbGRL ) BYTE *&pbGRL, _Inout_ UINT32 &cbGRL )
{
    HRESULT hr           = S_OK;
    DWORD   cCoreEntries = 0;

    m_CoreEntries.RemoveAll();

    SAFE_ADD_DWORD( m_oGRLHeader.cRevokedKernelBinaries, m_oGRLHeader.cRevokedUserBinaries, &cCoreEntries );
    SAFE_ADD_DWORD( cCoreEntries, m_oGRLHeader.cRevokedCertificates, &cCoreEntries );
    SAFE_ADD_DWORD( cCoreEntries, m_oGRLHeader.cTrustedRoots, &cCoreEntries );

    IF_FALSE_GOTO( GENERAL_GRL_DATA_SIZE_MAX / sizeof( GRL_ENTRY ) >= cCoreEntries, MF_E_GRL_INVALID_FORMAT );

    IF_FAILED_GOTO( m_CoreEntries.SetSize( cCoreEntries ) );

    READ_FROM_PBGRL( pbGRL, cbGRL, m_CoreEntries.P(), sizeof( GRL_ENTRY ) * cCoreEntries );

done:
    return hr;
}

HRESULT Cdm_clearkey_PEAuthHelper::ParseGRLExtEntries( _Inout_count_( cbGRL ) BYTE *&pbGRL, _Inout_ UINT32 &cbGRL )
{
    HRESULT hr                 = S_OK;
    DWORD   cExtensibleEntries = 0;

    m_ExtEntries.RemoveAll();

    cExtensibleEntries = m_oGRLHeader.cExtensibleEntries;
    IF_FALSE_GOTO( GENERAL_GRL_DATA_SIZE_MAX / sizeof( GRL_EXTENSIBLE_ENTRY ) >= cExtensibleEntries, MF_E_GRL_INVALID_FORMAT );

    IF_FAILED_GOTO( m_ExtEntries.SetSize( cExtensibleEntries ) );

    READ_FROM_PBGRL( pbGRL, cbGRL, m_ExtEntries.P(), sizeof( GRL_EXTENSIBLE_ENTRY ) * cExtensibleEntries );

done:
    return hr;
}

HRESULT Cdm_clearkey_PEAuthHelper::ParseGRLRenewalEntries( _Inout_count_( cbGRL ) BYTE *&pbGRL, _Inout_ UINT32 &cbGRL )
{
    HRESULT hr              = S_OK;
    DWORD   cRenewalEntries = 0;

    m_RenewalEntries.RemoveAll();

    SAFE_ADD_DWORD( m_oGRLHeader.cRevokedKernelBinaryRenewals, m_oGRLHeader.cRevokedUserBinaryRenewals, &cRenewalEntries );
    SAFE_ADD_DWORD( cRenewalEntries, m_oGRLHeader.cRevokedCertificateRenewals, &cRenewalEntries );

    IF_FALSE_GOTO( GENERAL_GRL_DATA_SIZE_MAX / sizeof( GRL_RENEWAL_ENTRY ) >= cRenewalEntries, MF_E_GRL_INVALID_FORMAT );

    IF_FAILED_GOTO( m_RenewalEntries.SetSize( cRenewalEntries ) );
    READ_FROM_PBGRL( pbGRL, cbGRL, m_RenewalEntries.P(), sizeof( GRL_RENEWAL_ENTRY ) * cRenewalEntries );

done:
    return hr;
}

HRESULT Cdm_clearkey_PEAuthHelper::ParseGRLSignature( _Inout_count_( cbGRL ) BYTE *&pbGRL, _Inout_ UINT32 &cbGRL, _Out_ CTByteArray &baSignature )
{
    HRESULT       hr                = S_OK;
    DWORD         cbSignature       = sizeof( MF_SIGNATURE ) - sizeof( BYTE );
    MF_SIGNATURE  oPartialSignature = { 0 };
    MF_SIGNATURE *pFullSignature    = nullptr;

    baSignature.RemoveAll();

    READ_FROM_PBGRL( pbGRL, cbGRL, &oPartialSignature, cbSignature );
    IF_FALSE_GOTO( GENERAL_GRL_DATA_SIZE_MAX >= oPartialSignature.cbSign, MF_E_GRL_INVALID_FORMAT );
    IF_FALSE_GOTO( GENERAL_GRL_DATA_SIZE_MAX >= cbSignature, MF_E_GRL_INVALID_FORMAT );
    IF_FALSE_GOTO( GENERAL_GRL_DATA_SIZE_MAX - cbSignature >= oPartialSignature.cbSign, MF_E_GRL_INVALID_FORMAT );

    cbSignature += oPartialSignature.cbSign;

    IF_FAILED_GOTO( baSignature.SetSize( cbSignature ) );

    pFullSignature = (MF_SIGNATURE*)baSignature.P();
    pFullSignature->dwSignVer = oPartialSignature.dwSignVer;
    pFullSignature->cbSign = oPartialSignature.cbSign;

    READ_FROM_PBGRL( pbGRL, cbGRL, pFullSignature->rgSign, pFullSignature->cbSign );

done:
    return hr;
}

HRESULT Cdm_clearkey_PEAuthHelper::VerifyGRLCoreSignature()
{
    HRESULT              hr                 = S_OK;
    const MF_SIGNATURE  *pMFSignature       = (const MF_SIGNATURE*)m_baCoreSignature.P();
    const BYTE          *rgEntries          = (const BYTE*) m_CoreEntries.P();
    DWORD                cbDataSize         = m_oGRLHeader.cbExtensibleSectionOffset;
    BYTE                *pbSignedWeakRef    = nullptr;
    CTByteArray baData;
    CRYPT_VERIFY_MESSAGE_PARA oVerifyParam;

    ZeroMemory( &oVerifyParam, sizeof( oVerifyParam ) );

    IF_FAILED_GOTO( baData.SetSize( cbDataSize ) );
    memcpy( baData.P(), (BYTE*)&m_oGRLHeader, sizeof( GRL_HEADER ) );
    memcpy( baData.P() + sizeof( GRL_HEADER ), rgEntries, cbDataSize - sizeof( GRL_HEADER ) );
    pbSignedWeakRef = baData.P();

    oVerifyParam.cbSize = sizeof( oVerifyParam );
    oVerifyParam.dwMsgAndCertEncodingType = CLEARKEY_PEAUTH_ENCODING_TYPES;

    //
    // Verify the signature of the GRL against the leaf certificate in the chain
    //
    {
        const BYTE* rgpbTobeSigned[] = { pbSignedWeakRef };
        if( !CryptVerifyDetachedMessageSignature( &oVerifyParam, 0, pMFSignature->rgSign, pMFSignature->cbSign, 1, rgpbTobeSigned, &cbDataSize, nullptr ) )
        {
            IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
            IF_FAILED_GOTO( MF_E_UNEXPECTED );
        }
    }

    //
    // Verify the GRL's signing certificate chain
    // The GRL can't revoke its own certificate, so pass null for the GRL information
    //
    IF_FAILED_GOTO( _VerifyCertificateChain( pMFSignature->rgSign, pMFSignature->cbSign, c_szGRLEKU, m_fBlockTestSignatures, nullptr, nullptr, nullptr ) );

done:
    return hr;
}

HRESULT Cdm_clearkey_PEAuthHelper::InitializeSessionKey()
{
    HRESULT                      hr                     = S_OK;
    HCRYPTPROV                   hProv                  = NULL;     // can't use nullptr
    HCRYPTKEY                    hKey                   = NULL;     // can't use nullptr
    DWORD                        cbImportableKey        = 0;
    PEAUTH_MSG_RESPONSE          oRsp                   = { 0 };
    DWORD                        cbExported             = 0;
    RSAPUBKEY                   *pPubKeyWeakRef         = nullptr;
    BYTE                        *pbModulusWeakRef       = nullptr;
    DWORD                        cbModulus              = 0;
    DWORD                        dwExponent             = 0;
    DWORD                        cbExponent             = 0;
    BCRYPT_RSAKEY_BLOB          *pBlobWeakRef           = nullptr;
    BCRYPT_ALG_HANDLE            hRSAAlg                = nullptr;
    BCRYPT_KEY_HANDLE            hRSAKey                = nullptr;
    NTSTATUS                     status                 = STATUS_SUCCESS;
    BYTE                        *pbTempWeakRef          = nullptr;
    BCRYPT_OAEP_PADDING_INFO     oPaddingInfo           = { 0 };
    DWORD                        cbEncrypted            = 0;
    BCRYPT_ALG_HANDLE            hAESECBAlg             = nullptr;
    BCRYPT_KEY_HANDLE            hAESECBTransientKey    = nullptr;
    DWORD                        cbDecrypted            = 0;
    BYTE rgbTransientKey[ AES_BLOCK_LEN ];
    CTByteArray baPEAuthRSA4096PublicKey;
    CTByteArray baImportableKey;
    CTByteArray baTransientKeyEncrypted;
    CTByteArray baSessionKeyMessage;
    CTByteArray baExportedKey;
    CTByteArray baBlob;

    IF_FAILED_GOTO( this->VerifyPEAuthCertificateChain( baPEAuthRSA4096PublicKey ) );

    //
    // Import the RSA public key from X.509 form and then export it as PUBLICKEYBLOB (i.e. PUBLICKEYSTRUC plus RSAPUBKEY plus modulus bytes)
    //
    if( !CryptDecodeObjectEx( CLEARKEY_PEAUTH_ENCODING_TYPES, RSA_CSP_PUBLICKEYBLOB, baPEAuthRSA4096PublicKey.P(), baPEAuthRSA4096PublicKey.GetSize(), 0, nullptr, nullptr, &cbImportableKey ) )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
    if( cbImportableKey == 0 )
    {
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
    IF_FAILED_GOTO( baImportableKey.SetSize( cbImportableKey ) );

    if( !CryptDecodeObjectEx( CLEARKEY_PEAUTH_ENCODING_TYPES, RSA_CSP_PUBLICKEYBLOB, baPEAuthRSA4096PublicKey.P(), baPEAuthRSA4096PublicKey.GetSize(), 0, nullptr, baImportableKey.P(), &cbImportableKey ) )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    if( !CryptAcquireContext( &hProv, nullptr, MS_ENHANCED_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT ) )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    if( !CryptImportKey( hProv, baImportableKey.P(), baImportableKey.GetSize(), NULL, CRYPT_EXPORTABLE, &hKey ) )  // can't use nullptr
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    if( !CryptExportKey( hKey, NULL, PUBLICKEYBLOB, 0, nullptr, &cbExported ) )     // can't use nullptr for second param
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
    IF_FAILED_GOTO( baExportedKey.SetSize( cbExported ) );
    if( !CryptExportKey( hKey, NULL, PUBLICKEYBLOB, 0, baExportedKey.P(), &cbExported ) )     // can't use nullptr
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
    IF_FAILED_GOTO( baExportedKey.SetSize( cbExported ) );

    //
    // Convert the output from exported PUBLICKEYBLOB to BCRYPT_RSAKEY_BLOB so we can import into BCrypt.
    // We can't use the Crypt* APIs because they do not have sufficient support for the variant of OAEP needed.
    // We must do the conversion manually because the LEGACY_RSAPUBLIC_BLOB type
    //  is not supported by the Microsoft primitive provider for BCrypt.
    // The conversion includes:
    //   Confirming that the algorithm is RSA
    //   Confirming the 'magic' number that the export should have included
    //   Converting from a little-endian DWORD exponent to a big-endian exponent
    //    which only includes the exact number of significant bytes
    //   Converting the modulus similarly.
    //
    if( cbExported < sizeof( PUBLICKEYSTRUC ) + sizeof( RSAPUBKEY ) )
    {
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
    if( ( ( (PUBLICKEYSTRUC*)( baExportedKey.P() ) )->aiKeyAlg != CALG_RSA_KEYX )
     && ( ( (PUBLICKEYSTRUC*)( baExportedKey.P() ) )->aiKeyAlg != CALG_RSA_SIGN ) )
    {
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
    pPubKeyWeakRef = ( (RSAPUBKEY*)( baExportedKey.P() + sizeof( PUBLICKEYSTRUC ) ) );
    if( pPubKeyWeakRef->magic != 0x31415352 )   // 'RSA1' for public keys per https://docs.microsoft.com/en-us/windows/desktop/api/wincrypt/ns-wincrypt-_rsapubkey
    {
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
    cbModulus = ( pPubKeyWeakRef->bitlen + 7 ) / 8;
    if( cbExported - ( sizeof( PUBLICKEYSTRUC ) + sizeof( RSAPUBKEY ) ) < cbModulus )
    {
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
    pbModulusWeakRef = baExportedKey.P() + sizeof( PUBLICKEYSTRUC ) + sizeof( RSAPUBKEY );

    dwExponent = pPubKeyWeakRef->pubexp;
    cbExponent = ( dwExponent & 0xFF000000 ) ? 4 : ( dwExponent & 0x00FF0000 ) ? 3 : ( dwExponent & 0x0000FF00 ) ? 2 : 1;
    IF_FAILED_GOTO( baBlob.SetSize( sizeof( BCRYPT_RSAKEY_BLOB ) + cbExponent + cbModulus ) );
    ZeroMemory( baBlob.P(), baBlob.GetSize() );
    pBlobWeakRef = (BCRYPT_RSAKEY_BLOB*)baBlob.P();
    pBlobWeakRef->Magic = BCRYPT_RSAPUBLIC_MAGIC;
    pBlobWeakRef->BitLength = pPubKeyWeakRef->bitlen;
    pBlobWeakRef->cbPublicExp = cbExponent;
    pBlobWeakRef->cbModulus = cbModulus;
    pBlobWeakRef->cbPrime1 = 0;
    pBlobWeakRef->cbPrime2 = 0;

    pbTempWeakRef = (BYTE*)( baBlob.P() + sizeof( BCRYPT_RSAKEY_BLOB ) );
    for( DWORD iexp = 0; iexp < cbExponent; iexp++ )
    {
        pbTempWeakRef[ iexp ] = ( (BYTE*)( &dwExponent ) )[ cbExponent - iexp - 1 ];
    }

    pbTempWeakRef += cbExponent;
    _ReverseMemCopy( pbTempWeakRef, pbModulusWeakRef, cbModulus );

    //
    // Import the converted RSA public key into BCrypt
    //
    status = BCryptOpenAlgorithmProvider( &hRSAAlg, BCRYPT_RSA_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0 );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

    status = BCryptImportKeyPair( hRSAAlg, nullptr, BCRYPT_RSAPUBLIC_BLOB, &hRSAKey, baBlob.P(), baBlob.GetSize(), 0 );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

    //
    // Generate a random key 'TransientKey'
    //
    IF_FAILED_GOTO( _GenerateRandom( rgbTransientKey ) );

    //
    // Encrypt 'TransientKey' with RSA using SHA1 as the OAEP algorithm with no label specified
    //
    IF_FAILED_GOTO( baTransientKeyEncrypted.SetSize( CLEARKEY_PEAUTH_MAX_RSA_OUTPUT_SIZE ) );
    oPaddingInfo.pszAlgId = BCRYPT_SHA1_ALGORITHM;
    status = BCryptEncrypt( hRSAKey, rgbTransientKey, AES_BLOCK_LEN, &oPaddingInfo, nullptr, 0, baTransientKeyEncrypted.P(), baTransientKeyEncrypted.GetSize(), &cbEncrypted, BCRYPT_PAD_OAEP );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );
    IF_FAILED_GOTO( baTransientKeyEncrypted.SetSize( cbEncrypted ) );

    //
    // Send 'TransientKey' to the kernel
    //
    IF_FAILED_GOTO( _GenerateSessionKeyMessage( m_fRequireTrustedKernel, baTransientKeyEncrypted, baSessionKeyMessage ) );
    IF_FAILED_GOTO( this->TransmitPEAuthMessage( (const PEAUTH_MSG*)baSessionKeyMessage.P(), baSessionKeyMessage.GetSize(), &oRsp, sizeof( oRsp ) ) );
    IF_FAILED_GOTO( _VerifyResponseMessageStatus( m_fRequireTrustedKernel, m_fRequireTrustedUsermode, &oRsp ) );
    IF_FAILED_GOTO( _VerifyBinarySigning( oRsp.ResponseBody.Status.ResponseFlag, m_fBlockTestSignatures ) );

    //
    // The entire response body is CMAC signed with 'TransientKey', so verify that signature
    //
    IF_FAILED_GOTO( _VerifyCMAC( rgbTransientKey, (BYTE*)&oRsp.ResponseBody, sizeof( oRsp.ResponseBody ), oRsp.rgSignedValue ) );

    //
    // Finally, obtain the session key by decrypting it using 'TransientKey'
    //
    status = BCryptOpenAlgorithmProvider( &hAESECBAlg, BCRYPT_AES_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0 );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

    status = BCryptGenerateSymmetricKey( hAESECBAlg, &hAESECBTransientKey, nullptr, 0, rgbTransientKey, AES_BLOCK_LEN, 0 );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

    status = BCryptSetProperty( hAESECBTransientKey, BCRYPT_CHAINING_MODE, (BYTE*)BCRYPT_CHAIN_MODE_ECB, sizeof( BCRYPT_CHAIN_MODE_ECB ), 0 );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

    status = BCryptDecrypt( hAESECBTransientKey, oRsp.ResponseBody.RespMsgBody.SessionMsg.rgEncryptedSessionKey, AES_BLOCK_LEN, nullptr, nullptr, 0, m_rgbSessionKeyClear, AES_BLOCK_LEN, &cbDecrypted, 0 );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );
    IF_FALSE_GOTO( cbDecrypted == AES_BLOCK_LEN, MF_E_UNEXPECTED );

done:
    if( hKey != NULL )  // can't use nullptr
    {
        CryptDestroyKey( hKey );
    }
    if( hProv != NULL ) // can't use nullptr
    {
        CryptReleaseContext( hProv, 0 );
    }
    if( hRSAKey != nullptr )
    {
        BCryptDestroyKey( hRSAKey );
    }
    if( hRSAAlg != nullptr )
    {
        BCryptCloseAlgorithmProvider( hRSAAlg, 0 );
    }
    if( hAESECBTransientKey != nullptr )
    {
        BCryptDestroyKey( hAESECBTransientKey );
    }
    if( hAESECBAlg != nullptr )
    {
        BCryptCloseAlgorithmProvider( hAESECBAlg, 0 );
    }
    CLEARKEY_CONFIRM_NTSTATUS( status, hr );
    return hr;
}

HRESULT Cdm_clearkey_PEAuthHelper::VerifyPEAuthCertificateChain(
    _Out_   CTByteArray &baPEAuthRSA4096PublicKey )
{
    HRESULT              hr                     = S_OK;
    PEAUTH_MSG           oMsg                   = { 0 };
    PEAUTH_MSG_RESPONSE  oRspSizeFetch          = { 0 };
    DWORD                cbCertChain            = 0;
    DWORD                cbRspCert              = 0;
    PEAUTH_MSG_RESPONSE *pRspCertWeakRef        = nullptr;
    BYTE                *pbCertChainWeakRef     = nullptr;
    BYTE                *pbGRLOffset            = nullptr;
    UINT32               cbGRLRemaining         = 0;
    CTByteArray          baRspCert;

    //
    // Obtain the PEAuth cert chain - two pass call to get the cert's size then get the cert itself
    //
    _GenerateCertChainMessage( m_fRequireTrustedKernel, &oMsg );
    IF_FAILED_GOTO( this->TransmitPEAuthMessage( &oMsg, sizeof( oMsg ), &oRspSizeFetch, sizeof( oRspSizeFetch ) ) );

    if( oRspSizeFetch.ResponseBody.Status.hStatus != MF_E_INSUFFICIENT_BUFFER )
    {
        IF_FAILED_GOTO( oRspSizeFetch.ResponseBody.Status.hStatus );
        IF_FAILED_GOTO( MF_E_PEAUTH_UNTRUSTED );
    }

    cbCertChain = oRspSizeFetch.ResponseBody.RespMsgBody.CertChainMsg.cbCertChain;
    IF_FALSE_GOTO( cbCertChain > 0, MF_E_PE_UNTRUSTED );

    cbRspCert = sizeof( PEAUTH_MSG_RESPONSE ) + cbCertChain;
    IF_FAILED_GOTO( baRspCert.SetSize( cbRspCert ) );
    ZeroMemory( baRspCert.P(), cbRspCert );

    pRspCertWeakRef = (PEAUTH_MSG_RESPONSE*)baRspCert.P();

    oMsg.MsgBody.CertChainMsg.cbCertChain = cbCertChain;
    IF_FAILED_GOTO( this->TransmitPEAuthMessage( &oMsg, sizeof( oMsg ), pRspCertWeakRef, cbRspCert ) );
    IF_FAILED_GOTO( _VerifyResponseMessageStatus( m_fRequireTrustedKernel, m_fRequireTrustedUsermode, pRspCertWeakRef ) );

    pbCertChainWeakRef = baRspCert.P() + pRspCertWeakRef->ResponseBody.RespMsgBody.CertChainMsg.ulCertChainOffset;

    //
    // Obtain the GRL (global revocation list), parse it, verify its signature, and verify its minimum version
    //
    IF_FAILED_GOTO( m_spIMFProtectedEnvironmentAccess->ReadGRL( &m_cbGRL, &m_pbGRL ) );
    pbGRLOffset = m_pbGRL;
    cbGRLRemaining = m_cbGRL;

    IF_FAILED_GOTO( this->ParseGRLHeader( pbGRLOffset, cbGRLRemaining ) );
    IF_FAILED_GOTO( this->ParseGRLCoreEntries( pbGRLOffset, cbGRLRemaining ) );
    IF_FAILED_GOTO( this->ParseGRLExtEntries( pbGRLOffset, cbGRLRemaining ) );
    IF_FAILED_GOTO( this->ParseGRLRenewalEntries( pbGRLOffset, cbGRLRemaining ) );
    IF_FAILED_GOTO( this->ParseGRLSignature( pbGRLOffset, cbGRLRemaining, m_baCoreSignature ) );
    IF_FAILED_GOTO( this->ParseGRLSignature( pbGRLOffset, cbGRLRemaining, m_baExtSignature ) );
    IF_FAILED_GOTO( this->VerifyGRLCoreSignature() );
    IF_FALSE_GOTO( m_oGRLHeader.dwSequenceNumber >= m_dwMinimumGRLVersionRequired, MF_E_GRL_VERSION_TOO_LOW );

    //
    // Our new minimum GRL version is the one we found on disk to make sure it isn't rolled back later
    //
    m_dwMinimumGRLVersionRequired = m_oGRLHeader.dwSequenceNumber;

    //
    // Verify the PEAuth certificate chain and check it for revocation against the GRL
    //
    IF_FAILED_GOTO( _VerifyCertificateChain( pbCertChainWeakRef, cbCertChain, c_szPEAuthEKU, m_fBlockTestSignatures, &m_oGRLHeader, &m_CoreEntries, &baPEAuthRSA4096PublicKey ) );

done:
    return hr;
}

Cdm_clearkey_PEAuthHelper::Cdm_clearkey_PEAuthHelper()
{
}

Cdm_clearkey_PEAuthHelper::~Cdm_clearkey_PEAuthHelper()
{
    Reset();
}

HRESULT Cdm_clearkey_PEAuthHelper::RuntimeClassInitialize()
{
    Reset();
    return S_OK;
}

STDMETHODIMP Cdm_clearkey_PEAuthHelper::Call( _In_ UINT32 inputLength, _In_reads_bytes_( inputLength ) const BYTE *input, _In_ UINT32 outputLength, _Out_writes_bytes_( outputLength ) BYTE *output )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr          = S_OK;
    DWORD   dwRequest   = 0;

    IF_FALSE_GOTO( inputLength >= sizeof( DWORD ), E_INVALIDARG );
    memcpy( &dwRequest, input, sizeof( DWORD ) );

    switch( dwRequest )
    {
    case c_dwPEAuthVerify:
        IF_FALSE_GOTO( inputLength == PEAUTHHELPER_INIT_BUFFER_SIZE, E_INVALIDARG );
        _DeserializeVerifyCallInput( input + sizeof( DWORD ), &m_fRequireTrustedKernel, &m_fRequireTrustedUsermode, &m_dwMinimumGRLVersionRequired, &m_fBlockTestSignatures );
        IF_FAILED_GOTO( this->Verify() );
        break;
    case c_dwPEAuthReset:
        IF_FALSE_GOTO( inputLength == sizeof( DWORD ), E_INVALIDARG );
        this->Reset();
        break;
    default:
        hr = E_INVALIDARG;
        goto done;
    }

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_PEAuthHelper::ReadGRL( _Out_ UINT32 *outputLength, _Outptr_result_bytebuffer_( *outputLength ) BYTE **output )
{
    //
    // This function is never called on this helper class
    //
    return E_NOTIMPL;
}

void Cdm_clearkey_PEAuthHelper::Reset()
{
    m_spIMFProtectedEnvironmentAccess = nullptr;
    CoTaskMemFree( m_pbGRL );
    m_pbGRL = nullptr;
    m_cbGRL = 0;
    ZeroMemory( &m_oGRLHeader, sizeof( m_oGRLHeader ) );
    m_CoreEntries.RemoveAll();
    m_ExtEntries.RemoveAll();
    m_RenewalEntries.RemoveAll();
    m_baCoreSignature.RemoveAll();
    m_baExtSignature.RemoveAll();
    m_fSessionKeyInitialized = FALSE;
    ZeroMemory( m_rgbSessionKeyClear, sizeof( AES_BLOCK_LEN ) );
}

HRESULT Cdm_clearkey_PEAuthHelper::Verify()
{
    HRESULT hr = S_OK;

    if( m_spIMFProtectedEnvironmentAccess == nullptr )
    {
        m_fSessionKeyInitialized = FALSE;
        IF_FAILED_GOTO( MFCreateProtectedEnvironmentAccess( m_spIMFProtectedEnvironmentAccess.GetAddressOf() ) );
    }

    if( !m_fSessionKeyInitialized )
    {
        IF_FAILED_GOTO( this->InitializeSessionKey() );
        m_fSessionKeyInitialized = TRUE;
    }

    IF_FAILED_GOTO( this->VerifyState() );

done:
    if( FAILED( hr ) )
    {
        //
        // On any failure, force any future Verify call to start from scratch
        //
        this->Reset();
    }
    return hr;
}


HRESULT Cdm_clearkey_PEAuthHelper::VerifyState()
{
    HRESULT              hr     = S_OK;
    PEAUTH_MSG           oMsg   = { 0 };
    PEAUTH_MSG_RESPONSE  oRsp   = { 0 };

    //
    // Obtain the current state of the protected environment from the kernel
    //
    IF_FAILED_GOTO( _GenerateKernelInquiryMessage( m_fRequireTrustedKernel, m_dwMinimumGRLVersionRequired, &oMsg ) );
    IF_FAILED_GOTO( this->TransmitPEAuthMessage( &oMsg, sizeof( oMsg ), &oRsp, sizeof( oRsp ) ) );
    IF_FAILED_GOTO( _VerifyResponseMessageStatus( m_fRequireTrustedKernel, m_fRequireTrustedUsermode, &oRsp ) );
    IF_FAILED_GOTO( _VerifyBinarySigning( oRsp.ResponseBody.Status.ResponseFlag, m_fBlockTestSignatures ) );

    //
    // The entire response body is CMAC signed with the session key, so verify that signature
    //
    IF_FAILED_GOTO( _VerifyCMAC( m_rgbSessionKeyClear, (BYTE*)&oRsp.ResponseBody, sizeof( oRsp.ResponseBody ), oRsp.rgSignedValue ) );

    //
    // Verify the nonce to prevent replay attacks
    //
    IF_FALSE_GOTO( 0 == memcmp( oMsg.MsgBody.KernelStateMsg.rgNonce, oRsp.ResponseBody.RespMsgBody.KernelStateMsg.rgNonce, AES_BLOCK_LEN ), MF_E_PE_UNTRUSTED );

    //
    // Verify that the GRL hasn't been rolled back
    //
    IF_FALSE_GOTO( m_dwMinimumGRLVersionRequired <= oRsp.ResponseBody.RespMsgBody.KernelStateMsg.GRLVersion, MF_E_GRL_VERSION_TOO_LOW );

done:
    return hr;
}

HRESULT VerifyPEAuthUsingPEAuthHelper(
    _In_        IMFProtectedEnvironmentAccess *pCdm_clearkey_PEAuthHelper,
    _In_        BOOL                           fRequireTrustedKernel,
    _In_        BOOL                           fRequireTrustedUsermode,
    _In_        DWORD                          dwMinimumGRLVersionRequired,
    _In_        BOOL                           fBlockTestSignatures,
    _Inout_opt_ IMFAttributes                 *pIMFAttributesCDM )
{
    HRESULT hr                          = S_OK;
    TRACE_FUNC_HR( hr );
    BYTE rgbInput[ PEAUTHHELPER_INIT_BUFFER_SIZE ];

    _SerializeVerifyCallInput( fRequireTrustedKernel, fRequireTrustedUsermode, dwMinimumGRLVersionRequired, fBlockTestSignatures, rgbInput );
    IF_FAILED_GOTO( pCdm_clearkey_PEAuthHelper->Call( PEAUTHHELPER_INIT_BUFFER_SIZE, rgbInput, 0, nullptr ) );

done:
    return hr;
}

HRESULT ResetPEAuthUsingPEAuthHelper(
    _In_ IMFProtectedEnvironmentAccess *pCdm_clearkey_PEAuthHelper )
{
    return pCdm_clearkey_PEAuthHelper->Call( sizeof( c_dwPEAuthReset ), (const BYTE*)&c_dwPEAuthReset, 0, nullptr );
}

#else   //CLEARKEY_INCLUDE_PEAUTH_HANDSHAKE

Cdm_clearkey_PEAuthHelper::Cdm_clearkey_PEAuthHelper()
{
}

Cdm_clearkey_PEAuthHelper::~Cdm_clearkey_PEAuthHelper()
{
}

HRESULT Cdm_clearkey_PEAuthHelper::RuntimeClassInitialize()
{
    return S_OK;
}

HRESULT VerifyPEAuthUsingPEAuthHelper(
    _In_        IMFProtectedEnvironmentAccess *pCdm_clearkey_PEAuthHelper,
    _In_        BOOL                           fRequireTrustedKernel,
    _In_        BOOL                           fRequireTrustedUsermode,
    _In_        DWORD                          dwMinimumGRLVersionRequired,
    _In_        BOOL                           fBlockTestSignatures,
    _Inout_opt_ IMFAttributes                 *pIMFAttributesCDM )
{
    return S_OK;
}

HRESULT ResetPEAuthUsingPEAuthHelper(
    _In_ IMFProtectedEnvironmentAccess *pCdm_clearkey_PEAuthHelper )
{
    return S_OK;
}

STDMETHODIMP Cdm_clearkey_PEAuthHelper::Call( _In_ UINT32 inputLength, _In_reads_bytes_( inputLength ) const BYTE *input, _In_ UINT32 outputLength, _Out_writes_bytes_( outputLength ) BYTE *output )
{
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_PEAuthHelper::ReadGRL( _Out_ UINT32 *outputLength, _Outptr_result_bytebuffer_( *outputLength ) BYTE **output )
{
    return E_NOTIMPL;
}

#endif  //CLEARKEY_INCLUDE_PEAUTH_HANDSHAKE

