#pragma once
#include <filesystem>

struct CLIOptions {
  public:
  bool spasm = false;
  bool smake = false;
  bool newArch = false;
  bool regexArchDump = false;
  bool c = false;

  bool silent = false;
  bool warns = false;
  bool debugs = false;

  std::filesystem::path spasmPath;
  std::filesystem::path smakePath;
  std::filesystem::path newArchPath;
  std::filesystem::path cPath;
  std::vector<std::filesystem::path> cIncPaths;

  bool evaluate(int argc, char* argv[]);
};