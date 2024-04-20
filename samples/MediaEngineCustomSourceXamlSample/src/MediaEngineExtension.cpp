// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "MediaEngineExtension.h"

using namespace Microsoft::WRL;

namespace media
{
    HRESULT MediaEngineExtension::RuntimeClassInitialize() 
    {
        return S_OK;
    }

    IFACEMETHODIMP MediaEngineExtension::CanPlayType(BOOL /*isAudioOnly*/, BSTR /*mimeType*/, MF_MEDIA_ENGINE_CANPLAY* result) noexcept
    {
        // We use MF_MEDIA_ENGINE_EXTENSION to resolve as custom media source for
        // MFMediaEngine, MIME types are not used.
        *result = MF_MEDIA_ENGINE_CANPLAY_NOT_SUPPORTED;
        return S_OK;
    }

    IFACEMETHODIMP MediaEngineExtension::BeginCreateObject(BSTR /*url*/, IMFByteStream* /*byteStream*/, MF_OBJECT_TYPE type, IUnknown** cancelCookie,
        IMFAsyncCallback* callback, IUnknown* state) noexcept
    {
        if (cancelCookie)
        {
            *cancelCookie = nullptr;
        }
        ComPtr<IUnknown> localSource;
        {
            auto lock = m_lock.lock();
            if(m_hasShutdown)
                return MF_E_SHUTDOWN;
            localSource = m_mfMediaSource;
        }

        if (type == MF_OBJECT_MEDIASOURCE && localSource != nullptr)
        {
            ComPtr<IMFAsyncResult> asyncResult;
            RETURN_IF_FAILED(MFCreateAsyncResult(localSource.Get(), callback, state, &asyncResult));
            RETURN_IF_FAILED(asyncResult->SetStatus(S_OK));
            m_uriType = ExtensionUriType::CustomSource;
            // Invoke the callback synchronously since no outstanding work is required.
            RETURN_IF_FAILED(callback->Invoke(asyncResult.Get()));
        }
        else
        {
            return MF_E_UNEXPECTED;
        }

        return S_OK;
    }

    STDMETHODIMP MediaEngineExtension::CancelObjectCreation(_In_ IUnknown* /*cancelCookie*/) noexcept
    {
        // Cancellation not supported
        return E_NOTIMPL;
    }

    STDMETHODIMP MediaEngineExtension::EndCreateObject(IMFAsyncResult* result, IUnknown** object) noexcept
    {
        *object = nullptr;
        if (m_uriType == ExtensionUriType::CustomSource)
        {
            RETURN_IF_FAILED(result->GetStatus());
            RETURN_IF_FAILED(result->GetObject(object));
            m_uriType = ExtensionUriType::Unknown;
        }
        else
        {
            return MF_E_UNEXPECTED;
        }
        return S_OK;
    }

    void MediaEngineExtension::SetMediaSource(IUnknown* mfMediaSource)
    {
        auto lock = m_lock.lock();
        if (m_hasShutdown)
            return;
        m_mfMediaSource = mfMediaSource;
    }

    // Break circular references.
    void MediaEngineExtension::Shutdown()
    {
        auto lock = m_lock.lock();
        if (!m_hasShutdown)
        {
            m_mfMediaSource = nullptr;
            m_hasShutdown = true;
        }
    }

} // namespace media