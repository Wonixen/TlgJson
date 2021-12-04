#include <nanodbc/nanodbc.h>
#include <boost/json.hpp>

#include <fstream>
#include <iostream>

namespace json = boost::json;

std::string readJsonFile(const std::string& filename)
{
    std::ifstream input(filename);
    return std::string ((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

int main(int argc, char **argv)
{
    try
    {
        std::string jsonData = readJsonFile(argv[1]);
        auto jsonDoc = json::parse(jsonData);
        auto version = jsonDoc.at("version");
        auto schema = jsonDoc.at("TlgSchema");

        auto const& obj = schema.get_object();
        if(! obj.empty())
        {
            auto it = obj.begin();
            for(;;)
            {
                std::cout <<  json::serialize(it->key()) << " : " << std::endl;
                if(++it == obj.end())
                    break;
                std::cout << ",\n";
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
    std::string database {NANODBC_TEXT(argv[2])};
    auto connection_string {NANODBC_TEXT("Driver={Microsoft Access Driver (*.mdb, *.accdb)};Dbq=") + database};

    return 0;
}