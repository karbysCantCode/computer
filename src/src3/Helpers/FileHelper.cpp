#include "FileHelper.hpp"

#include <fstream>
#include <iostream>
#include <filesystem>

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

void FileHelper::writeBytesToFile(const std::vector<uint8_t>& bytes, const std::filesystem::path& filepath) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists(filepath)) {
    fs::create_directories(filepath.parent_path(), ec);

    if (ec) {
      std::cerr << std::format(
        "Failed to create directories for \"{}\": {}\n",
        filepath.string(), ec.message()
      );
      return;
    }
  }
  std::ofstream out(filepath, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!out) {
    std::cerr << std::format(
      "Failed to open file \"{}\" for writing\n",
      filepath.string()
    );
    return;
  }

  out.write(
    reinterpret_cast<const char*>(bytes.data()),
    static_cast<std::streamsize>(bytes.size())
  );

  if (!out) {
    std::cerr << "Failed while writing to file\n";
  }
}

void FileHelper::writeBytesToFile(const std::vector<uint8_t>& bytes, const std::filesystem::path& filepath, Debug::FullLogger* logger) {
  namespace fs = std::filesystem;
  std::cout << "writing to: " << filepath.string() << '\n';
  std::error_code ec;
  std::cout << "DB0\n";
  if (!fs::exists(filepath)) {
    fs::create_directories(filepath.parent_path(), ec);

    if (ec) {
      if (logger) {
        logger->Errors.logMessage(std::format(
        "Failed to create directories for \"{}\": {}\n",
        filepath.string(), ec.message()
      ));
      std::cout << "retdb01\n";
      //return;
      }
    }
  }
  std::cout << "DB1\n";

  std::ofstream out(filepath, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!out) {
    if (logger) {
      logger->Errors.logMessage(
        std::format(
          "Failed to open file \"{}\" for writing\n",
          filepath.string()
        )
      );
    }
    return;
  }

  std::cout << "DB2\n";

  out.write(
    reinterpret_cast<const char*>(bytes.data()),
    static_cast<std::streamsize>(bytes.size())
  );

  std::cout << "DB3\n";

  if (!out) {
    if (logger) {
      logger->Errors.logMessage(std::format("Failed while writing to file \"{}\"\n", filepath.string()));
    }
  }

  std::cout << "\nsuccess write\n";
}

