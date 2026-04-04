#include "Spasm/Preprocessor.hpp"

#include <format>

namespace Spasm {
TokenHolder Preprocessor::run(TokenHolder& tokenHolder, SMake::Target& target, Spasm::Program& targetProgram, Debug::FullLogger* logger) {
  p_logger = logger;
  TokenHolder newTokenHolder;

  std::stack<TokenHolder> processStack;
  processStack.push(tokenHolder);

  std::unordered_set<std::string_view> invokedMacros;
  std::stack<std::string_view> invokedMacroStack;

  while (!processStack.empty()) {
    auto& currentHolder = processStack.top();

    while (currentHolder.notAtEnd()) {
      if (!currentHolder.match(Token::Type::DIRECTIVE)) {
        const auto token = currentHolder.consume();
        const auto it = p_macroMap.find(token.value);
        if (it != p_macroMap.end()) {
          if (invokedMacros.find(token.value) != invokedMacros.end()) {
            logError(token, "Recursive macro use.");
            continue;
          }
          processStack.push(processMacroInvocation(it->second, currentHolder));
          invokedMacros.insert(token.value);
          invokedMacroStack.push(token.value);
          break;
        } else {
          newTokenHolder.m_tokens.push_back(token);
        }

        continue;
      }

      switch (currentHolder.peek().nicheType) {
        using NT = Token::NicheType;
        case NT::DIRECTIVE_DEFINE: 
          processDefine(currentHolder, newTokenHolder);
        break;
        case NT::DIRECTIVE_ENTRY:
          processEntry(currentHolder, target);
        break;
        case NT::DIRECTIVE_INCLUDE:
          processInclude(currentHolder, target, targetProgram);
        break;
        default:
        logError(currentHolder.consume(), "Unknown directive.");
        break;
      }
    }

    if (currentHolder.isAtEnd()) {
      processStack.pop();

      if (!invokedMacroStack.empty()) {
        invokedMacros.erase(invokedMacroStack.top());
        invokedMacroStack.pop();
      }
    }
  }

  return newTokenHolder;
}

TokenHolder Preprocessor::processMacroInvocation(std::variant<FunctionMacro, ReplacementMacro>& macro, TokenHolder& tokenHolder) {
  TokenHolder newTokenHolder;
  std::visit([&](auto& m) {
    using T = std::decay_t<decltype(m)>;

    if constexpr (std::is_same_v<T, FunctionMacro>) {
      // handle function macro
      std::vector<std::vector<Token>> arguments;
      while (tokenHolder.notAtEnd() && !tokenHolder.match(Token::Type::NEWLINE)) {
        std::vector<Token> currentArgument;
        while (tokenHolder.notAtEnd() && !tokenHolder.match(Token::Type::NEWLINE) && !tokenHolder.match(Token::Type::COMMA)) {
          currentArgument.push_back(tokenHolder.consume());
        }
        if (tokenHolder.match(Token::Type::COMMA)) {
          tokenHolder.skip();
        }
        arguments.push_back(currentArgument);
      }

      m.fillWithReplacedContents(newTokenHolder,arguments);
    } else if constexpr (std::is_same_v<T, ReplacementMacro>) {
      // handle replacement macro
      newTokenHolder.m_tokens.insert(
        newTokenHolder.m_tokens.end(),
        m.contents.begin(),
        m.contents.end()
      );
    }
  }, macro);

  return newTokenHolder;
}

void Preprocessor::processDefine(TokenHolder& tokenHolder, TokenHolder& newTokenHolder) {
  tokenHolder.skip();

  const auto& identifierToken = tokenHolder.consume();

  if (tokenHolder.match(Token::Type::OPENPAREN)) {
    FunctionMacro newMacro = parseFunctionMacroDefinition(tokenHolder, newTokenHolder);
    newMacro.name = identifierToken.value;

    p_macroMap.emplace(identifierToken.value, std::move(newMacro));
  } else {
    ReplacementMacro newMacro = parseReplacementMacroDefinition(tokenHolder, newTokenHolder);
    newMacro.name = identifierToken.value;

    p_macroMap.emplace(identifierToken.value, std::move(newMacro));
  }
}

Preprocessor::FunctionMacro Preprocessor::parseFunctionMacroDefinition(TokenHolder& tokenHolder, TokenHolder& newTokenHolder) {
  FunctionMacro macro;

  tokenHolder.skip();

  while (tokenHolder.notAtEnd() && tokenHolder.match(Token::Type::IDENTIFIER)) {
    macro.addArgument(tokenHolder.consume().value);
    if (tokenHolder.match(Token::Type::COMMA)) {tokenHolder.skip();}
  }
  if (!tokenHolder.match(Token::Type::CLOSEPAREN)) {
    logError(tokenHolder.peek(), std::format("Expected ')', got '{}'", tokenHolder.peek().value));
    return FunctionMacro(); // dont let it slide bruh
  }

  tokenHolder.skip(); // skip ')'

  tokenHolder.skipUntilAfterType(Token::Type::OPENBLOCK);

  while (!tokenHolder.match(Token::Type::CLOSEBLOCK)) {
    macro.contents.push_back(tokenHolder.consume());
  }

  tokenHolder.skip(); // skip '}'

  // if (!) {
  //   logError(
  //     tokenHolder.peek(), 
  //     std::format(
  //       "Given arguments mismatch expected argument number of macro \"{}\", expected {} args, got {}", 
  //       name, 
  //       arguments.size(), 
  //       replacementArgs.size()
  //     )
  //   );
  // }

  return macro;
}

Preprocessor::ReplacementMacro Preprocessor::parseReplacementMacroDefinition(TokenHolder& tokenHolder, TokenHolder& newTokenHolder) {
  ReplacementMacro macro;
  while (tokenHolder.notAtEnd() && !tokenHolder.match(Token::Type::NEWLINE))
  {
    macro.contents.push_back(tokenHolder.consume());
  }

  return macro;
}

bool Preprocessor::FunctionMacro::fillWithReplacedContents(TokenHolder& tokenHolder, std::vector<std::vector<Token>> replacementArgs) {
  if (replacementArgs.size() != arguments.size()) {
    return false;
  }

  for (const auto& token : contents) {
    const auto it = arguments.find(token.value);
    if (it != arguments.end()) {
      const auto& repl = replacementArgs[it->second];
      tokenHolder.m_tokens.insert(
        tokenHolder.m_tokens.end(),
        repl.begin(),
        repl.end()
      );
    } else {
      tokenHolder.m_tokens.push_back(token);
    }
  }

  return true;
}

void Preprocessor::processInclude(TokenHolder& tokenHolder, SMake::Target& target, Spasm::Program& targetProgram) {
  tokenHolder.skip();
  if (!tokenHolder.match(Token::Type::STRING)) {
    logError(tokenHolder.peek(), "Expected string for @include.");
    return;
  }

  

  const auto includeStr = tokenHolder.consume();
  const auto path = target.searchForPathInIncludes(includeStr.value);
  if (path.empty()) {
    logError(includeStr, std::format("Preprocessor: path not found \"{}\"", includeStr.value));
    return;
  }

  targetProgram.m_includedFilesByPath.insert(path);
  
}
void Preprocessor::processEntry(TokenHolder& tokenHolder, SMake::Target& target) {
  tokenHolder.skip();
  if (!tokenHolder.match(Token::Type::IDENTIFIER)) {
    logError(tokenHolder.peek(), "Expected identifier for @entry.");
    return;
  }

  if (!target.m_entrySymbol.empty()) {
    logWarning(tokenHolder.peek(), std::format("Entry symbol of target \"{}\" is being changed from \"{}\" to \"{}\" ", target.m_name, target.m_entrySymbol, tokenHolder.peek().value));
  }

  target.m_entrySymbol = tokenHolder.consume().value;

}
}