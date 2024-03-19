#

## Sample Output

```
App.cpp(246): App::CreateView: this 00000211376276F0
App.cpp(253): App::Initialize: this 00000211376276F0
App.cpp(154): App::InitializePlayback: ...
onecoreuap\base\appmodel\statemanager\winrt\lib\windows.storage.applicationdatafactory.server.cpp(235)\Windows.Storage.ApplicationData.dll!00007FFCFC138483: (caller: 00007FFCFC148CCB) ReturnHr(1) tid(977c) 8000000B The operation attempted to access data outside the valid range
    Msg:[User S-1-5-21-2072995867-3123792656-4078183186-1001] 
EmeFactory.h(520): media::eme::EmeFactory::EmeFactory: this 0000008999CFEE30
EmeFactory.h(531): media::eme::EmeFactory::requestMediaKeySystemAccess: this 0000008999CFEE30, keySystem com.microsoft.playready.recommendation, configurations.size() 1
EmeFactory.h(536): media::eme::EmeFactory::requestMediaKeySystemAccess: Before IMFMediaEngineClassFactory4::CreateContentDecryptionModuleFactory, this 0000008999CFEE30
App.cpp(269): App::SetWindow: this 00000211376276F0
App.cpp(285): App::Load: this 00000211376276F0, EntryPoint MediaEngineEMEUWPSample.App
App.cpp(289): App::Run: this 00000211376276F0
EmeFactory.h(539): media::eme::EmeFactory::requestMediaKeySystemAccess: this 0000008999CFEE30, IMFContentDecryptionModuleFactory m_cdmFactory 000002113A2020B0
EmeFactory.h(502): media::eme::MediaKeySystemAccess::createMediaKeys: Before IMFContentDecryptionModuleAccess::CreateContentDecryptionModule, this 00000211376775D0
EmeFactory.h(504): media::eme::MediaKeySystemAccess::createMediaKeys: this 00000211376775D0, IMFContentDecryptionModule cdm 000002113A211E80
EmeFactory.h(70): media::eme::MediaKeySession::MediaKeySession: Before IMFContentDecryptionModule::CreateSession, this 00000211376DBE80, SessionType MF_MEDIAKEYSESSION_TYPE_TEMPORARY
EmeFactory.h(72): media::eme::MediaKeySession::MediaKeySession: this 00000211376DBE80, IMFContentDecryptionModuleSession m_cdmSession 000002113A20B910
EmeFactory.h(152): media::eme::MediaKeySession::generateRequest: Before IMFContentDecryptionModule::GenerateRequest, this 00000211376DBE80, InitializationDataType cenc, InitializationData 852
EmeFactory.h(210): media::eme::MediaKeySession::SessionCallbackHelper::KeyMessage: this 00000211376E6470, Type MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST, DataSize 26334, Location "https://test.playready.microsoft.com/core/rightsmanager.asmx?cfg=(playenablers:(786627d8-c2a6-44be-8f88-08ae255b01a7),sl:150,ck:W31bfVt9W31bfVt9W31bfQ==,ckt:AES128BitCBC)"
App.cpp(90): App::HandleKeySessionMessage: Type MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST, Data.size() 26334, Location "https://test.playready.microsoft.com/core/rightsmanager.asmx?cfg=(playenablers:(786627d8-c2a6-44be-8f88-08ae255b01a7),sl:150,ck:W31bfVt9W31bfVt9W31bfQ==,ckt:AES128BitCBC)"
App.cpp(147): App::HandleKeySessionMessage: Type MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST, Response.Length() 1372
EmeFactory.h(160): media::eme::MediaKeySession::update: Before IMFContentDecryptionModule::Update, this 00000211376DBE80, ResponseData 1372
EmeFactory.h(221): media::eme::MediaKeySession::SessionCallbackHelper::KeyStatusChanged: this 00000211376E6470
Exception thrown at 0x00007FFD211CCF19 in MediaEngineEMEUWPSample.exe: Microsoft C++ exception: std::bad_function_call at memory location 0x000000899A4FED60.
Exception thrown at 0x00007FFD211CCF19 in MediaEngineEMEUWPSample.exe: Microsoft C++ exception: [rethrow] at memory location 0x0000000000000000.
C:\Project\github.com\media-foundation\samples\MediaEngineEMEUWPSample\src\EmeFactory.h(227)\(null)!00007FF7B68579FF: (caller: 00007FFCE22F506D) ReturnHr(1) tid(91b4) 8007023E {Application Error}
The exception %s (0x    Msg:[std::exception: bad function call] [media::eme::MediaKeySession::SessionCallbackHelper::KeyStatusChanged]
MediaEnginePlayer.h(264): media::MediaEnginePlayer::MediaEnginePlayer: ...
MediaEnginePlayer.h(273): media::MediaEnginePlayer::Initialize::<lambda_ad5c2a06b0441b4d851aafb47d98785d>::operator (): ...
onecore\windows\directx\database\helperlibrary\lib\perappusersettingsqueryimpl.cpp(121)\dxgi.dll!00007FFD1F5F1B34: (caller: 00007FFD1F5CE332) ReturnHr(1) tid(977c) 8000FFFF Catastrophic failure
onecore\windows\directx\database\helperlibrary\lib\perappusersettingsqueryimpl.cpp(98)\dxgi.dll!00007FFD1F5F1805: (caller: 00007FFD1F5BDF01) ReturnHr(2) tid(977c) 8000FFFF Catastrophic failure
onecore\windows\directx\database\helperlibrary\lib\directxdatabasehelper.cpp(1410)\dxgi.dll!00007FFD1F5F1925: (caller: 00007FFD1F5BDF01) ReturnHr(3) tid(977c) 8000FFFF Catastrophic failure
Exception thrown at 0x00007FFD211CCF19 in MediaEngineEMEUWPSample.exe: Microsoft C++ exception: std::runtime_error at memory location 0x0000008999CF18E0.
Exception thrown at 0x00007FFD211CCF19 in MediaEngineEMEUWPSample.exe: Microsoft C++ exception: std::runtime_error at memory location 0x0000008999CF18E0.
Exception thrown at 0x00007FFD211CCF19 in MediaEngineEMEUWPSample.exe: Microsoft C++ exception: std::runtime_error at memory location 0x0000008999CF18E0.
Exception thrown at 0x00007FFD211CCF19 in MediaEngineEMEUWPSample.exe: Microsoft C++ exception: Poco::NotFoundException at memory location 0x0000008999CF12D0.
Exception thrown at 0x00007FFD211CCF19 in MediaEngineEMEUWPSample.exe: Microsoft C++ exception: Poco::NotFoundException at memory location 0x0000008999CF1310.
MediaEnginePlayer.h(161): media::MediaEngineProtectionManager::MediaEngineProtectionManager: this 000002113A5133B0, pmpServerKey Windows.Media.Protection.MediaProtectionPMPServer
MediaEnginePlayer.h(238): media::MediaEngineProtectionManager::Properties: this 000002113A5133B0, m_PropertySet.Size() 1
MediaEnginePlayer.h(238): media::MediaEngineProtectionManager::Properties: this 000002113A5133B0, m_PropertySet.Size() 3
MediaEnginePlayer.h(238): media::MediaEngineProtectionManager::Properties: this 000002113A5133B0, m_PropertySet.Size() 3
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED, 0000000000000001, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_LOADSTART, 0000000000000000, 00000000
MediaEnginePlayer.h(90): media::MediaEngineExtension::BeginCreateObject: this 000002113A512070, Location "MediaEnginePlayer", ByteStream 0000000000000000, Type 1
C:\Project\github.com\media-foundation\samples\MediaEngineEMEUWPSample\src\MediaEnginePlayer.h(95)\(null)!00007FF7B6813211: (caller: 00007FFCC2363957) ReturnHr(2) tid(91b4) C00D36BB An unexpected error has occurred in the operation requested.    [media::MediaEngineExtension::BeginCreateObject(Type != MF_OBJECT_MEDIASOURCE)]
MediaEnginePlayer.h(90): media::MediaEngineExtension::BeginCreateObject: this 000002113A512070, Location "MediaEnginePlayer", ByteStream 0000000000000000, Type 0
MediaEnginePlayer.h(118): media::MediaEngineExtension::EndCreateObject: this 000002113A512070
MediaEnginePlayer.h(238): media::MediaEngineProtectionManager::Properties: this 000002113A5133B0, m_PropertySet.Size() 3
MediaEnginePlayer.h(238): media::MediaEngineProtectionManager::Properties: this 000002113A5133B0, m_PropertySet.Size() 3
EmeFactory.h(641): media::InputTrustAuthorityMediaSource::GetInputTrustAuthority: this 000002113AC7B500, StreamIdentifier 1, InterfaceIdentifier {D19F8E98-B126-4446-890C-5DCB7AD71453}
EmeFactory.h(641): media::InputTrustAuthorityMediaSource::GetInputTrustAuthority: this 000002113AC7B500, StreamIdentifier 1, InterfaceIdentifier {D19F8E98-B126-4446-890C-5DCB7AD71453}
EmeFactory.h(641): media::InputTrustAuthorityMediaSource::GetInputTrustAuthority: this 000002113AC7B500, StreamIdentifier 1, InterfaceIdentifier {D19F8E98-B126-4446-890C-5DCB7AD71453}
EmeFactory.h(641): media::InputTrustAuthorityMediaSource::GetInputTrustAuthority: this 000002113AC7B500, StreamIdentifier 1, InterfaceIdentifier {D19F8E98-B126-4446-890C-5DCB7AD71453}
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA, 0000000000000000, 00000000
MediaEnginePlayer.h(238): media::MediaEngineProtectionManager::Properties: this 000002113A5133B0, m_PropertySet.Size() 3
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_LOADEDDATA, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_CANPLAY, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_SUSPEND, 0000000000000000, 00000000
MediaEnginePlayer.h(345): media::MediaEnginePlayer::GetSurfaceHandle: ...
MediaEnginePlayer.h(349): media::MediaEnginePlayer::GetSurfaceHandle::<lambda_1583d36193bc19908f8166b67cb9a224>::operator (): ...
App.cpp(207): App::SetupVideoVisual: ...
EmeFactory.h(641): media::InputTrustAuthorityMediaSource::GetInputTrustAuthority: this 000002113AC7B500, StreamIdentifier 1, InterfaceIdentifier {D19F8E98-B126-4446-890C-5DCB7AD71453}
MediaEnginePlayer.h(301): media::MediaEnginePlayer::StartPlayingFrom: ...
MediaEnginePlayer.h(304): media::MediaEnginePlayer::StartPlayingFrom::<lambda_deeb2fa9907b6633895e658d2064a857>::operator (): ...
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_SEEKING, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_TIMEUPDATE, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_SEEKED, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_PLAY, 0000000000000000, 00000000
EmeFactory.h(593): media::InputTrustAuthorityMediaSource::Start: this 000002113AC7B500
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_WAITING, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED, 0000000000000000, 00000001
EmeFactory.h(641): media::InputTrustAuthorityMediaSource::GetInputTrustAuthority: this 000002113AC7B500, StreamIdentifier 1, InterfaceIdentifier {D19F8E98-B126-4446-890C-5DCB7AD71453}
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_WAITING, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED, 0000000000000000, 00000001
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_FORMATCHANGE, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY, 0000000000000000, 00000000
EmeFactory.h(603): media::InputTrustAuthorityMediaSource::Pause: this 000002113AC7B500
EmeFactory.h(593): media::InputTrustAuthorityMediaSource::Start: this 000002113AC7B500
Exception thrown at 0x00007FFD211CCF19 (KernelBase.dll) in MediaEngineEMEUWPSample.exe: WinRT originate error - 0x80040155 : 'Failed to find proxy registration for IID: {3041C46C-9A45-47C7-9E23-9C9E1E4D4D50}.'.
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_WAITING, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED, 0000000000000000, 00000001
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_PLAYING, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_TIMEUPDATE, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_TIMEUPDATE, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_TIMEUPDATE, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_TIMEUPDATE, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_TIMEUPDATE, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_TIMEUPDATE, 0000000000000000, 00000000
MediaEnginePlayer.h(391): media::MediaEnginePlayer::MediaEngineNotify::EventNotify: this 000002113A512C40, Code MF_MEDIA_ENGINE_EVENT_TIMEUPDATE, 0000000000000000, 00000000
EmeFactory.h(603): media::InputTrustAuthorityMediaSource::Pause: this 000002113AC7B500
The program '[21156] MediaEngineEMEUWPSample.exe' has exited with code 1 (0x1).
```