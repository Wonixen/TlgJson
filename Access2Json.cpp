/* Copyright(c) Jada Informatique 2021.

Jada Informatique Software License - Version 1.0 - june 27th, 2021

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <variant>
#include <vector>

#include <boost/json.hpp>
#include <nanodbc/nanodbc.h>

#include "Platform.h"
#include "PrettyPrint.h"


// json uses utf8, database has a mixture of wide, utf8 and code page string
// for now this code assume string in db is CP1252, but this is only for MsAccess


#include "utf8Conversion.h"

namespace json = boost::json;

struct TableExport
{
   std::string name;
   std::string extractQry;
};

std::vector<TableExport> g_tablesToExport =
   {
      {"Tags", "SELECT * FROM Tags ORDER BY Tag_Code"},
      {"Fields", "SELECT * FROM Fields ORDER BY Msg_Code, Tag_Code"},
      {"LogFiles", "SELECT * FROM LogFiles ORDER BY Log_Code"},
      {"Messages", "SELECT * FROM Messages ORDER BY Msg_Code"},
      {"LoggerMessages", "SELECT * FROM LoggerMessages ORDER BY Schema, Log_Code, Msg_Code"},
};

// dumping row to std::cerr for debugging purposes!
// not use anymore
void DumpResult(nanodbc::result& result, short BadCol)
{
   for (int i = 0; i < result.columns(); i++)
   {
      std::cerr << result.column_name(i) << "[";
      if (result.is_bound(i))
         std::cerr << ((result.is_null(i)) ? std::string {"NULL"} : ("'" + result.get<nanodbc::string>(i) + "'"));
      std::cerr << "] ";
   }
   std::cerr << "\n error with: " << result.column_name(BadCol) << std::endl;
}

struct ToJsonValue
{
   json::value& _v;

   ToJsonValue(json::value& v) :
      _v(v) {}
   void operator()(void*) const { _v = nullptr; }
   void operator()(int64_t i) const { _v = i; }
   void operator()(double d) const { _v = d; }
   void operator()(const std::string& s) const { _v = s; }
   void operator()(const std::vector<uint8_t>& v) const { _v = json::value_from(v); }
};

struct ToJsonArray
{
   json::array& _a;

   ToJsonArray(json::array& a) :
      _a(a) {}
   void operator()(void*) const { _a.push_back(nullptr); }
   void operator()(int64_t i) const { _a.push_back(i); }
   void operator()(double d) const { _a.push_back(d); }
   void operator()(const std::string& s) const
   {
      json::value v;

      v = s;
      _a.push_back(v);
   }
   void operator()(const std::vector<uint8_t>& v) const
   {
      _a.push_back(json::value_from(v));
   }
};

using DbValue = std::variant<void*, int64_t, std::string, double, std::vector<uint8_t>>;

DbValue GetColumnValue(nanodbc::result& row, short col)
{
   // for string conversion, msaccess uses:
   // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Nls\CodePage
   // which in tmx case is codepage 1252

   // only when the row is bound can we test for NULL!
   if (row.is_bound(col) && row.is_null(col))
      return DbValue {};

   // for long data type, must fetch then test for NULL
   // for short data type, we must test for null before fetching!
   switch (row.column_datatype(col))
   {
      case SQL_LONGVARBINARY:
      {
         // Unbound data can only be tested for null once fetched
         auto blob = row.get<std::vector<uint8_t>>(col);   // try to fetch blob
         if (row.is_null(col) == false)                    // did we get data ?
         {
            return blob;
         }
         return DbValue {};
      }

      // for MsAccess, SQL_CHAR is CP1252,
      // Json is utf8 !!
      // now c++20 has support fo u8string (utf8) but not nanodbc or boost json!!!
      // so here we do a conversion dance
      case SQL_VARCHAR:
      case SQL_CHAR:
      {
         return from_u8string(Cp1252ToUtf8(row.get<nanodbc::string>(col)));
      }

      case SQL_LONGVARCHAR:
      {
         auto varchar = row.get<nanodbc::string>(col);

         // only is it's not NULL can we use it, without the test the NULL column
         // would be converted to empty string!!!
         if (row.is_null(col) == false)
         {
            return from_u8string(Cp1252ToUtf8(varchar));
         }
         return DbValue {};
      }

      // we have unicode!
      case SQL_WCHAR:
      case SQL_WVARCHAR:
         return from_u8string(WStringToUtf8(row.get<nanodbc::wide_string>(col)));

      case SQL_WLONGVARCHAR:
      {
         auto longVarChar = row.get<nanodbc::wide_string>(col);
         if (row.is_null(col) == false)
            return from_u8string(WStringToUtf8(longVarChar));
         return DbValue {};
      }

      case SQL_BIGINT:
      case SQL_TINYINT:
      case SQL_INTEGER:
      case SQL_SMALLINT:
         return row.get<std::int64_t>(col);

      case SQL_FLOAT:
      case SQL_REAL:
      case SQL_DOUBLE:
         return row.get<double>(col);

      // for now, other types, ask odbc to get their string representation
      // will need to be fixed as needs arise!
      case SQL_NUMERIC:
      case SQL_DECIMAL:
      default:
         return from_u8string(Cp1252ToUtf8(row.get<std::string>(col)));
   }
}

json::object RowToJsonObject(nanodbc::result& row)
{
   json::object                                   rowData;
   std::vector<std::tuple<unsigned, std::string>> badCols;
   for (short colIdx = 0; colIdx < row.columns(); colIdx++)
   {
      auto&   jsonValue = rowData[row.column_name(colIdx)];
      DbValue dbValue;
      try
      {
         dbValue = GetColumnValue(row, colIdx);
      }
      catch (std::exception& ex)
      {
         badCols.push_back(std::make_tuple(colIdx, ex.what()));
      }
      std::visit(ToJsonValue {jsonValue}, dbValue);
   }
   // if we got bad cols, report them
   if (!badCols.empty())
   {
      std::string errorMsg = std::format("bad row: [{}]", json::serialize(rowData));
      for (auto badCol: badCols)
         errorMsg += std::format("\nError on column: {}, what: {}", row.column_name(std::get<0>(badCol)), std::get<1>(badCol));
      throw std::runtime_error(errorMsg);
   }

   return rowData;
}

// could be used for a smaller output
// at the expanse of readability & having to reconstruct objects
json::value GetStructureOfArray(nanodbc::result rowIt)
{
   json::object data;

   for (auto colIdx = 0; colIdx < rowIt.columns(); colIdx++)
   {
      data[rowIt.column_name(colIdx)] = json::array {};
   }

   int rowCount {0};

   while (rowIt.next())
   {
      rowCount++;

      for (auto colIdx = 0; colIdx < rowIt.columns(); colIdx++)
      {
         // get Array to populate
         auto& colArray = data[rowIt.column_name(colIdx)].as_array();
         auto  dbValue  = GetColumnValue(rowIt, colIdx);
         std::visit(ToJsonArray {colArray}, dbValue);
      }
   }

   json::object object;
   object["recordCount"] = rowCount;
   object["data"]        = data;

   return object;
}

json::array GetArrayOfStructure(nanodbc::result rowIt)
{
   json::array rows;
   while (rowIt.next())
   {
      rows.push_back(RowToJsonObject(rowIt));
   }
   return rows;
}

std::string readJsonFile(const std::string& filename)
{
   std::ifstream input(filename);
   return std::string((std::istreambuf_iterator<char>(input)),
                      std::istreambuf_iterator<char>());
}

int main(int argc, char** argv)
{
   std::cout << "Copyright © Jada Informatique 2021." << std::endl;
#if defined(_WIN32)
   #if defined(_WIN64)
   std::cout << "using Odbc 64 bits" << std::endl;
   #elif defined(_M_IX86)
   std::cout << "using Odbc 32 bits" << std::endl;
   #endif
#endif

   std::string database {argv[1]};
   auto        connection_string = "Driver={Microsoft Access Driver (*.mdb, *.accdb)};Dbq=" + database;

   try
   {
      nanodbc::connection conn(connection_string);

      SQLUINTEGER uIntVal {};
      SQLGetEnvAttr(conn.native_env_handle(), SQL_ATTR_ODBC_VERSION, static_cast<SQLPOINTER>(&uIntVal), static_cast<SQLINTEGER>(sizeof(uIntVal)), nullptr);
      std::cout << std::format("odbc version: {}", uIntVal) << std::endl;
      json::object jsonDoc;
      jsonDoc["version"] = "1.0.0";
      json::object tables;

      for (auto tableInfo: g_tablesToExport)
      {
         auto rowIt = nanodbc::execute(conn, tableInfo.extractQry);
         // an array of object is more verbose, but easier to visualise and diff
         tables[tableInfo.name] = GetArrayOfStructure(rowIt);

         // structure of array would be more memory friendly, but less intuitive
         // see https://en.wikipedia.org/wiki/AoS_and_SoA
         // tables[tableInfo.name] = GetStructureOfArray(rowIt);
      }
      jsonDoc["TlgSchema"] = tables;
      if (argc > 2)
      {
         std::ofstream fileOut(argv[2]);
         pretty_print(fileOut, jsonDoc);
      }
      else
      {
         pretty_print(std::cout, jsonDoc);
      }
   }
   catch (const std::exception& e)
   {
      std::cerr << e.what() << '\n';
   }
   return 0;
}
