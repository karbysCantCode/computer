#pragma once
#include <filesystem>
#include "Debug.hpp"


namespace FileHelper {
  std::string openFileToString(const std::filesystem::path& path);
  std::string openFileToString(const std::filesystem::path& path, Debug::FullLogger* logger);
  void writeBytesToFile(const std::vector<uint8_t>& bytes, const std::filesystem::path& filepath);
  void writeBytesToFile(const std::vector<uint8_t>& bytes, const std::filesystem::path& filepath, Debug::FullLogger* logger);
}
