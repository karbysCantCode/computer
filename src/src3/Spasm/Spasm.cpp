#include "Spasm/Spasm.hpp"

#include "Spasm/Lexer.hpp"
#include "Spasm/Parser.hpp"
#include "Spasm/Preprocessor.hpp"
#include "Helpers/FileHelper.hpp"

namespace Spasm {

  void spasmPipeline(SMake::Project& project, Arch::Architecture& arch) {
    for (auto& target : project.m_targets) {

      Debug::FullLogger logger;

      Program program;

      for (const auto& sourcePath : target.second.m_sourceFilepaths) {
        const auto src = FileHelper::openFileToString(sourcePath);
        SpasmLexer lexer;
        auto preprocessedTokens = lexer.run(src, sourcePath);
        Preprocessor preproc;
        auto processedTokens = preproc.run(preprocessedTokens, target.second, program, &logger);
        Parser parser;
        parser.ParseTokens(processedTokens, arch, &logger);
        
      }
    }
  }
}