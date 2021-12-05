#include "utf8Conversion.h"

#include <codecvt>
#include <locale>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
   std::locale loc1252(".1252");

   std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
}   // namespace

std::wstring Utf8ToWString(const std::string& str)
{
   return myconv.from_bytes(str);
}

// convert wstring to UTF-8 string
std::string WStringToUtf8(const std::wstring& wstr)
{
   return myconv.to_bytes(wstr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string Utf8ToCp1252(const std::string& utf8str)
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

std::string Cp1252ToUtf8(const std::string& cp1252Str)
{
   std::wstring wstr(cp1252Str.size(), L'\0');
   use_facet<std::ctype<wchar_t>>(loc1252).widen(&cp1252Str[0], &cp1252Str[cp1252Str.size()], &wstr[0]);

   auto result = WStringToUtf8(wstr);
   return result;
}

std::string utf8ToLocale(const char* utf8str, const std::locale& loc)
{
   // UTF-8 to wstring
   std::wstring_convert<std::codecvt_utf8<wchar_t>> wconv;

   std::wstring wstr = wconv.from_bytes(utf8str);
   // wstring to string
   std::vector<char> buf(wstr.size());
   use_facet<std::ctype<wchar_t>>(loc).narrow(wstr.data(), wstr.data() + wstr.size(), '?', buf.data());
   return std::string(buf.data(), buf.size());
}
