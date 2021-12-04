#pragma once
#include <boost/json.hpp>
#include <ostream>
#include <string>

void pretty_print(std::ostream& os, boost::json::value const& jv, std::string* indent = nullptr);
