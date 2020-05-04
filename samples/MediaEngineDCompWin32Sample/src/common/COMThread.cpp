// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "COMThread.h"

namespace
{
    static DWORD WINAPI COMThreadProc(LPVOID parameter)
    {
        reinterpret_cast<COMThread*>(parameter)->Invoke();
        return 0;
    }
}

COMThread::COMThread(ApartmentType type, std::function<void(HANDLE)> function) : m_type(type), m_func(function)
{
    m_exitThread.create();
}

COMThread::~COMThread()
{
    m_exitThread.SetEvent();
    WaitForSingleObject(m_thread.get(), INFINITE);
}

void COMThread::Invoke()
{
    DWORD flags = (m_type == ApartmentType::MTA) ? COINIT_MULTITHREADED : COINIT_APARTMENTTHREADED;
    wil::unique_couninitialize_call unique_coinit = wil::CoInitializeEx_failfast(flags);
    if(!OnThreadLaunched())
    {
        return;
    }
    m_func(m_exitThread.get());
    return;
}

bool COMThread::OnThreadLaunched()
{
    if(!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &m_thread, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        return false;
    }
    m_threadLaunched.SetEvent();
    return true;
}

void COMThread::WaitForThreadLaunch()
{
    m_threadLaunched.wait();
}

std::unique_ptr<COMThread> COMThread::Create(COMThread::ApartmentType type, std::function<void(HANDLE)> function)
{
    std::unique_ptr<COMThread> comThread = std::make_unique<COMThread>(type, function);
    HANDLE threadHandle = CreateThread(nullptr, 0, COMThreadProc, static_cast<LPVOID>(comThread.get()), 0, nullptr);
    if(!threadHandle)
    {
        return nullptr;
    }
    comThread->WaitForThreadLaunch();
    return std::move(comThread);
}