#include "Helpers/SafeStoi.hpp"

#include <format>

//namespace std {
  std::pair<long long, std::string> safe_stoll(const std::string& str) {
    if (str.length() < 1) return {0,""};
    try
    {
      return {std::stoll(str), ""};
    }
    catch(const std::exception& e)
    {
      return {0, std::format("Failed to convert number, err \"{}\", with excfeption {}", str, e.what())};
    }
    
  }

  std::pair<long long , std::string> safe_stoll(const std::string& str, int base) {
    try
    {
      return {std::stoll(str, nullptr, base), ""};
    }
    catch(const std::exception& e)
    {
      return {0, std::format("Failed to convert number, err \"{}\", with exception {}", str, e.what())};
    }
    
  }
//}