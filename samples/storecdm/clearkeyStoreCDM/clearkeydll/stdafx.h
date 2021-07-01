//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#define CLEARKEY_INCLUDE_MICROSOFT_SPECIFIC_EXTENSIONS 1
#define CLEARKEY_INCLUDE_PEAUTH_HANDSHAKE 1
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP

//
// General SDK and WRL headers
//
#include <windows.h>
#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <windows.storage.h>
#include <windows.data.json.h>
#include <wrl\implements.h>
#include <wrl\module.h>
#include <wrl\event.h>
#include <wrl\client.h>
#include <windows.media.h>
#include <windows.media.protection.h>
#include <winstring.h>
#include <nserror.h>
#include <rtworkq.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <stdio.h>

//
// Media foundation
//
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mftransform.h>
#include <mfmediaengine.h>

using namespace ABI::Windows::Media;
using namespace ABI::Windows::Media::Protection;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Data::Json;
using namespace ABI::Windows::Data::Xml::Dom;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

//  Our own definitions
#define AES_BLOCK_LEN 16

#define IF_FAILED_GOTO(_x_) do { hr = (_x_); if (FAILED(hr)) goto done; } while(FALSE)
#define IF_NULL_GOTO(_x_, _hr_)  do { if (nullptr == (_x_)) { hr = _hr_; goto done; } } while(FALSE)
#define IF_FALSE_GOTO(_x_, _hr_) do { if (!(_x_)) { hr = _hr_; goto done; } } while(FALSE)
#define SAFE_RELEASE( _p_ ) do { if( (_p_) ) { (_p_)->Release(); (_p_) = nullptr; } } while(FALSE)

#define SAFE_ADD_DWORD_WITH_MAX( _dw1_, _dw2_, _dwMax_, _pdwOut_ ) do {                               \
    ULONGLONG _ull_ = (ULONGLONG)(_dw1_) + (ULONGLONG)(_dw2_);                                        \
    IF_FALSE_GOTO( _ull_ <= (ULONGLONG)(_dwMax_), HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) );    \
    *(_pdwOut_) = (DWORD)_ull_;                                                                       \
} while(FALSE)

#define SAFE_ADD_DWORD( _dw1_, _dw2_, _pdwOut_ ) SAFE_ADD_DWORD_WITH_MAX( _dw1_, _dw2_, 0xFFFFFFFF, _pdwOut_ )

#define SAFE_MULT_DWORD_WITH_MAX( _dw1_, _dw2_, _dwMax_, _pdwOut_ ) do {                              \
    ULONGLONG _ull_ = (ULONGLONG)(_dw1_) * (ULONGLONG)(_dw2_);                                        \
    IF_FALSE_GOTO( _ull_ <= (ULONGLONG)(_dwMax_), HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) );    \
    *(_pdwOut_) = (DWORD)_ull_;                                                                       \
} while(FALSE)

#define SAFE_MULT_DWORD( _dw1_, _dw2_, _pdwOut_ ) SAFE_MULT_DWORD_WITH_MAX( _dw1_, _dw2_, 0xFFFFFFFF, _pdwOut_ )

#define BIG_ENDIAN_FOUR_BYTES_TO_DWORD( _pb_, _pdw_ ) do {                              \
    *(_pdw_) = 0;                                                                       \
    for( DWORD _idx_ = 0; _idx_ < sizeof( DWORD ); _idx_++ )                            \
    {                                                                                   \
        *(_pdw_) |= (DWORD)( ( (DWORD)(_pb_)[ _idx_ ] ) << (DWORD)( 24 - 8 * _idx_ ) ); \
    }                                                                                   \
} while(FALSE)

#define BIG_ENDIAN_TWO_BYTES_TO_WORD( _pb_, _pw_ ) do { *( _pw_ ) = ( (WORD)( _pb_ )[ 0 ] << (WORD)8 ) | (WORD)( _pb_ )[ 1 ]; } while( FALSE )

//
// Converting to/from big endian is the same operation (reversal), so even though these are converting LE to BE, they can reuse the BE to LE macros.
//
#define DWORD_TO_BIG_ENDIAN_FOUR_BYTES( _pdw_, _pb_ ) BIG_ENDIAN_FOUR_BYTES_TO_DWORD( (BYTE*)(_pdw_), (DWORD*)(_pb_) )
#define WORD_TO_BIG_ENDIAN_TWO_BYTES( _pw_, _pb_ ) BIG_ENDIAN_TWO_BYTES_TO_WORD( (BYTE*)(_pw_), (WORD*)(_pb_) )

#define BIG_ENDIAN_SIXTEEN_BYTES_TO_GUID( _pb_, _pgd_ ) do {                                                    \
    BIG_ENDIAN_FOUR_BYTES_TO_DWORD( ( &( _pb_ )[ 0 ] ), &(_pgd_)->Data1 );                                      \
    BIG_ENDIAN_TWO_BYTES_TO_WORD( ( &( _pb_ )[ sizeof( DWORD ) ] ), &(_pgd_)->Data2 );                          \
    BIG_ENDIAN_TWO_BYTES_TO_WORD( ( &( _pb_ )[ sizeof( DWORD ) + sizeof( WORD ) ] ), &(_pgd_)->Data3 );         \
    memcpy( &(_pgd_)->Data4, ( &( _pb_ )[ sizeof( DWORD ) + 2 * sizeof( WORD ) ] ), sizeof( (_pgd_)->Data4 ) ); \
} while(FALSE)

#define GUID_TO_BIG_ENDIAN_SIXTEEN_BYTES( _pgd_, _pb_ ) do {                                                    \
    DWORD_TO_BIG_ENDIAN_FOUR_BYTES( &(_pgd_)->Data1, ( &( _pb_ )[ 0 ] ) );                                      \
    WORD_TO_BIG_ENDIAN_TWO_BYTES( &(_pgd_)->Data2, ( &( _pb_ )[ sizeof( DWORD ) ] ) );                          \
    WORD_TO_BIG_ENDIAN_TWO_BYTES( &(_pgd_)->Data3, ( &( _pb_ )[ sizeof( DWORD ) + sizeof( WORD ) ] ) );         \
    memcpy( ( &( _pb_ )[ sizeof( DWORD ) + 2 * sizeof( WORD ) ] ), &(_pgd_)->Data4, sizeof( (_pgd_)->Data4 ) ); \
} while(FALSE)

//
// Union to allow compile-time construction of Nan constant without using namespace std.
// 'double foo = NaN.f' is equivalent to 'double foo = std::numeric_limits<double>::quiet_NaN()'
//
union Int64AsDouble
{
    __int64 i;
    double f;
};
const Int64AsDouble NaN = { 0x7ff8000000000000 };

//
// A kid in base64url encoding is exactly 22 characters long (not including null terminator).
// A kid in standard base64 encoding is exactly 24 characters long (not including null terminator).
// Example: "L-VHf8JLtPrv2GUXFW2v_A"  (which would translate to  "L+VHf8JLtPrv2GUXFW2v/A=="  in standard base64)
//
#define GUID_B64_URL_LEN 22
#define GUID_B64_LEN 24

#define CLEARKEY_INVALID_SESSION_ID 0

#if DBG
__pragma( optimize( "", off ) )
#undef CLEARKEY_DEBUG_OUT
#define CLEARKEY_DEBUG_OUT( _str_ ) do { ::OutputDebugStringA("\n"); ::OutputDebugStringA((_str_)); } while(FALSE)
#else   //DBG
#undef CLEARKEY_DEBUG_OUT
#define CLEARKEY_DEBUG_OUT(...)
#endif  //DBG

//
// Note: Comparison logic for policy rotation *REQUIRES* that more restrictive policies/
// have larger integer values than less restrictive policies.
// In addition, zero MUST represent "no policy to enforce".
// For example, requiring HDCP Type 1 is more restrictive than HDCP with any version,
// so the integer value for the former is greater than the integer value for the latter,
// and both are larger than "do not attempt" which has a value of zero (nothing to enforce).
//
// Search for variable fMoreRestrictivePolicy in mft.cpp for more information.
//

// dwHDCP
#define CLEARKEY_POLICY_DO_NOT_ENFORCE_HDCP                         0
#define CLEARKEY_POLICY_REQUIRE_HDCP_WITH_ANY_VERSION               1
#define CLEARKEY_POLICY_REQUIRE_HDCP_WITH_TYPE_1                    2

// dwCGMSA
#define CLEARKEY_POLICY_DO_NOT_ENFORCE_CGMSA                        0
#define CLEARKEY_POLICY_BEST_EFFORT_CGMSA_COPY_FREE                 1
#define CLEARKEY_POLICY_BEST_EFFORT_CGMSA_COPY_ONCE                 2
#define CLEARKEY_POLICY_BEST_EFFORT_CGMSA_COPY_NEVER                3
#define CLEARKEY_POLICY_REQUIRE_CGMSA_COPY_FREE                     4
#define CLEARKEY_POLICY_REQUIRE_CGMSA_COPY_ONCE                     5
#define CLEARKEY_POLICY_REQUIRE_CGMSA_COPY_NEVER                    6

// dwDigitalOnly
#define CLEARKEY_POLICY_ALLOW_ANALOG_OUTPUT                         0
#define CLEARKEY_POLICY_BLOCK_ANALOG_OUTPUT                         1

#pragma pack(push, 1)
typedef struct
{
    DWORD dwHDCP;
    DWORD dwCGMSA;
    DWORD dwDigitalOnly;
} CLEARKEY_KEY_DATA_OUTPUT_POLICY_REQUIREMENTS;
#pragma pack(pop)

#include "storecdm_clearkey.h"
#include "debughelper.h"
#include "guids.h"
#include "typehelper.h"
#include "keydatahelper.h"
#include "mfasynchelper.h"
#include "dynamicarrayhelper.h"
#include "peauthhelper.h"
#include "cdm.h"
#include "mft.h"
#include "ce.h"

using namespace ABI::org::w3::clearkey;

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS 0
#endif  //STATUS_SUCCESS

#define CLEARKEY_CONFIRM_NTSTATUS( _status_, _hr_ ) (_hr_) = FAILED(_hr_) ? (_hr_) : (_status_) != STATUS_SUCCESS ? MF_E_UNEXPECTED : (_hr_)

