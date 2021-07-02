//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#pragma once

#include <windows.h>

//
// Verify that we are still running in the protected process every PEAUTH_STATE_CHECK_INTERVAL samples encountered during playback.
//
#define PEAUTH_STATE_CHECK_INTERVAL 5000

enum CLEARKEY_MFT_STATE
{
    CLEARKEY_MFT_STATE_STOPPED                                  = 0,    // We're stopped
    CLEARKEY_MFT_STATE_FEEDINPUT                                = 1,    // We're running - MFT needs more input
    CLEARKEY_MFT_STATE_GETOUTPUT                                = 2,    // We're running - MFT has output to provide
    CLEARKEY_MFT_STATE_SHUTDOWN                                 = 3,    // We're shutdown
    CLEARKEY_MFT_STATE_WAITING_FOR_POLICY_NOTIFICATION          = 4,    // We're waiting - MFT is waiting for policy notification
};

#define CLEARKEY_MFT_STATE_IS_A_WAITING_STATE( _state_ ) ( ( _state_ ) == CLEARKEY_MFT_STATE_WAITING_FOR_POLICY_NOTIFICATION )

enum CLEARKEY_MFT_DRAIN_STATE
{
    CLEARKEY_MFT_DRAIN_STATE_NONE     = 0,     // We're not draining
    CLEARKEY_MFT_DRAIN_STATE_PREDRAIN = 1,     // We're feeding the MFT all remaining input data
    CLEARKEY_MFT_DRAIN_STATE_DRAINING = 2,     // We're draining the MFT
};

enum CLEARKEY_MFT_PROCESSING_LOOP_STATE
{
    CLEARKEY_MFT_PROCESSING_LOOP_STATE_NOT_WAITING                              = 0,
    CLEARKEY_MFT_PROCESSING_LOOP_STATE_WAITING_FOR_START                        = 1,
    CLEARKEY_MFT_PROCESSING_LOOP_STATE_OTHER_ITEMS_IN_QUEUE_START_INTERNAL      = 2,
    CLEARKEY_MFT_PROCESSING_LOOP_STATE_RESOLVE_WAITING_STATE_100MS_RETRY        = 3,
    CLEARKEY_MFT_PROCESSING_LOOP_STATE_OTHER_ITEMS_IN_QUEUE_AFTER_STATE_CHANGED = 4,
    CLEARKEY_MFT_PROCESSING_LOOP_STATE_A_STATE_CHANGE                           = 5,
};

class CMFSampleData final
{
public:
    BOOL m_fEncrypted = FALSE;
    BOOL m_fIsCBCContent = FALSE;
    BOOL m_fSubSampleMappingSplit = FALSE;
    BYTE m_rgbIV[ AES_BLOCK_LEN ] = { 0 };
    PROPVARIANTWRAP         m_pvSubSampleMapping;       // VT_VECTOR | VT_UI4
    PROPVARIANTWRAP         m_propvarOffsets;           // VT_VECTOR | VT_UI4
    DWORD m_dwStripeEncryptedBlocks = 0;  // Number of encrypted BLOCKS in each stripe for CENS or CBCS content.  Zero if striping is not being used.
    DWORD m_dwStripeClearBlocks = 0;      // Number of clear BLOCKS in each stripe for CENS or CBCS content.  Zero if striping is not being used.
    CMFSampleData()
    {
    }
    ~CMFSampleData()
    {
    }

private:
    CMFSampleData( const CMFSampleData &nonCopyable ) = delete;
    CMFSampleData& operator=( const CMFSampleData &nonCopyable ) = delete;
};

class Cdm_clearkey_IMFTrustedInput final :
    public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, IUnused, CloakedIid<::IMFTrustedInput>, ChainInterfaces<::IPersistStream, ::IPersist>, IMFAttributes, FtmBase>
{
    InspectableClass( RuntimeClass_org_w3_clearkey_TrustedInput, BaseTrust );
public:
    Cdm_clearkey_IMFTrustedInput();
    ~Cdm_clearkey_IMFTrustedInput();
    HRESULT RuntimeClassInitialize();

    // IMFTrustedInput
    STDMETHODIMP GetInputTrustAuthority( _In_ DWORD streamId, _In_ REFIID riid, _COM_Outptr_ IUnknown **authority );

    // IPersistStream
    STDMETHODIMP GetClassID( _Out_ CLSID *pclsid );
    STDMETHODIMP GetSizeMax( _Out_ ULARGE_INTEGER *pcbSize );
    STDMETHODIMP IsDirty();
    STDMETHODIMP Load( _In_ IStream *pStream );
    STDMETHODIMP Save( _Inout_ IStream *pStm, _In_ BOOL fClearDirty );

    // IMFAttributes
    CDM_clearkey_IMPLEMENT_IMFATTRIBUTES( m_spIMFAttributes );

private:
    CriticalSection m_Lock;
    ComPtr<IMFAttributes> m_spIMFAttributes;

    Cdm_clearkey_IMFTrustedInput( const Cdm_clearkey_IMFTrustedInput &nonCopyable ) = delete;
    Cdm_clearkey_IMFTrustedInput& operator=( const Cdm_clearkey_IMFTrustedInput &nonCopyable ) = delete;
};

class Cdm_clearkey_IMFInputTrustAuthority final :
    public RuntimeClass<RuntimeClassFlags<ClassicCom>, ::IMFInputTrustAuthority, CloakedIid<::IMFAttributes>, CloakedIid<::IMFShutdown>, FtmBase>
{
public:
    Cdm_clearkey_IMFInputTrustAuthority();
    ~Cdm_clearkey_IMFInputTrustAuthority();
    HRESULT RuntimeClassInitialize( _In_ IMFAttributes *pIMFAttributesFromTI, _In_ UINT32 uiStreamId );

    // IMFInputTrustAuthority
    STDMETHODIMP GetDecrypter( _In_ REFIID riid, _COM_Outptr_ void **ppv );
    STDMETHODIMP RequestAccess( _In_ MFPOLICYMANAGER_ACTION eAction, _COM_Outptr_ IMFActivate **ppContentEnablerActivate );
    STDMETHODIMP GetPolicy( _In_ MFPOLICYMANAGER_ACTION eAction, _COM_Outptr_ IMFOutputPolicy ** ppPolicy );
    STDMETHODIMP BindAccess( _In_ MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS * pParams );
    STDMETHODIMP UpdateAccess( _In_ MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS *pParams );
    STDMETHODIMP Reset();

    // IMFAttributes
    CDM_clearkey_IMPLEMENT_IMFATTRIBUTES( m_spIMFAttributes );

    // IMFShutdown
    STDMETHODIMP GetShutdownStatus( _Out_ MFSHUTDOWN_STATUS *pStatus );
    STDMETHODIMP Shutdown();

private:
    void DisableCurrentDecryptersAndSetInsecureEnvironment();
    HRESULT VerifyPEAuth();

    CriticalSection m_Lock;
    BOOL m_fShutdown = FALSE;
    GUID m_guidKid = GUID_NULL;
    GUID m_guidLid = GUID_NULL;
    ComPtr<IMFAttributes> m_spIMFAttributes;
    ComPtr<IMFProtectedEnvironmentAccess> m_spCdm_clearkey_PEAuthHelper;

    Cdm_clearkey_IMFInputTrustAuthority( const Cdm_clearkey_IMFInputTrustAuthority &nonCopyable ) = delete;
    Cdm_clearkey_IMFInputTrustAuthority& operator=( const Cdm_clearkey_IMFInputTrustAuthority &nonCopyable ) = delete;
};

class Cdm_clearkey_IMFOutputPolicy final :
    public RuntimeClass<RuntimeClassFlags<ClassicCom>, ::IMFOutputPolicy, ::IMFAttributes, FtmBase>
{
public:
    Cdm_clearkey_IMFOutputPolicy();
    ~Cdm_clearkey_IMFOutputPolicy();
    HRESULT RuntimeClassInitialize( _In_ IMFAttributes *pIMFAttributesFromITA, _In_ MFPOLICYMANAGER_ACTION eAction, _In_ REFGUID guidKid, _In_ REFGUID guidLid );

    // IMFOutputPolicy
    STDMETHODIMP GenerateRequiredSchemas(
        _In_                                      DWORD           dwAttributes,
        _In_                                      GUID            guidOutputSubType,
        _In_count_( cProtectionSchemasSupported ) GUID           *rgGuidProtectionSchemasSupported,
        _In_                                      DWORD           cProtectionSchemasSupported,
        _COM_Outptr_                              IMFCollection **ppRequiredProtectionSchemas );
    STDMETHODIMP GetOriginatorID( _Out_ GUID *pguidOriginatorID );
    STDMETHODIMP GetMinimumGRLVersion( _Out_ DWORD *pMinimumGRLVersion );

    // IMFAttributes
    CDM_clearkey_IMPLEMENT_IMFATTRIBUTES( m_spIMFAttributes );

private:
    CriticalSection m_Lock;
    GUID m_guidKid = GUID_NULL;
    GUID m_guidLid = GUID_NULL;
    MFPOLICYMANAGER_ACTION m_eAction;
    ComPtr<IMFAttributes>  m_spIMFAttributes;

    Cdm_clearkey_IMFOutputPolicy( const Cdm_clearkey_IMFOutputPolicy &nonCopyable ) = delete;
    Cdm_clearkey_IMFOutputPolicy& operator=( const Cdm_clearkey_IMFOutputPolicy &nonCopyable ) = delete;
};

//
// The standard WinRT 'SyncLock' class does not support move semantics.  This class enables it.
//
class SyncLock2 final : public CriticalSection::SyncLock
{
public:
    SyncLock2( CriticalSection::SyncLock&& other ) noexcept
        : CriticalSection::SyncLock( static_cast<CriticalSection::SyncLock&&>( other ) )
    {
    }

    SyncLock2& operator=( SyncLock2&& other ) noexcept
    {
        if( this->sync_ != other.sync_ )
        {
            sync_ = other.sync_;
            other.sync_ = nullptr;
        }
        return *this;
    }

private:
    SyncLock2& operator=( SyncLock2& other );
};

class Cdm_clearkey_IMFTransform final :
    public RuntimeClass<RuntimeClassFlags<ClassicCom>, ::IMFTransform, CloakedIid<::IMFRealTimeClientEx>, CloakedIid<::IMFMediaEventGenerator>, CloakedIid<::IMFShutdown>, IPropertyStore, FtmBase>
{
public:
    Cdm_clearkey_IMFTransform();
    ~Cdm_clearkey_IMFTransform();

    HRESULT RuntimeClassInitialize( _In_ IMFAttributes *pIMFAttributesFromITA, _In_ IMFProtectedEnvironmentAccess *pCdm_clearkey_PEAuthHelper, _In_ REFGUID guidKid, _In_ REFGUID guidLid );

    // IMFTransform
    STDMETHODIMP GetStreamLimits( _Out_ DWORD *pdwInputMinimum, _Out_ DWORD *pdwInputMaximum, _Out_ DWORD *pdwOutputMinimum, _Out_ DWORD *pdwOutputMaximum );
    STDMETHODIMP GetStreamCount( _Out_ DWORD *pcInputStreams, _Out_ DWORD *pcOutputStreams );
    STDMETHODIMP GetStreamIDs( _In_ DWORD dwInputIDArraySize, _Out_ DWORD *pdwInputIDs, _In_ DWORD dwOutputIDArraySize, _Out_ DWORD *pdwOutputIDs );
    STDMETHODIMP GetInputStreamInfo( _In_ DWORD dwInputStreamID, _Out_ MFT_INPUT_STREAM_INFO *pStreamInfo );
    STDMETHODIMP GetOutputStreamInfo( _In_ DWORD dwOutputStreamID, _Out_ MFT_OUTPUT_STREAM_INFO *pStreamInfo );
    STDMETHODIMP GetAttributes( _COM_Outptr_ IMFAttributes **ppAttributes );
    STDMETHODIMP GetInputStreamAttributes( _In_ DWORD dwInputStreamID, _COM_Outptr_ IMFAttributes **ppAttributes );
    STDMETHODIMP GetOutputStreamAttributes( _In_ DWORD dwOutputStreamID, _COM_Outptr_ IMFAttributes **ppAttributes );
    STDMETHODIMP DeleteInputStream( _In_ DWORD dwStreamID );
    STDMETHODIMP AddInputStreams( _In_ DWORD cStreams, _In_count_( cStreams ) DWORD *rgdwStreamIDs );
    STDMETHODIMP GetInputAvailableType( _In_ DWORD dwInputStreamIndex, _In_ DWORD dwTypeIndex, _COM_Outptr_ IMFMediaType **ppType );
    STDMETHODIMP GetOutputAvailableType( _In_ DWORD dwOutputStreamIndex, _In_ DWORD dwTypeIndex, _COM_Outptr_ IMFMediaType **ppType );
    STDMETHODIMP SetInputType( _In_ DWORD dwInputStreamIndex, _In_ IMFMediaType *pType, _In_ DWORD dwFlags );
    STDMETHODIMP SetOutputType( _In_ DWORD dwOutputStreamIndex, _In_ IMFMediaType * pType, _In_ DWORD dwFlags );
    STDMETHODIMP GetInputCurrentType( _In_ DWORD dwInputStreamIndex, _COM_Outptr_ IMFMediaType **ppType );
    STDMETHODIMP GetOutputCurrentType( _In_ DWORD dwOutputStreamIndex, _COM_Outptr_ IMFMediaType **ppType );
    STDMETHODIMP GetInputStatus( _In_ DWORD dwInputStreamIndex, _Out_ DWORD *pdwFlags );
    STDMETHODIMP GetOutputStatus( _Out_ DWORD *pdwFlags );
    STDMETHODIMP SetOutputBounds( _In_ LONGLONG hnsLowerBound, _In_ LONGLONG hnsUpperBound );
    STDMETHODIMP ProcessEvent( _In_ DWORD dwInputStreamID, _In_ IMFMediaEvent *pEvent );
    STDMETHODIMP ProcessMessage( _In_ MFT_MESSAGE_TYPE eMessage, _In_ ULONG_PTR ulParam );
    STDMETHODIMP ProcessInput( _In_ DWORD dwInputStreamIndex, _In_ IMFSample *pSample, _In_ DWORD dwFlags );
    STDMETHODIMP ProcessOutput( _In_ DWORD dwFlags, _In_ DWORD cOutputBufferCount, _Inout_count_( cOutputBufferCount ) MFT_OUTPUT_DATA_BUFFER * pOutputSamples, _Out_ DWORD *pdwStatus );

    // IMFRealTimeClientEx
    STDMETHODIMP RegisterThreadsEx( _Out_ DWORD *pdwTaskIndex, _In_ _Null_terminated_ LPCWSTR pwszClassName, _In_ LONG lBasePriority );
    STDMETHODIMP UnregisterThreads( void );
    STDMETHODIMP SetWorkQueueEx( _In_ DWORD dwMultithreadedWorkQueueId, _In_ LONG lWorkItemBasePriority );

    // IMFMediaEventGenerator
    DEFINE_EVENT_GENERATOR;

    // IMFShutdown
    STDMETHODIMP Shutdown( void );
    STDMETHODIMP GetShutdownStatus( _Out_ MFSHUTDOWN_STATUS *pStatus );

    // IPropertyStore
    STDMETHODIMP Commit();
    STDMETHODIMP GetAt( _In_ DWORD iProp, _Out_ PROPERTYKEY *pkey );
    STDMETHODIMP GetCount( _Out_ DWORD *pcProps );
    STDMETHODIMP GetValue( _In_ REFPROPERTYKEY key, _Out_ PROPVARIANT *pv );
    STDMETHODIMP SetValue( _In_ REFPROPERTYKEY key, _In_ REFPROPVARIANT var );

private:

    HRESULT Enable( void );
    void Disable( void );
    BOOL IsWaitingForPolicyNotification();
    BOOL DidInputQueueChange( _In_ const IMFSample *pfExpectedFirstSample );
    BOOL ShouldIgnoreSampleAfterLongRunningOperation( _In_ const IMFSample *pfExpectedFirstSample );

    HRESULT LongRunning_DecryptSample( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock );

    void NotifyMediaKeyStatus();

    HRESULT DoProcessSample(
        _In_            SyncLock2&        stateLock,
        _In_            SyncLock2&        outputLock,
        _COM_Outptr_    IMFSample       **ppOutputSample,
        _COM_Outptr_    IMFCollection   **ppOutputCollection,
        _Out_           DWORD            *pdwSampleStatus,
        _Out_           DWORD            *pdwOutputStatus );

    HRESULT OnInputTypeChanged( void );

    HRESULT OnDrain( void );
    HRESULT OnFlush( void );

    HRESULT InternalGetInputStatus( _Out_ DWORD *pdwFlags );
    HRESULT InternalGetOutputStatus( _Out_ DWORD *pdwFlags );

    HRESULT OnSetD3DManager( _In_ IUnknown *pUnk );
    HRESULT OnMediaEventMessage( _In_ ULONG_PTR ulParam );

    HRESULT InternalProcessInput( _In_ IMFSample *pSample, _In_ DWORD dwFlags );

    HRESULT OnProcessEvent( _In_ DWORD dwInputStreamID, _In_ IMFMediaEvent *pEvent );

    void SetEnabledState( _In_ HRESULT hr );
    HRESULT GetEnabledState();

    HRESULT GetQueuedSampleOrEvents(
        _COM_Outptr_    IMFSample       **ppOutputSample,
        _COM_Outptr_    IMFCollection   **ppOutputCollection,
        _Out_           DWORD            *pdwSampleStatus,
        _Out_           DWORD            *pdwOutputStatus );

    void LongRunningOperationStartUnlockOutputLock( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock, _In_ BOOL fDuringProcessSample, _Out_ BOOL *pfOutputLockIsUnlocked );
    HRESULT LongRunningOperationEndRelockOutputLock( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock, _Inout_ BOOL *pfOutputLockIsUnlocked );
    HRESULT SignalStateChanged();
    HRESULT StartInternal( _In_ BOOL fStartOfStream );
    void SetState( _In_ CLEARKEY_MFT_STATE eState );
    HRESULT SetWorkQueueValue( _In_ DWORD dwMultithreadedWorkQueueId, _In_ LONG  lWorkItemBasePriority );
    CLEARKEY_MFT_STATE GetState();
    HRESULT ProcessMessageStateLockHeld( _In_ SyncLock2& stateLock, _In_ MFT_MESSAGE_TYPE eMessage, _In_ ULONG_PTR ulParam );
    void SetLongRunning( _In_ BOOL fLongRunning );
    HRESULT StopInternal();
    HRESULT FlushInternal( _In_ SyncLock2& stateLock );
    void SetDrainState( _In_ CLEARKEY_MFT_DRAIN_STATE eDrainState );
    CLEARKEY_MFT_DRAIN_STATE GetDrainState();
    BOOL IsDraining();
    HRESULT FinishDrain( _In_ BOOL fSendEvent );
    void SetProcessingLoopState( _In_ CLEARKEY_MFT_PROCESSING_LOOP_STATE eProcessingLoopState );
    HRESULT CheckState();
    HRESULT CheckInputState();
    HRESULT PropagateOutputTypeChange();
    void FlushOutputEventQueue();
    void FlushOutputSampleQueue( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock, _In_ BOOL fKeepEvents );
    DWORD GetOutputDataBufferQueueLength();
    BOOL IsOutputDataBufferQueueEmpty();
    void WaitForPendingOutputs( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock );
    HRESULT AddToOutputDataBufferQueue( _In_ MFT_OUTPUT_DATA_BUFFER *pOutputBuffer );
    HRESULT RemoveFromOutputDataBufferQueue( _In_ SyncLock2& stateLock, _In_ SyncLock2& outputLock, _Out_ MFT_OUTPUT_DATA_BUFFER **ppOutputBuffer );
    HRESULT GetOutput( _In_ SyncLock2& stateLock );
    HRESULT QueueInputSampleOrEvent( _In_ IUnknown* pSampleOrEvent );
    HRESULT FlushInputQueue( _In_ SyncLock2& stateLock, _Out_opt_ BOOL* pfFormatChange = nullptr );
    BOOL GetNextInputItem( _Out_ IUnknown** ppInputItem, _Out_ DWORD *pcItemsRemaining );
    DWORD GetWorkQueueValue();
    void ProcessingLoop( _In_ IMFAsyncResult *pResult );
    METHOD_ASYNC_CALLBACK( ProcessingLoop, Cdm_clearkey_IMFTransform, IMFMediaEventGenerator, GetWorkQueueValue );
    HRESULT ProcessEventInternal( _In_ IMFMediaEvent* pEvent, _In_ BOOL fFromProcessEventCall, _Inout_opt_ BOOL *pfPropagateEvent );
    HRESULT FeedInput( _In_ SyncLock2& stateLock );
    HRESULT AddPendingEventsToOutputSampleQueue();
    HRESULT RequestInputForStream();
    HRESULT RequestInput();
    DWORD GetSampleCount();
    HRESULT SetInputTypeLockIsHeld( _In_ SyncLock2& stateLock, _In_ DWORD dwInputStreamIndex, _In_ IMFMediaType *pType, _In_ DWORD dwFlags );
    HRESULT SetOutputTypeLockIsHeld( _In_ SyncLock2& stateLock, _In_ DWORD dwOutputStreamIndex, _In_ IMFMediaType *pType, _In_ DWORD dwFlags );
    HRESULT VerifyPEAuthStateTwicePerSample();

private:
    //
    // In order to optimize for async behavior, there are three locks in this class.
    //
    // A. m_StateLock is general-purpose and protects most member variables.
    // B. m_InputLock is used only when acting on input-specific member variables.
    // C. m_OutputLock is used only when acting on input-specific member variables.
    //
    // All other member variables are sorted into sections tagged with "ProtectedBy:"
    // indicating which lock (if any) protects them from concurrent access.
    //
    // In order to avoid deadlocks:
    // 1. When both m_StateLock and m_InputLock are acquired, m_StateLock MUST be acquired before m_InputLock.
    // 2. When both m_StateLock and m_OutputLock are acquired, m_StateLock MUST be acquired before m_OutputLock.
    // 3. When both m_InputLock and m_OutputLock are acquired, m_InputLock MUST be acquired before m_OutputLock.
    // 4. Certain APIs MUST acquire locks exactly once.  Specifically, when DoProcessSample is called
    //    (and during its called functions), m_StateLock and m_OutputLock are held exactly once
    //    and a second hold is never acquired during those functions.
    //    That codepath expects to be able to UNLOCK them temporarily and then RELOCK them.
    //
    CriticalSection m_StateLock;
    CriticalSection m_InputLock;
    CriticalSection m_OutputLock;

    //
    // ProtectedBy: m_StateLock
    //
    DWORD                                   m_dwNeedInputCount                  = 0;               // count of oustanding "need input" events
    BOOL                                    m_fLoopRunning                      = FALSE;
    CLEARKEY_MFT_STATE                      m_eState                            = CLEARKEY_MFT_STATE_STOPPED;
    CLEARKEY_MFT_DRAIN_STATE                m_eDrainState                       = CLEARKEY_MFT_DRAIN_STATE_NONE;
    CLEARKEY_MFT_PROCESSING_LOOP_STATE      m_eProcessingLoopState              = CLEARKEY_MFT_PROCESSING_LOOP_STATE_WAITING_FOR_START;
    BOOL                                    m_fWaitingForStartOfStream          = TRUE;         // We're not requesting any input as we're waiting for "start of stream" message
    HRESULT                                 m_hrError                           = S_OK;         // Error during internal async processing loop, propagated via a number of public APIs
    HRESULT                                 m_hrEnabled                         = MF_E_INVALIDREQUEST;
    BOOL                                    m_fEndOfStreamReached               = FALSE;
    BOOL                                    m_fInputSampleQueueDraining         = FALSE;
    BOOL                                    m_fIsAudio                          = FALSE;
    BOOL                                    m_fIsVideo                          = FALSE;
    GUID                                    m_idMFT                             = GUID_NULL;
    DWORD                                   m_cSamplesSinceProcessOutputCheck   = 0;
    DWORD                                   m_cSamplesSinceProcessSampleCheck   = 0;
    GUID                                    m_guidKid                           = GUID_NULL;
    GUID                                    m_guidLid                           = GUID_NULL;
    BOOL                                    m_fLongRunning                      = FALSE;
    DWORD                                   m_dwWorkQueueID                     = MFASYNC_CALLBACK_QUEUE_MULTITHREADED;       // If this value is "multithreaded", then we have NOT locked the queue.  Otherwise, we HAVE locked it.
    LONG                                    m_lWorkItemPriority                 = 0;
    DWORD                                   m_cSamplesSinceLastPMPCheck         = PEAUTH_STATE_CHECK_INTERVAL;   // Check immediately, then every PEAUTH_STATE_CHECK_INTERVAL samples afterwards
    CTPtrList<IMFSample>                    m_InputSampleQueue;                 // Input samples, originally in m_InputQueue but moved here during async processing so that m_InputLock has to be held for only a short time
    CTPtrList<IMFSample>                    m_OutputSampleQueue;                // Output samples ready to be returned (either originally in the clear or already decrypted)
    ComPtr<IMFAttributes>                   m_spIMFAttributes;
    ComPtr<IMFMediaType>                    m_spPendingInputMediaType;
    ComPtr<IMFMediaType>                    m_spOutputMediaType;
    ComPtr<IMFMediaType>                    m_spAvailableOutputMediaType;
    ComPtr<IPropertyStore>                  m_spIPropertyStore;
    CTDWORDArray                            m_rgdwPolicyIds;
    ComPtr<IMFDXGIDeviceManager>            m_spDXGIDeviceMgr;
    CTUnkArray<IMFOutputPolicy>             m_rgPendingPolicyChangeObjects;
    ComPtr<IMFProtectedEnvironmentAccess>   m_spCdm_clearkey_PEAuthHelper;

    //
    // ProtectedBy: m_InputLock
    //
    CTPtrList<IUnknown>     m_InputQueue;           // Input Samples and Events we haven't processed yet - we add to this list as soon as we see a new one
    ComPtr<IMFMediaType>    m_spInputMediaType;

    //
    // ProtectedBy: m_OutputLock
    //
    CTPtrList<IMFMediaEvent>          m_OutputEventQueue;               // "in-flight" events that we're propagating
    CTPtrList<MFT_OUTPUT_DATA_BUFFER> m_OutputDataBufferQueue;          // outgoing samples, media type changes, events, etc
    BOOL                              m_fOutputFormatChange = FALSE;
    DWORD                             m_cPendingOutputs     = 0;        // Number of pending outputs which will be added to the queue shortly.

    //
    // ProtectedBy: No lock is held for these variables.  They are a synchronization primitives that are only initialized/destroyed once.
    //
    HANDLE  m_hStateChanged       = nullptr;
    HANDLE  m_hNotProcessingEvent = nullptr;     // Set when we do NOT have any pending output.  We wait on this when we DO have pending output.
    HANDLE  m_hWaitStateEvent     = nullptr;

    //
    // ProtectedBy: InterlockedExchange is used when this value is written.
    //
    static DWORD    s_dwNextPolicyId;       // initialized to zero in .cpp file

    //
    // ProtectedBy: No lock needs to be held for constants
    //
    static const DWORD     c_dwMaxInputSamples = 5;     // maximum # of input samples that we will queue
    static const DWORD     c_dwMaxOutputSamples = 5;    // maximum # of output samples that we will queue
    static const BOOL      c_fYesDuringProcessSample = TRUE;
    static const BOOL      c_fNotDuringProcessSample  = FALSE;
    static const BOOL      c_fYesLongRunning = TRUE;
    static const BOOL      c_fNotLongRunning = FALSE;
    static const BOOL      c_fYesSendEvent = TRUE;
    static const BOOL      c_fDoNotSendEvent = FALSE;
    static const BOOL      c_fYesKeepEvents = TRUE;
    static const BOOL      c_fDoNotKeepEvents = FALSE;
    static const BOOL      c_fYesFromProcessEvent = TRUE;
    static const BOOL      c_fNotFromProcessEvent = FALSE;
    static const BOOL      c_fYesStartOfStream = TRUE;
    static const BOOL      c_fNotStartOfStream = FALSE;
    static const LONGLONG  c_hnsInputMaxLatency = 0;
    static const DWORD     c_dwInputStreamFlags = 0;
    static const DWORD     c_dwInputSampleSize = 0;
    static const DWORD     c_dwInputStreamLookahead = 0;
    static const DWORD     c_dwInputSampleAlignment = 0;

    Cdm_clearkey_IMFTransform( const Cdm_clearkey_IMFTransform &nonCopyable ) = delete;
    Cdm_clearkey_IMFTransform& operator=( const Cdm_clearkey_IMFTransform &nonCopyable ) = delete;
};

class Cdm_clearkey_IMFOutputSchema final :
    public RuntimeClass<RuntimeClassFlags<ClassicCom>, ::IMFOutputSchema, CloakedIid<::IMFAttributes>, FtmBase>
{
public:
    Cdm_clearkey_IMFOutputSchema();
    ~Cdm_clearkey_IMFOutputSchema();

    HRESULT RuntimeClassInitialize(
        _In_         REFGUID           guidOriginatorID,
        _In_         REFGUID           guidSchemaType,
        _In_         DWORD             dwConfigurationData );

    // IMFOutputSchema
    STDMETHODIMP GetSchemaType( _Out_ GUID *pguidSchemaType );
    STDMETHODIMP GetConfigurationData( _Out_ DWORD *pdwVal );
    STDMETHODIMP GetOriginatorID( _Out_ GUID *pguidOriginatorID );

    // IMFAttributes
    CDM_clearkey_IMPLEMENT_IMFATTRIBUTES( m_spIMFAttributes );

private:
    //
    // All functions except creation are read-only on all member variables,
    // so there is no need to have a synchronization lock in this class.
    //
    GUID    m_guidOriginatorID = GUID_NULL;
    GUID    m_guidSchemaType = GUID_NULL;
    DWORD   m_dwConfigurationData = 0;
    ComPtr<IMFAttributes> m_spIMFAttributes;

    Cdm_clearkey_IMFOutputSchema( const Cdm_clearkey_IMFOutputSchema &nonCopyable ) = delete;
    Cdm_clearkey_IMFOutputSchema& operator=( const Cdm_clearkey_IMFOutputSchema &nonCopyable ) = delete;
};


