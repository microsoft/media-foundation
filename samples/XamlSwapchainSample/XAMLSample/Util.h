#pragma once

#include "pch.h"
#include <functional>

namespace util
{

    class MFCallbackBase : public winrt::implements<MFCallbackBase, IMFAsyncCallback>
    {
    public:
        MFCallbackBase(DWORD flags = 0, DWORD queue = MFASYNC_CALLBACK_QUEUE_MULTITHREADED) : m_flags(flags), m_queue(queue) {}

        DWORD GetQueue() const { return m_queue; }
        DWORD GetFlags() const { return m_flags; }

        // IMFAsyncCallback methods
        IFACEMETHODIMP GetParameters(_Out_ DWORD* flags, _Out_ DWORD* queue)
        {
            *flags = m_flags;
            *queue = m_queue;
            return S_OK;
        }

    private:
        DWORD m_flags = 0;
        DWORD m_queue = 0;
    };


    class MFWorkItem : public MFCallbackBase
    {
    public:
        MFWorkItem(std::function<void()> callback, DWORD flags = 0, DWORD queue = MFASYNC_CALLBACK_QUEUE_MULTITHREADED) : MFCallbackBase(flags, queue)
        {
            m_callback = callback;
        }

        // IMFAsyncCallback methods

        IFACEMETHODIMP Invoke(_In_opt_ IMFAsyncResult* /*result*/) noexcept override
            try
        {
            m_callback();
            return S_OK;
        }
        CATCH_RETURN();

    private:
        std::function<void()> m_callback;
    };

    inline void MFPutWorkItem(std::function<void()> callback)
    {
        winrt::com_ptr<MFWorkItem> workItem = winrt::make_self<MFWorkItem>(callback);
        THROW_IF_FAILED(MFPutWorkItem2(workItem->GetQueue(), 0, workItem.get(), nullptr));
    }

}