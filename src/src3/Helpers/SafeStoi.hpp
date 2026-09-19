#pragma once

#include <string>
#include <utility>

//namespace std {
  std::pair<long long, std::string> safe_stoll(const std::string& str);

  std::pair<long long, std::string> safe_stoll(const std::string& str, int base);
//} 