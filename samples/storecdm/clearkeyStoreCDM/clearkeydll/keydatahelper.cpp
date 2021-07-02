//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************


#include "stdafx.h"

void AddValueToEightByteBigEndianInteger( _Inout_count_( sizeof( QWORD ) ) BYTE rgbExistingValue[ sizeof( QWORD ) ], _In_ QWORD qwValueToAdd )
{
    QWORD qwUpdatedValue = 0;

    //
    // Convert big endian bytes to little endian QWORD
    //
    for( DWORD idx = 0; idx < sizeof( QWORD ); idx++ )
    {
        qwUpdatedValue |= (QWORD)( ( (QWORD)rgbExistingValue[ idx ] ) << (QWORD)( 56 - 8 * idx ) );
    }

    //
    // Add offset - Note that integer overflow is allowed/expected in this case
    //
    qwUpdatedValue += qwValueToAdd;

    //
    // Convert little endian QWORD to big endian bytes
    //
    for( DWORD idx = 0; idx < sizeof( QWORD ); idx++ )
    {
        rgbExistingValue[ idx ] = (BYTE)( qwUpdatedValue >> ( 56 - 8 * idx ) );
    }
}

static HRESULT _SetKeyDataGeneric(
    _Inout_count_( KEY_DATA_TOTAL_SIZE )       BYTE pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _In_opt_                             const BYTE pbSessionId[ KEY_DATA_SESSION_ID_SIZE ],
    _In_opt_                             const BYTE pbKid[ KEY_DATA_KID_SIZE ],
    _In_opt_                             const BYTE pbAES128Key[ KEY_DATA_KEY_SIZE ],
    _In_opt_                             const BYTE pbNewStatus[ KEY_DATA_STATUS_SIZE ],
    _In_opt_                             const BYTE pbLastNotifiedStatus[ KEY_DATA_LASTNOTIFIEDSTATUS_SIZE ],
    _In_opt_                             const BYTE pbExpiration[ KEY_DATA_EXPIRATION_SIZE ],
    _In_opt_                             const BYTE pbPolicy[ KEY_DATA_OUTPUT_POLICY_SIZE ],
    _In_                                       BOOL fNewLid )
{
    HRESULT hr = S_OK;
    if( pbSessionId != nullptr )
    {
        memcpy( &pbKeyData[ KEY_DATA_SESSION_ID_OFFSET ], pbSessionId, KEY_DATA_SESSION_ID_SIZE );
    }

    if( pbKid != nullptr )
    {
        memcpy( &pbKeyData[ KEY_DATA_KID_OFFSET ], pbKid, KEY_DATA_KID_SIZE );
    }

    if( pbAES128Key != nullptr )
    {
        memcpy( &pbKeyData[ KEY_DATA_KEY_OFFSET ], pbAES128Key, KEY_DATA_KEY_SIZE );
    }

    if( pbNewStatus != nullptr )
    {
        memcpy( &pbKeyData[ KEY_DATA_STATUS_OFFSET ], pbNewStatus, KEY_DATA_STATUS_SIZE );
    }

    if( pbLastNotifiedStatus != nullptr )
    {
        memcpy( &pbKeyData[ KEY_DATA_LASTNOTIFIEDSTATUS_OFFSET ], pbLastNotifiedStatus, KEY_DATA_LASTNOTIFIEDSTATUS_SIZE );
    }

    if( pbExpiration != nullptr )
    {
        memcpy( &pbKeyData[ KEY_DATA_EXPIRATION_OFFSET ], pbExpiration, KEY_DATA_EXPIRATION_SIZE );
    }

    if( pbPolicy != nullptr )
    {
        memcpy( &pbKeyData[ KEY_DATA_OUTPUT_POLICY_OFFSET ], pbPolicy, KEY_DATA_OUTPUT_POLICY_SIZE );
    }

    if( fNewLid )
    {
        //
        // We're just using CoCreateGuid to generate a non-cryptographically-random GUID, so endianness is irrelevant during generation
        //
        IF_FAILED_GOTO( CoCreateGuid( (GUID*)&pbKeyData[ KEY_DATA_LID_OFFSET ] ) );
    }

done:
    return hr;
}

HRESULT SetKeyDataInitial(
    _Inout_count_( KEY_DATA_TOTAL_SIZE )       BYTE                                          pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _In_                                       int                                           sessionId,
    _In_                                       REFGUID                                       guidKid,
    _In_count_( KEY_DATA_KEY_SIZE )      const BYTE                                          pbAES128Key[ KEY_DATA_KEY_SIZE ],
    _In_                                       double                                        dblExpiration,
    _In_                                 const CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS *pPolicy )
{
    BYTE rgbKid[ sizeof( GUID ) ];
    BOOL fNewLid = TRUE;
    MF_MEDIAKEY_STATUS eStatus = MF_MEDIAKEY_STATUS_STATUS_PENDING;
    GUID_TO_BIG_ENDIAN_SIXTEEN_BYTES( &guidKid, rgbKid );
    return _SetKeyDataGeneric( pbKeyData, (const BYTE*)&sessionId, rgbKid, pbAES128Key, (const BYTE*)&eStatus, (const BYTE*)&eStatus, (const BYTE*)&dblExpiration, (const BYTE*)pPolicy, fNewLid );
}

void SetKeyDataUpdateStatus(
    _Inout_count_( KEY_DATA_TOTAL_SIZE ) BYTE               pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _In_                                 MF_MEDIAKEY_STATUS eNewStatus )
{
    _SetKeyDataGeneric( pbKeyData, nullptr, nullptr, nullptr, (BYTE*)&eNewStatus, nullptr, nullptr, nullptr, FALSE );
}

void SetKeyDataUpdateNotifiedStatus(
    _Inout_count_( KEY_DATA_TOTAL_SIZE )       BYTE               pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _In_opt_                             const BYTE               pbAES128Key[ KEY_DATA_KEY_SIZE ],
    _In_                                       MF_MEDIAKEY_STATUS eNewStatus,
    _In_                                       MF_MEDIAKEY_STATUS eLastNotifiedStatus )
{
    _SetKeyDataGeneric( pbKeyData, nullptr, nullptr, pbAES128Key, (BYTE*)&eNewStatus, (BYTE*)&eLastNotifiedStatus, nullptr, nullptr, FALSE );
}

static void _GetKeyDataGeneric(
    _In_count_( KEY_DATA_TOTAL_SIZE ) const BYTE pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _Out_opt_                               BYTE pbSessionId[ KEY_DATA_SESSION_ID_SIZE ],
    _Out_opt_                               BYTE pbKid[ KEY_DATA_KID_SIZE ],
    _Out_opt_                               BYTE pbAES128Key[ KEY_DATA_KEY_SIZE ],
    _Out_opt_                               BYTE pbCurrentStatus[ KEY_DATA_STATUS_SIZE ],
    _Out_opt_                               BYTE pbLid[ KEY_DATA_LID_SIZE ],
    _Out_opt_                               BYTE pbLastNotifiedStatus[ KEY_DATA_LASTNOTIFIEDSTATUS_SIZE ],
    _Out_opt_                               BYTE pbExpiration[ KEY_DATA_EXPIRATION_SIZE ],
    _Out_opt_                               BYTE pbPolicy[ KEY_DATA_OUTPUT_POLICY_SIZE ] )
{
    if( pbSessionId != nullptr )
    {
        memcpy( pbSessionId, &pbKeyData[ KEY_DATA_SESSION_ID_OFFSET ], KEY_DATA_SESSION_ID_SIZE );
    }

    if( pbKid != nullptr )
    {
        memcpy( pbKid, &pbKeyData[ KEY_DATA_KID_OFFSET ], KEY_DATA_KID_SIZE );
    }

    if( pbAES128Key != nullptr )
    {
        memcpy( pbAES128Key, &pbKeyData[ KEY_DATA_KEY_OFFSET ], KEY_DATA_KEY_SIZE );
    }

    if( pbCurrentStatus != nullptr )
    {
        memcpy( pbCurrentStatus, &pbKeyData[ KEY_DATA_STATUS_OFFSET ], KEY_DATA_STATUS_SIZE );
    }

    if( pbLid != nullptr )
    {
        memcpy( pbLid, &pbKeyData[ KEY_DATA_LID_OFFSET ], KEY_DATA_LID_SIZE );
    }

    if( pbLastNotifiedStatus != nullptr )
    {
        memcpy( pbLastNotifiedStatus, &pbKeyData[ KEY_DATA_LASTNOTIFIEDSTATUS_OFFSET ], KEY_DATA_LASTNOTIFIEDSTATUS_SIZE );
    }

    if( pbExpiration != nullptr )
    {
        memcpy( pbExpiration, &pbKeyData[ KEY_DATA_EXPIRATION_OFFSET ], KEY_DATA_EXPIRATION_SIZE );
    }

    if( pbPolicy != nullptr )
    {
        memcpy( pbPolicy, &pbKeyData[ KEY_DATA_OUTPUT_POLICY_OFFSET ], KEY_DATA_OUTPUT_POLICY_SIZE );
    }
}

void GetKeyData(
    _In_count_( KEY_DATA_TOTAL_SIZE ) const BYTE                                             pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _Out_opt_                               int                                             *pSessionId,
    _Out_opt_                               GUID                                            *pguidKid,
    _Out_opt_                               BYTE                                             pbAES128Key[ KEY_DATA_KEY_SIZE ],
    _Out_opt_                               MF_MEDIAKEY_STATUS                              *peCurrentStatus,
    _Out_opt_                               GUID                                            *pguidLid,
    _Out_opt_                               MF_MEDIAKEY_STATUS                              *peLastNotifiedStatus,
    _Out_opt_                               double                                          *pdblExpiration,
    _Out_opt_                               CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS    *pPolicy )
{
    BYTE rgbKid[ sizeof( GUID ) ];
    BYTE rgbLid[ sizeof( GUID ) ];
    _GetKeyDataGeneric( pbKeyData, (BYTE*)pSessionId, rgbKid, pbAES128Key, (BYTE*)peCurrentStatus, rgbLid, (BYTE*)peLastNotifiedStatus, (BYTE*)pdblExpiration, (BYTE*)pPolicy );
    if( pguidKid != nullptr )
    {
        BIG_ENDIAN_SIXTEEN_BYTES_TO_GUID( rgbKid, pguidKid );
    }
    if( pguidLid != nullptr )
    {
        BIG_ENDIAN_SIXTEEN_BYTES_TO_GUID( rgbLid, pguidLid );
    }
}

BOOL IsExpired( _In_ double dblExpiration )
{
    if( dblExpiration == NaN.f )
    {
        return FALSE;
    }
    else
    {
        FILETIME ft;
        GetSystemTimeAsFileTime( &ft );
        ULARGE_INTEGER ularge;
        double dblCurrent;

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

        dblCurrent = (double)ularge.QuadPart;

        return ( dblCurrent > dblExpiration );
    }
}

HRESULT ConvertB64urlStringToBigEndianSixteenBytes( _In_count_( GUID_B64_URL_LEN ) const WCHAR pwszGuid[ GUID_B64_URL_LEN ], _Out_writes_bytes_( sizeof( GUID ) ) BYTE pbGuid[ sizeof( GUID ) ] )
{
    HRESULT hr = S_OK;
    TRACE_FUNC_HR(hr);
    WCHAR   rgwchGuid[ GUID_B64_LEN + 1 ] = { 0 };  // Add one for null terminator
    DWORD   cbGuid = sizeof( GUID );

    memcpy( rgwchGuid, pwszGuid, GUID_B64_LEN * sizeof( WCHAR ) );

    //
    // We have to convert from base64url to standard base64.
    // Append the two '=' characters (padding).
    //
    rgwchGuid[ GUID_B64_URL_LEN ] = L'=';
    rgwchGuid[ GUID_B64_URL_LEN + 1 ] = L'=';
    rgwchGuid[ GUID_B64_URL_LEN + 2 ] = L'\0';

    //
    // Convert the two special-case characters.
    //
    for( DWORD idx = 0; idx < GUID_B64_URL_LEN; idx++ )
    {
        switch( rgwchGuid[ idx ] )
        {
        case '-':
            rgwchGuid[ idx ] = '+';
            break;
        case '_':
            rgwchGuid[ idx ] = '/';
            break;
        default:
            // no-op
            break;
        }
    }

    //
    // Use a standard windows system function to base64 decode the value for us.
    //
    IF_FALSE_GOTO( CryptStringToBinaryW(
        rgwchGuid,
        0,
        CRYPT_STRING_BASE64,
        pbGuid,
        &cbGuid,
        nullptr,
        nullptr ), MF_TYPE_ERR );

done:
    return hr;
}

static HRESULT _GetKidsFromPRH(
    _In_count_( cchPRH )                const WCHAR  *pwszPRH,
    _In_                                const DWORD   cchPRH,
    _Out_                                     DWORD  *pcbKids,
    _Outptr_result_buffer_( *pcbKids )        BYTE  **ppbKids )
{
    HRESULT     hr          = S_OK;
    LPCWSTR     pwszWeakRef = nullptr;
    UINT        cchWeakRef  = 0;
    DWORD       cbKids      = 0;
    BYTE       *pbKids      = nullptr;
    HString     hstr1;
    ComPtr<IXmlDocument> spIXmlDocument;
    ComPtr<IXmlDocumentIO> spIXmlDocumentIO;
    ComPtr<IXmlNodeSelector> spIXmlNodeSelector;
    ComPtr<IXmlNode> spIXmlNode1;
    ComPtr<IXmlNode> spIXmlNode2;
    ComPtr<IXmlNamedNodeMap> spIXmlNamedNodeMap;
    ComPtr<IPropertyValue> spIPropertyValue;

    IF_FAILED_GOTO( Windows::Foundation::ActivateInstance( HStringReference( RuntimeClass_Windows_Data_Xml_Dom_XmlDocument ).Get(), &spIXmlDocument ) );

    IF_FAILED_GOTO( spIXmlDocument.As( &spIXmlDocumentIO ) );
    IF_FAILED_GOTO( spIXmlDocument.As( &spIXmlNodeSelector ) );

    //
    // PRH will not be null-terminated, so explicitly specify length to WindowsCreateString
    //
    IF_FAILED_GOTO( WindowsCreateString( pwszPRH, cchPRH, hstr1.GetAddressOf() ) );
    IF_FAILED_GOTO( spIXmlDocumentIO->LoadXml( hstr1.Get() ) );
    hstr1.Release();

    //
    // Locate top-level node
    //
    IF_FAILED_GOTO( spIXmlNodeSelector->SelectSingleNode( HStringReference( L"//*" ).Get(), &spIXmlNode1 ) );

    //
    // Get version attribute
    //
    IF_FAILED_GOTO( spIXmlNode1->get_Attributes( &spIXmlNamedNodeMap ) );

    IF_FAILED_GOTO( spIXmlNamedNodeMap->GetNamedItem( HStringReference( L"version" ).Get(), &spIXmlNode2 ) );

    IF_FAILED_GOTO( spIXmlNode2->get_NodeValue( &spIPropertyValue ) );
    IF_FAILED_GOTO( spIPropertyValue->GetString( hstr1.GetAddressOf() ) );

    pwszWeakRef = WindowsGetStringRawBuffer( hstr1.Get(), &cchWeakRef );

    if( CSTR_EQUAL == CompareStringOrdinal( pwszWeakRef, cchWeakRef, L"4.0.0.0", -1, FALSE ) )
    {
        ComPtr<IXmlNodeList> spIXmlNodeList;
        BOOL fFoundKid = FALSE;
        UINT uiLen = 0;

        cbKids = sizeof( GUID );
        pbKids = new BYTE[ cbKids ];
        IF_NULL_GOTO( pbKids, E_OUTOFMEMORY );

        //
        // Select all nodes
        //
        IF_FAILED_GOTO( spIXmlNodeSelector->SelectNodes( HStringReference( L"//*" ).Get(), &spIXmlNodeList ) );

        IF_FAILED_GOTO( spIXmlNodeList->get_Length( &uiLen ) );

        //
        // for each node
        //
        for( UINT ui = 0; !fFoundKid && ui < uiLen; ui++ )
        {
            HString hstr2;
            HString hstr3;

            spIXmlNode1 = nullptr;
            IF_FAILED_GOTO( spIXmlNodeList->Item( ui, &spIXmlNode1 ) );
            IF_FAILED_GOTO( spIXmlNode1->get_NodeName( hstr2.GetAddressOf() ) );

            pwszWeakRef = WindowsGetStringRawBuffer( hstr2.Get(), &cchWeakRef );

            if( CSTR_EQUAL == CompareStringOrdinal( pwszWeakRef, cchWeakRef, L"KID", -1, FALSE ) )  // String comparison is case-sensitive.
            {
                GUID guidKid;
                DWORD cbOut = cbKids;

                //
                // Found the (single) Kid node
                //
                ComPtr<IXmlNodeSerializer> spIXmlNodeSerializer;
                IF_FAILED_GOTO( spIXmlNode1.As( &spIXmlNodeSerializer ) );

                IF_FAILED_GOTO( spIXmlNodeSerializer->get_InnerText( hstr3.GetAddressOf() ) );
                pwszWeakRef = WindowsGetStringRawBuffer( hstr3.Get(), &cchWeakRef );

                //
                // PRH uses standard base64, but unlike most formats it uses LITTLE endian
                //
                IF_FALSE_GOTO( cchWeakRef == GUID_B64_LEN, MF_E_INVALIDTYPE );
                IF_FALSE_GOTO( CryptStringToBinaryW(
                    pwszWeakRef,
                    0,
                    CRYPT_STRING_BASE64,
                    (BYTE*)&guidKid,
                    &cbOut,
                    nullptr,
                    nullptr ), MF_E_INVALIDTYPE );
                IF_FALSE_GOTO( cbOut == sizeof( GUID ), MF_E_INVALIDTYPE );
                GUID_TO_BIG_ENDIAN_SIXTEEN_BYTES( &guidKid, pbKids );
                fFoundKid = TRUE;
            }
        }
        IF_FALSE_GOTO( fFoundKid, MF_E_INVALIDTYPE );  // Must have a KID
    }
    else
    {
        //
        // Note: This implementation only supports PRH version 4.0.0.0.
        //
        IF_FAILED_GOTO( MF_E_INVALIDTYPE );
    }

    *pcbKids = cbKids;
    *ppbKids = pbKids;
    pbKids = nullptr;

done:
    delete[] pbKids;
    return hr;
}

static HRESULT _GetKidsFromPRO(
    _In_count_( cbPRO )                 const BYTE      *pbPRO,
    _In_                                      DWORD      cbPRO,
    _Out_                                     DWORD     *pcbKids,
    _Outptr_result_buffer_( *pcbKids )        BYTE     **ppbKids )
{
    //
    // https://docs.microsoft.com/en-us/playready/specifications/playready-header-specification
    //
    // Note that PROs use LITTLE-endian for integers.
    //
    HRESULT  hr        = S_OK;
    DWORD    ibPRO     = 0;
    DWORD    cbPRORead = 0;
    WORD     cRecords  = 0;
    DWORD    cbKids    = 0;
    BYTE    *pbKids    = nullptr;

    //
    // PRO size
    //
    SAFE_ADD_DWORD_WITH_MAX( ibPRO, sizeof( cbPRORead ), cbPRO, &ibPRO );
    memcpy( &cbPRORead, pbPRO, sizeof( cbPRORead ) );
    pbPRO += sizeof( cbPRORead );

    IF_FALSE_GOTO( cbPRORead == cbPRO, MF_E_INVALIDTYPE );

    //
    // Record count
    //
    SAFE_ADD_DWORD_WITH_MAX( ibPRO, sizeof( cRecords ), cbPRO, &ibPRO );
    memcpy( &cRecords, pbPRO, sizeof( cRecords ) );
    pbPRO += sizeof( cRecords );

    IF_FALSE_GOTO( cRecords > 0, MF_E_INVALIDTYPE );

    for( DWORD iRecord = 0; iRecord < cRecords && cbKids == 0; iRecord++ )
    {
        WORD         wRecordType = 0;
        WORD         cbRecord    = 0;
        const BYTE  *pbRecord    = nullptr;

        //
        // Record Type
        //
        SAFE_ADD_DWORD_WITH_MAX( ibPRO, sizeof( wRecordType ), cbPRO, &ibPRO );
        memcpy( &wRecordType, pbPRO, sizeof( wRecordType ) );
        pbPRO += sizeof( wRecordType );

        //
        // Record Size
        //
        SAFE_ADD_DWORD_WITH_MAX( ibPRO, sizeof( cbRecord ), cbPRO, &ibPRO );
        memcpy( &cbRecord, pbPRO, sizeof( cbRecord ) );
        pbPRO += sizeof( cbRecord );

        SAFE_ADD_DWORD_WITH_MAX( ibPRO, cbRecord, cbPRO, &ibPRO );
        pbRecord = pbPRO;

        //
        // 0x0001  Indicates that the record contains a PlayReady Header (PRH).
        //
        if( wRecordType != 1 )
        {
            //
            // Not a PRH.  Skip.
            //
            pbPRO += cbRecord;
        }
        else
        {
            IF_FAILED_GOTO( _GetKidsFromPRH( (const WCHAR*)pbRecord, cbRecord / sizeof( WCHAR ), &cbKids, &pbKids ) );
        }
    }

    *pcbKids = cbKids;
    *ppbKids = pbKids;
    pbKids = nullptr;

done:
    delete[] pbKids;
    return hr;
}

HRESULT GetKidsFromPSSH(
    _In_  DWORD           cbPSSH,
    _In_  const BYTE     *pbPSSH,
    _Out_ DWORD          *pcbKids,
    _Out_ BYTE          **ppbKids )
{
    HRESULT      hr             = S_OK;
    const BYTE  *pbCur          = pbPSSH;
    DWORD        ibCur          = 0;
    DWORD        cbSize         = 0;
    DWORD        dwType         = 0;
    WORD         wVersion       = 0;
    WORD         wFlags         = 0;
    GUID         guidSystemID   = GUID_NULL;
    GUID         guidUUID       = GUID_NULL;
    DWORD        cbSystemSize   = 0;
    BOOL         fFound         = FALSE;
    BOOL         fUUIDBox       = FALSE;
    DWORD        cbMultiKid     = 0;
    DWORD        dwResult       = 0;
    DWORD        cbKids         = 0;
    BYTE        *pbKids         = nullptr;

    while( ibCur < cbPSSH && !fFound )
    {
        SAFE_ADD_DWORD_WITH_MAX( ibCur, sizeof( cbSize ), cbPSSH, &ibCur );
        BIG_ENDIAN_FOUR_BYTES_TO_DWORD( pbCur, &cbSize );
        pbCur += sizeof( cbSize );

        SAFE_ADD_DWORD_WITH_MAX( ibCur, sizeof( dwType ), cbPSSH, &ibCur );
        BIG_ENDIAN_FOUR_BYTES_TO_DWORD( pbCur, &dwType );
        pbCur += sizeof( dwType );

        // 0x64697575 in big endian stands for "uuid"
        if( dwType == 0x75756964 )
        {
            SAFE_ADD_DWORD_WITH_MAX( ibCur, sizeof( guidUUID ), cbPSSH, &ibCur );
            BIG_ENDIAN_SIXTEEN_BYTES_TO_GUID( pbCur, &guidUUID );
            pbCur += sizeof( guidUUID );

            IF_FALSE_GOTO( guidUUID == CLEARKEY_GUID_PSSH_BOX_ID, MF_E_INVALIDTYPE );
            fUUIDBox = TRUE;
        }
        else
        {
            // 0x68737370 in big endian stands for "pssh".
            IF_FALSE_GOTO( dwType == 0x70737368, MF_E_INVALIDTYPE );
        }

        SAFE_ADD_DWORD_WITH_MAX( ibCur, sizeof( wVersion ), cbPSSH, &ibCur );
        BIG_ENDIAN_TWO_BYTES_TO_WORD( pbCur, &wVersion );
        pbCur += sizeof( wVersion );

        SAFE_ADD_DWORD_WITH_MAX( ibCur, sizeof( wFlags ), cbPSSH, &ibCur );
        BIG_ENDIAN_TWO_BYTES_TO_WORD( pbCur, &wFlags );
        pbCur += sizeof( wFlags );

        SAFE_ADD_DWORD_WITH_MAX( ibCur, sizeof( guidSystemID ), cbPSSH, &ibCur );
        BIG_ENDIAN_SIXTEEN_BYTES_TO_GUID( pbCur, &guidSystemID );
        pbCur += sizeof( guidSystemID );

        // Handle multi-Kids pssh box
        if( wVersion > 0 )
        {
            DWORD cKids = 0;

            SAFE_ADD_DWORD_WITH_MAX( ibCur, sizeof( cKids ), cbPSSH, &ibCur );
            BIG_ENDIAN_FOUR_BYTES_TO_DWORD( pbCur, &cKids );
            pbCur += sizeof( cKids );

            //
            // Assume that more than 100,000 KidS specified means
            // someone is trying to cause a buffer overflow.
            //
            IF_FALSE_GOTO( cKids <= 100000, MF_E_INVALIDTYPE );

            cbKids = cKids * sizeof( GUID );
            pbKids = new BYTE[ cbKids ];
            IF_NULL_GOTO( pbKids, E_OUTOFMEMORY );

            memcpy( pbKids, pbCur, cbKids );
            pbCur += cbKids;

            cbMultiKid = sizeof( cKids ) + cbKids;
        }

        SAFE_ADD_DWORD_WITH_MAX( ibCur, sizeof( cbSystemSize ), cbPSSH, &ibCur );
        BIG_ENDIAN_FOUR_BYTES_TO_DWORD( pbCur, &cbSystemSize );
        pbCur += sizeof( cbSystemSize );

        if( cbKids > 0 )
        {
            if( guidSystemID == CLEARKEY_GUID_COMMON_SYSTEM_ID )
            {
                fFound = TRUE;
            }
            else if( guidSystemID == CLEARKEY_GUID_PLAYREADY_SYSTEM_ID )
            {
                fFound = TRUE;
            }
            else
            {
                cbKids = 0;
                delete[] pbKids;
                pbKids = nullptr;
            }
        }
        else if( guidSystemID == CLEARKEY_GUID_PLAYREADY_SYSTEM_ID )
        {
            //
            // In order to enable usage of PlayReady test content from test.playready.microsoft.com
            // which does not always include a v1 PSSH with a Kid list in the PSSH,
            // this implementation supports parsing a PlayReady Object (PRO) obtained from the PSSH.
            //
            IF_FAILED_GOTO( _GetKidsFromPRO( pbCur, cbSystemSize, &cbKids, &pbKids ) );
            fFound = TRUE;
        }
        //else if( guidSystemID == CLEARKEY_GUID_WIDEVINE_SYSTEM_ID && cbSystemSize >= sizeof( DWORD ) + sizeof( GUID ) )
        //{
        //    cbKids = sizeof( GUID );
        //    pbKids = new BYTE[ cbKids ];
        //    IF_NULL_GOTO( pbKids, E_OUTOFMEMORY );
        //    memcpy( pbKids, pbCur + sizeof( DWORD ), sizeof( GUID ) );
        //    fFound = TRUE;
        //}

        SAFE_ADD_DWORD_WITH_MAX( ibCur, cbSystemSize, cbPSSH, &ibCur );
        pbCur += cbSystemSize;

        // Should never happen - indicates the code above has a bug
        IF_FALSE_GOTO( pbPSSH + ibCur == pbCur, MF_E_UNEXPECTED );
    }

    IF_FALSE_GOTO( fFound, MF_E_INVALIDTYPE );

    //
    // Size verification to ensure it's a valid PSSH box
    // We already know these can't overflow because we used SAFE_ADD macros above.
    //
    if( fUUIDBox )
    {
        IF_FALSE_GOTO(
            cbSystemSize + sizeof( cbSize ) +
            sizeof( dwType ) + sizeof( guidUUID ) +
            sizeof( wVersion ) + sizeof( wFlags ) +
            sizeof( guidSystemID ) + sizeof( cbSystemSize ) == cbSize, MF_E_INVALIDTYPE );
    }
    else
    {
        IF_FALSE_GOTO(
            cbSystemSize + sizeof( cbSize ) +
            sizeof( dwType ) + sizeof( wVersion ) +
            sizeof( wFlags ) + sizeof( guidSystemID ) +
            sizeof( cbSystemSize ) + cbMultiKid == cbSize, MF_E_INVALIDTYPE );
    }

    *pcbKids = cbKids;
    *ppbKids = pbKids;
    pbKids = nullptr;

done:
    delete[] pbKids;
    return hr;
}

HRESULT DoAllKidsHaveKeysInAttributes(
    _In_                        IMFAttributes *pIMFAttributes,
    _In_                        DWORD          cbKids,
    _In_count_( cbKids )  const BYTE          *pbKids,
    _Out_                       BOOL          *pfAllKidsHaveKeys )
{
    HRESULT hr                  = S_OK;
    BOOL    fAllKidsHaveKeys    = TRUE;

    for( DWORD ibKids = 0; ibKids < cbKids && fAllKidsHaveKeys; ibKids += sizeof( GUID ) )
    {
        GUID guidKidExpected;
        BOOL fKidHasKey      = FALSE;

        BIG_ENDIAN_SIXTEEN_BYTES_TO_GUID( &pbKids[ ibKids ], &guidKidExpected );

        //
        // Enumerate existing keys and check to see whether we have a key for the current Kid
        //
        IF_FAILED_GOTO( ForAllKeys( pIMFAttributes, CLEARKEY_INVALID_SESSION_ID, [&fKidHasKey, &guidKidExpected]( REFGUID guidKeyDataLambda, BYTE *pbKeyDataLambda, BOOL *pfHaltEnumerationLambda )->HRESULT
        {
            MF_MEDIAKEY_STATUS eCurrentStatus = MF_MEDIAKEY_STATUS_INTERNAL_ERROR;
            GUID guidKid = GUID_NULL;
            double dblExpiration = NaN.f;

            GetKeyData( pbKeyDataLambda, nullptr, &guidKid, nullptr, &eCurrentStatus, nullptr, nullptr, &dblExpiration, nullptr );

            fKidHasKey = IS_STATUS_USABLE_OR_PENDING( eCurrentStatus ) && guidKid == guidKidExpected && !IsExpired( dblExpiration );

            if( !fKidHasKey )
            {
                *pfHaltEnumerationLambda = TRUE;
            }

            return S_OK;
        } ) );

        fAllKidsHaveKeys = fAllKidsHaveKeys && fKidHasKey;
    }

    *pfAllKidsHaveKeys = fAllKidsHaveKeys;

done:
    return hr;
}

HRESULT GetKeyDataForLicense(
    _In_                                              IMFAttributes  *pIMFAttributes,
    _In_                                              int             sessionId,
    _In_                                              REFGUID         guidKidDesired,
    _In_                                              REFGUID         guidLidDesired,
    _Out_writes_bytes_( KEY_DATA_TOTAL_SIZE )         BYTE            pbKeyData[ KEY_DATA_TOTAL_SIZE ] )
{
    HRESULT hr      = S_OK;
    GUID    guidKid = GUID_NULL;
    GUID    guidLid = GUID_NULL;

    IF_FAILED_GOTO( ForAllKeys( pIMFAttributes, sessionId, [&pbKeyData, &guidKid, &guidLid, &guidKidDesired, &guidLidDesired]( REFGUID guidKeyDataLambda, BYTE *pbKeyDataLambda, BOOL *pfHaltEnumerationLambda )->HRESULT
    {
        guidKid = GUID_NULL;
        guidLid = GUID_NULL;
        GetKeyData( pbKeyDataLambda, nullptr, &guidKid, nullptr, nullptr, &guidLid, nullptr, nullptr, nullptr );
        if( guidKidDesired == guidKid && guidLidDesired == guidLid )
        {
            memcpy( pbKeyData, pbKeyDataLambda, KEY_DATA_TOTAL_SIZE );
            *pfHaltEnumerationLambda = TRUE;
        }
        return S_OK;
    } ) );

    IF_FALSE_GOTO( guidKid == guidKidDesired, MF_E_LICENSE_INCORRECT_RIGHTS );
    IF_FALSE_GOTO( guidLid == guidLidDesired, MF_E_LICENSE_INCORRECT_RIGHTS );

done:
    return hr;
}

