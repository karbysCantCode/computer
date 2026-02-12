#pragma once

#include <string>
#include <filesystem>

#include "debugHelpers.hpp"

namespace FileHelper {
  std::string openFileToString(const std::filesystem::path& path);
  std::string openFileToString(const std::filesystem::path& path, Debug::FullLogger* logger);
  std::string openFileToString(const std::filesystem::path& path, Debug::MessageLogger* mlogger);
}