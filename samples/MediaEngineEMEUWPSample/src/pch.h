// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <array>

#include <windows.h>

#include <d3d11_4.h>
#include <Audioclient.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfmediaengine.h>
#include <mfidl.h>
#include <mfcontentdecryptionmodule.h>

// Include ABI composition headers for interop with DCOMP surface handle from MediaEngine
#include <windows.ui.composition.h>
#include <windows.ui.composition.interop.h>

// Include prior to WinRT Headers
#include <wil/cppwinrt.h> 

// WinRT Headers
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.Media.Protection.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.ApplicationModel.h>

// Windows Implementation Library
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

// GSL (C++ Guidelines Support Library)
#include <gsl/span>

////////////////////////////////////////////////////////////////////////////////

struct SourceCodeReference
{
	SourceCodeReference(char const* File, int Line, char const* Function) :
		File(File),
		Line(Line),
		Function(Function)
	{
	}
	char const* FileName() const
	{
		if(NotShorten)
			return File;
		char const* Separator = strrchr(File, '\\');
		if(!Separator)
			return File;
		return Separator + 1;
	}

	char const* File;
	bool NotShorten = false;
	int Line;
	char const* Function;
};

struct TraceContext
{
	TraceContext(char const* File, int Line, char const* Function) :
		m_SourceCodeReference(File, Line, Function)
	{
	}
	explicit TraceContext(SourceCodeReference& SourceCodeReference) :
		m_SourceCodeReference(SourceCodeReference)
	{
	}

	void operator () (wchar_t const* Format, ...) const
	{
		va_list Arguments;
		va_start(Arguments, Format);
		wchar_t TextA[8u << 10] { };
		vswprintf_s(TextA, Format, Arguments);
		va_end(Arguments);
		wchar_t TextB[8u << 10] { };
		swprintf_s(TextB, L"%hs(%d): %hs: ", m_SourceCodeReference.FileName(), m_SourceCodeReference.Line, m_SourceCodeReference.Function);
		wcscat_s(TextB, TextA);
		OutputDebugStringW(TextB);
	}

	SourceCodeReference m_SourceCodeReference;
};

#if defined(DEVELOPMENT) || 1
	#define TRACE TraceContext(__FILE__, __LINE__, __FUNCTION__)
#else
	#define TRACE 1 ? 0 : 
#endif

inline std::wstring ToString(GUID const& Value)
{
    OLECHAR ValueString[40];
    WI_VERIFY(StringFromGUID2(Value, ValueString, static_cast<int>(std::size(ValueString))));
    return ValueString;
}

template <typename ValueType, size_t t_Size> 
inline std::string ToString(std::pair<ValueType, char const*> const (&Values)[t_Size], ValueType Value)
{
	for(auto&& ArrayValue: Values)
		if(ArrayValue.first == Value)
			return ArrayValue.second;
	return "";
}
