// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "common/COMThread.h"

namespace ui
{

// RAII abstract class for managing lifetime of window class registrations
class ScopedWindowClass
{
  public:
    ScopedWindowClass() {}
    virtual ~ScopedWindowClass() {}
    static void ReleaseWindowClass(ScopedWindowClass* windowClass);
    void AddRef();
    void Release();
    virtual void RegisterClass() = 0;
    virtual void UnregisterClass() = 0;
    HMODULE GetInstanceModule() { return m_instance; }

  protected:
    uint32_t m_refCount = 0;
    ATOM m_windowClass = NULL;
    HMODULE m_instance = nullptr;
};

typedef wil::unique_any<ScopedWindowClass*, decltype(&ScopedWindowClass::ReleaseWindowClass), ScopedWindowClass::ReleaseWindowClass>
    unique_windowclass;

// This class handles creation and management of the underlying system window used with direct composition.
// It also manages creation and updates to the direct composition visual tree.
class DirectCompositionWindow
{
  public:
    typedef std::function<void(uint32_t, uint32_t)> WindowSizeChangedCB;

    DirectCompositionWindow(HWND parentWindow, WindowSizeChangedCB sizeChangedCB, std::function<void()> windowClosedCB)
        : m_parentWindow(parentWindow), m_sizeChangedCB(sizeChangedCB), m_windowClosedCB(windowClosedCB)
    {
    }
    virtual ~DirectCompositionWindow();

    void Initialize();
    HWND GetHandle();
    IDCompositionDevice2* GetDevice();
    IDCompositionVisual2* GetRootVisual();
    void CommitUpdates();
    void SetRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

  private:
    HWND m_parentWindow = nullptr;
    WindowSizeChangedCB m_sizeChangedCB;
    std::function<void()> m_windowClosedCB;
    HRESULT m_initResult = S_OK;
    std::unique_ptr<COMThread> m_windowThread;
    wil::unique_hwnd m_window;
    unique_windowclass m_windowClass;
    winrt::com_ptr<IDCompositionDevice2> m_dcompDevice;
    winrt::com_ptr<IDCompositionTarget> m_dcompTarget;
    winrt::com_ptr<IDCompositionVisual2> m_rootVisual;

    void InitializeWindowThread();
    HRESULT WindowThreadMain(HANDLE initCompleteEvent, HANDLE exitEvent);
    void InitializeDirectComposition();
};

} // namespace ui