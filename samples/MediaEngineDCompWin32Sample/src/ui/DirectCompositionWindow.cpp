// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "DirectCompositionWindow.h"

using namespace Microsoft::WRL;

namespace ui
{

void ScopedWindowClass::ReleaseWindowClass(ScopedWindowClass* windowClass) { windowClass->Release(); }

void ScopedWindowClass::AddRef()
{
    if(m_refCount == 0)
    {
        RegisterClass();
    }
    m_refCount++;
}

void ScopedWindowClass::Release()
{
    m_refCount--;
    if(m_refCount == 0)
    {
        if(m_windowClass != NULL)
        {
            UnregisterClass();
            m_windowClass = NULL;
            m_instance = nullptr;
        }
    }
}

namespace
{
    LRESULT CALLBACK DirectCompositionWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    constexpr wchar_t c_DirectCompositionWindowClass[] = L"DirectCompositionWindowClass";

    void GetModuleFromWindowProc(WNDPROC windowProc, HMODULE* instance)
    {
        *instance = nullptr;
        THROW_LAST_ERROR_IF(!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<wchar_t*>(windowProc), instance));
    }

    // Class to manage lifetime of window class registration
    class DirectCompositionWindowClass : public ScopedWindowClass
    {
      public:
        DirectCompositionWindowClass() {}
        ~DirectCompositionWindowClass() {}

        static void GetInstance(ScopedWindowClass** instance)
        {
            s_instance.AddRef();
            *instance = &s_instance;
        }

        void RegisterClass() override
        {
            // Register window class for DirectCompositionWindow
            WNDCLASSEXW windowClassInit = {};
            GetModuleFromWindowProc(DirectCompositionWindowProc, &m_instance);

            windowClassInit.cbSize = sizeof(WNDCLASSEX);
            windowClassInit.style = CS_HREDRAW | CS_VREDRAW;
            windowClassInit.lpfnWndProc = DirectCompositionWindowProc;
            windowClassInit.hInstance = m_instance;
            windowClassInit.hCursor = LoadCursor(nullptr, IDC_ARROW);
            windowClassInit.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            windowClassInit.lpszClassName = c_DirectCompositionWindowClass;

            m_windowClass = RegisterClassExW(&windowClassInit);
            THROW_LAST_ERROR_IF(!m_windowClass);
        }

        void UnregisterClass() override { ::UnregisterClass(c_DirectCompositionWindowClass, m_instance); }

      private:
        static DirectCompositionWindowClass s_instance;
    };

    DirectCompositionWindowClass DirectCompositionWindowClass::s_instance = DirectCompositionWindowClass();

    // Window proc method for processing messages for DirectCompositionWindow
    LRESULT CALLBACK DirectCompositionWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        DirectCompositionWindow* window = reinterpret_cast<DirectCompositionWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        RETURN_HR_IF_NULL(E_POINTER, window);
        return window->WindowProc(message, wParam, lParam);
    }
} // namespace

void DirectCompositionWindow::Initialize()
{
    InitializeWindowThread();
    InitializeDirectComposition();
}

DirectCompositionWindow::~DirectCompositionWindow()
{
    if(m_window)
    {
        // Send exit message to window to ensure that window thread exits
        PostMessage(m_window.get(), WM_QUIT, 0, 0);
    }
}

HWND DirectCompositionWindow::GetHandle() { return m_window.get(); }

HRESULT DirectCompositionWindow::WindowThreadMain(HANDLE initCompleteEvent, HANDLE /*exitEvent*/)
try
{
    auto setInitEventSentinel = wil::scope_exit([&]() { SetEvent(initCompleteEvent); });

    // For cases where a parent window is specified, configure the DirectCompositionWindow as a child without extra chrome
    // Otherwise, configure it as an independent top-level window.
    int windowWidth = 0;
    int windowHeight = 0;
    uint32_t windowStylesEx = 0;
    uint32_t windowStyles = 0;

    if(m_parentWindow)
    {
        RECT clientRect = {};
        THROW_LAST_ERROR_IF(!GetClientRect(m_parentWindow, &clientRect));
        windowWidth = clientRect.right;
        windowHeight = clientRect.bottom;
        windowStylesEx = WS_EX_NOPARENTNOTIFY | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOREDIRECTIONBITMAP;
        windowStyles |= (WS_CHILDWINDOW | WS_DISABLED | WS_CLIPSIBLINGS);
    }
    else
    {
        windowWidth = CW_USEDEFAULT;
        windowHeight = CW_USEDEFAULT;
        windowStylesEx = 0;
        windowStyles |= WS_OVERLAPPEDWINDOW;
    }

    DirectCompositionWindowClass::GetInstance(&m_windowClass);
    m_window = wil::unique_hwnd(CreateWindowEx(windowStylesEx, c_DirectCompositionWindowClass, L"", windowStyles, 0, 0, windowWidth,
                                               windowHeight, m_parentWindow, nullptr, m_windowClass.get()->GetInstanceModule(), nullptr));
    THROW_LAST_ERROR_IF(!m_window);

    // Store a pointer to this object in the window to allow the window proc to access it
    SetWindowLongPtr(m_window.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    setInitEventSentinel.reset();

    // Pump window messages
    MSG message;
    while(GetMessage(&message, nullptr, 0, 0))
    {
        DispatchMessage(&message);
    }

    return S_OK;
}
CATCH_RETURN();

LRESULT DirectCompositionWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_SIZE:
        {
            RECT windowRect = {};
            if(GetClientRect(m_window.get(), &windowRect) && m_sizeChangedCB)
            {
                m_sizeChangedCB(windowRect.right, windowRect.bottom);
            }
            break;
        }
        case WM_CLOSE:
            if(m_windowClosedCB)
            {
                m_windowClosedCB();
            }
            break;
        default:
            return DefWindowProc(m_window.get(), message, wParam, lParam);
    }
    return 0;
}

void DirectCompositionWindow::InitializeWindowThread()
{
    wil::unique_event initEvent;
    initEvent.create();

    // Create STA window thread
    m_windowThread =
        COMThread::Create(COMThread::ApartmentType::STA, [&](HANDLE exitEvent) { m_initResult = WindowThreadMain(initEvent.get(), exitEvent); });

    THROW_LAST_ERROR_IF(WaitForSingleObject(initEvent.get(), INFINITE) == WAIT_FAILED);
    THROW_IF_FAILED(m_initResult);
}

void DirectCompositionWindow::InitializeDirectComposition()
{
    // Create a D3D11 device and use it to create a direct composition device
    winrt::com_ptr<ID3D11Device> d3d11Device;
    UINT creationFlags = (D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS);
    constexpr D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                                      D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
                                                      D3D_FEATURE_LEVEL_9_1};
    THROW_IF_FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, creationFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
                          d3d11Device.put(), nullptr, nullptr));
    winrt::com_ptr<IDXGIDevice> dxgiDevice = d3d11Device.as<IDXGIDevice>();
    winrt::com_ptr<IDCompositionDevice> dcompDevice;
    THROW_IF_FAILED(DCompositionCreateDevice2(dxgiDevice.get(), IID_PPV_ARGS(dcompDevice.put())));
    m_dcompDevice = dcompDevice.as<IDCompositionDevice2>();

    // Create target against HWND
    winrt::com_ptr<IDCompositionDesktopDevice> desktopDevice = m_dcompDevice.as < IDCompositionDesktopDevice>();
    THROW_IF_FAILED(desktopDevice->CreateTargetForHwnd(m_window.get(), TRUE, m_dcompTarget.put()));

    // Create root visual and set it on the target
    THROW_IF_FAILED(m_dcompDevice->CreateVisual(m_rootVisual.put()));
    THROW_IF_FAILED(m_dcompTarget->SetRoot(m_rootVisual.get()));
}

IDCompositionDevice2* DirectCompositionWindow::GetDevice() { return m_dcompDevice.get(); }

IDCompositionVisual2* DirectCompositionWindow::GetRootVisual() { return m_rootVisual.get(); }

void DirectCompositionWindow::CommitUpdates() { THROW_IF_FAILED(m_dcompDevice->Commit()); }

void DirectCompositionWindow::SetRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    THROW_HR_IF(E_UNEXPECTED, !m_window);
    THROW_LAST_ERROR_IF(!SetWindowPos(m_window.get(), HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW));
    THROW_LAST_ERROR_IF(!UpdateWindow(m_window.get()));
}

} // namespace ui