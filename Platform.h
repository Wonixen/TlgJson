#include <locale>

#if defined(_DLL)
   #error not ready for DLL
#endif

#if !defined(_MT)
   #error should be multithread!
#endif


#if defined(_WIN64) || defined(_WIN32)
// clang-format off

#include <windows.h>
#include <sqltypes.h>
#include <sqlucode.h>
// clang-format on

#endif

std::locale& GetConsoleLoc();
