cmake_minimum_required(VERSION 3.21.0)
project(TlgAccess2Json VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 23)

include ("D:/devtools/vcpkg/scripts/buildsystems/vcpkg.cmake")

find_package(nanodbc CONFIG REQUIRED)
set(Boost_USE_STATIC_LIBS   ON)
find_package(Boost 1.77 REQUIRED COMPONENTS json)

add_definitions(-DUNICODE -D_UNICODE)

find_package(ODBC REQUIRED)

add_executable(TlgAccess2Json
               Access2Json.cpp
               PrettyPrint.cpp
               PrettyPrint.h
               )
target_link_libraries(TlgAccess2Json PRIVATE  ${Boost_LIBRARIES}  nanodbc ODBC::ODBC)

add_executable(JSon2Access  JSon2Access.cpp)
target_link_libraries(JSon2Access PRIVATE  ${Boost_LIBRARIES}  nanodbc ODBC::ODBC)
