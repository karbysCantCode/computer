#include "FileHelper.hpp"

#include <fstream>
#include <iostream>

std::string FileHelper::openFileToString(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
      std::cerr << "Failed to open the file \"" << path.generic_string() << '\"' << std::endl;
      return std::string{};
  }

  // Read the entire file content into a std::string using iterators
  std::string file_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  //std::cout << "[File opened]" << file_contents << "[file open end]" << std::endl;
  return file_contents;
}
std::string FileHelper::openFileToString(const std::filesystem::path& path, Debug::FullLogger* logger) {
  std::ifstream file(path);
  if (!file.is_open()) {
      if (logger != nullptr) {logger->Errors.logMessage("Lexer failed to open the file \"" + path.string() + '\"');}
      return std::string{};
  }

  // Read the entire file content into a std::string using iterators
  std::string file_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  //std::cout << "[File opened]" << file_contents << "[file open end]" << std::endl;
  return file_contents;
}
