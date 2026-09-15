#include "translationUnit.hpp"

#include <filesystem>

namespace C {

std::filesystem::path TranslationUnit::searchincludedDirectoriesForFile(const std::filesystem::path& path) {
  if (std::filesystem::exists(path)) {
    return path.lexically_normal();
  }
  for (auto iPath : includeDirectories) {
    const auto connected = iPath / path;
    if (std::filesystem::exists(connected)) {
      return connected.lexically_normal();
    }
  }
  return {};
}
}