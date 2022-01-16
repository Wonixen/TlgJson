#pragma once

#include <string>

// dealing with wide string in database
std::wstring  Utf8ToWString(const std::u8string& str);
std::u8string WStringToUtf8(const std::wstring& str);

// dealing with MsAccess code page 1252
std::string   Utf8ToCp1252(const std::u8string& utf8str);
std::u8string Cp1252ToUtf8(const std::string& cp1252Str);

// see: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1423r2.html
// until utf8 is widely supported!!
std::string from_u8string(const std::string& s);
std::string from_u8string(std::string&& s);
#if defined(__cpp_lib_char8_t)
std::string from_u8string(const std::u8string& s);
#endif