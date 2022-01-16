
cmake_minimum_required(VERSION 3.21.0)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 23)

project(TlgAccess2Json VERSION 0.1.0)

#we want to use UNICODE instead of the messy codepage stuff
add_definitions(-DUNICODE -D_UNICODE )

# this allows us to link with proper msvc runtime, if not we end up with the DLL runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Boost 1.77.0 REQUIRED COMPONENTS json)
find_package(nanodbc CONFIG REQUIRED)
find_package(ODBC REQUIRED)


message ("============================================")
message ("platform name: ${CMAKE_VS_PLATFORM_NAME}")
message ("boost lib dirs: ${Boost_LIBRARY_DIRS}")
message ("boost libs: ${Boost_LIBRARIES}")
message ("============================================")

set(TlgAccess2JsonSrc
                "Access2Json.cpp"
                "Platform.cpp"
                "Platform.h"
                "PrettyPrint.cpp"
                "PrettyPrint.h"
                "utf8Conversion.cpp"
                "utf8Conversion.h"
)

add_executable(TlgAccess2Json ${TlgAccess2JsonSrc} )
target_link_libraries(TlgAccess2Json PRIVATE  Boost::json  nanodbc ODBC::ODBC)

add_executable(JSon2Access  JSon2Access.cpp utf8Conversion.cpp)
target_link_libraries(JSon2Access PRIVATE  Boost::json  nanodbc ODBC::ODBC)

add_executable(ToText  ToText.cpp )
target_link_libraries(ToText PRIVATE  )

add_executable(OSql  OdbcPrimitive.cpp )
target_link_libraries(OSql PRIVATE  odbc32.lib)


add_executable(ShellEx  ShellEx.cpp )
#target_link_libraries(ShellEx PRIVATE)
