#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

#include <boost/json.hpp>
#include <nanodbc/nanodbc.h>

#include <windows.h>

#include <sqlext.h>

namespace json = boost::json;

struct TableExport
{
   std::string name;
   std::string extractQry;
};

std::vector<TableExport> g_tablesToExport =
   {{"Tags", "SELECT * FROM Tags ORDER BY Tag_Code"},
    {"Fields", "SELECT * FROM Fields ORDER BY Msg_Code, Tag_Code"},
    {"LogFiles", "SELECT * FROM LogFiles ORDER BY Log_Code"},
    {"Messages", "SELECT * FROM Messages ORDER BY Msg_Code"},
    {"LoggerMessages", "SELECT Log_Code, Msg_Code, Schema FROM LoggerMessages WHERE  Header_Msg_Code IS NULL ORDER BY Schema, Log_Code, Msg_Code"},
    {"HeaderMessages", "SELECT Header_Msg_Code as Header_Code, Schema, Log_Code, Msg_Code FROM LoggerMessages WHERE  Header_Msg_Code IS NOT NULL ORDER BY Header_Msg_Code, Schema, Log_Code, Msg_Code"}};

json::value toJsonValue(int colNb, nanodbc::result& row)
{
   if (row.is_null(colNb))
      return json::value {nullptr};

   switch (row.column_datatype(colNb))
   {
      case SQL_LONGVARCHAR:
      case SQL_CHAR:
      case SQL_VARCHAR:
         return json::value {row.get<nanodbc::string>(colNb)};

      case SQL_BIT:
         return json::value {row.get<int>(colNb)};

      // SQL_FLOAT is "driver defined" could be 32 bit, 64 bit, 80 bit whatever!! use double
      case SQL_FLOAT:
         return json::value {row.get<double>(colNb)};

      case SQL_DOUBLE:
         return json::value {row.get<double>(colNb)};
      case SQL_REAL:
         return json::value {row.get<float>(colNb)};

      default:
         return json::value {row.get<nanodbc::string>(colNb)};
   }
}

std::string convertWideToUtf8(nanodbc::wide_string wData)
{
   std::string utf8Data(1000 + wData.size() * 2, '\0');

   auto convertedLen = ::WideCharToMultiByte(CP_UTF8, 0, wData.c_str(), wData.size() + 1, &utf8Data[0], utf8Data.size(), NULL, NULL);
   if (convertedLen == 0)
   {
      throw std::runtime_error("can't convert to utf8 string");
   }

   std::string result(&utf8Data[0], &utf8Data[convertedLen - 1]);
   return result;
}
//
std::string convert1252ToUtf8(std::string code1252)
{
   std::vector<wchar_t> outBuff(1000 + code1252.size() * 2, L'\0');

   auto convertedLen = ::MultiByteToWideChar(CP_ACP, 0, code1252.c_str(), code1252.size() + 1, &outBuff[0], outBuff.size());
   if (convertedLen == 0)
   {
      throw std::runtime_error("can't convert to wstring");
   }
   std::vector<char> utf8Data(outBuff.size(), '\0');

   convertedLen = ::WideCharToMultiByte(CP_UTF8, 0, &outBuff[0], convertedLen, &utf8Data[0], utf8Data.size(), NULL, NULL);
   if (convertedLen == 0)
   {
      throw std::runtime_error("can't convert to utf8 string");
   }

   std::string result(&utf8Data[0], &utf8Data[convertedLen - 1]);
   return result;
}

std::string GetUtf8StringFromResult(nanodbc::result& results, short i)
{
   try
   {
      return convertWideToUtf8(results.get<nanodbc::wide_string>(i));
   }
   catch (std::exception& ex)
   {
      for (short idx = 0; idx < results.columns(); idx++)
      {
         if (results.is_bound(idx) && results.is_null(idx) == false)
            std::cout << results.column_name(idx) << ": [" << results.get<nanodbc::string>(idx) << "]";
      }
      std::cout << "\n" << ex.what() << " on column: " << results.column_name(i) << std::endl;
      throw;
   }
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

json::object RowToJsonObject(nanodbc::result& results)
{
   json::object rowData;
   const short  columns = results.columns();
   for (short i = 0; i < columns; ++i)
   {
      auto colName = results.column_name(i);
      // for long data type, must fetch then test for NULL
      // for short data type, we must test for null before fetching!
      switch (results.column_datatype(i))
      {
         case SQL_LONGVARBINARY:
         {
            // Unbound data can only be tested for null once fetched
            auto blob = results.get<std::vector<uint8_t>>(i);   // try to fetch blob
            if (results.is_null(i) == false)                    // got data
            {
               rowData[colName] = json::value_from(blob);
            }
            else
            {
               rowData[colName] = nullptr;
            }
            break;
         }

         case SQL_LONGVARCHAR:
         {
            // Unbound data can only be tested for null once fetched
            auto longText = results.get<nanodbc::string>(i);   // try to fetch long text
            if (results.is_null(i) == false)                   // got something!!!
            {
               rowData[colName] = convert1252ToUtf8(longText);
            }
            else
            {
               rowData[colName] = nullptr;
            }
            break;
         }

         default:
         {
            if (results.is_null(i) == false)
            {
               rowData[colName] = convertWideToUtf8(results.get<nanodbc::wide_string>(i));
            }
            else
            {
               rowData[colName] = nullptr;
            }
            break;
         }
      }
   }

   return rowData;
}

constexpr int json_indent = 2;
void          pretty_print(std::ostream& os, json::value const& jv, std::string* indent = nullptr)
{
   std::string indent_;
   if (!indent)
      indent = &indent_;
   switch (jv.kind())
   {
      case json::kind::object:
      {
         os << "{\n";
         indent->append(json_indent, ' ');
         auto const& obj = jv.get_object();
         if (!obj.empty())
         {
            auto it = obj.begin();
            for (;;)
            {
               os << *indent << json::serialize(it->key()) << " : ";
               pretty_print(os, it->value(), indent);
               if (++it == obj.end())
                  break;
               os << ",\n";
            }
         }
         os << "\n";
         indent->resize(indent->size() - json_indent);
         os << *indent << "}";
         break;
      }

      case json::kind::array:
      {
         os << "[\n";
         indent->append(json_indent, ' ');
         auto const& arr = jv.get_array();
         if (!arr.empty())
         {
            auto it = arr.begin();
            for (;;)
            {
               os << *indent;
               pretty_print(os, *it, indent);
               if (++it == arr.end())
                  break;
               os << ",\n";
            }
         }
         os << "\n";
         indent->resize(indent->size() - json_indent);
         os << *indent << "]";
         break;
      }

      case json::kind::string:
      {
         os << json::serialize(jv.get_string());
         break;
      }

      case json::kind::uint64:
         os << jv.get_uint64();
         break;

      case json::kind::int64:
         os << jv.get_int64();
         break;

      case json::kind::double_:
         os << jv.get_double();
         break;

      case json::kind::bool_:
         if (jv.get_bool())
            os << "true";
         else
            os << "false";
         break;

      case json::kind::null:
         os << "null";
         break;
   }

   if (indent->empty())
      os << "\n";
}


int main(int argc, char** argv)
{
   std::string database {argv[1]};
   auto        connection_string = "Driver={Microsoft Access Driver (*.mdb, *.accdb)};Dbq=" + database;

   if (argc == 3)
   {
      std::cout << "allo";
   }
   try
   {
      nanodbc::connection conn(connection_string);

      json::object jsonDoc;
      jsonDoc["version"] = "1.0.0";
      json::object tables;

      for (auto tableInfo: g_tablesToExport)
      {
         json::array rows;
         for (auto result = nanodbc::execute(conn, tableInfo.extractQry); result.next();)
         {
            rows.push_back(RowToJsonObject(result));
         }

         tables[tableInfo.name] = rows;
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
