//**@@@*@@@****************************************************
//
// Microsoft Windows MediaFoundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//**@@@*@@@****************************************************

//
// This file contains declarations that are currently Microsoft-Internal but
// will become public as part of the overall "3rd-party CDM enablement" project.
//

#pragma region Private declarations from mfgrl.h that need to become public

#define GRL_HEADER_IDENTIFIER L"MSGRL"
#define GRL_FORMAT_MAJOR 1
#define GRL_FORMAT_MINOR 0

typedef struct _GRL_HEADER
{
    WCHAR       wszIdentifier[ 6 ];
    WORD        wFormatMajor;
    WORD        wFormatMinor;
    FILETIME    CreationTime;
    DWORD       dwSequenceNumber;
    DWORD       dwForceRebootVersion;
    DWORD       dwForceProcessRestartVersion;

    DWORD       cbRevocationSectionOffset;
    DWORD       cRevokedKernelBinaries;
    DWORD       cRevokedUserBinaries;
    DWORD       cRevokedCertificates;
    DWORD       cTrustedRoots;

    DWORD       cbExtensibleSectionOffset;
    DWORD       cExtensibleEntries;

    DWORD       cbRenewalSectionOffset;
    DWORD       cRevokedKernelBinaryRenewals;
    DWORD       cRevokedUserBinaryRenewals;
    DWORD       cRevokedCertificateRenewals;

    DWORD       cbSignatureCoreOffset;
    DWORD       cbSignatureExtOffset;
} GRL_HEADER;

#define GRL_HASH_SIZE 20
#define GRL_EXTENSIBLE_ENTRY_SIZE 4096

typedef struct _GRL_ENTRY
{
    BYTE rgbGRLEntry[ GRL_HASH_SIZE ];
} GRL_ENTRY;

typedef struct _GRL_EXTENSIBLE_ENTRY
{
    GUID guidExtensionID;
    BYTE rgbExtensibleEntry[ GRL_EXTENSIBLE_ENTRY_SIZE ];
} GRL_EXTENSIBLE_ENTRY;

typedef struct _GRL_RENEWAL_ENTRY
{
    GRL_ENTRY grlEntry;
    GUID      guidRenewalID;
} GRL_RENEWAL_ENTRY;

#pragma pack(push, 1)
typedef struct _MF_SIGNATURE
{
    DWORD dwSignVer;
    DWORD cbSign;
    BYTE rgSign[ 1 ];
} MF_SIGNATURE;
#pragma pack(pop)

#pragma endregion Private declarations from mfgrl.h that need to become public

#pragma region Private declarations from PEAuthCommunication.w that need to become public

//
// Device type           -- in the "User Defined" range."
//
#define PEAUTH_TYPE 40001

//
// The IOCTL function codes from 0x800 to 0xFFF are for customer use.
//

#define IOCTL_PEAUTH_PROCESS_MESSAGE \
    CTL_CODE( PEAUTH_TYPE, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS  )


//
// Request / response structures
//
#define PEAUTHMSG_VERSION 1

typedef enum PEAUTH_MSG_TYPE
{
    PEAUTH_MSG_CERTCHAIN = 0,
    PEAUTH_MSG_ACQUIRESESSIONKEY,
    PEAUTH_MSG_STATEINQUIRY,
    PEAUTH_MSG_GRLRELOAD,
    PEAUTH_MSG_COMPONENT_LIST
} PEAUTH_MSG_TYPE;

typedef enum PEAUTH_MSG_SECURITY_LEVEL
{
    PEAUTH_MSG_HIGH_SECURITY = 0,
    PEAUTH_MSG_LOW_SECURITY
} PEAUTH_MSG_SECURITY_LEVEL;

//
// Flags for RESPONSE_STATUS
//

#define PEAUTH_VERIFY_DEBUG_CREDENTIALS      0x00000001
#define PEAUTH_KERNEL_UNSECURE               0x00000002
#define PEAUTH_USER_MODE_UNSECURE            0x00000004
#define PEAUTH_PROCESS_RESTART_REQUIRED      0x00000008
#define PEAUTH_ALL_PROCESS_RESTART_REQUIRED  0x00000010 
#define PEAUTH_REBOOT_REQUIRED               0x00000020
#define PEAUTH_PE_TRUSTED                    0x00000040
#define PEAUTH_PE_UNTRUSTED                  0x00000080
#define PEAUTH_USER_MODE_DEBUGGER            0x00000100 

//
// DO NOT ADD FLAGS IN THE 0xFF000000 RANGE - THEY ARE USED
// FOR PEAUTHTRUST BITS.
//
// PEAUTH Trust Bits detail which root was used to sign the
// binary.  These are included in RESPONSE_STATUS.
//

#define PEAUTH_TRUSTBITS_MASK               0xFF000000

// Offical DMD or MS system root
#define PEAUTH_TRUST_SYSTEM_ROOT            0x00000001

// MS test root
#define PEAUTH_TRUST_MS_TEST_ROOT           0x00000002

// DMD Beta Root
#define PEAUTH_TRUST_DMD_BETA_ROOT          0x00000004

// DMD Test Root
#define PEAUTH_TRUST_DMD_TEST_ROOT          0x00000008

// Trust bit MACROS - use these to set / get the policy bits for either 
// user or kernel mode binaries.

#define PEAUTH_SET_KERNEL_TRUST(_s_, _b_ )   ( _s_ |= (( _b_  << 0x1c ) & 0xF0000000))
#define PEAUTH_SET_USERMODE_TRUST(_s_, _b_ ) ( _s_ |=  (( _b_ << 0x18 ) & 0x0F000000))
#define PEAUTH_KERNEL_TRUST_BITS( _b_ )      (( _b_ >> 0x1c ) & 0x0000000F)
#define PEAUTH_USERMODE_TRUST_BITS( _b_ )    (( _b_ >> 0x18 ) & 0x0000000F)

#define PEAUTH_IS_REAL_ROOT( _b_ )       ((_b_ & ( PEAUTH_TRUST_MS_TEST_ROOT | PEAUTH_TRUST_DMD_TEST_ROOT )) == 0)
#define PEAUTH_IS_MS_TEST_ROOT( _b_ )       ((_b_ & PEAUTH_TRUST_MS_TEST_ROOT ) != 0)
#define PEAUTH_IS_REAL_PMP_BETA_ROOT( _b_ ) ((_b_ & PEAUTH_TRUST_DMD_BETA_ROOT) != 0)
#define PEAUTH_IS_TEST_PMP_BETA_ROOT( _b_ ) ((_b_ & PEAUTH_TRUST_DMD_TEST_ROOT) != 0)


// This value corresponds to RTL_MAX_HASH_LEN_V1 in hashlib.h
#define PEAUTH_COMP_MAX_HASH_LEN         20
#define PEAUTH_COMP_MAX_NAME_LEN         MAX_PATH

//
// Flags for events during PEAuth initialization
//

#define PEAUTH_EVENT_DEBUGGER_FOUND            0x00000001
#define PEAUTH_EVENT_UNSAFE_MEM_DUMP           0x00000002
#define PEAUTH_EVENT_INTEGRITY_CHECKS_DISABLED 0x00000004

//
// Flags for component load type
//

#define PEAUTH_EVENT_USER_MODE_COMPONENT_LOADING   0x00000001
#define PEAUTH_EVENT_KERNEL_MODE_COMPONENT_LOADING 0x00000002

//
// Flags for failures during component load
//

#define PEAUTH_EVENT_GRL_LOAD_FAILED                 0x00000010
#define PEAUTH_EVENT_INVALID_GRL_SIGNATURE           0x00000020
#define PEAUTH_EVENT_GRL_ABSENT                      0x00001000
#define PEAUTH_EVENT_COMPONENT_REVOKED               0x00002000
#define PEAUTH_EVENT_COMPONENT_INVALID_EKU           0x00004000
#define PEAUTH_EVENT_COMPONENT_CERT_REVOKED          0x00008000
#define PEAUTH_EVENT_COMPONENT_INVALID_ROOT          0x00010000
#define PEAUTH_EVENT_COMPONENT_HS_CERT_REVOKED       0x00020000
#define PEAUTH_EVENT_COMPONENT_LS_CERT_REVOKED       0x00040000
#define PEAUTH_EVENT_BOOT_DRIVER_VERIFICATION_FAILED 0x00100000
#define PEAUTH_EVENT_TEST_SIGNED_COMPONENT_LOADING   0x01000000
#define PEAUTH_EVENT_MINCRYPT_FAILURE                0x10000000

//
// PEAuth options
//

#define PEAUTH_ENABLE_LOGGING 0x00000001

//
// PEAuth returned component information
//
typedef struct _PEAUTH_COMPONENT_HASH_INFO
{
    ULONG ulReason;
    UCHAR rgHeaderHash[ PEAUTH_COMP_MAX_HASH_LEN ];
    UCHAR rgPublicKeyHash[ PEAUTH_COMP_MAX_HASH_LEN ];
    WCHAR wszName[ PEAUTH_COMP_MAX_NAME_LEN ];
} PEAUTH_COMPONENT_HASH_INFO, *PPEAUTH_COMPONENT_HASH_INFO;

//
// PEAuth returned status
//
typedef struct _RESPONSE_STATUS
{
    HRESULT hStatus;
    ULONG   ResponseFlag;
} RESPONSE_STATUS;

//
// MSG sent to PEAuth
//

typedef struct _INQUIRY_CERTCHAIN
{
    ULONG cbCertChain;
} INQUIRY_CERTCHAIN;

//
// Response from PEAuth for Cert chain
//

typedef struct _RESPONSE_CERTCHAIN
{
    ULONG  ulCertChainOffset;
    ULONG  cbCertChain;
} RESPONSE_CERTCHAIN;

//
// MSG sent to PEAuth
//

typedef struct _INQUIRY_SESSIONKEY
{
    //
    // Offset to encrypted session 
    // with PEAuth RSA public key
    //

    ULONG ulEncryptedSessionKeyOffset;

    // 
    // size of encrypted session key
    //

    ULONG cbEncryptedSessionkey;
} INQUIRY_SESSIONKEY;

//
// Response from PEAuth
//

typedef struct _RESPONSE_SESSIONKEY
{
    //
    // Session key Encrypted with Temp key
    //
    BYTE  rgEncryptedSessionKey[ 16 ];
} RESPONSE_SESSIONKEY;

//
// MSG sent to PEAuth
//

typedef struct _INQUIRY_KERNELSTATE
{
    ULONG GRLVersion;
    BYTE  rgNonce[ 16 ];
} INQUIRY_KERNELSTATE;

//
// Response from PEAuth
//

typedef struct _RESPONSE_KERNELSTATE
{
    //
    // Session key Encrypted with Temp key
    //
    ULONG GRLVersion;
    BYTE  rgNonce[ 16 ];
} RESPONSE_KERNELSTATE;

//
// MSG sent to PEAuth
//

typedef struct _INQUIRY_COMPLIST
{
    ULONG cbCompInfo;
    ULONG ulProcessId;
} INQUIRY_COMPLIST;

//
// Response from PEAuth for component list
//

typedef struct _RESPONSE_COMPLIST
{
    ULONG  ulCompInfoOffset;
    ULONG  cbCompInfo;
} RESPONSE_COMPLIST;


typedef struct _PEAUTH_MSG
{
    ULONG Version;
    ULONG cbMsgSize;
    PEAUTH_MSG_TYPE Type;
    PEAUTH_MSG_SECURITY_LEVEL SecurityLevel;
    union MsgBody
    {
        INQUIRY_CERTCHAIN   CertChainMsg;
        INQUIRY_SESSIONKEY  SessionMsg;
        INQUIRY_KERNELSTATE KernelStateMsg;
        INQUIRY_COMPLIST    ComponentInfoList;
    } MsgBody;

} PEAUTH_MSG;

typedef struct _PEAUTH_MSG_RESPONSE_BODY
{
    RESPONSE_STATUS Status;
    union RespMsgBody
    {
        RESPONSE_CERTCHAIN   CertChainMsg;
        RESPONSE_SESSIONKEY  SessionMsg;
        RESPONSE_KERNELSTATE KernelStateMsg;
        RESPONSE_COMPLIST    ComponentInfoList;
    } RespMsgBody;

} PEAUTH_MSG_RESPONSE_BODY;

// The size of rgSignedValue is AES block length - 16 bytes.
typedef struct _PEAUTH_MSG_RESPONSE
{
    ULONG Version;
    PEAUTH_MSG_RESPONSE_BODY ResponseBody;
    BYTE rgSignedValue[ 16 ];
} PEAUTH_MSG_RESPONSE;

#pragma endregion Private declarations from PEAuthCommunication.w that need to become public

