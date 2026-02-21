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

CLIOptions parseArguments(int argc, char* argv[]) {
  CLIOptions options;
  for (int i = 1; i < argc; i++) {

    const std::string arg = argv[i];
    if (arg == "--smake") {
      if (options.spasm) {std::cerr << "--smake mutually exclusive to --asm, aborted."; options.errc = -1; return options;}
      options.smake = true;

      i++;
      if (i < argc) {
        options.smakePath = argv[i];
        if (!std::filesystem::exists(options.smakePath)) {std::cerr << "file \"" << options.smakePath.generic_string() << "\" does not exist. Aborted."; options.errc = -2; return options;}
        options.smakePath = std::filesystem::canonical(options.smakePath);
      }
    } else if (arg =="--asm") {
      if (options.smake) {std::cerr << "--asm mutually exclusive to --smake, aborted."; options.errc = -1; return options;}
      options.spasm = true;

      i++;
      if (i < argc) {
        options.spasmPath = argv[i];
        if (!std::filesystem::exists(options.spasmPath)) {std::cerr << "file \"" << options.smakePath.generic_string() << "\" does not exist. Aborted."; options.errc = -2; return options;}
      }
    } else if (arg =="--arch") {
      options.arch = true;

      i++;
      if (i < argc) {
        options.archPath = argv[i];
        if (!std::filesystem::exists(options.archPath)) {std::cerr << "file \"" << options.archPath.generic_string() << "\" does not exist. Aborted."; options.errc = -2; return options;}
      }
    } else if (arg =="-v") {
      options.readWarnings = true;
    } else if (arg =="-vv") {
      options.readDebugs = true;
    } else if (arg =="-s") {
      options.readErrors = false;
    } else {
      if (!options.readWarnings) {continue;}
      std::cout << "Unknown arg \"" << arg << "\"" << std::endl;
    }
  }

  if (!options.smake && !options.spasm) {
    options.spasm = true;
    if (argc < 2) {std::cerr << "No arguments passed, aborted."; options.errc = -3; return options;}
    if (!(std::filesystem::exists(argv[1]))) {std::cerr << "file \"" << argv[1] << "\" does not exist. Aborted."; options.errc = -2; return options;}
    options.spasmPath = argv[2];
  }
}

