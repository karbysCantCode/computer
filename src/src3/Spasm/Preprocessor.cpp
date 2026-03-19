#include "Spasm/Preprocessor.hpp"

#include <format>

namespace Spasm {
TokenHolder Preprocessor::run(TokenHolder& tokenHolder, SMake::Target& target) {
  while (tokenHolder.notAtEnd()) {
    if (!tokenHolder.match(Token::Type::DIRECTIVE)) {continue;}

    switch (tokenHolder.peek().nicheType) {
      using NT = Token::NicheType;
      case NT::DIRECTIVE_DEFINE: 

      break;
      case NT::DIRECTIVE_ENTRY:

      break;
      case NT::DIRECTIVE_INCLUDE:

      break;
      default:
      logError(tokenHolder.peek(), "Unknown directive.");
      break;
    }
  }
}

void Preprocessor::processDefine(TokenHolder& tokenHolder, TokenHolder& newTokenHolder) {
  tokenHolder.skip();
}
void Preprocessor::processInclude(TokenHolder& tokenHolder, SMake::Target& target) {
  tokenHolder.skip();
  if (!tokenHolder.match(Token::Type::IDENTIFIER)) {
    logError(tokenHolder.peek(), "Expected identifier for @include.");
    return;
  }

  const auto& includeStr = tokenHolder.consume();
  
}
void Preprocessor::processEntry(TokenHolder& tokenHolder, SMake::Target& target) {
  tokenHolder.skip();
  if (!tokenHolder.match(Token::Type::IDENTIFIER)) {
    logError(tokenHolder.peek(), "Expected identifier for @entry.");
    return;
  }

  if (!target.m_entrySymbol.empty()) {
    logWarning(tokenHolder.peek(), std::Format("Entry symbol of target \"{}\" is being changed from \"{}\" to \"{}\" ", target.m_name, target.m_entrySymbol, tokenHolder.peek().value));
  }

  target.m_entrySymbol = tokenHolder.consume().value;

}
}