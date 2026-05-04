#include "Helpers/SafeStoi.hpp"

#include <format>

//namespace std {
  std::pair<long, std::string> safe_stol(const std::string& str) {
    try
    {
      return {std::stol(str), ""};
    }
    catch(const std::exception& e)
    {
      return {0, std::format("Failed to convert number, err \"{}\", with excfeption {}", str, e.what())};
    }
    
  }

  std::pair<long, std::string> safe_stol(const std::string& str, int base) {
    try
    {
      return {std::stol(str, nullptr, base), ""};
    }
    catch(const std::exception& e)
    {
      return {0, std::format("Failed to convert number, err \"{}\", with exception {}", str, e.what())};
    }
    
  }
//}