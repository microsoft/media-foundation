#pragma once

#include <string>
#include <utility>
#include <functional>

// NOTE:
//   https://github.com/microsoft/media-foundation/issues/37
//   https://github.com/chromium/chromium/commit/43ba4df2434db7cf1eba5698d1dd355061575459#diff-fea3e99ae70746938f3efcf994f1b385fc430770b2adcc15d2c9bfd99b1969f3

inline std::string ToString(MF_MEDIAKEYSESSION_TYPE Value)
{
	static std::pair<MF_MEDIAKEYSESSION_TYPE, char const* const> constexpr const g_Items[]
	{
		#define IDENFITIER(Name) std::make_pair(Name, #Name),
		IDENFITIER(MF_MEDIAKEYSESSION_TYPE_TEMPORARY)
		IDENFITIER(MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE)
		IDENFITIER(MF_MEDIAKEYSESSION_TYPE_PERSISTENT_RELEASE_MESSAGE)
		IDENFITIER(MF_MEDIAKEYSESSION_TYPE_PERSISTENT_USAGE_RECORD)
		#undef IDENFITIER
	};
	return ToString(g_Items, Value);
}

inline std::string ToString(MF_MEDIAKEY_STATUS Value)
{
	static std::pair<MF_MEDIAKEY_STATUS, char const* const> constexpr const g_Items[]
	{
		#define IDENFITIER(Name) std::make_pair(Name, #Name),
		IDENFITIER(MF_MEDIAKEY_STATUS_USABLE)
		IDENFITIER(MF_MEDIAKEY_STATUS_EXPIRED)
		IDENFITIER(MF_MEDIAKEY_STATUS_OUTPUT_DOWNSCALED)
		IDENFITIER(MF_MEDIAKEY_STATUS_OUTPUT_NOT_ALLOWED)
		IDENFITIER(MF_MEDIAKEY_STATUS_STATUS_PENDING)
		IDENFITIER(MF_MEDIAKEY_STATUS_INTERNAL_ERROR)
		IDENFITIER(MF_MEDIAKEY_STATUS_RELEASED)
		IDENFITIER(MF_MEDIAKEY_STATUS_OUTPUT_RESTRICTED)
		#undef IDENFITIER
	};
	return ToString(g_Items, Value);
}

// MFMediaKeyStatus

inline std::string ToString(MF_MEDIAKEYSESSION_MESSAGETYPE Value)
{
	static std::pair<MF_MEDIAKEYSESSION_MESSAGETYPE, char const* const> constexpr const g_Items[]
	{
		#define IDENFITIER(Name) std::make_pair(Name, #Name),
		IDENFITIER(MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST)
		IDENFITIER(MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RENEWAL)
		IDENFITIER(MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RELEASE)
		IDENFITIER(MF_MEDIAKEYSESSION_MESSAGETYPE_INDIVIDUALIZATION_REQUEST)
		#undef IDENFITIER
	};
	return ToString(g_Items, Value);
}

// MF_CROSS_ORIGIN_POLICY

namespace Eme {

inline void SetPropertyValue(wil::com_ptr<IPropertyStore> const& PropertyStore, PROPERTYKEY const& Key, std::wstring const& Value)
{
	WI_ASSERT(PropertyStore);
	wil::unique_prop_variant VariantValue;
	VariantValue.vt = VT_BSTR;
	VariantValue.bstrVal = wil::make_bstr(Value.c_str()).release();
	THROW_IF_FAILED(PropertyStore->SetValue(Key, VariantValue));
}

// NOTE: EME MediaKeySession interface 
//       https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession

struct MediaKeySession : std::enable_shared_from_this<MediaKeySession>
{
	// https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeystatusmap
	using MediaKeyStatus = std::pair<std::vector<uint8_t>, MF_MEDIAKEY_STATUS>;
	using MediaKeyStatusMap = std::vector<MediaKeyStatus>;

	MediaKeySession()
	{
		TRACE(L"this %p\n", this);
	}
	~MediaKeySession()
	{
		TRACE(L"this %p\n", this);
		if(Callbacks)
			Callbacks->Owner.reset();
	}

	void Initialize(wil::com_ptr<IMFContentDecryptionModule> const& ContentDecryptionModule, MF_MEDIAKEYSESSION_TYPE Type = MF_MEDIAKEYSESSION_TYPE_TEMPORARY)
	{
		TRACE(L"this %p, Type %hs\n", this, ToString(Type).c_str());
		WI_ASSERT(ContentDecryptionModule && !Callbacks);
		Callbacks = winrt::make_self<SessionCallbacks>(shared_from_this());
		THROW_IF_FAILED(ContentDecryptionModule->CreateSession(Type, Callbacks.get(), ContentDecryptionModuleSession.put()));
		TRACE(L"this %p, ContentDecryptionModuleSession %p\n", this, ContentDecryptionModuleSession.get());
	}

	std::wstring SessionId() const // sessionId
	{
		TRACE(L"this %p\n", this);
		// NOTE: https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-sessionid
		WI_ASSERT(ContentDecryptionModuleSession);
		wil::unique_cotaskmem_string Result;
		THROW_IF_FAILED(ContentDecryptionModuleSession->GetSessionId(Result.put()));
		return Result.get();
	}
	double Expiration() const // expiration
	{
		TRACE(L"this %p\n", this);
		// NOTE: https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-expiration
		//       https://tc39.es/ecma262/#sec-time-values-and-time-range
		WI_ASSERT(ContentDecryptionModuleSession);
		double Result = 0.0;
		THROW_IF_FAILED(ContentDecryptionModuleSession->GetExpiration(&Result));
		return Result;
	}
	// closed

	MediaKeyStatusMap KeyStatuses() const // keyStatuses
	{
		TRACE(L"this %p\n", this);
		// NOTE: https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-keystatuses
		WI_ASSERT(ContentDecryptionModuleSession);
		wil::unique_cotaskmem_array_ptr<MFMediaKeyStatus> ArrayResult;
		THROW_IF_FAILED(ContentDecryptionModuleSession->GetKeyStatuses(ArrayResult.put(), ArrayResult.size_address<UINT>()));
		MediaKeyStatusMap Result;
		Result.reserve(ArrayResult.size());
		for(size_t Index = 0; Index < ArrayResult.size(); Index++)
		{
			auto const& Element = ArrayResult[Index];
			std::vector<uint8_t> KeyId(Element.pbKeyId, Element.pbKeyId + Element.cbKeyId);
			Result.emplace_back(std::make_pair(std::move(KeyId), Element.eMediaKeyStatus));
		}
		return Result;
	}

	std::function<void(MF_MEDIAKEYSESSION_MESSAGETYPE, std::vector<uint8_t> const&, std::wstring)> OnMessage; // onmessage
	std::function<void()> OnKeyStatusChange; // onkeystatuseschange
	
	void GenerateRequest(std::wstring const& InitializationDataType, std::vector<uint8_t> const& InitializationData) // generateRequest
	{
		TRACE(L"this %p, InitializationDataType %ls, InitializationData %zu\n", this, InitializationDataType.c_str(), InitializationData.size());
		TRACE(L"SessionId %ls\n", SessionId().c_str());
		// NOTE: https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-generaterequest
		//       https://www.w3.org/TR/eme-initdata-registry/
		THROW_IF_FAILED(ContentDecryptionModuleSession->GenerateRequest(InitializationDataType.c_str(), InitializationData.data(), static_cast<DWORD>(InitializationData.size())));
	}
	bool Load(std::wstring const& SessionIdentifier) // load
	{
		TRACE(L"this %p, SessionIdentifier %ls\n", this, SessionIdentifier.c_str());
		// NOTE: https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-load
		WI_ASSERT(ContentDecryptionModuleSession);
		BOOL Result = FALSE;
		THROW_IF_FAILED(ContentDecryptionModuleSession->Load(SessionIdentifier.c_str(), &Result));
		return Result != FALSE;
	}
	void Update(std::vector<uint8_t> const& ResponseData) // update
	{
		TRACE(L"this %p, ResponseData %zu\n", this, ResponseData.size());
		// NOTE: https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-update
		WI_ASSERT(ContentDecryptionModuleSession);
		THROW_IF_FAILED(ContentDecryptionModuleSession->Update(ResponseData.data(), static_cast<DWORD>(ResponseData.size())));
	}
	void Close() // close
	{
		TRACE(L"this %p\n", this);
		// NOTE: https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-close
		WI_ASSERT(ContentDecryptionModuleSession);
		THROW_IF_FAILED(ContentDecryptionModuleSession->Close());
	}
	void Remove() // remove
	{
		TRACE(L"this %p\n", this);
		// NOTE: https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysession-remove
		WI_ASSERT(ContentDecryptionModuleSession);
		THROW_IF_FAILED(ContentDecryptionModuleSession->Remove());
	}

	class SessionCallbacks : public winrt::implements<SessionCallbacks, IMFContentDecryptionModuleSessionCallbacks>
	{
	public:
		SessionCallbacks(std::shared_ptr<MediaKeySession> const& Owner) :
			Owner(Owner)
		{
			TRACE(L"this %p\n", this);
		}
		~SessionCallbacks()
		{
			TRACE(L"this %p\n", this);
		}

	// IMFContentDecryptionModuleSessionCallbacks
		IFACEMETHOD(KeyMessage)(MF_MEDIAKEYSESSION_MESSAGETYPE Type, BYTE const* Data, DWORD DataSize, LPCWSTR Location) override
		{
			TRACE(L"this %p, Type %hs, DataSize %u, Location \"%ls\"\n", this, ToString(Type).c_str(), DataSize, Location);
			try
			{
				auto const Owner = this->Owner.lock();
				if(!Owner)
					return S_FALSE;
				if(Owner->OnMessage)
					Owner->OnMessage(Type, std::vector<uint8_t>(Data, Data + DataSize), Location);
			}
			CATCH_RETURN();
			return S_OK;
		}
		IFACEMETHOD(KeyStatusChanged)() override
		{
			TRACE(L"this %p\n", this);
			try
			{
				auto const Owner = this->Owner.lock();
				if(!Owner)
					return S_FALSE;
				if(Owner->OnKeyStatusChange)
					Owner->OnKeyStatusChange();
			}
			CATCH_RETURN();
			return S_OK;
		}

		std::weak_ptr<MediaKeySession> Owner;
	};

	wil::com_ptr<IMFContentDecryptionModuleSession> ContentDecryptionModuleSession;
	winrt::com_ptr<SessionCallbacks> Callbacks;
};

// NOTE: EME MediaKeys interface
//       https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeys

struct MediaKeys
{
public:
	MediaKeys(wil::com_ptr<IMFContentDecryptionModule> const& ContentDecryptionModule) :
		ContentDecryptionModule(ContentDecryptionModule)
	{
		TRACE(L"this %p\n", this);
	}
	~MediaKeys()
	{
		TRACE(L"this %p\n", this);
	}

	std::shared_ptr<MediaKeySession> CreateSession(MF_MEDIAKEYSESSION_TYPE Type = MF_MEDIAKEYSESSION_TYPE_TEMPORARY) // createSession
	{
		auto const Result = std::make_shared<MediaKeySession>();
		Result->Initialize(ContentDecryptionModule, Type);
		return Result;
	}
	bool SetServerCertificate(std::vector<uint8_t>& ServerCertificate) // setServerCertificate
	{
		return SUCCEEDED(ContentDecryptionModule->SetServerCertificate(ServerCertificate.data(), static_cast<DWORD>(ServerCertificate.size())));
	}

	wil::com_ptr<IMFContentDecryptionModule> const ContentDecryptionModule;
};

struct FactoryConfiguration
{
	wil::com_ptr<IPropertyStore> CreatePropertyStore() const
	{
		wil::com_ptr<IPropertyStore> PropertyStore;
		THROW_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(PropertyStore.put())));
		SetPropertyValue(PropertyStore, MF_EME_CDM_STOREPATH, StoragePath);
		if(!InPrivateStoragePath.empty())
			SetPropertyValue(PropertyStore, MF_EME_CDM_INPRIVATESTOREPATH, InPrivateStoragePath);
		return PropertyStore;
	}

	std::wstring StoragePath;
	std::wstring InPrivateStoragePath;
};

// NOTE: EME MediaKeySystemMediaCapability dictionary 
//       https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-mediakeysystemmediacapability

struct MediaKeySystemMediaCapability
{
	MediaKeySystemMediaCapability() = default;
	MediaKeySystemMediaCapability(wil::com_ptr<IPropertyStore> const& PropertyStore)
	{
		WI_ASSERT(PropertyStore);
		{
			wil::unique_prop_variant Value;
			if (SUCCEEDED(PropertyStore->GetValue(MF_EME_ROBUSTNESS, &Value)) && Value.vt == VT_BSTR)
				Robustness = Value.bstrVal;
		}
		{
			wil::unique_prop_variant Value;
			if (SUCCEEDED(PropertyStore->GetValue(MF_EME_CONTENTTYPE, &Value)) && Value.vt == VT_BSTR)
				ContentType = Value.bstrVal;
		}
	}

	wil::com_ptr<IPropertyStore> CreatePropertyStore() const
	{
		wil::com_ptr<IPropertyStore> PropertyStore;
		THROW_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(PropertyStore.put())));
		SetPropertyValue(PropertyStore, MF_EME_ROBUSTNESS, Robustness);
		SetPropertyValue(PropertyStore, MF_EME_CONTENTTYPE, ContentType);
		return PropertyStore;
	}

	std::wstring ContentType; // contentType
	std::wstring Robustness; // robustness
};

// NOTE: EME MediaKeySystemConfiguration dictionary
//       https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#mediakeysystemconfiguration-dictionary

struct MediaKeySystemConfiguration
{
	static std::vector<MediaKeySystemMediaCapability> CapabilityVectorFromVariant(PROPVARIANT const& Value)
	{
		std::vector<MediaKeySystemMediaCapability> Result;
		Result.reserve(Value.capropvar.cElems);
		for(ULONG Index = 0; Index < Value.capropvar.cElems; Index++)
		{
			PROPVARIANT& Element = Value.capropvar.pElems[Index];
			wil::com_ptr<IPropertyStore> ElementPropertyStore;
			if(Element.vt != VT_UNKNOWN || FAILED(Element.punkVal->QueryInterface(IID_PPV_ARGS(ElementPropertyStore.put()))))
				continue;
			Result.emplace_back(MediaKeySystemMediaCapability(ElementPropertyStore));
		}
		return Result;
	}

	MediaKeySystemConfiguration() = default;
	MediaKeySystemConfiguration(wil::com_ptr<IPropertyStore> const& PropertyStore)
	{
		WI_ASSERT(PropertyStore);
		{
			wil::unique_prop_variant Value;
			if(SUCCEEDED(PropertyStore->GetValue(MF_EME_INITDATATYPES, &Value)) && Value.vt == (VT_VECTOR | VT_BSTR))
			{
				InitDataTypeVector.reserve(Value.cabstr.cElems);
				for(ULONG Index = 0; Index < Value.cabstr.cElems; Index++)
					InitDataTypeVector.emplace_back(Value.cabstr.pElems[Index]);
			}
		}
		{
			wil::unique_prop_variant Value;
			if (SUCCEEDED(PropertyStore->GetValue(MF_EME_AUDIOCAPABILITIES, &Value)) && Value.vt == (VT_VECTOR | VT_VARIANT))
				AudioCapabilityVector = CapabilityVectorFromVariant(Value);
		}
		{
			wil::unique_prop_variant Value;
			if (SUCCEEDED(PropertyStore->GetValue(MF_EME_VIDEOCAPABILITIES, &Value)) && Value.vt == (VT_VECTOR | VT_VARIANT))
				VideoCapabilityVector = CapabilityVectorFromVariant(Value);
		}
		{
			wil::unique_prop_variant Value;
			if (SUCCEEDED(PropertyStore->GetValue(MF_EME_DISTINCTIVEID, &Value)) && Value.vt == VT_UI4)
				DistinctiveIdentifier = static_cast<MF_MEDIAKEYS_REQUIREMENT>(Value.ulVal);
		}
		{
			wil::unique_prop_variant Value;
			if (SUCCEEDED(PropertyStore->GetValue(MF_EME_PERSISTEDSTATE, &Value)) && Value.vt == VT_UI4)
				PersistentState = static_cast<MF_MEDIAKEYS_REQUIREMENT>(Value.ulVal);
		}
		{
			wil::unique_prop_variant Value;
			if (SUCCEEDED(PropertyStore->GetValue(MF_EME_SESSIONTYPES, &Value)) && Value.vt == (VT_VECTOR | VT_UI4))
			{
				SessionTypeVector.reserve(Value.caul.cElems);
				for(ULONG Index = 0; Index < Value.caul.cElems; Index++)
					SessionTypeVector.emplace_back(static_cast<MF_MEDIAKEYSESSION_TYPE>(Value.caul.pElems[Index]));
			}
		}
	}

	template <typename ValueType>
	static wil::unique_prop_variant VariantFromInteger(ValueType Value)
	{
		wil::unique_prop_variant Result;
		Result.vt = VT_UI4;
		Result.ulVal = static_cast<DWORD>(Value);
		return Result;
	}
	static wil::unique_prop_variant VariantFromCapabilityVector(std::vector<MediaKeySystemMediaCapability> const& CapabilityVector)
	{
		auto Array = wil::make_unique_cotaskmem<PROPVARIANT[]>(CapabilityVector.size());
		for(size_t Index = 0; Index < CapabilityVector.size(); Index++)
		{
			auto& Capability = Array[Index];
			Capability.vt = VT_UNKNOWN;
			Capability.punkVal = CapabilityVector[Index].CreatePropertyStore().detach();
		}
		wil::unique_prop_variant Result;
		Result.vt = VT_VECTOR | VT_VARIANT;
		Result.capropvar.cElems = static_cast<ULONG>(CapabilityVector.size());
		Result.capropvar.pElems = Array.release();
		return Result;
	}
	wil::com_ptr<IPropertyStore> CreatePropertyStore() const
	{
		wil::com_ptr<IPropertyStore> PropertyStore;
		THROW_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(PropertyStore.put())));
		{
			auto Vector = wil::make_unique_cotaskmem<BSTR[]>(InitDataTypeVector.size());
			for(size_t Index = 0; Index < InitDataTypeVector.size(); Index++)
				Vector[Index] = wil::make_bstr(InitDataTypeVector[Index].c_str()).release();
			wil::unique_prop_variant Value;
			Value.vt = VT_VECTOR | VT_BSTR;
			Value.cabstr.cElems = static_cast<ULONG>(InitDataTypeVector.size());
			Value.cabstr.pElems = Vector.release();
			THROW_IF_FAILED(PropertyStore->SetValue(MF_EME_INITDATATYPES, Value));
		}
		THROW_IF_FAILED(PropertyStore->SetValue(MF_EME_AUDIOCAPABILITIES, VariantFromCapabilityVector(AudioCapabilityVector)));
		THROW_IF_FAILED(PropertyStore->SetValue(MF_EME_VIDEOCAPABILITIES, VariantFromCapabilityVector(VideoCapabilityVector)));
		THROW_IF_FAILED(PropertyStore->SetValue(MF_EME_DISTINCTIVEID, VariantFromInteger(DistinctiveIdentifier)));
		THROW_IF_FAILED(PropertyStore->SetValue(MF_EME_PERSISTEDSTATE, VariantFromInteger(PersistentState)));
		{
			auto Vector = wil::make_unique_cotaskmem<ULONG[]>(SessionTypeVector.size());
			for(size_t Index = 0; Index < SessionTypeVector.size(); Index++)
				Vector[Index] = static_cast<ULONG>(SessionTypeVector[Index]);
			wil::unique_prop_variant Value;
			Value.vt = VT_VECTOR | VT_UI4;
			Value.caul.cElems = static_cast<ULONG>(SessionTypeVector.size());
			Value.caul.pElems = Vector.release();
			THROW_IF_FAILED(PropertyStore->SetValue(MF_EME_SESSIONTYPES, Value));
		}
		return PropertyStore;
	}

	std::wstring Label; // label
	std::vector<std::wstring> InitDataTypeVector; // Value
	std::vector<MediaKeySystemMediaCapability> AudioCapabilityVector; // audioCapabilities
	std::vector<MediaKeySystemMediaCapability> VideoCapabilityVector; // videoCapabilities
	MF_MEDIAKEYS_REQUIREMENT DistinctiveIdentifier; // distinctiveIdentifier
	MF_MEDIAKEYS_REQUIREMENT PersistentState; // persistentState
	std::vector<MF_MEDIAKEYSESSION_TYPE> SessionTypeVector; // Value
};

// NOTE: EME MediaKeySystemAccess interface 
//       https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#mediakeysystemaccess-interface

class MediaKeySystemAccess
{
public:
	MediaKeySystemAccess(FactoryConfiguration& Configuration, wil::com_ptr<IMFContentDecryptionModuleAccess> const& ContentDecryptionModuleAccess) :
		m_ContentDecryptionModulePropertyStore(Configuration.CreatePropertyStore()),
		m_ContentDecryptionModuleAccess(ContentDecryptionModuleAccess)
	{
		TRACE(L"this %p\n", this);
		wil::com_ptr<IPropertyStore> PropertyStore;
		THROW_IF_FAILED(ContentDecryptionModuleAccess->GetConfiguration(PropertyStore.put()));
		m_Configuration = MediaKeySystemConfiguration(PropertyStore);
	}
	~MediaKeySystemAccess()
	{
		TRACE(L"this %p\n", this);
	}

	std::wstring KeySystem() const // keySystem
	{
		wil::unique_cotaskmem_string Result;
		THROW_IF_FAILED(m_ContentDecryptionModuleAccess->GetKeySystem(Result.put()));
		return Result.get();
	}
	MediaKeySystemConfiguration const& Configuration() const // getConfiguration
	{
		return m_Configuration;
	}
	std::shared_ptr<MediaKeys> CreateMediaKeys() const // createMediaKeys
	{
		wil::com_ptr<IMFContentDecryptionModule> ContentDecryptionModule;
		THROW_IF_FAILED(m_ContentDecryptionModuleAccess->CreateContentDecryptionModule(m_ContentDecryptionModulePropertyStore.get(), ContentDecryptionModule.put()));
		TRACE(L"this %p, ContentDecryptionModule %p\n", this, ContentDecryptionModule.get());
		return std::make_shared<MediaKeys>(ContentDecryptionModule);
	}

private:
	wil::com_ptr<IPropertyStore> const m_ContentDecryptionModulePropertyStore;
	wil::com_ptr<IMFContentDecryptionModuleAccess> const m_ContentDecryptionModuleAccess;
	MediaKeySystemConfiguration m_Configuration;
};

struct Factory
{
	Factory(FactoryConfiguration const& Configuration) :
		Configuration(Configuration)
	{
		TRACE(L"this %p\n", this);
		THROW_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(ClassFactory4.put())));
	}
	~Factory()
	{
		TRACE(L"this %p\n", this);
	}

	std::shared_ptr<MediaKeySystemAccess> RequestMediaKeySystemAccess(std::wstring const& KeySystem, std::vector<MediaKeySystemConfiguration>& ConfigurationVector) // requestMediaKeySystemAccess
	{
		// NOTE: EME Navigator Extension: requestMediaKeySystemAccess()
		//       https://www.w3.org/TR/2017/REC-encrypted-media-20170918/#dom-navigator-requestmediakeysystemaccess
		TRACE(L"this %p, KeySystem %ls, ConfigurationVector.size() %zu\n", this, KeySystem.c_str(), ConfigurationVector.size());
		LOG_HR_IF(E_NOTIMPL, KeySystem != L"com.microsoft.playready.recommendation");
		if(!ContentDecryptionModuleFactory || this->KeySystem != KeySystem)
		{
			auto const CreateContentDecryptionModuleFactoryResult = ClassFactory4->CreateContentDecryptionModuleFactory(KeySystem.c_str(), IID_PPV_ARGS(ContentDecryptionModuleFactory.put()));
			LOG_IF_FAILED(CreateContentDecryptionModuleFactoryResult);
			if(FAILED(CreateContentDecryptionModuleFactoryResult))
				return nullptr;
			TRACE(L"this %p, ContentDecryptionModuleFactory %p\n", this, ContentDecryptionModuleFactory.get());
			this->KeySystem = KeySystem;
		}
		auto const IsTypeSupported = ContentDecryptionModuleFactory->IsTypeSupported(KeySystem.c_str(), nullptr);
		LOG_HR_IF(E_FAIL, !IsTypeSupported);
		if(!IsTypeSupported)
			return nullptr;
		wil::com_ptr<IMFContentDecryptionModuleAccess> ContentDecryptionModuleAccess;
		std::vector<wil::com_ptr<IPropertyStore>> ConfigurationPropertyStoreVector;
		ConfigurationPropertyStoreVector.reserve(ConfigurationVector.size());
		std::vector<IPropertyStore*> RawConfigurationPropertyStoreVector;
		RawConfigurationPropertyStoreVector.reserve(ConfigurationVector.size());
		for(auto&& Configuration: ConfigurationVector)
		{
			ConfigurationPropertyStoreVector.emplace_back(Configuration.CreatePropertyStore());
			RawConfigurationPropertyStoreVector.emplace_back(ConfigurationPropertyStoreVector.back().get());
		}
		auto const CreateContentDecryptionModuleAccessResult = ContentDecryptionModuleFactory->CreateContentDecryptionModuleAccess(KeySystem.c_str(), RawConfigurationPropertyStoreVector.data(), static_cast<DWORD>(RawConfigurationPropertyStoreVector.size()), ContentDecryptionModuleAccess.put());
		LOG_IF_FAILED(CreateContentDecryptionModuleAccessResult);
		if(FAILED(CreateContentDecryptionModuleAccessResult))
			return nullptr;
		TRACE(L"this %p, ContentDecryptionModuleAccess %p\n", this, ContentDecryptionModuleAccess.get());
		return std::make_shared<MediaKeySystemAccess>(Configuration, ContentDecryptionModuleAccess);
	}

	FactoryConfiguration Configuration;
	wil::com_ptr<IMFMediaEngineClassFactory4> ClassFactory4;
	wil::com_ptr<IMFContentDecryptionModuleFactory> ContentDecryptionModuleFactory;
	std::wstring KeySystem;
};

} // namespace Eme

#include <map>

class InputTrustAuthorityMediaSource : public winrt::implements<InputTrustAuthorityMediaSource, IMFMediaSource, IMFTrustedInput>
{
public:
	InputTrustAuthorityMediaSource(wil::com_ptr<IMFMediaSource> const& MediaSource, wil::com_ptr<IMFContentDecryptionModule> const& ContentDecryptionModule) :
		m_Inner(MediaSource),
		m_ContentDecryptionModule(ContentDecryptionModule)
	{
		WI_ASSERT(m_Inner && m_ContentDecryptionModule);
	}

// IMFMediaSource
	IFACEMETHOD(GetCharacteristics)(DWORD* Characteristics) override
	{
		//TRACE(L"this %p\n", this);
		return m_Inner->GetCharacteristics(Characteristics);
	}
	IFACEMETHOD(CreatePresentationDescriptor)(IMFPresentationDescriptor** PresentationDescriptor) override
	{
		//TRACE(L"this %p\n", this);
		return m_Inner->CreatePresentationDescriptor(PresentationDescriptor);
	}
	IFACEMETHOD(Start)(IMFPresentationDescriptor* PresentationDescriptor, GUID const* TimeFormat, PROPVARIANT const* StartPosition) override
	{
		TRACE(L"this %p\n", this);
		return m_Inner->Start(PresentationDescriptor, TimeFormat, StartPosition);
	}
	IFACEMETHOD(Stop)() override
	{
		TRACE(L"this %p\n", this);
		return m_Inner->Stop();
	}
	IFACEMETHOD(Pause)() override
	{
		TRACE(L"this %p\n", this);
		return m_Inner->Pause();
	}
	IFACEMETHOD(Shutdown)() override
	{
		TRACE(L"this %p\n", this);
		try
		{
			[[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
			m_TrustedInput.reset();
			m_StreamTrustedInputMap.clear();
			return m_Inner->Shutdown();    
		}
		CATCH_RETURN();
	}

// IMFMediaEventGenerator
	IFACEMETHOD(GetEvent)(DWORD Flags, IMFMediaEvent** MediaEvent) override
	{ 
		//TRACE(L"this %p\n", this);
		return m_Inner->GetEvent(Flags, MediaEvent);
	}
	IFACEMETHOD(BeginGetEvent)(IMFAsyncCallback* AsyncCallback, IUnknown* StateUnknown) override
	{
		//TRACE(L"this %p\n", this);
		return m_Inner->BeginGetEvent(AsyncCallback, StateUnknown);
	}
	IFACEMETHOD(EndGetEvent)(IMFAsyncResult* AsyncResult, IMFMediaEvent** MediaEvent) override
	{ 
		//TRACE(L"this %p\n", this);
		return m_Inner->EndGetEvent(AsyncResult, MediaEvent);
	}
	IFACEMETHOD(QueueEvent)(MediaEventType Type, REFGUID ExtendedType, HRESULT Status, PROPVARIANT const* Value) override
	{ 
		//TRACE(L"this %p\n", this);
		return m_Inner->QueueEvent(Type, ExtendedType, Status, Value);
	}

// IMFTrustedInput
	IFACEMETHOD(GetInputTrustAuthority)(DWORD StreamIdentifier, REFIID InterfaceIdentifier, IUnknown** Object) override
	{
		TRACE(L"this %p, StreamIdentifier %u, InterfaceIdentifier %ls\n", this, StreamIdentifier, ToString(InterfaceIdentifier).c_str());
		try
		{
			LOG_HR_IF(S_FALSE, InterfaceIdentifier != __uuidof(IMFInputTrustAuthority));
			WI_ASSERT(Object);
			wil::com_ptr<IUnknown> Unknown;
			[[maybe_unused]] auto&& DataLock = m_DataMutex.lock_exclusive();
			auto const Iterator = m_StreamTrustedInputMap.find(StreamIdentifier);
			if(Iterator == m_StreamTrustedInputMap.end())
			{
				if(!m_TrustedInput)
					THROW_IF_FAILED(m_ContentDecryptionModule->CreateTrustedInput(nullptr, 0, m_TrustedInput.put()));
				THROW_IF_FAILED(m_TrustedInput->GetInputTrustAuthority(StreamIdentifier, InterfaceIdentifier, Unknown.put()));
				m_StreamTrustedInputMap.emplace(StreamIdentifier, Unknown);
			} else
				Unknown = Iterator->second;
			*Object = Unknown.detach();
		}
		CATCH_RETURN();
		return S_OK;
	}

private:
	wil::com_ptr<IMFContentDecryptionModule> const m_ContentDecryptionModule;
	wil::com_ptr<IMFMediaSource> const m_Inner;
	mutable wil::srwlock m_DataMutex;
	wil::com_ptr<IMFTrustedInput> m_TrustedInput;
	std::map<DWORD, wil::com_ptr<IUnknown>> m_StreamTrustedInputMap;
};

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

class PmpHostApp : public winrt::implements<PmpHostApp, IMFPMPHostApp> 
{
public:
	PmpHostApp(wil::com_ptr<IMFPMPHost> const& PmpHost) : 
		m_PmpHost(PmpHost)
	{
	}

// IMFPMPHostApp
	IFACEMETHOD(LockProcess)() override
	{
		TRACE(L"this %p\n", this);
		try
		{
			THROW_IF_FAILED(m_PmpHost->LockProcess());
		}
		CATCH_RETURN();
		return S_OK;
	}
	IFACEMETHOD(UnlockProcess)() override
	{
		TRACE(L"this %p\n", this);
		try
		{
			THROW_IF_FAILED(m_PmpHost->UnlockProcess());
		}
		CATCH_RETURN();
		return S_OK;
	}
	IFACEMETHOD(ActivateClassById)(LPCWSTR Identifier, IStream* Stream, REFIID InterfaceIdentifier, void** Object) override
	{
		TRACE(L"this %p, Identifier %ls\n", this, Identifier);
		try
		{
			struct __declspec(uuid("{77631A31-E5E7-4785-BF17-20F57B224802}")) ClassName;
			struct __declspec(uuid("{3E73735C-E6C0-481D-8260-EE5DB1343B5F}")) ObjectStream;
			struct __declspec(uuid("{2DF7B51E-797B-4D06-BE71-D14A52CF8421}")) StoreActivate;
			wil::com_ptr<IMFAttributes> Attributes;
			THROW_IF_FAILED(MFCreateAttributes(Attributes.put(), 3));
			THROW_IF_FAILED(Attributes->SetString(__uuidof(ClassName), Identifier));
			if(Stream) 
			{
				STATSTG StatStg;
				THROW_IF_FAILED(Stream->Stat(&StatStg, STATFLAG_NOOPEN | STATFLAG_NONAME));
				WI_ASSERT(!StatStg.cbSize.QuadPart);
				std::vector<uint8_t> Data(StatStg.cbSize.LowPart);
				ULONG DataSize = 0;
				THROW_IF_FAILED(Stream->Read(Data.data(), static_cast<ULONG>(Data.size()), &DataSize));
				THROW_IF_FAILED(Attributes->SetBlob(__uuidof(ObjectStream), Data.data(), DataSize));
			}
			wil::com_ptr<IStream> AttributeStream;
			THROW_IF_FAILED(CreateStreamOnHGlobal(nullptr, TRUE, AttributeStream.put()));
			THROW_IF_FAILED(MFSerializeAttributesToStream(Attributes.get(), 0, AttributeStream.get()));
			THROW_IF_FAILED(AttributeStream->Seek({ }, STREAM_SEEK_SET, nullptr));
			wil::com_ptr<IMFActivate> Activate;
			THROW_IF_FAILED(m_PmpHost->CreateObjectByCLSID(__uuidof(StoreActivate), AttributeStream.get(), IID_PPV_ARGS(Activate.put())));
			THROW_IF_FAILED(Activate->ActivateObject(InterfaceIdentifier, Object));
		}
		CATCH_RETURN();
		return S_OK;
	}

private:
	wil::com_ptr<IMFPMPHost> m_PmpHost;
};

#endif