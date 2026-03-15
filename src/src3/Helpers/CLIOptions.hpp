#pragma once
#include <filesystem>

struct CLIOptions {
  public:
  bool spasm = false;
  bool smake = false;
  bool newArch = false;

  bool silent = false;
  bool warns = false;
  bool debugs = false;

  std::filesystem::path spasmPath;
  std::filesystem::path smakePath;
  std::filesystem::path newArchPath;

  bool evaluate(int argc, char* argv[]);
};