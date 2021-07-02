//**@@@*@@@****************************************************
//
// Microsoft Windows MediaFoundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//**@@@*@@@****************************************************

#include "stdafx.h"

Cdm_clearkey_IMFContentEnabler::Cdm_clearkey_IMFContentEnabler()
{
}

Cdm_clearkey_IMFContentEnabler::~Cdm_clearkey_IMFContentEnabler()
{
}

HRESULT Cdm_clearkey_IMFContentEnabler::RuntimeClassInitialize()
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR( hr );

    IF_FAILED_GOTO( MFCreateAttributes( &m_spIMFAttributes, 0 ) );

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::GetClassID( _Out_ CLSID *pclsid )
{
    //
    // API not used
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::GetSizeMax( _Out_ ULARGE_INTEGER *pcbSize )
{
    //
    // API not used
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::IsDirty()
{
    //
    // API not used
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::Load( _In_ IStream *pStream )
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);

    IF_FAILED_GOTO( MFDeserializeAttributesFromStream( m_spIMFAttributes.Get(), MF_ATTRIBUTE_SERIALIZE_UNKNOWN_BYREF, pStream ) );

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::Save( _Inout_ IStream *pStm, _In_ BOOL fClearDirty )
{
    //
    // API not used
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::AutomaticEnable()
{
    //
    // API not used
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::Cancel()
{
    //
    // API not used
    //
    return E_NOTIMPL;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::GetEnableData( _Outptr_result_bytebuffer_( *pcbData ) BYTE **ppbData, _Out_ DWORD *pcbData )
{
    return MF_E_NOT_AVAILABLE;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::GetEnableURL(
    _Out_writes_bytes_( *pcchURL )  LPWSTR                 *ppwszURL,
    _Out_                           DWORD                  *pcchURL,
    _Inout_                         MF_URL_TRUST_STATUS    *pTrustStatus )
{
    return MF_E_NOT_AVAILABLE;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::IsAutomaticSupported( _Out_ BOOL *pAutomatic )
{
    if( pAutomatic == nullptr )
    {
        return E_INVALIDARG;
    }
    *pAutomatic = FALSE;
    return S_OK;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::MonitorEnable()
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    HRESULT  hr                 = S_OK;
    BYTE    *pbContentInitData  = nullptr;
    UINT32   cbContentInitData  = 0;
    BYTE    *pbKids             = nullptr;
    DWORD    cbKids             = 0;
    BOOL     fAllKidsHaveKeys   = FALSE;
    ComPtr<IMFAsyncResult> spIMFAsyncResult;

    IF_FALSE_GOTO( !m_fShutdown, MF_E_SHUTDOWN );

    hr = this->GetUnknown( CLEARKEY_GUID_ATTRIBUTE_CE_PENDING_RESULT, IID_PPV_ARGS( &spIMFAsyncResult ) );
    if( hr == MF_E_ATTRIBUTENOTFOUND )
    {
        //
        // This call is during Update before ProcessContentEnabler has been called.
        // We can't complete this CE right now even if we have the key(s), so return S_FALSE to indicate this.
        //
        hr = S_FALSE;
        goto done;
    }
    else if( hr == MF_E_INVALIDTYPE )
    {
        //
        // We've already completed this CE.  There's nothing to do.
        // If we're inside Update, returning S_OK will remove this CE from the pending list.
        //
        hr = S_OK;
        goto done;
    }
    IF_FAILED_GOTO( hr );

    //
    // We have a pending result that we might be able to complete.
    // Therefore, we're either inside ProcessContentEnabler or inside Update after
    // ProcessContentEnabler has been called.  Let's check if we have the key(s) we need.
    //
    hr = this->GetAllocatedBlob( CLEARKEY_GUID_ATTRIBUTE_PSSH_FROM_CONTENT_FILE_OR_GEN_REQUEST, &pbContentInitData, &cbContentInitData );
    if( hr == MF_E_ATTRIBUTENOTFOUND )
    {
        //
        // We never create a content enabler without a PSSH, so this should be impossible!
        //
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }
    IF_FAILED_GOTO( hr );

    IF_FAILED_GOTO( GetKidsFromPSSH( cbContentInitData, pbContentInitData, &cbKids, &pbKids ) );
    IF_FAILED_GOTO( DoAllKidsHaveKeysInAttributes( this, cbKids, pbKids, &fAllKidsHaveKeys ) );
    if( fAllKidsHaveKeys )
    {
        //
        // We have the key we need for our PSSH, so complete the IMFAsyncResult
        // and set it to null so we don't try to complete it twice.
        // (Future calls to this function will get MF_E_INVALIDTYPE above.)
        //
        IF_FAILED_GOTO( this->SetUnknown( CLEARKEY_GUID_ATTRIBUTE_CE_PENDING_RESULT, nullptr ) );
        IF_FAILED_GOTO( MFInvokeCallback( spIMFAsyncResult.Get() ) );

        //
        // If we're inside Update, returning S_OK will remove this CE from the pending list.
        //
        goto done;
    }

    //
    // We searched our keys but didn't find the one(s) we needed.
    // Return S_FALSE to indicate this.
    // If we're inside ProcessContentEnabler, this CE will be added to the pending list to be completed during Update.
    // If we're inside Update, the key(s) just received didn't help this CE, so it will remain on the pending list.
    //
    hr = S_FALSE;

done:
    CoTaskMemFree( pbContentInitData );
    delete[] pbKids;
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::GetEnableType( _Out_ GUID *pType )
{
    if( pType == nullptr )
    {
        return E_INVALIDARG;
    }
    *pType = CLEARKEY_GUID_CONTENT_ENABLER_TYPE;
    return S_OK;
}

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::GetShutdownStatus( _Out_ MFSHUTDOWN_STATUS *pStatus )
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

STDMETHODIMP Cdm_clearkey_IMFContentEnabler::Shutdown()
{
    CriticalSection::SyncLock lock = m_Lock.Lock();
    if( m_spIMFAttributes != nullptr )
    {
        (void)m_spIMFAttributes->SetUnknown( CLEARKEY_GUID_ATTRIBUTE_CE_PENDING_RESULT, nullptr );
    }
    m_fShutdown = TRUE;
    return S_OK;
}

