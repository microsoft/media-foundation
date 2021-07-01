//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#pragma once

#include <windows.h>

void AddValueToEightByteBigEndianInteger( _Inout_count_( sizeof( QWORD ) ) BYTE rgbExistingValue[ sizeof( QWORD ) ], _In_ QWORD qwValueToAdd );
#define ADD_TO_IV( _rgb16ByteIV_, _qwValue_ ) AddValueToEightByteBigEndianInteger( &((_rgb16ByteIV_)[8]), (_qwValue_) )
#define ADD_TO_GUID( _guid_, _qwValue_ ) AddValueToEightByteBigEndianInteger( &(((BYTE*)(&(_guid_)))[8]), (_qwValue_) )

HRESULT SetKeyDataInitial(
    _Inout_count_( KEY_DATA_TOTAL_SIZE )       BYTE                                          pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _In_                                       int                                           sessionId,
    _In_                                       REFGUID                                       guidKid,
    _In_count_( KEY_DATA_KEY_SIZE )      const BYTE                                          pbAES128Key[ KEY_DATA_KEY_SIZE ],
    _In_                                       double                                        dblExpiration,
    _In_                                 const CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS *pPolicy );

void SetKeyDataUpdateStatus(
    _Inout_count_( KEY_DATA_TOTAL_SIZE ) BYTE               pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _In_                                 MF_MEDIAKEY_STATUS eNewStatus );

void SetKeyDataUpdateNotifiedStatus(
    _Inout_count_( KEY_DATA_TOTAL_SIZE )       BYTE               pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _In_opt_                             const BYTE               pbAES128Key[ KEY_DATA_KEY_SIZE ],
    _In_                                       MF_MEDIAKEY_STATUS eNewStatus,
    _In_                                       MF_MEDIAKEY_STATUS eLastNotifiedStatus );

void GetKeyData(
    _In_count_( KEY_DATA_TOTAL_SIZE ) const BYTE                                             pbKeyData[ KEY_DATA_TOTAL_SIZE ],
    _Out_opt_                               int                                             *pSessionId,
    _Out_opt_                               GUID                                            *pguidKid,
    _Out_opt_                               BYTE                                             pbAES128Key[ KEY_DATA_KEY_SIZE ],
    _Out_opt_                               MF_MEDIAKEY_STATUS                              *peCurrentStatus,
    _Out_opt_                               GUID                                            *pguidLid,
    _Out_opt_                               MF_MEDIAKEY_STATUS                              *peLastNotifiedStatus,
    _Out_opt_                               double                                          *pdblExpiration,
    _Out_opt_                               CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS    *pPolicy );

BOOL IsExpired( _In_ double dblExpiration );
#define IS_STATUS_USABLE_OR_PENDING( _eStatus_ ) ( ( (_eStatus_) == MF_MEDIAKEY_STATUS_USABLE ) || ( (_eStatus_) == MF_MEDIAKEY_STATUS_STATUS_PENDING ) )

HRESULT ConvertB64urlStringToBigEndianSixteenBytes( _In_count_( GUID_B64_URL_LEN ) const WCHAR pwszGuid[ GUID_B64_URL_LEN ], _Out_writes_bytes_( sizeof( GUID ) ) BYTE pbGuid[ sizeof( GUID ) ] );

HRESULT GetKidsFromPSSH(
    _In_                        DWORD      cbPSSH,
    _In_count_( cbPSSH )  const BYTE      *pbPSSH,
    _Out_                       DWORD     *pcbKids,
    _Out_                       BYTE     **ppbKids );

HRESULT DoAllKidsHaveKeysInAttributes(
    _In_                        IMFAttributes *pIMFAttributes,
    _In_                        DWORD          cbKids,
    _In_count_( cbKids )  const BYTE          *pbKids,
    _Out_                       BOOL          *pfAllKidsHaveKeys );

template<typename TFunc>
HRESULT ForAllKeys(
    _In_    IMFAttributes  *pIMFAttributes,
    _In_    int             sessionId,
    _In_    TFunc         &&func )
{
    HRESULT hr          = S_OK;
    GUID    guidKeyData = CLEARKEY_GUID_ATTRIBUTE_KEYDATA_LIST_BASE;
    BOOL    fHalt       = FALSE;
    BYTE    rgbKeyData[ KEY_DATA_TOTAL_SIZE ];

    hr = pIMFAttributes->GetBlob( guidKeyData, rgbKeyData, KEY_DATA_TOTAL_SIZE, nullptr );
    while( SUCCEEDED( hr ) && !fHalt )
    {
        int sessionIdKey;
        GetKeyData( rgbKeyData, &sessionIdKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
        if( sessionId == CLEARKEY_INVALID_SESSION_ID || sessionIdKey == sessionId )
        {
            IF_FAILED_GOTO( func( guidKeyData, rgbKeyData, &fHalt ) );
        }
        ADD_TO_GUID( guidKeyData, 1 );
        hr = pIMFAttributes->GetBlob( guidKeyData, rgbKeyData, KEY_DATA_TOTAL_SIZE, nullptr );
    }
    if( hr == MF_E_ATTRIBUTENOTFOUND )
    {
        hr = S_OK;
    }
    IF_FAILED_GOTO( hr );

done:
    return hr;
}

HRESULT GetKeyDataForLicense(
    _In_                                              IMFAttributes  *pIMFAttributes,
    _In_                                              int             sessionId,
    _In_                                              REFGUID         guidKidDesired,
    _In_                                              REFGUID         guidLidDesired,
    _Out_writes_bytes_( KEY_DATA_TOTAL_SIZE )         BYTE            pbKeyData[ KEY_DATA_TOTAL_SIZE ] );


