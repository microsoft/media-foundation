#pragma once

#include <string>

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
		char const* Separator = strrchr(File, '\\');
		if(!Separator)
			return File;
		return Separator + 1;
	}

	char const* File;
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

#if !defined(NDEBUG) || 1
	#define TRACE TraceContext(__FILE__, __LINE__, __FUNCTION__)
#else
	#define TRACE(...) 1 ? 0 : std::ignore = std::make_tuple(__VA_ARGS__)
#endif

template <typename ValueType, size_t t_ItemCount>
inline std::string ToString(std::pair<ValueType, char const* const> const (&Items)[t_ItemCount], ValueType Value)
{
    auto const Iterator = std::find_if(Items, Items + t_ItemCount, [=] (auto&& Item) { return Item.first == Value; });
    return (Iterator != Items + t_ItemCount) ? Iterator->second : std::to_string(Value);
}

inline std::wstring ToString(GUID const& Value)
{
    OLECHAR ValueString[40];
    WI_VERIFY(StringFromGUID2(Value, ValueString, static_cast<int>(std::size(ValueString))));
    return ValueString;
}

template <size_t t_ItemCount>
inline std::wstring ToString(std::pair<GUID const*, wchar_t const* const> const (&Items)[t_ItemCount], GUID const& Value)
{
    auto const Iterator = std::find_if(Items, Items + t_ItemCount, [=] (auto&& Item) { return *Item.first == Value; });
    return (Iterator != Items + t_ItemCount) ? Iterator->second : ToString(Value);
}
