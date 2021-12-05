#pragma once

#include <string>

std::wstring Utf8ToWString(const std::string& str);
std::string  WStringToUtf8(const std::wstring& str);
std::string  Utf8ToCp1252(const std::string& utf8str);
std::string  Cp1252ToUtf8(const std::string& cp1252Str);
std::string  utf8ToLocale(const char* utf8str, const std::locale& loc);
