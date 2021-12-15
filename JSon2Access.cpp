#include "utf8Conversion.h"

#include <boost/json.hpp>
#include <nanodbc/nanodbc.h>

#include <format>
#include <fstream>
#include <iostream>
#include <set>
#include <variant>

namespace json = boost::json;

// this is the locale used by the console in Québec!!!
static std::locale loc850(".850");

std::string readJsonFile(const std::string& filename)
{
   std::ifstream input(filename);
   return std::string((std::istreambuf_iterator<char>(input)),
                      std::istreambuf_iterator<char>());
}

/*
specify tables to erase  order is important
specify data to import, order is important and not necessarely same as in erase

   simplification:
      - object member name are same as column name
      - all strings are passed as string in codepage 1252
      -
*/
enum TableIds
{
   TAGS           = 0,
   FIELDS         = 1,
   LOGFILES       = 2,
   MESSAGE        = 3,
   LOGGERMESSAGES = 4
};

std::vector<std::string> g_tables = {
   {"Tags"},
   {"Fields"},
   {"LogFiles"},
   {"Messages"},
   {"LoggerMessages"},
};

std::vector<int> deletionOrder = {
   LOGGERMESSAGES,
   MESSAGE,
   LOGFILES,
   FIELDS,
   TAGS,
};

std::vector<int> creationOrder = {
   TAGS,
   FIELDS,
   LOGFILES,
   MESSAGE,
   LOGGERMESSAGES,
};

std::string Quotify(std::string s)
{
   std::string r;
   for (int i = 0; i < s.size(); i++)
      if (s[i] == '\'')
         r += "''";
      else
         r += s[i];
   return r;
}

std::string ToDb(const json::value& jv)
{
   switch (jv.kind())
   {
      case json::kind::uint64:
         return std::to_string(jv.get_uint64());

      case json::kind::int64:
         return std::to_string(jv.get_int64());

      case json::kind::null:
         return "NULL";

      case json::kind::string:
         return "'" + Quotify(Utf8ToCp1252(jv.get_string().c_str())) + "'";

      case json::kind::double_:
         return std::to_string(jv.get_double());
   }
   throw std::runtime_error("invalid json kind for database");
}


int main(int argc, char** argv)
{
   try
   {
      std::string database {argv[1]};
      auto        connection_string =
         "Driver={Microsoft Access Driver (*.mdb, *.accdb)};Dbq=" + database;
      std::string jsonData = readJsonFile(argv[2]);

      auto jsonDoc = json::parse(jsonData);

      nanodbc::connection conn(connection_string);

      for (auto tblId: deletionOrder)
      {
         std::string eraseCmd = std::format("DELETE FROM {}", g_tables.at(tblId));
         auto        rowIt    = nanodbc::execute(conn, eraseCmd);
         if (rowIt.has_affected_rows())
         {
            std::cout << "deleted row(s): " << rowIt.affected_rows() << std::endl;
         }
      }

      auto        version    = jsonDoc.at("version");
      auto        schema     = jsonDoc.at("TlgSchema");
      auto const& jsonTables = schema.get_object();

      for (auto tblId: creationOrder)
      {
         auto                 tableName = g_tables.at(tblId);
         auto                 tableData = jsonTables.at(tableName).as_array();
         nanodbc::transaction transaction(conn);
         for (const auto& data: tableData)
         {
            const auto& row = data.as_object();
            if (row.empty())
               continue;
            std::string colList;
            std::string values;

            for (auto it = row.begin(); it != row.end(); ++it)
            {
               if (it != row.begin())
               {
                  colList += ", ";
                  values += ", ";
               }

               colList += it->key_c_str();
               values += ToDb(it->value());
            }
            auto sqlCmd = std::format("insert into {} ({}) VALUES({});", tableName, colList, values);
            nanodbc::execute(conn, sqlCmd);
         }
         std::cout << std::endl;

         transaction.commit();
      }
   }
   catch (const std::exception& e)
   {
      std::cerr << e.what() << '\n';
   }

   return 0;
}