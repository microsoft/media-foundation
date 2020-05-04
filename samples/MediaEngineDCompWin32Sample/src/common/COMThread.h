// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

// Helper class for executing provided std::function on a thread which is initialized with the specified COM apartment type (STA or MTA)
// The provided std::function must accept an exit event which is signaled when the thread is signaled to terminate (i.e when the COMThread object is destroyed)
class COMThread
{
public:
    enum class ApartmentType
    {
        None,
        STA,
        MTA
    };
    COMThread() {}
    COMThread(ApartmentType type, std::function<void(HANDLE)> function);
    virtual ~COMThread();

    static std::unique_ptr<COMThread> Create(COMThread::ApartmentType type, std::function<void(HANDLE)> function);

    void Invoke();
    void WaitForThreadLaunch();
    bool IsValid() const { return m_type != ApartmentType::None; }

private:
    wil::slim_event m_threadLaunched;
    wil::unique_event m_exitThread;
    wil::unique_handle m_thread;
    ApartmentType m_type = ApartmentType::None;
    std::function<void(HANDLE)> m_func;
    bool OnThreadLaunched();
};