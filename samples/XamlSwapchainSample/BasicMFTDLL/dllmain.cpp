// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "BasicMFTClass\BasicMFT.h"

CoCreatableClass(BasicMFT);

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        Module<InProc>::GetModule().Create();
        break;
    case DLL_PROCESS_DETACH:
        Module<InProc>::GetModule().Terminate();
        break;
    }
    return TRUE;
}

HRESULT __stdcall DllCanUnloadNow()
{
    auto& module = Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule();
    HRESULT hr = (module.Terminate()) ? S_OK : S_FALSE;
    return hr;
}

HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID* ppv)
{
    RETURN_HR_IF(E_POINTER, !ppv);
    RETURN_IF_FAILED(Module<ModuleType::InProc>::GetModule().GetClassObject(clsid, iid, ppv));
    return S_OK;
}
