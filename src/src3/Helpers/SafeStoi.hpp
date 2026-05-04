#pragma once

#include <string>
#include <utility>

//namespace std {
  std::pair<long, std::string> safe_stol(const std::string& str);

  std::pair<long, std::string> safe_stol(const std::string& str, int base);
//}