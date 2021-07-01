//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#pragma once

#include <windows.h>

#define METHOD_ASYNC_CALLBACK(_Callback_, _Parent_, _IFace_, _GetQueue_)                                                                                \
class _Callback_##AsyncCallback;                                                                                                                        \
friend class _Callback_##AsyncCallback;                                                                                                                 \
class _Callback_##AsyncCallback : public IMFAsyncCallback                                                                                               \
{                                                                                                                                                       \
public:                                                                                                                                                 \
    _Callback_##AsyncCallback()                                                                                                                         \
    {                                                                                                                                                   \
        Module<InProc>::GetModule().IncrementObjectCount();                                                                                             \
    }                                                                                                                                                   \
    ~_Callback_##AsyncCallback()                                                                                                                        \
    {                                                                                                                                                   \
        Module<InProc>::GetModule().DecrementObjectCount();                                                                                             \
    }                                                                                                                                                   \
    __override STDMETHOD_( ULONG, AddRef )()                                                                                                            \
    {                                                                                                                                                   \
        _IFace_ *pThis = static_cast<_IFace_*>(reinterpret_cast<_Parent_*>(reinterpret_cast<BYTE*>(this) - offsetof(_Parent_, m_x##_Callback_)));       \
        return pThis->AddRef();                                                                                                                         \
    }                                                                                                                                                   \
    __override STDMETHOD_( ULONG, Release )()                                                                                                           \
    {                                                                                                                                                   \
        _IFace_ * pThis = (static_cast<_IFace_*>(reinterpret_cast<_Parent_*>(reinterpret_cast<BYTE*>(this) - offsetof(_Parent_, m_x##_Callback_))));    \
        return pThis->Release();                                                                                                                        \
    }                                                                                                                                                   \
    __override STDMETHOD( QueryInterface )( REFIID riid, void **ppvObject )                                                                             \
    {                                                                                                                                                   \
        *ppvObject = nullptr;                                                                                                                           \
        return E_NOINTERFACE;                                                                                                                           \
    }                                                                                                                                                   \
    __override STDMETHOD( GetParameters )(                                                                                                              \
        DWORD *pdwFlags,                                                                                                                                \
        DWORD *pdwQueue)                                                                                                                                \
    {                                                                                                                                                   \
        _Parent_ * pThis = reinterpret_cast<_Parent_*>(reinterpret_cast<BYTE*>(this) - offsetof(_Parent_, m_x##_Callback_));                            \
        *pdwFlags = 0;                                                                                                                                  \
        *pdwQueue = pThis->_GetQueue_();                                                                                                                \
        return S_OK;                                                                                                                                    \
    }                                                                                                                                                   \
    __override STDMETHOD( Invoke )( IMFAsyncResult * pResult )                                                                                          \
    {                                                                                                                                                   \
        _Parent_ * pThis = reinterpret_cast<_Parent_*>(reinterpret_cast<BYTE*>(this) - offsetof(_Parent_, m_x##_Callback_));                            \
        pThis->_Callback_( pResult );                                                                                                                   \
        return S_OK;                                                                                                                                    \
    }                                                                                                                                                   \
} m_x##_Callback_;

class CEventGenerator
{
public:
    CEventGenerator()
    {
    }

    virtual ~CEventGenerator()
    {
        if( m_spIMFMediaEventQueue )
        {
            m_spIMFMediaEventQueue->Shutdown();
        }
    }

    HRESULT Init( _Out_opt_ IMFMediaEventQueue** ppIMFMediaEventQueue = nullptr )
    {
        HRESULT hr = S_OK;
        CriticalSection::SyncLock lock = m_Lock.Lock();

        if( m_fShutdown )
        {
            // If shutdown has previously been called then all event operations should fail
            // This works here because they all call Init before anything else.
            IF_FAILED_GOTO( MF_E_SHUTDOWN );
        }

        if( !m_fInitialized )
        {
            m_spIMFMediaEventQueue = nullptr;
            hr = ::MFCreateEventQueue( &m_spIMFMediaEventQueue );
            if( SUCCEEDED( hr ) )
            {
                m_fInitialized = TRUE;
            }
            else
            {
                m_spIMFMediaEventQueue = nullptr;
                IF_FAILED_GOTO( hr );
            }
        }

        if( ppIMFMediaEventQueue != nullptr )
        {
            ComPtr<IMFMediaEventQueue> speventQueue = m_spIMFMediaEventQueue;
            *ppIMFMediaEventQueue = speventQueue.Detach();
        }

    done:
        return hr;
    }


public:
    STDMETHODIMP Shutdown()
    {
        HRESULT  hr = S_OK;
        CriticalSection::SyncLock lock = m_Lock.Lock();

        if( m_fInitialized )
        {
            IF_FAILED_GOTO( m_spIMFMediaEventQueue->Shutdown() );
            m_spIMFMediaEventQueue = nullptr;
            m_fInitialized = FALSE;
            m_fShutdown    = TRUE;
        }

    done:
        return hr;
    }

    // IMFMediaEventGenerator
    HRESULT STDMETHODCALLTYPE GetEvent(
        /* IN */ DWORD dwFlags,
        /* OUT */ IMFMediaEvent **ppEvent )
    {
        HRESULT         hr = S_OK;
        ComPtr<IMFMediaEventQueue> speventQueue;

        IF_FAILED_GOTO( Init( &speventQueue ) );     // Take a reference to the IMFMediaEventQueue in case Shutdown is called after Init but before dereference.
        IF_FAILED_GOTO( speventQueue->GetEvent( dwFlags, ppEvent ) );

    done:
        return hr;
    }

    HRESULT STDMETHODCALLTYPE BeginGetEvent(
        /* IN */ IMFAsyncCallback *pCallback,
        /* IN */ IUnknown *punkState )
    {
        HRESULT         hr = S_OK;
        ComPtr<IMFMediaEventQueue> speventQueue;

        IF_FAILED_GOTO( Init( &speventQueue ) );     // Take a reference to the IMFMediaEventQueue in case Shutdown is called after Init but before dereference.
        IF_FAILED_GOTO( speventQueue->BeginGetEvent( pCallback, punkState ) );

    done:
        return hr;
    }

    HRESULT STDMETHODCALLTYPE EndGetEvent(
        /* IN */ IMFAsyncResult *pResult,
        /* OUT */ IMFMediaEvent **ppEvent )
    {
        HRESULT         hr = S_OK;
        ComPtr<IMFMediaEventQueue> speventQueue;

        IF_FAILED_GOTO( Init( &speventQueue ) );     // Take a reference to the IMFMediaEventQueue in case Shutdown is called after Init but before dereference.
        IF_FAILED_GOTO( speventQueue->EndGetEvent( pResult, ppEvent ) );

    done:
        return hr;
    }

    HRESULT STDMETHODCALLTYPE QueueEvent(
        /* IN */ MediaEventType met,
        /* IN */ REFGUID guidExtendedType,
        /* IN */ HRESULT hrStatus,
        /* IN */ const PROPVARIANT * pvValue,
        /* IN */ IUnknown * punkState )
    {
        HRESULT         hr = S_OK;
        ComPtr<IMFMediaEvent> spEvent;
        ComPtr<IMFMediaEventQueue> speventQueue;

        IF_FAILED_GOTO( Init( &speventQueue ) );     // Take a reference to the IMFMediaEventQueue in case Shutdown is called after Init but before dereference.
        IF_FAILED_GOTO( ::MFCreateMediaEvent( met, guidExtendedType, hrStatus, pvValue, &spEvent ) );
        if( punkState != nullptr )
        {
            IF_FAILED_GOTO( spEvent->SetUnknown( CLEARKEY_GUID_ATTRIBUTE_EVENT_GENERATOR_STATE, punkState ) );
        }
        IF_FAILED_GOTO( speventQueue->QueueEvent( spEvent.Get() ) );

    done:
        return hr;
    }

    HRESULT QueueEvent( IMFMediaEvent *pEvent )
    {
        HRESULT hr = S_OK;
        ComPtr<IMFMediaEventQueue> speventQueue;

        IF_FAILED_GOTO( Init( &speventQueue ) );     // Take a reference to the IMFMediaEventQueue in case Shutdown is called after Init but before dereference.
        IF_FAILED_GOTO( speventQueue->QueueEvent( pEvent ) );

    done:
        return hr;
    }

    HRESULT QueueEventWithIUnknown(
        MediaEventType met,
        REFGUID        guidExtendedType,
        HRESULT        hrStatus,
        IUnknown      *pIUnknown )
    {
        HRESULT hr = S_OK;
        PROPVARIANTWRAP propvar;

        IF_FAILED_GOTO( Init() );
        if( nullptr != pIUnknown )
        {
            propvar.vt = VT_UNKNOWN;
            propvar.punkVal = pIUnknown;
            pIUnknown->AddRef();
            IF_FAILED_GOTO( QueueEvent( met, guidExtendedType, hrStatus, &propvar, nullptr ) );
        }
        else
        {
            IF_FAILED_GOTO( QueueEvent( met, guidExtendedType, hrStatus, nullptr, nullptr ) );
        }

    done:
        return hr;
    }

private:
    CriticalSection m_Lock;
    BOOL m_fInitialized = FALSE;
    BOOL m_fShutdown = FALSE;
    ComPtr<IMFMediaEventQueue> m_spIMFMediaEventQueue;
};

//
//  Use the following macros to define all the IMSPREventGenerator methods for your class
//  Make sure to use the helper macros as appropriate as well...
//  INIT_EVENT_GENERATOR
//  SHUTDOWN_EVENT_GENERATOR

#define DEFINE_EVENT_GENERATOR                                                  \
private:                                                                        \
  CEventGenerator m_eventGenerator;                                             \
public:                                                                         \
__override STDMETHODIMP GetEvent( DWORD dwFlags, IMFMediaEvent **ppEvent)       \
{                                                                               \
                                                                                \
    return m_eventGenerator.GetEvent( dwFlags, ppEvent );                       \
}                                                                               \
__override STDMETHODIMP BeginGetEvent(IMFAsyncCallback *pCallback,              \
                                      IUnknown         *punkState)              \
{                                                                               \
    return m_eventGenerator.BeginGetEvent( pCallback,punkState );               \
}                                                                               \
__override STDMETHODIMP EndGetEvent(IMFAsyncResult  *pResult,                   \
                                    IMFMediaEvent  **ppEvent)                   \
{                                                                               \
    return m_eventGenerator.EndGetEvent( pResult,ppEvent );                     \
}                                                                               \
__override STDMETHODIMP QueueEvent(                                             \
        /* [in] */ MediaEventType     met,                                      \
        /* [in] */ REFGUID            guidExtendedType,                         \
        /* [in] */ HRESULT            hrStatus,                                 \
        /* [in] */ const PROPVARIANT *pvValue)                                  \
{                                                                               \
    return m_eventGenerator.QueueEvent( met,                                    \
                                        guidExtendedType,                       \
                                        hrStatus,                               \
                                        pvValue,                                \
                                        nullptr );                              \
}

#define INIT_EVENT_GENERATOR     IF_FAILED_GOTO( m_eventGenerator.Init() )
#define SHUTDOWN_EVENT_GENERATOR (void)m_eventGenerator.Shutdown()

template <typename TFunc>
class TMFWorkItem : public IMFAsyncCallback
{
public:
    TMFWorkItem( _In_ DWORD dwWorkQueueId, _In_ const TFunc &func, HANDLE hEvent ) :
        m_dwWorkQueueId( dwWorkQueueId ),
        m_func( func ),
        m_hEvent( hEvent )  // Note -- not taking ownership of this handle, so we will not close it
    {
        Module<InProc>::GetModule().IncrementObjectCount();
    }

    STDMETHODIMP_( ULONG ) AddRef()
    {
        ULONG ulRetValue = (ULONG)InterlockedIncrement( reinterpret_cast<volatile LONG*>( &m_ulReferenceCount ) );
        return ulRetValue;
    }

    STDMETHODIMP_( ULONG ) Release()
    {
        ULONG ulRetValue = (ULONG)InterlockedDecrement( reinterpret_cast<volatile LONG*>( &m_ulReferenceCount ) );

        if( 0 == ulRetValue )
        {
            delete this;
        }

        return ulRetValue;
    }

    STDMETHODIMP QueryInterface( REFIID riid, void** ppvObject )
    {
        if( nullptr == ppvObject )
        {
            return E_POINTER;
        }

        *ppvObject = nullptr;

        if( IID_IUnknown == riid )
        {
            *ppvObject = static_cast<IUnknown*>( this );
            AddRef();
            return S_OK;
        }
        else if( IID_IMFAsyncCallback == riid )
        {
            *ppvObject = static_cast<IMFAsyncCallback*>( this );
            AddRef();
            return S_OK;
        }
        else
        {
            return E_NOINTERFACE;
        }
    }

    IFACEMETHODIMP GetParameters(
        _Out_ DWORD *pdwFlags,
        _Out_ DWORD *pdwQueue )
    {
        *pdwFlags = 0;
        *pdwQueue = m_dwWorkQueueId;
        return S_OK;
    }

    IFACEMETHODIMP Invoke( _In_ IMFAsyncResult * pResult )
    {
        m_hrFuncResult = m_func();
        SetEvent( m_hEvent );
        return S_OK;
    }

    HRESULT GetResult()
    {
        return m_hrFuncResult;
    }

private:
    ~TMFWorkItem()
    {
        Module<InProc>::GetModule().DecrementObjectCount();
    }

    TMFWorkItem( const TMFWorkItem<TFunc> &nonCopyable ) = delete;
    TMFWorkItem& operator=( const TMFWorkItem<TFunc> &nonCopyable ) = delete;

private:
    const DWORD m_dwWorkQueueId;
    const TFunc m_func;
    HANDLE m_hEvent = nullptr;
    HRESULT m_hrFuncResult = S_OK;
    volatile ULONG m_ulReferenceCount = 1;
};

template <typename TFunc>
HRESULT MFPutWorkItem( _In_ DWORD dwWorkQueueId, _In_ const TFunc &func, HANDLE hWaitEvent )
{
    HRESULT hr = S_OK;
    ComPtr<IMFAsyncCallback> spIMFAsyncCallback;

    TMFWorkItem<TFunc> *pItem = nullptr;

    IF_NULL_GOTO( hWaitEvent, E_INVALIDARG );
    pItem = new TMFWorkItem<TFunc>( dwWorkQueueId, func, hWaitEvent );
    IF_NULL_GOTO( pItem, E_OUTOFMEMORY );
    spIMFAsyncCallback.Attach( pItem );

    IF_FAILED_GOTO( MFPutWorkItem( dwWorkQueueId, spIMFAsyncCallback.Get(), nullptr ) );

    if( WAIT_OBJECT_0 != WaitForSingleObjectEx( hWaitEvent, INFINITE, FALSE ) )
    {
        IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
        IF_FAILED_GOTO( MF_E_UNEXPECTED );
    }

    IF_FAILED_GOTO( pItem->GetResult() );

done:
    return hr;
}

template<typename TFunc>
HRESULT RunSyncInMTA( _In_ TFunc &&func )
{
    HRESULT          hr         = S_OK;
    HANDLE           hComplete  = nullptr;
    BOOL             fCoInit    = FALSE;
    APTTYPE          eType      = APTTYPE_CURRENT;
    APTTYPEQUALIFIER eQualifier = APTTYPEQUALIFIER_NONE;

    (void)CoGetApartmentType( &eType, &eQualifier );

    if( eType == APTTYPE_MTA )
    {
        if( eQualifier == APTTYPEQUALIFIER_IMPLICIT_MTA )
        {
            IF_FAILED_GOTO( CoInitializeEx( nullptr, COINIT_MULTITHREADED ) );
            fCoInit = TRUE;
        }

        IF_FAILED_GOTO( func() );
    }
    else
    {
        hComplete = CreateEvent( nullptr, TRUE, FALSE, nullptr );
        if( hComplete == nullptr )
        {
            IF_FAILED_GOTO( HRESULT_FROM_WIN32( GetLastError() ) );
            IF_FAILED_GOTO( MF_E_UNEXPECTED );
        }
        IF_FAILED_GOTO( MFPutWorkItem( MFASYNC_CALLBACK_QUEUE_MULTITHREADED, func, hComplete ) );
    }

done:
    if( hComplete != nullptr )
    {
        CloseHandle( hComplete );
    }
    if( fCoInit )
    {
        CoUninitialize();
    }
    return hr;
}

#define CDM_clearkey_IMPLEMENT_IMFATTRIBUTES( _spIMFAttributes_ )                                                                                                                       \
    STDMETHODIMP GetItem( __RPC__in REFGUID guidKey, __RPC__inout_opt PROPVARIANT *pValue )                                                                                             \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return ( _spIMFAttributes_ )->GetItem( guidKey, pValue );                                                                                                                   \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetItemType( __RPC__in REFGUID guidKey, __RPC__out MF_ATTRIBUTE_TYPE *pType )                                                                                          \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetItemType( guidKey, pType );                                                                                                                  \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP CompareItem( __RPC__in REFGUID guidKey, __RPC__in REFPROPVARIANT Value, __RPC__out BOOL *pbResult )                                                                    \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->CompareItem( guidKey, Value, pbResult );                                                                                                        \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP Compare( __RPC__in_opt IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, __RPC__out BOOL *pbResult )                                                         \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->Compare( pTheirs, MatchType, pbResult );                                                                                                        \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetUINT32( __RPC__in REFGUID guidKey, __RPC__out UINT32 *punValue )                                                                                                    \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetUINT32( guidKey, punValue );                                                                                                                 \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetUINT64( __RPC__in REFGUID guidKey, __RPC__out UINT64 *punValue )                                                                                                    \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetUINT64( guidKey, punValue );                                                                                                                 \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetDouble( __RPC__in REFGUID guidKey, __RPC__out double *pfValue )                                                                                                     \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetDouble( guidKey, pfValue );                                                                                                                  \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetGUID( __RPC__in REFGUID guidKey, __RPC__out GUID *pguidValue )                                                                                                      \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetGUID( guidKey, pguidValue );                                                                                                                 \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetStringLength( __RPC__in REFGUID guidKey, __RPC__out UINT32 *pcchLength )                                                                                            \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetStringLength( guidKey, pcchLength );                                                                                                         \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetString( __RPC__in REFGUID guidKey, __RPC__out_ecount_full( cchBufSize ) LPWSTR pwszValue, UINT32 cchBufSize, __RPC__inout_opt UINT32 *pcchLength )                  \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetString( guidKey, pwszValue, cchBufSize, pcchLength );                                                                                        \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetAllocatedString( __RPC__in REFGUID guidKey, __RPC__deref_out_ecount_full_opt( ( *pcchLength + 1 ) ) LPWSTR *ppwszValue, __RPC__out UINT32 *pcchLength )             \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetAllocatedString( guidKey, ppwszValue, pcchLength );                                                                                          \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetBlobSize( __RPC__in REFGUID guidKey, __RPC__out UINT32 *pcbBlobSize )                                                                                               \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetBlobSize( guidKey, pcbBlobSize );                                                                                                            \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetBlob( __RPC__in REFGUID guidKey, __RPC__out_ecount_full( cbBufSize ) UINT8 *pBuf, UINT32 cbBufSize, __RPC__inout_opt UINT32 *pcbBlobSize )                          \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetBlob( guidKey, pBuf, cbBufSize, pcbBlobSize );                                                                                               \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetAllocatedBlob( __RPC__in REFGUID guidKey, __RPC__deref_out_ecount_full_opt( *pcbSize ) UINT8 **ppBuf, __RPC__out UINT32 *pcbSize )                                  \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetAllocatedBlob( guidKey, ppBuf, pcbSize );                                                                                                    \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetUnknown( __RPC__in REFGUID guidKey, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID *ppv )                                                                       \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetUnknown( guidKey, riid, ppv );                                                                                                               \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP SetItem( __RPC__in REFGUID guidKey, __RPC__in REFPROPVARIANT Value )                                                                                                   \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->SetItem( guidKey, Value );                                                                                                                      \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP DeleteItem( __RPC__in REFGUID guidKey )                                                                                                                                \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->DeleteItem( guidKey );                                                                                                                          \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP DeleteAllItems( void )                                                                                                                                                 \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->DeleteAllItems();                                                                                                                               \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP SetUINT32( __RPC__in REFGUID guidKey, UINT32 unValue )                                                                                                                 \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->SetUINT32( guidKey, unValue );                                                                                                                  \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP SetUINT64( __RPC__in REFGUID guidKey, UINT64 unValue )                                                                                                                 \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->SetUINT64( guidKey, unValue );                                                                                                                  \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP SetDouble( __RPC__in REFGUID guidKey, double fValue )                                                                                                                  \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->SetDouble( guidKey, fValue );                                                                                                                   \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP SetGUID( __RPC__in REFGUID guidKey, __RPC__in REFGUID guidValue )                                                                                                      \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->SetGUID( guidKey, guidValue );                                                                                                                  \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP SetString( __RPC__in REFGUID guidKey, __RPC__in_string LPCWSTR wszValue )                                                                                              \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->SetString( guidKey, wszValue );                                                                                                                 \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP SetBlob( __RPC__in REFGUID guidKey, __RPC__in_ecount_full( cbBufSize ) const UINT8 *pBuf, UINT32 cbBufSize )                                                           \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->SetBlob( guidKey, pBuf, cbBufSize );                                                                                                            \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP SetUnknown( __RPC__in REFGUID guidKey, __RPC__in_opt IUnknown *pUnknown )                                                                                              \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->SetUnknown( guidKey, pUnknown );                                                                                                                \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP LockStore( void )                                                                                                                                                      \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->LockStore();                                                                                                                                    \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP UnlockStore( void )                                                                                                                                                    \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->UnlockStore();                                                                                                                                  \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetCount( __RPC__out UINT32 *pcItems )                                                                                                                                 \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetCount( pcItems );                                                                                                                            \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP GetItemByIndex( UINT32 unIndex, __RPC__out GUID *pguidKey, __RPC__inout_opt PROPVARIANT *pValue )                                                                      \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->GetItemByIndex( unIndex, pguidKey, pValue );                                                                                                    \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }                                                                                                                                                                                   \
                                                                                                                                                                                        \
    STDMETHODIMP CopyAllItems( __RPC__in_opt IMFAttributes *pDest )                                                                                                                     \
    {                                                                                                                                                                                   \
        HRESULT hrRet = RunSyncInMTA( [&]() -> HRESULT                                                                                                                                  \
        {                                                                                                                                                                               \
            return (_spIMFAttributes_)->CopyAllItems( pDest );                                                                                                                          \
        } );                                                                                                                                                                            \
        return hrRet;                                                                                                                                                                   \
    }

