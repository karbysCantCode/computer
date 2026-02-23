#pragma once

#define ARCH_CACHED_FILE_PATH "__CACHED_ARCH_COPY__.arch"

#include <filesystem>
#include <iostream>

#include "debugHelpers.hpp"

struct CLIOptions {
  bool smake = false;
  bool spasm = false;
  bool arch = false;

  bool readErrors = true;
  bool readWarnings = false;
  bool readDebugs = false;

  int errc = 0;

  std::filesystem::path smakePath;
  std::filesystem::path spasmPath;
  std::filesystem::path archPath = ARCH_CACHED_FILE_PATH;

  Debug::FullLogger logger;
};

CLIOptions parseArguments(int argc, char* argv[]);