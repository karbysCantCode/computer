#include "Helpers/CLIOptions.hpp"

#include <string>
#include <iostream>


// execute on MAC with: ./exeName Args

// set cmake tools launch args in settings.json at: "cmake.debugConfig"

//args
  //--smake smakeFilePath
  //  executes the smake file (mutually exclusive to --asm)
  //--asm spasmFilePath
  //  compiles the spasm file (mutually exclusive to --smake)
  //--arch archFilePath
  //  caches the arch file
  //-v
  //  dumps warnings
  //-vv
  //  dumps warnings and debugs
  //-s
  //  mutes errors


bool CLIOptions::evaluate(int argc, char* argv[]) {
  size_t arg = 1;

  while (arg < argc) {
    std::string option = argv[arg];
    if (option == "--smake") {
      arg++;
      smake = true;
      if (arg < argc) {
        auto pathString = std::string(argv[arg]);
        smakePath = {pathString};
        if (!std::filesystem::exists(smakePath)) {
          std::cerr << "file \"" << smakePath.generic_string() << "\" does not exist. Aborted.";
          return false;
        }
        smakePath = std::filesystem::canonical(smakePath);
        // if (!std::filesystem::exists(smakePath)) {
        //   std::cerr << "Could not find file \"" << pathString << "\", Aborted.";
        //   return false;
        // }
      } else {
        std::cerr << "Expected a filepath got none. Aborted";
        return false;
      }
    } else if (option == "--asm") {
      arg++;
      spasm = true;
      if (arg < argc) {
        auto pathString = std::string(argv[arg]);
        spasmPath = {pathString};
        if (!std::filesystem::exists(spasmPath)) {
          std::cerr << "file \"" << spasmPath.generic_string() << "\" does not exist. Aborted.";
          return false;
        }
        spasmPath = std::filesystem::canonical(spasmPath);
        // if (!std::filesystem::exists(spasmPath)) {
        //   std::cerr << "Could not find file \"" << pathString << "\", Aborted.";
        //   return false;
      } else {
        std::cerr << "Expected a filepath for --asm, got none. Aborted";
        return false;
      }
    } else if (option == "--arch") {
      arg++;
      newArch = true;
      if (arg < argc) {
        auto pathString = std::string(argv[arg]);
        newArchPath = {pathString};
                if (!std::filesystem::exists(newArchPath)) {
          std::cerr << "file \"" << newArchPath.generic_string() << "\" does not exist. Aborted.";
          return false;
        }
        newArchPath = std::filesystem::canonical(newArchPath);
        // if (!std::filesystem::exists(newArchPath)) {
        //   std::cerr << "Could not find file \"" << pathString << "\", Aborted.";
        //   return false;
        // }
      } else {
        std::cerr << "Expected a filepath for --arch, got none. Aborted";
        return false;
      }
    } else if (option == "-v") {
      warns = true;
    } else if (option == "-vv") {
      debugs = true;
    } else if (option == "-s") {
      silent = true;
    } else if (option == "--regex-arch") {
      regexArchDump = true;
    } else {
      std::cout << "Unknown arg \"" << option << "\", use --help for usage.\n";
    }
    arg++;
  }

  if (spasm && smake) {
    std::cerr << "--asm and --smake are mutually exclusive. Aborted.";
    return false;
  }
  return true;
}