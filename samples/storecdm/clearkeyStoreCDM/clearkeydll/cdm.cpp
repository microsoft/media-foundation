//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#include "stdafx.h"

static const WCHAR g_wszKeySystem_clearkey[] = L"org.w3.clearkey";

static const WCHAR g_wszkeyids[] = L"keyids";
static const WCHAR g_wszkids[] = L"kids";
static const WCHAR g_wsztype[] = L"type";
static const WCHAR g_wszkeys[] = L"keys";
static const WCHAR g_wszexpiration[] = L"expiration";
static const WCHAR g_wszpolicy[] = L"policy";

//
// See _ParseExpirationFromJSON
// 2020/01/31/11:30:00 is 19 characters long
//
#define EXPIRATION_STR_LEN  19
#define ISNUM( _wc_ ) ((_wc_)>=L'0' && (_wc_)<=L'9')

static const WCHAR g_wszkty[] = L"kty";
static const WCHAR g_wszoct[] = L"oct";
static const WCHAR g_wszk[] = L"k";
static const WCHAR g_wszkid[] = L"kid";

static const WCHAR g_wszTemporary[] = L"temporary";
static const WCHAR g_wszPersistentLicense[] = L"persistent-license";

static const char g_szJsonLicenseRequestStart[] = "{\"kids\":[";
static const char g_szJsonLicenseRequestMiddle[] = "],\"type\":\"";
static const char g_szJsonLicenseRequestTypeEndTemporary[] = "temporary\"}";
static const char g_szJsonLicenseRequestTypeEndPersistent[] = "persistent-license\"}";
#define g_szJsonLicenseReleaseStart g_szJsonLicenseRequestStart
static const char g_szJsonLicenseReleaseEnd[] = "]}";

static const WCHAR g_wszcenc[] = L"cenc";

#define SESSION_ID_MIN_VAL 251658240
#define SESSION_ID_MIN_STR L"251658240"
#define SESSION_ID_STRLEN  9

static const char g_szBeginReleaseMessage[] = "\"kids\":[";
static const char g_szEndReleaseMessage[] = "]}";

static void _CompileTimeAsserts()
{
    static_assert( sizeof( GUID ) == AES_BLOCK_LEN, "Only AES128 is supported" );
    static_assert( sizeof( GUID ) == sizeof( OriginatorId ), "OriginatorId must be the same size as GUID" );
    static_assert( sizeof( MFPOLICYMANAGER_ACTION ) == sizeof( UINT32 ), "Disk serialization requires that MFPOLICYMANAGER_ACTION be 4 bytes in size" );
}

static HRESULT _ClonePropertyStore(
    _In_    IPropertyStore *pIPropertyStoreOriginal,
    _Inout_ IPropertyStore *pIPropertyStoreClone )
{
    HRESULT hr    = S_OK;
    DWORD   cProp = 0;
    PROPVARIANTWRAP propvar;

    IF_FAILED_GOTO( pIPropertyStoreOriginal->GetCount( &cProp ) );

    for( DWORD iProp = 0; iProp < cProp; iProp++ )
    {
        PROPERTYKEY key;

        IF_FAILED_GOTO( pIPropertyStoreOriginal->GetAt( iProp, &key ) );
        IF_FAILED_GOTO( pIPropertyStoreOriginal->GetValue( key, &propvar ) );
        IF_FAILED_GOTO( pIPropertyStoreClone->SetValue( key, propvar ) );
        IF_FAILED_GOTO( propvar.Clear() );
    }

done:
    return hr;
}

static HRESULT _CopyStringAttributeFromIPropertyStoreToIMFAttributes(
    _In_        IPropertyStore     *pIPropertyStoreIn,
    _In_        const PROPERTYKEY  &key,
    _Inout_     IMFAttributes      *pIMFAttributesOut,
    _In_        REFGUID             guidAttributeOut,
    _Out_opt_   BOOL               *pfFound )
{
    HRESULT hr         = S_OK;
    WCHAR  *pwszBuffer = nullptr;
    PROPVARIANTWRAP propvar;

    IF_FAILED_GOTO( pIPropertyStoreIn->GetValue( key, &propvar ) );   // Returns S_OK / VT_EMPTY if not found
    if( propvar.vt != VT_EMPTY )
    {
        DWORD cch = 0;

        IF_FALSE_GOTO( propvar.vt == VT_BSTR, MF_E_UNEXPECTED );
        cch = SysStringLen( propvar.bstrVal ) + 1;
        pwszBuffer = new WCHAR[ cch ];
        IF_NULL_GOTO( pwszBuffer, E_OUTOFMEMORY );
        memcpy( pwszBuffer, propvar.bstrVal, cch * sizeof( WCHAR ) );

        IF_FAILED_GOTO( pIMFAttributesOut->SetString( guidAttributeOut, pwszBuffer ) );
        if( pfFound != nullptr )
        {
            *pfFound = TRUE;
        }
    }
    else if( pfFound != nullptr )
    {
        *pfFound = FALSE;
    }

done:
    delete[] pwszBuffer;
    return hr;
}

static HRESULT _AddPropertyToSet(
    _Inout_ IPropertySet *pIPropertySet,
    _In_    LPCWSTR       pwszName,
    _In_    IInspectable *pIInspectableValue )
{
    HRESULT hr        = S_OK;
    boolean fReplaced = false;
    ComPtr<IMap<HSTRING, IInspectable*>> spIMap;

    IF_FAILED_GOTO( pIPropertySet->QueryInterface( IID_PPV_ARGS( &spIMap ) ) );
    IF_FAILED_GOTO( spIMap->Insert( HStringReference( pwszName ).Get(), pIInspectableValue, &fReplaced ) );

done:
    return hr;
}

static HRESULT _AddStringToPropertySet(
    _Inout_ IPropertySet *pIPropertySet,
    _In_    LPCWSTR       pwszName,
    _In_    LPCWSTR       pwszString )
{
    HRESULT hr   = S_OK;
    ComPtr<IPropertyValue>        spIPropertyValue;
    ComPtr<IPropertyValueStatics> spIPropertyValueStatics;

    IF_FAILED_GOTO( GetActivationFactory( HStringReference( RuntimeClass_Windows_Foundation_PropertyValue ).Get(), spIPropertyValueStatics.GetAddressOf() ) );

    IF_FAILED_GOTO( spIPropertyValueStatics->CreateString( HStringReference( pwszString ).Get(), &spIPropertyValue ) );
    IF_FAILED_GOTO( _AddPropertyToSet( pIPropertySet, pwszName, spIPropertyValue.Get() ) );

done:
    return hr;
}

static HRESULT _AddBoolToPropertySet(
    _Inout_ IPropertySet *pIPropertySet,
    _In_    LPCWSTR       pwszName,
    _In_    BOOL          fValue )
{
    HRESULT hr   = S_OK;
    ComPtr<IPropertyValue>        spIPropertyValue;
    ComPtr<IPropertyValueStatics> spIPropertyValueStatics;

    IF_FAILED_GOTO( GetActivationFactory( HStringReference( RuntimeClass_Windows_Foundation_PropertyValue ).Get(), spIPropertyValueStatics.GetAddressOf() ) );

    IF_FAILED_GOTO( spIPropertyValueStatics->CreateBoolean( !!fValue, &spIPropertyValue ) );
    IF_FAILED_GOTO( _AddPropertyToSet( pIPropertySet, pwszName, spIPropertyValue.Get() ) );

done:
    return hr;
}

static DWORD _ComputeLicenseRequestMessageSize( _In_ DWORD cKids, _In_ BOOL fTemporary )
{
    return 1                                                // single null terminator
        + ARRAYSIZE( g_szJsonLicenseRequestStart ) - 1      // exclude null terminator
        + ( GUID_B64_URL_LEN + 2 ) * cKids                  // add two for opening/closing quotes surrounding each kid
        + 1 * ( cKids - 1 )                                 // comma after each kid except last
        + ARRAYSIZE( g_szJsonLicenseRequestMiddle ) - 1     // exclude null terminator
        + ( fTemporary ? ARRAYSIZE( g_szJsonLicenseRequestTypeEndTemporary ) : ARRAYSIZE( g_szJsonLicenseRequestTypeEndPersistent ) ) - 1;  // exclude null terminator
}

static DWORD _ComputeLicenseReleaseMessageSize( _In_ DWORD cKids )
{
    return 1                                                // single null terminator
        + ARRAYSIZE( g_szJsonLicenseReleaseStart ) - 1      // exclude null terminaator
        + ( GUID_B64_URL_LEN + 2 ) * cKids                  // add two for opening/closing quotes surrounding each kid
        + 1 * ( cKids - 1 )                                 // comma after each kid except last
        + ARRAYSIZE( g_szJsonLicenseReleaseEnd ) - 1;
}

Cdm_clearkey_IMFContentDecryptionModuleFactory::Cdm_clearkey_IMFContentDecryptionModuleFactory()
{
}

Cdm_clearkey_IMFContentDecryptionModuleFactory::~Cdm_clearkey_IMFContentDecryptionModuleFactory()
{
}

static HRESULT _IsTypeSupported( _In_ LPCWSTR keySystem )
{
    TRACE_FUNC();
    if( CSTR_EQUAL != CompareStringOrdinal( g_wszKeySystem_clearkey, -1, keySystem, -1, FALSE ) )   // String comparison is case-sensitive.
    {
        return MF_NOT_SUPPORTED_ERR;
    }

    return S_OK;
}

STDMETHODIMP_( BOOL ) Cdm_clearkey_IMFContentDecryptionModuleFactory::IsTypeSupported( _In_ LPCWSTR keySystem, _In_opt_ LPCWSTR contentType )
{
    HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        return _IsTypeSupported( keySystem );
    } );

    return SUCCEEDED( hrRet );
}

static HRESULT _AddKidToMessage( _In_ BYTE *pbMessage, _Inout_ DWORD *pibMessage, _In_ REFGUID guidKid, _In_ BOOL fAddComma )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    BYTE    rgbKid[ sizeof( GUID ) ];
    WCHAR   rgwchGuid[ GUID_B64_LEN + 3 ] = { 0 };  // CryptBinaryToStringW requires more space than is strictly necessary, so provide that much.
    DWORD   cchGuid = ARRAYSIZE( rgwchGuid );

    pbMessage[ *pibMessage ] = '"';
    ( *pibMessage )++;

    //
    // Use a standard windows system function to base64 encode the value for us.
    //
    GUID_TO_BIG_ENDIAN_SIXTEEN_BYTES( &guidKid, rgbKid );
    IF_FALSE_GOTO( CryptBinaryToStringW(
        rgbKid,
        KEY_DATA_KID_SIZE,
        CRYPT_STRING_BASE64,
        rgwchGuid,
        &cchGuid ), MF_E_UNEXPECTED );

    //
    // We have to convert from standard base64 to base64url.
    //

    //
    // Convert the two special-case characters.
    //
    for( DWORD idx = 0; idx < GUID_B64_URL_LEN; idx++ )
    {
        switch( rgwchGuid[ idx ] )
        {
        case '+':
            rgwchGuid[ idx ] = '-';
            break;
        case '/':
            rgwchGuid[ idx ] = '_';
            break;
        default:
            // no-op
            break;
        }
    }

    //
    // Remove the two '=' characters (padding) by copying only GUID_B64_URL_LEN characters instead of GUID_B64_LEN characters.
    //
    IF_FALSE_GOTO( GUID_B64_URL_LEN == WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        rgwchGuid,
        GUID_B64_URL_LEN,
        (LPSTR)&pbMessage[ *pibMessage ],
        GUID_B64_URL_LEN,
        nullptr,
        nullptr ), MF_TYPE_ERR );
    ( *pibMessage ) += GUID_B64_URL_LEN;

    pbMessage[ *pibMessage ] = '"';
    ( *pibMessage )++;

    if( fAddComma )
    {
        pbMessage[ *pibMessage ] = ',';
        ( *pibMessage )++;
    }

done:
    return hr;
}

static HRESULT _ValidateInitDataTypes( _In_ IPropertyStore *pIPropertyStoreConfiguration, _Out_ BOOL *pfSupportedConfiguration )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // This function implements step 3.
    //
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    BOOL    fKeyIds = FALSE;
    BOOL    fCenc   = FALSE;
    DWORD   cTypes  = 0;
    PROPVARIANTWRAP propvar;

    if( !*pfSupportedConfiguration )
    {
        goto done;
    }

    //
    // Default to FALSE.  Supported if we find at least one init data type we support OR there are no init data types specified.
    //
    *pfSupportedConfiguration = FALSE;

    IF_FAILED_GOTO( pIPropertyStoreConfiguration->GetValue( MF_EME_INITDATATYPES, &propvar ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // 3. If the initDataTypes member of candidate configuration is non-empty, run the following steps:
    //
    if( propvar.vt != VT_EMPTY )
    {
        DWORD iInitData = 0;
        IF_FALSE_GOTO( propvar.vt == ( VT_VECTOR | VT_BSTR ), MF_TYPE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // Note: Step 3.1 is handled inside an 'else' clause below.
        // 3.2. For each value in candidate configuration's initDataTypes member:
        //
        for( iInitData = 0; iInitData < propvar.cabstr.cElems; iInitData++ )
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
            //
            // 3.2.1. Let initDataType be the value
            //
            // The value is propvar.cabstr.pElems[ iInitData ]
            //
            // 3.2.2. If the implementation supports generating requests based on initDataType, add initDataType to supported types.
            //
            // Handled by setting the BOOL to TRUE for the corresponding initDataType.
            //

            //
            // We support "keyids" and "cenc".
            //
            if( CSTR_EQUAL == CompareStringOrdinal( g_wszcenc, -1, propvar.cabstr.pElems[ iInitData ], -1, FALSE ) )            // String comparison is case-sensitive.
            {
                fCenc = TRUE;
                cTypes++;
            }
            else if( CSTR_EQUAL == CompareStringOrdinal( g_wszkeyids, -1, propvar.cabstr.pElems[ iInitData ], -1, FALSE ) )     // String comparison is case-sensitive.
            {
                fKeyIds = TRUE;
                cTypes++;
            }
        }

        if( cTypes == 0 )
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
            //
            // 3.3. If supported types is empty, return NotSupported.
            //
            *pfSupportedConfiguration = FALSE;
        }
        else
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
            //
            // 3.1. Let accumulated configuration be a new MediaKeySystemConfiguration dictionary.
            // 
            // Handled by freeing the existing list of entries.
            //
            IF_FAILED_GOTO( propvar.Clear() );

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
            // 
            // 3.4. Set the initDataTypes member of accumulated configuration to supported types.
            //
            propvar.cabstr.pElems = (BSTR*)CoTaskMemAlloc( cTypes * sizeof( BSTR ) );
            IF_NULL_GOTO( propvar.cabstr.pElems, E_OUTOFMEMORY );
            ZeroMemory( propvar.cabstr.pElems, cTypes * sizeof( BSTR ) );
            propvar.cabstr.cElems = cTypes;

            iInitData = 0;
            if( fCenc )
            {
                propvar.cabstr.pElems[ iInitData ] = SysAllocString( g_wszcenc );
                IF_NULL_GOTO( propvar.cabstr.pElems[ iInitData ], E_OUTOFMEMORY );
                iInitData++;
            }
            if( fKeyIds )
            {
                propvar.cabstr.pElems[ iInitData ] = SysAllocString( g_wszkeyids );
                IF_NULL_GOTO( propvar.cabstr.pElems[ iInitData ], E_OUTOFMEMORY );
                iInitData++;
            }

            *pfSupportedConfiguration = TRUE;
        }
    }
    else
    {
        *pfSupportedConfiguration = TRUE;
    }

done:
    return hr;
}

static HRESULT _ValidateDistinctiveIdentifier( _In_ IPropertyStore *pIPropertyStoreConfiguration, _Out_ BOOL *pfSupportedConfiguration )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // This function implements steps 4-7.
    //
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);

    PROPVARIANTWRAP propvar;

    if( !*pfSupportedConfiguration )
    {
        goto done;
    }

    //
    // Default to TRUE.  Supported if we DON'T find "required".
    //
    *pfSupportedConfiguration = TRUE;

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // 4. Let distinctive identifier requirement be the value of candidate configuration's distinctiveIdentifier member.
    //
    IF_FAILED_GOTO( pIPropertyStoreConfiguration->GetValue( MF_EME_DISTINCTIVEID, &propvar ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Step 5 is skipped because there are no "restrictions".
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // 6. Follow the steps for distinctive identifier requirement from the following list:
    //
    if( propvar.vt != VT_EMPTY )
    {
        IF_FALSE_GOTO( propvar.vt == VT_UI4, MF_TYPE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 6. Follow the steps for distinctive identifier requirement from the following list:
        //      "required"
        //        If the implementation does not support use of Distinctive Identifier(s)
        //        in combination with accumulated configuration and restrictions, return NotSupported.
        //
        if( (MF_MEDIAKEYS_REQUIREMENT)propvar.ulVal == MF_MEDIAKEYS_REQUIREMENT_REQUIRED )
        {
            //
            // We never support use of Distinctive Identifier(s).
            //
            *pfSupportedConfiguration = FALSE;
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 6. Follow the steps for distinctive identifier requirement from the following list:
        //      "not-allowed"
        //        If the implementation requires use Distinctive Identifier(s) or Distinctive Permanent Identifier(s)
        //        in combination with accumulated configuration and restrictions, return NotSupported.
        //
        // We never requires use of Distinctive Identifier(s) or Distinctive Permanent Identifier(s).
        // Thus, no action is required for "not-allowed".
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 6. Follow the steps for distinctive identifier requirement from the following list:
        //      "optional"
        //        Continue with the following steps.
        //
        // Thus, no action is required for "optional".
        //

        // no-op
    }
    else
    {
        //
        // Not specified defaults to "optional".
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 6. Follow the steps for distinctive identifier requirement from the following list:
        // "optional"
        //  Continue with the following steps.
        //
        // Thus, no action is required for "optional".
        //

        // no-op
    }

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // 7. Set the distinctiveIdentifier member of accumulated configuration to equal distinctive identifier requirement.
    //
    // Note: This occurs at step 18 without changing the algorithm's overall behavior.
    // Refer to that step in _GetSupportedConfigurationAlgorithm.
    //

done:
    return hr;
}

static HRESULT _ValidatePersistentState( _In_ IPropertyStore *pIPropertyStoreConfiguration, _Out_ BOOL *pfSupportedConfiguration, _Out_ BOOL *pfPersistentStateAllowed )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // This function implements steps 8-11.
    //
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    PROPVARIANTWRAP propvar;

    if( !*pfSupportedConfiguration )
    {
        goto done;
    }

    //
    // Default to TRUE.  All persisted state values are supported.
    //
    *pfSupportedConfiguration = TRUE;

    //
    // Default to TRUE.  Persistent state is allowed if we DON'T find "not-allowed".
    //
    *pfPersistentStateAllowed = TRUE;

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // 8. Let persistent state requirement be equal to the value of candidate configuration's persistentState member. 
    //
    IF_FAILED_GOTO( pIPropertyStoreConfiguration->GetValue( MF_EME_PERSISTEDSTATE, &propvar ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Step 9 is skipped because there are no "restrictions".
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Step 10 is skipped because:
    //   We always support persisting state.
    //   We never require persisting state.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // 11. Set the persistentState member of accumulated configuration to equal the value of persistent state requirement.
    //
    // Note: This occurs at step 19 without changing the algorithm's overall behavior.
    // Refer to that step in _GetSupportedConfigurationAlgorithm.
    //

    //
    // Save whether persistent state is allowed or not for use in later algorithm steps.
    //
    if( propvar.vt != VT_EMPTY )
    {
        IF_FALSE_GOTO( propvar.vt == VT_UI4, MF_TYPE_ERR );
        if( propvar.vt == MF_MEDIAKEYS_REQUIREMENT_NOT_ALLOWED )
        {
            *pfPersistentStateAllowed = FALSE;
        }
    }

done:
    return hr;
}

static HRESULT _ValidateSessionTypes( _In_ IPropertyStore *pIPropertyStoreConfiguration, _Out_ BOOL *pfSupportedConfiguration, _In_ BOOL fPersistentStateAllowed, _Out_ BOOL *pfPersistentSessionsAllowed )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // This function implements steps 12-14.
    //
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    PROPVARIANTWRAP propvar;

    if( !*pfSupportedConfiguration )
    {
        goto done;
    }

    //
    // Default to TRUE.  All session types are supported UNLESS the session type is "persistent-license" AND persistent state is NOT allowed.
    //
    *pfSupportedConfiguration = TRUE;

    //
    // Default to FALSE.  Only allowed if specified.
    //
    *pfPersistentSessionsAllowed = FALSE;

    IF_FAILED_GOTO( pIPropertyStoreConfiguration->GetValue( MF_EME_SESSIONTYPES, &propvar ) );

    if( propvar.vt != VT_EMPTY )
    {
        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 12. Follow the steps for the first matching condition from the following list:
        //     If the sessionTypes member is present [WebIDL] in candidate configuration
        //      Let session types be candidate configuration's sessionTypes member.
        //
        // propvar.caul now contains the session types
        //

        IF_FALSE_GOTO( propvar.vt == ( VT_VECTOR | VT_UI4 ), MF_TYPE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 13. For each value in session types:
        //
        for( DWORD iSessionType = 0; iSessionType < propvar.caul.cElems && *pfSupportedConfiguration; iSessionType++ )
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
            //
            // 13.1. Let session type be the value
            //
            // propvar.caul.pElems[ iSessionType ] is the value.
            //
            switch( propvar.caul.pElems[ iSessionType ] )
            {
            case MF_MEDIAKEYSESSION_TYPE_TEMPORARY:
                // no-op
                break;
            case MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE:
                if( fPersistentStateAllowed )
                {
                    *pfPersistentSessionsAllowed = TRUE;
                }
                else
                {
                    //
                    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
                    //
                    // 13.2. If accumulated configuration's persistentState value is "not-allowed" and the
                    //       Is persistent session type? algorithm returns true for session type return NotSupported.
                    //
                    *pfSupportedConfiguration = FALSE;
                }
                break;
            default:
                *pfSupportedConfiguration = FALSE;      // Unknown session type
                break;
            }
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 13.3. If the implementation does not support session type in combination with accumulated
        //       configuration and restrictions for other reasons, return NotSupported.
        //
        // We have no other reasons.  No action is required here.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 13.4. If accumulated configuration's persistentState value is "optional" and the result of
        //       running the Is persistent session type? algorithm on session type is true,
        //       change accumulated configuration's persistentState value to "required".
        //
        // Note: This occurs at to step 19 without changing the algorithm's overall behavior.
        // Refer to that step in _GetSupportedConfigurationAlgorithm.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 14. Set the sessionTypes member of accumulated configuration to session types.
        //
        //  This is handled automatically via cloning the dictionary in
        //  Cdm_clearkey_IMFContentDecryptionModuleAccess::RuntimeClassInitialize.  No action is required here.
        //
    }
    else
    {
        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 12. Follow the steps for the first matching condition from the following list:
        //     Otherwise
        //      Let session types be [ "temporary" ].
        //
        propvar.vt = ( VT_VECTOR | VT_UI4 );


#define TODO_WORKAROUND_EDGEHTML_BUG_20973652 1
#if TODO_WORKAROUND_EDGEHTML_BUG_20973652

        //
        // There's an edge-html bug where they do not properly set the MF_EME_SESSIONTYPES property regardless of what's in the MediaKeySystemConfiguration.
        // Thus, we always enter this block, and if we default to "temporary" (per-spec), then persistent-license sessions will fail to be created.
        //
        // We workaround this bug by making our configuration always support both session types, i.e. as if the website passed in sessionTypes:["temporary","persistent-license"]
        // This is NOT spec-compliant.
        //
        propvar.caul.cElems = 2;
        propvar.caul.pElems = (ULONG*)CoTaskMemAlloc( 2 * sizeof( ULONG ) );
        IF_NULL_GOTO( propvar.caul.pElems, E_OUTOFMEMORY );
        propvar.caul.pElems[ 0 ] = MF_MEDIAKEYSESSION_TYPE_TEMPORARY;
        propvar.caul.pElems[ 1 ] = MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE;

#else   // TODO_WORKAROUND_EDGEHTML_BUG_20973652

        //
        // The following would be spec-compliant behavior but cannot be used so long as bug 20973652 exists.
        //
        propvar.caul.cElems = 1;
        propvar.caul.pElems = (ULONG*)CoTaskMemAlloc( sizeof( ULONG ) );
        IF_NULL_GOTO( propvar.caul.pElems, E_OUTOFMEMORY );
        *propvar.caul.pElems = MF_MEDIAKEYSESSION_TYPE_TEMPORARY;

#endif  // TODO_WORKAROUND_EDGEHTML_BUG_20973652

        IF_FAILED_GOTO( pIPropertyStoreConfiguration->SetValue( MF_EME_SESSIONTYPES, propvar ) );
    }

done:
    return hr;
}

static HRESULT _CheckRobustnessValue( _In_ IPropertyStore *pIPropertyStore, _Out_ BOOL *pfSupportedRobustnessValue )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    PROPVARIANTWRAP propvar;

    //
    // Default to TRUE.  Supported unless we find a non-empty robustness value.
    //
    *pfSupportedRobustnessValue = TRUE;

    IF_FAILED_GOTO( pIPropertyStore->GetValue( MF_EME_ROBUSTNESS, &propvar ) );

    if( propvar.vt != VT_EMPTY )
    {
        IF_FALSE_GOTO( propvar.vt == VT_BSTR, MF_TYPE_ERR );
        if( SysStringLen( propvar.bstrVal ) != 0 )
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-capabilities-for-audio-video-type
            //
            // 3.12. If robustness is not the empty string and contains an unrecognized value or a value not supported
            //         by implementation, continue to the next iteration. String comparison is case-sensitive.
            //
            *pfSupportedRobustnessValue = FALSE;
        }
    }

done:
    return hr;
}

static HRESULT _CheckCapabilitiesBasedOnRobustnessValues( _Inout_ PROPVARIANTWRAP& propvar, _Out_ BOOL *pfSupportedConfiguration, _Out_ BOOL *pfUpdated )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    //   This function is a shared implementation of portions of steps 16 and 17.
    //
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);

    if( !*pfSupportedConfiguration )
    {
        goto done;
    }

    *pfUpdated = FALSE;

    if( propvar.vt != VT_EMPTY )
    {
        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 16. If the videoCapabilities member in candidate configuration is non-empty:
        // 17. If the audioCapabilities member in candidate configuration is non-empty:
        //
        ComPtr<IPropertyStore> spIPropertyStore;
        DWORD cSupportedRobustnessValuesFound = 0;

        IF_FALSE_GOTO( propvar.vt == ( VT_VECTOR | VT_VARIANT ), MF_TYPE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 16.1. Let video capabilities be the result of executing the Get Supported Capabilities
        //         for Audio / Video Type algorithm on Video, candidate configuration's videoCapabilities
        //         member, accumulated configuration, and restrictions.
        //
        // 17.1. Let audio capabilities be the result of executing the Get Supported Capabilities
        //         for Audio/Video Type algorithm on Audio, candidate configuration's audioCapabilities
        //         member, accumulated configuration, and restrictions.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-capabilities-for-audio-video-type
        //
        // Note: All steps of this algorithm are either irrelevant to org.w3.clearkey or are handled by
        // the caller (e.g. supported codecs, etc) with the following exceptions.
        //
        // 3. For each requested media capability in requested media capabilities:
        //      ...
        //      3.12. If robustness is not the empty string and contains an unrecognized value or a value not supported by implementation,
        //              continue to the next iteration. String comparison is case-sensitive.
        // 4. If supported media capabilities is empty, return null.
        //
        // Note that this translates into returning NotSupported at 16.2. or 17.2. of the following, so it is implicit by handling those steps below.
        //   https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 5. Return supported media capabilities.
        //
        // Note that this translates into setting a member of accumulated configuration at step 16.3. or 17.3 of the following, so it is implicit by handling those steps below.
        //   https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-capabilities-for-audio-video-type
        //
        // 3. For each requested media capability in requested media capabilities:
        //
        for( DWORD iVideoCapabilities = 0; iVideoCapabilities < propvar.capropvar.cElems; iVideoCapabilities++ )
        {
            BOOL fSupportedRobustnessValue = FALSE;
            IF_FALSE_GOTO( propvar.capropvar.pElems[ iVideoCapabilities ].vt == VT_UNKNOWN, MF_TYPE_ERR );
            IF_FAILED_GOTO( propvar.capropvar.pElems[ iVideoCapabilities ].punkVal->QueryInterface( IID_PPV_ARGS( &spIPropertyStore ) ) );

            IF_FAILED_GOTO( _CheckRobustnessValue( spIPropertyStore.Get(), &fSupportedRobustnessValue ) );
            if( fSupportedRobustnessValue )
            {
                cSupportedRobustnessValuesFound++;
            }
            else
            {
                //
                // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-capabilities-for-audio-video-type
                //
                // 3.12. If robustness is not the empty string and contains an unrecognized value or a value not supported by implementation,
                //         continue to the next iteration. String comparison is case-sensitive.
                //
                PropVariantClear( &propvar.capropvar.pElems[ iVideoCapabilities ] );
            }
        }

        if( cSupportedRobustnessValuesFound == 0 )
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
            //
            // 16.2. If video capabilities is null, return NotSupported.
            // 17.2. If audio capabilities is null, return NotSupported.
            //
            *pfSupportedConfiguration = FALSE;
        }
        else if( cSupportedRobustnessValuesFound != propvar.capropvar.cElems )
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
            //
            // 16.3. Set the videoCapabilities member of accumulated configuration to video capabilities.
            // 17.3. Set the audioCapabilities member of accumulated configuration to audio capabilities.
            //
            // This function modifies the in/out propvar and sets *pfUpdated to TRUE.
            // The caller actually sets the member.
            //
            DWORD iUpdated = 0;
            for( DWORD iVideoCapabilities = 0; iVideoCapabilities < propvar.capropvar.cElems; iVideoCapabilities++ )
            {
                if( propvar.capropvar.pElems[ iVideoCapabilities ].vt != VT_EMPTY && iVideoCapabilities != iUpdated )
                {
                    //
                    // Shift this entry left
                    //
                    propvar.capropvar.pElems[ iUpdated ].vt = propvar.capropvar.pElems[ iVideoCapabilities ].vt;
                    propvar.capropvar.pElems[ iVideoCapabilities ].vt = VT_EMPTY;
                    propvar.capropvar.pElems[ iUpdated ].punkVal = propvar.capropvar.pElems[ iVideoCapabilities ].punkVal;
                    propvar.capropvar.pElems[ iVideoCapabilities ].punkVal = nullptr;
                    iUpdated++;
                }
            }
            IF_FALSE_GOTO( iUpdated == cSupportedRobustnessValuesFound, MF_E_UNEXPECTED );   // Should be impossible!
            propvar.capropvar.cElems = cSupportedRobustnessValuesFound;

            *pfUpdated = TRUE;
            *pfSupportedConfiguration = TRUE;
        }
        else
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
            //
            // 16.3. Set the videoCapabilities member of accumulated configuration to video capabilities.
            // 17.3. Set the audioCapabilities member of accumulated configuration to audio capabilities.
            //

            //
            // All values were supported.  No action is required here.
            //
        }
    }
    else
    {
        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 16. Otherwise:
        //       Set the videoCapabilities member of accumulated configuration to an empty sequence.
        // 17. Otherwise:
        //       Set the audioCapabilities member of accumulated configuration to an empty sequence.
        //

        //
        // It's already set to an empty sequence.  No action is required here.
        //
        *pfSupportedConfiguration = TRUE;
    }

done:
    return hr;
}

static HRESULT _ValidateVideoCapabilities( _In_ IPropertyStore *pIPropertyStoreConfiguration, _Out_ BOOL *pfSupportedConfiguration )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // This function implements step 16.
    //
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    BOOL fUpdated = FALSE;
    PROPVARIANTWRAP propvar;

    if( !*pfSupportedConfiguration )
    {
        goto done;
    }

    //
    // Default to TRUE.  All video capabilities are supported UNLESS the robustness string is non-empty.
    //
    *pfSupportedConfiguration = TRUE;

    IF_FAILED_GOTO( pIPropertyStoreConfiguration->GetValue( MF_EME_VIDEOCAPABILITIES, &propvar ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Forward to shared implementation for steps 16.1 and 16.2.
    //
    IF_FAILED_GOTO( _CheckCapabilitiesBasedOnRobustnessValues( propvar, pfSupportedConfiguration, &fUpdated ) );
    if( fUpdated )
    {
        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 16.3. Set the videoCapabilities member of accumulated configuration to video capabilities.
        //
        IF_FAILED_GOTO( pIPropertyStoreConfiguration->SetValue( MF_EME_VIDEOCAPABILITIES, propvar ) );
    }

done:
    return hr;
}

static HRESULT _ValidateAudioCapabilities( _In_ IPropertyStore *pIPropertyStoreConfiguration, _Out_ BOOL *pfSupportedConfiguration )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // This function implements step 17.
    //
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    BOOL fUpdated = FALSE;
    PROPVARIANTWRAP propvar;

    if( !*pfSupportedConfiguration )
    {
        goto done;
    }

    //
    // Default to TRUE.  All audio capabilities are supported UNLESS the robustness string is non-empty.
    //
    *pfSupportedConfiguration = TRUE;

    IF_FAILED_GOTO( pIPropertyStoreConfiguration->GetValue( MF_EME_AUDIOCAPABILITIES, &propvar ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Forward to shared implementation for steps 17.1 and 17.2.
    //
    IF_FAILED_GOTO( _CheckCapabilitiesBasedOnRobustnessValues( propvar, pfSupportedConfiguration, &fUpdated ) );
    if( fUpdated )
    {
        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
        //
        // 17.3. Set the audioCapabilities member of accumulated configuration to audio capabilities.
        //
        IF_FAILED_GOTO( pIPropertyStoreConfiguration->SetValue( MF_EME_AUDIOCAPABILITIES, propvar ) );
    }

done:
    return hr;
}

static HRESULT _GetSupportedConfigurationAlgorithm( _In_ IPropertyStore *pIPropertyStoreConfiguration, _Out_ BOOL *pfSupportedConfiguration )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // No consent is required for org.w3.clearkey.
    // Thus, we bypass any steps related to acquiring consent, specifically:
    // -) Get Supported Configuration simply boils down to Get Supported Configuration and Consent
    // -) Get Supported Configuration and Consent steps 21 and 22 are skipped.
    //
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    BOOL fPersistentStateAllowed    = FALSE;
    BOOL fPersistentSessionsAllowed = FALSE;
    PROPVARIANTWRAP propvar;

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Step 1 is handled via cloning the dictionary in Cdm_clearkey_IMFContentDecryptionModuleAccess::RuntimeClassInitialize
    // Note: Step 2 is handled by caller.
    //

    //
    // Assume the configuration is supported until proven otherwise.
    //
    *pfSupportedConfiguration = TRUE;

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Step 3 is handled by the following function.
    //
    IF_FAILED_GOTO( _ValidateInitDataTypes( pIPropertyStoreConfiguration, pfSupportedConfiguration ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Steps 4-7 are handled by the following function.
    //
    IF_FAILED_GOTO( _ValidateDistinctiveIdentifier( pIPropertyStoreConfiguration, pfSupportedConfiguration ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Steps 8-11 is handled by the following function.
    //
    IF_FAILED_GOTO( _ValidatePersistentState( pIPropertyStoreConfiguration, pfSupportedConfiguration, &fPersistentStateAllowed ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Steps 12-14 are handled by the following function.
    //
    IF_FAILED_GOTO( _ValidateSessionTypes( pIPropertyStoreConfiguration, pfSupportedConfiguration, fPersistentStateAllowed, &fPersistentSessionsAllowed ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Step 15 is handled by caller.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Step 16 is handled by the following function.
    //
    IF_FAILED_GOTO( _ValidateVideoCapabilities( pIPropertyStoreConfiguration, pfSupportedConfiguration ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Step 17 is handled by the following function.
    //
    IF_FAILED_GOTO( _ValidateAudioCapabilities( pIPropertyStoreConfiguration, pfSupportedConfiguration ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // 18. If accumulated configuration's distinctiveIdentifier value is "optional", follow the steps for the first matching condition from the following list:
    //     ...
    //     Otherwise
    //       Change accumulated configuration's distinctiveIdentifier value to "not-allowed".
    //
    if( *pfSupportedConfiguration )
    {
        IF_FAILED_GOTO( propvar.Clear() );
        propvar.vt = VT_UI4;
        propvar.ulVal = MF_MEDIAKEYS_REQUIREMENT_NOT_ALLOWED;
        IF_FAILED_GOTO( pIPropertyStoreConfiguration->SetValue( MF_EME_DISTINCTIVEID, propvar ) );
    }

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // 19. If accumulated configuration's persistentState value is "optional", follow the steps for the first matching condition from the following list:
    //       If the implementation requires persisting state for any of the combinations in accumulated configuration
    //         Change accumulated configuration's persistentState value to "required".
    //       Otherwise Change accumulated configuration's persistentState value to "not-allowed".
    //
    if( *pfSupportedConfiguration )
    {
        IF_FAILED_GOTO( propvar.Clear() );
        IF_FAILED_GOTO( pIPropertyStoreConfiguration->GetValue( MF_EME_PERSISTEDSTATE, &propvar ) );
        if( propvar.vt == VT_EMPTY )
        {
            propvar.vt = VT_UI4;
            propvar.ulVal = fPersistentSessionsAllowed ? MF_MEDIAKEYS_REQUIREMENT_REQUIRED : MF_MEDIAKEYS_REQUIREMENT_NOT_ALLOWED;
        }
        else
        {
            IF_FALSE_GOTO( propvar.vt == VT_UI4, MF_TYPE_ERR );
            if( (MF_MEDIAKEYS_REQUIREMENT)propvar.ulVal == MF_MEDIAKEYS_REQUIREMENT_OPTIONAL )
            {
                propvar.ulVal = fPersistentSessionsAllowed ? MF_MEDIAKEYS_REQUIREMENT_REQUIRED : MF_MEDIAKEYS_REQUIREMENT_NOT_ALLOWED;
            }
        }
        IF_FAILED_GOTO( pIPropertyStoreConfiguration->SetValue( MF_EME_PERSISTEDSTATE, propvar ) );
    }

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // 20. If implementation in the configuration specified by the combination of the values in
    //     accumulated configuration is not supported or not allowed in the origin, return NotSupported:
    //
    // In this case, *pfSupportedConfiguration will be FALSE at this point.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // Note: Steps 21-22 are skipped as noted at the top of this function.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#get-supported-configuration-and-consent
    //
    // 23. Return accumulated configuration.
    //
    // In this case, *pfSupportedConfiguration will be TRUE at this point.
    //

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleFactory::CreateContentDecryptionModuleAccess(
    _In_ LPCWSTR keySystem,
    _In_count_(numConfigurations) IPropertyStore **configurations,
    _In_ DWORD numConfigurations,
    _COM_Outptr_ IMFContentDecryptionModuleAccess **contentDecryptionModuleAccess
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#mediakeysystemconfiguration-dictionary
        //
        // Note: Each IPropertyStore in ppProperties represents a single MediaKeySystemConfiguration.
        //

        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        ComPtr<IPropertyStore> spIPropertyStoreConfiguration;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
        //
        // 1. If keySystem is the empty string, return a promise rejected with a newly created TypeError.
        // 2. If configurations is empty, return a promise rejected with a newly created TypeError.
        //
        IF_FALSE_GOTO( keySystem != nullptr && keySystem[ 0 ] != L'\0', MF_TYPE_ERR );
        IF_FALSE_GOTO( numConfigurations != 0, MF_TYPE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
        //
        // Note: Steps 3, 4, and 5 are handled by the caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
        //
        // 6.1. If keySystem is not one of the Key Systems supported by the user agent,
        //      reject promise with a NotSupportedError.
        //
        IF_FALSE_GOTO( this->IsTypeSupported( keySystem, nullptr ), MF_NOT_SUPPORTED_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
        //
        // 6.2. Let implementation be the implementation of keySystem
        //
        // The 'this' pointer (along with its member variables) is sufficient.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
        //
        // 6.3. For each value in supportedConfigurations:
        //
        for( DWORD iConfiguration = 0; iConfiguration < numConfigurations && spIPropertyStoreConfiguration == nullptr; iConfiguration++ )
        {
            BOOL fSupportedConfiguration = FALSE;
            ComPtr<IPropertyStore> spIPropertyStoreConfigurationCandidate;

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
            //
            // 6.3.1. Let candidate configuration be the value
            //
            // value == spIPropertyStoreConfigurationCandidate
            //
            IF_FAILED_GOTO( PSCreateMemoryPropertyStore( IID_PPV_ARGS( &spIPropertyStoreConfigurationCandidate ) ) );
            IF_FAILED_GOTO( _ClonePropertyStore( configurations[ iConfiguration ], spIPropertyStoreConfigurationCandidate.Get() ) );

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
            //
            // 6.3.2. Let supported configuration be the result of executing the Get Supported Configuration algorithm on implementation, candidate configuration, and origin.
            //
            // Note: 'origin' is handled by caller.
            //
            IF_FAILED_GOTO( _GetSupportedConfigurationAlgorithm( spIPropertyStoreConfigurationCandidate.Get(), &fSupportedConfiguration ) );

            if( fSupportedConfiguration )
            {
                //
                // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
                //
                // 6.3.3. If supported configuration is not NotSupported, run the following steps:
                //
                // [Ends loop.  See 6.3.3 continuation below.]
                //
                spIPropertyStoreConfiguration = spIPropertyStoreConfigurationCandidate;
            }
        }

        if( spIPropertyStoreConfiguration != nullptr )
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
            //
            // 6.3.3.1 Let access be a new MediaKeySystemAccess object, and initialize it as follows:
            // 6.3.3.1.1. Set the keySystem attribute to keySystem
            //

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
            //
            // Only a single key system is supported, so GetKeySystem returns a fixed value and no action is required here.
            //

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
            //
            // 6.3.3.1.2. Let the configuration value be supported configuration
            //
            // configuration value == spIPropertyStoreConfiguration
            //

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
            //
            // 6.3.3.1.3. Let the cdm implementation value be implementation
            //
            // implementation == contentDecryptionModuleAccess
            //

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
            ///
            // 6.3.3.2. Resolve promise with access and abort the parallel steps of this algorithm
            //
            // Return success.
            //
            hr = MakeAndInitialize<Cdm_clearkey_IMFContentDecryptionModuleAccess, IMFContentDecryptionModuleAccess>( contentDecryptionModuleAccess, spIPropertyStoreConfiguration.Get() );
            IF_FAILED_GOTO( hr );
        }
        else
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
            //
            // (No configuration is supported.)
            // 6.4. Reject promise with a NotSupportedError.
            //
            // Return the error.
            //
            IF_FAILED_GOTO( MF_NOT_SUPPORTED_ERR );
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
        //
        // 7. Return promise.
        //
        // Return success.
        //

    done:
        return hr;
    } );

    return hrRet;
}

Cdm_clearkey_IMFContentDecryptionModuleAccess::Cdm_clearkey_IMFContentDecryptionModuleAccess()
{
}

Cdm_clearkey_IMFContentDecryptionModuleAccess::~Cdm_clearkey_IMFContentDecryptionModuleAccess()
{
}

HRESULT Cdm_clearkey_IMFContentDecryptionModuleAccess::RuntimeClassInitialize( _In_ IPropertyStore *pIPropertyStoreConfiguration )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);

    _CompileTimeAsserts();

    IF_FAILED_GOTO( PSCreateMemoryPropertyStore( IID_PPV_ARGS( &m_spIPropertyStoreConfiguration ) ) );
    IF_FAILED_GOTO( _ClonePropertyStore( pIPropertyStoreConfiguration, m_spIPropertyStoreConfiguration.Get() ) );

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleAccess::CreateContentDecryptionModule(
    _In_ IPropertyStore *contentDecryptionModuleProperties,
    _COM_Outptr_ IMFContentDecryptionModule **contentDecryptionModule
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;

        //
        // While the interface declares the property store as optional, we require it because
        // otherwise persistent licenses will not have a path on disk where they can be written.
        //
        IF_NULL_GOTO( contentDecryptionModuleProperties, MF_E_UNEXPECTED );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
        //
        // Note: Refer to Cdm_clearkey_IMFContentDecryptionModule::RuntimeClassInitialize for algorithm steps 1 and 2.1-2.8.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
        //
        // 2.9. If any of the preceding steps failed, reject promise with a new DOMException whose name is the appropriate error name
        //
        // A failure before this point will return an error code to the caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
        //
        // Note: Refer to Cdm_clearkey_IMFContentDecryptionModule::RuntimeClassInitialize for algorithm step 2.10.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
        //
        // 2.11. Resolve promise with media keys
        //
        // Return 'cdm' upon success
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
        //
        // 3. Return promise
        //
        // return S_OK upon success
        //
        hr = MakeAndInitialize<Cdm_clearkey_IMFContentDecryptionModule, IMFContentDecryptionModule>( contentDecryptionModule, contentDecryptionModuleProperties, m_spIPropertyStoreConfiguration.Get() );
        IF_FAILED_GOTO( hr );

    done:
        return hr;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleAccess::GetConfiguration(
    _COM_Outptr_  IPropertyStore **configuration
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-getconfiguration
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        ComPtr<IPropertyStore> spIPropertyStoreConfiguration;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-getconfiguration
        //
        // 1. Return this object's configuration value
        //    Note: This results in a new object being created and initialized
        //    from configuration each time this method is called:
        //
        // m_spIPropertyStoreConfiguration represents the MediaKeySystemConfiguration.
        // Return a new clone of m_spIPropertyStoreConfiguration.
        //

        IF_FAILED_GOTO( PSCreateMemoryPropertyStore( IID_PPV_ARGS( &spIPropertyStoreConfiguration ) ) );
        IF_FAILED_GOTO( _ClonePropertyStore( m_spIPropertyStoreConfiguration.Get(), spIPropertyStoreConfiguration.Get() ) );

        *configuration = spIPropertyStoreConfiguration.Detach();

    done:
        return hr;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleAccess::GetKeySystem(
    _Outptr_ LPWSTR *keySystem
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-getconfiguration
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-keysystem
        //
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;

        //
        // Only a single key system is supported.  Return its fixed value.
        //
        *keySystem = (LPWSTR)SysAllocString(g_wszKeySystem_clearkey);
        IF_NULL_GOTO( keySystem, E_OUTOFMEMORY );

    done:
        return hr;
    } );

    return hrRet;
}

Cdm_clearkey_IMFContentDecryptionModule::Cdm_clearkey_IMFContentDecryptionModule()
{
}

Cdm_clearkey_IMFContentDecryptionModule::~Cdm_clearkey_IMFContentDecryptionModule()
{
}

HRESULT Cdm_clearkey_IMFContentDecryptionModule::RuntimeClassInitialize(
    _In_ IPropertyStore *pIPropertyStoreCDM3,
    _In_ IPropertyStore *pIPropertyStoreConfiguration
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
    //
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    BOOL    fFound = FALSE;
    ComPtr<IPropertySet> spIPropertySetPMP;
    ComPtr<IPropertySet> spIPropertySetSystems;
    ComPtr<IMediaProtectionPMPServerFactory> spIMediaProtectionPMPServerFactory;
    PROPVARIANTWRAP propvar;

    //
    // Note: Refer to Cdm_clearkey_IMFContentDecryptionModuleAccess::CreateContentDecryptionModule for algorithm step 2.9, 2.11, and 3.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
    //
    // Note: Step 1 is handled by the caller.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
    //
    // 2.1. Let configuration be the value of this object's configuration value.
    // 2.2. Let use distinctive identifier be true if the value of configuration's distinctiveIdentifier member is "required" and false otherwise.
    // 2.3. Let persistent state allowed be true if the value of configuration's persistentState member is "required" and false otherwise.
    //
    // These steps are automatically handled by the fact that we take a copy of the
    // configuration itself (pIPropertyStoreConfiguration) as member variable m_spIPropertyStoreConfiguration
    // below and then query the configuration as needed elsewhere.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
    //
    // 2.4. Load and initialize the Key System implementation represented by this object's cdm implementation value if necessary.
    //
    // This involves creating a property set for Media Foundation representing the protection system
    // and setting up the Protected Media Process if possible.  (The PMP will be set up later if not.)
    //

    //
    // Set MediaProtectionSystemIdMapping with:
    // pmp["...protection system id..."] = "org.w3.clearkey"
    // mpm.Properties["Windows.Media.Protection.MediaProtectionSystemId"] = "...protection system id..."
    //
    IF_FAILED_GOTO( Windows::Foundation::ActivateInstance( HStringReference( RuntimeClass_Windows_Foundation_Collections_PropertySet ).Get(), &spIPropertySetPMP ) );
    IF_FAILED_GOTO( _AddStringToPropertySet( spIPropertySetPMP.Get(), L"Windows.Media.Protection.MediaProtectionSystemId", CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID_STRING ) );

    IF_FAILED_GOTO( Windows::Foundation::ActivateInstance( HStringReference( RuntimeClass_Windows_Foundation_Collections_PropertySet ).Get(), &spIPropertySetSystems ) );
    IF_FAILED_GOTO( _AddStringToPropertySet( spIPropertySetSystems.Get(), CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID_STRING, g_wszKeySystem_clearkey ) );

    IF_FAILED_GOTO( _AddPropertyToSet( spIPropertySetPMP.Get(), L"Windows.Media.Protection.MediaProtectionSystemIdMapping", spIPropertySetSystems.Get() ) );

    //
    // Because we are a store app, we also need to check and pass our package full name string
    //
    hr = pIPropertyStoreCDM3->GetValue( MF_CONTENTDECRYPTIONMODULE_PMPSTORECONTEXT, &propvar );
    if ( hr == S_OK )
    {
        IF_FALSE_GOTO( propvar.vt == VT_LPWSTR, MF_E_UNEXPECTED );

        IF_FAILED_GOTO( _AddStringToPropertySet( spIPropertySetPMP.Get(), L"Windows.Media.Protection.PMPStoreContext", propvar.pwszVal ) );
    }

    IF_FAILED_GOTO( GetActivationFactory( HStringReference( RuntimeClass_Windows_Media_Protection_MediaProtectionPMPServer ).Get(), spIMediaProtectionPMPServerFactory.GetAddressOf() ) );

    //
    // To disable the protected process, run the following command-line as an administrator.
    // Note that debugging the protected process will still fail without special debug credentials from Microsoft
    // and your own PEAuth (protected environment authorization) code should cause your own playback to fail.
    //
    // reg add "HKLM\Software\Microsoft\Windows Media Foundation\PEAuth" /v NoProtectedProcess /d 1
    //
    IF_FAILED_GOTO( spIMediaProtectionPMPServerFactory->CreatePMPServer( spIPropertySetPMP.Get(), &m_spIMediaProtectionPMPServer ) );

    //
    // If this fails, we can assume that SetPMPHostApp will get called later
    // OR that CreateTrustedInput will receive a host as an input parameter
    //
    (void)this->GetService( MF_CONTENTDECRYPTIONMODULE_SERVICE, IID_PPV_ARGS( &m_spIMFPMPHostApp ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
    //
    // 2.5. Let instance be a new instance of the Key System implementation represented by this object's cdm implementation value.
    //
    // The 'this' pointer (along with its member variables) is sufficient.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
    //
    // 2.6. Initialize instance to enable, disable and/or select Key System features using configuration.
    // 2.7. If use distinctive identifier is false, prevent instance from using Distinctive Identifier(s) and Distinctive Permanent Identifier(s).
    // 2.8. If persistent state allowed is false, prevent instance from persisting any state related to the application or origin of this object's Document.
    //
    // These steps are automatically handled by the fact that we take a copy of the
    // configuration itself (pIPropertyStoreConfiguration) as member variable m_spIPropertyStoreConfiguration
    // below and then query the configuration as needed elsewhere.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
    //
    // 2.9. If any of the preceding steps failed, reject promise with a new DOMException whose name is the appropriate error name.
    //
    // A failure before this point will return an error code to the caller.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
    //
    // 2.10. Let media keys be a new MediaKeys object, and initialize it as follows:
    // 2.10.1. Let the use distinctive identifier value be use distinctive identifier.
    // 2.10.2. Let the persistent state allowed value be persistent state allowed.
    // 2.10.3. Let the supported session types value be be the value of configuration's sessionTypes member.
    // 2.10.4. Let the cdm implementation value be this object's cdm implementation value.
    // 2.10.5. Let the cdm instance value be instance.
    //
    // These steps are automatically handled by the fact that we take a copy of the
    // configuration itself (pIPropertyStoreConfiguration) as member variable m_spIPropertyStoreConfiguration
    // below and then query the configuration as needed elsewhere.
    //
    IF_FAILED_GOTO( MFCreateAttributes( &m_spIMFAttributes, 1 ) );
    IF_FAILED_GOTO( PSCreateMemoryPropertyStore( IID_PPV_ARGS( &m_spIPropertyStoreConfiguration ) ) );
    IF_FAILED_GOTO( _ClonePropertyStore( pIPropertyStoreConfiguration, m_spIPropertyStoreConfiguration.Get() ) );

    IF_FAILED_GOTO( PSCreateMemoryPropertyStore( IID_PPV_ARGS( &m_spIPropertyStoreCDM3 ) ) );
    IF_FAILED_GOTO( _ClonePropertyStore( pIPropertyStoreCDM3, m_spIPropertyStoreCDM3.Get() ) );

    IF_FAILED_GOTO( _CopyStringAttributeFromIPropertyStoreToIMFAttributes( m_spIPropertyStoreCDM3.Get(), MF_EME_CDM_STOREPATH, this, CLEARKEY_GUID_ATTRIBUTE_DEFAULT_STORE_PATH, &fFound ) );
    IF_FALSE_GOTO( fFound, MF_E_UNEXPECTED );   // Default store path should always be available
    IF_FAILED_GOTO( _CopyStringAttributeFromIPropertyStoreToIMFAttributes( m_spIPropertyStoreCDM3.Get(), MF_EME_CDM_INPRIVATESTOREPATH, this, CLEARKEY_GUID_ATTRIBUTE_IN_PRIVATE_STORE_PATH, nullptr ) );

    IF_FAILED_GOTO( this->SetGUID( CLEARKEY_GUID_ATTRIBUTE_NEXT_AVAILABLE_GUID_KEY_DATA, CLEARKEY_GUID_ATTRIBUTE_KEYDATA_LIST_BASE ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemaccess-createmediakeys
    //
    // 11. Resolve promise with media keys.
    // 3. Return promise.
    //
    // Return success.
    //

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModule::SetContentEnabler(
    _In_ IMFContentEnabler *contentEnabler,
    _In_ IMFAsyncResult    *result )
{
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        ComPtr<IMFAttributes> spIMFAttributesCE;
        ComPtr<IMFContentEnabler> spIMFContentEnabler;

        IF_NULL_GOTO( contentEnabler, E_INVALIDARG );
        IF_NULL_GOTO( result, E_INVALIDARG );

        IF_FAILED_GOTO( contentEnabler->QueryInterface( IID_PPV_ARGS( &spIMFAttributesCE ) ) );

        IF_FAILED_GOTO( spIMFAttributesCE->SetUnknown( CLEARKEY_GUID_ATTRIBUTE_CE_PENDING_RESULT, result ) );

        //
        // Enumerate through *existing* keys and add them to the CE.
        //
        IF_FAILED_GOTO( ForAllKeys( this, CLEARKEY_INVALID_SESSION_ID, [&spIMFAttributesCE]( REFGUID guidKeyDataLambda, BYTE *pbKeyDataLambda, BOOL *pfHaltEnumerationLambda )->HRESULT
        {
            return spIMFAttributesCE->SetBlob( guidKeyDataLambda, pbKeyDataLambda, KEY_DATA_TOTAL_SIZE );
        } ) );

        {
            //
            // Done enumerating existing keys.  Try to complete the CE now.
            //
            GUID guidCE = CLEARKEY_GUID_ATTRIBUTE_CE_PENDING_REQUEST_LIST_BASE;

            hr = contentEnabler->MonitorEnable();
            if( hr != S_FALSE )
            {
                IF_FAILED_GOTO( hr );

                //
                // This key was enough to complete the CE so MonitorEnable invoked the IMFAsyncResult.  We're done!
                //
                goto done;
            }
            hr = S_OK;

            //
            // A key that could complete the CE was not found.
            // So, add it to the list of pending CEs.
            // Update will attempt to complete them whenever it sees a new key.
            //
            ADD_TO_GUID( guidCE, m_cCEs );
            m_cCEs++;
            IF_FAILED_GOTO( this->SetUnknown( guidCE, contentEnabler ) );
        }

    done:
        return hr;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModule::GetSuspendNotify( _COM_Outptr_ IMFCdmSuspendNotify **notify )
{
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        ComPtr<IMFCdmSuspendNotify> spIMFCdmSuspendNotify;

        hr = MakeAndInitialize<Cdm_clearkey_IMFCdmSuspendNotify, IMFCdmSuspendNotify>( notify, this );
        IF_FAILED_GOTO( hr );

    done:
        return hr;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModule::SetPMPHostApp( _In_ IMFPMPHostApp *pPMPHostApp )
{
    HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        if( m_spIMFPMPHostApp == nullptr )
        {
            m_spIMFPMPHostApp = pPMPHostApp;
        }
        return S_OK;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModule::CreateSession(
    _In_ MF_MEDIAKEYSESSION_TYPE sessionType,
    _In_ IMFContentDecryptionModuleSessionCallbacks *callbacks,
    _COM_Outptr_ IMFContentDecryptionModuleSession **session
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // Note: Part of this algorithm is implemented inside Cdm_clearkey_IMFContentDecryptionModuleSession::RuntimeClassInitialize as noted below
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        BOOL fSupportedSessionType = FALSE;
        PROPVARIANTWRAP propvar;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
        //
        // 1. If this object's supported session types value does not contain sessionType, throw [WebIDL] a NotSupportedError.
        //
        // Note that an unspecified session type implies 'temporary'.
        //
        IF_FAILED_GOTO( m_spIPropertyStoreConfiguration->GetValue( MF_EME_SESSIONTYPES, &propvar ) );
        if( propvar.vt != VT_EMPTY )
        {
            IF_FALSE_GOTO( propvar.vt == ( VT_VECTOR | VT_UI4 ), MF_TYPE_ERR );
            for( DWORD iSessionType = 0; iSessionType < propvar.caul.cElems && !fSupportedSessionType; iSessionType++ )
            {
                IF_FALSE_GOTO( propvar.vt == ( VT_VECTOR | VT_UI4 ), MF_E_UNEXPECTED );     // Should have been guaranteed by _ValidateSessionTypes
                fSupportedSessionType = ( propvar.caul.pElems[ iSessionType ] == sessionType );
            }
            IF_FALSE_GOTO( fSupportedSessionType, MF_NOT_SUPPORTED_ERR );
        }
        else
        {
            IF_FALSE_GOTO( sessionType == MF_MEDIAKEYSESSION_TYPE_TEMPORARY, MF_NOT_SUPPORTED_ERR );
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
        //
        // Note
        //   sessionType values for which the Is persistent session type ? algorithm returns true
        //   will fail if this object's persistent state allowed value is false.
        //
        if( m_spIPropertyStoreConfiguration != nullptr && sessionType == MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE )
        {
            BOOL fSupportedConfiguration = TRUE;
            BOOL fPersistentStateAllowed = TRUE;
            IF_FAILED_GOTO( _ValidatePersistentState( m_spIPropertyStoreConfiguration.Get(), &fSupportedConfiguration, &fPersistentStateAllowed ) );
            IF_FALSE_GOTO( fSupportedConfiguration && fPersistentStateAllowed, MF_NOT_SUPPORTED_ERR );
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
        //
        // 2. If the implementation does not support MediaKeySession operations
        //    in the current state, throw [WebIDL] an InvalidStateError.
        //
        // There is no state where this needs to occur.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
        //
        // 3. Let session be a new MediaKeySession object, and initialize it as follows:
        //
        // Remainder of step 3 occurs in Cdm_clearkey_IMFContentDecryptionModuleSession::RuntimeClassInitialize
        //
        hr = MakeAndInitialize<Cdm_clearkey_IMFContentDecryptionModuleSession, IMFContentDecryptionModuleSession>( session, this, sessionType, callbacks );
        IF_FAILED_GOTO( hr );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
        //
        // Return session.
        //
        // Return 'session' output parameter.
        //

    done:
        return hr;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModule::SetServerCertificate(
    _In_reads_bytes_opt_(certificateSize) const BYTE *certificate,
    _In_ DWORD certificateSize
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-setservercertificate
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#clear-key-capabilities
    //
    // 9.1.1. The setServerCertificate() method: Not supported.
    //
    TRACE_FUNC();
    HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        return MF_NOT_SUPPORTED_ERR;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModule::CreateTrustedInput(
    _In_reads_bytes_opt_(contentInitDataSize) const BYTE *contentInitData,
    _In_ DWORD contentInitDataSize,
    _COM_Outptr_ IMFTrustedInput **trustedInput
)
{
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        LARGE_INTEGER    large                  = { 0 };
        ULARGE_INTEGER   ularge                 = { 0 };
        BOOL             fFound                 = FALSE;
        BYTE            *pbPSSHFromGenRequest   = nullptr;
        UINT32           cbPSSHFromGenRequest   = 0;
        ComPtr<IMFAttributes> spIMFAttributesForTI;
        ComPtr<IStream> spIStream;
        ComPtr<IMFTrustedInput> spIMFTrustedInput;
        ComPtr<IUnknown> spIUnknown;

        IF_FAILED_GOTO( MFCreateAttributes( &spIMFAttributesForTI, 0 ) );

        IF_FAILED_GOTO( _CopyStringAttributeFromIPropertyStoreToIMFAttributes( m_spIPropertyStoreCDM3.Get(), MF_EME_CDM_STOREPATH, spIMFAttributesForTI.Get(), CLEARKEY_GUID_ATTRIBUTE_DEFAULT_STORE_PATH, &fFound ) );
        IF_FALSE_GOTO( fFound, MF_E_UNEXPECTED );   // Default store path should always be available
        IF_FAILED_GOTO( _CopyStringAttributeFromIPropertyStoreToIMFAttributes( m_spIPropertyStoreCDM3.Get(), MF_EME_CDM_INPRIVATESTOREPATH, spIMFAttributesForTI.Get(), CLEARKEY_GUID_ATTRIBUTE_IN_PRIVATE_STORE_PATH, nullptr ) );

        //
        // Favor any init data that the application specifically provided over the init data from the content.
        //
        hr = this->GetAllocatedBlob( CLEARKEY_GUID_ATTRIBUTE_PSSH_FROM_CONTENT_FILE_OR_GEN_REQUEST, &pbPSSHFromGenRequest, &cbPSSHFromGenRequest );
        if( SUCCEEDED( hr ) )
        {
            //
            // Serialize the init data from the last call to generate request
            //
            IF_FAILED_GOTO( spIMFAttributesForTI->SetBlob( CLEARKEY_GUID_ATTRIBUTE_PSSH_FROM_CONTENT_FILE_OR_GEN_REQUEST, pbPSSHFromGenRequest, cbPSSHFromGenRequest ) );
        }
        else
        {
            if( MF_E_ATTRIBUTENOTFOUND == hr )
            {
                hr = S_OK;
            }
            IF_FAILED_GOTO( hr );
            if( contentInitDataSize > 0 )
            {
                //
                // Serialize the init data from the content
                //
                IF_NULL_GOTO( contentInitData, E_INVALIDARG );
                IF_FAILED_GOTO( spIMFAttributesForTI->SetBlob( CLEARKEY_GUID_ATTRIBUTE_PSSH_FROM_CONTENT_FILE_OR_GEN_REQUEST, contentInitData, contentInitDataSize) );
            }
        }

        //
        // Serialize a reference to ourselves.  This will allow the MFT to have read/write access to the keys we've acquired.
        //
        IF_FAILED_GOTO( this->QueryInterface( IID_PPV_ARGS( &spIUnknown ) ) );
        IF_FAILED_GOTO( spIMFAttributesForTI->SetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_TI, spIUnknown.Get() ) );

        IF_FAILED_GOTO( CreateStreamOnHGlobal( nullptr, TRUE, &spIStream ) );
        IF_FAILED_GOTO( MFSerializeAttributesToStream( spIMFAttributesForTI.Get(), MF_ATTRIBUTE_SERIALIZE_UNKNOWN_BYREF, spIStream.Get() ) );
        IF_FAILED_GOTO( spIStream->Seek( large, STREAM_SEEK_SET, &ularge ) );

        if( m_spIMFPMPHostApp == nullptr )
        {
            IF_FAILED_GOTO( MF_E_UNEXPECTED );
        }

        IF_FAILED_GOTO( m_spIMFPMPHostApp->ActivateClassById( RuntimeClass_org_w3_clearkey_TrustedInput, spIStream.Get(), IID_PPV_ARGS( &spIMFTrustedInput ) ) );

        *trustedInput = spIMFTrustedInput.Detach();

    done:
        CoTaskMemFree( pbPSSHFromGenRequest );
        return hr;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModule::GetProtectionSystemIds(
    _Outptr_result_buffer_all_( *count ) GUID **systemIds,
    _Out_ DWORD *count
)
{
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        GUID *pGuid = nullptr;

        pGuid = (GUID*)CoTaskMemAlloc( sizeof( GUID ) );
        IF_NULL_GOTO( pGuid, E_OUTOFMEMORY );
        *pGuid = CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID;

        *systemIds = pGuid;
        *count = 1;
        pGuid = nullptr;

    done:
        CoTaskMemFree( pGuid );
        return hr;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModule::GetService( __RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID *ppvObject )
{
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;

        IF_FALSE_GOTO( MF_CONTENTDECRYPTIONMODULE_SERVICE == guidService, MF_E_UNSUPPORTED_SERVICE );
        IF_NULL_GOTO( m_spIMediaProtectionPMPServer, MF_INVALID_STATE_ERR );

        if( riid == CLEARKEY_GUID_NOTIFY_KEY_STATUS )
        {
            //
            // We are overloading this function in order to avoid writing another interface that
            // must be marshalled cross-process between the application and mfpmp.exe.
            // Specifically, when this specific riid is passed, we are using it to tell us
            // that one or more key statuses have changed and that we should notify each
            // session for which this occurred.
            //
            CTUnkArray<IMFContentDecryptionModuleSessionCallbacks> rgCallbacksToInvoke;

            IF_FAILED_GOTO( ForAllKeys( this, CLEARKEY_INVALID_SESSION_ID, [this, &hr, &rgCallbacksToInvoke]( REFGUID guidKeyDataLambda, BYTE *pbKeyDataLambda, BOOL *pfHaltEnumerationLambda )->HRESULT
            {
                GUID               guidLid              = GUID_NULL;
                MF_MEDIAKEY_STATUS eCurrentStatus       = MF_MEDIAKEY_STATUS_INTERNAL_ERROR;
                MF_MEDIAKEY_STATUS eLastNotifiedStatus  = MF_MEDIAKEY_STATUS_INTERNAL_ERROR;
                double dblExpiration;

                //
                // Determine the key's current status, last-notified status, and expiration
                //
                GetKeyData( pbKeyDataLambda, nullptr, nullptr, nullptr, &eCurrentStatus, &guidLid, &eLastNotifiedStatus, &dblExpiration, nullptr );

                if( IS_STATUS_USABLE_OR_PENDING( eCurrentStatus ) && IsExpired( dblExpiration ) )
                {
                    //
                    // Move from usable or pending status to expired status
                    //
                    eCurrentStatus = MF_MEDIAKEY_STATUS_EXPIRED;
                    SetKeyDataUpdateStatus( pbKeyDataLambda, eCurrentStatus );
                    IF_FAILED_GOTO( this->SetBlob( guidKeyDataLambda, pbKeyDataLambda, KEY_DATA_TOTAL_SIZE ) );
                }

                if( eCurrentStatus != eLastNotifiedStatus )
                {
                    //
                    // Status has changed.  We will need to notify the corresponding session.
                    //
                    BOOL fNewCallback = TRUE;
                    ComPtr<IMFContentDecryptionModuleSessionCallbacks> spIMFContentDecryptionModuleSessionCallbacks;

                    //
                    // Add the key's callback to the list upon which we will call KeyStatusChanged, but only
                    // if it's not already in the list so we don't notify the application multiple times.
                    //
                    IF_FAILED_GOTO( this->GetUnknown( guidLid, IID_PPV_ARGS( &spIMFContentDecryptionModuleSessionCallbacks) ) );
                    for( DWORD iToInvoke = 0; iToInvoke < rgCallbacksToInvoke.GetSize() && fNewCallback; iToInvoke++ )
                    {
                        if( rgCallbacksToInvoke[ iToInvoke ] == spIMFContentDecryptionModuleSessionCallbacks.Get() )
                        {
                            fNewCallback = FALSE;
                        }
                    }
                    if( fNewCallback )
                    {
                        IF_FAILED_GOTO( rgCallbacksToInvoke.Add(spIMFContentDecryptionModuleSessionCallbacks.Get() ) );
                    }

                    //
                    // Update the key's last-notified-status so that we notify again until the status changes again.
                    //
                    SetKeyDataUpdateNotifiedStatus( pbKeyDataLambda, nullptr, eCurrentStatus, eCurrentStatus );
                    IF_FAILED_GOTO( this->SetBlob( guidKeyDataLambda, pbKeyDataLambda, KEY_DATA_TOTAL_SIZE ) );
                }

            done:
                return hr;
            } ) );

            //
            // Invoke each callback exactly once
            //
            for( DWORD iToInvoke = 0; iToInvoke < rgCallbacksToInvoke.GetSize(); iToInvoke++ )
            {
                rgCallbacksToInvoke[ iToInvoke ]->KeyStatusChanged();
            }
        }
        else if( riid == IID_IMediaProtectionPMPServer )
        {
            IF_FAILED_GOTO( m_spIMediaProtectionPMPServer.CopyTo( riid, ppvObject ) );
        }
        else
        {
            ComPtr<IMFGetService> spIMFGetService;
            IF_FAILED_GOTO( m_spIMediaProtectionPMPServer.As( &spIMFGetService ) );
            IF_FAILED_GOTO( spIMFGetService->GetService( MF_PMP_SERVICE, riid, ppvObject ) );
        }

    done:
        return hr;
    } );

    return hrRet;
}

Cdm_clearkey_IMFContentDecryptionModuleSession::Cdm_clearkey_IMFContentDecryptionModuleSession()
{
}

Cdm_clearkey_IMFContentDecryptionModuleSession::~Cdm_clearkey_IMFContentDecryptionModuleSession()
{
    //
    // These will point to the same buffer if the private store path wasn't specified
    //
    if( m_pwszDefaultStorePath == m_pwszInPrivateStorePath )
    {
        CoTaskMemFree( m_pwszDefaultStorePath );
        m_pwszDefaultStorePath = nullptr;
        m_pwszInPrivateStorePath = nullptr;
    }
    else
    {
        CoTaskMemFree( m_pwszDefaultStorePath );
        m_pwszDefaultStorePath = nullptr;
        CoTaskMemFree( m_pwszInPrivateStorePath );
        m_pwszInPrivateStorePath = nullptr;
    }
    if( m_hPersistentSessionFile != INVALID_HANDLE_VALUE )
    {
        CloseHandle( m_hPersistentSessionFile );
        m_hPersistentSessionFile = INVALID_HANDLE_VALUE;
    }
}

static int _GenerateRandomSessionId()
{
    //
    // To simplify representation of the session ID as a string,
    // we generate a random number in the range 0x0F000000 to 0x0FFFFFFF,
    // i.e. 251658240 - 268435455, so that the strlen is always 9 chars.
    //
    return ( rand() & (int)0x0FFFFFFF ) | (int)0x0F000000;
}

HRESULT Cdm_clearkey_IMFContentDecryptionModuleSession::RuntimeClassInitialize(
    _In_ IMFAttributes *pIMFAttributesFromCDM,
    _In_ MF_MEDIAKEYSESSION_TYPE eSessionType,
    _In_ IMFContentDecryptionModuleSessionCallbacks *pIMFContentDecryptionModuleSessionCallbacks )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // Note: Any parts of the algorithm not mentioned in this function is
    // implemented inside Cdm_clearkey_IMFContentDecryptionModule::CreateSession.
    //

    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    ComPtr<IMFAttributes> spIMFAttributes;

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // 3.1. Let the sessionId attribute be the empty string.
    //
    // 'm_sessionId' is declaration-initialized to CLEARKEY_INVALID_SESSION_ID and
    // Cdm_clearkey_IMFContentDecryptionModuleSession::GetSessionId returns the empty string when it is CLEARKEY_INVALID_SESSION_ID.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // 3.2. Let the expiration attribute be NaN.
    //
    // m_dblExpiration is declaration-initialized to NaN and get_Expiration always returns m_dblExpiration.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // Note: Step 3.3 is handled by the caller.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // 3.4. Let key status be a new empty MediaKeyStatusMap object, and initialize it as follows:
    // 3.4.1. Let the size attribute be 0.
    //
    // Cdm_clearkey_IMFContentDecryptionModuleSession::GetKeyStatuses automatically returns an empty list
    // until the session is populated with keys.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // 3.5. Let the session type value be sessionType.
    //
    m_eSessionType = eSessionType;

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // 3.6. Let the uninitialized value be true.
    //
    // m_fUninitialized is declaration-initialized to TRUE.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // 3.7. Let the callable value be false.
    //
    // m_sessionId is declaration-initialized to CLEARKEY_INVALID_SESSION_ID, and the 'callable' value is represented by
    // this->IsCallable() { return m_sessionId != CLEARKEY_INVALID_SESSION_ID; }
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // 3.8. Let the closing or closed value be false.
    //
    // m_fClosed is declaration-initialized to FALSE.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // 3.9. Let the use distinctive identifier value be this object's use distinctive identifier value.
    //
    // We never use distinctive identifiers, so the use distinctive identifier value
    // is always implied to take the value FALSE.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys-createsession
    //
    // 3.10. Let the cdm implementation value be this object's cdm implementation.
    // 3.11. Let the cdm instance value be this object's cdm instance.
    //
    // 'this' pointer represents both the cdm implementation value and the cdm instance value.
    //

    m_spIMFContentDecryptionModuleSessionCallbacks = pIMFContentDecryptionModuleSessionCallbacks;
    IF_FAILED_GOTO( MFCreateAttributes( &spIMFAttributes, 1 ) );
    IF_FAILED_GOTO( spIMFAttributes->SetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_SESSION, pIMFAttributesFromCDM ) );

    m_spIMFAttributes = spIMFAttributes;

    if( m_eSessionType == MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE )
    {
        IF_FAILED_GOTO( this->LoadStorePaths() );
    }

done:
    return hr;
}

static HRESULT _AddKeyDataToCDMAttributes(
    _Inout_ IMFAttributes *pIMFAttributesCDM,
    _In_ const BYTE pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _In_ IMFContentDecryptionModuleSessionCallbacks *pIMFContentDecryptionModuleSessionCallbacks
)
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    GUID guidCE      = CLEARKEY_GUID_ATTRIBUTE_CE_PENDING_REQUEST_LIST_BASE;
    GUID guidLid     = GUID_NULL;
    GUID guidKeyData = GUID_NULL;
    ComPtr<IMFAttributes> spIMFAttributesNextCE;

    IF_FAILED_GOTO( pIMFAttributesCDM->GetGUID( CLEARKEY_GUID_ATTRIBUTE_NEXT_AVAILABLE_GUID_KEY_DATA, &guidKeyData ) );

    //
    // Enumerate through *existing* CEs
    //
    hr = pIMFAttributesCDM->GetUnknown( guidCE, IID_PPV_ARGS( &spIMFAttributesNextCE ) );
    while( SUCCEEDED( hr ) || hr == MF_E_INVALIDTYPE )
    {
        if( SUCCEEDED( hr ) )
        {
            //
            // Found CE, hand it the new key
            //
            ComPtr<IMFContentEnabler> spIMFContentEnabler;
            IF_FAILED_GOTO( spIMFAttributesNextCE.As( &spIMFContentEnabler ) );

            CLEARKEY_DEBUG_OUT( "Handing key data blob to an existing CE and attempting enable" );
            IF_FAILED_GOTO( spIMFAttributesNextCE->SetBlob( guidKeyData, pbKeyData, KEY_DATA_TOTAL_SIZE ) );

            hr = spIMFContentEnabler->MonitorEnable();
            if( hr == S_FALSE )
            {
                // This key wasn't useful for this CE.  Ignore.
                // no-op
            }
            else
            {
                IF_FAILED_GOTO( hr );

                //
                // This CE was completed successfully.  Discard our reference to it.
                // Our next enumeration of this CE will return MF_E_INVALIDTYPE.
                //
                IF_FAILED_GOTO( pIMFAttributesCDM->SetUnknown( guidCE, nullptr ) );
            }
        }
        else
        {
            // There was a CE for this guid, but it was previously removed.  Ignore it.
            hr = S_OK;
        }

        //
        // Look for next CE
        //
        ADD_TO_GUID( guidCE, 1 );
        spIMFAttributesNextCE = nullptr;
        hr = pIMFAttributesCDM->GetUnknown( guidCE, IID_PPV_ARGS( &spIMFAttributesNextCE ) );
    }
    if( hr == MF_E_ATTRIBUTENOTFOUND )
    {
        //
        // Done enumerating CEs
        //
        hr = S_OK;
    }
    IF_FAILED_GOTO( hr );

    //
    // Cache the key in the CDM.  This automatically makes it available
    // to all (existing and future) ITAs and enables handing it to future CEs.
    //
    CLEARKEY_DEBUG_OUT( "Handing key data blob to the CDM for existing/future ITAs and for future CEs" );
    GetKeyData( pbKeyData, nullptr, nullptr, nullptr, nullptr, &guidLid, nullptr, nullptr, nullptr );
    IF_FAILED_GOTO( pIMFAttributesCDM->SetBlob( guidKeyData, pbKeyData, KEY_DATA_TOTAL_SIZE ) );
    IF_FAILED_GOTO( pIMFAttributesCDM->SetUnknown( guidLid, pIMFContentDecryptionModuleSessionCallbacks) );
    ADD_TO_GUID( guidKeyData, 1 );
    IF_FAILED_GOTO( pIMFAttributesCDM->SetGUID( CLEARKEY_GUID_ATTRIBUTE_NEXT_AVAILABLE_GUID_KEY_DATA, guidKeyData ) );

done:
    return hr;
}

static HRESULT _JsonStringToObject( _In_reads_bytes_( cbJson ) const BYTE *pbJson, _In_ DWORD cbJson, _COM_Outptr_ IJsonObject **ppIJsonObject )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    DWORD                            cch                = 0;
    WCHAR                           *pwszJSON           = nullptr;
    boolean                          fParsed            = false;
    ComPtr<IJsonValueStatics>        spIJsonValueStatics;
    ComPtr<IJsonValue>               spIJsonValue;

    //
    // Convert the JSON from UTF8 to UTF16 so we can use the WinRT JSON parser.
    //
    cch = cbJson + 1;   // Add one for null-terminator
    pwszJSON = new WCHAR[ cch ];
    IF_NULL_GOTO( pwszJSON, E_OUTOFMEMORY );
    ZeroMemory( pwszJSON, cch * sizeof( WCHAR ) );
    IF_FALSE_GOTO( 0 != MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        (const char*)pbJson,
        cbJson,
        pwszJSON,
        cch ), MF_TYPE_ERR );

    //
    // Parse the JSON using interfaces in Windows::Data::Json.
    // Ignore any unrecognized JSON element.
    //
    IF_FAILED_GOTO( GetActivationFactory( HStringReference( RuntimeClass_Windows_Data_Json_JsonValue ).Get(), spIJsonValueStatics.GetAddressOf() ) );

    IF_FAILED_GOTO( spIJsonValueStatics->TryParse( HStringReference( pwszJSON ).Get(), &spIJsonValue, &fParsed ) );
    IF_FALSE_GOTO( fParsed, MF_TYPE_ERR );

    IF_FAILED_GOTO( spIJsonValue->GetObject( ppIJsonObject ) );

done:
    delete[] pwszJSON;
    return hr;
}

static HRESULT _ParseExpirationFromJSON( _In_ IJsonObject *pIJsonObject, _Out_ double *pdblExpiration )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    HString hstr1;

    *pdblExpiration = NaN.f;

#if CLEARKEY_INCLUDE_MICROSOFT_SPECIFIC_EXTENSIONS

    //
    // Microsoft Extensions to Clearkey:
    //
    // Allow the "expiration" attribute to indicate YYYY/MM/DD/hh:mm:ss
    // e.g. 2020/01/31/11:30:00 represents January 31, 2020 at 11:30 am.
    //
    // All portions are required, e.g. just 2020/01/31 is NOT allowed.
    //

    //
    // If the passed in JSON object has the "expiration":"..." specifier, parse it.
    //
    if( SUCCEEDED( pIJsonObject->GetNamedString( HStringReference( g_wszexpiration ).Get(), hstr1.GetAddressOf() ) ) )
    {
        UINT  cchExpiration  = 0;
        WCHAR wszYear[ 5 ]   = { 0 };
        WCHAR wszMonth[ 3 ]  = { 0 };
        WCHAR wszDay[ 3 ]    = { 0 };
        WCHAR wszHour[ 3 ]   = { 0 };
        WCHAR wszMinute[ 3 ] = { 0 };
        WCHAR wszSecond[ 3 ] = { 0 };
        SYSTEMTIME st;
        FILETIME ft;
        ULARGE_INTEGER ularge;
        LPCWSTR pwszExpirationWeakRef = WindowsGetStringRawBuffer( hstr1.Get(), &cchExpiration );           // output length excludes the null terminator

        ZeroMemory( &st, sizeof( st ) );

        //
        // Time
        // Time MUST be equivalent to that represented in ECMAScript Time Values and Time Range[ ECMA-262 ].
        // Time will equal NaN if no such time exists or if the time is indeterminate.
        // It should never have the value Infinity.
        //

        //
        // https://tc39.github.io/ecma262/#sec-time-values-and-time-range
        //
        // Time is measured in ECMAScript as milliseconds since midnight at the beginning of 01 January, 1970 UTC.
        //

        //
        // First, verify format
        //
        IF_FALSE_GOTO( cchExpiration == EXPIRATION_STR_LEN, MF_TYPE_ERR );
        IF_FALSE_GOTO(
            ISNUM( pwszExpirationWeakRef[ 0 ] )
         && ISNUM( pwszExpirationWeakRef[ 1 ] )
         && ISNUM( pwszExpirationWeakRef[ 2 ] )
         && ISNUM( pwszExpirationWeakRef[ 3 ] )
         && pwszExpirationWeakRef[ 4 ] == L'/'
         && ISNUM( pwszExpirationWeakRef[ 5 ] )
         && ISNUM( pwszExpirationWeakRef[ 6 ] )
         && pwszExpirationWeakRef[ 7 ] == L'/'
         && ISNUM( pwszExpirationWeakRef[ 8 ] )
         && ISNUM( pwszExpirationWeakRef[ 9 ] )
         && pwszExpirationWeakRef[ 10 ] == L'/'
         && ISNUM( pwszExpirationWeakRef[ 11 ] )
         && ISNUM( pwszExpirationWeakRef[ 12 ] )
         && pwszExpirationWeakRef[ 13 ] == L':'
         && ISNUM( pwszExpirationWeakRef[ 14 ] )
         && ISNUM( pwszExpirationWeakRef[ 15 ] )
         && pwszExpirationWeakRef[ 16 ] == L':'
         && ISNUM( pwszExpirationWeakRef[ 17 ] )
         && ISNUM( pwszExpirationWeakRef[ 18 ] ), MF_TYPE_ERR );

        //
        // Convert to windows SYSTEMTIME structure
        //
        memcpy( wszYear, pwszExpirationWeakRef, 4 * sizeof( WCHAR ) );
        memcpy( wszMonth, &pwszExpirationWeakRef[ 5 ], 2 * sizeof( WCHAR ) );
        memcpy( wszDay, &pwszExpirationWeakRef[ 8 ], 2 * sizeof( WCHAR ) );
        memcpy( wszHour, &pwszExpirationWeakRef[ 11 ], 2 * sizeof( WCHAR ) );
        memcpy( wszMinute, &pwszExpirationWeakRef[ 14 ], 2 * sizeof( WCHAR ) );
        memcpy( wszSecond, &pwszExpirationWeakRef[ 17 ], 2 * sizeof( WCHAR ) );

        st.wYear = (WORD)_wtoi( wszYear );
        st.wMonth = (WORD)_wtoi( wszMonth );
        st.wDay = (WORD)_wtoi( wszDay );
        st.wHour = (WORD)_wtoi( wszHour );
        st.wMinute = (WORD)_wtoi( wszMinute );
        st.wSecond = (WORD)_wtoi( wszSecond );

        //
        // Convert to FILETIME structure
        //
        IF_FALSE_GOTO( SystemTimeToFileTime( &st, &ft ), MF_TYPE_ERR );
        ularge.LowPart = ft.dwLowDateTime;
        ularge.HighPart = ft.dwHighDateTime;

        //
        // FILETIME contains a 64-bit value representing the number
        // of 100-nanosecond intervals since January 1, 1601 (UTC).
        //
        // Convert to milliseconds since midnight at the beginning of 01 January, 1970 UTC.
        //
        ularge.QuadPart /= 10000000ull;     // Convert to seconds
        ularge.QuadPart -= 0x2B6109100ull;  // Seconds between 1601 and 1970
        ularge.QuadPart *= 1000ull;         // Convert to milliseconds

        *pdblExpiration = (double)ularge.QuadPart;
    }
    else
#endif  //CLEARKEY_INCLUDE_MICROSOFT_SPECIFIC_EXTENSIONS
    {
        goto done;
    }

done:
    return hr;
}

static HRESULT _ParsePolicyFromJSON( _In_ IJsonObject *pIJsonObject, _Out_ CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS *pPolicy )
{
#if CLEARKEY_INCLUDE_MICROSOFT_SPECIFIC_EXTENSIONS

    HRESULT hr = S_OK;
    TRACE_FUNC_HR( hr );
    HString hstr1;

    //
    // Microsoft Extensions to Clearkey:
    //
    // Allow the "policy" attribute to have be set to exactly three characters.
    // All three are required.  All three are single-byte integers.
    //
    // The first character represents dwHDCP, the second dwCGMSA, and the third dwDigitalOnly:
    // those are members of the In the CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS structure.
    //
    // Refer to the declaration of CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS
    // and its related constants in stdafx.h for more information.
    //

    if( SUCCEEDED( pIJsonObject->GetNamedString( HStringReference( g_wszpolicy ).Get(), hstr1.GetAddressOf() ) ) )
    {
        UINT    cchPolicy;
        LPCWSTR pwszPolicyWeakRef = WindowsGetStringRawBuffer( hstr1.Get(), &cchPolicy );           // output length excludes the null terminator
        WCHAR   wszNext[ 2 ] = { 0 };

        IF_FALSE_GOTO( cchPolicy == 3, MF_TYPE_ERR );

        wszNext[ 0 ] = pwszPolicyWeakRef[ 0 ];
        if( L'0' <= wszNext[ 0 ] && wszNext[ 0 ] <= L'2' )
        {
            pPolicy->dwHDCP = (DWORD)_wtoi( wszNext );
        }
        else
        {
            IF_FAILED_GOTO( MF_TYPE_ERR );
        }

        wszNext[ 0 ] = pwszPolicyWeakRef[ 1 ];
        if( L'0' <= wszNext[ 0 ] && wszNext[ 0 ] <= L'6' )
        {
            pPolicy->dwCGMSA = (DWORD)_wtoi( wszNext );
        }
        else
        {
            IF_FAILED_GOTO( MF_TYPE_ERR );
        }

        wszNext[ 0 ] = pwszPolicyWeakRef[ 2 ];
        if( L'0' <= wszNext[ 0 ] && wszNext[ 0 ] <= L'1' )
        {
            pPolicy->dwDigitalOnly = (DWORD)_wtoi( wszNext );
        }
        else
        {
            IF_FAILED_GOTO( MF_TYPE_ERR );
        }
    }

done:
    return hr;

#else   // CLEARKEY_INCLUDE_MICROSOFT_SPECIFIC_EXTENSIONS

    ZeroMemory( pPolicy, sizeof( CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS ) );
    return S_OK;

#endif  // CLEARKEY_INCLUDE_MICROSOFT_SPECIFIC_EXTENSIONS
}

static HRESULT _ParseTypeFromJSON( _In_ IJsonObject *pIJsonObject, _Out_ BOOL *pfTemporary )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    HString hstr1;

    //
    // If the passed in JSON object has the "type":"..." specifier, parse it.
    //
    if( SUCCEEDED( pIJsonObject->GetNamedString( HStringReference( g_wsztype ).Get(), hstr1.GetAddressOf() ) ) )
    {
        UINT    cchType  = 0;
        LPCWSTR pwszWeakRefType = WindowsGetStringRawBuffer( hstr1.Get(), &cchType );           // output length excludes the null terminator
        if( CSTR_EQUAL == CompareStringOrdinal( g_wszTemporary, -1, pwszWeakRefType, cchType, FALSE ) )
        {
            *pfTemporary = TRUE;
        }
        else if( CSTR_EQUAL == CompareStringOrdinal( g_wszPersistentLicense, -1, pwszWeakRefType, cchType, FALSE ) )
        {
            *pfTemporary = FALSE;
        }
        else
        {
            IF_FAILED_GOTO( MF_TYPE_ERR );
        }
    }
    else
    {
        //
        // Allow "temporary" versus "persistent-license" to be unspecified.  Default to "temporary".
        //
        *pfTemporary = TRUE;
    }

done:
    return hr;
}

static HRESULT _GetGUIDFromJSONObject( _In_ IJsonObject *pIJsonObject, _In_ const WCHAR *pwszName, _Out_writes_bytes_( sizeof( GUID ) ) BYTE pbGuid[ sizeof( GUID ) ] )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    LPCWSTR pwszWeakRef = nullptr;
    UINT    cch         = 0;
    HString hstr1;

    //
    // Assume the given JSON object has a Base64url-encoded GUID with name pwszName.
    // Fail if it doesn't.  Fail if we can't base64 decode it into a GUID.
    //
    IF_FAILED_GOTO( pIJsonObject->GetNamedString( HStringReference( pwszName ).Get(), hstr1.GetAddressOf() ) );
    pwszWeakRef = WindowsGetStringRawBuffer( hstr1.Get(), &cch );           // output length excludes the null terminator
    IF_FALSE_GOTO( cch == GUID_B64_URL_LEN, MF_TYPE_ERR );
    IF_FAILED_GOTO( ConvertB64urlStringToBigEndianSixteenBytes( pwszWeakRef, pbGuid ) );

done:
    return hr;
}

HRESULT Cdm_clearkey_IMFContentDecryptionModuleSession::LoadStorePaths()
{
    HRESULT hr = S_OK;

    if( m_pwszDefaultStorePath == nullptr )
    {
        UINT32 ui32unused = 0;                  // IMFAttributes::GetAllocatedString requires the output parameter for string length but we do not need it.
        ComPtr<IMFAttributes> spIMFAttributesCDM;
        IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_SESSION, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );

        hr = spIMFAttributesCDM->GetAllocatedString( CLEARKEY_GUID_ATTRIBUTE_DEFAULT_STORE_PATH, &m_pwszDefaultStorePath, &ui32unused );
        if( hr == MF_E_ATTRIBUTENOTFOUND )
        {
            hr = MF_E_UNEXPECTED;   // Default store path should always be available
        }
        IF_FAILED_GOTO( hr );

        hr = spIMFAttributesCDM->GetAllocatedString( CLEARKEY_GUID_ATTRIBUTE_IN_PRIVATE_STORE_PATH, &m_pwszInPrivateStorePath, &ui32unused );
        if( hr == MF_E_ATTRIBUTENOTFOUND )
        {
            m_pwszInPrivateStorePath = m_pwszDefaultStorePath;  // Use the default if in-private is not available
            hr = S_OK;
        }
        IF_FAILED_GOTO( hr );
    }

done:
    return hr;
}

static HRESULT _DeterminePersistentSessionFilePath( _In_ WCHAR *pwszInPrivateStorePath, _In_ int sessionId, _Out_ WCHAR **ppwszFileName )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    DWORD    cchFile        = 0;
    DWORD    cchDir         = 0;
    WCHAR   *pwszFileName   = nullptr;

    //
    // Each persistent-license session id will simply store all licenses
    // in a clear sequential file on disk with the file name being the session ID.
    //
    cchDir = (DWORD)wcslen( pwszInPrivateStorePath );
    cchFile = cchDir + SESSION_ID_STRLEN + 2;   // +1 for '\' path separator, +1 for null terminator
    pwszFileName = new WCHAR[ cchFile ];
    IF_NULL_GOTO( pwszFileName, E_OUTOFMEMORY );
    ZeroMemory( pwszFileName, cchFile * sizeof( WCHAR ) );
    memcpy( pwszFileName, pwszInPrivateStorePath, cchDir * sizeof( WCHAR ) );
    pwszFileName[ cchDir ] = L'\\';
    swprintf( &pwszFileName[ cchDir + 1 ], L"%d", sessionId );

    *ppwszFileName = pwszFileName;
    pwszFileName = nullptr;

done:
    delete[] pwszFileName;
    return hr;
}

static HRESULT _OpenPersistentSessionFile( _In_ DWORD dwCreationDispositionForCreateFile, _In_ WCHAR *pwszInPrivateStorePath, _In_ int sessionId, _Inout_ HANDLE *phFile )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    WCHAR   *pwszFileName   = nullptr;
    HANDLE   hFile          = INVALID_HANDLE_VALUE;

    if( *phFile == INVALID_HANDLE_VALUE )
    {
        IF_FAILED_GOTO( _DeterminePersistentSessionFilePath( pwszInPrivateStorePath, sessionId, &pwszFileName ) );

        hFile = CreateFileW(
            pwszFileName,
            GENERIC_READ | GENERIC_WRITE,
            0,  // Exclusive read/write
            nullptr,
            dwCreationDispositionForCreateFile,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr );
        if( hFile == INVALID_HANDLE_VALUE )
        {
            IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
            IF_FAILED_GOTO( MF_E_UNEXPECTED );
        }

        *phFile = hFile;
        hFile = INVALID_HANDLE_VALUE;
    }
    else
    {
        DWORD dwRet = SetFilePointer( *phFile, 0, nullptr, FILE_BEGIN );
        if( dwRet == INVALID_SET_FILE_POINTER )
        {
            IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
            IF_FAILED_GOTO( MF_E_UNEXPECTED );
        }
    }

done:
    if( hFile != INVALID_HANDLE_VALUE )
    {
        CloseHandle( hFile );
    }
    delete[] pwszFileName;
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleSession::Update(
    _In_reads_bytes_(initDataSize) const BYTE *response,
    _In_ DWORD responseSize
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        BOOL                             fTemporary             = FALSE;
        UINT                             cKeys                  = 0;
        WCHAR                           *pwszFileName           = nullptr;
        ComPtr<IMFAttributes>            spIMFAttributesCDM;
        ComPtr<IJsonObject>              spIJsonObject;
        ComPtr<IJsonArray>               spIJsonArray;
        ComPtr<IVector<IJsonValue *>>    spIVectorOfIJsonValue;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 1. If this object's closing or closed value is true, return a promise rejected with an InvalidStateError.
        //
        IF_FALSE_GOTO( !m_fClosed, MF_INVALID_STATE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 2. If this object's callable value is false, return a promise rejected with an InvalidStateError.
        //
        IF_FALSE_GOTO( this->IsCallable(), MF_INVALID_STATE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 3. If response is an empty array, return a promise rejected with a newly created TypeError.
        //
        IF_FALSE_GOTO( response != nullptr && response[ 0 ] != 0 && responseSize > 0, MF_TYPE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 4. Let response copy be a copy of the contents of the response parameter.
        //
        // Converting the response string to a JSON object takes a copy inside 'spIJsonObject'.
        //
        IF_FAILED_GOTO( _JsonStringToObject( response, responseSize, spIJsonObject.GetAddressOf() ) );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // Note: Step 5 is handled by caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6. Run the following steps
        // 6.1. Let sanitized response be a validated and/or sanitized version of response copy.
        // 6.2. If the preceding step failed, or if sanitized response is empty, reject promise with a newly created TypeError.
        //
        // Converting the response string to a JSON object does thorough validation.
        // Thus, 'spIJsonObject' is already a validated version of response copy,
        // and convesion would have failed (and thus rejected the promise) if validation failed.
        //
        // Note: Additional validation occurs during parsing below.
        //       The promise will also be rejected with a newly created TypeError if that validation fails.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.3. Let message be null.
        // 6.4. Let message type be null.
        //
        // We never sends a message during Update, so these are always null.
        // No action is required here.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.5. Let session closed be false.
        //
        // This is implied.  The value is only written to TRUE by the algorithm at step 6.7.2
        // and only read by the algorithm at step 6.8.1.  We run the
        // Session Closed algorithm [by calling Close()] directly at step 6.7.2
        // so there is no need for a separate 'session closed' variable.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.6. Let cdm be the CDM instance represented by this object's cdm instance value.
        //
        IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_SESSION, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.7. Use the cdm to execute the following steps:
        // 6.7.1. If the format of sanitized response is invalid in any way, reject promise with a newly created TypeError.
        //
        // Converting the response string to a JSON object does thorough validation.
        // Thus, 'spIJsonObject' is already a validated version of response copy,
        // and conversion would have failed (and thus rejected the promise) if validation failed.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.7.2. Process sanitized response, following the stipulation for the first matching condition from the following list:
        //     If sanitized response contains a license or key(s)
        //
        // We determine whether it has keys by looking for the JSON "keys" array.
        // If we find it, we assume that the response contains keys.  We'll still fail if other required objects are missing.
        // If we don't, we assume that the response is a record of license destruction acknowledgement.
        // We'll still fail if other required objects are missing.
        //
        // "keys":...
        //
        hr = spIJsonObject->GetNamedArray( HStringReference( g_wszkeys ).Get(), &spIJsonArray );

        if( SUCCEEDED( hr ) )
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
            //
            // 6.7.2. Process sanitized response, following the stipulation for the first matching condition from the following list:
            //     If sanitized response contains a license or key(s)
            //         Process sanitized response, following the stipulation for the first matching condition from the following list:
            //         ...
            //         Otherwise
            //             Reject promise with a newly created TypeError.
            //
            // If(temporary {} Else If(persistent-license) {} Else {'Otherwise'} set of conditions
            // are logically-equivalent to
            // If( !temporary && !persistent-license ) {} Else If( temporary ) Else If( persistent-license)
            // We use the latter logic, so we handle the 'Otherwise' case first.
            //
            IF_FAILED_GOTO( _ParseTypeFromJSON( spIJsonObject.Get(), &fTemporary ) );
            IF_FALSE_GOTO( ( m_eSessionType == MF_MEDIAKEYSESSION_TYPE_TEMPORARY ) == fTemporary, MF_TYPE_ERR );    // License and session type must match

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
            //
            // 6.8.1.2. If the expiration time for the session changed, run the Update Expiration algorithm on the session, providing the new expiration time.
            //
            // We perform this step early since the expiration comes in the JSON and the entire
            // algorithm boils down to updating m_dblExpiration.
            //
            IF_FAILED_GOTO( _ParseExpirationFromJSON( spIJsonObject.Get(), &m_dblExpiration ) );

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
            //
            // 6.7.2. ...
            //         Process sanitized response, following the stipulation for the first matching condition from the following list:
            // 
            //         If sessionType is "temporary" and sanitized response does not specify that session data,
            //             including any license, key(s), or similar session data it contains, should be stored
            //             Process sanitized response, not storing any session data.
            //         If sessionType is "persistent-license" and sanitized response contains a persistable license
            //             Process sanitized response, storing the license/key(s) and related session data contained
            //             in sanitized response. Such data MUST be stored such that only the origin of this object's Document can access it. 
            //         ...
            //
            // Parse the org.w3.clearkey license format as documented here:
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#clear-key-license-format
            //

            //
            // "keys":...
            // (continued)
            //
            IF_FAILED_GOTO( spIJsonArray->QueryInterface( IID_PPV_ARGS( &spIVectorOfIJsonValue ) ) );
            IF_FAILED_GOTO( spIVectorOfIJsonValue->get_Size( &cKeys ) );
            IF_FALSE_GOTO( cKeys > 0, MF_TYPE_ERR );

            //
            // Replacement Algorithm
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
            //
            // 6.7.2. ...
            //        Note
            //        The replacement algorithm within a session is Key System-dependent.
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#clear-key-license-format
            //
            // The org.w3.clearkey documentation does not specify a replacement algorithm.
            //
            // Because it is both Key System-dependent AND not specified by the Key System definition,
            // each implementation must choose its own replacement algorithm.  We choose to fully
            // replace all acquired keys in the session any time new keys are provided via a call
            // to 'update'.  Thus, at this point, we delete all keys associated with this session.
            //

            IF_FAILED_GOTO( this->DestroyKeys( MF_MEDIAKEY_STATUS_INTERNAL_ERROR, nullptr, nullptr ) );

            if( m_eSessionType == MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE )
            {
                //
                // For the persistent-license case, we use CREATE_ALWAYS when opening the file.
                // This will truncate the file to zero bytes thus deleting all existing persistent keys.
                //
                IF_FAILED_GOTO( _OpenPersistentSessionFile( CREATE_ALWAYS, m_pwszInPrivateStorePath, m_sessionId, &m_hPersistentSessionFile ) );
            }

            //
            // Loop through the set of new keys we received
            //
            for( DWORD iKey = 0; iKey < cKeys; iKey++ )
            {
                LPCWSTR pwszWeakRef = nullptr;
                UINT    cch     = 0;
                GUID    guidKid = GUID_NULL;
                BYTE    rgbKid[ sizeof( GUID ) ];
                BYTE    rgbAES128Key[ KEY_DATA_KEY_SIZE ] = { 0 };
                BYTE    rgbKeyData[ KEY_DATA_TOTAL_SIZE ] = { 0 };
                HString hstr1;
                CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS oPolicy = { 0 };

                ComPtr<IJsonObject> spIJsonObjectKey;
                IF_FAILED_GOTO( spIJsonArray->GetObjectAt( iKey, &spIJsonObjectKey ) );

                //
                // Ensure the "kty" attribute is set to "oct".
                //
                IF_FAILED_GOTO( spIJsonObjectKey->GetNamedString( HStringReference( g_wszkty ).Get(), hstr1.GetAddressOf() ) );
                pwszWeakRef = WindowsGetStringRawBuffer( hstr1.Get(), &cch );           // output length excludes the null terminator
                IF_FALSE_GOTO( CSTR_EQUAL == CompareStringOrdinal( g_wszoct, -1, pwszWeakRef, cch, FALSE ), MF_TYPE_ERR );

                //
                // Parse the "k" (key) and "kid" attributes.
                //
                IF_FAILED_GOTO( _GetGUIDFromJSONObject( spIJsonObjectKey.Get(), g_wszk, rgbAES128Key ) );
                IF_FAILED_GOTO( _GetGUIDFromJSONObject( spIJsonObjectKey.Get(), g_wszkid, rgbKid ) );
                BIG_ENDIAN_SIXTEEN_BYTES_TO_GUID( rgbKid, &guidKid );

                //
                // Parse policy from the JSON
                //
                IF_FAILED_GOTO( _ParsePolicyFromJSON( spIJsonObjectKey.Get(), &oPolicy ) );

                //
                // Set up the key data.
                //
                IF_FAILED_GOTO( SetKeyDataInitial( rgbKeyData, m_sessionId, guidKid, rgbAES128Key, m_dblExpiration, &oPolicy ) );

                //
                // Save the key data in memory.
                //
                IF_FAILED_GOTO( _AddKeyDataToCDMAttributes( spIMFAttributesCDM.Get(), rgbKeyData, m_spIMFContentDecryptionModuleSessionCallbacks.Get() ) );

                if( m_eSessionType == MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE )
                {
                    //
                    // Save the persistent key data to disk.
                    // The key data is stored in binary form as a copy of the in-memory data
                    // with each key data simplly appended to the previous one.  In other words,
                    // the total file size will be (cKeys * KEY_DATA_TOTAL_SIZE).
                    //
                    // Note that the key status on disk is always "status-pending" or "released".
                    //
                    DWORD cbWritten = 0;
                    if( !WriteFile( m_hPersistentSessionFile, rgbKeyData, KEY_DATA_TOTAL_SIZE, &cbWritten, nullptr ) )
                    {
                        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
                        IF_FAILED_GOTO( MF_E_UNEXPECTED );
                    }
                    IF_FALSE_GOTO( cbWritten == KEY_DATA_TOTAL_SIZE, MF_TYPE_ERR );
                }

                //
                // Move on to the next key (if any).
                //
            }
        }
        else if( hr == WEB_E_JSON_VALUE_NOT_FOUND )
        {
            //
            // We didn't find the "keys" json object, so we assume this is a license destruction acknowledgement message.
            // We'll still fail if other required objects are missing.
            // See also: comments regarding step 6.5 above.
            //
            hr = S_OK;

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
            //
            // 6.7.2. Process sanitized response, following the stipulation for the first matching condition from the following list:
            //     ...
            //     If sanitized response contains a record of license destruction acknowledgement and sessionType is "persistent-license"
            //         Run the following steps:
            //             Close the key session and clear all stored session data associated with this object, including the sessionId and record of license destruction. 
            //             Set session closed to true.
            //
            // 6.8.1 If session closed is true:
            //         Run the Session Closed algorithm on this object.
            //
            if( m_eSessionType == MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE )
            {
                //
                // Confirm that the "kids" JSON array is present.
                // The algorithm for processing a "record of license destruction acknowledgement" message does not include
                // any action that depends on the set of kids being acknowledged - it's based on the session only.
                // So, we ignore the set of kids passed in.
                //
                IF_FAILED_GOTO( spIJsonObject->GetNamedArray( HStringReference( g_wszkids ).Get(), &spIJsonArray ) );

                IF_FAILED_GOTO( this->Close() );
                IF_FAILED_GOTO( _DeterminePersistentSessionFilePath( m_pwszInPrivateStorePath, m_sessionId, &pwszFileName ) );
                if( !DeleteFile( pwszFileName ) )
                {
                    hr = HRESULT_FROM_WIN32( GetLastError() );
                    if( hr == HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND ) )
                    {
                        hr = S_OK;
                    }
                    else
                    {
                        IF_FAILED_GOTO( hr );
                        IF_FAILED_GOTO( MF_E_UNEXPECTED );
                    }
                }
            }
        }
        else
        {
            IF_FAILED_GOTO( hr );
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.7.2. Process sanitized response, following the stipulation for the first matching condition from the following list:
        //     ...
        //     Otherwise
        //         Process sanitized response, not storing any session data. 
        //
        // We support no other responses.
        // Step 6.2 (including its "Note" regarding additional validation) would have
        // failed before this point for any other response.
        // Thus, no action is required here.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.7.3. If a message needs to be sent, execute the following steps:
        //
        // We never require a message to be sent as a result of Update.
        // Thus, no action is required here and step 6.7.3 is skipped entirely.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.8.1 If session closed is true:
        //         Run the Session Closed algorithm on this object.
        //         ...
        //
        // Refer to steps 6.5 and 6.8.1.  No action is required here.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.8.1 ...
        //       Otherwise:
        //         Run the following steps:
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.8.1.1. If the set of keys known to the CDM for this object changed or the status of any key(s) changed,
        //          run the Update Key Statuses algorithm on the session, providing each known key's key ID along with the appropriate MediaKeyStatus. 
        //          Should additional processing be necessary to determine with certainty the status of a key, use "status-pending".
        //          Once the additional processing for one or more keys has completed, run the Update Key Statuses algorithm again with the actual status(es). 
        //
        // We set the key status to "pending" while enumerating through the keys and adding them to our list.
        // Thus, there is nothing more to do except for firing the key status change event.
        //
        m_spIMFContentDecryptionModuleSessionCallbacks->KeyStatusChanged();

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.8.1.2. If the expiration time for the session changed, run the Update Expiration algorithm on the session, providing the new expiration time.
        //
        // We performed this step early during JSON parsing above.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.8.1.3. If any of the preceding steps failed, reject promise with a new DOMException whose name is the appropriate error name.
        //
        // A failure before this point will return an error code to the caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.8.1.4. If message is not null, run the Queue a "message" Event algorithm on the session, providing message type and message.
        //
        // We never send a message during Update, so these are always null.
        // No action is required here.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
        //
        // 6.8.2. Resolve promise.
        // 7. Return promise.
        //
        // Return success.
        //

    done:
        delete[] pwszFileName;
        return hr;
    } );

    return hrRet;
}

static HRESULT _CountSessionKeys( _In_ IMFAttributes *pIMFAttributesCDM, _In_ int sessionId, _Out_ DWORD *pcKeys )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);

    *pcKeys = 0;
    IF_FAILED_GOTO( ForAllKeys( pIMFAttributesCDM, sessionId, [&pcKeys]( REFGUID guidKeyDataLambda, BYTE *pbKeyDataLambda, BOOL *pfHaltEnumerationLambda )->HRESULT
    {
        *pcKeys = *pcKeys + 1;
        return S_OK;
    } ) );
done:
    return hr;
}

HRESULT Cdm_clearkey_IMFContentDecryptionModuleSession::DestroyKeys( _In_ MF_MEDIAKEY_STATUS eNewStatus, _Inout_ BYTE *pbMessage, _Inout_ DWORD *pibMessage )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    ComPtr<IMFAttributes> spIMFAttributesCDM;
    DWORD cKeys;
    DWORD iKey = 0;
    BOOL  fRemoving = ( m_eSessionType == MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE && eNewStatus == MF_MEDIAKEY_STATUS_RELEASED && m_hPersistentSessionFile != INVALID_HANDLE_VALUE );

    IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_SESSION, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );

    IF_FAILED_GOTO( _CountSessionKeys( spIMFAttributesCDM.Get(), m_sessionId, &cKeys ) );

    if( cKeys == 0 )
    {
        // Nothing to destroy
        goto done;
    }

    if( fRemoving )
    {
        IF_FAILED_GOTO( _OpenPersistentSessionFile( OPEN_EXISTING, m_pwszInPrivateStorePath, m_sessionId, &m_hPersistentSessionFile ) );
    }

    IF_FAILED_GOTO( ForAllKeys( spIMFAttributesCDM.Get(), m_sessionId, [this, &hr, &pbMessage, &pibMessage, &iKey, &spIMFAttributesCDM, &eNewStatus, &cKeys, &fRemoving]( REFGUID guidKeyDataLambda, BYTE *pbKeyDataLambda, BOOL *pfHaltEnumerationLambda )->HRESULT
    {
        BYTE rgbInvalidAES128Key[ KEY_DATA_KEY_SIZE ] = { 0 };
        if( iKey >= cKeys )
        {
            IF_FAILED_GOTO( MF_E_UNEXPECTED );
        }
        if( pbMessage != nullptr )
        {
            GUID guidKid;
            GetKeyData( pbKeyDataLambda, nullptr, &guidKid, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
            IF_FAILED_GOTO( _AddKidToMessage( pbMessage, pibMessage, guidKid, iKey + 1 < cKeys ) );
        }

        //
        // We will KeyStatusChanged on all codepaths that call this function,
        // so the last-notified-status should match the current status
        //
        CLEARKEY_DEBUG_OUT( "Removing key data blob from the CDM" );
        SetKeyDataUpdateNotifiedStatus( pbKeyDataLambda, rgbInvalidAES128Key, eNewStatus, eNewStatus );
        IF_FAILED_GOTO( spIMFAttributesCDM->SetBlob( guidKeyDataLambda, pbKeyDataLambda, KEY_DATA_TOTAL_SIZE ) );

        if( fRemoving )
        {
            //
            // We're removing the session, so make sure all the key statuses are set to 'released' on disk
            //
            DWORD cbWritten = 0;

            if( !WriteFile( m_hPersistentSessionFile, pbKeyDataLambda, KEY_DATA_TOTAL_SIZE, &cbWritten, nullptr ) )
            {
                IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
                IF_FAILED_GOTO( MF_E_UNEXPECTED );
            }
            IF_FALSE_GOTO( cbWritten == KEY_DATA_TOTAL_SIZE, MF_TYPE_ERR );
        }
        iKey++;

    done:
        return hr;
    } ) );

done:
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleSession::Close()
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
    //
    // This function also implements:
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#session-closed
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
        //
        // 1. If this object's closing or closed value is true, return a resolved promise.
        //
        if( m_fClosed )
        {
            goto done;
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
        //
        // 2. If this object's callable value is false, return a promise rejected with an InvalidStateError.
        //
        IF_FALSE_GOTO( this->IsCallable(), MF_INVALID_STATE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
        //
        // Note: Step 3 is handled by the caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
        //
        // 4. Set this object's closing or closed value to true.
        //
        m_fClosed = TRUE;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
        //
        // 5. Run the following steps:
        // 5.1. Let cdm be the CDM instance represented by this object's cdm instance value.
        //
        // The 'this' pointer (along with its member variables) is sufficient.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
        //
        // 5.2. Use cdm to close the key session associated with this object.
        //      Note
        //      Closing the key session results in the destruction of any license(s) and
        //      key(s) that have not been explicitly stored.
        //
        IF_FAILED_GOTO( this->DestroyKeys( MF_MEDIAKEY_STATUS_INTERNAL_ERROR, nullptr, nullptr ) );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
        //
        // 5.3. Run the following steps:
        // 5.3.1. Run the Session Closed algorithm on this object.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#session-closed
        //
        // 1. Let session be the associated MediaKeySession object.
        //
        // The 'this' pointer (along with its member variables) is sufficient.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#session-closed
        //
        // 2. Let promise be the session's closed attribute.
        // 3. If promise is resolved, abort these steps.
        //
        // If the promise was already resolved, we would have jumped to 'done' already.
        // No action is required here.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#session-closed
        //
        // 4. Set the session's closing or closed value to true.
        //
        // Already occurred at step 4 of Close above
        // (https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close)
        // No action is required here.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#session-closed
        //
        // 5. Run the Update Key Statuses algorithm on the session, providing an empty sequence.
        //
        // Since m_fClosed is now set to TRUE, GetKeyStatuses will always return the empty sequence.
        // Thus, there is nothing more to do except for firing the key status change event.
        //
        m_spIMFContentDecryptionModuleSessionCallbacks->KeyStatusChanged();

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#session-closed
        //
        // 6. Run the Update Expiration algorithm on the session, providing NaN.
        //
        // Simply set m_dblExpiration to NaN.
        //
        m_dblExpiration = NaN.f;

        //
        // In the persistent session case, this session may be holding onto a file handle
        // (acquired in Update or Load) which prevents a new session for the same session ID
        // from being opened while an existing MediaKeySession object is not closed (per step 8.3 in
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load).
        // Since we're closing this MediaKeySession object, we close that file so a call to Load
        // with the same session ID in another session object will start succeeding.
        //
        if( m_hPersistentSessionFile != INVALID_HANDLE_VALUE )
        {
            CloseHandle( m_hPersistentSessionFile );
            m_hPersistentSessionFile = INVALID_HANDLE_VALUE;
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#session-closed
        //
        // 7. Resolve promise.
        //
        // and
        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
        //
        // 5.3.2. Resolve promise.
        // 6. Return promise.
        //
        // Return success.
        //

    done:
        return hr;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleSession::GetKeyStatuses(
    _Outptr_result_buffer_(*numKeyStatuses) MFMediaKeyStatus **keyStatuses,
    _Out_ UINT *numKeyStatuses
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-keystatuses
    //
    // A reference to a read-only map of key IDs (known to the session) to the current
    // status of the associated key. Each entry MUST have a unique key ID.
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        DWORD iKey  = 0;
        DWORD cKeys = 0;
        MFMediaKeyStatus *pKeyStatusArray = nullptr;
        ComPtr<IMFAttributes> spIMFAttributesCDM;

        *keyStatuses = nullptr;
        *numKeyStatuses = 0;

        //
        // We maintain key status information for each Kid alongside the key itself in the CDM.
        //
        if( m_fClosed )
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
            //
            // 5. Run the Update Key Statuses algorithm on the session, providing an empty sequence.
            //
            // The session is closed.  Return an empty sequence.
            //
            goto done;
        }

        IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_SESSION, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );
        IF_FAILED_GOTO( _CountSessionKeys( spIMFAttributesCDM.Get(), m_sessionId, &cKeys ) );

        if( cKeys == 0 )
        {
            //
            // If we have zero keys, then neither update nor load has ever been called
            // and we're unaware of any keys at all.  Return an empty sequence.
            //
            goto done;
        }

        //
        // Allocate the key status map.
        //
        pKeyStatusArray = (MFMediaKeyStatus*)CoTaskMemAlloc( cKeys * sizeof( MFMediaKeyStatus ) );
        IF_NULL_GOTO( pKeyStatusArray, E_OUTOFMEMORY );
        ZeroMemory( pKeyStatusArray, cKeys * sizeof( MFMediaKeyStatus ) );

        //
        // Loop through the set of keys we know about.  
        //
        IF_FAILED_GOTO( ForAllKeys( spIMFAttributesCDM.Get(), m_sessionId, [this, &pKeyStatusArray, &hr, &iKey, &cKeys]( REFGUID guidKeyDataLambda, BYTE *pbKeyDataLambda, BOOL *pfHaltEnumerationLambda )->HRESULT
        {
            if( iKey >= cKeys )
            {
                IF_FAILED_GOTO( MF_E_UNEXPECTED );
            }

            //
            // Read the Kid and key status for this key
            //
            pKeyStatusArray[ iKey ].pbKeyId = (BYTE*)CoTaskMemAlloc( KEY_DATA_KID_SIZE );
            IF_NULL_GOTO( pKeyStatusArray[ iKey ].pbKeyId, E_OUTOFMEMORY );
            pKeyStatusArray[ iKey ].cbKeyId = KEY_DATA_KID_SIZE;

            GetKeyData( pbKeyDataLambda, nullptr, (GUID*)( pKeyStatusArray[ iKey ].pbKeyId ), nullptr, &pKeyStatusArray[ iKey ].eMediaKeyStatus, nullptr, nullptr, nullptr, nullptr );
            iKey++;
        done:
            return hr;
        } ) );

        if( iKey != cKeys )
        {
            IF_FAILED_GOTO( MF_E_UNEXPECTED );
        }

        *keyStatuses = pKeyStatusArray;
        pKeyStatusArray = nullptr;
        *numKeyStatuses = cKeys;

    done:
        if( pKeyStatusArray )
        {
            for( DWORD iKey = 0; iKey < cKeys; iKey++ )
            {
                CoTaskMemFree( pKeyStatusArray[ iKey ].pbKeyId );
            }

            CoTaskMemFree( pKeyStatusArray );
        }

        //
        // Note: A failure during this function is NOT propagated.
        // Instead, it will return an empty key status array.
        //
        // *** DO NOT change this line to return 'hr'. ***
        //
        return S_OK;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleSession::Load(
    _In_ LPCWSTR sessionId,
    _Out_ BOOL *loaded
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        int             dwSessionId   = CLEARKEY_INVALID_SESSION_ID;
        LARGE_INTEGER   large       = { 0 };
        LONGLONG        cKeys       = 0;
        BOOL            fReleased   = FALSE;
        BYTE           *pbMessage   = nullptr;
        DWORD           cbMessage   = 0;
        DWORD           ibMessage   = 0;
        BOOL            fHasData       = FALSE;
        ComPtr<IMFAttributes> spIMFAttributesCDM;

        *loaded = FALSE;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 1. If this object's closing or closed value is true, return a promise rejected with an InvalidStateError.
        //
        IF_FALSE_GOTO( !m_fClosed, MF_INVALID_STATE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 2. If this object's uninitialized value is false, return a promise rejected with an InvalidStateError.
        //
        IF_FALSE_GOTO( m_fUninitialized, MF_INVALID_STATE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 3. Let this object's uninitialized value be false.
        //
        m_fUninitialized = FALSE;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 4. If sessionId is the empty string, return a promise rejected with a newly created TypeError.
        //
        IF_FALSE_GOTO( wcslen( sessionId ) != 0, MF_TYPE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 5. If the result of running the Is persistent session type? algorithm on this object's
        //    session type is false, return a promise rejected with a newly created TypeError.
        //
        IF_FALSE_GOTO( m_eSessionType == MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE, MF_TYPE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // Note: Steps 6-7 are handled by caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8. Run the following steps:
        // 8.1. Let sanitized session ID be a validated and/or sanitized version of sessionId.
        // 8.2. If the preceding step failed, or if sanitized session ID is empty, reject promise with a newly created TypeError.
        //
        IF_FALSE_GOTO( wcslen( sessionId ) == SESSION_ID_STRLEN, MF_TYPE_ERR );
        for( DWORD iStr = 0; iStr < SESSION_ID_STRLEN; iStr++ )
        {
            IF_FALSE_GOTO( ISNUM( sessionId[ iStr ] ), MF_TYPE_ERR );
        }
        swscanf_s( sessionId, L"%d", &dwSessionId );
        IF_FALSE_GOTO( dwSessionId >= SESSION_ID_MIN_VAL, MF_TYPE_ERR );

        hr = _OpenPersistentSessionFile( OPEN_EXISTING, m_pwszInPrivateStorePath, dwSessionId, &m_hPersistentSessionFile );
        if( hr == HRESULT_FROM_WIN32( ERROR_SHARING_VIOLATION ) )
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
            //
            // 8.3. If there is a MediaKeySession object that is not closed in this object's Document
            // whose sessionId attribute is sanitized session ID, reject promise with a QuotaExceededError.
            //
            IF_FAILED_GOTO( MF_QUOTA_EXCEEDED_ERR );
        }
        else if( hr == HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND ) )
        {
            //
            // No data stored for the sanitized session ID.  Handled at step 8.8.1 below.
            //
            fHasData = FALSE;
            hr = S_OK;
        }
        else
        {
            IF_FAILED_GOTO( hr );
            fHasData = TRUE;
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.4. Let expiration time be NaN.
        //
        // Simply set m_dblExpiration to NaN.
        //
        m_dblExpiration = NaN.f;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.5. Let message be null.
        // 8.6. Let message type be null.
        //
        // Implied by variable initialization.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.7 Let cdm be the CDM instance represented by this object's cdm instance value.
        //
        // The 'this' pointer (along with its member variables) is sufficient.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.8. Use the cdm to execute the following steps:
        // 8.8.1. If there is no data stored for the sanitized session ID in the origin, resolve promise with false and abort these steps.
        //
        if( !fHasData )
        {
            *loaded = FALSE;
            goto done;
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.8.2. If the stored session's session type is not the same as the current MediaKeySession session type, reject promise with a newly created TypeError.
        //
        // Since we only support writing sessions of type MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE to disk,
        // and we already verified that m_eSessionType == MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE at step 5,
        // there is no need to take any action here - the session types are already guaranteed to be equal.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.8.3. Let session data be the data stored for the sanitized session ID in the origin.
        //        This MUST NOT include data from other origin(s) or that is not associated with an origin.
        // 8.8.4. If there is a MediaKeySession object that is not closed in any Document and
        // that represents the session data, reject promise with a QuotaExceededError.
        //
        // This would only be true if another session had loaded the same data
        // (or the original session was still open).
        // In either of those cases, _OpenPersistentSessionFile would have failed with
        // ERROR_SHARING_VIOLATION and we would have caught it at step 8.3 above.
        // Thus, take no action here.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.8.5. Load the session data.
        //
        IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_SESSION, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );
        if( !GetFileSizeEx( m_hPersistentSessionFile, &large ) )
        {
            IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
            IF_FAILED_GOTO( MF_E_UNEXPECTED );
        }
        IF_FALSE_GOTO( large.QuadPart > 0 && large.QuadPart % KEY_DATA_TOTAL_SIZE == 0, MF_TYPE_ERR );
        cKeys = large.QuadPart / KEY_DATA_TOTAL_SIZE;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.8.7. If a message needs to be sent, execute the following steps:
        // 8.8.7.1. Let message be a message generated based on the session data.
        //
        // Without impacting the algorithm's behavior, we perform step 8.8.7.1 partway through step 8.8.5,
        // and we then discard the message after step 8.8.5 if a message does not need to be sent.
        // We do this so we don't have to walk through the list of keys twice.
        //

        //
        // The format of the release message is so simple that we'll simply build it as a string rather than use a JSON builder.
        //
        cbMessage = _ComputeLicenseReleaseMessageSize( (DWORD)cKeys );
        pbMessage = new BYTE[ cbMessage ];
        IF_NULL_GOTO( pbMessage, E_OUTOFMEMORY );

        //
        // Start the json and kid array
        //
        memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseReleaseStart, ARRAYSIZE( g_szJsonLicenseReleaseStart ) - 1 );
        ibMessage += ARRAYSIZE( g_szJsonLicenseReleaseStart ) - 1;

        for( DWORD iKey = 0; iKey < cKeys; iKey++ )
        {
            GUID guidKid = GUID_NULL;
            BYTE rgbKeyData[ KEY_DATA_TOTAL_SIZE ] = { 0 };
            MF_MEDIAKEY_STATUS eCurrentStatus = MF_MEDIAKEY_STATUS_INTERNAL_ERROR;
            double dblExpiration;

            DWORD cbRead = 0;
            if( !ReadFile( m_hPersistentSessionFile, rgbKeyData, KEY_DATA_TOTAL_SIZE, &cbRead, nullptr ) )
            {
                IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
                IF_FAILED_GOTO( MF_E_UNEXPECTED );
            }
            IF_FALSE_GOTO( cbRead == KEY_DATA_TOTAL_SIZE, MF_TYPE_ERR );

            GetKeyData( rgbKeyData, nullptr, &guidKid, nullptr, &eCurrentStatus, nullptr, nullptr, &dblExpiration, nullptr );

            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
            //
            // 8.8.6. If the session data indicates an expiration time for the session, let expiration time be that expiration time.
            // 8.9.5. Run the Update Expiration algorithm on the session, providing expiration time.
            //
            // We do these early so that we make sure the lowest expiration for all keys is used for the session's expiration
            // (under the assumption that each key could potentially have a different expiration).
            //
            if( m_dblExpiration == NaN.f || dblExpiration < m_dblExpiration )
            {
                m_dblExpiration = dblExpiration;
            }

            if( IS_STATUS_USABLE_OR_PENDING( eCurrentStatus ) )
            {
                if( IsExpired( dblExpiration ) )
                {
                    //
                    // Move from usable or pending status to expired status.
                    //
                    eCurrentStatus = MF_MEDIAKEY_STATUS_EXPIRED;
                }
                else
                {
                    //
                    // Status should never be usable immediately on load.
                    // Use pending instead.
                    //
                    eCurrentStatus = MF_MEDIAKEY_STATUS_STATUS_PENDING;
                }
            }

            //
            // We will KeyStatusChanged on all codepaths that call this function,
            // so the last-notified-status should match the current status
            //
            SetKeyDataUpdateNotifiedStatus( rgbKeyData, nullptr, eCurrentStatus, eCurrentStatus );

            fReleased = fReleased || ( eCurrentStatus == MF_MEDIAKEY_STATUS_RELEASED );

            IF_FAILED_GOTO( _AddKidToMessage( pbMessage, &ibMessage, guidKid, iKey + 1 < cKeys ) );

            //
            // Save the key data in memory.
            //
            IF_FAILED_GOTO( _AddKeyDataToCDMAttributes( spIMFAttributesCDM.Get(), rgbKeyData, m_spIMFContentDecryptionModuleSessionCallbacks.Get() ) );

            //
            // Move on to the next key (if any).
            //
        }

        if( fReleased )
        {
            //
            // Finish the license-release message.
            //
            memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseReleaseEnd, ARRAYSIZE( g_szJsonLicenseReleaseEnd ) - 1 );
            ibMessage += ARRAYSIZE( g_szJsonLicenseReleaseEnd ) - 1;

            pbMessage[ ibMessage ] = '\0';
            ibMessage++;
            IF_FALSE_GOTO( ibMessage == cbMessage, MF_E_UNEXPECTED );
        }
        else
        {
            //
            // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
            //
            // 8.8.7. If a message needs to be sent, execute the following steps:
            // 8.8.7.1. Let message be a message generated based on the session data.
            //
            // Without impacting the algorithm's behavior, we performed step 8.8.7.1 partway through step 8.8.5 (above),
            // and we now discard the message at the end of step 8.8.5 if a message does not need to be sent (i.e. fReleased == false)
            //
            delete[] pbMessage;
            pbMessage = nullptr;
            cbMessage = 0;
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.8.6. If the session data indicates an expiration time for the session, let expiration time be that expiration time.
        //
        // Handled above during step 8.8.7.1.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.8.7. If a message needs to be sent, execute the following steps:
        // 8.8.7.1. Let message be a message generated based on the session data.
        //
        // Handled above during step 8.8.5.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.8.7.2. Let message type be the appropriate MediaKeyMessageType for the message.
        //
        // The message type is hard-coded below to "license-release" when calling KeyMessage below
        // because "license-release" is the only supported message type in this function.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.9. Run the following steps:
        // 8.9.1. If any of the preceding steps failed, reject promise with a the appropriate error name.
        //
        // A failure before this point will return an error code to the caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.9.2. Set the sessionId attribute to sanitized session ID.
        //
        m_sessionId = dwSessionId;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.9.3. Set this object's callable value to true.
        //
        IF_FAILED_GOTO( this->SetIsCallable() );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.9.4. If the loaded session contains information about any keys (there are known keys),
        //        run the Update Key Statuses algorithm on the session, providing each key's key ID along with the appropriate MediaKeyStatus.
        //        Should additional processing be necessary to determine with certainty the status of a key, use "status-pending".
        //        Once the additional processing for one or more keys has completed, run the Update Key Statuses algorithm again with the actual status(es).
        //
        // The key status was automatically set when we loaded the session data at step 8.8.5.
        // Thus, there is nothing more to do except for firing the key status change event.
        //
        m_spIMFContentDecryptionModuleSessionCallbacks->KeyStatusChanged();

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.9.5. Run the Update Expiration algorithm on the session, providing expiration time.
        //
        // Handled above during step 8.8.7.1.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.9.6. If message is not null, run the Queue a "message" Event algorithm on the session, providing message type and message.
        //
        if( pbMessage != nullptr )
        {
            m_spIMFContentDecryptionModuleSessionCallbacks->KeyMessage( MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RELEASE, pbMessage, cbMessage, nullptr );
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
        //
        // 8.9.7. Resolve promise with true.
        // 9. Return promise.
        //
        // Return success.
        //
        *loaded = TRUE;

    done:
        delete[] pbMessage;
        return hr;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleSession::GetSessionId(
    _Out_ LPWSTR *sessionId
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-sessionid
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;

        //
        // The Session ID for this object and the associated key(s) or license(s)
        //   Just return it.
        //
        if( m_sessionId == CLEARKEY_INVALID_SESSION_ID )
        {
            *sessionId = (LPWSTR)SysAllocString(L"");
            IF_NULL_GOTO(sessionId, E_OUTOFMEMORY);
        }
        else
        {
            *sessionId = (LPWSTR)SysAllocString(SESSION_ID_MIN_STR);
            IF_NULL_GOTO(sessionId, E_OUTOFMEMORY);
            swprintf( *sessionId, L"%d", m_sessionId );
        }

    done:
        return hr;
    } );

    return hrRet;
}

BOOL Cdm_clearkey_IMFContentDecryptionModuleSession::IsCallable()
{
    return m_sessionId != CLEARKEY_INVALID_SESSION_ID;
}

HRESULT Cdm_clearkey_IMFContentDecryptionModuleSession::SetIsCallable()
{
    if( m_sessionId == CLEARKEY_INVALID_SESSION_ID )
    {
        return MF_E_UNEXPECTED;
    }
    return S_OK;
}

static HRESULT _GenerateRequestForPSSH( _In_ MF_MEDIAKEYSESSION_TYPE eSessionType, _In_reads_bytes_( cbPSSH ) const BYTE *pbPSSH, _In_ DWORD cbPSSH, _Outptr_result_buffer_( *pcbMessage ) BYTE **ppbMessage, _Out_ DWORD *pcbMessage )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // This function implements steps 10.1 through 10.9.2 when initDataType == "cenc".
    // It also creates 'message' for step 10.9.4.
    //

    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    BYTE    *pbKids             = nullptr;
    DWORD    cbKids             = 0;
    DWORD    cKids              = 0;
    BYTE    *pbMessage          = nullptr;
    DWORD    cbMessage          = 0;
    DWORD    ibMessage          = 0;
    BOOL     fTemporary         = FALSE;

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // 10.1. If the init data is not valid for initDataType, reject promise with a newly created TypeError.
    // 10.2. Let sanitized init data be a validated and sanitized version of init data.
    // 10.3. If the preceding step failed, reject promise with a newly created TypeError.
    // 10.4. If sanitized init data is empty, reject promise with a NotSupportedError.
    //
    // The following function will fail if any of the preceding conditions hold.
    //
    IF_FAILED_GOTO( GetKidsFromPSSH( cbPSSH, pbPSSH, &cbKids, &pbKids ) );
    cKids = cbKids / sizeof( GUID );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // 10.5. Let session id be the empty string.
    // 10.6. Let message be null.
    // 10.7. Let message type be null.
    //
    // Implied by variable initialization.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // 10.8. Let cdm be the CDM instance represented by this object's cdm instance value.
    //
    // The 'this' pointer (along with its member variables) is sufficient.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // 10.9. Use the cdm to execute the following steps:
    // 10.9.1. If the sanitized init data is not supported by the cdm, reject promise with a NotSupportedError.
    //
    // Once we've verified the init data in steps 10.1 through 10.4 above, it is always supported.
    // No action is required here.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // 10.9.2. Follow the steps for the value of session type from the following list:
    //         "temporary"
    //             Let requested license type be a temporary non-persistable license.
    //         "persistent-license"
    //             Let requested license type be a persistable license.
    //
    // 'fTemporary' indicates which requested license type will be used.
    // Since a PSSH doesn't explicitly request a license type, we simply
    // request licenses that are valid for this session type.
    //
    fTemporary = ( eSessionType == MF_MEDIAKEYSESSION_TYPE_TEMPORARY );

    //
    // Now we're going to create the license request format defined by:
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#clear-key-request-format
    //
    // This will be the 'message' for step 10.9.4.
    //

    //
    // The format of the request is so simple that we'll simply build it as a string rather than use a JSON builder.
    //

    //
    // Determine size of license request based on key count
    //
    cbMessage = _ComputeLicenseRequestMessageSize( cKids, fTemporary );

    pbMessage = new BYTE[ cbMessage ];
    IF_NULL_GOTO( pbMessage, E_OUTOFMEMORY );

    //
    // Start the json and kid array
    //
    memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseRequestStart, ARRAYSIZE( g_szJsonLicenseRequestStart ) - 1 );
    ibMessage += ARRAYSIZE( g_szJsonLicenseRequestStart ) - 1;

    //
    // Write the kid array
    //
    for( DWORD iKid = 0; iKid < cKids; iKid++ )
    {
        GUID guidKid;
        BIG_ENDIAN_SIXTEEN_BYTES_TO_GUID( &pbKids[ iKid * sizeof( GUID ) ], &guidKid );
        IF_FAILED_GOTO( _AddKidToMessage( pbMessage, &ibMessage, guidKid, iKid + 1 < cKids ) );
    }

    //
    // End the kid array
    //
    memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseRequestMiddle, ARRAYSIZE( g_szJsonLicenseRequestMiddle ) - 1 );
    ibMessage += ARRAYSIZE( g_szJsonLicenseRequestMiddle ) - 1;

    //
    // Add the session type
    //
    if( fTemporary )
    {
        memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseRequestTypeEndTemporary, ARRAYSIZE( g_szJsonLicenseRequestTypeEndTemporary ) - 1 );
        ibMessage += ARRAYSIZE( g_szJsonLicenseRequestTypeEndTemporary ) - 1;
    }
    else
    {
        memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseRequestTypeEndPersistent, ARRAYSIZE( g_szJsonLicenseRequestTypeEndPersistent ) - 1 );
        ibMessage += ARRAYSIZE( g_szJsonLicenseRequestTypeEndPersistent ) - 1;
    }

    //
    // Null terminate and verify length
    //
    pbMessage[ ibMessage ] = '\0';
    ibMessage++;
    IF_FALSE_GOTO( ibMessage == cbMessage, MF_E_UNEXPECTED );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // Return 'message' for use at step 10.9.4.
    //
    *ppbMessage = pbMessage;
    pbMessage = nullptr;
    *pcbMessage = cbMessage - 1;  // Exclude null terminator

done:
    delete[] pbKids;
    delete[] pbMessage;
    return hr;
}

static HRESULT _GenerateRequestForKidS( _In_ MF_MEDIAKEYSESSION_TYPE eSessionType, _In_reads_bytes_( cbKidS ) const BYTE *pbKidS, _In_ DWORD cbKidS, _Outptr_result_buffer_( *pcbMessage ) BYTE **ppbMessage, _Out_ DWORD *pcbMessage )
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // This function implements steps 10.1 through 10.9.2 when initDataType == "kids".
    // It also creates 'message' for step 10.9.4.
    //
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);

    WCHAR   *pwszJSON   = nullptr;
    UINT     cKids      = 0;
    BYTE    *pbMessage  = nullptr;
    DWORD    cbMessage  = 0;
    DWORD    ibMessage  = 0;
    BOOL     fTemporary = TRUE;
    ComPtr<IJsonObject>              spIJsonObject;
    ComPtr<IJsonArray>               spIJsonArray;
    ComPtr<IVector<IJsonValue *>>    spIVectorOfIJsonValue;

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // 10.1. If the init data is not valid for initDataType, reject promise with a newly created TypeError.
    // 10.2. Let sanitized init data be a validated and sanitized version of init data.
    // 10.3. If the preceding step failed, reject promise with a newly created TypeError.
    // 10.4. If sanitized init data is empty, reject promise with a NotSupportedError.
    //
    // Converting the response string to a JSON object does thorough validation.
    // Thus, 'spIJsonObject' is already a validated version of response copy,
    // and convesion would have failed (and thus rejected the promise) if validation failed.
    //
    // Note: Additional validation occurs during parsing below.
    //       The promise will also be rejected with a newly created TypeError if that validation fails.
    //
    IF_FAILED_GOTO( _JsonStringToObject( pbKidS, cbKidS, spIJsonObject.GetAddressOf() ) );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // 10.5. Let session id be the empty string.
    // 10.6. Let message be null.
    // 10.7. Let message type be null.
    //
    // Implied by variable initialization.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // 10.8. Let cdm be the CDM instance represented by this object's cdm instance value.
    //
    // The 'this' pointer (along with its member variables) is sufficient.
    //

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // 10.9. Use the cdm to execute the following steps:
    // 10.9.1. If the sanitized init data is not supported by the cdm, reject promise with a NotSupportedError.
    //
    // Once we've verified the init data in steps 10.1 through 10.4 above, it is always supported.
    // No action is required here.
    //

    IF_FAILED_GOTO( spIJsonObject->GetNamedArray( HStringReference( g_wszkids ).Get(), &spIJsonArray ) );

    IF_FAILED_GOTO( spIJsonArray->QueryInterface( IID_PPV_ARGS( &spIVectorOfIJsonValue ) ) );
    IF_FAILED_GOTO( spIVectorOfIJsonValue->get_Size( &cKids ) );
    IF_FALSE_GOTO( cKids > 0, MF_TYPE_ERR );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // 10.9.2. Follow the steps for the value of session type from the following list:
    //         "temporary"
    //             Let requested license type be a temporary non-persistable license.
    //         "persistent-license"
    //             Let requested license type be a persistable license.
    //
    // 'fTemporary' indicates which requested license type will be used.
    //
    IF_FAILED_GOTO( _ParseTypeFromJSON( spIJsonObject.Get(), &fTemporary ) );
    IF_FALSE_GOTO( ( eSessionType == MF_MEDIAKEYSESSION_TYPE_TEMPORARY ) == fTemporary, MF_TYPE_ERR );

    //
    // Now we're going to create the license request format defined by:
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#clear-key-request-format
    //
    // This will be the 'message' for step 10.9.4.
    //

    //
    // The format of the request is so simple that we'll simply build it as a string rather than use a JSON builder.
    //

    //
    // Determine size of license request based on key count
    //
    cbMessage = _ComputeLicenseRequestMessageSize( cKids, fTemporary );

    pbMessage = new BYTE[ cbMessage ];
    IF_NULL_GOTO( pbMessage, E_OUTOFMEMORY );

    //
    // Start the json and kid array
    //
    memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseRequestStart, ARRAYSIZE( g_szJsonLicenseRequestStart ) - 1 );
    ibMessage += ARRAYSIZE( g_szJsonLicenseRequestStart ) - 1;

    //
    // Write the kid array
    //
    for( DWORD iKid = 0; iKid < cKids; iKid++ )
    {
        LPCWSTR pwszWeakRefKid = nullptr;
        UINT32  cchKid = 0;
        HString hstr1;

        IF_FAILED_GOTO( spIJsonArray->GetStringAt( iKid, hstr1.GetAddressOf() ) );
        pwszWeakRefKid = WindowsGetStringRawBuffer( hstr1.Get(), &cchKid );           // output length excludes the null terminator
        IF_FALSE_GOTO( cchKid == GUID_B64_URL_LEN, MF_TYPE_ERR );

        pbMessage[ ibMessage ] = '"';
        ibMessage++;

        IF_FALSE_GOTO( GUID_B64_URL_LEN == WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            pwszWeakRefKid,
            GUID_B64_URL_LEN,
            (LPSTR)&pbMessage[ ibMessage ],
            GUID_B64_URL_LEN,
            nullptr,
            nullptr ), MF_TYPE_ERR );
        ibMessage += GUID_B64_URL_LEN;

        pbMessage[ ibMessage ] = '"';
        ibMessage++;

        if( iKid < cKids - 1 )
        {
            pbMessage[ ibMessage ] = ',';
            ibMessage++;
        }
    }

    //
    // End the kid array
    //
    memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseRequestMiddle, ARRAYSIZE( g_szJsonLicenseRequestMiddle ) - 1 );
    ibMessage += ARRAYSIZE( g_szJsonLicenseRequestMiddle ) - 1;

    //
    // Add the session type
    //
    if( fTemporary )
    {
        memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseRequestTypeEndTemporary, ARRAYSIZE( g_szJsonLicenseRequestTypeEndTemporary ) - 1 );
        ibMessage += ARRAYSIZE( g_szJsonLicenseRequestTypeEndTemporary ) - 1;
    }
    else
    {
        memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseRequestTypeEndPersistent, ARRAYSIZE( g_szJsonLicenseRequestTypeEndPersistent ) - 1 );
        ibMessage += ARRAYSIZE( g_szJsonLicenseRequestTypeEndPersistent ) - 1;
    }

    //
    // Null terminate and verify length
    //
    pbMessage[ ibMessage ] = '\0';
    ibMessage++;
    IF_FALSE_GOTO( ibMessage == cbMessage, MF_E_UNEXPECTED );

    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    // Return 'message' for use at step 10.9.4.
    //
    *ppbMessage = pbMessage;
    pbMessage = nullptr;
    *pcbMessage = cbMessage - 1;  // Exclude null terminator

done:
    delete[] pbMessage;
    return hr;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleSession::GenerateRequest(
    _In_ LPCWSTR initDataType,
    _In_reads_bytes_(initDataSize) const BYTE *initData,
    _In_ DWORD initDataSize
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        BYTE *pbMessage = nullptr;
        DWORD cbMessage = 0;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 1. If this object's closing or closed value is true, return a promise rejected with an InvalidStateError.
        //
        IF_FALSE_GOTO( !m_fClosed, MF_INVALID_STATE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 2. If this object's uninitialized value is false, return a promise rejected with an InvalidStateError.
        //
        IF_FALSE_GOTO( m_fUninitialized, MF_INVALID_STATE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 3. Let this object's uninitialized value be false.
        //
        m_fUninitialized = FALSE;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 4. If initDataType is the empty string, return a promise rejected with a newly created TypeError.
        //
        IF_FALSE_GOTO( wcslen( initDataType ) > 0, MF_TYPE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 5. If initData is an empty array, return a promise rejected with a newly created TypeError.
        //
        IF_FALSE_GOTO( initData != nullptr && initDataType != 0, MF_TYPE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 6. If the Key System implementation represented by this object's cdm implementation value
        //    does not support initDataType as an Initialization Data Type, return a promise rejected
        //    with a NotSupportedError. String comparison is case-sensitive.
        //
        // The 'else' clause below handles this case.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // Note: Step 7 is handled by caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 8. Let session type be this object's session type.
        //
        // session type == 'm_eSessionType'
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // Note: Step 9 is handled by caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 10. Run the following steps:
        //
        // Steps 10.1 through 10.9.2 are handled by the called function _GenerateRequestForPSSH or _GenerateRequestForKidS.
        // Refer to those functions for more information.
        //
        if( CSTR_EQUAL == CompareStringOrdinal( g_wszcenc, -1, initDataType, -1, FALSE ) )
        {
            //
            // Examples:
            //
            // We get a PSSH when the app directly forwards
            // the init data given to it via the "encrypted" event,
            // in which case the init data is from the content file.
            //
            // The application can directly pass in its own PSSH.
            //
            ComPtr<IMFAttributes> spIMFAttributesCDM;
            IF_FAILED_GOTO( _GenerateRequestForPSSH( m_eSessionType, initData, initDataSize, &pbMessage, &cbMessage ) );

            //
            // If we've successfully generated a request for this PSSH, then we assume it's the 'best' PSSH
            // because the application gave it to us (regardless of where it came from originally).
            // We will use it later to determine what kids we are dealing with, e.g. we pass it to our
            // IMFTrustedInput implementation so that RequestAccess can check whether we need a key.
            //
            IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_SESSION, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );
            IF_FAILED_GOTO( spIMFAttributesCDM->SetBlob( CLEARKEY_GUID_ATTRIBUTE_PSSH_FROM_CONTENT_FILE_OR_GEN_REQUEST, initData, initDataSize ) );
        }
        else if( CSTR_EQUAL == CompareStringOrdinal( g_wszkeyids, -1, initDataType, -1, FALSE ) )
        {
            IF_FAILED_GOTO( _GenerateRequestForKidS( m_eSessionType, initData, initDataSize, &pbMessage, &cbMessage ) );
        }
        else
        {
            IF_FAILED_GOTO( MF_NOT_SUPPORTED_ERR );
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 10.9.3. Let session id be a unique Session ID string.
        //
        // Handled at step 10.10.2 below.  Note that there is no executable code until step 10.10.2.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 10.9.4. If a license request for the requested license type can be generated based on the sanitized init data:
        // 10.9.4.1. Let message be a license request for the requested license type generated based on the sanitized init data interpreted per initDataType.
        //
        // message == 'pbMessage'
        // See _GenerateRequestForPSSH and _GenerateRequestForKidS for its creation.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 10.9.4.2. Let message type be "license-request".
        //
        // The message type is hard-coded below to "license-request" when calling KeyMessage below
        // because "license-request" is the only supported message type in this function.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 10.9.4. ...
        //         Otherwise:
        //         ...
        // We can always generate a license request.  Thus, no action is required here.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 10.10. Run the following steps:
        // 10.10.1. If any of the preceding steps failed, reject promise with a new DOMException whose name is the appropriate error name.
        //
        // A failure before this point will return an error code to the caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 10.10.2. Set the sessionId attribute to session id.
        //
        m_sessionId = _GenerateRandomSessionId();

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 10.10.3. Set this object's callable value to true.
        //
        IF_FAILED_GOTO( this->SetIsCallable() );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 10.10.4. Run the Queue a "message" Event algorithm on the session, providing message type and message.
        //
        m_spIMFContentDecryptionModuleSessionCallbacks->KeyMessage( MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST, pbMessage, cbMessage, nullptr );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
        //
        // 10.10.5. Resolve promise.
        // 11. Return promise.
        //
        // Return success.
        //

    done:
        delete[] pbMessage;
        return hr;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleSession::GetExpiration(
    _Out_ double *expiration
)
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-expiration
    //
    // The expiration time for all key(s) in the session, or NaN if no such time exists
    // or if the license explicitly never expires, as determined by the CDM.
    //
    // m_dblExpiration represents the expiration, so just return it.
    //
    HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        *expiration = m_dblExpiration;
        return S_OK;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFContentDecryptionModuleSession::Remove()
{
    //
    // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
    //
    HRESULT hrRet;
    TRACE_FUNC_HR( hrRet );
    hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        HRESULT hr = S_OK;
        BYTE  *pbMessage = nullptr;
        DWORD  cbMessage = 0;
        DWORD  ibMessage = 0;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
        //
        // 1. If this object's closing or closed value is true, return a promise rejected with an InvalidStateError.
        //
        IF_FALSE_GOTO( !m_fClosed, MF_INVALID_STATE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
        //
        // 2. If this object's callable value is false, return a promise rejected with an InvalidStateError.
        //
        IF_FALSE_GOTO( this->IsCallable(), MF_INVALID_STATE_ERR );

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
        //
        // Note: Step 3 is handled by caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
        //
        // 4. Run the following steps:
        // 4.1. Let cdm be the CDM instance represented by this object's cdm instance value.
        //
        // The 'this' pointer (along with its member variables) is sufficient.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
        //
        // 4.2. Let message be null.
        // 4.3. Let message type be null.
        //
        // Implied by variable initialization.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
        //
        // 4.4. Use the cdm to execute the following steps:
        // 4.4.1. If any license(s) and/or key(s) are associated with the session:
        // 4.4.1.1. Destroy the license(s) and/or key(s) associated with the session.
        //          Note
        //          This implies destruction of the license(s) and/or keys(s) whether they are in memory, persistent store or both.
        // 4.4.1.2. Follow the steps for the value of this object's session type from the following list:
        //          "temporary"
        //          Continue with the following steps.
        //          "persistent-license"
        //          Let record of license destruction be a record of license destruction for the license represented by this object.
        //          Store the record of license destruction.
        //          Let message be a message containing or reflecting the record of license destruction.
        //
        // 4.5. Run the following steps:
        // 4.5.1. Run the Update Key Statuses algorithm on the session, providing all key ID(s) in the session along with the "released" MediaKeyStatus value for each.
        //
        // Without impacting the algorithm's behavior, we perform step 4.4 through 4.5.1 as a unit.
        // We do this so we don't have to walk through the list of keys twice.
        //
        if( m_eSessionType == MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE )
        {
            //
            // The format of the release message is so simple that we'll simply build it as a string rather than use a JSON builder.
            //
            DWORD cKeys;
            ComPtr<IMFAttributes> spIMFAttributesCDM;

            IF_FAILED_GOTO( this->GetUnknown( CLEARKEY_GUID_ATTRIBUTES_FROM_CDM_TO_SESSION, IID_PPV_ARGS( &spIMFAttributesCDM ) ) );
            IF_FAILED_GOTO( _CountSessionKeys( spIMFAttributesCDM.Get(), m_sessionId, &cKeys ) );
            cbMessage = _ComputeLicenseReleaseMessageSize( cKeys );

            pbMessage = new BYTE[ cbMessage ];
            IF_NULL_GOTO( pbMessage, E_OUTOFMEMORY );

            //
            // Start the json and kid array
            //
            memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseReleaseStart, ARRAYSIZE( g_szJsonLicenseReleaseStart ) - 1 );
            ibMessage += ARRAYSIZE( g_szJsonLicenseReleaseStart ) - 1;

            //
            // DestroyKeys will both destroy the keys (both in memory and on disk if appropriate)
            // and update the message to include the kids for the keys destroyed
            //
            hr = _OpenPersistentSessionFile( OPEN_EXISTING, m_pwszInPrivateStorePath, m_sessionId, &m_hPersistentSessionFile );
            if( hr == HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND ) )
            {
                hr = S_OK;
            }
            IF_FAILED_GOTO( hr );
            IF_FAILED_GOTO( this->DestroyKeys( MF_MEDIAKEY_STATUS_RELEASED, pbMessage, &ibMessage ) );

            //
            // Finish the license-release message.
            //
            memcpy( &pbMessage[ ibMessage ], g_szJsonLicenseReleaseEnd, ARRAYSIZE( g_szJsonLicenseReleaseEnd ) - 1 );
            ibMessage += ARRAYSIZE( g_szJsonLicenseReleaseEnd ) - 1;

            pbMessage[ ibMessage ] = '\0';
            ibMessage++;
            IF_FALSE_GOTO( ibMessage == cbMessage, MF_E_UNEXPECTED );
        }
        else
        {
            IF_FAILED_GOTO( this->DestroyKeys( MF_MEDIAKEY_STATUS_RELEASED, nullptr, nullptr ) );
        }
        m_spIMFContentDecryptionModuleSessionCallbacks->KeyStatusChanged();

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
        //
        // 4.5.2. Run the Update Expiration algorithm on the session, providing NaN.
        //
        // Simply set m_dblExpiration to NaN.
        //
        m_dblExpiration = NaN.f;

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
        //
        // 4.5.3. If any of the preceding steps failed, reject promise with a new DOMException whose name is the appropriate error name.
        //
        // A failure before this point will return an error code to the caller.
        //

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
        //
        // 4.5.4. Let message type be "license-release".
        // 4.5.5. If message is not null, run the Queue a "message" Event algorithm on the session, providing message type and message.
        //
        if( pbMessage != nullptr )
        {
            m_spIMFContentDecryptionModuleSessionCallbacks->KeyMessage( MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RELEASE, pbMessage, cbMessage, nullptr );
        }

        //
        // https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
        //
        // 4.5.6. Resolve promise.
        // 5. Return promise.
        //
        // Return success.
        //

    done:
        delete[] pbMessage;
        return hr;
    } );

    return hrRet;
}

Cdm_clearkey_IMFCdmSuspendNotify::Cdm_clearkey_IMFCdmSuspendNotify()
{
}

Cdm_clearkey_IMFCdmSuspendNotify::~Cdm_clearkey_IMFCdmSuspendNotify()
{
}

HRESULT Cdm_clearkey_IMFCdmSuspendNotify::RuntimeClassInitialize( _In_ IMFContentDecryptionModule *pIMFContentDecryptionModule )
{
    return S_OK;
}

STDMETHODIMP Cdm_clearkey_IMFCdmSuspendNotify::Begin()
{
    //
    // If the CDM needs to take action when the application gets suspended,
    // e.g. when the machine is put to sleep, that action should be taken here.
    //
    // If this function fails, suspend will be delayed and this function will
    // be called again after a short timeout.
    //
    // This function should execute quickly or the process may be terminated.
    //
    // Example usage: if the application has state on disk which needs to
    // have consistency maintained across multiple file read/write calls,
    // it may be useful to attempt to lock the entire file inside this
    // function so that if a series of reads/writes are acting on the file,
    // this function will fail and can be reattempted, thus giving those
    // reads/writes a chance to complete to keep the file in a consistent
    // state.  The 'End' function would then release the file lock.
    //
    HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        return S_OK;
    } );

    return hrRet;
}

STDMETHODIMP Cdm_clearkey_IMFCdmSuspendNotify::End()
{
    //
    // This function will be called when the application resumes after
    // it was suspended.  Refer to comments in "Begin" for more information.
    //
    HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT
    {
        CriticalSection::SyncLock lock = m_Lock.Lock();
        return S_OK;
    } );

    return hrRet;
}

