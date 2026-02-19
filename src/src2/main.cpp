#include <iostream>
#include <filesystem>

#include "smake.hpp"
#include "spasm.hpp"
#include "arch.hpp"
#include "spasmPreprocessor.hpp"

#define ARCH_CACHED_FILE_PATH "__CACHED_ARCH_COPY__.arch"

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
  //--arch archFilePath
  //  caches the arch file
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
  bool archFlag = false;

  bool readErrors = true;
  bool readWarnings = false;
  bool readDebugs = false;

  std::filesystem::path smakePath;
  std::filesystem::path asmPath;
  std::filesystem::path archPath = ARCH_CACHED_FILE_PATH;

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
    } else if (arg =="--arch") {
      archFlag = true;

      i++;
      if (i < argc) {
        archPath = argv[i];
        if (!std::filesystem::exists(archPath)) {std::cerr << "file \"" << archPath.generic_string() << "\" does not exist. Aborted."; return -2;}
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
  if (archFlag) {
    std::filesystem::copy(archPath, std::filesystem::path(ARCH_CACHED_FILE_PATH), std::filesystem::copy_options::overwrite_existing);
  }
  Debug::FullLogger archLogger;
  Arch::Architecture targetArch;
  auto archTokens = Arch::Lexer::lex(archPath,&archLogger);
  Arch::assembleTokens(archTokens,targetArch,&archLogger);
  

  std::cout << targetArch.toString();
  DumpLogger(archLogger, "ARCH");

  if (smakeFlag) {  
    Debug::FullLogger logger;
    SMake::SMakeProject project;

    auto tokens = SMake::lex(smakePath, &logger);

    DumpLogger(logger, "SMAKELEX");
    

    SMake::parseTokensToProject(tokens, project, smakePath, &logger);
    DumpLogger(logger, "SMAKEPARSER");
 
    std::cout << project.toString() << std::endl;

    for (auto& target : project.m_targets) {
      std::unordered_map<std::filesystem::path, std::unique_ptr<Spasm::Program::ProgramForm>> filePathProgramMap;
      for (const auto& path : target.second.m_sourceFilepaths) {
        Debug::FullLogger logger;
        auto tokens = Spasm::Lexer::lex(path, targetArch.m_keywordSet, &logger);
        
        auto program = std::make_unique<Spasm::Program::ProgramForm>();
        auto programPtr = program.get();
        filePathProgramMap.emplace(path, std::move(program));
        preprocessSpasm(tokens, *program, target.second, &logger);
        Spasm::Program::parseProgram(tokens, targetArch, *program, &logger, path);
        for (const auto& statement : (*program).m_statements) {
          std::cout << "[state]" << statement->toString();
        }
        DumpLogger(logger, "SPASM");
      }
    }

    //implement byte length calculation for instructions

    return errc;
  }

  if (asmFlag) {

    return errc;
  }





  return 0;
}