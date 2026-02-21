#include "corePipelines.hpp"

#include "Arch/arch.hpp"
#include "SMake/smake.hpp"
#include "debugHelpers.hpp"

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

  auto smakeTokens = SMake::lex(options.smakePath,&options.logger);
  SMake::parseTokensToProject(smakeTokens, project, options.smakePath, &options.logger);
  
  
  return 0;
}