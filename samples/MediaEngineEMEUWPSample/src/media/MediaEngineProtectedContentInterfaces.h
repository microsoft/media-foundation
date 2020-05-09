// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

// These interfaces / constants are required for supporting protected playback using a CDM with the IMFMediaEngine
// They are defined in mfidl.h, however they are currently not available in the app partition (this will be addressed in a newer version of the SDK)

MIDL_INTERFACE("ACF92459-6A61-42bd-B57C-B43E51203CB0")
IMFContentProtectionManager : public IUnknown
{
public:
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE BeginEnableContent(
        /* [in] */ IMFActivate * pEnablerActivate,
        /* [in] */ IMFTopology * pTopo,
        /* [in] */ IMFAsyncCallback * pCallback,
        /* [in] */ IUnknown * punkState) = 0;

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE EndEnableContent(
        /* [in] */ IMFAsyncResult* pResult) = 0;

};

enum MF_URL_TRUST_STATUS
{
    MF_LICENSE_URL_UNTRUSTED = 0,
    MF_LICENSE_URL_TRUSTED = (MF_LICENSE_URL_UNTRUSTED + 1),
    MF_LICENSE_URL_TAMPERED = (MF_LICENSE_URL_TRUSTED + 1)
};

MIDL_INTERFACE("D3C4EF59-49CE-4381-9071-D5BCD044C770")
IMFContentEnabler : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetEnableType(
        /* [out] */ __RPC__out GUID * pType) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetEnableURL(
        /* [size_is][size_is][out] */ __RPC__deref_out_ecount_full_opt(*pcchURL) LPWSTR* ppwszURL,
        /* [out] */ __RPC__out DWORD* pcchURL,
        /* [unique][out][in] */ __RPC__inout_opt MF_URL_TRUST_STATUS* pTrustStatus) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetEnableData(
        /* [size_is][size_is][out] */ __RPC__deref_out_ecount_full_opt(*pcbData) BYTE** ppbData,
        /* [out] */ __RPC__out DWORD* pcbData) = 0;

    virtual HRESULT STDMETHODCALLTYPE IsAutomaticSupported(
        /* [out] */ __RPC__out BOOL* pfAutomatic) = 0;

    virtual HRESULT STDMETHODCALLTYPE AutomaticEnable(void) = 0;

    virtual HRESULT STDMETHODCALLTYPE MonitorEnable(void) = 0;

    virtual HRESULT STDMETHODCALLTYPE Cancel(void) = 0;

};

EXTERN_GUID(MFENABLETYPE_MF_UpdateRevocationInformation, 0xe558b0b5, 0xb3c4, 0x44a0, 0x92, 0x4c, 0x50, 0xd1, 0x78, 0x93, 0x23, 0x85);
EXTERN_GUID(MFENABLETYPE_MF_UpdateUntrustedComponent, 0x9879f3d6, 0xcee2, 0x48e6, 0xb5, 0x73, 0x97, 0x67, 0xab, 0x17, 0x2f, 0x16);
EXTERN_GUID(MFENABLETYPE_MF_RebootRequired, 0x6d4d3d4b, 0x0ece, 0x4652, 0x8b, 0x3a, 0xf2, 0xd2, 0x42, 0x60, 0xd8, 0x87);

MIDL_INTERFACE("fbe256c1-43cf-4a9b-8cb8-ce8632a34186")
IMFMediaEngineClassFactory4 : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE CreateContentDecryptionModuleFactory(
        /* [annotation][in] */
        _In_  LPCWSTR keySystem,
        /* [annotation][in] */
        _In_  REFIID riid,
        /* [annotation][iid_is][out] */
        _Outptr_  LPVOID * ppvObject) = 0;

};

enum MF_MEDIAKEYS_REQUIREMENT
{
    MF_MEDIAKEYS_REQUIREMENT_REQUIRED	= 1,
    MF_MEDIAKEYS_REQUIREMENT_OPTIONAL	= 2,
    MF_MEDIAKEYS_REQUIREMENT_NOT_ALLOWED	= 3
};

EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_INITDATATYPES =     { { 0x497d231b, 0x4eb9, 0x4df0, { 0xb4, 0x74, 0xb9, 0xaf, 0xeb, 0x0a, 0xdf, 0x38 } }, PID_FIRST_USABLE+0x00000001 }; 
EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_DISTINCTIVEID =     { { 0x7dc9c4a5, 0x12be, 0x497e, { 0x8b, 0xff, 0x9b, 0x60, 0xb2, 0xdc, 0x58, 0x45 } }, PID_FIRST_USABLE+0x00000002 }; 
EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_PERSISTEDSTATE =    { { 0x5d4df6ae, 0x9af1, 0x4e3d, { 0x95, 0x5b, 0x0e, 0x4b, 0xd2, 0x2f, 0xed, 0xf0 } }, PID_FIRST_USABLE+0x00000003 }; 
EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_AUDIOCAPABILITIES = { { 0x980fbb84, 0x297d, 0x4ea7, { 0x89, 0x5f, 0xbc, 0xf2, 0x8a, 0x46, 0x28, 0x81 } }, PID_FIRST_USABLE+0x00000004 }; 
EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_VIDEOCAPABILITIES = { { 0xb172f83d, 0x30dd, 0x4c10, { 0x80, 0x06, 0xed, 0x53, 0xda, 0x4d, 0x3b, 0xdb } }, PID_FIRST_USABLE+0x00000005 }; 
EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_LABEL =             { { 0x9eae270e, 0xb2d7, 0x4817, { 0xb8, 0x8f, 0x54, 0x00, 0x99, 0xf2, 0xef, 0x4e } }, PID_FIRST_USABLE+0x00000006 }; 
EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_SESSIONTYPES =      { { 0x7623384f, 0x00f5, 0x4376, { 0x86, 0x98, 0x34, 0x58, 0xdb, 0x03, 0x0e, 0xd5 } }, PID_FIRST_USABLE+0x00000007 }; 
EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_ROBUSTNESS =  { { 0x9d3d2b9e, 0x7023, 0x4944, { 0xa8, 0xf5, 0xec, 0xca, 0x52, 0xa4, 0x69, 0x90 } }, PID_FIRST_USABLE+0x00000001 }; 
EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_CONTENTTYPE = { { 0x289fb1fc, 0xd9c4, 0x4cc7, { 0xb2, 0xbe, 0x97, 0x2b, 0x0e, 0x9b, 0x28, 0x3a } }, PID_FIRST_USABLE+0x00000002 }; 
EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_CDM_INPRIVATESTOREPATH = { { 0xec305fd9, 0x039f, 0x4ac8, { 0x98, 0xda, 0xe7, 0x92, 0x1e, 0x00, 0x6a, 0x90 } }, PID_FIRST_USABLE+0x00000001 }; 
EXTERN_C const DECLSPEC_SELECTANY PROPERTYKEY MF_EME_CDM_STOREPATH =          { { 0xf795841e, 0x99f9, 0x44d7, { 0xaf, 0xc0, 0xd3, 0x09, 0xc0, 0x4c, 0x94, 0xab } }, PID_FIRST_USABLE+0x00000002 }; 