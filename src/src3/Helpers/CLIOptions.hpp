#pragma once
#include <filesystem>

class CLIOptions {
  bool spasm = false;
  bool smake = false;
  bool newArch = false;

  std::filesystem::path spasmPath;
  std::filesystem::path smakePath;
  std::filesystem::path newArchPath;
};