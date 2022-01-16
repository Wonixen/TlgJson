#include "utf8Conversion.h"

#include <codecvt>
#include <locale>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
   // access is using this code page
   std::locale loc1252(".1252");

   std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
}   // namespace

std::wstring Utf8ToWString(const std::u8string& str)
{
   return myconv.from_bytes(std::string {str.cbegin(), str.cend()});
}

// convert wstring to UTF-8 string
std::u8string WStringToUtf8(const std::wstring& wstr)
{
   auto narrow = myconv.to_bytes(wstr);
   return std::u8string(narrow.cbegin(), narrow.cend());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string Utf8ToCp1252(const std::u8string& utf8str)
{
   // convert to wide string!
   auto wstr = Utf8ToWString(utf8str);

   auto& f = std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t>>(loc1252);

   std::mbstate_t mb {};
   std::string    cp1252Str(wstr.size() * f.max_length(), '\0');
   const wchar_t* fromNext {};
   char*          toNext {};
   f.out(mb, &wstr[0], &wstr[wstr.size()], fromNext, &cp1252Str[0], &cp1252Str[cp1252Str.size()], toNext);
   // error checking skipped for brevity
   cp1252Str.resize(toNext - &cp1252Str[0]);

   return cp1252Str;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::u8string Cp1252ToUtf8(const std::string& cp1252Str)
{
   std::wstring wstr(cp1252Str.size(), L'\0');
   use_facet<std::ctype<wchar_t>>(loc1252).widen(&cp1252Str[0], &cp1252Str[cp1252Str.size()], &wstr[0]);

   auto result = WStringToUtf8(wstr);
   return result;
}

std::string Utf8ToLocale(const char8_t* utf8str, const std::locale& loc)
{
   // UTF-8 to wstring
   std::wstring_convert<std::codecvt_utf8<wchar_t>> wconv;

   std::wstring wstr = wconv.from_bytes(reinterpret_cast<const char*>(utf8str));
   // wstring to string
   std::vector<char> buf(wstr.size());
   use_facet<std::ctype<wchar_t>>(loc).narrow(wstr.data(), wstr.data() + wstr.size(), '?', buf.data());
   return std::string(buf.data(), buf.size());
}

std::string from_u8string(const std::string& s)
{
   return s;
}
std::string from_u8string(std::string&& s)
{
   return std::move(s);
}
#if defined(__cpp_lib_char8_t)
std::string from_u8string(const std::u8string& s)
{
   return std::string(s.begin(), s.end());
}
#endif
