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

#include "utf8Conversion.h"

namespace json = boost::json;

struct TableExport {
  std::string name;
  std::string extractQry;
};

std::vector<TableExport> g_tablesToExport = {
    {"Tags", "SELECT  * FROM Tags ORDER BY Tag_Code"},
    {"Fields", "SELECT  * FROM Fields ORDER BY Msg_Code, Tag_Code"},
    {"LogFiles", "SELECT  * FROM LogFiles ORDER BY Log_Code"},
    {"Messages", "SELECT  * FROM Messages ORDER BY Msg_Code"},
    {"LoggerMessages", "SELECT  Log_Code, Msg_Code, Schema FROM LoggerMessages "
                       "WHERE  Header_Msg_Code IS NULL ORDER BY Schema, "
                       "Log_Code, Msg_Code"},
    {"HeaderMessages", "SELECT  Header_Msg_Code as Header_Code, Schema, "
                       "Log_Code, Msg_Code FROM LoggerMessages WHERE  "
                       "Header_Msg_Code IS NOT NULL ORDER BY Header_Msg_Code, "
                       "Schema, Log_Code, Msg_Code"},
};

// dumping row to std::cerr for debugging purposes!
// not use anymore
void DumpResult(nanodbc::result &result, short BadCol) {
  for (int i = 0; i < result.columns(); i++) {
    std::cerr << result.column_name(i) << "[";
    if (result.is_bound(i))
      std::cerr << ((result.is_null(i))
                        ? std::string{"NULL"}
                        : ("'" + result.get<nanodbc::string>(i) + "'"));
    std::cerr << "] ";
  }
  std::cerr << "\n error with: " << result.column_name(BadCol) << std::endl;
}

struct ToJsonValue {
  json::value &_v;

  ToJsonValue(json::value &v) : _v(v) {}
  void operator()(void *) const { _v = nullptr; }
  void operator()(int64_t i) const { _v = i; }
  void operator()(double d) const { _v = d; }
  void operator()(const std::string &s) const { _v = s; }
  void operator()(const std::vector<uint8_t> &v) const {
    _v = json::value_from(v);
  }
};

struct ToJsonArray {
  json::array &_a;

  ToJsonArray(json::array &a) : _a(a) {}
  void operator()(void *) const { _a.push_back(nullptr); }
  void operator()(int64_t i) const { _a.push_back(i); }
  void operator()(double d) const { _a.push_back(d); }
  void operator()(const std::string &s) const {
    json::value v;

    v = s;
    _a.push_back(v);
  }
  void operator()(const std::vector<uint8_t> &v) const {
    _a.push_back(json::value_from(v));
  }
};

using DbValue =
    std::variant<void *, int64_t, std::string, double, std::vector<uint8_t>>;

DbValue GetColumnValue(nanodbc::result &row, short col) {
  DbValue dbData;
  if (row.is_bound(col) && row.is_null(col))
    return dbData;

  // for long data type, must fetch then test for NULL
  // for short data type, we must test for null before fetching!
  switch (row.column_datatype(col)) {
  case SQL_LONGVARBINARY: {
    // Unbound data can only be tested for null once fetched
    auto blob = row.get<std::vector<uint8_t>>(col); // try to fetch blob
    if (row.is_null(col) == false)                  // did we get data ?
      dbData = blob;
    return dbData;
  }

  case SQL_LONGVARCHAR: {
    // Unbound data can only be tested for null once fetched
    auto longText = row.get<nanodbc::string>(col); // try to fetch long text
    if (row.is_null(col) == false)                 // got something!!!
      dbData = Cp1252ToUtf8(longText); // long text appear as codepage 1252,
                                       // convert to utf8 which is what json
                                       // understand
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
    dbData = WStringToUtf8(row.get<nanodbc::wide_string>(col));
    return dbData;

  case SQL_NUMERIC:
  case SQL_DECIMAL:
  case SQL_FLOAT:
  case SQL_REAL:
  case SQL_DOUBLE:
    dbData = row.get<double>(col);
    return dbData;

  default:
    dbData = WStringToUtf8(row.get<nanodbc::wide_string>(col));
    return dbData;
  }
}

json::object RowToJsonObject(nanodbc::result &row) {
  json::object rowData;
  for (short colIdx = 0; colIdx < row.columns(); colIdx++) {
    auto &jsonValue = rowData[row.column_name(colIdx)];
    auto dbValue = GetColumnValue(row, colIdx);
    std::visit(ToJsonValue{jsonValue}, dbValue);
  }

  return rowData;
}

// could be used for a smaller output
// at the expanse of readability & having to reconstruct objects
json::value GetStructureOfArray(nanodbc::result rowIt) {
  json::object data;

  for (auto colIdx = 0; colIdx < rowIt.columns(); colIdx++) {
    data[rowIt.column_name(colIdx)] = json::array{};
  }

  int rowCount{0};

  while (rowIt.next()) {
    rowCount++;

    for (auto colIdx = 0; colIdx < rowIt.columns(); colIdx++) {
      // get Array to populate
      auto &colArray = data[rowIt.column_name(colIdx)].as_array();
      auto dbValue = GetColumnValue(rowIt, colIdx);
      std::visit(ToJsonArray{colArray}, dbValue);
    }
  }

  json::object object;
  object["recordCount"] = rowCount;
  object["data"] = data;

  return object;
}

json::array GetArrayOfStructure(nanodbc::result rowIt) {
  json::array rows;
  while (rowIt.next()) {
    rows.push_back(RowToJsonObject(rowIt));
  }
  return rows;
}

std::string readJsonFile(const std::string &filename) {
  std::ifstream input(filename);
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

int main(int argc, char **argv) {
  std::string database{argv[1]};
  auto connection_string =
      "Driver={Microsoft Access Driver (*.mdb, *.accdb)};Dbq=" + database;

  try {
    nanodbc::connection conn(connection_string);

    json::object jsonDoc;
    jsonDoc["version"] = "1.0.0";
    json::object tables;

    for (auto tableInfo : g_tablesToExport) {
      auto rowIt = nanodbc::execute(conn, tableInfo.extractQry);
      // an array of object is more verbose, but easier to visualise and diff
      tables[tableInfo.name] = GetArrayOfStructure(rowIt);

      // structure of array would be more memory friendly, but less intuitive
      // see https://en.wikipedia.org/wiki/AoS_and_SoA
      // tables[tableInfo.name] = GetStructureOfArray(rowIt);
    }

    jsonDoc["TlgSchema"] = tables;

    pretty_print(std::cout, jsonDoc);
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
  }
  return 0;
}
