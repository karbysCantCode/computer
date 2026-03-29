#include "Spasm/Parser.hpp"

namespace Spasm {
Program Parser::ParseTokens(TokenHolder& tokenHolder, Arch::Architecture& arch, Debug::FullLogger* logger) {
  //implementation list
  /*
  [X] Directives
  [X]   Define
  [X]     Replacement
  [X]     Function
  [X]       Field replacement
  [X]   Include
  [X]   Entry
  [X] Instructions
  [X]   Keywords/instruction names
  [X]   Registers
  [X]   Staticly compiled numbers
  [X]   Constants
  [X]   Field range checking
  [X] Labels
  [X]   Basic inheritance
  [X]   Relative labels
  [X] Datatypes
  [X]   Text
  [X]   Array
  [X]   Auto
  [X]   Size checking
  */

  p_logger = logger;
  Program program;

  while (tokenHolder.notAtEnd()) {
    switch (tokenHolder.peek().type) {
      using TY = Token::Type;
      case TY::DIRECTIVE: {
        logError(tokenHolder.peek(), "Preproccessor didn't handle directive.");
        tokenHolder.skip();
        break;
      }
      case TY::IDENTIFIER: {
        parseIdentifier(tokenHolder);
        break;
      }
      default: {
        logError(tokenHolder.peek(), "Unexpected token type.");
        tokenHolder.skip();
        break;
      }
    }
  }

  return program;
}

void Parser::parseIdentifier(TokenHolder& tokenHolder, Arch::Architecture& arch) {
  // probably doesnt actually need to differ register and format,
  //   and could probably actually use datatypes instead...
  switch (arch.getKeywordTypeOfWord(tokenHolder.peek().value))
  {
  case Arch::Architecture::KeywordType::INSTRUCTION:
    /* code */
    break;
  case Arch::Architecture::KeywordType::REGISTER:
    /* code */
    break;
  case Arch::Architecture::KeywordType::FORMAT:
    /* code */
    break;
  case Arch::Architecture::KeywordType::NONE:
    /* code */
    break;
  }
}
}