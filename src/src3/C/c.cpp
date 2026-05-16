#include "c.hpp"

#include <unordered_set>

#include "lexer.hpp"
#include "preprocessor.hpp"
#include "Helpers/FileHelper.hpp"

namespace C {

void compileFile(std::filesystem::path& path, std::filesystem::path& output, std::vector<std::filesystem::path>& includes) {
  /*
  lex
  preprocess
  parse
  asm gen
  */

  TranslationUnit translationUnit;
  translationUnit.sourcePath = path;
  translationUnit.includeDirectories = includes;
  translationUnit.source = FileHelper::openFileToString(path);
  TokenHolder preprocessed = preprocessStage(translationUnit);
  for (const auto& tok : preprocessed.m_tokens) {
    std::cout << tok.value << '\n';
  }
}

TokenHolder preprocessStage(TranslationUnit& translationUnit) {
  CLexer lexer(translationUnit.source, translationUnit.sourcePath);
  TokenHolder unprocessed = lexer.run();

  Preprocessor preprocessor;
  TokenHolder preprocessed = preprocessor.run(translationUnit, unprocessed);

  return preprocessed;
}

}