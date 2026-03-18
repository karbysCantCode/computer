#include "Spasm/Parser.hpp"

namespace Spasm {
Program Parser::ParseTokens(TokenHolder& tokenHolder, Debug::FullLogger* logger) {
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
}
}