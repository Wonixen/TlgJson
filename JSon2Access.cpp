#include <boost/json.hpp>
#include <nanodbc/nanodbc.h>

#include <fstream>
#include <iostream>
#include <set>

namespace json = boost::json;

std::string readJsonFile(const std::string& filename)
{
   std::ifstream input(filename);
   return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

struct ColumnDef
{
   std::string tag;
   std::string column;
};

class JsonImport
{
public:
   virtual std::string         GetEraseCmd()  = 0;
   virtual std::string         GetTableName() = 0;
   virtual std::string         GetJsonName()  = 0;
   virtual std::set<ColumnDef> GetColumnDef() = 0;
};
#if 0
struct JSonImport
{
   std::string              tag;
   std::string              table;
   std::string              removeTableDataCmd;
   std::vector<TagToColumn> tagMapping;
};

std::vector<JSonImport> g_importInfo = {{"Tags",
                                         "Tags",
                                         "DELETE FROM Tags",
                                         {
                                            {"Tag_Code", nullptr},
                                            {"Tag_Code", nullptr},
                                            {"Tag_Code", nullptr},
                                         }},
                                        {"HeaderMessages", "LoggerMessages", ""}};
#endif


std::vector<std::string> g_tablesToErase = {
   "DELETE FROM LoggerMessages",
   "DELETE FROM Messages",
   "DELETE FROM LogFiles",
   "DELETE FROM Fields",
   "DELETE FROM Tags",
};

int main(int argc, char** argv)
{
   try
   {
      std::string database {argv[1]};
      auto        connection_string = "Driver={Microsoft Access Driver (*.mdb, *.accdb)};Dbq=" + database;
      std::string jsonData          = readJsonFile(argv[2]);

      auto jsonDoc = json::parse(jsonData);

      nanodbc::connection conn(connection_string);

      for (auto sqlCmd: g_tablesToErase)
      {
         auto rowIt = nanodbc::execute(conn, sqlCmd);
         if (rowIt.has_affected_rows())
         {
            std::cout << "deleted row(s): " << rowIt.affected_rows() << std::endl;
         }
      }

      auto        version = jsonDoc.at("version");
      auto        schema  = jsonDoc.at("TlgSchema");
      auto const& tables  = schema.get_object();

      if (!tables.empty())
      {
         for (auto table = tables.begin(); table != tables.end(); ++table)
         {
            auto const& rows = table->value().get_array();
            if (!rows.empty())
            {
               std::ostringstream cmdHeader;
               cmdHeader << "insert into " << table->key_c_str() << "(";

               for (size_t idx = 0; idx < rows.size(); idx++)
               {
                  auto row = rows[idx].get_object();
                  if (row.empty() == false)
                  {
                     std::ostringstream sqlColumns;
                     std::ostringstream sqlValues;
                     for (auto it = row.begin(); it != row.end(); ++it)
                     {
                        if (it != row.begin())
                        {
                           sqlColumns << ", ";
                           sqlValues << ", ";
                        }
                        sqlColumns << it->key_c_str();
                        sqlValues << it->value();
                     }
                     std::ostringstream sqlCmd;
                     sqlCmd << cmdHeader.str() << sqlColumns.str() << ") VALUES(" << sqlValues.str() << ")";
                     std::cout << sqlCmd.str() << std::endl;
                  }
               }
            }
         }
      }
   }
   catch (const std::exception& e)
   {
      std::cerr << e.what() << '\n';
   }

   return 0;
}