#include <windows.h>

#include <cstdio>
#include <locale>

class SwitchConsoleToUtf8
{
   UINT m_ConsoleCp {};

public:
   SwitchConsoleToUtf8()
   {
      m_ConsoleCp = GetConsoleOutputCP();
      SetConsoleOutputCP(CP_UTF8);
      setvbuf(stdout, nullptr, _IOFBF, 1000);
   }
   ~SwitchConsoleToUtf8()
   {
      fflush(stdout);
      SetConsoleOutputCP(m_ConsoleCp);
   }
};

static SwitchConsoleToUtf8 ToUtf8;

std::locale&
   GetConsoleLoc()
{
   // the console is using codepage defined at:
   // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Nls\CodePage\OEMCP
   // which happens to be 850 in North America

   static std::locale locConsole(".850");
   return locConsole;
}
