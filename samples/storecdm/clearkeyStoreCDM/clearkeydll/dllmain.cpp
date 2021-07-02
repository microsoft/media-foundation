//////////////////////////////////////////////////////////////////////////
//
// dllmain.cpp
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <initguid.h>
#include "guids.h"

using namespace Microsoft::WRL;

ActivatableClass( Cdm_clearkey_IMFContentDecryptionModuleFactory );
ActivatableClass( Cdm_clearkey_IMFTrustedInput );
ActivatableClass( Cdm_clearkey_IMFContentEnabler );

BOOL WINAPI DllMain( _In_opt_ HINSTANCE hInstance, _In_ DWORD dwReason, _In_opt_ LPVOID lpReserved )
{
    if( DLL_PROCESS_ATTACH == dwReason )
    {
        //
        //  Don't need per-thread callbacks
        //
        DisableThreadLibraryCalls( hInstance );

        Module<InProc>::GetModule().Create();
    }
    else if( DLL_PROCESS_DETACH == dwReason )
    {
        Module<InProc>::GetModule().Terminate();
    }

    return TRUE;
}

HRESULT WINAPI DllGetActivationFactory( _In_ HSTRING activatibleClassId, _COM_Outptr_ IActivationFactory** factory )
{
    auto &module = Microsoft::WRL::Module< Microsoft::WRL::InProc >::GetModule();
    return module.GetActivationFactory( activatibleClassId, factory );
}

HRESULT WINAPI DllCanUnloadNow()
{
    auto &module = Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule();    
    return (module.Terminate()) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject( _In_ REFCLSID rclsid, _In_ REFIID riid, _COM_Outptr_ LPVOID FAR* ppv )
{
    auto &module = Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule();
    return module.GetClassObject( rclsid, riid, ppv );
}

