#include <algorithm>
#include <fstream>
#include <iostream>
#include <variant>
#include <vector>

#include <boost/json.hpp>
#include <nanodbc/nanodbc.h>

#include "PrettyPrint.h"

// clang-format off
#include <windows.h>
#include <sqltypes.h>
#include <sqlucode.h>
// clang-format on

namespace json = boost::json;

struct TableExport
{
   std::string name;
   std::string extractQry;
};

std::vector<TableExport> g_tablesToExport = {
   {"Tags", "SELECT top 1 * FROM Tags ORDER BY Tag_Code"},
   {"Fields", "SELECT top 1 * FROM Fields ORDER BY Msg_Code, Tag_Code"},
   {"LogFiles", "SELECT top 1 * FROM LogFiles ORDER BY Log_Code"},
   {"Messages", "SELECT top 1 * FROM Messages ORDER BY Msg_Code"},
   {"LoggerMessages", "SELECT top 1 Log_Code, Msg_Code, Schema FROM LoggerMessages WHERE  Header_Msg_Code IS NULL ORDER BY Schema, Log_Code, Msg_Code"},
   {"HeaderMessages",
    "SELECT top 1 Header_Msg_Code as Header_Code, Schema, Log_Code, Msg_Code FROM LoggerMessages WHERE  Header_Msg_Code IS NOT NULL ORDER BY Header_Msg_Code, Schema, Log_Code, Msg_Code"},
};

std::string convertWideToUtf8(const nanodbc::wide_string& wData)
{
   if (wData.empty())
      return std::string {};

   auto utf8Len = ::WideCharToMultiByte(CP_UTF8, 0, wData.c_str(), wData.size(), nullptr, 0, nullptr, nullptr);
   if (utf8Len == 0)
   {
      throw std::runtime_error("can't convert to utf8 string");
   }

   std::string utf8Data(utf8Len, '\0');

   if (::WideCharToMultiByte(CP_UTF8, 0, wData.c_str(), wData.size(), &utf8Data[0], utf8Data.size(), nullptr, nullptr) == 0)
   {
      throw std::runtime_error("can't convert from wide to utf8 string");
   }

   return utf8Data;
}

//
std::string convert1252ToUtf8(const std::string& code1252)
{
   if (code1252.empty())
      return std::string {};

   auto wideLen = ::MultiByteToWideChar(CP_ACP, 0, code1252.c_str(), code1252.size(), nullptr, 0);
   if (wideLen == 0)
   {
      throw std::runtime_error("can't convert from codepage 1252 to wstring");
   }

   nanodbc::wide_string wideString(wideLen, L'\0');

   if (::MultiByteToWideChar(CP_ACP, 0, code1252.c_str(), code1252.size(), &wideString[0], wideString.size()) == 0)
   {
      throw std::runtime_error("can't convert to wstring");
   }
   return convertWideToUtf8(wideString);
}

void DumpResult(nanodbc::result& result, short BadCol)
{
   for (int i = 0; i < result.columns(); i++)
   {
      std::cout << result.column_name(i) << "[";
      if (result.is_bound(i))
         std::cout << ((result.is_null(i)) ? std::string {"NULL"} : ("'" + result.get<nanodbc::string>(i) + "'"));
      std::cout << "] ";
   }
   std::cout << "\n error with: " << result.column_name(BadCol) << std::endl;
}

struct ToJsonValue
{
   json::value& _v;

   ToJsonValue(json::value& v) : _v(v) {}
   void operator()(void*) const { _v = nullptr; }
   void operator()(int64_t i) const { _v = i; }
   void operator()(double d) const { _v = d; }
   void operator()(const std::string& s) const { _v = s; }
   void operator()(const std::vector<uint8_t>& v) const { _v = json::value_from(v); }
};

struct ToJsonArray
{
   json::array& _a;

   ToJsonArray(json::array& a) : _a(a) {}
   void operator()(void*) const { _a.push_back(nullptr); }
   void operator()(int64_t i) const { _a.push_back(i); }
   void operator()(double d) const { _a.push_back(d); }
   void operator()(const std::string& s) const
   {
      json::value v;

      v = s;
      _a.push_back(v);
   }
   void operator()(const std::vector<uint8_t>& v) const { _a.push_back(json::value_from(v)); }
};

using DbValue = std::variant<void*, int64_t, std::string, double, std::vector<uint8_t>>;

DbValue GetColumnValue(nanodbc::result& row, short col)
{
   DbValue dbData;
   if (row.is_bound(col) && row.is_null(col))
      return dbData;

   // for long data type, must fetch then test for NULL
   // for short data type, we must test for null before fetching!
   switch (row.column_datatype(col))
   {
      case SQL_LONGVARBINARY:
      {
         // Unbound data can only be tested for null once fetched
         auto blob = row.get<std::vector<uint8_t>>(col);   // try to fetch blob
         if (row.is_null(col) == false)                    // did we get data ?
            dbData = blob;
         return dbData;
      }

      case SQL_LONGVARCHAR:
      {
         // Unbound data can only be tested for null once fetched
         auto longText = row.get<nanodbc::string>(col);   // try to fetch long text
         if (row.is_null(col) == false)                   // got something!!!
            dbData = convert1252ToUtf8(longText);         // long text appear as codepage 1252, convert to utf8 which is what json understand
         return dbData;
      }

      case SQL_BIGINT:
      case SQL_TINYINT:
      case SQL_INTEGER:
      case SQL_SMALLINT:
         dbData = row.get<std::int64_t>(col);
         return dbData;

      case SQL_VARCHAR:
      case SQL_CHAR:
         dbData = convertWideToUtf8(row.get<nanodbc::wide_string>(col));
         return dbData;

      case SQL_NUMERIC:
      case SQL_DECIMAL:
      case SQL_FLOAT:
      case SQL_REAL:
      case SQL_DOUBLE:
         dbData = row.get<double>(col);
         return dbData;

      default:
         dbData = convertWideToUtf8(row.get<nanodbc::wide_string>(col));
         return dbData;
   }
}

json::object RowToJsonObject(nanodbc::result& row)
{
   json::object rowData;
   for (short colIdx = 0; colIdx < row.columns(); colIdx++)
   {
      auto& jsonValue = rowData[row.column_name(colIdx)];
      auto  dbValue   = GetColumnValue(row, colIdx);
      std::visit(ToJsonValue {jsonValue}, dbValue);
   }

   return rowData;
}


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

int main(int argc, char** argv)
{
   std::string database {argv[1]};
   auto        connection_string = "Driver={Microsoft Access Driver (*.mdb, *.accdb)};Dbq=" + database;

   try
   {
      nanodbc::connection conn(connection_string);

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

      std::ostream* out = &std::cout;

      if (argc > 2)
      {
         static std::ofstream file(argv[2]);
         out = &file;
      }

      pretty_print(*out, jsonDoc);
   }
   catch (const std::exception& e)
   {
      std::cerr << e.what() << '\n';
   }

   return 0;
}
