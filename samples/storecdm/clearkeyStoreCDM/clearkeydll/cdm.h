//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#pragma once

#include <windows.h>
#include <mfidl.h>

using namespace ABI::org::w3::clearkey;

class Cdm_clearkey_IMFContentDecryptionModuleFactory final :
    public Microsoft::WRL::RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
    IUnused,
    IMFContentDecryptionModuleFactory,
    FtmBase>
{
    InspectableClass( RuntimeClass_org_w3_clearkey_CdmFactory, BaseTrust );

public:
    Cdm_clearkey_IMFContentDecryptionModuleFactory();
    ~Cdm_clearkey_IMFContentDecryptionModuleFactory();

    STDMETHODIMP_( BOOL ) IsTypeSupported(
        _In_ LPCWSTR keySystem,
        _In_opt_ LPCWSTR contentType
    );

    STDMETHODIMP CreateContentDecryptionModuleAccess(
        _In_ LPCWSTR keySystem,
        _In_count_(numConfigurations) IPropertyStore **configurations,
        _In_ DWORD numConfigurations,
        _COM_Outptr_ IMFContentDecryptionModuleAccess **contentDecryptionModuleAccess
    );

private:
    CriticalSection m_Lock;

    Cdm_clearkey_IMFContentDecryptionModuleFactory( const Cdm_clearkey_IMFContentDecryptionModuleFactory &nonCopyable ) = delete;
    Cdm_clearkey_IMFContentDecryptionModuleFactory& operator=( const Cdm_clearkey_IMFContentDecryptionModuleFactory &nonCopyable ) = delete;
};

class Cdm_clearkey_IMFContentDecryptionModuleAccess final :
    public Microsoft::WRL::RuntimeClass<RuntimeClassFlags<ClassicCom>,
    IMFContentDecryptionModuleAccess,
    FtmBase>
{
public:
    Cdm_clearkey_IMFContentDecryptionModuleAccess();
    ~Cdm_clearkey_IMFContentDecryptionModuleAccess();

    HRESULT RuntimeClassInitialize( _In_ IPropertyStore *pIPropertyStoreConfiguration );

    // IMFContentDecryptionModuleAccess
    STDMETHODIMP CreateContentDecryptionModule(
        _In_ IPropertyStore *contentDecryptionModuleProperties,
        _COM_Outptr_ IMFContentDecryptionModule **contentDecryptionModule
    );

    STDMETHODIMP GetConfiguration(
        _COM_Outptr_  IPropertyStore **configuration
    );

    STDMETHODIMP GetKeySystem(
        _Outptr_ LPWSTR *keySystem
    );

private:
    CriticalSection m_Lock;
    ComPtr<IPropertyStore> m_spIPropertyStoreConfiguration;

    Cdm_clearkey_IMFContentDecryptionModuleAccess( const Cdm_clearkey_IMFContentDecryptionModuleAccess &nonCopyable ) = delete;
    Cdm_clearkey_IMFContentDecryptionModuleAccess& operator=( const Cdm_clearkey_IMFContentDecryptionModuleAccess &nonCopyable ) = delete;
};

class Cdm_clearkey_IMFContentDecryptionModule final :
    public RuntimeClass<RuntimeClassFlags<ClassicCom>,
    IMFContentDecryptionModule,
    IMFGetService,
    IMFAttributes,
    FtmBase>
{
public:
    Cdm_clearkey_IMFContentDecryptionModule();
    ~Cdm_clearkey_IMFContentDecryptionModule();

    HRESULT RuntimeClassInitialize(
        _In_ IPropertyStore *pIPropertyStoreCDM3,
        _In_ IPropertyStore *pIPropertyStoreConfiguration
    );

    // IMFContentDecryptionModule
    STDMETHODIMP SetContentEnabler(
        _In_ IMFContentEnabler *contentEnabler,
        _In_ IMFAsyncResult *result
    );

    STDMETHODIMP GetSuspendNotify(
        _COM_Outptr_ IMFCdmSuspendNotify **notify
    );

    STDMETHODIMP SetPMPHostApp(
        _In_ IMFPMPHostApp *pmpHostApp
    );

    STDMETHODIMP CreateSession(
        _In_ MF_MEDIAKEYSESSION_TYPE sessionType,
        _In_ IMFContentDecryptionModuleSessionCallbacks *callbacks,
        _COM_Outptr_ IMFContentDecryptionModuleSession **session
    );

    STDMETHODIMP SetServerCertificate(
        _In_reads_bytes_opt_(certificateSize) const BYTE *certificate,
        _In_ DWORD certificateSize
    );

    STDMETHODIMP CreateTrustedInput(
        _In_reads_bytes_opt_(contentInitDataSize) const BYTE *contentInitData,
        _In_ DWORD contentInitDataSize,
        _COM_Outptr_ IMFTrustedInput **trustedInput
    );

    STDMETHODIMP GetProtectionSystemIds(
        _Outptr_result_buffer_all_( *count ) GUID **systemIds,
        _Out_ DWORD *count
    );

    // IMFGetService
    STDMETHODIMP GetService( __RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID *ppvObject );

    // IMFAttributes
    CDM_clearkey_IMPLEMENT_IMFATTRIBUTES( m_spIMFAttributes );

private:
    CriticalSection m_Lock;
    ComPtr<IPropertyStore> m_spIPropertyStoreCDM3;
    ComPtr<IPropertyStore> m_spIPropertyStoreConfiguration;
    ComPtr<IMediaProtectionPMPServer> m_spIMediaProtectionPMPServer;
    ComPtr<IMFPMPHostApp> m_spIMFPMPHostApp;
    ComPtr<IMFAttributes> m_spIMFAttributes;
    BYTE m_cITAs = 0;
    BYTE m_cCEs = 0;

    Cdm_clearkey_IMFContentDecryptionModule( const Cdm_clearkey_IMFContentDecryptionModule &nonCopyable ) = delete;
    Cdm_clearkey_IMFContentDecryptionModule& operator=( const Cdm_clearkey_IMFContentDecryptionModule &nonCopyable ) = delete;
};

class Cdm_clearkey_IMFContentDecryptionModuleSession final :
    public RuntimeClass<RuntimeClassFlags<ClassicCom>,
    IMFContentDecryptionModuleSession,
    IMFAttributes,
    FtmBase>
{
public:
    Cdm_clearkey_IMFContentDecryptionModuleSession();
    ~Cdm_clearkey_IMFContentDecryptionModuleSession();

    HRESULT RuntimeClassInitialize(
        _In_ IMFAttributes *pIMFAttributesFromCDM,
        _In_ MF_MEDIAKEYSESSION_TYPE eSessionType,
        _In_ IMFContentDecryptionModuleSessionCallbacks *pIMFContentDecryptionModuleSessionCallbacks
    );

    // IMFAttributes
    CDM_clearkey_IMPLEMENT_IMFATTRIBUTES( m_spIMFAttributes );

    // IMFContentDecryptionModuleSession
    STDMETHODIMP GetSessionId(
        _Out_ LPWSTR *sessionId
    );

    STDMETHODIMP GetExpiration(
        _Out_ double *expiration
    );

    STDMETHODIMP GetKeyStatuses(
        _Outptr_result_buffer_(*numKeyStatuses) MFMediaKeyStatus **keyStatuses,
        _Out_ UINT *numKeyStatuses
    );

    STDMETHODIMP Load(
        _In_ LPCWSTR sessionId,
        _Out_ BOOL *loaded
    );

    STDMETHODIMP GenerateRequest(
        _In_ LPCWSTR initDataType,
        _In_reads_bytes_(initDataSize) const BYTE *initData,
        _In_ DWORD initDataSize
    );

    STDMETHODIMP Update(
        _In_reads_bytes_(responseSize) const BYTE *response,
        _In_ DWORD responseSize
    );

    STDMETHODIMP Close();

    STDMETHODIMP Remove();

private:
    BOOL IsCallable();
    HRESULT SetIsCallable();
    HRESULT DestroyKeys( _In_ MF_MEDIAKEY_STATUS eStatus, _Inout_ BYTE *pbMessage, _Inout_ DWORD *pibMessage );
    HRESULT LoadStorePaths();

    CriticalSection m_Lock;
    int m_sessionId = CLEARKEY_INVALID_SESSION_ID;
    BOOL m_fUninitialized = TRUE;   // Set to FALSE by Load and GenerateRequest
    BOOL m_fClosed = FALSE;         // Set to TRUE by Close
    MF_MEDIAKEYSESSION_TYPE m_eSessionType = MF_MEDIAKEYSESSION_TYPE_TEMPORARY;
    HANDLE m_hPersistentSessionFile = INVALID_HANDLE_VALUE;
    double m_dblExpiration = NaN.f;
    ComPtr<IMFContentDecryptionModuleSessionCallbacks> m_spIMFContentDecryptionModuleSessionCallbacks;
    ComPtr<IMFAttributes> m_spIMFAttributes;
    WCHAR *m_pwszDefaultStorePath = nullptr;
    WCHAR *m_pwszInPrivateStorePath = nullptr;

    Cdm_clearkey_IMFContentDecryptionModuleSession( const Cdm_clearkey_IMFContentDecryptionModuleSession &nonCopyable ) = delete;
    Cdm_clearkey_IMFContentDecryptionModuleSession& operator=( const Cdm_clearkey_IMFContentDecryptionModuleSession &nonCopyable ) = delete;
};

class Cdm_clearkey_IMFCdmSuspendNotify final :
    public RuntimeClass<RuntimeClassFlags<ClassicCom>, ::IMFCdmSuspendNotify, FtmBase>
{
public:
    Cdm_clearkey_IMFCdmSuspendNotify();
    ~Cdm_clearkey_IMFCdmSuspendNotify();
    HRESULT RuntimeClassInitialize( _In_ IMFContentDecryptionModule *pIMFContentDecryptionModule );

    // IMFCdmSuspendNotify
    STDMETHODIMP Begin();
    STDMETHODIMP End();

private:
    CriticalSection m_Lock;

    Cdm_clearkey_IMFCdmSuspendNotify( const Cdm_clearkey_IMFCdmSuspendNotify &nonCopyable ) = delete;
    Cdm_clearkey_IMFCdmSuspendNotify& operator=( const Cdm_clearkey_IMFCdmSuspendNotify &nonCopyable ) = delete;
};

