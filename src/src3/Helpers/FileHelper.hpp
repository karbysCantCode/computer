#pragma once
#include <filesystem>
#include "Debug.hpp"


namespace FileHelper {
  std::string openFileToString(const std::filesystem::path& path);
  std::string openFileToString(const std::filesystem::path& path, Debug::FullLogger* logger);
}