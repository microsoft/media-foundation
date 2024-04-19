// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

using namespace Microsoft::WRL;

namespace media
{

    // This implementation of IMFMediaEngineExtension is used to integrate a custom
    // IMFMediaSource with the MediaEngine pipeline
    class MediaEngineExtension
        : public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>,
        IMFMediaEngineExtension>
    {
    public:
        MediaEngineExtension() = default;
        ~MediaEngineExtension() override = default;

        HRESULT RuntimeClassInitialize();

        // IMFMediaEngineExtension
        IFACEMETHOD(CanPlayType)
            (BOOL isAudioOnly, BSTR mimeType, MF_MEDIA_ENGINE_CANPLAY* result) noexcept override;
        IFACEMETHOD(BeginCreateObject)
            (BSTR url, IMFByteStream* byteStream, MF_OBJECT_TYPE type, IUnknown** cancelCookie, IMFAsyncCallback* callback,
                IUnknown* state) noexcept override;
        IFACEMETHOD(CancelObjectCreation)(IUnknown* cancelCookie) noexcept override;
        IFACEMETHOD(EndCreateObject)
            (IMFAsyncResult* result, IUnknown** object) noexcept override;

        // Public methods
        void SetMediaSource(IUnknown* mfMediaSource);
        void Shutdown();

    private:
        wil::critical_section m_lock;
        enum class ExtensionUriType
        {
            Unknown = 0,
            CustomSource
        };
        ExtensionUriType m_uriType = ExtensionUriType::Unknown;
        bool m_hasShutdown = false;
        ComPtr<IUnknown> m_mfMediaSource;
    };
} // namespace media