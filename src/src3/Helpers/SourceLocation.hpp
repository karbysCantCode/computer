#pragma once
#include <filesystem>
#include <string>
#include <format>

struct SourceLocation {
  std::filesystem::path filePath;
  size_t line;
  size_t column;

  SourceLocation(std::filesystem::path path, size_t ln, size_t col) : filePath(path), line(ln), column(col) {}

  std::string toString() const {return std::format("{}:{}:{}:", filePath.string(), line, column);}
};