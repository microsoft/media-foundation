//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#include "stdafx.h"

#pragma warning( disable:4127 )   // "conditional expression is constant"

DWORD Cdm_clearkey_IMFTransform::s_dwNextPolicyId = 0;

static const MFPOLICYMANAGER_ACTION s_rgbSupportedActions[] = { PEACTION_PLAY, PEACTION_EXTRACT, PEACTION_NO };

#define CLEARKEY_MFT_AUTOLOCK_STATE SyncLock2 stateLock = m_StateLock.Lock()
#define CLEARKEY_MFT_AUTOLOCK_INPUT SyncLock2 inputLock = m_InputLock.Lock()
#define CLEARKEY_MFT_AUTOLOCK_OUTPUT SyncLock2 outputLock = m_OutputLock.Lock()
#define CLEARKEY_MFT_STATE_LOCK_RELEASE stateLock.Unlock()
#define CLEARKEY_MFT_STATE_LOCK_REACQUIRE stateLock = SyncLock2(m_StateLock.Lock())
#define CLEARKEY_MFT_OUTPUT_LOCK_RELEASE outputLock.Unlock()
#define CLEARKEY_MFT_OUTPUT_LOCK_REACQUIRE outputLock = SyncLock2(m_OutputLock.Lock())

#define CLEARKEY_MFT_UNLOCK_BEFORE_LONG_RUNNING_OPERATION( _fDuringProcessSample_, _pfOutputLockIsUnlocked_ ) this->LongRunningOperationStartUnlockOutputLock( stateLock, outputLock, _fDuringProcessSample_, _pfOutputLockIsUnlocked_ )

//
// We can't directly use IF_FAILED_GOTO in this macro because hr may already be set to a value
// which the caller expects to remain unchanged.  So, we only set it on failure.
//
#define CLEARKEY_MFT_RELOCK_AFTER_LONG_RUNNING_OPERATION( _pfOutputLockIsUnlocked_ ) do {                                   \
    HRESULT _hrTemp_ = this->LongRunningOperationEndRelockOutputLock( stateLock, outputLock, _pfOutputLockIsUnlocked_ );    \
    if( FAILED( _hrTemp_ ) )                                                                                                \
    {                                                                                                                       \
        IF_FAILED_GOTO( _hrTemp_ );                                                                                         \
    }                                                                                                                       \
} while(FALSE)

static HRESULT _MoveSampleIntoQueue(
    _Inout_ CTPtrList<IMFSample>   &oSampleQueue,
    _Inout_ ComPtr<IMFSample>      &spSample )
{
    HRESULT hr = S_OK;
    IMFSample *pSample = spSample.Detach();

    IF_NULL_GOTO( oSampleQueue.AddTail( pSample ), E_OUTOFMEMORY );
    pSample = nullptr;

done:
    SAFE_RELEASE( pSample );
    return hr;
}

static BOOL _IsSupportedAction( _In_ MFPOLICYMANAGER_ACTION eAction )
{
    switch( eAction )
    {
    case PEACTION_PLAY:
    case PEACTION_EXTRACT:
    case PEACTION_NO:
        return TRUE;
    default:
        return FALSE;
    }
}

static REFGUID _ActionToGuid( _In_ MFPOLICYMANAGER_ACTION eAction )
{
    switch( eAction )
    {
    case PEACTION_PLAY:
        return CLEARKEY_GUID_ATTRIBUTE_PEACTION_PLAY;
    case PEACTION_EXTRACT:
        return CLEARKEY_GUID_ATTRIBUTE_PEACTION_EXTRACT;
    case PEACTION_NO:
        return CLEARKEY_GUID_ATTRIBUTE_PEACTION_NO;
    default:
        return GUID_NULL;
    }
}

static HRESULT _LocateUsableLicense(
    _In_      IMFAttributes *pIMFAttributesCDM,
    _In_      REFGUID        guidKid,
    _Inout_   GUID          *pguidLid,
    _Out_opt_ BYTE           pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _Out_opt_ BOOL          *pfNotify,
    _Out_opt_ BOOL          *pfExpired )
{
    HRESULT hr = S_OK;
    BOOL fNotify = FALSE;
    BOOL fFound  = FALSE;

    if( pfExpired != nullptr )
    {
        *pfExpired = FALSE;
    }

    IF_FAILED_GOTO( ForAllKeys( pIMFAttributesCDM, CLEARKEY_INVALID_SESSION_ID, [&hr, &guidKid, &fNotify, &pIMFAttributesCDM, &pguidLid, &fFound, &pbKeyData, &pfExpired]( REFGUID guidKeyDataLambda, BYTE *pbKeyDataLambda, BOOL *pfHaltEnumerationLambda )->HRESULT
    {
        GUID guidKidLambda = GUID_NULL;
        GUID guidLid = GUID_NULL;
        MF_MEDIAKEY_STATUS eCurrentStatus;
        MF_MEDIAKEY_STATUS eLastNotifiedStatus;
        double dblExpiration = NaN.f;
        GetKeyData( pbKeyDataLambda, nullptr, &guidKidLambda, nullptr, &eCurrentStatus, &guidLid, &eLastNotifiedStatus, &dblExpiration, nullptr );
        if( guidKid == guidKidLambda )
        {
            //
            // Matches kid
            //
            if( IS_STATUS_USABLE_OR_PENDING( eCurrentStatus ) )
            {
                MF_MEDIAKEY_STATUS eNewStatus = MF_MEDIAKEY_STATUS_USABLE;

                if( IsExpired( dblExpiration ) )
                {
                    eNewStatus = MF_MEDIAKEY_STATUS_EXPIRED;
                    if( pfExpired != nullptr )
                    {
                        *pfExpired = TRUE;
                    }
                }

                //
                // We will KeyStatusChange when NotifyMediaKeyStatus is called below,
                // but only on sessions with keys where a key status change occurred.
                // Leave the last-notified-status as-is.
                // NotifyMediaKeyStatus will update it if it has changed.
                //
                fNotify = fNotify || ( eNewStatus != eLastNotifiedStatus );
                if( eNewStatus != eCurrentStatus )
                {
                    SetKeyDataUpdateStatus( pbKeyDataLambda, eNewStatus );
                    IF_FAILED_GOTO( pIMFAttributesCDM->SetBlob( guidKeyDataLambda, pbKeyDataLambda, KEY_DATA_TOTAL_SIZE ) );
                }

                if( eNewStatus == MF_MEDIAKEY_STATUS_USABLE )
                {
                    //
                    // Usable license
                    //
                    if( *pguidLid == GUID_NULL )
                    {
                        //
                        // No specific Lid requested: set Lid and use this license
                        //
                        *pguidLid = guidLid;
                        fFound = TRUE;
                    }
                    if( *pguidLid == guidLid )
                    {
                        //
                        // Matches existing Lid: use this license
                        //
                        fFound = TRUE;
                    }
                }
            }
        }

        if( fFound )
        {
            if( pbKeyData != nullptr )
            {
                memcpy( pbKeyData, pbKeyDataLambda, KEY_DATA_TOTAL_SIZE );
            }
            *pfHaltEnumerationLambda = TRUE;
        }
    done:
        return hr;
    } ) );
    IF_FALSE_GOTO( fFound, MF_E_LICENSE_INCORRECT_RIGHTS );

    if( pfNotify != nullptr )
    {
        *pfNotify = fNotify;
    }

done:
    return hr;
}

static BOOL _PolicyRequiresOTA( _In_ const CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS& oPolicy )
{
    return oPolicy.dwHDCP != CLEARKEY_POLICY_DO_NOT_ENFORCE_HDCP
        || oPolicy.dwCGMSA != CLEARKEY_POLICY_DO_NOT_ENFORCE_CGMSA
        || oPolicy.dwDigitalOnly != CLEARKEY_POLICY_ALLOW_ANALOG_OUTPUT;
}

static BOOL _PolicyRequiresHDCP( _In_ DWORD dwHDCP )
{
    return dwHDCP != CLEARKEY_POLICY_DO_NOT_ENFORCE_HDCP;
}

static BOOL _PolicyRequiresCGMSA( _In_ DWORD dwCGMSA )
{
    switch( dwCGMSA )
    {
    case CLEARKEY_POLICY_DO_NOT_ENFORCE_CGMSA:
    case CLEARKEY_POLICY_BEST_EFFORT_CGMSA_COPY_FREE:
    case CLEARKEY_POLICY_BEST_EFFORT_CGMSA_COPY_ONCE:
    case CLEARKEY_POLICY_BEST_EFFORT_CGMSA_COPY_NEVER:
        return FALSE;
    case CLEARKEY_POLICY_REQUIRE_CGMSA_COPY_FREE:
    case CLEARKEY_POLICY_REQUIRE_CGMSA_COPY_ONCE:
    case CLEARKEY_POLICY_REQUIRE_CGMSA_COPY_NEVER:
    default:
        return TRUE;
    }
}

static HRESULT _SetSchemaAttribute(
    _Inout_ IMFOutputSchema *pIMFOutputSchema,
    _In_    REFGUID          guidAttribute,
    _In_    UINT32           uiAttribute )
{
    PROPVARIANTWRAP propvar;
    propvar.vt = VT_UI4;
    propvar.ulVal = uiAttribute;
    return pIMFOutputSchema->SetItem( guidAttribute, propvar );
}

Cdm_clearkey_IMFTrustedInput::Cdm_clearkey_IMFTrustedInput()
{
}

Cdm_clearkey_IMFTrustedInput::~Cdm_clearkey_IMFTrustedInput()
{
}

HRESULT Cdm_clearkey_IMFTrustedInput::RuntimeClassInitialize()
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR( hr );

    IF_FAILED_GOTO( MFCreateAttributes( &m_spIMFAttributes, 0 ) );

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTrustedInput::GetInputTrustAuthority( _In_ DWORD streamId, _In_ REFIID riid, _COM_Outptr_ IUnknown **authority )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    ComPtr<IMFInputTrustAuthority> spIMFInputTrustAuthority;

    hr = MakeAndInitialize<Cdm_clearkey_IMFInputTrustAuthority, IMFInputTrustAuthority>( spIMFInputTrustAuthority.GetAddressOf(), this, streamId );
    IF_FAILED_GOTO( hr );
    *authority = spIMFInputTrustAuthority.Detach();

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTrustedInput::GetClassID( _Out_ CLSID *pclsid )
{
    //
    // API not used
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFTrustedInput::GetSizeMax( _Out_ ULARGE_INTEGER *pcbSize )
{
    //
    // API not used
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFTrustedInput::IsDirty()
{
    //
    // API not used
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFTrustedInput::Load( _In_ IStream *pStream )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);

    IF_FAILED_GOTO( MFDeserializeAttributesFromStream( m_spIMFAttributes.Get(), MF_ATTRIBUTE_SERIALIZE_UNKNOWN_BYREF, pStream ) );

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTrustedInput::Save( _Inout_ IStream *pStm, _In_ BOOL fClearDirty )
{
    //
    // API not used
    //
    return E_NOTIMPL;
}

Cdm_clearkey_IMFInputTrustAuthority::Cdm_clearkey_IMFInputTrustAuthority()
{
}

Cdm_clearkey_IMFInputTrustAuthority::~Cdm_clearkey_IMFInputTrustAuthority()
{
    this->DisableCurrentDecryptersAndSetInsecureEnvironment();
}

HRESULT Cdm_clearkey_IMFInputTrustAuthority::RuntimeClassInitialize( _In_ IMFAttributes *pIMFAttributesFromTI, _In_ UINT32 uiStreamId )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    GUID guidDisable;
    GUID guidOriginatorID;
    ComPtr<IMFAttributes> spIMFAttributes;

    IF_FAILED_GOTO( MFCreateAttributes( &spIMFAttributes, 7 ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_TI_TO_ITA, pIMFAttributesFromTI ) );

    //
    // https://docs.microsoft.com/en-us/windows/desktop/api/mfidl/nf-mfidl-imfoutputpolicy-getoriginatorid
    // "All of the policy objects and output schemas from the same ITA should return the same originator identifier (including dynamic policy changes)."
    //
    // This implementation uses a random GUID for the ITA but sets the high-order DWORD to the stream ID
    // to make it easy to determine for which stream a given policy object is being applied (e.g. while debugging).
    //
    IF_FAILED_GOTO( CoCreateGuid( &guidOriginatorID ) );
    guidOriginatorID.Data1 = uiStreamId;
    IF_FAILED_GOTO( spIMFAttributes->SetGUID( CLEARKEY_GUID_ATTRIBUTE_ORIGINATOR_ID, guidOriginatorID ) );

    IF_FAILED_GOTO( spIMFAttributes->SetUINT32( _ActionToGuid( PEACTION_PLAY ), FALSE ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUINT32( _ActionToGuid( PEACTION_EXTRACT ), FALSE ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUINT32( _ActionToGuid( PEACTION_NO ), FALSE ) );

    //
    // Initialize a unique 'disable' GUID for all decrypters we create before Reset is called,
    // and mark that guid as NOT disabled ("FALSE").
    //
    IF_FAILED_GOTO( CoCreateGuid( &guidDisable ) );
    IF_FAILED_GOTO( spIMFAttributes->SetGUID( CLEARKEY_GUID_ATTRIBUTE_DISABLE_GUID, guidDisable ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUINT32( guidDisable, FALSE ) );

    hr = MakeAndInitialize<Cdm_clearkey_PEAuthHelper, IMFProtectedEnvironmentAccess>( m_spCdm_clearkey_PEAuthHelper.GetAddressOf() );
    IF_FAILED_GOTO( hr );

    m_spIMFAttributes = spIMFAttributes;

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFInputTrustAuthority::GetDecrypter( _In_ REFIID riid, _COM_Outptr_ void **ppv )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    ComPtr<IMFTransform> spIMFTransform;

    IF_FALSE_GOTO( !m_fShutdown, MF_E_SHUTDOWN );

    hr = MakeAndInitialize<Cdm_clearkey_IMFTransform, IMFTransform>( spIMFTransform.GetAddressOf(), this, m_spCdm_clearkey_PEAuthHelper.Get(), m_guidKid, m_guidLid );
    IF_FAILED_GOTO( hr );
    IF_FAILED_GOTO( spIMFTransform.CopyTo( riid, ppv ) );

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFInputTrustAuthority::VerifyPEAuth()
{
    HRESULT hr = S_OK;
    BOOL    fRequireTrustedKernel       = FALSE;
    BOOL    fRequireTrustedUsermode     = FALSE;
    DWORD   dwMinimumGRLVersionRequired = 0;
    BOOL    fBlockTestSignatures        = FALSE;
    ComPtr<IMFAttributes> spIMFAttributesTI;
    ComPtr<IMFAttributes> spIMFAttributesCDM;

    IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_TI_TO_ITA, IID_PPV_ARGS( &spIMFAttributesTI ) ) );
    IF_FAILED_GOTO( spIMFAttributesTI->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_TI, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );

    IF_FAILED_GOTO( VerifyPEAuthUsingPEAuthHelper(
        m_spCdm_clearkey_PEAuthHelper.Get(),
        fRequireTrustedKernel,
        fRequireTrustedUsermode,
        dwMinimumGRLVersionRequired,
        fBlockTestSignatures,
        spIMFAttributesCDM.Get() ) );
done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFInputTrustAuthority::RequestAccess( _In_ MFPOLICYMANAGER_ACTION eAction, _COM_Outptr_ IMFActivate **ppContentEnablerActivate )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    BYTE            *pbContentInitData  = nullptr;
    UINT32           cbContentInitData  = 0;
    BYTE            *pbKids             = nullptr;
    DWORD            cbKids             = 0;
    LARGE_INTEGER    li                 = { 0 };
    ULARGE_INTEGER   uli                = { 0 };
    GUID             guidLid            = GUID_NULL;

    ComPtr<IMFAttributes> spIMFAttributesTI;
    ComPtr<IMFAttributes> spIMFAttributesCDM;
    ComPtr<IMFAttributes> spIMFAttributesForCE;
    ComPtr<IStream> spIStream;
    ComPtr<IMFGetService> spIMFGetServiceCDM;
    ComPtr<IMFPMPHostApp> spIMFPMPHostApp;

    *ppContentEnablerActivate = nullptr;

    IF_FALSE_GOTO( !m_fShutdown, MF_E_SHUTDOWN );

    //
    // The ITA only allows the PLAY, EXTRACT and NO actions
    // NOTE: Topology created only on the basis of EXTRACT or NO action will NOT decrypt content
    //
    if( PEACTION_EXTRACT == eAction || PEACTION_NO == eAction )
    {
        goto done;
    }
    else
    {
        IF_FALSE_GOTO( PEACTION_PLAY == eAction, MF_E_ITA_UNSUPPORTED_ACTION );
    }

    IF_FAILED_GOTO( this->VerifyPEAuth() );

    IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_TI_TO_ITA, IID_PPV_ARGS( &spIMFAttributesTI ) ) );
    IF_FAILED_GOTO( spIMFAttributesTI->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_TI, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );

    hr = spIMFAttributesTI->GetAllocatedBlob( CLEARKEY_GUID_ATTRIBUTE_PSSH_FROM_CONTENT_FILE_OR_GEN_REQUEST, &pbContentInitData, &cbContentInitData );
    if( hr == MF_E_ATTRIBUTENOTFOUND )
    {
        //
        // We don't have any init data.  Without it, we don't have a Kid.
        // Thus, we can't fire "waiting for key" because it requires a Kid.
        // The content might be clear or a license might be acquired out-of-band.
        // Either way, RequestAccess should just succeed.
        //
        hr = S_OK;
        goto done;
    }
    IF_FAILED_GOTO( hr );

    IF_FAILED_GOTO( GetKidsFromPSSH( cbContentInitData, pbContentInitData, &cbKids, &pbKids ) );

    for( DWORD ibKids = 0; ibKids < cbKids && SUCCEEDED( hr ); ibKids += sizeof( GUID ) )
    {
        GUID guidKid;
        guidLid = GUID_NULL;
        BIG_ENDIAN_SIXTEEN_BYTES_TO_GUID( &pbKids[ ibKids ], &guidKid );
        hr = _LocateUsableLicense( spIMFAttributesCDM.Get(), guidKid, &guidLid, nullptr, nullptr, nullptr );
    }
    if( SUCCEEDED( hr ) )
    {
        if( cbKids == sizeof( GUID ) )
        {
            BIG_ENDIAN_SIXTEEN_BYTES_TO_GUID( pbKids, &m_guidKid );
            m_guidLid = guidLid;
        }
        else
        {
            m_guidKid = GUID_NULL;
            m_guidLid = GUID_NULL;
        }
        //
        // We have key data for all Kids in the PSSH box, so RequestAccess should just succeed.
        //
        goto done;
    }
    else if( hr != MF_E_LICENSE_INCORRECT_RIGHTS )
    {
        IF_FAILED_GOTO( hr );
    }

    //
    // At this point, we know we do NOT have key data for all Kids from the PSSH (hr == MF_E_LICENSE_INCORRECT_RIGHTS).
    // Thus, RequestAccess needs to fail and return an IMFActivate which can activate an IMFContentEnabler.
    //
    IF_FAILED_GOTO( MFCreateAttributes( &spIMFAttributesForCE, 1 ) );
    IF_FAILED_GOTO( spIMFAttributesForCE->SetBlob( CLEARKEY_GUID_ATTRIBUTE_PSSH_FROM_CONTENT_FILE_OR_GEN_REQUEST, pbContentInitData, cbContentInitData ) );

    IF_FAILED_GOTO( CreateStreamOnHGlobal( nullptr, TRUE, &spIStream ) );
    IF_FAILED_GOTO( MFSerializeAttributesToStream( spIMFAttributesForCE.Get(), MF_ATTRIBUTE_SERIALIZE_UNKNOWN_BYREF, spIStream.Get() ) );
    IF_FAILED_GOTO( spIStream->Seek( li, STREAM_SEEK_SET, &uli ) );

    //
    // We need a pointer to the PMP Host object to create an object cross process.
    //
    IF_FAILED_GOTO( spIMFAttributesCDM.As( &spIMFGetServiceCDM ) );
    IF_FAILED_GOTO( spIMFGetServiceCDM->GetService( MF_CONTENTDECRYPTIONMODULE_SERVICE, IID_PPV_ARGS( &spIMFPMPHostApp ) ) );

    IF_FAILED_GOTO( MFCreateEncryptedMediaExtensionsStoreActivate( spIMFPMPHostApp.Get(), spIStream.Get(), RuntimeClass_org_w3_clearkey_ContentEnabler, ppContentEnablerActivate ) );
    IF_FAILED_GOTO( NS_E_LICENSE_REQUIRED );

done:
    CoTaskMemFree( pbContentInitData );
    delete[] pbKids;
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFInputTrustAuthority::GetPolicy( _In_ MFPOLICYMANAGER_ACTION eAction, _COM_Outptr_ IMFOutputPolicy **ppPolicy )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    ComPtr<IMFOutputPolicy> spIMFOutputPolicy;

    IF_FALSE_GOTO( !m_fShutdown, MF_E_SHUTDOWN );

    hr = MakeAndInitialize<Cdm_clearkey_IMFOutputPolicy, IMFOutputPolicy>( &spIMFOutputPolicy, this, eAction, m_guidKid, m_guidLid );
    IF_FAILED_GOTO( hr );

    IF_FAILED_GOTO( this->SetUINT32( _ActionToGuid( eAction ), TRUE ) );

    *ppPolicy = spIMFOutputPolicy.Detach();

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFInputTrustAuthority::BindAccess( _In_ MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS *pParams )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);

    IF_FALSE_GOTO( !m_fShutdown, MF_E_SHUTDOWN );

    IF_FALSE_GOTO( nullptr != pParams, E_INVALIDARG );
    IF_FALSE_GOTO( 0 == pParams->dwVer, E_INVALIDARG ); // Per MSDN, this value must always be zero

    for( DWORD iAction = 0; iAction < pParams->cActions; iAction++ )
    {
        MFPOLICYMANAGER_ACTION eAction = pParams->rgOutputActions[ iAction ].Action;
        IF_FALSE_GOTO( _IsSupportedAction( eAction ), MF_E_UNEXPECTED );
        IF_FAILED_GOTO( this->SetUINT32( _ActionToGuid( eAction ), TRUE ) );
    }

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFInputTrustAuthority::UpdateAccess( _In_ MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS *pParams )
{
    return BindAccess( pParams );
}

void Cdm_clearkey_IMFInputTrustAuthority::DisableCurrentDecryptersAndSetInsecureEnvironment()
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);

    GUID guidDisablePrevious;
    GUID guidDisableNew;

    //
    // 1. All decryptors we've created since creation or the last call have our current 'disable' GUID set.
    //    Disable them by setting that GUID to "TRUE" indicating "disabled".
    //
    IF_FAILED_GOTO( this->GetGUID( CLEARKEY_GUID_ATTRIBUTE_DISABLE_GUID, &guidDisablePrevious ) );
    IF_FAILED_GOTO( this->SetUINT32( guidDisablePrevious, TRUE ) );

    //
    // 2. Create a new guid for any decrypters we create after this Reset call.
    //    Start them out NOT *explicitly* disabled using "FALSE".
    //    They will obviously be *implicitly* disabled until they receive their first policy notification.
    //
    IF_FAILED_GOTO( CoCreateGuid( &guidDisableNew ) );
    IF_FAILED_GOTO( this->SetGUID( CLEARKEY_GUID_ATTRIBUTE_DISABLE_GUID, guidDisableNew ) );
    IF_FAILED_GOTO( this->SetUINT32( guidDisableNew, FALSE ) );

    IF_FAILED_GOTO( ResetPEAuthUsingPEAuthHelper( m_spCdm_clearkey_PEAuthHelper.Get() ) );

done:
    //
    // No caller would take action on the return code, so no reason to return one.
    //
    return;
}

STDMETHODIMP Cdm_clearkey_IMFInputTrustAuthority::Reset()
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);

    //
    // https://docs.microsoft.com/en-us/windows/desktop/api/mfidl/nf-mfidl-imfinputtrustauthority-reset
    // When this method is called, the ITA should disable any decrypter that was returned in the IMFInputTrustAuthority::GetDecrypter method.
    //
    this->DisableCurrentDecryptersAndSetInsecureEnvironment();

    //
    // We must do this *after* the above cleanup method to ensure disable occurs
    //
    IF_FALSE_GOTO( !m_fShutdown, MF_E_SHUTDOWN );

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFInputTrustAuthority::GetShutdownStatus( _Out_ MFSHUTDOWN_STATUS *pStatus )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    if( m_fShutdown )
    {
        *pStatus = MFSHUTDOWN_COMPLETED;
        return S_OK;
    }
    else
    {
        return MF_E_INVALIDREQUEST;
    }
}

STDMETHODIMP Cdm_clearkey_IMFInputTrustAuthority::Shutdown()
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    this->DisableCurrentDecryptersAndSetInsecureEnvironment();
    if( m_spIMFAttributes != nullptr )
    {
        (void)m_spIMFAttributes->SetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_TI_TO_ITA, nullptr );
    }
    m_fShutdown = TRUE;
    return S_OK;
}

Cdm_clearkey_IMFOutputPolicy::Cdm_clearkey_IMFOutputPolicy()
{
}

Cdm_clearkey_IMFOutputPolicy::~Cdm_clearkey_IMFOutputPolicy()
{
}

HRESULT Cdm_clearkey_IMFOutputPolicy::RuntimeClassInitialize( _In_ IMFAttributes *pIMFAttributesFromITA, _In_ MFPOLICYMANAGER_ACTION eAction, _In_ REFGUID guidKid, _In_ REFGUID guidLid )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    ComPtr<IMFAttributes> spIMFAttributes;

    IF_FALSE_GOTO( _IsSupportedAction( eAction ), MF_E_ITA_UNSUPPORTED_ACTION );
    IF_FALSE_GOTO( ( guidKid == GUID_NULL ) == ( guidLid == GUID_NULL ), MF_E_UNEXPECTED );     // Should be impossible!

    IF_FAILED_GOTO( MFCreateAttributes( &spIMFAttributes, 1 ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_ITA_TO_MFOP, pIMFAttributesFromITA ) );
    m_guidKid = guidKid;
    m_guidLid = guidLid;
    m_eAction = eAction;

    m_spIMFAttributes = spIMFAttributes;

done:
    return hr;
}

static void _GetOutputInformation(
    _In_  DWORD  dwAttributes,
    _Out_ BOOL  *pfVideo,
    _Out_ BOOL  *pfDigital,
    _Out_ BOOL  *pfCompressed,
    _Out_ BOOL  *pfBus,
    _Out_ BOOL  *pfSoftwareConnector )
{
    *pfVideo = !!( MFOUTPUTATTRIBUTE_VIDEO & ( dwAttributes ) );
    *pfDigital = !!( MFOUTPUTATTRIBUTE_DIGITAL & ( dwAttributes ) );
    *pfCompressed = !!( MFOUTPUTATTRIBUTE_COMPRESSED & ( dwAttributes ) );
    *pfBus = !!( MFOUTPUTATTRIBUTE_BUS & ( dwAttributes ) ) || !!( MFOUTPUTATTRIBUTE_BUSIMPLEMENTATION & ( dwAttributes ) ) || !!( MFOUTPUTATTRIBUTE_NONSTANDARDIMPLEMENTATION & ( dwAttributes ) );
    *pfSoftwareConnector = !!( MFOUTPUTATTRIBUTE_SOFTWARE & ( dwAttributes ) );
}

static BOOL _IsGuidInList(
    _In_                        REFGUID  guidToFind,
    _In_count_( cGuids )  const GUID    *rgGuids,
    _In_                        DWORD    cGuids )
{
    BOOL fGuidInList = FALSE;

    if( rgGuids != nullptr )
    {
        DWORD iGuid = 0;

        while( ( iGuid < cGuids ) && ( !fGuidInList ) )
        {
            if( IsEqualGUID( guidToFind, rgGuids[ iGuid ] ) )
            {
                fGuidInList = TRUE;
            }
            iGuid++;
        }
    }

    return fGuidInList;
}

static BOOL _IsKnownAnalogVideoTVConnector( _In_ REFGUID guidOutputType )
{
    return IsEqualGUID( MFCONNECTOR_SVIDEO, guidOutputType )
        || IsEqualGUID( MFCONNECTOR_COMPOSITE, guidOutputType )
        || IsEqualGUID( MFCONNECTOR_COMPONENT, guidOutputType );
}

STDMETHODIMP Cdm_clearkey_IMFOutputPolicy::GenerateRequiredSchemas(
    _In_                                      DWORD           dwAttributes,
    _In_                                      GUID            guidOutputSubType,
    _In_count_( cProtectionSchemasSupported ) GUID           *rgGuidProtectionSchemasSupported,
    _In_                                      DWORD           cProtectionSchemasSupported,
    _COM_Outptr_                              IMFCollection **ppRequiredProtectionSchemas )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    GUID     guidKid            = GUID_NULL;
    GUID     guidLid            = GUID_NULL;
    BOOL     fVideo             = FALSE;
    BOOL     fDigital           = FALSE;
    BOOL     fCompressed        = FALSE;
    BOOL     fBus               = FALSE;
    BOOL     fSoftwareConnector = FALSE;
    BOOL     fUnknownConnector  = IsEqualGUID( MFCONNECTOR_UNKNOWN, guidOutputSubType );
    GUID     guidOriginatorID   = GUID_NULL;
    ComPtr<IMFAttributes>   spIMFAttributesITA;
    ComPtr<IMFAttributes>   spIMFAttributesTI;
    ComPtr<IMFAttributes>   spIMFAttributesCDM;
    ComPtr<IMFCollection>   spIMFCollection;
    ComPtr<IMFOutputSchema> spIMFOutputSchema;
    BYTE rgbKeyData[ KEY_DATA_TOTAL_SIZE ] = { 0 };
    CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS oPolicy = { 0 };

    //
    // Note: Throughout this function, the word "enforce" is typically shorthand for "require the OTA (output trust authority) to enforce".
    //

    //
    // MUST be first
    //
    IF_FAILED_GOTO( MFCreateCollection( &spIMFCollection ) );

    if( m_guidKid == GUID_NULL )
    {
        //
        // Without a Kid, we don't know which key we are using, so we don't know which policy to use.
        //
        // The content might be clear or a license might be acquired out-of-band.
        //
        // In any case, GenerateRequiredSchemas should just succeed without yielding any policy.
        //
        // If a license with policy that we need to enforce is encountered later,
        // we will add its policy to m_rgPendingPolicyChangeObjects which will
        // then be sent downstream via an MEPolicyChanged event.
        //
        goto done;
    }

    //
    // Obtain the key data for our kid + lid
    //
    IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_ITA_TO_MFOP, IID_PPV_ARGS( &spIMFAttributesITA ) ) );
    IF_FAILED_GOTO( spIMFAttributesITA->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_TI_TO_ITA, IID_PPV_ARGS( &spIMFAttributesTI ) ) );
    IF_FAILED_GOTO( spIMFAttributesTI->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_TI, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );

    IF_FAILED_GOTO( GetKeyDataForLicense( spIMFAttributesCDM.Get(), CLEARKEY_INVALID_SESSION_ID, m_guidKid, m_guidLid, rgbKeyData ) );
    GetKeyData( rgbKeyData, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &oPolicy );

    _GetOutputInformation( dwAttributes, &fVideo, &fDigital, &fCompressed, &fBus, &fSoftwareConnector );
    if( !fDigital && fCompressed )
    {
        // Compressed analog output (video or audio) is nonsense - should never happen in practice
        IF_FAILED_GOTO( MF_E_POLICY_UNSUPPORTED );
    }

    if( m_eAction == PEACTION_NO )
    {
        if( !IsEqualGUID( MFCONNECTOR_PCI, guidOutputSubType )
         && !IsEqualGUID( MFCONNECTOR_PCIX, guidOutputSubType )
         && !IsEqualGUID( MFCONNECTOR_PCI_Express, guidOutputSubType )
         && !IsEqualGUID( MFCONNECTOR_AGP, guidOutputSubType ) )
        {
            //
            // These are the only valid output sub-types for "no" action.
            //
            IF_FAILED_GOTO( MF_E_POLICY_UNSUPPORTED );
        }

        //
        // Generally speaking, the only case where you should do anything here is if there
        // is policy in the license which you don't *understand*, i.e. you don't even know
        // what it would *mean* to enforce it, in which case you should return
        // MF_E_POLICY_UNSUPPORTED or MF_E_LICENSE_INCORRECT_RIGHTS.
        //
        // This enables "fail-fast" in such cases.
        //
        // Since this implementation has no such policy, there's nothing more to do.
        //
        goto done;
    }
    else if( m_eAction == PEACTION_EXTRACT )
    {
        //
        // Per https://docs.microsoft.com/en-us/windows/desktop/api/mfidl/ne-mfidl-_mfpolicymanager_action
        // "Extract the data from the stream and pass it to the application. For example, acoustic echo cancellation requires this action."
        //
        // This implementation does not support any scenarios that require the extract action.
        //
        IF_FAILED_GOTO( MF_E_POLICY_UNSUPPORTED );
    }
    else if( m_eAction != PEACTION_PLAY )
    {
        //
        // Should be impossible!  (Blocked in RuntimeClassInitialize.)
        //
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    if( fBus )
    {
        //
        // This implementation does not enforce any policy when output is over a bus
        //
        goto done;
    }

    switch( oPolicy.dwDigitalOnly )
    {
    case CLEARKEY_POLICY_ALLOW_ANALOG_OUTPUT:
        // Nothing to do
        break;
    case CLEARKEY_POLICY_BLOCK_ANALOG_OUTPUT:
        IF_FALSE_GOTO( fDigital, MF_E_POLICY_UNSUPPORTED );
        break;
    default:
        IF_FAILED_GOTO( MF_E_LICENSE_INCORRECT_RIGHTS );
    }

    if( fSoftwareConnector )
    {
        if( fUnknownConnector )
        {
            if( _IsGuidInList( MFPROTECTION_CONSTRICTVIDEO, rgGuidProtectionSchemasSupported, cProtectionSchemasSupported ) )
            {
                // Desktop Windows Manager (DWM) with OPM established.  This implementation allows it with no other restrictions to apply.
                goto done;
            }
            else if( !_IsGuidInList( MFPROTECTION_CONSTRICTVIDEO_NOOPM, rgGuidProtectionSchemasSupported, cProtectionSchemasSupported ) )
            {
                // Unknown software output - fail
                IF_FAILED_GOTO( MF_E_POLICY_UNSUPPORTED );
            }

            //
            // Getting here means Desktop Windows Manager (DWM) without OPM established, so we fall-through and apply other restrictions.
            //
        }
        else
        {
            // Software output which is not DWM - we don't recognize it, so fail
            IF_FAILED_GOTO( MF_E_POLICY_UNSUPPORTED );
        }
    }

    if( !fVideo )
    {
        // Nothing further to enforce for audio
        goto done;
    }

    if( fCompressed )
    {
        // Never allow compressed video
        IF_FAILED_GOTO( MF_E_POLICY_UNSUPPORTED );
    }

    //
    // At this point, we know that:
    // 1. We're connected to an external screen (hardware output)
    //    and
    // 2. We are outputting either analog video or uncompressed digital video.
    //
    // So now we enforce HDCP/CGMSA as needed
    //

    if( fUnknownConnector )
    {
        //
        // Can't enforce HDCP or CGMSA over an unknown output
        //
        IF_FALSE_GOTO( !_PolicyRequiresHDCP( oPolicy.dwHDCP ), MF_E_POLICY_UNSUPPORTED );
        IF_FALSE_GOTO( !_PolicyRequiresCGMSA( oPolicy.dwCGMSA ), MF_E_POLICY_UNSUPPORTED );

        //
        // But if don't need to enforce either one, we have nothing further to do for an unknown output
        //
        goto done;
    }

    IF_FAILED_GOTO( this->GetOriginatorID( &guidOriginatorID ) );

    if( fDigital )
    {
        if( oPolicy.dwHDCP != CLEARKEY_POLICY_DO_NOT_ENFORCE_HDCP )
        {
            BOOL fHDCPTypeSupported = FALSE;
            BOOL fHDCPSupported     = FALSE;

            fHDCPSupported = fHDCPTypeSupported = _IsGuidInList( MFPROTECTION_HDCP_WITH_TYPE_ENFORCEMENT, rgGuidProtectionSchemasSupported, cProtectionSchemasSupported );
            if( !fHDCPSupported )
            {
                fHDCPSupported = _IsGuidInList( MFPROTECTION_HDCP, rgGuidProtectionSchemasSupported, cProtectionSchemasSupported );
            }

            switch( oPolicy.dwHDCP )
            {
            case CLEARKEY_POLICY_DO_NOT_ENFORCE_HDCP:
                // Nothing to do
                break;
            case CLEARKEY_POLICY_REQUIRE_HDCP_WITH_ANY_VERSION:
                IF_FALSE_GOTO( fHDCPSupported, MF_E_POLICY_UNSUPPORTED );
                hr = MakeAndInitialize<Cdm_clearkey_IMFOutputSchema, IMFOutputSchema>( &spIMFOutputSchema, guidOriginatorID, fHDCPTypeSupported ? MFPROTECTION_HDCP_WITH_TYPE_ENFORCEMENT : MFPROTECTION_HDCP, 1 /* Any HDCP version */ );
                IF_FAILED_GOTO( hr );
                IF_FAILED_GOTO( spIMFCollection->AddElement( spIMFOutputSchema.Get() ) );
                break;
            case CLEARKEY_POLICY_REQUIRE_HDCP_WITH_TYPE_1:
                IF_FALSE_GOTO( fHDCPTypeSupported, MF_E_POLICY_UNSUPPORTED );
                hr = MakeAndInitialize<Cdm_clearkey_IMFOutputSchema, IMFOutputSchema>( &spIMFOutputSchema, guidOriginatorID, MFPROTECTION_HDCP_WITH_TYPE_ENFORCEMENT, 2 /* HDCP type 1 required */ );
                IF_FAILED_GOTO( hr );
                IF_FAILED_GOTO( spIMFCollection->AddElement( spIMFOutputSchema.Get() ) );
                break;
            default:
                IF_FAILED_GOTO( MF_E_LICENSE_INCORRECT_RIGHTS );
            }
        }
    }
    else
    {
        BOOL fAnalogTVOut      = _IsKnownAnalogVideoTVConnector( guidOutputSubType );
        BOOL fAnalogMonitorOut = IsEqualGUID( MFCONNECTOR_VGA, guidOutputSubType );

        //
        // Only allow known analog video outputs
        //
        IF_FALSE_GOTO( fAnalogTVOut || fAnalogMonitorOut, MF_E_ITA_UNRECOGNIZED_ANALOG_VIDEO_OUTPUT );

        //
        // HDCP is impossible for analog video
        //
        IF_FALSE_GOTO( !_PolicyRequiresHDCP( oPolicy.dwHDCP ), MF_E_POLICY_UNSUPPORTED );

        if( fAnalogMonitorOut )
        {
            //
            // Can't enforce CGMSA for a VGA monitor, so fail if it's required
            //
            IF_FALSE_GOTO( !_PolicyRequiresCGMSA( oPolicy.dwCGMSA ), MF_E_POLICY_UNSUPPORTED );
        }
        else
        {
            //
            // Ok, the output is an analog TV.  We should enforce CGMSA as specified.
            //
            MF_OPM_CGMSA_PROTECTION_LEVEL level = MF_OPM_CGMSA_OFF;
            BOOL fBestEffort = FALSE;
            switch( oPolicy.dwCGMSA )
            {
            case CLEARKEY_POLICY_DO_NOT_ENFORCE_CGMSA:
                // Nothing to do
                break;
            case CLEARKEY_POLICY_BEST_EFFORT_CGMSA_COPY_FREE:
                level = MF_OPM_CGMSA_COPY_FREELY;
                fBestEffort = TRUE;
                break;
            case CLEARKEY_POLICY_BEST_EFFORT_CGMSA_COPY_ONCE:
                level = MF_OPM_CGMSA_COPY_ONE_GENERATION;
                fBestEffort = TRUE;
                break;
            case CLEARKEY_POLICY_BEST_EFFORT_CGMSA_COPY_NEVER:
                level = MF_OPM_CGMSA_COPY_NEVER;
                fBestEffort = TRUE;
                break;
            case CLEARKEY_POLICY_REQUIRE_CGMSA_COPY_FREE:
                level = MF_OPM_CGMSA_COPY_FREELY;
                break;
            case CLEARKEY_POLICY_REQUIRE_CGMSA_COPY_ONCE:
                level = MF_OPM_CGMSA_COPY_ONE_GENERATION;
                break;
            case CLEARKEY_POLICY_REQUIRE_CGMSA_COPY_NEVER:
                level = MF_OPM_CGMSA_COPY_NEVER;
                break;
            default:
                IF_FAILED_GOTO( MF_E_LICENSE_INCORRECT_RIGHTS );
            }

            if( level != MF_OPM_CGMSA_OFF )
            {
                hr = MakeAndInitialize<Cdm_clearkey_IMFOutputSchema, IMFOutputSchema>( &spIMFOutputSchema, guidOriginatorID, MFPROTECTION_CGMSA, level );
                IF_FAILED_GOTO( hr );
                IF_FAILED_GOTO( _SetSchemaAttribute( spIMFOutputSchema.Get(), MFPROTECTIONATTRIBUTE_BEST_EFFORT, fBestEffort ) );
                IF_FAILED_GOTO( spIMFCollection->AddElement( spIMFOutputSchema.Get() ) );
            }
        }
    }

    //
    // We get this far when we enforce all required policy sucessfully.
    //
    goto done;

done:
    if( SUCCEEDED( hr ) )
    {
        //
        // Even if there's nothing to enforce, we should return an empty collection on success.
        // Thus, we set the output parameter after the 'done' label in all success cases.
        //
        *ppRequiredProtectionSchemas = spIMFCollection.Detach();
        if( spIMFAttributesTI != nullptr )
        {
            hr = spIMFAttributesITA->SetBlob( CLEARKEY_GUID_ATTRIBUTE_LAST_ITA_ENFORCED_POLICY, (BYTE*)&oPolicy, sizeof( oPolicy ) );
        }
    }
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFOutputPolicy::GetOriginatorID( _Out_ GUID *pguidOriginatorID )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr = S_OK;
    TRACE_FUNC_HR( hr );
    ComPtr<IMFAttributes> spIMFAttributesITA;

    IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_ITA_TO_MFOP, IID_PPV_ARGS( &spIMFAttributesITA ) ) );
    IF_FAILED_GOTO( spIMFAttributesITA->GetGUID( CLEARKEY_GUID_ATTRIBUTE_ORIGINATOR_ID, pguidOriginatorID ) );

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFOutputPolicy::GetMinimumGRLVersion( _Out_ DWORD *pMinimumGRLVersion )
{
    //
    // The GRL (Global Revocation List) is used to revoke the Protected Environment.
    // This method returns the minimum value required for this list to render content.
    // Since this implementation does not require any specific version of the
    // Global Revocation List, it simply returns zero.
    //
    *pMinimumGRLVersion = 0;
    return S_OK;
}

Cdm_clearkey_IMFTransform::Cdm_clearkey_IMFTransform()
{
}

Cdm_clearkey_IMFTransform::~Cdm_clearkey_IMFTransform()
{
    Disable();

    SetProcessingLoopState( CLEARKEY_MFT_PROCESSING_LOOP_STATE_NOT_WAITING );

    (void)Shutdown();

    if( m_hStateChanged != nullptr )
    {
        CloseHandle( m_hStateChanged );
    }

    if( m_hWaitStateEvent != nullptr )
    {
        CloseHandle( m_hWaitStateEvent );
    }

    if( m_hNotProcessingEvent != nullptr )
    {
        CloseHandle( m_hNotProcessingEvent );
    }
}

HRESULT Cdm_clearkey_IMFTransform::RuntimeClassInitialize( _In_ IMFAttributes *pIMFAttributesFromITA, _In_ IMFProtectedEnvironmentAccess *pCdm_clearkey_PEAuthHelper, _In_ REFGUID guidKid, _In_ REFGUID guidLid )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    PROPVARIANTWRAP propvar;
    GUID guidDisable;
    ComPtr<IMFAttributes> spIMFAttributes;

    INIT_EVENT_GENERATOR;

    IF_FALSE_GOTO( ( guidKid == GUID_NULL ) == ( guidLid == GUID_NULL ), MF_E_UNEXPECTED );     // Should be impossible!

    IF_FAILED_GOTO( MFCreateAttributes( &spIMFAttributes, 8 ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_ITA_TO_MFT, pIMFAttributesFromITA ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUINT32( MF_TRANSFORM_ASYNC, TRUE ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUINT32( MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, TRUE ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUINT32( MF_SA_D3D11_AWARE, TRUE ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUINT32( MFT_END_STREAMING_AWARE, TRUE ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUINT32( MFT_POLICY_SET_AWARE, TRUE ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUINT32( MFT_USING_HARDWARE_DRM, FALSE ) );

    //
    // The 'disable' GUID will change when IMFInputTrustAuthority::Reset is called, so we need to take a copy.
    // Refer to comments regarding CLEARKEY_GUID_ATTRIBUTE_DISABLE_GUID elsewhere in this file.
    //
    IF_FAILED_GOTO( pIMFAttributesFromITA->GetGUID( CLEARKEY_GUID_ATTRIBUTE_DISABLE_GUID, &guidDisable ) );
    IF_FAILED_GOTO( spIMFAttributes->SetGUID( CLEARKEY_GUID_ATTRIBUTE_DISABLE_GUID, guidDisable ) );

    IF_FAILED_GOTO( CoCreateGuid( &m_idMFT ) );
    IF_FAILED_GOTO( m_OutputSampleQueue.Initialize( 1 ) );
    IF_FAILED_GOTO( m_InputSampleQueue.Initialize( 1 ) );
    IF_FAILED_GOTO( m_InputQueue.Initialize( c_dwMaxInputSamples ) );
    IF_FAILED_GOTO( m_OutputDataBufferQueue.Initialize( c_dwMaxOutputSamples ) );
    IF_FAILED_GOTO( m_OutputEventQueue.Initialize( 1 ) );

    IF_FAILED_GOTO( PSCreateMemoryPropertyStore( IID_PPV_ARGS( &m_spIPropertyStore ) ) );
    propvar.vt = VT_BOOL;
    propvar.boolVal = VARIANT_FALSE;
    IF_FAILED_GOTO( m_spIPropertyStore->SetValue( MFPKEY_EXATTRIBUTE_SUPPORTED, propvar ) );
    IF_FAILED_GOTO( propvar.Clear() );

    // FALSE, FALSE: Auto-reset event, starts non-signaled
    m_hStateChanged = CreateEvent( nullptr, FALSE, FALSE, nullptr );
    if( m_hStateChanged == nullptr )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    // TRUE, FALSE: Manual-reset event, starts non-signaled
    m_hWaitStateEvent = CreateEvent( nullptr, TRUE, FALSE, nullptr );
    if( m_hWaitStateEvent == nullptr )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    // TRUE, TRUE: Manual-reset event, starts signaled
    m_hNotProcessingEvent = CreateEvent( nullptr, TRUE, TRUE, nullptr );
    if( m_hNotProcessingEvent == nullptr )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    m_spCdm_clearkey_PEAuthHelper = pCdm_clearkey_PEAuthHelper;
    m_spIMFAttributes = spIMFAttributes;
    m_guidKid = guidKid;
    m_guidLid = guidLid;

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetStreamLimits( _Out_ DWORD *pdwInputMinimum, _Out_ DWORD *pdwInputMaximum, _Out_ DWORD *pdwOutputMinimum, _Out_ DWORD *pdwOutputMaximum )
{
    if( pdwInputMinimum )
    {
        *pdwInputMinimum = 1;
    }
    if( pdwInputMaximum )
    {
        *pdwInputMaximum = 1;
    }
    if( pdwOutputMinimum )
    {
        *pdwOutputMinimum = 1;
    }
    if( pdwOutputMaximum )
    {
        *pdwOutputMaximum = 1;
    }

    return S_OK;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetStreamCount( _Out_ DWORD *pcInputStreams, _Out_ DWORD *pcOutputStreams )
{
    if( pcInputStreams )
    {
        *pcInputStreams = 1;
    }
    if( pcOutputStreams )
    {
        *pcOutputStreams = 1;
    }

    return S_OK;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetStreamIDs( _In_ DWORD dwInputIDArraySize, _Out_ DWORD *pdwInputIDs, _In_ DWORD dwOutputIDArraySize, _Out_ DWORD *pdwOutputIDs )
{
    //
    // Optional API
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetInputStreamInfo( _In_ DWORD dwInputStreamIndex, _Out_ MFT_INPUT_STREAM_INFO *pStreamInfo )
{
    if( dwInputStreamIndex != 0 )
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    pStreamInfo->dwFlags = c_dwInputStreamFlags;
    pStreamInfo->hnsMaxLatency = c_hnsInputMaxLatency;
    pStreamInfo->cbSize = c_dwInputSampleSize;
    pStreamInfo->cbAlignment = c_dwInputSampleAlignment;
    pStreamInfo->cbMaxLookahead = c_dwInputStreamLookahead;

    return S_OK;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetOutputStreamInfo( _In_ DWORD dwOutputStreamIndex, _Out_ MFT_OUTPUT_STREAM_INFO *pStreamInfo )
{
    if( dwOutputStreamIndex != 0 )
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    pStreamInfo->dwFlags = MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
    pStreamInfo->cbSize = 0;
    pStreamInfo->cbAlignment = 0;

    return S_OK;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetAttributes( _COM_Outptr_ IMFAttributes **ppAttributes )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    HRESULT hr = S_OK;

    IF_NULL_GOTO( ppAttributes, E_INVALIDARG );

    *ppAttributes = m_spIMFAttributes.Get();
    ( *ppAttributes )->AddRef();

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetInputStreamAttributes( _In_ DWORD dwInputStreamID, _COM_Outptr_ IMFAttributes **ppAttributes )
{
    //
    // Optional API
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetOutputStreamAttributes( _In_ DWORD dwOutputStreamID, _COM_Outptr_ IMFAttributes **ppAttributes )
{
    //
    // Optional API
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::DeleteInputStream( _In_ DWORD dwStreamID )
{
    //
    // Optional API
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::AddInputStreams( _In_ DWORD cStreams, _In_count_( cStreams ) DWORD *rgdwStreamIDs )
{
    //
    // Optional API
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetInputAvailableType( _In_ DWORD dwInputStreamIndex, _In_ DWORD dwTypeIndex, _COM_Outptr_ IMFMediaType **ppType )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    HRESULT hr = S_OK;

    IF_FALSE_GOTO( dwInputStreamIndex == 0, MF_E_INVALIDSTREAMNUMBER );

    //
    // If the available types haven't been filled in by this point, we
    // assume that we're not offering any types, and thus the default type
    // from Function Discovery will be used.
    //
    IF_FALSE_GOTO( m_spInputMediaType != nullptr, E_NOTIMPL );
    IF_FALSE_GOTO( dwTypeIndex == 0, MF_E_NO_MORE_TYPES );

    *ppType = m_spInputMediaType.Get();
    ( *ppType )->AddRef();

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetOutputAvailableType( _In_ DWORD dwOutputStreamIndex, _In_ DWORD dwTypeIndex, _COM_Outptr_ IMFMediaType **ppType )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    HRESULT hr = S_OK;

    IF_FALSE_GOTO( dwOutputStreamIndex == 0, MF_E_INVALIDSTREAMNUMBER );
    IF_FALSE_GOTO( m_spAvailableOutputMediaType != nullptr, MF_E_TRANSFORM_TYPE_NOT_SET );
    IF_FALSE_GOTO( dwTypeIndex == 0, MF_E_NO_MORE_TYPES );

    *ppType = m_spAvailableOutputMediaType.Get();
    ( *ppType )->AddRef();

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::SetInputType( _In_ DWORD dwInputStreamIndex, _In_ IMFMediaType *pType, _In_ DWORD dwFlags )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    CLEARKEY_MFT_AUTOLOCK_INPUT;
    HRESULT hr = S_OK;

    IF_FALSE_GOTO( dwInputStreamIndex == 0, MF_E_INVALIDSTREAMNUMBER );

    if( ( dwFlags & MFT_SET_TYPE_TEST_ONLY ) )
    {
        IF_FAILED_GOTO( this->SetInputTypeLockIsHeld( stateLock, dwInputStreamIndex, pType, dwFlags ) );
    }
    else if( ( this->GetState() == CLEARKEY_MFT_STATE_STOPPED ) )
    {
        //
        // If we are not streaming, we can set the media type directly on the underlying MFT
        //
        (void)this->OnFlush();
        IF_FAILED_GOTO( this->SetInputTypeLockIsHeld( stateLock, dwInputStreamIndex, pType, dwFlags ) );
    }
    else if( pType != nullptr )
    {
        //
        // We must queue up the input media type change, it will be processed synchronously with
        //      the samples in the input queue (MFT may also need to be drained first)
        //

        IF_FAILED_GOTO( this->QueueInputSampleOrEvent( pType ) );

        //
        // Trigger the processing loop which will propagate the format change
        //
        IF_FAILED_GOTO( this->SignalStateChanged() );

        // For the purposes of the external caller, the input type has already changed
        m_spInputMediaType = pType;
    }
    else
    {
        IF_FAILED_GOTO( this->SetInputTypeLockIsHeld( stateLock, dwInputStreamIndex, pType, dwFlags ) );
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::SetInputTypeLockIsHeld( _In_ SyncLock2& stateLock, _In_ DWORD dwInputStreamIndex, _In_ IMFMediaType *pType, _In_ DWORD dwFlags )
{
    HRESULT hr = S_OK;

    IF_FALSE_GOTO( dwInputStreamIndex == 0, MF_E_INVALIDSTREAMNUMBER );

    if( !( dwFlags & MFT_SET_TYPE_TEST_ONLY ) )
    {
        m_spInputMediaType = pType;

        IF_FAILED_GOTO( this->ProcessMessageStateLockHeld( stateLock, MFT_MESSAGE_NOTIFY_END_STREAMING, 0 ) );
        IF_FAILED_GOTO( this->OnInputTypeChanged() );
    }

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::SetOutputType( _In_ DWORD dwOutputStreamIndex, _In_ IMFMediaType *pType, _In_ DWORD dwFlags )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    HRESULT hr = S_OK;

    IF_FALSE_GOTO( dwOutputStreamIndex == 0, MF_E_INVALIDSTREAMNUMBER );

    IF_FAILED_GOTO( this->SetOutputTypeLockIsHeld( stateLock, dwOutputStreamIndex, pType, dwFlags ) );

    if( ( dwFlags & MFT_SET_TYPE_TEST_ONLY ) != 0 )
    {
        // "test only, we're done
        goto done;
    }

    {
        CLEARKEY_MFT_AUTOLOCK_OUTPUT;

        if( !this->IsOutputDataBufferQueueEmpty() )
        {
            //
            // If the output format renegotiation was triggered by us, there could be samples or pending format changes in the output
            //      queue at this point. We need to flush the samples only as they will not be valid after the format change.
            // This can happen if the caller changed our output type without flushing\draining first. This isn't ideal but is possible.
            //

            this->FlushOutputSampleQueue( stateLock, outputLock, c_fYesKeepEvents );
        }

        if( m_fOutputFormatChange )
        {
            m_fOutputFormatChange = FALSE;

            //
            // Trigger the processing loop
            //
            (void)this->SignalStateChanged();
        }
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::SetOutputTypeLockIsHeld( _In_ SyncLock2& stateLock, _In_ DWORD dwOutputStreamIndex, _In_ IMFMediaType *pType, _In_ DWORD dwFlags )
{
    HRESULT hr = S_OK;
    DWORD dwIsEqualFlags = 0;

    IF_FALSE_GOTO( dwOutputStreamIndex == 0, MF_E_INVALIDSTREAMNUMBER );
    IF_FALSE_GOTO( m_spAvailableOutputMediaType != nullptr, MF_E_TRANSFORM_TYPE_NOT_SET );

    if( !pType )
    {
        m_spOutputMediaType = nullptr;
        goto done;
    }

    IF_FAILED_GOTO( pType->IsEqual( m_spAvailableOutputMediaType.Get(), &dwIsEqualFlags ) );
    IF_FALSE_GOTO( dwIsEqualFlags & MF_MEDIATYPE_EQUAL_MAJOR_TYPES, MF_E_INVALIDMEDIATYPE );
    IF_FALSE_GOTO( dwIsEqualFlags & MF_MEDIATYPE_EQUAL_FORMAT_TYPES, MF_E_INVALIDMEDIATYPE );

    if( !( dwFlags & MFT_SET_TYPE_TEST_ONLY ) )
    {
        m_spOutputMediaType = pType;

        //
        // if the type has changed, we should end streaming if it's begun
        //
        IF_FAILED_GOTO( this->ProcessMessageStateLockHeld( stateLock, MFT_MESSAGE_NOTIFY_END_STREAMING, 0 ) );
    }

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetInputCurrentType( _In_ DWORD dwInputStreamIndex, _COM_Outptr_ IMFMediaType **ppType )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    CLEARKEY_MFT_AUTOLOCK_INPUT;

    if( dwInputStreamIndex != 0 )
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }
    else if( m_spInputMediaType != nullptr )
    {
        *ppType = m_spInputMediaType.Get();
        ( *ppType )->AddRef();
        return S_OK;
    }
    else
    {
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }
}

HRESULT Cdm_clearkey_IMFTransform::OnFlush( void )
{
    // MF requested that we flush our sample queue.

    while( !m_OutputSampleQueue.IsEmpty() )
    {
        ComPtr<IMFSample> spSample;
        (void)m_OutputSampleQueue.RemoveHead( &spSample );
    }

    while( !m_InputSampleQueue.IsEmpty() )
    {
        ComPtr<IMFSample> spSample;
        (void)m_InputSampleQueue.RemoveHead( &spSample );
    }

    m_fInputSampleQueueDraining = FALSE;

    return S_OK;
}

HRESULT Cdm_clearkey_IMFTransform::QueueInputSampleOrEvent( _In_ IUnknown *pSampleOrEvent )
{
    HRESULT hr = S_OK;

    IF_NULL_GOTO( m_InputQueue.AddTail( pSampleOrEvent ), E_OUTOFMEMORY );
    if( pSampleOrEvent )
    {
        pSampleOrEvent->AddRef();  // queue now holds a reference
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::SignalStateChanged()
{
    HRESULT hr = S_OK;

    //
    // Trigger the processing loop
    //
    if( m_hStateChanged && SetEvent( m_hStateChanged ) == 0 )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::ProcessMessageStateLockHeld( _In_ SyncLock2& stateLock, _In_ MFT_MESSAGE_TYPE eMessage, _In_ ULONG_PTR ulParam )
{
    HRESULT hr = S_OK;

    if( eMessage == MFT_MESSAGE_NOTIFY_EVENT )
    {
        SetEvent( m_hWaitStateEvent );
    }

    if( this->GetState() == CLEARKEY_MFT_STATE_SHUTDOWN )
    {
        IF_FAILED_GOTO( MF_E_SHUTDOWN );
    }

    switch( eMessage )
    {
    case MFT_MESSAGE_COMMAND_DRAIN:
    {
        if( this->GetDrainState() == CLEARKEY_MFT_DRAIN_STATE_NONE )
        {
            // Switch into draining state but DO NOT forward message to MFT
            // We will tell MFT to drain once we feed it all the samples in our queue
            SetDrainState( CLEARKEY_MFT_DRAIN_STATE_PREDRAIN );

            m_dwNeedInputCount = 0;
        }

        //
        // Trigger the processing loop
        //
        IF_FAILED_GOTO( this->SignalStateChanged() );

        break;
    }

    case MFT_MESSAGE_COMMAND_MARKER:
    {
        CLEARKEY_MFT_AUTOLOCK_INPUT;

        //
        // Queue the marker event serially with input samples
        // Once all samples ahead of the marker are processed, signal the event
        //

        ComPtr<IMFMediaEvent> spEvent;
        IF_FAILED_GOTO( MFCreateMediaEvent( METransformMarker, GUID_NULL, S_OK, nullptr, &spEvent ) );
        IF_FAILED_GOTO( spEvent->SetUINT64( MF_EVENT_MFT_CONTEXT, ulParam ) );

        //
        // Add the event to the input queue, it will be processed serially with the samples
        //
        IF_FAILED_GOTO( this->QueueInputSampleOrEvent( spEvent.Get() ) );
        spEvent = nullptr;

        // Trigger the processing loop
        IF_FAILED_GOTO( this->SignalStateChanged() );

        break;
    }

    case MFT_MESSAGE_COMMAND_FLUSH:
        IF_FAILED_GOTO( this->FlushInternal( stateLock ) );
        break;

    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
    {
        IF_FAILED_GOTO( this->StartInternal( c_fYesStartOfStream ) );

        if( m_fWaitingForStartOfStream )
        {
            //
            // We may have been flushed\drained but not stopped. In these caes, we should trigger the processing loop
            // which will properly reset the state (e.g. request sample when in low-latency\thumbnail mode).
            //
            IF_FAILED_GOTO( this->SignalStateChanged() );

            m_fWaitingForStartOfStream = FALSE;
        }

        //
        // Request input samples
        //
        if( !this->IsWaitingForPolicyNotification() )
        {
            IF_FAILED_GOTO( this->RequestInput() );
        }

        break;
    }

    case MFT_MESSAGE_NOTIFY_END_STREAMING:
        break;

    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
        IF_FAILED_GOTO( this->StopInternal() );
        m_fEndOfStreamReached = TRUE;
        break;

    case MFT_MESSAGE_NOTIFY_RELEASE_RESOURCES:
    {
        CLEARKEY_MFT_AUTOLOCK_OUTPUT;
        // When we are told to release resources we need to flush all output samples
        this->FlushOutputSampleQueue( stateLock, outputLock, c_fDoNotKeepEvents );
    }
        IF_FAILED_GOTO( this->OnFlush() );
    break;

    case MFT_MESSAGE_NOTIFY_REACQUIRE_RESOURCES:
        IF_FAILED_GOTO( this->GetEnabledState() );
        IF_FAILED_GOTO( this->StartInternal( c_fNotStartOfStream ) );
        break;

    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        //
        // Reset the error state
        //
        m_hrError = S_OK;
        IF_FAILED_GOTO( this->StartInternal( c_fNotStartOfStream ) );
        break;

    case MFT_MESSAGE_SET_D3D_MANAGER:
        IF_FAILED_GOTO( this->OnSetD3DManager( reinterpret_cast<IUnknown*>( ulParam ) ) );
        break;

    case MFT_MESSAGE_NOTIFY_EVENT:
        IF_FAILED_GOTO( this->OnMediaEventMessage( ulParam ) );
        break;

    case MFT_MESSAGE_DROP_SAMPLES:                      // fallthrough
    case MFT_MESSAGE_COMMAND_TICK:                      // fallthrough
    case MFT_MESSAGE_COMMAND_SET_OUTPUT_STREAM_STATE:   // fallthrough
    case MFT_MESSAGE_COMMAND_FLUSH_OUTPUT_STREAM:       // fallthrough
    default:
        // No-op.  An MFT is allowed to ignore message types it doesn't care about and simply return S_OK.
        break;
    }

    ResetEvent( m_hWaitStateEvent );

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetOutputCurrentType( _In_ DWORD dwOutputStreamIndex, _COM_Outptr_ IMFMediaType **ppType )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;

    if( dwOutputStreamIndex != 0 )
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }
    else if( m_spOutputMediaType != nullptr )
    {
        *ppType = m_spOutputMediaType.Get();
        ( *ppType )->AddRef();
        return S_OK;
    }
    else
    {
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetInputStatus( _In_ DWORD dwInputStreamIndex, _Out_ DWORD *pdwFlags )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    CLEARKEY_MFT_AUTOLOCK_INPUT;
    HRESULT hr = S_OK;

    IF_NULL_GOTO( pdwFlags, E_INVALIDARG );

    IF_FAILED_GOTO( this->CheckState() );

    IF_FALSE_GOTO( dwInputStreamIndex == 0, MF_E_INVALIDSTREAMNUMBER );
    IF_FALSE_GOTO( m_spInputMediaType != nullptr, MF_E_TRANSFORM_TYPE_NOT_SET );

    *pdwFlags = 0;

    //
    // Don't need input if we're draining
    //
    if( !this->IsDraining() )
    {
        //
        // Check whether we can accept more samples
        //
        if( ( this->GetSampleCount() + m_dwNeedInputCount ) < c_dwMaxInputSamples )
        {
            *pdwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;
        }
    }

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetOutputStatus( _Out_ DWORD *pdwFlags )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    CLEARKEY_MFT_AUTOLOCK_OUTPUT;
    HRESULT hr = S_OK;

    IF_NULL_GOTO( pdwFlags, E_INVALIDARG );

    IF_FALSE_GOTO( m_spInputMediaType != nullptr, MF_E_TRANSFORM_TYPE_NOT_SET );
    IF_FALSE_GOTO( m_spOutputMediaType != nullptr, MF_E_TRANSFORM_TYPE_NOT_SET );

    IF_FAILED_GOTO( this->CheckState() );

    *pdwFlags = 0;

    //
    // Check whether we have any output to provide
    // NOTE: if the only output we have is pending (m_cPendingOutputs > 0),
    //       then we'll (only) block when that output is requested.
    //
    if( !this->IsOutputDataBufferQueueEmpty() )
    {
        *pdwFlags |= MFT_OUTPUT_STATUS_SAMPLE_READY;
    }

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::SetOutputBounds( _In_ LONGLONG hnsLowerBound, _In_ LONGLONG hnsUpperBound )
{
    //
    // Optional API
    //
    return E_NOTIMPL;
}

static void _CopySampleTimeAndDuration( _In_ IMFSample *pInputSample, _Inout_ IMFSample *pOutputSample )
{
    {
        HRESULT  hr2     = S_OK;
        LONGLONG hnsTime = 0;

        hr2 = pInputSample->GetSampleTime( &hnsTime );
        if( SUCCEEDED( hr2 ) )
        {
            (void)pOutputSample->SetSampleTime( hnsTime );
        }
    }

    {
        HRESULT  hr2     = S_OK;
        LONGLONG hnsTime = 0;

        hr2 = pInputSample->GetSampleDuration( &hnsTime );
        if( SUCCEEDED( hr2 ) )
        {
            (void)pOutputSample->SetSampleDuration( hnsTime );
        }
    }
}

BOOL Cdm_clearkey_IMFTransform::IsWaitingForPolicyNotification()
{
    // Cases where we have to wait for policy event
    // -) Enable() hasn't been called and we are in a disable state.
    //      We need to wait for the initial policy set event
    // -) Dynamic policy change triggered when Kid rotates. i.e. KeyRotation
    //      We need to wait for the dynamic policy set event.
    return ( this->GetEnabledState() == MF_E_INVALIDREQUEST ) || ( m_rgdwPolicyIds.GetSize() > 0 );
}

void Cdm_clearkey_IMFTransform::SetEnabledState( _In_ HRESULT hr )
{
    //
    // We start out *implicitly* disabled via initialization of m_hrEnabled to MF_E_INVALIDREQUEST.
    //
    // When we process our first policy notification (in OnMediaEventMessage -> Enable) is when we can first become enabled.
    // If we are ever *explicitly* disabled (neither S_OK nor MF_E_INVALIDREQUEST), whether that's before or after
    // our first policy notification, we can never beome enabled again.
    //
    // In other words:
    //    If input is an error, set our enabled state.  This is permanent.
    //    If we've never been enabled (MF_E_INVALIDREQUEST), set our enabled state.  This enables us until/unless we become disabled.
    //    Otherwise, DON'T set our enabled state - we're permanently disabled already.
    //
    if( FAILED( hr ) || m_hrEnabled == MF_E_INVALIDREQUEST )
    {
        m_hrEnabled = hr;
    }
}

HRESULT Cdm_clearkey_IMFTransform::GetEnabledState()
{
    HRESULT hr        = S_OK;
    UINT32  fDisabled = FALSE;
    GUID    guidDisable;
    ComPtr<IMFAttributes> spIMFAttributesFromITA;

    //
    // If we're already in a disabled state, stop and return it.
    //
    // This could be due to:
    // 1. *implicit* disabling because we haven't received our first policy notification (which will happen via OnMediaEventMessage -> Enable)
    // OR
    // 2. *explicit* disabling, e.g. because IMFInputTrustAuthority::Reset was called.
    //
    IF_FAILED_GOTO( m_hrEnabled );

    //
    // Check whether this decrypter has been *explicitly* disabled since the last time this method was called.
    // First, check whether we disabled ourselves by setting our unique disable-GUID to GUID_NULL.
    // If it's not GUID_NULL, use its unique disable-GUID to query the ITA to see if the ITA has disabled this decrypter.
    //
    // In either case, if this decrypter been *explicitly* disabled, it can never be re-enabled, so set the error code
    // to something OTHER than MF_E_INVALIDREQUEST (to indicate it is not *implicitly* disabled).
    //
    IF_FAILED_GOTO( m_spIMFAttributes->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_ITA_TO_MFT, IID_PPV_ARGS( &spIMFAttributesFromITA ) ) );
    IF_FAILED_GOTO( m_spIMFAttributes->GetGUID( CLEARKEY_GUID_ATTRIBUTE_DISABLE_GUID, &guidDisable ) );
    fDisabled = ( guidDisable == GUID_NULL );
    if( !fDisabled )
    {
        IF_FAILED_GOTO( spIMFAttributesFromITA->GetUINT32( guidDisable, &fDisabled ) );
    }
    IF_FALSE_GOTO( !fDisabled, MF_E_UNRECOVERABLE_ERROR_OCCURRED );    // Note: Must NOT be error code 'MF_E_INVALIDREQUEST'.

done:
    m_hrEnabled = hr;
    return hr;
}

CLEARKEY_MFT_STATE Cdm_clearkey_IMFTransform::GetState()
{
    return m_eState;
}

void Cdm_clearkey_IMFTransform::SetDrainState( _In_ CLEARKEY_MFT_DRAIN_STATE eDrainState )
{
    m_eDrainState = eDrainState;
}

CLEARKEY_MFT_DRAIN_STATE Cdm_clearkey_IMFTransform::GetDrainState()
{
    return m_eDrainState;
}

BOOL Cdm_clearkey_IMFTransform::IsDraining()
{
    return this->GetDrainState() != CLEARKEY_MFT_DRAIN_STATE_NONE;
}

DWORD Cdm_clearkey_IMFTransform::GetSampleCount()
{
    CLEARKEY_MFT_AUTOLOCK_INPUT;
    return m_InputQueue.GetCount();
}

BOOL Cdm_clearkey_IMFTransform::DidInputQueueChange( _In_ const IMFSample *pfExpectedFirstSample )
{
    IMFSample *pFirstSampleWeakRef = nullptr;
    if( !m_InputSampleQueue.GetHead( &pFirstSampleWeakRef ) || pFirstSampleWeakRef != pfExpectedFirstSample )
    {
        // Input queue changed while processing an in-flight sample.  Abort it.
        return TRUE;
    }
    return FALSE;
}

BOOL Cdm_clearkey_IMFTransform::ShouldIgnoreSampleAfterLongRunningOperation( _In_ const IMFSample *pfExpectedFirstSample )
{
    if( this->GetState() == CLEARKEY_MFT_STATE_SHUTDOWN )
    {
        // Samples should not be propagated when shutdown
        // Entered shudown state while processing an in-flight sample.  Abort it.
        return TRUE;
    }
    if( this->GetState() == CLEARKEY_MFT_STATE_STOPPED )
    {
        // Samples should not be propagated when stopped
        // Entered stopped state while processing an in-flight sample.  Abort it.
        return TRUE;
    }
    if( m_fEndOfStreamReached )
    {
        // Samples should not be propagated after end of stream
        // Reached end of stream while processing an in-flight sample.  Abort it.
        return TRUE;
    }
    if( this->DidInputQueueChange( pfExpectedFirstSample ) )
    {
        return TRUE;
    }
    return FALSE;
}

static HRESULT _GetSampleEncryptionData( _In_ IMFSample *pSample, _Out_ CMFSampleData &oMFSampleData, _Inout_ GUID *pguidKidCurrent, _Inout_ GUID *pguidLidCurrent, _Out_ BOOL *pfEncrypted )
{
    HRESULT  hr                      = S_OK;
    BOOL     fEncrypted              = FALSE;
    UINT32   cbInputSampleID         = 0;
    UINT32   cbPacketOffsets         = 0;
    UINT32   cbSubSampleMapping      = 0;
    BYTE    *pbSubSampleMapping      = nullptr;
    UINT32   cbSubSampleMappingSplit = 0;
    BYTE    *pbSubSampleMappingSplit = nullptr;
    UINT32   ui32Attribute           = 0;
    BOOL     fHasEncryptionScheme    = FALSE;
    GUID     guidKid                 = GUID_NULL;

    IF_NULL_GOTO( pSample, E_INVALIDARG );

    //
    // NOTE: If any of the calls below succeed ( for MFSampleExtension_Encryption_SampleID,
    // MFSampleExtension_Content_KeyID, MFSampleExtension_Encryption_SubSample_Mapping,
    // or MFSampleExtension_Encryption_SampleOffsets ), we will IGNORE MFSampleExtension_PacketCrossOffsets,
    // even if it is available
    //

    //
    // Determine packet cross offsets
    //
    hr = pSample->GetBlobSize( MFSampleExtension_PacketCrossOffsets, &cbPacketOffsets );
    if( SUCCEEDED( hr ) )
    {
        fEncrypted = TRUE;

        //
        // The reason for adding 1 to the number of elements and setting the first element of the array of offsets
        // to 0 is that the parser/source creates the array of offsets assuming that it is not necessary to specify
        // the first range as 0-X (where 0 is the first element, and X the second), but rather it is sufficient to
        // set the first element to X, and it is then implied that the range is 0-X. However, the decrypt
        // function expects that the elements in the array will specify ranges, i.e. X as the first element would imply
        // that the first portion of the sample to decrypt starts at X. Therefore, we need to add a 0 at the beginning
        // of the array in order to translate the array from the parser to the decryptor
        //
        IF_FAILED_GOTO( oMFSampleData.m_propvarOffsets.Clear() );
        IF_NULL_GOTO( oMFSampleData.m_propvarOffsets.caul.pElems = (ULONG*)CoTaskMemAlloc( sizeof( DWORD ) + cbPacketOffsets ), E_OUTOFMEMORY );
        oMFSampleData.m_propvarOffsets.caul.cElems = 1 + cbPacketOffsets / sizeof( DWORD );
        oMFSampleData.m_propvarOffsets.vt = VT_VECTOR | VT_UI4;
        oMFSampleData.m_propvarOffsets.caul.pElems[ 0 ] = 0;
        IF_FAILED_GOTO( pSample->GetBlob(
            MFSampleExtension_PacketCrossOffsets,
            reinterpret_cast<UINT8*>( &( ( oMFSampleData.m_propvarOffsets ).caul.pElems[ 1 ] ) ),
            cbPacketOffsets,
            &cbPacketOffsets ) );
    }
    else
    {
        if( MF_E_ATTRIBUTENOTFOUND == hr )
        {
            hr = S_OK;
        }
        IF_FAILED_GOTO( hr );
    }

    //
    // Determine sample ID, i.e. AES128 IV, which may be 8 or 16 bytes in size
    //
    hr = pSample->GetBlobSize( MFSampleExtension_Encryption_SampleID, &cbInputSampleID );
    if( SUCCEEDED( hr ) )
    {
        fEncrypted = TRUE;

        // The sample ID from the content is actually the IV, and only 8 byte and 16 byte IVs are supported
        IF_FALSE_GOTO( sizeof( QWORD ) == cbInputSampleID || 2 * sizeof( QWORD ) == cbInputSampleID, MF_E_UNEXPECTED );
        IF_FAILED_GOTO( pSample->GetBlob( MFSampleExtension_Encryption_SampleID, oMFSampleData.m_rgbIV, cbInputSampleID, &cbInputSampleID ) );
        IF_FALSE_GOTO( sizeof( QWORD ) == cbInputSampleID || 2 * sizeof( QWORD ) == cbInputSampleID, MF_E_UNEXPECTED );
    }
    else
    {
        if( MF_E_ATTRIBUTENOTFOUND == hr )
        {
            hr = S_OK;
        }
        IF_FAILED_GOTO( hr );
    }

    //
    // Determine Kid
    //
    hr = pSample->GetGUID( MFSampleExtension_Content_KeyID, &guidKid );
    if( SUCCEEDED( hr ) )
    {
        fEncrypted = TRUE;
    }
    else
    {
        if( MF_E_ATTRIBUTENOTFOUND == hr )
        {
            hr = S_OK;
        }
        IF_FAILED_GOTO( hr );
    }

    //
    // NOTE: If we successfully retrieve MFSampleExtension_Encryption_SubSample_Mapping and we've
    // successfully retrieved MFSampleExtension_Encryption_SubSampleMappingSplit,
    // we will end up IGNORING MFSampleExtension_Encryption_SubSample_Mapping
    //

    //
    // Determine subsample mapping
    //
    hr = pSample->GetAllocatedBlob( MFSampleExtension_Encryption_SubSample_Mapping, &pbSubSampleMapping, &cbSubSampleMapping );
    if( SUCCEEDED( hr ) )
    {
        fEncrypted = TRUE;

        IF_FALSE_GOTO( ( 0 < cbSubSampleMapping ) && ( 0 == ( cbSubSampleMapping % ( 2 * sizeof( DWORD ) ) ) ), MF_E_UNEXPECTED );

        IF_FAILED_GOTO( oMFSampleData.m_pvSubSampleMapping.Clear() );
        IF_NULL_GOTO( oMFSampleData.m_pvSubSampleMapping.caul.pElems = (ULONG*)CoTaskMemAlloc( cbSubSampleMapping ), E_OUTOFMEMORY );
        oMFSampleData.m_pvSubSampleMapping.caul.cElems = cbSubSampleMapping / sizeof( DWORD );
        oMFSampleData.m_pvSubSampleMapping.vt = VT_VECTOR | VT_UI4;

        memcpy( oMFSampleData.m_pvSubSampleMapping.caul.pElems, pbSubSampleMapping, cbSubSampleMapping );
    }
    else
    {
        if( MF_E_ATTRIBUTENOTFOUND == hr )
        {
            hr = S_OK;
        }
        IF_FAILED_GOTO( hr );
    }

    //
    // Determine subsample mapping split
    //
    hr = pSample->GetAllocatedBlob( MFSampleExtension_Encryption_SubSampleMappingSplit, &pbSubSampleMappingSplit, &cbSubSampleMappingSplit );
    if( SUCCEEDED( hr ) )
    {
        oMFSampleData.m_fSubSampleMappingSplit = TRUE;
        fEncrypted = TRUE;

        IF_FALSE_GOTO( ( 0 < cbSubSampleMappingSplit ) && ( 0 == ( cbSubSampleMappingSplit % ( 2 * sizeof( DWORD ) ) ) ), MF_E_UNEXPECTED );

        IF_FAILED_GOTO( oMFSampleData.m_pvSubSampleMapping.Clear() );
        IF_NULL_GOTO( oMFSampleData.m_pvSubSampleMapping.caul.pElems = (ULONG*)CoTaskMemAlloc( cbSubSampleMappingSplit ), E_OUTOFMEMORY );
        oMFSampleData.m_pvSubSampleMapping.caul.cElems = cbSubSampleMappingSplit / sizeof( DWORD );
        oMFSampleData.m_pvSubSampleMapping.vt = VT_VECTOR | VT_UI4;

        memcpy( oMFSampleData.m_pvSubSampleMapping.caul.pElems, pbSubSampleMappingSplit, cbSubSampleMappingSplit );
    }
    else
    {
        if( MF_E_ATTRIBUTENOTFOUND == hr )
        {
            hr = S_OK;
        }
        IF_FAILED_GOTO( hr );
    }

    //
    // Determine encryption algorithm
    //
    hr = pSample->GetUINT32( MFSampleExtension_Encryption_ProtectionScheme, &ui32Attribute );
    if( SUCCEEDED( hr ) )
    {
        switch( ui32Attribute )
        {
        case MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CTR:
            fEncrypted = TRUE;
            oMFSampleData.m_fIsCBCContent = FALSE;
            fHasEncryptionScheme = TRUE;
            break;
        case MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CBC:
            fEncrypted = TRUE;
            oMFSampleData.m_fIsCBCContent = TRUE;
            fHasEncryptionScheme = TRUE;
            break;
        case MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_NONE:
            // no-op
            break;
        default:
            // MFSampleExtension_Encryption_ProtectionScheme is set to an invalid value.  Failing playback.
            IF_FAILED_GOTO( MF_E_INVALID_STREAM_DATA );
        }
    }
    else
    {
        if( MF_E_ATTRIBUTENOTFOUND == hr )
        {
            hr = S_OK;
        }
        IF_FAILED_GOTO( hr );
    }

    //
    // Determine the number of encrypted blocks in each stripe if striping is being used
    //
    hr = pSample->GetUINT32( MFSampleExtension_Encryption_CryptByteBlock, &ui32Attribute );
    if( SUCCEEDED( hr ) )
    {
        oMFSampleData.m_dwStripeEncryptedBlocks = ui32Attribute;
    }
    else
    {
        if( MF_E_ATTRIBUTENOTFOUND == hr )
        {
            hr = S_OK;
        }
        IF_FAILED_GOTO( hr );
    }

    //
    // Determine the number of clear blocks in each stripe if striping is being used
    //
    hr = pSample->GetUINT32( MFSampleExtension_Encryption_SkipByteBlock, &ui32Attribute );
    if( SUCCEEDED( hr ) )
    {
        oMFSampleData.m_dwStripeClearBlocks = ui32Attribute;
    }
    else
    {
        if( MF_E_ATTRIBUTENOTFOUND == hr )
        {
            hr = S_OK;
        }
        IF_FAILED_GOTO( hr );
    }

    //
    // Verify that both stripe values are zero or non-zero
    //
    if( ( oMFSampleData.m_dwStripeEncryptedBlocks == 0 ) != ( oMFSampleData.m_dwStripeClearBlocks == 0 ) )
    {
        // MFSampleExtension_Encryption_CryptByteBlock (%u) and MFSampleExtension_Encryption_SkipByteBlock (%u) are not both zero or non-zero.  Failing playback.
        IF_FAILED_GOTO( MF_E_INVALID_STREAM_DATA );
    }

    if( fEncrypted )
    {
        if( !fHasEncryptionScheme )
        {
            //
            // Not all sources correctly set the encryption scheme to the
            // default of MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CTR
            // when it is not specified in the content file, e.g. because
            // the (optional) 'schm' box is not present.
            // To ensure that downstream components don't also have to
            // be aware of this case, set that default here.
            //
            IF_FAILED_GOTO( pSample->SetUINT32( MFSampleExtension_Encryption_ProtectionScheme, MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CTR ) );
        }
    }
    else
    {
        //
        //  Even if the sample isn't encrypted just create a sub sample mapping set
        //  as it may be necessary later during sample processing.
        //

        IF_FAILED_GOTO( oMFSampleData.m_pvSubSampleMapping.Clear() );
        IF_NULL_GOTO( oMFSampleData.m_pvSubSampleMapping.caul.pElems = (ULONG*)CoTaskMemAlloc( 2 * sizeof( DWORD ) ), E_OUTOFMEMORY );
        oMFSampleData.m_pvSubSampleMapping.caul.cElems = 2;
        oMFSampleData.m_pvSubSampleMapping.vt = VT_VECTOR | VT_UI4;

        IF_FAILED_GOTO( pSample->GetTotalLength( &oMFSampleData.m_pvSubSampleMapping.caul.pElems[ 0 ] ) );
        oMFSampleData.m_pvSubSampleMapping.caul.pElems[ 1 ] = 0;
    }

    if( fEncrypted )
    {
        if( guidKid == GUID_NULL )
        {
            //
            // We can't decrypt an encrypted sample if we don't know what key to use.
            //
            IF_FALSE_GOTO( *pguidKidCurrent != GUID_NULL, MF_E_INVALID_KEY );
        }
        else
        {
            if( *pguidKidCurrent != guidKid )
            {
                *pguidKidCurrent = guidKid;
                *pguidLidCurrent = GUID_NULL;
            }
        }
    }
    *pfEncrypted = fEncrypted;

done:
    CoTaskMemFree( pbSubSampleMapping );
    CoTaskMemFree( pbSubSampleMappingSplit );
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::VerifyPEAuthStateTwicePerSample()
{
    HRESULT hr                          = S_OK;
    BOOL    fRequireTrustedKernel       = FALSE;
    BOOL    fRequireTrustedUsermode     = FALSE;
    DWORD   dwMinimumGRLVersionRequired = 0;
    BOOL    fBlockTestSignatures        = FALSE;

    //
    // Since we call this function twice per sample, multiply the interval by two
    //
    if( m_cSamplesSinceLastPMPCheck >= ( PEAUTH_STATE_CHECK_INTERVAL << 1 ) )
    {
        IF_FAILED_GOTO( VerifyPEAuthUsingPEAuthHelper(
            m_spCdm_clearkey_PEAuthHelper.Get(),
            fRequireTrustedKernel,
            fRequireTrustedUsermode,
            dwMinimumGRLVersionRequired,
            fBlockTestSignatures,
            nullptr ) );
        m_cSamplesSinceLastPMPCheck = 0;
    }
    else
    {
        m_cSamplesSinceLastPMPCheck++;
    }

done:
    return hr;
}

static HRESULT _CreateNewSampleAndCopyDataIntoIt(
    _In_opt_                      IMFSample       *pSampleForDurationAndTime,
    _In_                          DWORD            cbData,
    _In_count_( cbData )    const BYTE            *pbData,
    _COM_Outptr_                  IMFSample      **ppSample )
{
    HRESULT hr = S_OK;
    BYTE  *pb           = nullptr;
    DWORD  cb           = 0;
    DWORD  dwAlignment  = 16;
    ComPtr<IMFSample>      spNewSample;
    ComPtr<IMFMediaBuffer> spNewBuffer;

    IF_NULL_GOTO( pbData, E_INVALIDARG );
    IF_NULL_GOTO( ppSample, E_INVALIDARG );

    IF_FAILED_GOTO( MFCreateSample( &spNewSample ) );
    IF_FAILED_GOTO( MFCreateAlignedMemoryBuffer( cbData, dwAlignment - 1, &spNewBuffer ) );
    IF_FAILED_GOTO( spNewSample->AddBuffer( spNewBuffer.Get() ) );
    IF_FAILED_GOTO( spNewBuffer->SetCurrentLength( cbData ) );

    IF_FAILED_GOTO( spNewBuffer->Lock( &pb, nullptr, &cb ) );

    //
    // Performance:
    // MF has a memcpy equivalent that's specifically
    // designed for speed when acting on large blocks of data.
    //
    if( cbData >= 4096 )  // 4 KB
    {
        IF_FAILED_GOTO( MFCopyImage( pb, 0, pbData, 0, cbData, 1 ) );
    }
    else
    {
        memcpy( pb, pbData, cb );
    }

    *ppSample = spNewSample.Detach();

done:
    if( spNewBuffer != nullptr )
    {
        spNewBuffer->Unlock();
    }
    return hr;
}

static HRESULT _CloneSample( _In_ IMFSample *pSample, _COM_Outptr_ IMFSample **ppSample )
{
    HRESULT   hr                = S_OK;
    BYTE     *pbInput           = nullptr;
    DWORD     cbInput           = 0;
    LONGLONG  hnsSampleDuration = 0;
    DWORD     dwSampleFlags     = 0;
    LONGLONG  hnsSampleTime     = 0;
    ComPtr<IMFMediaBuffer> spInputBuffer;
    ComPtr<IMFSample>      spNewSample;

    IF_FAILED_GOTO( pSample->ConvertToContiguousBuffer( &spInputBuffer ) );
    IF_FAILED_GOTO( spInputBuffer->Lock( &pbInput, nullptr, &cbInput ) );
    IF_FAILED_GOTO( _CreateNewSampleAndCopyDataIntoIt( pSample, cbInput, pbInput, &spNewSample ) );

    // copy attributes FROM source pSample TO dest spNewSample
    IF_FAILED_GOTO( pSample->CopyAllItems( spNewSample.Get() ) );

    // Only copy the sample duration if the source sample has one
    hr = pSample->GetSampleDuration( &hnsSampleDuration );
    if( hr == MF_E_NO_SAMPLE_DURATION )
    {
        hr = S_OK;
    }
    else
    {
        IF_FAILED_GOTO( hr );
        IF_FAILED_GOTO( spNewSample->SetSampleDuration( hnsSampleDuration ) );
    }

    // Only copy the sample time if the source sample has one
    hr = pSample->GetSampleTime( &hnsSampleTime );
    if( hr == MF_E_NO_SAMPLE_TIMESTAMP )
    {
        hr = S_OK;
    }
    else
    {
        IF_FAILED_GOTO( hr );
        IF_FAILED_GOTO( spNewSample->SetSampleTime( hnsSampleTime ) );
    }

    // GetSampleFlags should not fail.  It does not have an error like the above.
    IF_FAILED_GOTO( pSample->GetSampleFlags( &dwSampleFlags ) );
    IF_FAILED_GOTO( spNewSample->SetSampleFlags( dwSampleFlags ) );

    *ppSample = spNewSample.Detach();

done:
    if( spInputBuffer != nullptr )
    {
        (void)spInputBuffer->Unlock();
    }

    return hr;
}

static HRESULT _DecryptData(
    _In_                    BCRYPT_KEY_HANDLE    hKey,
    _Inout_                 CMFSampleData       &oMFSampleData,
    _In_                    QWORD                qwOffset,
    _In_                    BYTE                 bByteOffset,
    _In_                    DWORD                cbData,
    _Inout_count_( cbData ) BYTE                *pbData )
{
    HRESULT  hr           = S_OK;
    NTSTATUS status       = STATUS_SUCCESS;
    DWORD    cbDataBcrypt = 0;
    BYTE rgbIV[ AES_BLOCK_LEN ] = { 0 };
    DWORD dwStripeEncryptedBlocks = oMFSampleData.m_dwStripeEncryptedBlocks;

    memcpy( rgbIV, oMFSampleData.m_rgbIV, AES_BLOCK_LEN );

    if( oMFSampleData.m_fIsCBCContent )
    {
        if( dwStripeEncryptedBlocks == 0 )
        {
            //
            // CBC1: IV carries across sub-samples per spec, so update it
            //
            ADD_TO_IV( rgbIV, qwOffset );
        }
        else
        {
            //
            // CBCS: IV gets reset at the start of each sub-sample per spec, so ignore qwOffset
            //
            // no-op
        }

        if( bByteOffset != 0 )
        {
            //
            // CBCS/CBC1: partial blocks are not supported, per spec
            //
            IF_FAILED_GOTO( MF_E_INVALID_FILE_FORMAT );
        }

        status = BCryptDecrypt( hKey, pbData, cbData, nullptr, rgbIV, AES_BLOCK_LEN, pbData, cbData, &cbDataBcrypt, BCRYPT_BLOCK_PADDING );
        IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );
    }
    else
    {
        BYTE rgbIVEncrypted[ AES_BLOCK_LEN ];
        BYTE *pbDataCurrent = pbData;
        DWORD cbDataCurrent = cbData;
        DWORD cbToDecrypt = 0;

        if( dwStripeEncryptedBlocks != 0 )
        {
            //
            // CENS is not supported by this MFT
            //
            IF_FAILED_GOTO( MF_E_INVALID_FILE_FORMAT );
        }

        //
        // CENC: IV (counter) carries across sub-samples per spec, so update it
        //
        ADD_TO_IV( rgbIV, qwOffset );

        // BCrypt doesn't support CTR without auth, but that's what we need.  So we have to do it ourselves a block at a time using ECB.

        if( bByteOffset != 0 )
        {
            //
            // Handle a partial block at the start of the sub-sample
            //
            IF_FALSE_GOTO( bByteOffset + 1 < AES_BLOCK_LEN, MF_E_INVALID_FILE_FORMAT );

            //
            // The entire subsample might be less than a single block, so make sure we act on no more than sub-sample size
            //
            cbToDecrypt = min( (DWORD)( AES_BLOCK_LEN - bByteOffset ), cbDataCurrent );

            //
            // *Encrypt* the current IV
            //
            status = BCryptEncrypt( hKey, rgbIV, AES_BLOCK_LEN, nullptr, nullptr, 0, rgbIVEncrypted, AES_BLOCK_LEN, &cbDataBcrypt, 0 );
            IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

            //
            // XOR the relevant portion of the IV with the portion of the data we are acting on
            //
            for( DWORD ibData = 0; ibData < cbToDecrypt; ibData++ )
            {
                pbDataCurrent[ ibData ] ^= rgbIVEncrypted[ bByteOffset + ibData ];
            }

            //
            // Increment the IV for the next block
            //
            ADD_TO_IV( rgbIV, 1 );
            pbDataCurrent += cbToDecrypt;
            cbDataCurrent -= cbToDecrypt;
        }

        while( cbDataCurrent > 0 )
        {
            //
            // Handle all complete blocks plus any partial block at the end of the sub-sample
            //
            cbToDecrypt = min( cbDataCurrent, AES_BLOCK_LEN );

            //
            // *Encrypt* the current IV
            //
            status = BCryptEncrypt( hKey, rgbIV, AES_BLOCK_LEN, nullptr, nullptr, 0, rgbIVEncrypted, AES_BLOCK_LEN, &cbDataBcrypt, 0 );
            IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

            //
            // XOR the relevant portion of the IV with the portion of the data we are acting on
            //
            for( DWORD ibBlock = 0; ibBlock < cbToDecrypt; ibBlock++ )
            {
                pbDataCurrent[ ibBlock ] ^= rgbIVEncrypted[ ibBlock ];
            }

            //
            // Increment the IV for the next block
            //
            ADD_TO_IV( rgbIV, 1 );
            pbDataCurrent += cbToDecrypt;
            cbDataCurrent -= cbToDecrypt;
        }
    }

done:
    CLEARKEY_CONFIRM_NTSTATUS( status, hr );
    return hr;
}

static HRESULT _DecryptSample(
    _In_count_( AES_BLOCK_LEN ) const BYTE              pbAES128Key[ AES_BLOCK_LEN ],
    _In_                        CMFSampleData          &oMFSampleData,
    _In_                        IMFSample              *pSample,
    _Inout_                     CTPtrList<IMFSample>   &oSampleQueue )
{
    HRESULT                hr                 = S_OK;
    BYTE                  *pbOutputWeakRef    = nullptr;
    DWORD                  cbOutputWeakRef    = 0;
    DWORD                  cSubSampleMappings = oMFSampleData.m_pvSubSampleMapping.caul.cElems;
    BOOL                   fDecrypted         = FALSE;
    BYTE                   bByteOffset        = 0;
    QWORD                  qwOffset           = 0;
    NTSTATUS               status             = STATUS_SUCCESS;
    BCRYPT_ALG_HANDLE      hAlg               = nullptr;
    BCRYPT_KEY_HANDLE      hKey               = nullptr;
    ComPtr<IMFSample>      spSample;
    ComPtr<IMFMediaBuffer> spOutputBuffer;

    status = BCryptOpenAlgorithmProvider( &hAlg, BCRYPT_AES_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0 );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

    status = BCryptGenerateSymmetricKey( hAlg, &hKey, nullptr, 0, (BYTE*)pbAES128Key, AES_BLOCK_LEN, 0 );
    IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );

    if( oMFSampleData.m_fIsCBCContent )
    {
        status = BCryptSetProperty( hKey, BCRYPT_CHAINING_MODE, (BYTE*)BCRYPT_CHAIN_MODE_CBC, sizeof( BCRYPT_CHAIN_MODE_CBC ), 0 );
        IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );
    }
    else
    {
        // BCrypt doesn't support CTR without auth, but that's what we need.  So we have to do it ourselves a block at a time using ECB.
        status = BCryptSetProperty( hKey, BCRYPT_CHAINING_MODE, (BYTE*)BCRYPT_CHAIN_MODE_ECB, sizeof( BCRYPT_CHAIN_MODE_ECB ), 0 );
        IF_FALSE_GOTO( status == STATUS_SUCCESS, HRESULT_FROM_NT( status ) );
    }

    IF_FAILED_GOTO( _CloneSample( pSample, &spSample ) );
    IF_FAILED_GOTO( spSample->ConvertToContiguousBuffer( &spOutputBuffer ) );
    IF_FAILED_GOTO( spOutputBuffer->Lock( &pbOutputWeakRef, nullptr, &cbOutputWeakRef ) );

    if( cSubSampleMappings == 0 )
    {
        IF_FAILED_GOTO( _DecryptData( hKey, oMFSampleData, 0, 0, cbOutputWeakRef, pbOutputWeakRef ) );
        IF_FAILED_GOTO( _MoveSampleIntoQueue( oSampleQueue, spSample ) );
    }
    else
    {
        DWORD  cbTotalSampleLength    = 0;
        DWORD  cbTotalEncryptedLength = 0;
        DWORD  iMapping               = 0;
        BYTE  *pbCurrentBuffLocation  = pbOutputWeakRef;
        BOOL   fPropertiesCopied      = FALSE;

        while( iMapping < cSubSampleMappings )
        {
            DWORD cbClearLength;
            DWORD cbEncryptedLength;

            cbClearLength     = oMFSampleData.m_pvSubSampleMapping.caul.pElems[ iMapping++ ];
            cbEncryptedLength = oMFSampleData.m_pvSubSampleMapping.caul.pElems[ iMapping++ ];

            if( ( 0 == cbClearLength ) && ( 0 == cbEncryptedLength ) )
            {
                // The subsample mapping had both clear and encrypted values set to zero!
                IF_FAILED_GOTO( MF_E_INVALID_FILE_FORMAT );
            }

            SAFE_ADD_DWORD( cbTotalEncryptedLength, cbEncryptedLength, &cbTotalEncryptedLength );
            SAFE_ADD_DWORD( cbTotalSampleLength, cbClearLength, &cbTotalSampleLength );
            SAFE_ADD_DWORD( cbTotalSampleLength, cbEncryptedLength, &cbTotalSampleLength );

            if( cbTotalSampleLength > cbOutputWeakRef )
            {
                IF_FAILED_GOTO( MF_E_INVALID_FILE_FORMAT );
            }

            if( oMFSampleData.m_fSubSampleMappingSplit && cbClearLength > 0 )
            {
                ComPtr<IMFSample> spClearSample;

                IF_FAILED_GOTO( _CreateNewSampleAndCopyDataIntoIt( nullptr, cbClearLength, pbCurrentBuffLocation, &spClearSample ) );

                if( !fPropertiesCopied )
                {
                    IF_FAILED_GOTO( spSample->CopyAllItems( spClearSample.Get() ) );
                    fPropertiesCopied = TRUE;
                }
                IF_FAILED_GOTO( _MoveSampleIntoQueue( oSampleQueue, spClearSample ) );
            }

            pbCurrentBuffLocation += cbClearLength;

            if( 0 < cbEncryptedLength )
            {
                if( cbEncryptedLength > cbOutputWeakRef )
                {
                    // The total encrypted mapping length exceeded the frame size!
                    IF_FAILED_GOTO( MF_E_INVALID_FILE_FORMAT );
                }

                IF_FAILED_GOTO( _DecryptData( hKey, oMFSampleData, qwOffset, bByteOffset, cbEncryptedLength, pbCurrentBuffLocation ) );
                fDecrypted = TRUE;
                if( oMFSampleData.m_fSubSampleMappingSplit )
                {
                    ComPtr<IMFSample> spEncSample;

                    IF_FAILED_GOTO( _CreateNewSampleAndCopyDataIntoIt( nullptr, cbEncryptedLength, pbCurrentBuffLocation, &spEncSample ) );
                    if( !fPropertiesCopied )
                    {
                        IF_FAILED_GOTO( spSample->CopyAllItems( spEncSample.Get() ) );
                        fPropertiesCopied = TRUE;
                    }
                    IF_FAILED_GOTO( _MoveSampleIntoQueue( oSampleQueue, spEncSample ) );
                }

                pbCurrentBuffLocation += cbEncryptedLength;
            }

            //
            //  Since the encrypted data within the subsample is effectively treated as one chunk of continuously
            //  encrypted data, we need to update the byte and block offsets in oSampleData for the next call.
            //
            bByteOffset = cbTotalEncryptedLength % AES_BLOCK_LEN;
            qwOffset    = cbTotalEncryptedLength / AES_BLOCK_LEN;
        }

        if( cbTotalSampleLength > cbOutputWeakRef )
        {
            // The total sample length was greater than the output length!
            IF_FAILED_GOTO( MF_E_INVALID_FILE_FORMAT );
        }

        if( !oMFSampleData.m_fSubSampleMappingSplit )
        {
            IF_FAILED_GOTO( _MoveSampleIntoQueue( oSampleQueue, spSample ) );
        }
    }

done:
    if( hKey != nullptr )
    {
        BCryptDestroyKey( hKey );
    }
    if( hAlg != nullptr )
    {
        BCryptCloseAlgorithmProvider( hAlg, 0 );
    }
    if( nullptr != spOutputBuffer )
    {
        spOutputBuffer->Unlock();
    }
    CLEARKEY_CONFIRM_NTSTATUS( status, hr );
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::Enable( void )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;

    HRESULT hr = S_OK;
    if( this->GetEnabledState() == MF_E_INVALIDREQUEST )
    {
        this->SetEnabledState( S_OK );
        IF_FAILED_GOTO( this->StartInternal( c_fNotStartOfStream ) );
    }

done:
    if( FAILED( hr ) )
    {
        this->SetEnabledState( hr );
    }
    return hr;
}

void Cdm_clearkey_IMFTransform::Disable( void )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;

    m_guidKid = GUID_NULL;
    m_guidLid = GUID_NULL;

    this->SetEnabledState( MF_E_UNRECOVERABLE_ERROR_OCCURRED );    // Note: Must NOT be error code 'MF_E_INVALIDREQUEST' - disable is permanent.

    m_rgPendingPolicyChangeObjects.RemoveAll();

    while( !m_OutputSampleQueue.IsEmpty() )
    {
        ComPtr<IMFSample> spSample;
        (void)m_OutputSampleQueue.RemoveHead( &spSample );
    }

    while( !m_InputSampleQueue.IsEmpty() )
    {
        ComPtr<IMFSample> spSample;
        (void)m_InputSampleQueue.RemoveHead( &spSample );
    }
    m_fInputSampleQueueDraining = FALSE;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::RegisterThreadsEx(
    _Inout_ DWORD    *pdwTaskIndex,
    _In_    LPCWSTR   pwszClassName,
    _In_    LONG      lBasePriority )
{
    //
    // Optional API
    //
    return S_OK;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::UnregisterThreads()
{
    //
    // Optional API
    //
    return S_OK;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::SetWorkQueueEx(
    _In_ DWORD dwMultithreadedWorkQueueId,
    _In_ LONG  lWorkItemBasePriority )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;

    HRESULT hr = S_OK;

    IF_FAILED_GOTO( this->SetWorkQueueValue( dwMultithreadedWorkQueueId, lWorkItemBasePriority ) );

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::LongRunning_DecryptSample( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock )
{
    HRESULT      hr                     = S_OK;
    IMFSample   *pFirstSampleWeakRef    = nullptr;
    GUID         guidLidOld             = m_guidLid;
    BOOL         fNotify                = FALSE;
    BYTE        *pbPreviousPolicy       = nullptr;
    BOOL         fEncrypted             = FALSE;
    CMFSampleData firstSampleData;
    BYTE rgbKeyData[ KEY_DATA_TOTAL_SIZE ];
    BYTE rgbAES128Key[ KEY_DATA_KEY_SIZE ];
    CTPtrList<IMFSample> oTempSampleQueue;
    ComPtr<IMFAttributes> spIMFAttributesITA;
    ComPtr<IMFAttributes> spIMFAttributesTI;
    ComPtr<IMFAttributes> spIMFAttributesCDM;
    CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS oPolicy = { 0 };

    IF_FALSE_GOTO( m_InputSampleQueue.GetHead( &pFirstSampleWeakRef ), MF_E_TRANSFORM_NEED_MORE_INPUT );

    IF_FAILED_GOTO( _GetSampleEncryptionData( pFirstSampleWeakRef, firstSampleData, &m_guidKid, &m_guidLid, &fEncrypted ) );

    if( !fEncrypted )
    {
        //
        // This sample is in the clear - just move it from the input queue to the output queue and jump to done
        //
        {
            ComPtr<IMFSample> spSample = pFirstSampleWeakRef;
            IF_FAILED_GOTO( _MoveSampleIntoQueue( m_OutputSampleQueue, spSample ) );
        }

        {
            ComPtr<IMFSample> spSample;
            (void)m_InputSampleQueue.RemoveHead( &spSample );
        }
        goto done;
    }

    //
    // This sample is encrypted.
    //

    IF_FAILED_GOTO( m_spIMFAttributes->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_ITA_TO_MFT, IID_PPV_ARGS( &spIMFAttributesITA ) ) );
    IF_FAILED_GOTO( spIMFAttributesITA->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_TI_TO_ITA, IID_PPV_ARGS( &spIMFAttributesTI ) ) );
    IF_FAILED_GOTO( spIMFAttributesTI->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_TI, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );

    //
    // Any licenses?
    //
    // If m_guidLid == guidLidOld == GUID_NULL, _LocateUsableLicense will find the first usable license for the Kid.
    //  If one exists, m_guidLid will be set to a real Lid, _LocateUsableLicense will succeed,
    //   and guidLidOld == GUID_NULL && m_guidLid == [Lid_2] which will trigger policy rotation below.
    //  Otherwise, _LocateUsableLicense will return MF_E_LICENSE_INCORRECT_RIGHTS.
    //
    // Otherwise, _LocateUsableLicense will determine whether the license for m_guidLid is still usable.
    //  If it IS, m_guidLid will remain unchanged, _LocateUsableLicense will succeed,
    //   guidLidOld == m_guidLid, and we'll continue to decrypt with the same license.
    //  Otherwise, _LocateUsableLicense will return MF_E_LICENSE_INCORRECT_RIGHTS.
    //
    hr = _LocateUsableLicense( spIMFAttributesCDM.Get(), m_guidKid, &m_guidLid, rgbKeyData, &fNotify, nullptr );

    if( hr == MF_E_LICENSE_INCORRECT_RIGHTS )
    {
        if( guidLidOld == GUID_NULL )
        {
            //
            // We didn't previously have a usable license for this Kid and we just failed to find one.
            // Thus, we can't decrypt.  Propagate the MF_E_LICENSE_INCORRECT_RIGHTS error immediately.
            //
            IF_FAILED_GOTO( hr );
        }
        else
        {
            BOOL fExpired = FALSE;

            //
            // We previously had a usable license, but it is no longer usable.
            // Any other license for this kid?
            //  If not, _LocateUsableLicense will return MF_E_LICENSE_INCORRECT_RIGHTS which we propagate now.
            //  If so, m_guidLid will change to the new license's Lid and
            //   guidLidOld == [Lid_1] && m_guidLid == [Lid_2] which will trigger policy rotation below.
            //
            m_guidLid = GUID_NULL;
            hr = _LocateUsableLicense( spIMFAttributesCDM.Get(), m_guidKid, &m_guidLid, rgbKeyData, &fNotify, &fExpired );
            if( hr == MF_E_LICENSE_INCORRECT_RIGHTS && fExpired )
            {
                hr = MF_E_LICENSE_OUTOFDATE;
            }
            IF_FAILED_GOTO( hr );

            //
            // Found a usable license; notify updated status if required
            //
            if( fNotify )
            {
                this->NotifyMediaKeyStatus();
            }
        }
    }
    else
    {
        //
        // Handle general errors tring to find a usable license
        //
        IF_FAILED_GOTO( hr );

        //
        // Found a usable license; notify updated status if required
        //
        if( fNotify )
        {
            this->NotifyMediaKeyStatus();
        }
    }

    if( guidLidOld != m_guidLid )
    {
        //
        // We're now acting on a new license, either because we never used this license for this Kid
        //  or because the previous license we were using became unusable.
        //
        GetKeyData( rgbKeyData, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &oPolicy );
        BOOL fMoreRestrictivePolicy = FALSE;
        if( _PolicyRequiresOTA( oPolicy ) )
        {
            if( guidLidOld == GUID_NULL )
            {
                //
                // This is the first time we're using a license for this Kid.
                // We need to make sure we enforce policy for this license.
                //
                fMoreRestrictivePolicy = TRUE;
            }
            else
            {
                //
                // We're rotating from one license to another, e.g. one license expired but we have another license for the same kid.
                // If the license policy for the new license is more restrictive than the policy we already enforced,
                // we need to enforce the more restrictive policy before we decrypt any samples using the new license.
                //
                UINT32 cbPreviousPolicy = 0;

                hr = spIMFAttributesITA->GetAllocatedBlob( CLEARKEY_GUID_ATTRIBUTE_LAST_ITA_ENFORCED_POLICY, &pbPreviousPolicy, &cbPreviousPolicy );
                if( SUCCEEDED( hr ) )
                {
                    IF_FALSE_GOTO( cbPreviousPolicy == sizeof( CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS ), MF_E_UNEXPECTED );
                    CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS *pPreviousPolicyWeakRef = (CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS*)pbPreviousPolicy;

                    //
                    // Note: The following comparison logic for policy rotation *REQUIRES* that more
                    // restrictive policies have larger integer values than less restrictive policies.
                    // For example, requiring HDCP Type 1 is more restrictive than HDCP with any version,
                    // so the integer value for the former is greater than the integer value for the latter.
                    //
                    // This allows us to do a simple integer comparison - if any policy has a larger,
                    // then the (overall) policy is more restrictive and we need to enforce it.
                    //
                    fMoreRestrictivePolicy = oPolicy.dwHDCP > pPreviousPolicyWeakRef->dwHDCP || oPolicy.dwCGMSA > pPreviousPolicyWeakRef->dwCGMSA || oPolicy.dwDigitalOnly > pPreviousPolicyWeakRef->dwDigitalOnly;
                }
                else
                {
                    if( MF_E_ATTRIBUTENOTFOUND == hr )
                    {
                        hr = S_OK;
                    }
                    IF_FAILED_GOTO( hr );
                }
            }
        }

        if( fMoreRestrictivePolicy )
        {
            //
            // We have more restrictive policy that the OTA needs to enforce, e.g.
            // engage HDCP or enforcement of a higher HDCP version than is currently engaged.
            //
            // We should not decrypt samples using a license that has unenforced policy,
            // so we need to halt sample processing until that new policy is enforced.
            //
            // This may be a time-consuming operation, e.g. if we need to (newly) establish HDCP,
            // and we don't want to stall the pipeline while waiting by doing it synchronously.
            //
            // So, we set up one or more policy objects (one per action we've encountered),
            // cache them in the m_rgPendingPolicyChangeObjects list, enter the
            // waiting state, abort sample processing by jumping to 'done', queue
            // the policy objects to be sent downstream via AddToOutputDataBufferQueue
            // (in GetOutput), fire the MEPolicyChanged event (in GetQueuedSampleOrEvents),
            // and fire the METransformHaveOutput event (in GetOutput).
            //
            // Entering the waiting state will then will halt ProcessingLoop because
            // there's nothing for it to do until the policy is enforced.
            //
            // When the pipeline calls ProcessOutput in response to METransformHaveOutput,
            // the pending policy objects will be sent downstream to the OTA.
            //
            // Once the OTA has finished enforcing any given policy, it will
            // fire the MEPolicySet event back at us for that single policy,
            // which we handle in OnMediaEventMessage.  Once we've received
            // one for each policy, we'll exit the waiting state (in StartInternal)
            // and fire up the processing loop again (also in StartInternal).
            //
            // This function will then be reentered, but _LocateUsableLicense will
            // then succeed on the first call with m_guidLid already having been set.
            // Thus, on that future call, guidLidOld == m_guidLid and this block
            // will not be entered.
            //
            for( DWORD iAction = 0; iAction < ARRAYSIZE( s_rgbSupportedActions ); iAction++ )
            {
                ComPtr<IMFOutputPolicy> spIMFOutputPolicyPending;
                MFPOLICYMANAGER_ACTION eAction = s_rgbSupportedActions[ iAction ];
                UINT32 uiActionTracked = 0;

                IF_FAILED_GOTO( spIMFAttributesITA->GetUINT32( _ActionToGuid( eAction ), &uiActionTracked ) );

                if( uiActionTracked )
                {
                    DWORD dwPolicyIndex = InterlockedIncrement( &s_dwNextPolicyId );
                    if( dwPolicyIndex == 0 )
                    {
                        dwPolicyIndex = InterlockedIncrement( &s_dwNextPolicyId );
                    }

                    hr = MakeAndInitialize<Cdm_clearkey_IMFOutputPolicy, IMFOutputPolicy>( &spIMFOutputPolicyPending, spIMFAttributesITA.Get(), eAction, m_guidKid, m_guidLid );
                    IF_FAILED_GOTO( hr );

                    IF_FAILED_GOTO( spIMFOutputPolicyPending->SetUINT32( MF_POLICY_ID, dwPolicyIndex ) );
                    IF_FAILED_GOTO( m_rgPendingPolicyChangeObjects.Add( spIMFOutputPolicyPending.Get() ) );
                    IF_FAILED_GOTO( m_rgdwPolicyIds.Add( dwPolicyIndex ) );
                }
            }
            this->SetState( CLEARKEY_MFT_STATE_WAITING_FOR_POLICY_NOTIFICATION );
            goto done;
        }
    }

    //
    // If we got this far, we found a usable license and m_guidLid is set to its Lid.
    //
    // We also know that no new license policy needs to be enforced or
    // we would have entered a waiting state and jumped to 'done' inside the
    // fMoreRestrictivePolicy block above.
    //
    // So, we're ready to decrypt.  Grab the license's AES key.
    //
    GetKeyData( rgbKeyData, nullptr, nullptr, rgbAES128Key, nullptr, nullptr, nullptr, nullptr, nullptr );

    //
    // We execute actual decryption outside the lock as we don't want to stall the overall
    // pipeline during the potentially long-running operation of decrypting a large sample.
    //
    //
    // Decrypt can break up a single input sample into multiple output
    // samples based on sub-sample mappings.
    // In addition, m_OutputSampleQueue is protected by the output lock.
    // Since we won't be holding the output lock, we place Decrypt's
    // output sample(s) into a temporary queue rather than directly into
    // m_OutputSampleQueue.  We then transfer those samples, in sequence,
    // from the temporary queue to m_OutputSampleQueue after we've
    // reacquired the lock.
    //
    IF_FAILED_GOTO( oTempSampleQueue.Initialize( 1 ) );

    //
    // Make sure no modifiable values (members OR pointer/reference parameters) are used while
    // we aren't holding the lock!  (Params included because caller might pass a member.)
    // Note: DO NOT execute any jumps / jump macros between Unlock and Lock!
    //
    {
        BOOL fOutputLockIsUnlocked = FALSE;
        CLEARKEY_MFT_UNLOCK_BEFORE_LONG_RUNNING_OPERATION( c_fYesDuringProcessSample, &fOutputLockIsUnlocked );
        hr = _DecryptSample( rgbAES128Key, firstSampleData, pFirstSampleWeakRef, oTempSampleQueue );
        CLEARKEY_MFT_RELOCK_AFTER_LONG_RUNNING_OPERATION( &fOutputLockIsUnlocked );
    }

    //
    // Handle general errors during decryption
    //
    IF_FAILED_GOTO( hr );

    if( this->ShouldIgnoreSampleAfterLongRunningOperation( pFirstSampleWeakRef ) )
    {
        //
        // We've just modified the head item in the queue via Decrypt,
        // but our state changed in a way that makes the sample irrelevant
        // (e.g. we were stopped, flushed, etc) (while we were outside the lock).
        // We don't add anything to the output queue; we just discard the sample completely.
        //
        if( !this->DidInputQueueChange( pFirstSampleWeakRef ) )
        {
            ComPtr<IMFSample> spInputSampleDiscard;
            (void)m_InputSampleQueue.RemoveHead( &spInputSampleDiscard );
        }

        //
        // We also need to discard any temporary samples created by decrypt.
        //
        while( !oTempSampleQueue.IsEmpty() )
        {
            ComPtr<IMFSample> spTempSampleDiscard;
            (void)oTempSampleQueue.RemoveHead( &spTempSampleDiscard );
        }

        //
        // Restart the processing loop given our new state.
        //
        goto done;
    }

    //
    // Successfully decrypted the sample!!!
    //

    //
    // Transfer samples from the temporary queue to the real output queue
    //
    while( !oTempSampleQueue.IsEmpty() )
    {
        ComPtr<IMFSample> spSample;
        IF_FALSE_GOTO( oTempSampleQueue.RemoveHead( &spSample ), MF_E_UNEXPECTED );
        IF_FAILED_GOTO( _MoveSampleIntoQueue( m_OutputSampleQueue, spSample ) );
    }

    {
        //
        // If the sample was broken up into multiple output samples,
        // we need to ensure that the samples beyond the first have correct data.
        //
        LISTPOS    pos                  = m_OutputSampleQueue.GetHeadPosition();
        BOOL       fCleanPoint          = FALSE;
        IMFSample *pOutputSampleWeakRef = nullptr;

        if( 0 < MFGetAttributeUINT32( pFirstSampleWeakRef, MFSampleExtension_CleanPoint, 0 ) )
        {
            fCleanPoint = TRUE;
        }

        while( m_OutputSampleQueue.GetNext( pos, &pOutputSampleWeakRef ) )
        {
            if( fCleanPoint )
            {
                IF_FAILED_GOTO( pOutputSampleWeakRef->SetUINT32( MFSampleExtension_CleanPoint, 1 ) );
            }

            IF_FAILED_GOTO( pOutputSampleWeakRef->DeleteItem( MFSampleExtension_MaxDecodeFrameSize ) );
            _CopySampleTimeAndDuration( pFirstSampleWeakRef, pOutputSampleWeakRef );
        }
    }

    //
    // We do two pseudo-random verifications of the PEAuth state on the code path that decrypt a sample - the
    // first verification is at the very beginning inside DoProcessSample and the second one is at the end
    // inside LongRunning_DecryptSample. The two checks are independent of each other.
    //
    IF_FAILED_GOTO( this->VerifyPEAuthStateTwicePerSample() );

    //
    // Only when we reach here have we successfully decrypted the head
    // item in the queue, so we need to remove and release it.
    //
    {
        ComPtr<IMFSample> spSample;
        (void)m_InputSampleQueue.RemoveHead( &spSample );
    }

done:
    CoTaskMemFree( pbPreviousPolicy );
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::GetQueuedSampleOrEvents(
    _COM_Outptr_    IMFSample       **ppOutputSample,
    _COM_Outptr_    IMFCollection   **ppOutputCollection,
    _Out_           DWORD            *pdwSampleStatus,
    _Out_           DWORD            *pdwOutputStatus )
{
    HRESULT hr = S_OK;
    PROPVARIANTWRAP propvar;

    IF_NULL_GOTO( ppOutputSample, E_INVALIDARG );
    IF_NULL_GOTO( pdwSampleStatus, E_INVALIDARG );
    IF_NULL_GOTO( pdwOutputStatus, E_INVALIDARG );
    IF_NULL_GOTO( ppOutputCollection, E_INVALIDARG );

    *pdwSampleStatus = 0;
    *pdwOutputStatus = 0;

    //
    // Propagate any pending policy changes downstream
    //
    if( 0 < m_rgPendingPolicyChangeObjects.GetSize() )
    {
        ComPtr<IMFCollection> spEventCollection;

        SAFE_RELEASE( *ppOutputCollection );

        IF_FAILED_GOTO( MFCreateCollection( &spEventCollection ) );

        for( DWORD iAction = 0; iAction < m_rgPendingPolicyChangeObjects.GetSize(); iAction++ )
        {
            ComPtr<IMFMediaEvent> spPolicyChangedEvent;

            propvar.vt = VT_UNKNOWN;
            propvar.punkVal = m_rgPendingPolicyChangeObjects.GetAt( iAction );
            propvar.punkVal->AddRef();
            IF_FAILED_GOTO( MFCreateMediaEvent( MEPolicyChanged, GUID_NULL, S_OK, &propvar, &spPolicyChangedEvent ) );
            IF_FAILED_GOTO( spEventCollection->AddElement( spPolicyChangedEvent.Get() ) );
        }

        m_rgPendingPolicyChangeObjects.RemoveAll();
        *ppOutputCollection = spEventCollection.Detach();

        //
        // MFT returns policy changes from ProcessOutput.
        //
    }
    else if( !m_OutputSampleQueue.IsEmpty() )
    {
        IF_FALSE_GOTO( *ppOutputSample == nullptr, MF_E_UNEXPECTED );
        if( m_OutputSampleQueue.RemoveHead( ppOutputSample ) )
        {
            // Returning one sample.

            if( !m_OutputSampleQueue.IsEmpty() )
            {
                // More Samples exist.
                *pdwSampleStatus = MFT_OUTPUT_DATA_BUFFER_INCOMPLETE;
            }
        }
    }
    else
    {
        // No samples available
        *pdwSampleStatus = MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE;
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::DoProcessSample(
    _In_            SyncLock2&        stateLock,
    _In_            SyncLock2&        outputLock,
    _COM_Outptr_    IMFSample       **ppOutputSample,
    _COM_Outptr_    IMFCollection   **ppOutputCollection,
    _Out_           DWORD            *pdwSampleStatus,
    _Out_           DWORD            *pdwOutputStatus )
{
    HRESULT hr = S_OK;

    IF_NULL_GOTO( ppOutputSample, E_INVALIDARG );
    IF_NULL_GOTO( pdwSampleStatus, E_INVALIDARG );
    IF_NULL_GOTO( pdwOutputStatus, E_INVALIDARG );

    *pdwSampleStatus = 0;
    *pdwOutputStatus = 0;

    IF_FAILED_GOTO( this->GetEnabledState() );

    //
    // We do two pseudo-random verifications of the PEAuth state on the code path that decrypt a sample - the
    // first verification is at the very beginning inside DoProcessSample and the second one is at the end
    // inside LongRunning_DecryptSample. The two checks are independent of each other.
    //
    IF_FAILED_GOTO( this->VerifyPEAuthStateTwicePerSample() );

    IF_FAILED_GOTO( this->GetQueuedSampleOrEvents( ppOutputSample, ppOutputCollection, pdwSampleStatus, pdwOutputStatus ) );
    if( *pdwSampleStatus == MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE )
    {
        IF_FAILED_GOTO( this->LongRunning_DecryptSample( stateLock, outputLock ) );
        IF_FAILED_GOTO( this->GetQueuedSampleOrEvents( ppOutputSample, ppOutputCollection, pdwSampleStatus, pdwOutputStatus ) );
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::OnInputTypeChanged( void )
{
    HRESULT hr            = S_OK;
    GUID    guidMajorType = GUID_NULL;
    ComPtr<IMFMediaType> spUnprotType;

    IF_NULL_GOTO( m_spInputMediaType, MF_E_UNEXPECTED );

    IF_FAILED_GOTO( m_spInputMediaType->GetMajorType( &guidMajorType ) );

    //
    // Since we are processing both clear and protected content,
    // we are going to get that are wrapped as well as ones that are
    // not. Therefore, we need to handle both cases, and NOT send an
    // error if we get non-wrapped samples
    //
    if( MFMediaType_Protected == guidMajorType )
    {
        IF_FAILED_GOTO( MFUnwrapMediaType( m_spInputMediaType.Get(), &spUnprotType ) );
    }
    else
    {
        spUnprotType = m_spInputMediaType;
    }

    IF_FAILED_GOTO( spUnprotType->GetMajorType( &guidMajorType ) );

    if( guidMajorType == MFMediaType_Audio )
    {
        if( m_fIsVideo )
        {
            // Error: Received guidMajorType = MFMediaType_Audio With Video MFT

            // If we were already set up as video we can't change to audio
            IF_FAILED_GOTO( MF_E_INVALIDMEDIATYPE );
        }
        m_fIsAudio = TRUE;
    }
    else if( guidMajorType == MFMediaType_Video )
    {
        GUID guidSubType = GUID_NULL;

        if( m_fIsAudio )
        {
            // Error: Received guidMajorType = MFMediaType_Video With Audio MFT

            // If we were already set up as audio we can't change to video
            IF_FAILED_GOTO( MF_E_INVALIDMEDIATYPE );
        }
        m_fIsVideo = TRUE;

        IF_FAILED_GOTO( spUnprotType->GetGUID( MF_MT_SUBTYPE, &guidSubType ) );
    }

    m_spAvailableOutputMediaType = spUnprotType;

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::OnDrain( void )
{
    // MF requested that we drain our sample queue.
    m_fInputSampleQueueDraining = !m_InputSampleQueue.IsEmpty();
    return S_OK;
}

HRESULT Cdm_clearkey_IMFTransform::InternalGetInputStatus( _Out_ DWORD *pdwFlags )
{
    HRESULT hr                   = S_OK;
    DWORD   dwOutpustStatusFlags = 0;

    IF_FAILED_GOTO( this->InternalGetOutputStatus( &dwOutpustStatusFlags ) );

    if( dwOutpustStatusFlags == MFT_OUTPUT_STATUS_SAMPLE_READY )
    {
        // We have output samples ready.  Don't allow input - require process output.
        *pdwFlags = 0;
    }
    else
    {
        // Output samples are NOT ready.
        *pdwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::InternalGetOutputStatus( _Out_ DWORD *pdwFlags )
{
    HRESULT hr = S_OK;

    if( !m_OutputSampleQueue.IsEmpty() )
    {
        //
        // We obviously have output samples ready.
        //
        *pdwFlags = MFT_OUTPUT_STATUS_SAMPLE_READY;
    }
    else if( m_fInputSampleQueueDraining )
    {
        //
        // We are draining, so we can and must convert any remaining input samples to output samples.
        //
        IF_FALSE_GOTO( !m_InputSampleQueue.IsEmpty(), MF_E_UNEXPECTED );
        *pdwFlags = MFT_OUTPUT_STATUS_SAMPLE_READY;
    }
    else if( !m_InputSampleQueue.IsEmpty() )
    {
        {
            //
            // We have at least one input sample in the queue
            // so we can and must convert any remaining input samples to output samples.
            //
            *pdwFlags = MFT_OUTPUT_STATUS_SAMPLE_READY;
        }
    }
    else
    {
        *pdwFlags = 0;    // We have no input or output samples, so we obviously don't have a sample ready.
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::OnSetD3DManager( _In_ IUnknown *pUnk )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    HRESULT hr = S_OK;

    IF_NULL_GOTO( pUnk, E_INVALIDARG );

    m_spDXGIDeviceMgr = nullptr;
    IF_FAILED_GOTO( pUnk->QueryInterface( IID_PPV_ARGS( &m_spDXGIDeviceMgr ) ) );

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::OnMediaEventMessage( _In_ ULONG_PTR ulParam )
{
    HRESULT         hr      = S_OK;
    MediaEventType  met     = MEUnknown;
    IMFMediaEvent  *pEvent  = reinterpret_cast<IMFMediaEvent*>( ulParam );
    PROPVARIANTWRAP propvar;

    IF_FAILED_GOTO( pEvent->GetType( &met ) );
    if( met != MEPolicySet )
    {
        goto done;
    }

    IF_FAILED_GOTO( pEvent->GetValue( &propvar ) );
    IF_FALSE_GOTO( propvar.vt == VT_UI4, MF_E_UNEXPECTED );

    if( propvar.ulVal == 0 )
    {
        //
        // Initial policy notification received.
        //
        IF_FAILED_GOTO( this->Enable() );
        IF_FAILED_GOTO( this->StartInternal( c_fNotStartOfStream ) );
    }
    else
    {
        // Dynamic policy notification received.
        if( this->IsWaitingForPolicyNotification() )
        {
            for( DWORD iPolicyId = 0; iPolicyId < m_rgdwPolicyIds.GetSize(); iPolicyId++ )
            {
                if( m_rgdwPolicyIds[ iPolicyId ] == propvar.ulVal )
                {
                    IF_FAILED_GOTO( m_rgdwPolicyIds.RemoveAt( iPolicyId ) );
                    break;
                }
            }

            if( !this->IsWaitingForPolicyNotification() )
            {
                //
                // All notifications received.  MFT no longer waiting.
                //
                IF_FAILED_GOTO( this->StartInternal( c_fNotStartOfStream ) );
            }
        }
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::InternalProcessInput( _In_ IMFSample *pSample, _In_ DWORD dwFlags )
{
    HRESULT hr = S_OK;

    IF_NULL_GOTO( pSample, E_INVALIDARG );

    IF_FAILED_GOTO( this->GetEnabledState() );

    //
    // If ProcessInput is called again and we got an end of stream
    // message, it means that Start has been called again, so we need
    // to cleanup our existing state and start again
    //
    if( m_fEndOfStreamReached )
    {
        m_guidKid = GUID_NULL;
        m_guidLid = GUID_NULL;

        m_fEndOfStreamReached = FALSE;
        m_rgPendingPolicyChangeObjects.RemoveAll();

        while( !m_OutputSampleQueue.IsEmpty() )
        {
            ComPtr<IMFSample> spSample;
            (void)m_OutputSampleQueue.RemoveHead( &spSample );
        }

        while( !m_InputSampleQueue.IsEmpty() )
        {
            ComPtr<IMFSample> spSample;
            (void)m_InputSampleQueue.RemoveHead( &spSample );
        }
        m_fInputSampleQueueDraining = FALSE;
    }

    {
        DWORD dwInputStatusFlags = 0;
        IF_FAILED_GOTO( this->InternalGetInputStatus( &dwInputStatusFlags ) );
        IF_FALSE_GOTO( dwInputStatusFlags == MFT_INPUT_STATUS_ACCEPT_DATA, MF_E_NOTACCEPTING );
    }

    {
        ComPtr<IMFSample> spSample( pSample );
        IF_FAILED_GOTO( _MoveSampleIntoQueue( m_InputSampleQueue, spSample ) );
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::OnProcessEvent( _In_ DWORD dwInputStreamID, _In_ IMFMediaEvent *pEvent )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;

    HRESULT          hr              = S_OK;
    MediaEventType   met             = MEUnknown;
    BOOL             fDoNotPropagate = FALSE;
    BYTE            *pbSystemID      = nullptr;
    PROPVARIANTWRAP  propvar;

    IF_NULL_GOTO( pEvent, E_INVALIDARG );

    //
    // MF itself will make sure we don't get a ProcessEvent
    // call that was not meant for us, so just ignore
    // dwInputStreamID except for making sure it's zero.
    //
    IF_FALSE_GOTO( 0 == dwInputStreamID, E_INVALIDARG );

    IF_FAILED_GOTO( this->GetEnabledState() );

    IF_FAILED_GOTO( pEvent->GetType( &met ) );

    if( MEContentProtectionMetadata == met )
    {
        ComPtr<IMFAttributes> spAttributes;
        UINT32             cbSize;
        BOOL fProcessEvent = FALSE;

        IF_FAILED_GOTO( pEvent->QueryInterface( IID_PPV_ARGS( &spAttributes ) ) );

        hr = spAttributes->GetAllocatedBlob( MF_EVENT_STREAM_METADATA_SYSTEMID, &pbSystemID, &cbSize );

        if( SUCCEEDED( hr ) )
        {
            IF_FALSE_GOTO( cbSize == sizeof( GUID ), MF_E_UNEXPECTED );

            if( 0 == memcmp( &CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID, pbSystemID, sizeof( GUID ) ) )
            {
                fProcessEvent = TRUE; // Downstream MFTs shouldn't handle this event
            }
        }
        else
        {
            //
            // If we don't find system ID explicitly, we always process the event
            //
            if( MF_E_ATTRIBUTENOTFOUND == hr )
            {
                hr = S_OK;
                fProcessEvent = TRUE; // Downstream MFTs shouldn't handle this event
            }

            IF_FAILED_GOTO( hr );
        }

        if( fProcessEvent )
        {
            //
            // A manifest with additional protection system data was found.
            // If either/both of the following attributes are set, we could
            // (in theory) set up any decrypter(s) for the given data in
            // advance of any samples that need said decrypter(s).
            //
            // spAttributes->GetAllocatedBlob( MF_EVENT_STREAM_METADATA_CONTENT_KEYIDS, &pbKids, &cbKids );
            // spAttributes->GetAllocatedBlob( MF_EVENT_STREAM_METADATA_KEYDATA, &pbKids, &cbKids );
            //
            // Doing so can prevent pipeline stalls if decrypter setup takes a long time.
            //
            fDoNotPropagate = TRUE; // Downstream MFTs shouldn't handle this event
        }
    }

    //
    //  All other event types are considered to have been examined and ignored
    //  According to MSDN http://msdn.microsoft.com/en-us/library/windows/desktop/ms695394(v=vs.85).aspx
    //  ProcessEvent should only return E_NOTIMPL in the case NO events are
    //  handled by this method.
    //  In our case we should be returning S_OK
    //
    hr = S_OK;

done:
    CoTaskMemFree( pbSystemID );
    if( SUCCEEDED( hr ) && fDoNotPropagate )
    {
        hr = MF_S_TRANSFORM_DO_NOT_PROPAGATE_EVENT;
    }
    return hr;
}

void Cdm_clearkey_IMFTransform::NotifyMediaKeyStatus()
{
    HRESULT hr = S_OK;
    ComPtr<IMFAttributes> spIMFAttributesITA;
    ComPtr<IMFAttributes> spIMFAttributesTI;
    ComPtr<IMFGetService> spIMFGetServiceCDM;
    ComPtr<IUnknown> spIUnknownUnused;

    IF_FAILED_GOTO( m_spIMFAttributes->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_ITA_TO_MFT, IID_PPV_ARGS( &spIMFAttributesITA ) ) );
    IF_FAILED_GOTO( spIMFAttributesITA->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_TI_TO_ITA, IID_PPV_ARGS( &spIMFAttributesTI ) ) );
    IF_FAILED_GOTO( spIMFAttributesTI->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_TI, IID_PPV_ARGS( &spIMFGetServiceCDM ) ) );

    //
    // Refer to comments in Cdm_clearkey_IMFContentDecryptionModule::GetService.
    //
    IF_FAILED_GOTO( spIMFGetServiceCDM->GetService( MF_CONTENTDECRYPTIONMODULE_SERVICE, CLEARKEY_GUID_NOTIFY_KEY_STATUS, (LPVOID*)spIUnknownUnused.GetAddressOf() ) );

done:
    return;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::Commit()
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    return m_spIPropertyStore->Commit();
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetAt( _In_ DWORD iProp, _Out_ PROPERTYKEY *pkey )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    return m_spIPropertyStore->GetAt( iProp, pkey );
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetCount( _Out_ DWORD *pcProps )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    return m_spIPropertyStore->GetCount( pcProps );
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetValue( _In_ REFPROPERTYKEY key, _Out_ PROPVARIANT *pv )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    return m_spIPropertyStore->GetValue( key, pv );
}

STDMETHODIMP Cdm_clearkey_IMFTransform::SetValue( _In_ REFPROPERTYKEY key, _In_ REFPROPVARIANT var )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    return m_spIPropertyStore->SetValue( key, var );
}

STDMETHODIMP Cdm_clearkey_IMFTransform::Shutdown()
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    HRESULT hr = S_OK;

    if( this->GetState() == CLEARKEY_MFT_STATE_SHUTDOWN )
    {
        IF_FAILED_GOTO( MF_E_SHUTDOWN );
    }

    // If we've locked a work queue, unlock it.  (Note that Shutdown is also called by our destructor.)
    (void)this->SetWorkQueueValue( MFASYNC_CALLBACK_QUEUE_MULTITHREADED, 0 );

    (void)this->StopInternal();
    (void)this->FlushInternal( stateLock );

    if( m_spIMFAttributes != nullptr )
    {
        (void)m_spIMFAttributes->SetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_ITA_TO_MFT, nullptr );
    }

    SHUTDOWN_EVENT_GENERATOR;

done:
    this->SetState( CLEARKEY_MFT_STATE_SHUTDOWN );
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::GetShutdownStatus(
    MFSHUTDOWN_STATUS *pStatus )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    HRESULT hr = S_OK;

    IF_NULL_GOTO( pStatus, E_INVALIDARG );
    if( this->GetState() != CLEARKEY_MFT_STATE_SHUTDOWN )
    {
        IF_FAILED_GOTO( MF_E_INVALIDREQUEST );
    }

    *pStatus = MFSHUTDOWN_COMPLETED;

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::ProcessEvent(
    _In_    DWORD           dwInputStreamID,
    _In_    IMFMediaEvent*  pEvent )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    CLEARKEY_MFT_AUTOLOCK_INPUT;
    HRESULT hr              = S_OK;
    BOOL    fPropagateEvent = FALSE;

    IF_FALSE_GOTO( dwInputStreamID == 0, E_INVALIDARG );

    // if the input state indicates that we can process the event now AND there are no samples, process the event immediately;
    //      otherwise just queue the event for processing later
    if( SUCCEEDED( this->CheckInputState() ) && GetSampleCount() == 0 )
    {
        //
        // We can process the event immediately since the input queue is empty
        //
        IF_FAILED_GOTO( this->ProcessEventInternal( pEvent, c_fYesFromProcessEvent, &fPropagateEvent ) );
    }
    else
    {
        //
        // Add the event to the input queue, it will be processed serially with the samples
        //
        IF_FAILED_GOTO( this->QueueInputSampleOrEvent( pEvent ) );

        // Trigger the processing loop
        IF_FAILED_GOTO( this->SignalStateChanged() );
    }

    if( !fPropagateEvent )
    {
        // We will propagate the events ourselves since pipeline can't correlate input samples to output samples for async MFTs
        hr = MF_S_TRANSFORM_DO_NOT_PROPAGATE_EVENT;
    }
    else
    {
        // Allow the pipeline to propagate the event.  This is necessary when we have no data buffered
        hr = S_OK;
    }

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::ProcessInput( _In_ DWORD dwInputStreamIndex, _In_ IMFSample *pSample, _In_ DWORD dwFlags )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    CLEARKEY_MFT_AUTOLOCK_INPUT;
    HRESULT hr = S_OK;

    IF_FALSE_GOTO( dwInputStreamIndex == 0, MF_E_INVALIDSTREAMNUMBER );

    //
    // Decrement "need input" count since we received a sample from the pipeline
    // Pipeline decrements its count before making the call - thus we should decrement our count regardless of whether
    //      we succeeded or failed
    //
    if( m_dwNeedInputCount != 0 )
    {
        m_dwNeedInputCount--;
    }

    IF_FAILED_GOTO( this->CheckInputState() );

    //
    // Add the sample to the input queue
    //
    IF_FAILED_GOTO( this->QueueInputSampleOrEvent( pSample ) );

    //
    // We bound the total # of input samples AND requests
    // As such, there is no reason to request input here as what used to be a request is now a sample in the input queue
    //

    //
    // Trigger the processing loop
    //
    IF_FAILED_GOTO( this->SignalStateChanged() );

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::ProcessOutput(
    _In_    DWORD                   dwFlags,
    _In_    DWORD                   cOutputBufferCount,
    _Inout_ MFT_OUTPUT_DATA_BUFFER *pOutputSamples,
    _Out_   DWORD                  *pdwStatus )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    HRESULT hr                  = S_OK;
    BOOL    fReturnStreamChange = FALSE;
    MFT_OUTPUT_DATA_BUFFER *pOutputBuffer = nullptr;

    IF_NULL_GOTO( pOutputSamples, E_INVALIDARG );
    IF_NULL_GOTO( pdwStatus, E_INVALIDARG );
    IF_FALSE_GOTO( cOutputBufferCount == 1, E_INVALIDARG );
    *pdwStatus = 0;

    // Validate that the caller didn't provide a sample (we're always the allocator)
    IF_FALSE_GOTO( pOutputSamples[ 0 ].pSample == nullptr, E_INVALIDARG );

    IF_FAILED_GOTO( this->CheckState() );

    {
        CLEARKEY_MFT_AUTOLOCK_OUTPUT;
        if( !this->IsOutputDataBufferQueueEmpty() )
        {
            //
            // Get the output buffers from our queue
            //
            IF_FAILED_GOTO( this->RemoveFromOutputDataBufferQueue( stateLock, outputLock, &pOutputBuffer ) );

            if( pOutputBuffer->dwStatus == MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE )
            {
                // Output media type was probably invalidated by the format change

                // We will notify the pipeline of the format change by returning MF_E_TRANSFORM_STREAM_CHANGE
                fReturnStreamChange = TRUE;
            }

            {
                //
                // Copy into caller provided structure
                //
                *pOutputSamples = *pOutputBuffer;
                pOutputBuffer->pEvents = nullptr;         // caller's structure has the ref. now
                pOutputBuffer->pSample = nullptr;         // caller's structure has the ref. now
            }
        }
        else
        {
            {
                //
                // No sample to deliver, return E_UNEXPECTED as per async MFT contract
                //
                // The way we can get into this situation is by the output media type being changed (which flushes our output sample queue)
                // In such a case, we may also need to "kick" the processing loop as it may have stalled due to the queue being full before the flush
                //

                IF_FAILED_GOTO( this->SignalStateChanged() );
                IF_FAILED_GOTO( E_UNEXPECTED );
            }
        }

        //
        // Trigger the processing loop
        //
        IF_FAILED_GOTO( this->SignalStateChanged() );
    }

done:

    if( pOutputBuffer != nullptr )
    {
        SAFE_RELEASE( pOutputBuffer->pEvents );
        SAFE_RELEASE( pOutputBuffer->pSample );
        CoTaskMemFree( pOutputBuffer );
    }

    if( fReturnStreamChange && ( SUCCEEDED( hr ) ) )
    {
        // Pipeline expects MF_E_TRANSFORM_STREAM_CHANGE
        hr = MF_E_TRANSFORM_STREAM_CHANGE;
    }

    return hr;
}


HRESULT Cdm_clearkey_IMFTransform::StartInternal( _In_ BOOL fStartOfStream )
{
    HRESULT hr = S_OK;
    CLEARKEY_MFT_STATE eState;

    if( fStartOfStream && m_fWaitingForStartOfStream )
    {
        CLEARKEY_MFT_AUTOLOCK_OUTPUT;

        if( this->IsOutputDataBufferQueueEmpty() && m_fOutputFormatChange )
        {
            //
            // There was a pending format change but it was never completed by the pipeline (either flushed or simply ignored for some reason)
            // Now that we're starting again, we better requeue the format change notification
            //

            IF_FAILED_GOTO( this->PropagateOutputTypeChange() );
        }
    }

    //
    // If we're stopped, queue a processing loop workitem
    // Otherwise, we're already running and have nothing to do
    //
    eState = this->GetState();

    if( ( eState == CLEARKEY_MFT_STATE_STOPPED || CLEARKEY_MFT_STATE_IS_A_WAITING_STATE( eState ) ) && !this->IsWaitingForPolicyNotification() )
    {
        //
        // We may not be flushed which means the MFT may actually have output to deliver
        //
        this->SetState( CLEARKEY_MFT_STATE_GETOUTPUT );

        if( m_fLoopRunning )
        {
            //
            // Trigger the processing loop
            //
            IF_FAILED_GOTO( this->SignalStateChanged() );
        }
        else
        {
            ComPtr<IMFAsyncResult> spResult;
            m_fLoopRunning = TRUE;

            //
            // Queue a processing loop workitem
            //
            IF_FAILED_GOTO( MFCreateAsyncResult( nullptr, &m_xProcessingLoop, nullptr, &spResult ) );

            this->SetProcessingLoopState( CLEARKEY_MFT_PROCESSING_LOOP_STATE_OTHER_ITEMS_IN_QUEUE_START_INTERNAL );

            IF_FAILED_GOTO( MFPutWorkItemEx2( m_dwWorkQueueID, m_lWorkItemPriority, spResult.Get() ) );
        }
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::StopInternal()
{
    HRESULT hr = S_OK;

    if( this->GetState() != CLEARKEY_MFT_STATE_STOPPED )
    {
        //
        // Async MFTs are supposed to reset the outstanding "need input" count
        //      when they are stopped
        //
        m_dwNeedInputCount = 0;

        //
        // Stopping should get us out of the draining state
        //
        IF_FAILED_GOTO( this->FinishDrain( c_fDoNotSendEvent ) );

        //
        // Set our state to stopped and trigger the processing loop
        //   (which will simply exit when it detects the state)
        //
        this->SetState( CLEARKEY_MFT_STATE_STOPPED );
        IF_FAILED_GOTO( this->SignalStateChanged() );
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::FlushInternal( _In_ SyncLock2& stateLock )
{
    HRESULT hr = S_OK;

    //
    // Flush the input queue
    //
    HRESULT hrInput = this->FlushInputQueue( stateLock );

    {
        CLEARKEY_MFT_AUTOLOCK_OUTPUT;

        //
        // Flush output event queue
        //
        this->FlushOutputEventQueue();

        //
        // Flush output sample queue
        //
        this->FlushOutputSampleQueue( stateLock, outputLock, c_fDoNotKeepEvents );

        IF_FAILED_GOTO( this->FinishDrain( c_fDoNotSendEvent ) );

        //
        // Don't request any more input until the next "start of stream"
        //
        m_fWaitingForStartOfStream = TRUE;
    }

    IF_FAILED_GOTO( this->OnFlush() );

    IF_FAILED_GOTO( hrInput );

done:
    return hr;
}


void Cdm_clearkey_IMFTransform::FlushOutputEventQueue()
{
    while( !m_OutputEventQueue.IsEmpty() )
    {
        ComPtr<IMFMediaEvent> spEvent;
        m_OutputEventQueue.RemoveHead( &spEvent );
    }
}

void Cdm_clearkey_IMFTransform::FlushOutputSampleQueue( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock, _In_ BOOL fKeepEvents )
{
    CTPtrList<MFT_OUTPUT_DATA_BUFFER> oTempEventQueue;
    oTempEventQueue.Initialize( c_dwMaxOutputSamples );

    while( !IsOutputDataBufferQueueEmpty() )
    {
        MFT_OUTPUT_DATA_BUFFER *pOutputBuffer = nullptr;
        (void)this->RemoveFromOutputDataBufferQueue( stateLock, outputLock, &pOutputBuffer );

        if( pOutputBuffer != nullptr )
        {
            if( pOutputBuffer->pEvents != nullptr && fKeepEvents )
            {
                // An event was found in the queue and should be kept.
                oTempEventQueue.AddTail( pOutputBuffer );
            }
            else
            {
                SAFE_RELEASE( pOutputBuffer->pEvents );
                SAFE_RELEASE( pOutputBuffer->pSample );
                CoTaskMemFree( pOutputBuffer );
            }
        }
    }

    // Any events found in the output sample queue
    // should be pushed back to the OutputDataBufferQueue
    while( !oTempEventQueue.IsEmpty() )
    {
        MFT_OUTPUT_DATA_BUFFER *pOutputBuffer = nullptr;
        // Found an event, don't flush it
        (void)oTempEventQueue.RemoveHead( &pOutputBuffer );
        m_OutputDataBufferQueue.AddTail( pOutputBuffer );
    }
}

void Cdm_clearkey_IMFTransform::SetState( _In_ CLEARKEY_MFT_STATE eState )
{
    m_eState = eState;
}

DWORD Cdm_clearkey_IMFTransform::GetWorkQueueValue()
{
    return m_dwWorkQueueID;
}

HRESULT Cdm_clearkey_IMFTransform::SetWorkQueueValue( _In_ DWORD dwMultithreadedWorkQueueId, _In_ LONG  lWorkItemBasePriority )
{
    HRESULT hr = S_OK;

    if( m_dwWorkQueueID != dwMultithreadedWorkQueueId )
    {
        // If we've locked before, unlock.
        if( m_dwWorkQueueID != MFASYNC_CALLBACK_QUEUE_MULTITHREADED )
        {
            this->SetLongRunning( c_fNotLongRunning );   // Make sure we don't leave ourselves in the long running state.
            IF_FAILED_GOTO( MFUnlockWorkQueue( m_dwWorkQueueID ) );
            m_dwWorkQueueID = MFASYNC_CALLBACK_QUEUE_MULTITHREADED;
        }

        // If we should lock, do so.
        if( dwMultithreadedWorkQueueId != MFASYNC_CALLBACK_QUEUE_MULTITHREADED )
        {
            IF_FAILED_GOTO( MFLockWorkQueue( dwMultithreadedWorkQueueId ) );
            m_dwWorkQueueID = dwMultithreadedWorkQueueId;
        }
    }
    m_lWorkItemPriority = lWorkItemBasePriority;

done:
    return hr;
}

void Cdm_clearkey_IMFTransform::SetLongRunning( _In_ BOOL fLongRunning )
{
    if( m_fLongRunning != fLongRunning )
    {
        HRESULT hr = RtwqSetLongRunning( m_dwWorkQueueID, fLongRunning );
        if( FAILED( hr ) )
        {
            // RtwqSetLongRunning failed.  Ignoring error and continuing.
            goto done;
        }
        m_fLongRunning = fLongRunning;
    }

done:
    return;
}

void Cdm_clearkey_IMFTransform::SetProcessingLoopState( _In_ CLEARKEY_MFT_PROCESSING_LOOP_STATE eProcessingLoopState )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    m_eProcessingLoopState = eProcessingLoopState;
}

HRESULT Cdm_clearkey_IMFTransform::PropagateOutputTypeChange()
{
    HRESULT hr = S_OK;

    //
    // We need to propagate the format change on our output
    // - Add the format change notification to our output queue
    // - Mark all streams as being at a format change
    //

    MFT_OUTPUT_DATA_BUFFER *pOutputBuffer = nullptr;
    IF_NULL_GOTO( pOutputBuffer = (MFT_OUTPUT_DATA_BUFFER*)CoTaskMemAlloc( sizeof( MFT_OUTPUT_DATA_BUFFER ) ), E_OUTOFMEMORY );
    ZeroMemory( pOutputBuffer, sizeof( MFT_OUTPUT_DATA_BUFFER ) );

    pOutputBuffer->dwStatus = MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE;

    IF_FAILED_GOTO( this->AddToOutputDataBufferQueue( pOutputBuffer ) );
    pOutputBuffer = nullptr;             // structure is owned by the output queue now

    //
    // Generate "transform have output" to inform pipeline of the format change
    //
    IF_FAILED_GOTO( this->QueueEvent( METransformHaveOutput, GUID_NULL, S_OK, nullptr ) );

done:
    CoTaskMemFree( pOutputBuffer );
    return hr;
}


HRESULT Cdm_clearkey_IMFTransform::CheckState()
{
    HRESULT hr = S_OK;

    CLEARKEY_MFT_STATE eState = this->GetState();

    if( eState == CLEARKEY_MFT_STATE_SHUTDOWN )
    {
        IF_FAILED_GOTO( MF_E_SHUTDOWN );
    }
    else if( eState == CLEARKEY_MFT_STATE_STOPPED )
    {
        IF_FAILED_GOTO( MF_E_INVALIDREQUEST );
    }

    //
    // If we have a cached error code, return it now
    //
    IF_FAILED_GOTO( m_hrError );

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::CheckInputState()
{
    HRESULT hr = S_OK;

    IF_FAILED_GOTO( this->CheckState() );

    if( this->IsDraining() )
    {
        IF_FAILED_GOTO( MF_E_NOTACCEPTING );
    }

done:
    return hr;
}

void Cdm_clearkey_IMFTransform::ProcessingLoop( _In_ IMFAsyncResult *pResult )
{
    HRESULT hr = S_OK;
    BOOL fScheduleItemWithTimeout = FALSE;

    this->SetProcessingLoopState( CLEARKEY_MFT_PROCESSING_LOOP_STATE_NOT_WAITING );

    do
    {
        //
        // We're going to check our state and act on it.
        //
        ResetEvent( m_hStateChanged );

        CLEARKEY_MFT_AUTOLOCK_STATE;
        this->SetLongRunning( c_fYesLongRunning );

        CLEARKEY_MFT_STATE eState = this->GetState();

        if( eState == CLEARKEY_MFT_STATE_STOPPED
         || eState == CLEARKEY_MFT_STATE_SHUTDOWN
         || CLEARKEY_MFT_STATE_IS_A_WAITING_STATE( eState ) )
        {
            //
            // If we aren't running, bail
            //
            m_fLoopRunning = FALSE;
            goto done;
        }

        // If we have any pending events not associated with a specific output, go ahead
        // and add them directly now. This allows async handling of policy notification
        // events.
        // HRESULT code here is for debugging only, not changing state-machine.
        HRESULT hrAppend = this->AddPendingEventsToOutputSampleQueue();
        (void)hrAppend;

        //
        // If we were flushed\drained, don't push data until the next "start streaming" call
        //
        if( m_fWaitingForStartOfStream )
        {
            // Waiting for 'start of stream', delay processing until we're told to start again.
            break;
        }

        //
        // Check whether we need to provide input to the MFT
        //
        if( this->GetState() == CLEARKEY_MFT_STATE_FEEDINPUT )
        {
            if( ( this->GetSampleCount() == 0 ) && !this->IsDraining() )
            {
                //
                // We need to wait for input from upstream
                // If we're in non-queuing mode, we need to request input now
                //
                if( m_dwNeedInputCount == 0 )
                {
                    IF_FAILED_GOTO( this->RequestInputForStream() );
                }

                break;
            }

            IF_FAILED_GOTO( this->FeedInput( stateLock ) );
        }

        //
        // NOT an else-if: FeedInput may have changed our state.
        //
        if( this->GetState() == CLEARKEY_MFT_STATE_GETOUTPUT )
        {
            BOOL fCallGetOutput = FALSE;

            //
            // Get output from MFT unless the output queue is already full
            // GetOutput must be called OUTSIDE the output lock
            //
            {
                CLEARKEY_MFT_AUTOLOCK_OUTPUT;

                if( this->GetOutputDataBufferQueueLength() >= c_dwMaxOutputSamples )
                {
                    // Output queue is full, delay processing until pipeline calls ProcessOutput()
                    break;
                }
                else
                {
                    fCallGetOutput = TRUE;
                }
            }

            if( fCallGetOutput )
            {
                IF_FAILED_GOTO( this->GetOutput( stateLock ) );
                continue;
            }
        }
    } while( TRUE );

    //
    // Check if we're already signalled (in which case we can skip the waiting workitem)
    //
    if( fScheduleItemWithTimeout )
    {
        // State handle is NOT signalled, queue the same work item to run after 100ms.

        this->SetProcessingLoopState( CLEARKEY_MFT_PROCESSING_LOOP_STATE_RESOLVE_WAITING_STATE_100MS_RETRY );

        //
        // We're waiting for something beyond our control, e.g. reacquiring hardware resources.
        // Queue a workitem that will reinvoke this same function after 100ms which will then
        // look to see whether what we're waiting on is completed or it's still running.
        // We'll keep hitting this codepath until what we're waiting on is completed,
        // something fails which halts us, or our state changes (e.g. because playback gets stopped).
        // Since we DON'T have something to do immediately, we stop long running.
        // (Note: since we're scheduling the work item in the same queue i.e. thread on which we're currently
        // executing, it does not matter whether we queue the item first or stop long running first.)
        //
        {
            CLEARKEY_MFT_AUTOLOCK_STATE;
            this->SetLongRunning( c_fNotLongRunning );
        }
        IF_FAILED_GOTO( MFScheduleWorkItemEx( pResult, -100, nullptr ) );
    }
    else if( WaitForSingleObject( m_hStateChanged, 0 ) == WAIT_OBJECT_0 )
    {
        // State handle is already signalled, queue a workitem to restart the processing loop.

        this->SetProcessingLoopState( CLEARKEY_MFT_PROCESSING_LOOP_STATE_OTHER_ITEMS_IN_QUEUE_AFTER_STATE_CHANGED );

        //
        // We were already signalled by something outside this function while we were
        // in the middle of executing the last loop iteration (e.g. MF gave us new input samples),
        // so just restart the loop to process the new state.
        // Since we DO have something to do immediately, we do NOT stop long running.
        //
        IF_FAILED_GOTO( MFPutWorkItemEx2( m_dwWorkQueueID, m_lWorkItemPriority, pResult ) );
    }
    else
    {
        // State handle is NOT signalled, queue a waiting workitem to wait for a state change.

        this->SetProcessingLoopState( CLEARKEY_MFT_PROCESSING_LOOP_STATE_A_STATE_CHANGE );

        //
        // Now we wait for our state to change again (e.g. new samples from MF in our input queue,
        // our own decrypt finished and put decrypted samples in our output queue, playback ended, etc).
        // Queue a wait for the stream state changed event to be signalled.
        // Since we DON'T have something to do immediately, we stop long running.
        // (Note: since we're scheduling the work item in the same queue i.e. thread on which we're currently
        // executing, it does not matter whether we queue the item first or stop long running first.)
        //
        {
            CLEARKEY_MFT_AUTOLOCK_STATE;
            this->SetLongRunning( c_fNotLongRunning );
        }
        IF_FAILED_GOTO( MFPutWaitingWorkItem( m_hStateChanged, m_lWorkItemPriority, pResult, nullptr ) );
    }

done:

    //
    // Propagate errors via subsequent return from our ProcessInput()\ProcessOutput()
    //
    if( FAILED( hr ) )
    {
        {
            CLEARKEY_MFT_AUTOLOCK_STATE;
            this->SetLongRunning( c_fNotLongRunning );
        }

        this->SetProcessingLoopState( CLEARKEY_MFT_PROCESSING_LOOP_STATE_WAITING_FOR_START );

        // E_UNEXPECTED is a special return code for async MFTs. If the underlying error is E_UNEXPECTED remap to MF_E_UNEXPECTED
        if( hr == E_UNEXPECTED )
        {
            hr = MF_E_UNEXPECTED;
        }

        {
            CLEARKEY_MFT_AUTOLOCK_STATE;
            CLEARKEY_MFT_AUTOLOCK_OUTPUT;

            m_hrError = hr;

            // If the output queue is empty, pipeline will not make a call to ProcessOutput()
            // Thus, we signal METransformHaveOutput to trigger the pipeline to make this call (so that we can propagate an error)

            if( this->IsOutputDataBufferQueueEmpty() )
            {
                (void)this->QueueEvent( METransformHaveOutput, GUID_NULL, m_hrError, nullptr );  // Don't infinite loop by jumping to done if this fails!
            }
        }
    }
    return;
}

HRESULT Cdm_clearkey_IMFTransform::ProcessEventInternal( _In_ IMFMediaEvent *pEvent, _In_ BOOL fFromProcessEventCall, _Inout_opt_ BOOL *pfPropagateEvent )
{
    HRESULT hr = S_OK;

    MediaEventType met = MEUnknown;

    IF_FAILED_GOTO( pEvent->GetType( &met ) );

    if( met == METransformMarker )
    {
        //
        // This is the METransformMarker event that we queued internally to serialize with input samples
        // Queue the event on the event generator now
        //

        IF_FAILED_GOTO( m_eventGenerator.QueueEvent( pEvent ) );
    }
    else
    {
        hr = this->OnProcessEvent( 0, pEvent );
        if( hr == E_NOTIMPL )
        {
            // MFT ignores the events but we still need to propagate them to output
            hr = S_OK;
        }
        else if( hr == MF_S_TRANSFORM_DO_NOT_PROPAGATE_EVENT )
        {
            // MFT will handle the event internally, we don't need to propagate it
            goto done;
        }

        IF_FAILED_GOTO( hr );

        {
            CLEARKEY_MFT_AUTOLOCK_OUTPUT;

            if( fFromProcessEventCall && this->IsOutputDataBufferQueueEmpty() )
            {
                //
                // If this is called from IMFTransform::ProcessEvent and we have no output samples queued up
                // then tell the pipeline to propagate the event immediately rather than waiting until we have
                // more output.
                //

                hr = S_OK;
                if( pfPropagateEvent != nullptr )
                {
                    *pfPropagateEvent = TRUE;
                }
            }
            else
            {
                //
                // Queue the event to the output queue, it will be propagated to the pipeline via ProcessOutput() later
                // The event will be propagated with the next sample that MFT gives out via ProcessOutput
                //

                IF_NULL_GOTO( m_OutputEventQueue.AddTail( pEvent ), E_OUTOFMEMORY );
                pEvent->AddRef();      // queue now holds a reference to the event
            }
        }
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::FeedInput( _In_ SyncLock2& stateLock )
{
    HRESULT hr = S_OK;

    //
    // Get input sample, event or new media type
    //
    DWORD cItemsRemaining;
    for( ComPtr<IUnknown> spUnk; this->GetNextInputItem( &spUnk, &cItemsRemaining ); /* no-op */ )
    {
        ComPtr<IMFSample> spSample;
        ComPtr<IMFMediaEvent> spEvent;
        ComPtr<IMFMediaType> spMediaType;

        if( SUCCEEDED( spUnk->QueryInterface( IID_PPV_ARGS( &spSample ) ) ) )
        {
            if( !m_spInputMediaType )
            {
                hr = MF_E_TRANSFORM_TYPE_NOT_SET;
                goto done;
            }

            IF_FAILED_GOTO( this->ProcessMessageStateLockHeld( stateLock, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0 ) );

            IF_FAILED_GOTO( this->InternalProcessInput( spSample.Get(), 0 ) );
            break;
        }
        else if( SUCCEEDED( spUnk->QueryInterface( IID_PPV_ARGS( &spEvent ) ) ) )
        {
            IF_FAILED_GOTO( this->ProcessEventInternal( spEvent.Get(), c_fNotFromProcessEvent, nullptr ) );
        }
        else if( SUCCEEDED( spUnk->QueryInterface( IID_PPV_ARGS( &spMediaType ) ) ) )
        {
            IF_FAILED_GOTO( this->OnDrain() );

            m_spPendingInputMediaType = spMediaType;
        }
        else
        {
            // Unexpected object in the queue
            IF_FAILED_GOTO( MF_E_UNEXPECTED );
        }
    }

    if( !IsDraining() )
    {
        //
        // If we processed something from the input queue, we can request more input
        //
        IF_FAILED_GOTO( this->RequestInput() );
    }

    if( this->GetSampleCount() == 0 )
    {
        if( this->GetDrainState() == CLEARKEY_MFT_DRAIN_STATE_PREDRAIN )
        {
            //
            // Time to tell the MFT to drain
            //
            IF_FAILED_GOTO( this->OnDrain() );
            this->SetDrainState( CLEARKEY_MFT_DRAIN_STATE_DRAINING );
        }
    }

    this->SetState( CLEARKEY_MFT_STATE_GETOUTPUT );
    hr = S_OK;

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::AddPendingEventsToOutputSampleQueue()
{
    CLEARKEY_MFT_AUTOLOCK_OUTPUT;
    HRESULT hr = S_OK;

    MFT_OUTPUT_DATA_BUFFER* pOutputBuffer = nullptr;

    if( !m_OutputEventQueue.IsEmpty() )
    {
        // Allocate output buffers
        IF_NULL_GOTO( pOutputBuffer = (MFT_OUTPUT_DATA_BUFFER*)CoTaskMemAlloc( sizeof( MFT_OUTPUT_DATA_BUFFER ) ), E_OUTOFMEMORY );
        ZeroMemory( pOutputBuffer, sizeof( MFT_OUTPUT_DATA_BUFFER ) );

        // Create event collection with all the pending events
        IMFCollection* wpEvents = nullptr;
        IF_FAILED_GOTO( MFCreateCollection( &wpEvents ) );
        pOutputBuffer->pEvents = wpEvents;

        while( !m_OutputEventQueue.IsEmpty() )
        {
            ComPtr<IMFMediaEvent> spEvent;
            ComPtr<IUnknown> spEventUnk;

            m_OutputEventQueue.RemoveHead( &spEvent );
            IF_NULL_GOTO( spEvent, MF_E_UNEXPECTED );

            IF_FAILED_GOTO( spEvent->QueryInterface( IID_PPV_ARGS( &spEventUnk ) ) );
            IF_FAILED_GOTO( wpEvents->AddElement( spEventUnk.Get() ) );

            // Event removed from output EVENT queue and added to data buffer's collection
        }

        // Add the sample to our output queue
        IF_FAILED_GOTO( this->AddToOutputDataBufferQueue( pOutputBuffer ) );
        pOutputBuffer = nullptr;             // structure is owned by the output queue now

        // We have a sample, signal "transform have output"
        IF_FAILED_GOTO( this->QueueEvent( METransformHaveOutput, GUID_NULL, S_OK, nullptr ) );
    }

done:
    if( pOutputBuffer != nullptr )
    {
        SAFE_RELEASE( pOutputBuffer->pEvents );
        SAFE_RELEASE( pOutputBuffer->pSample );
        CoTaskMemFree( pOutputBuffer );
    }

    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::GetOutput( _In_ SyncLock2& stateLock )
{
    HRESULT hr       = S_OK;
    DWORD   dwStatus = 0;
    BOOL    fShouldPropagatePendingInputTypeChange = FALSE;
    MFT_OUTPUT_DATA_BUFFER *pOutputBuffer = nullptr;

    {
        CLEARKEY_MFT_AUTOLOCK_OUTPUT;

        // Allocate output buffers
        IF_NULL_GOTO( pOutputBuffer = (MFT_OUTPUT_DATA_BUFFER*)CoTaskMemAlloc( sizeof( MFT_OUTPUT_DATA_BUFFER ) ), E_OUTOFMEMORY );
        ZeroMemory( pOutputBuffer, sizeof( MFT_OUTPUT_DATA_BUFFER ) );

        hr = this->DoProcessSample( stateLock, outputLock, &( pOutputBuffer[ 0 ].pSample ), &( pOutputBuffer[ 0 ].pEvents ), &( pOutputBuffer[ 0 ].dwStatus ), &dwStatus );

        //
        // The MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE case can occur if sample processing was aborted
        // due to a state change during decryption (i.e. while the state lock wasn't held).
        //
        if( SUCCEEDED( hr ) && dwStatus != MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE )
        {
            //
            // We have a sample (or events), signal "transform have output"
            //
            IF_FAILED_GOTO( this->AddToOutputDataBufferQueue( pOutputBuffer ) );
            pOutputBuffer = nullptr;

            // Signal METransformHaveOutput
            IF_FAILED_GOTO( this->QueueEvent( METransformHaveOutput, GUID_NULL, S_OK, nullptr ) );
        }
        else
        {

            BOOL fBackToFeedInput = ( hr == MF_E_TRANSFORM_NEED_MORE_INPUT );

            if( SUCCEEDED( hr ) )
            {
                if( this->IsOutputDataBufferQueueEmpty() && this->GetState() == CLEARKEY_MFT_STATE_GETOUTPUT )
                {
                    //
                    // DoProcessSample returned success with MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE,
                    // the output queue is empty, and we're in CLEARKEY_MFT_STATE_GETOUTPUT.
                    // Switching back to CLEARKEY_MFT_STATE_FEEDINPUT.
                    //
                    fBackToFeedInput = TRUE;
                }
            }

            if( fBackToFeedInput )
            {
                //
                // Switch back to feed input state
                //
                this->SetState( CLEARKEY_MFT_STATE_FEEDINPUT );
                hr = S_OK;

                //
                // If we were draining, draining is done
                //
                if( this->GetDrainState() == CLEARKEY_MFT_DRAIN_STATE_DRAINING )
                {
                    IF_FAILED_GOTO( this->FinishDrain( c_fYesSendEvent ) );
                }

                //
                // If there was a pending input type change, we can propagate it now
                //
                if( m_spPendingInputMediaType )
                {
                    fShouldPropagatePendingInputTypeChange = TRUE;
                }
            }
            else
            {
                IF_FAILED_GOTO( hr );
            }
        }
    }

    if( fShouldPropagatePendingInputTypeChange )
    {
        IF_FAILED_GOTO( this->SetInputTypeLockIsHeld( stateLock, 0, m_spPendingInputMediaType.Get(), 0 ) );
        {
            CLEARKEY_MFT_AUTOLOCK_OUTPUT;
            IF_FAILED_GOTO( this->PropagateOutputTypeChange() );
            m_spPendingInputMediaType = nullptr;
        }
    }

done:
    if( pOutputBuffer != nullptr )
    {
        SAFE_RELEASE( pOutputBuffer->pEvents );
        SAFE_RELEASE( pOutputBuffer->pSample );
        CoTaskMemFree( pOutputBuffer );
    }
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::FinishDrain( _In_ BOOL fSendEvent )
{
    HRESULT hr = S_OK;

    if( this->GetDrainState() != CLEARKEY_MFT_DRAIN_STATE_NONE )
    {
        this->SetDrainState( CLEARKEY_MFT_DRAIN_STATE_NONE );

        if( fSendEvent )
        {
            //
            // Signal drain completion (for every enabled stream) to the pipeline
            //
            ComPtr<IMFMediaEvent> spEvent;
            IF_FAILED_GOTO( MFCreateMediaEvent( METransformDrainComplete, GUID_NULL, S_OK, nullptr, &spEvent ) );
            IF_FAILED_GOTO( spEvent->SetUINT32( MF_EVENT_MFT_INPUT_STREAM_ID, 0 ) );

            IF_FAILED_GOTO( m_eventGenerator.QueueEvent( spEvent.Get() ) );
        }

        //
        // Don't request any more input until the next "start of stream"
        //
        m_fWaitingForStartOfStream = TRUE;
    }
    (void)this->OnDrain();

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::RequestInputForStream()
{
    HRESULT hr = S_OK;

    ComPtr<IMFMediaEvent> spEvent;

    //
    // We need to increment the count *BEFORE* queueing the event as there may be a context switch right after queueing
    // the event and the event will get handled before our count is incremented. This would leave us in an inconsistent state.
    //

    m_dwNeedInputCount++;

    //
    // Request input sample
    //

    IF_FAILED_GOTO( MFCreateMediaEvent( METransformNeedInput, GUID_NULL, S_OK, nullptr, &spEvent ) );
    IF_FAILED_GOTO( spEvent->SetUINT32( MF_EVENT_MFT_INPUT_STREAM_ID, 0 ) );
    IF_FAILED_GOTO( m_eventGenerator.QueueEvent( spEvent.Get() ) );

    // Signalled METransformNeedInput

done:

    if( FAILED( hr ) )
    {
        // We've already incremented the count, need to decrement it here to keep our state consistent
        m_dwNeedInputCount--;
    }

    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::RequestInput()
{
    CLEARKEY_MFT_AUTOLOCK_INPUT;
    HRESULT hr = S_OK;

    if( m_fWaitingForStartOfStream )
    {
        //
        // Don't request any input until we see a "start of stream" from the pipeline
        //

        goto done;
    }

    //
    // Request input samples until we hit our buffering bound in terms of outstanding input samples + requests
    //

    while( ( this->GetSampleCount() + m_dwNeedInputCount ) < c_dwMaxInputSamples )
    {
        IF_FAILED_GOTO( this->RequestInputForStream() );
    }

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::FlushInputQueue( _In_ SyncLock2& stateLock, _Out_opt_ BOOL* pfFormatChange )
{
    CLEARKEY_MFT_AUTOLOCK_INPUT;
    HRESULT hr = S_OK;

    if( pfFormatChange != nullptr )
    {
        *pfFormatChange = FALSE;
    }

    // Flushing should reset the pending input count
    m_dwNeedInputCount = 0;

    while( !m_InputQueue.IsEmpty() )
    {
        ComPtr<IUnknown> spUnk;

        m_InputQueue.RemoveHead( &spUnk );

        ComPtr<IMFMediaType> spMediaType;
        if( SUCCEEDED( spUnk->QueryInterface( IID_PPV_ARGS( &spMediaType ) ) ) )
        {
            //
            // Commit the pending input media type change
            //

            hr = this->SetInputTypeLockIsHeld( stateLock, 0, spMediaType.Get(), 0 );
            if( pfFormatChange != nullptr )
            {
                *pfFormatChange = TRUE;
            }
        }
    }
    IF_FAILED_GOTO( hr );

done:
    return hr;
}

BOOL Cdm_clearkey_IMFTransform::GetNextInputItem( _Out_ IUnknown** ppInputItem, _Out_ DWORD *pcItemsRemaining )
{
    CLEARKEY_MFT_AUTOLOCK_INPUT;

    BOOL fReturn = m_InputQueue.RemoveHead( ppInputItem );
    *pcItemsRemaining = m_InputQueue.GetCount();
    return fReturn;
}

HRESULT Cdm_clearkey_IMFTransform::AddToOutputDataBufferQueue( _In_ MFT_OUTPUT_DATA_BUFFER *pOutputBuffer )
{
    HRESULT hr = S_OK;
    IF_NULL_GOTO( m_OutputDataBufferQueue.AddTail( pOutputBuffer ), E_OUTOFMEMORY );
done:
    return hr;
}

HRESULT Cdm_clearkey_IMFTransform::RemoveFromOutputDataBufferQueue( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock, _Out_ MFT_OUTPUT_DATA_BUFFER **ppOutputBuffer )
{
    HRESULT hr = S_OK;

    this->WaitForPendingOutputs( stateLock, outputLock );     // We may have an output buffer in-flight.  Let it finish.

    if( !m_OutputDataBufferQueue.RemoveHead( ppOutputBuffer ) )
    {
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
done:
    return hr;
}

DWORD Cdm_clearkey_IMFTransform::GetOutputDataBufferQueueLength()
{
    //
    // Note: No need for mathsafe.
    // m_OutputDataBufferQueue.GetCount() is bound by constant DEFAULT_MAX_OUTPUT_SAMPLES.
    // m_cPendingOutputs is bound to zero or one.
    //
    return m_OutputDataBufferQueue.GetCount() + m_cPendingOutputs;
}

BOOL Cdm_clearkey_IMFTransform::IsOutputDataBufferQueueEmpty()
{
    return m_OutputDataBufferQueue.IsEmpty() && ( 0 == m_cPendingOutputs );
}


void Cdm_clearkey_IMFTransform::WaitForPendingOutputs( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock )
{
    //
    // If there is any pending output and the list is actually empty, wait
    // for the pending output to complete so that we can remove it from the queue.
    //
    while( m_OutputDataBufferQueue.GetCount() == 0 && m_cPendingOutputs > 0 )
    {
        CLEARKEY_MFT_OUTPUT_LOCK_RELEASE;
        CLEARKEY_MFT_STATE_LOCK_RELEASE;
        WaitForSingleObject( m_hNotProcessingEvent, INFINITE );
        CLEARKEY_MFT_STATE_LOCK_REACQUIRE;
        CLEARKEY_MFT_OUTPUT_LOCK_REACQUIRE;
    }
}

void Cdm_clearkey_IMFTransform::LongRunningOperationStartUnlockOutputLock( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock, _In_ BOOL fDuringProcessSample, _Out_ BOOL *pfOutputLockIsUnlocked )
{
    if( fDuringProcessSample && m_OutputDataBufferQueue.GetCount() > 0 )
    {
        //
        // We already have samples in the output queue and
        // we're about the perform a long running operation
        // while trying to prepare the next sample.
        // To avoid blocking downstream components from fetching
        // those samples, record that we have an output in
        // progress and unlock the output lock.
        //
        m_cPendingOutputs++;
        ResetEvent( m_hNotProcessingEvent );
        CLEARKEY_MFT_OUTPUT_LOCK_RELEASE;
        CLEARKEY_MFT_STATE_LOCK_RELEASE;
        *pfOutputLockIsUnlocked = TRUE;
    }
    else
    {
        *pfOutputLockIsUnlocked = FALSE;
    }
}

HRESULT Cdm_clearkey_IMFTransform::LongRunningOperationEndRelockOutputLock( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock, _Inout_ BOOL *pfOutputLockIsUnlocked )
{
    if( *pfOutputLockIsUnlocked )
    {
        //
        // record that we no longer have an output in progress
        //
        CLEARKEY_MFT_STATE_LOCK_REACQUIRE;
        CLEARKEY_MFT_OUTPUT_LOCK_REACQUIRE;
        SetEvent( m_hNotProcessingEvent );
        m_cPendingOutputs--;
        *pfOutputLockIsUnlocked = FALSE;
        return this->GetEnabledState();
    }
    return S_OK;
}

STDMETHODIMP Cdm_clearkey_IMFTransform::ProcessMessage( _In_ MFT_MESSAGE_TYPE eMessage, _In_ ULONG_PTR ulParam )
{
    CLEARKEY_MFT_AUTOLOCK_STATE;
    return this->ProcessMessageStateLockHeld( stateLock, eMessage, ulParam );
}

Cdm_clearkey_IMFOutputSchema::Cdm_clearkey_IMFOutputSchema()
{
}

Cdm_clearkey_IMFOutputSchema::~Cdm_clearkey_IMFOutputSchema()
{
}

HRESULT Cdm_clearkey_IMFOutputSchema::RuntimeClassInitialize(
    _In_         REFGUID           guidOriginatorID,
    _In_         REFGUID           guidSchemaType,
    _In_         DWORD             dwConfigurationData )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR( hr );

    IF_FAILED_GOTO( MFCreateAttributes( &m_spIMFAttributes, 0 ) );
    m_guidOriginatorID = guidOriginatorID;
    m_guidSchemaType = guidSchemaType;
    m_dwConfigurationData = dwConfigurationData;

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFOutputSchema::GetSchemaType( _Out_ GUID *pguidSchemaType )
{
    if( pguidSchemaType == nullptr )
    {
        return E_INVALIDARG;
    }
    else
    {
        *pguidSchemaType = m_guidSchemaType;
        return S_OK;
    }
}

STDMETHODIMP Cdm_clearkey_IMFOutputSchema::GetConfigurationData( _Out_ DWORD *pdwVal )
{
    if( pdwVal == nullptr )
    {
        return E_INVALIDARG;
    }
    else
    {
        *pdwVal = m_dwConfigurationData;
        return S_OK;
    }
}

STDMETHODIMP Cdm_clearkey_IMFOutputSchema::GetOriginatorID( _Out_ GUID *pguidOriginatorID )
{
    if( pguidOriginatorID == nullptr )
    {
        return E_INVALIDARG;
    }
    else
    {
        *pguidOriginatorID = m_guidOriginatorID;
        return S_OK;
    }
}


