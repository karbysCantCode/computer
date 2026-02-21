#include <iostream>
#include <filesystem>

#include "Arch/arch.hpp"
#include "CLIHelper.hpp"
#include "corePipelines.hpp"

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

  #define DumpLoggerRaw(logger) \
  if (readErrors) {                                         \
    while (logger.Errors.isNotEmpty()) {                    \
      std::cout << logger.Errors.consumeMessage() << '\n';  \
    }                                                       \
  }                                                         \
  if (readWarnings) {                                       \
    while (logger.Warnings.isNotEmpty()) {                  \
      std::cout << logger.Warnings.consumeMessage() << '\n';\
    }                                                       \
  }                                                         \
  if (readDebugs) {                                         \
    while (logger.Debugs.isNotEmpty()) {                    \
      std::cout << logger.Debugs.consumeMessage() << '\n';  \
    }                                                       \
  }

struct parseInfo {
  const std::filesystem::path& m_path;
  SMake::Target& m_target;

  parseInfo(
    const std::filesystem::path& path,
    SMake::Target& target
  ) : 
    m_path(path),
    m_target(target)
  {}
};

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
//-4 == arch unparsed
int main(int argc, char* argv[]) {
  auto options = parseArguments(argc,argv);

  if (options.errc != 0) {return options.errc;}

  Arch::Architecture* arch = nullptr;
  
  if (options.arch) {
    //arch pipeline
    runArchPipeline(options, &arch);
  }

  if (options.smake) {
    //smake pipeline
    if (arch == nullptr) {std::cerr << "Fatal. Arch unparsed."; return -1;}

    runSMakePipeline(options, *arch);
  }

  if (options.spasm) {
    //single asm pipeline
    if (arch == nullptr) {std::cerr << "Fatal. Arch unparsed."; return -1;}
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

    std::stack<parseInfo> sourceToParseStack;
    std::unordered_set<std::filesystem::path> parsedSet;

    for (auto& target : project.m_targets) {
      for (const auto& path : target.second.m_sourceFilepaths) {
        sourceToParseStack.push(parseInfo(path, target.second));
      }
    }

    Debug::FullLogger lexLogger;
    Debug::FullLogger ppLogger;
    Debug::FullLogger parseLogger;


    while (!sourceToParseStack.empty()) {
      const auto& sourceInfo = sourceToParseStack.top();
      sourceToParseStack.pop();
      parsedSet.insert(sourceInfo.m_path);

      std::cout << "SPASMPARSING" << sourceInfo.m_path.string() << '\n';

      //lex
      auto tokens = Spasm::Lexer::lex(sourceInfo.m_path, targetArch.m_keywordSet, &lexLogger);
      
      //resolve the program object.
      Spasm::Program::ProgramForm* programPtr = nullptr;
      auto it = sourceInfo.m_target.m_filePathProgramMap.find(sourceInfo.m_path);
      if (it == sourceInfo.m_target.m_filePathProgramMap.end()) {
        auto program = std::make_unique<Spasm::Program::ProgramForm>();
        programPtr = program.get();
        sourceInfo.m_target.m_filePathProgramMap.emplace(sourceInfo.m_path, std::move(program));
      } else {
        programPtr = it->second.get();
        if (programPtr == nullptr) {
          it->second = std::make_unique<Spasm::Program::ProgramForm>();
          programPtr = it->second.get();
        }
      }

      //process tokens

      Preprocessor pp(&ppLogger);
      auto preprocessedTokens = pp.run(tokens, *programPtr, sourceInfo.m_target);
      std::cout << "tokenLength:" << preprocessedTokens.m_tokens.size() << '\n';
      preprocessedTokens.reset();
      Spasm::Program::ProgramParser programParser(&parseLogger);
      programParser.run(preprocessedTokens, targetArch, *programPtr, sourceInfo.m_path);
      std::cout << "instrlength " << (*programPtr).m_statements.size() << '\n';
      for (const auto& statement : (*programPtr).m_statements) {
        std::cout << "[state]" << statement->toString();
      }
    }
    DumpLoggerRaw(lexLogger);
    DumpLoggerRaw(ppLogger);
    DumpLoggerRaw(parseLogger);


    // DumpLogger(lexLogger, "LEXER");
    // DumpLogger(ppLogger, "PREPROC");
    // DumpLogger(parseLogger, "PARSER");

    // for (auto& target : project.m_targets) {
    //   for (const auto& path : target.second.m_sourceFilepaths) {
    //     Debug::FullLogger logger;
    //     auto tokens = Spasm::Lexer::lex(path, targetArch.m_keywordSet, &logger);
        
    //     Spasm::Program::ProgramForm* programPtr = nullptr;
    //     auto it = target.second.m_filePathProgramMap.find(path);
    //     if (it == target.second.m_filePathProgramMap.end()) {
    //       auto program = std::make_unique<Spasm::Program::ProgramForm>();
    //       programPtr = program.get();
    //       target.second.m_filePathProgramMap.emplace(path, std::move(program));
    //     } else {
    //       programPtr = it->second.get();
    //       if (programPtr == nullptr) {
    //         it->second = std::make_unique<Spasm::Program::ProgramForm>();
    //         programPtr = it->second.get();
    //       }
    //     }
    //     Debug::FullLogger ppLogger;
    //     Preprocessor pp(&ppLogger);
    //     std::vector<Spasm::Lexer::Token> preprocessedTokens = pp.run(tokens, *programPtr, target.second);
    //     std::cout << "tokenLength:" << preprocessedTokens.size() << '\n';
    //     Spasm::Program::parseProgram(preprocessedTokens, targetArch, *programPtr, &logger, path);
    //     for (const auto& statement : (*programPtr).m_statements) {
    //       std::cout << "[state]" << statement->toString();
    //     }
    //     DumpLogger(ppLogger, "PREPROC");
    //     DumpLogger(logger, "SPASM");
    //   }
    //}

    //implement byte length calculation for instructions

    return errc;
  }

  if (asmFlag) {

    return errc;
  }





  return 0;
}