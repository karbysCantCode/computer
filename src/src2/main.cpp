#include <iostream>
#include <filesystem>

#include "smake.hpp"
#include "spasm.hpp"

#define DumpLogger(logger, AuthorName) \
  if (readErrors) {                                         \
    while (logger.Errors.isNotEmpty()) {                    \
      std::cout << '[' << AuthorName << "][ERROR] " << logger.Errors.consumeMessage() << '\n';  \
    }                                                       \
  }                                                         \
  if (readWarnings) {                                       \
    while (logger.Warnings.isNotEmpty()) {                  \
      std::cout << '[' << AuthorName << "][WARNING] " << logger.Warnings.consumeMessage() << '\n';\
    }                                                       \
  }                                                         \
  if (readDebugs) {                                         \
    while (logger.Debugs.isNotEmpty()) {                    \
      std::cout << '[' << AuthorName << "][DEBUG] " << logger.Debugs.consumeMessage() << '\n';  \
    }                                                       \
  }

// execute on MAC with: ./exeName Args

// set cmake tools launch args in settings.json at: "cmake.debugConfig"

//args
  //--smake smakeFilePath
  //  executes the smake file (mutually exclusive to --asm)
  //--asm spasmFilePath
  //  compiles the spasm file (mutually exclusive to --smake)
  //-v
  //  dumps warnings
  //-vv
  //  dumps warnings and debugs
  //-s
  //  mutes errors

//-1 == conflicting flags
//-2 == file not found
//-3 == no args
int main(int argc, char* argv[]) {
  bool smakeFlag = false;
  bool asmFlag = false;

  bool readErrors = true;
  bool readWarnings = false;
  bool readDebugs = false;

  std::filesystem::path smakePath;
  std::filesystem::path asmPath;

  for (int i = 1; i < argc; i++) {

    const std::string arg = argv[i];
    if (arg == "--smake") {
      if (asmFlag) {std::cerr << "--smake mutually exclusive to --asm, aborted."; return -1;}
      smakeFlag = true;

      i++;
      if (i < argc) {
        smakePath = argv[i];
        if (!std::filesystem::exists(smakePath)) {std::cerr << "file \"" << smakePath.generic_string() << "\" does not exist. Aborted."; return -2;}
      }
    } else if (arg =="--asm") {
      if (smakeFlag) {std::cerr << "--asm mutually exclusive to --smake, aborted."; return -1;}
      asmFlag = true;

      i++;
      if (i < argc) {
        asmPath = argv[i];
        if (!std::filesystem::exists(asmPath)) {std::cerr << "file \"" << smakePath.generic_string() << "\" does not exist. Aborted."; return -2;}
      }
    } else if (arg =="-v") {
      readWarnings = true;
    } else if (arg =="-vv") {
      readDebugs = true;
    } else if (arg =="-s") {
      readErrors = false;
    } else {
      if (!readWarnings) {continue;}
      std::cout << "Unknown arg \"" << arg << "\"" << std::endl;
    }
  }

  if (!smakeFlag && !asmFlag) {
    asmFlag = true;
    if (argc < 2) {std::cerr << "No arguments passed, aborted."; return -3;}
    if (!(std::filesystem::exists(argv[1]))) {std::cerr << "file \"" << argv[1] << "\" does not exist. Aborted."; return -2;}
    asmPath = argv[2];
  }

  int errc = 0;
  if (smakeFlag) {  
    Debug::FullLogger logger;
    SMake::SMakeProject project;
    auto tokens = SMake::lex(smakePath, &logger);
    DumpLogger(logger, "SMAKELEX");
 
    SMake::parseTokensToProject(tokens, project, smakePath, &logger);
    DumpLogger(logger, "SMAKEPARSER");
 
    std::cout << project.toString() << std::endl;

    return errc;
  }

  if (asmFlag) {

    return errc;
  }





  return 0;
}