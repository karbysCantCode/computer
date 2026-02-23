#include "corePipelines.hpp"

#include <filesystem>

#include "Arch/arch.hpp"
#include "SMake/smake.hpp"
#include "debugHelpers.hpp"
#include "fileHelper.hpp"
#include "Spasm/spasmPreprocessor.hpp"

int runArchPipeline(CLIOptions& options, Arch::Architecture** targetArch) {

  if (options.arch) {
    std::filesystem::copy(options.archPath, std::filesystem::path(ARCH_CACHED_FILE_PATH), std::filesystem::copy_options::overwrite_existing);
  }
  Debug::FullLogger archLogger;
  *targetArch = new Arch::Architecture();
  auto archTokens = Arch::Lexer::lex(options.archPath,&archLogger);
  Arch::assembleTokens(archTokens,**targetArch,&archLogger);

  return 0;
}

int runSMakePipeline(CLIOptions& options, Arch::Architecture& targetArch) {

  SMake::SMakeProject project;
  Debug::FullLogger logger;

  auto smakeTokens = SMake::lex(options.smakePath,&options.logger);
  SMake::parseTokensToProject(smakeTokens, project, options.smakePath, &options.logger);
  
  std::unordered_set<std::filesystem::path> filePathsParsedOrInQue;
  std::unordered_map<std::filesystem::path, std::unique_ptr<Spasm::Program::ProgramForm>> parsedPrograms;
  std::stack<pathTargetPair> filesToParse;

  for (const auto& targetIt : project.m_targets) {
    for (const auto& path : targetIt.second.m_sourceFilepaths) {
      filesToParse.emplace(path, targetIt.second);
    }
  }

  while (!filesToParse.empty()) {
    const auto currentPathPair = filesToParse.top();
    filePathsParsedOrInQue.insert(currentPathPair.m_path);
    filesToParse.pop();

    auto program = std::make_unique<Spasm::Program::ProgramForm>(compileSpasm(currentPathPair.m_path, targetArch, &logger));
    
    for (const auto& includedPath : program->m_includedPrograms) {
      auto resolvedPath = currentPathPair.m_target.searchForPathInIncluded(includedPath.first);
      if (resolvedPath.empty()) {
        logger.Errors.logMessage(includedPath.second.positionToString() + "Included file couldn't be found.");
        continue;
      }
      if (filePathsParsedOrInQue.find(includedPath.first) == filePathsParsedOrInQue.end()) {
        filePathsParsedOrInQue.emplace(includedPath.first);
        filesToParse.emplace(includedPath, currentPathPair.m_target);
      }
    }

    parsedPrograms.emplace(currentPathPair.m_path, std::move(program));
  }
  
  return 0;
}

Spasm::Program::ProgramForm compileSpasm(std::filesystem::path path, Arch::Architecture& arch, Debug::FullLogger* logger) {
  Spasm::Program::ProgramForm program;
  
  Spasm::Lexer::Lexer lexer(logger);
  Preprocessor pp(logger);
  Spasm::Program::ProgramParser programParser(logger);
  logger->setAuthor("[Lexer]");
  auto lexedTokens = lexer.run(path, arch.m_keywordSet);
  logger->setAuthor("[Preprocessor]");
  auto processedTokens = pp.run(lexedTokens, program);
  logger->setAuthor("[Parser]");
  programParser.run(processedTokens, arch, program, path);

  return program;
}

