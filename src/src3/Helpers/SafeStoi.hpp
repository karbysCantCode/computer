#pragma once

#include <string>
#include <utility>

namespace std {
  std::pair<int, std::string> safe_stoi(const std::string& str);

  std::pair<int, std::string> safe_stoi(const std::string& str, int base);
}