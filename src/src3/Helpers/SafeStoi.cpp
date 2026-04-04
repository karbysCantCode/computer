#include "Helpers/SafeStoi.hpp"

#include <format>

namespace std {
  std::pair<int, std::string> safe_stoi(const std::string& str) {
    try
    {
      return {std::stoi(str), ""};
    }
    catch(const std::exception& e)
    {
      return {0, std::format("Failed to convert number, err \"{}\"", str)};
    }
    
  }

  std::pair<int, std::string> safe_stoi(const std::string& str, int base) {
    try
    {
      return {std::stoi(str, nullptr, base), ""};
    }
    catch(const std::exception& e)
    {
      return {0, std::format("Failed to convert number, err \"{}\"", str)};
    }
    
  }
}