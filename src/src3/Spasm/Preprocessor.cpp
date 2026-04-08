#include "Spasm/Preprocessor.hpp"

#include "Helpers/FileHelper.hpp"
#include <format>

namespace Spasm {
TokenHolder Preprocessor::run(TokenHolder& tokenHolder, SMake::Target& target, Spasm::Program::TranslationUnit& translationUnit, Spasm::Program& targetProgram, std::unordered_map<std::filesystem::path, std::vector<Program::TranslationUnit*>>&  dependantTranslationUnitMap, Debug::FullLogger* logger) {
  p_logger = logger;
  TokenHolder newTokenHolder;

  auto myIt = p_macroMap.emplace(translationUnit.m_sourcePath, InnerMacroMapType{});
  auto& myMacroMap = myIt.first->second;

  std::stack<TokenHolder> processStack;
  processStack.push(tokenHolder);

  std::unordered_set<std::string_view> invokedMacros;
  std::stack<std::string_view> invokedMacroStack;

  while (!processStack.empty()) {
    auto& currentHolder = processStack.top();

    while (currentHolder.notAtEnd()) {
      if (!currentHolder.match(Token::Type::DIRECTIVE)) {
        const auto token = currentHolder.consume();
        const auto it = myMacroMap.find(token.value);
        if (it != myMacroMap.end()) {
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
          processDefine(currentHolder, newTokenHolder, myMacroMap);
        break;
        case NT::DIRECTIVE_ENTRY:
          processEntry(currentHolder, target);
        break;
        case NT::DIRECTIVE_INCLUDE:
          processInclude(currentHolder, target, translationUnit, targetProgram, myMacroMap, dependantTranslationUnitMap);
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

void Preprocessor::processDefine(TokenHolder& tokenHolder, TokenHolder& newTokenHolder, InnerMacroMapType& myMacroMap) {
  tokenHolder.skip();

  const auto& identifierToken = tokenHolder.consume();

  if (tokenHolder.match(Token::Type::OPENPAREN)) {
    FunctionMacro newMacro = parseFunctionMacroDefinition(tokenHolder, newTokenHolder);
    newMacro.name = identifierToken.value;

    myMacroMap.emplace(identifierToken.value, std::move(newMacro));
  } else {
    ReplacementMacro newMacro = parseReplacementMacroDefinition(tokenHolder, newTokenHolder);
    newMacro.name = identifierToken.value;

    myMacroMap.emplace(identifierToken.value, std::move(newMacro));
  }
}

Preprocessor::FunctionMacro Preprocessor::parseFunctionMacroDefinition(TokenHolder& tokenHolder, TokenHolder& newTokenHolder) {
  FunctionMacro macro(tokenHolder.peek());

  tokenHolder.skip();

  while (tokenHolder.notAtEnd() && tokenHolder.match(Token::Type::IDENTIFIER)) {
    macro.addArgument(tokenHolder.consume().value);
    if (tokenHolder.match(Token::Type::COMMA)) {tokenHolder.skip();}
  }
  if (!tokenHolder.match(Token::Type::CLOSEPAREN)) {
    logError(tokenHolder.peek(), std::format("Expected ')', got '{}'", tokenHolder.peek().value));
    return macro; // dont let it slide bruh
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
  ReplacementMacro macro(tokenHolder.peek());
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

void Preprocessor::processInclude(
  TokenHolder& tokenHolder, 
  SMake::Target& target, 
  Program::TranslationUnit& translationUnit, 
  Program& targetProgram, 
  InnerMacroMapType& myMacroMap,
  std::unordered_map<
    std::filesystem::path, 
    std::vector<Program::TranslationUnit*>>& dependantTranslationUnitMap) {
  tokenHolder.skip();
  if (!tokenHolder.match(Token::Type::STRING)) {
    logError(tokenHolder.peek(), "Expected string for @include.");
    return;
  }

  

  const auto includeStr = tokenHolder.consume();
  const auto path = target.searchForPathInIncludes(translationUnit.m_sourcePath, includeStr.value);
  if (path.empty()) {
    logError(includeStr, std::format("Preprocessor: path not found \"{}\"", includeStr.value));
    return;
  }

  if (targetProgram.m_translationUnits.find(path) == targetProgram.m_translationUnits.end()) {
    auto transUnit = std::make_unique<Program::TranslationUnit>();
    auto transUnitPtr = transUnit.get();

    transUnitPtr->m_sourcePath = path;

    targetProgram.m_translationUnits.emplace(path, std::move(transUnit));
    targetProgram.m_translationUnitsToParseAndLink.push(transUnitPtr);
    transUnitPtr->m_source = std::make_unique<std::string>(FileHelper::openFileToString(transUnitPtr->m_sourcePath));

    SpasmLexer lexer;
    auto preprocessedTokens = lexer.run(*transUnitPtr->m_source, transUnitPtr->m_sourcePath);

    transUnitPtr->processedTokens = run(preprocessedTokens, target, *transUnitPtr, targetProgram, dependantTranslationUnitMap, p_logger);
    
    for (auto& macro : p_macroMap[path]) {
      auto [it, inserted] = myMacroMap.emplace(macro.first, macro.second);
      if (!inserted) {
        std::visit([&](auto&& arg){
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, FunctionMacro>) {
              logError(arg.definitionToken, std::format("Macro \"{}\" is already defined in \"{}\".", arg.name, path.string()));
          } else if constexpr (std::is_same_v<T, ReplacementMacro>) {
              logError(arg.definitionToken, std::format("Macro \"{}\" is already defined in \"{}\".", arg.name, path.string()));
          }
        }, it->second);
      }
    }

    //p_macroMap.merge(macroMap.find(path)->second);
    //targetProgram.m_filePathsToCreateTranslationUnitsOf.push(path);
  }

  auto [it, inserted] = translationUnit.m_includedFiles.emplace(path);

  if (dependantTranslationUnitMap.find(path) == dependantTranslationUnitMap.end()) {
    dependantTranslationUnitMap.emplace(path, std::vector<Program::TranslationUnit*>()); 
  }
  dependantTranslationUnitMap[path].push_back(&translationUnit);
  
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