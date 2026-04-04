#include "Spasm/Spasm.hpp"

#include "Spasm/Lexer.hpp"
#include "Spasm/Parser.hpp"
#include "Spasm/Preprocessor.hpp"
#include "Helpers/FileHelper.hpp"

namespace Spasm {

  void spasmPipeline(SMake::Project& project, Arch::Architecture& arch, CLIOptions& options) {
    for (auto& target : project.m_targets) {

      Debug::FullLogger logger;

      Program program;

      for (const auto& sourcePath : target.second.m_sourceFilepaths) {
        std::cout << "Working on file \"" << sourcePath.string() << "\"\n";
        const auto src = FileHelper::openFileToString(sourcePath);
        // std::cout << src << '\n';
        SpasmLexer lexer;
        auto preprocessedTokens = lexer.run(src, sourcePath);
        // std::cout << "PREPROC\n";
        // for (const auto& tk : preprocessedTokens.m_tokens) {
        //   std::cout << tk.value << '\n';
        // }
        // std::cout << "PREPROCDONE\n";
        Preprocessor preproc;
        auto processedTokens = preproc.run(preprocessedTokens, target.second, program, &logger);
        // std::cout << "POSTPROC\n";
        // for (const auto& tk : processedTokens.m_tokens) {
        //   std::cout << tk.value << '\n';
        // }
        Parser parser;
        parser.ParseTokens(processedTokens, arch, &logger);
        
      }
      std::cout << logger.Errors.m_messages.size() << '\n';
      if (!options.silent) {
        while (!logger.Errors.isEmpty()) {
          std::cout << logger.Errors.consumeMessage() << '\n';
        }
      }

      if (options.warns) {
        while (!logger.Warnings.isEmpty()) {
          std::cout << logger.Warnings.consumeMessage() << '\n';
        }
      }
      if (options.debugs) {
        while (!logger.Debugs.isEmpty()) {
          std::cout << logger.Debugs.consumeMessage() << '\n';
        }
      }
    }
  }
}