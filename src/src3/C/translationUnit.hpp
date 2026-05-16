#pragma once

#include <unordered_set>
#include <unordered_map>
#include <set>
#include <set>
#include <filesystem>

#include "lexer.hpp"
#include "macro.hpp"

namespace C
{

struct TranslationUnit {
  std::vector<std::filesystem::path> includeDirectories;
  std::unordered_set<std::filesystem::path> includedFilesToNotReinclude;  // only includes if not pragma once
  std::filesystem::path sourcePath;
  std::string source;
  TokenHolder tokenHolder;
  std::unordered_map<std::string_view, Macro> macros;

  bool pragmaOnce = false;
  

  std::filesystem::path searchincludedDirectoriesForFile(const std::filesystem::path& path);
};


} // namespace C
