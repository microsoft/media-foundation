//*@@@+++@@@@******************************************************************
//
// Microsoft Windows Media Foundation
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//*@@@---@@@@******************************************************************

#pragma once

#include <windows.h>

class PROPVARIANTWRAP : public PROPVARIANT
{
public:
    PROPVARIANTWRAP()
    {
        PropVariantInit( this );
    }
    ~PROPVARIANTWRAP()
    {
        (void)Clear();
    }
    HRESULT Clear()
    {
        return PropVariantClear( this );
    }
};

#pragma pack(push, 1)
struct OriginatorId
{
    BYTE   rgbITAId[ 12 ];
    UINT32 uiStreamId;
};
#pragma pack(pop)

