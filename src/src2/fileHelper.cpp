#include "fileHelper.hpp"

#include <fstream>
#include <iostream>

std::string FileHelper::openFileToString(const std::filesystem::path& path) {
  std::ifstream file(path.generic_string());
  if (!file.is_open()) {
      std::cerr << "Failed to open the file \"" << path.generic_string() << '\"' << std::endl;
      return std::string{};
  }

  // Read the entire file content into a std::string using iterators
  std::string file_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return file_contents;
}
std::string openFileToString(const std::filesystem::path& path, Debug::FullLogger* logger) {
  std::ifstream file(path.generic_string());
  if (!file.is_open()) {
      if (logger != nullptr) {logger->Errors.logMessage("Lexer failed to open the file \"" + path.generic_string() + '\"');}
      return std::string{};
  }

  // Read the entire file content into a std::string using iterators
  std::string file_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return file_contents;
}
std::string openFileToString(const std::filesystem::path& path, Debug::MessageLogger* mlogger) {
  {
  std::ifstream file(path.generic_string());
  if (!file.is_open()) {
      if (mlogger != nullptr) {mlogger->logMessage("Lexer failed to open the file \"" + path.generic_string() + '\"');}
      return std::string{};
  }

  // Read the entire file content into a std::string using iterators
  std::string file_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return file_contents;
}
}