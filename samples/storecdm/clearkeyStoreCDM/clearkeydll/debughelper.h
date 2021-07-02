//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#pragma once

#include <windows.h>
#include <stdio.h>

#ifdef CLEARKEY_DEBUG_OUT
class TraceFunc
{
public:
    TraceFunc( char *pszFunc, HRESULT *phr, void *pThis )
    {
        m_pszFunc = pszFunc;
        m_phr = phr;
        m_qwThis = (QWORD)pThis;

        ZeroMemory( m_buff, sizeof( m_buff ) );
        sprintf_s( m_buff, "=>%s (0x%I64x)", m_pszFunc, m_qwThis );
        CLEARKEY_DEBUG_OUT( m_buff );
    }
    ~TraceFunc()
    {
        ZeroMemory( m_buff, sizeof( m_buff ) );
        if( m_phr == nullptr )
        {
            sprintf_s( m_buff, "<=%s (0x%I64x)", m_pszFunc, m_qwThis );
        }
        else
        {
            sprintf_s( m_buff, "<=%s 0x%x (0x%I64x)", m_pszFunc, *m_phr, m_qwThis );
        }
        CLEARKEY_DEBUG_OUT( m_buff );
    }
private:
    char m_buff[ 200 ];
    QWORD m_qwThis = 0;
    HRESULT *m_phr = nullptr;
    char *m_pszFunc = nullptr;
};

#define TRACE_FUNC() void *_pThis_ = nullptr; __if_exists(this){_pThis_=this;} TraceFunc _func_((char*)__FUNCTION__, nullptr, _pThis_)
#define TRACE_FUNC_HR(_hr_) void *_pThis_ = nullptr; __if_exists(this){_pThis_=this;} TraceFunc _func_((char*)__FUNCTION__, &(_hr_), _pThis_)
#else   //CLEARKEY_DEBUG_OUT
#define TRACE_FUNC()
#define TRACE_FUNC_HR(_hr_)
#endif  //CLEARKEY_DEBUG_OUT

