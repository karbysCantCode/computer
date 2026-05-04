#include "Spasm/Preprocessor.hpp"

#include "Helpers/FileHelper.hpp"
#include <format>

namespace Spasm {
TokenHolder Preprocessor::run(TokenHolder& tokenHolder, SMake::Target& target, Spasm::Program::TranslationUnit& translationUnit, Spasm::Program& targetProgram, std::unordered_map<std::filesystem::path, std::vector<Program::TranslationUnit*>>&  dependantTranslationUnitMap, Debug::FullLogger* logger) {
  p_logger = logger;
  TokenHolder newTokenHolder;

  //

  auto myIt = p_macroMap.emplace(translationUnit.m_sourcePath, InnerMacroMapType{});
  auto& myMacroMap = myIt.first->second;

  // std::stack<TokenHolder*> processStack;
  // processStack.push(&tokenHolder);

  std::vector<std::unique_ptr<TokenHolder>> temporaryOwner;

  // std::unordered_set<std::string_view> invokedMacros;
  // std::stack<std::string_view> invokedMacroStack;

  while (tokenHolder.notAtEnd()) {
    if (!tokenHolder.match(Token::Type::DIRECTIVE)) {
      const auto token = tokenHolder.consume();
      const auto it = myMacroMap.find(token.value);
      if (it != myMacroMap.end()) {
        std::cout << "MACRO \"" << it->second->name << "\" called at " << token.location.toString() << std::endl;
        // if (invokedMacros.find(token.value) != invokedMacros.end()) {
        //   logError(token, "Recursive macro use.");
        //   tokenHolder.skipUntilAfterType(Token::Type::NEWLINE);
        //   continue;
        // }

        // auto temporaryHolder = std::make_unique<TokenHolder>(processMacroInvocation(it->second.get(), tokenHolder));
        // processStack.push(temporaryHolder.get());
        // temporaryOwner.push_back(std::move(temporaryHolder));

        const auto temporaryHolder = processMacroInvocation(p_logger, translationUnit, it->second.get(), tokenHolder, myMacroMap);
        newTokenHolder.m_tokens.insert(newTokenHolder.m_tokens.end(), temporaryHolder.m_tokens.begin(), temporaryHolder.m_tokens.end());

        //invokedMacros.insert(token.value);
        //invokedMacroStack.push(token.value);
      } else {
        // if (!token.type == Token::Type::NEWLINE);
        newTokenHolder.m_tokens.push_back(token);
      }

      continue;
    }

    switch (tokenHolder.peek().nicheType) {
      using NT = Token::NicheType;
      case NT::DIRECTIVE_DEFINE: 
        processDefine(tokenHolder, newTokenHolder, myMacroMap, target, translationUnit, targetProgram, dependantTranslationUnitMap);
        break;
      case NT::DIRECTIVE_ENTRY:
        processEntry(tokenHolder, target);
      break;
      case NT::DIRECTIVE_INCLUDE:
        processInclude(tokenHolder, target, translationUnit, targetProgram, myMacroMap, dependantTranslationUnitMap);
      break;
      default:
      logError(tokenHolder.consume(), "Unknown directive.");
      break;
    }
  }


  return newTokenHolder;
}

TokenHolder Preprocessor::processMacroInvocation(Debug::FullLogger* logger, Program::TranslationUnit& translationUnit, AbstractMacro* macro, TokenHolder& tokenHolder, InnerMacroMapType& macroMap) {
  TokenHolder newTokenHolder;
  macro->invokeCount++;
  switch (macro->getKind())
  {
  case AbstractMacro::Kind::FUNCTION:
  {
    // handle function macro
    auto functionMacro = static_cast<FunctionMacro*>(macro);
    std::vector<std::vector<Token>> arguments;
    while (tokenHolder.notAtEnd() && !tokenHolder.match(Token::Type::NEWLINE)) {
      std::vector<Token> currentArgument;
      while (tokenHolder.notAtEnd() && !tokenHolder.match(Token::Type::NEWLINE) && !tokenHolder.match(Token::Type::COMMA)) {
        const auto& token = tokenHolder.consume();
        const auto it = macroMap.find(token.value);
        if (it != macroMap.end()) {
          if (it->second->getKind() == AbstractMacro::Kind::REPLACEMENT) {
            const TokenHolder replacementTokens = processMacroInvocation(logger, translationUnit, it->second.get(), tokenHolder, macroMap);
            currentArgument.insert(currentArgument.end(), replacementTokens.m_tokens.begin(), replacementTokens.m_tokens.end());
            continue;
          } else {
            logError(logger, token, std::format("Function macros cannot be passed as arguments into function macros."));
          }
        }
        currentArgument.push_back(token);
      }
      if (tokenHolder.match(Token::Type::COMMA)) {
        tokenHolder.skip();
      }
      arguments.push_back(currentArgument);
    }

    functionMacro->fillWithReplacedContents(logger, translationUnit, newTokenHolder, macroMap, arguments);
  }
  break;
  case AbstractMacro::Kind::REPLACEMENT:
    {
      //auto replacementMacro = static_cast<ReplacementMacro*>(macro);
      // handle replacement macro
      newTokenHolder.m_tokens.insert(
        newTokenHolder.m_tokens.end(),
        macro->contents.m_tokens.begin(),
        macro->contents.m_tokens.end()
      );
    }
    break;
  
  default:
    break;
  }

  return newTokenHolder;
}

TokenHolder Preprocessor::recurseDefineContents(AbstractMacro* macro, InnerMacroMapType& macroMap, SMake::Target& target, Spasm::Program::TranslationUnit& translationUnit, Spasm::Program& targetProgram, std::unordered_map<std::filesystem::path, std::vector<Program::TranslationUnit*>>& dependantTranslationUnitMap) {
  if (!macro) assert(false);

  TokenHolder newTokenHolder;

  std::stack<TokenHolder*> processStack;
  processStack.push(&macro->contents);

  std::vector<std::unique_ptr<TokenHolder>> temporaryOwner;

  std::unordered_set<AbstractMacro*> invokedMacros;
  std::stack<AbstractMacro*> invokedMacroStack;
  invokedMacros.insert(macro);
  invokedMacroStack.push(macro);

  while (!processStack.empty()) {
    auto& currentHolder = *processStack.top();

    while (currentHolder.notAtEnd()) {
      if (!currentHolder.match(Token::Type::DIRECTIVE)) {
        const auto token = currentHolder.consume();
        const auto it = macroMap.find(token.value);
        if (it != macroMap.end() && !currentHolder.match(Token::Type::PERIOD) && !currentHolder.match(Token::Type::PERIOD, -2) && !currentHolder.match(Token::Type::COLON)) {
          if (invokedMacros.find(it->second.get()) != invokedMacros.end()) {
            logError(token, "Recursive macro use.");
            continue;
          }

          auto temporaryHolder = std::make_unique<TokenHolder>(processMacroInvocation(p_logger, translationUnit, it->second.get(), currentHolder, macroMap));
          processStack.push(temporaryHolder.get());
          temporaryOwner.push_back(std::move(temporaryHolder));

          invokedMacros.insert(it->second.get());
          invokedMacroStack.push(it->second.get());
          break;
        } else {
          newTokenHolder.m_tokens.push_back(token);
        }

        continue;
      }

      switch (currentHolder.peek().nicheType) {
        using NT = Token::NicheType;
        case NT::DIRECTIVE_DEFINE: 
          logError(currentHolder.peek(), "Macro definition forbidden inside macro defintions.");
          currentHolder.skip();
          if (currentHolder.match(Token::Type::OPENPAREN)) {
            currentHolder.skipUntilAfterType(Token::Type::CLOSEPAREN);
            size_t depth = 1;
            currentHolder.skipUntilAfterType(Token::Type::OPENBLOCK);
            while (depth) {
              switch (currentHolder.peek().type)
              {
              case Token::Type::OPENBLOCK:
                depth++;
                break;
              case Token::Type::CLOSEBLOCK:
                depth--;
                break;
              
              default:
                break;
              }

              currentHolder.skip();
            }
          } else {
            currentHolder.skipUntilAfterType(Token::Type::NEWLINE);
          }
          break;
        case NT::DIRECTIVE_ENTRY:
          processEntry(currentHolder, target);
        break;
        case NT::DIRECTIVE_INCLUDE:
          processInclude(currentHolder, target, translationUnit, targetProgram, macroMap, dependantTranslationUnitMap);
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

  return std::move(newTokenHolder);
}

void Preprocessor::processDefine(TokenHolder& tokenHolder, TokenHolder& newTokenHolder, InnerMacroMapType& myMacroMap, SMake::Target& target, Spasm::Program::TranslationUnit& translationUnit, Spasm::Program& targetProgram, std::unordered_map<std::filesystem::path, std::vector<Program::TranslationUnit*>>& dependantTranslationUnitMap) {
  tokenHolder.skip();

  const auto& identifierToken = tokenHolder.consume();

  if (tokenHolder.match(Token::Type::OPENPAREN)) {
    auto newMacro = std::make_shared<FunctionMacro>(parseFunctionMacroDefinition(tokenHolder, newTokenHolder));
    newMacro->name = identifierToken.value;

    //processStack.push(&newMacro.contents);

    auto [it, success] = myMacroMap.emplace(identifierToken.value, std::move(newMacro));
    if (success) {
      //recurseDefineContents(it->second.get(),myMacroMap,target,translationUnit,targetProgram,dependantTranslationUnitMap);
    }
  } else {
    auto newMacro = std::make_shared<ReplacementMacro>(parseReplacementMacroDefinition(tokenHolder, newTokenHolder));
    newMacro->name = identifierToken.value;

    //processStack.push(&newMacro.contents);

    auto [it, success] = myMacroMap.emplace(identifierToken.value, std::move(newMacro));
    if (success) {
      //recurseDefineContents(it->second.get(),myMacroMap,target,translationUnit,targetProgram,dependantTranslationUnitMap);
    }
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
  size_t blockDepth = 1;
  while (blockDepth && tokenHolder.notAtEnd()) {
    const auto token = tokenHolder.consume();
    switch (token.type)
    {
    case Token::Type::OPENBLOCK:
      macro.contents.m_tokens.push_back(token);
      blockDepth++;
      break;
    case Token::Type::CLOSEBLOCK:
      blockDepth--;
      if (blockDepth) {
        macro.contents.m_tokens.push_back(token);
      }
      break;
    
    default:
      macro.contents.m_tokens.push_back(token);
      break;
    }

    // std::cout << "macro '" << macro.name << "':\n";
    // for (const auto& token : macro.contents.m_tokens) {
    //   std::cout << token.value << '\n';
    // }
    // std::cout << "macro END" << "\n";
  }

  //method change, this is redundant
  //tokenHolder.skip(); // skip '}'

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
    macro.contents.m_tokens.push_back(tokenHolder.consume());
  }

  return macro;
}

bool Preprocessor::FunctionMacro::fillWithReplacedContents(Debug::FullLogger* logger, Program::TranslationUnit& translationUnit, TokenHolder& tokenHolder, InnerMacroMapType& macroMap, std::vector<std::vector<Token>> replacementArgs) {
  if (replacementArgs.size() != arguments.size()) {
    return false;
  }
  TokenHolder fpass;
  fpass.reset();
  contents.reset();
  while (!contents.isAtEnd()) {
    const auto& token = contents.consume();
    const auto it = arguments.find(token.value);
    if (it != arguments.end()) {
      const auto& repl = replacementArgs[it->second];
      fpass.m_tokens.insert(
        fpass.m_tokens.end(),
        repl.begin(),
        repl.end()
      );
    } else if (token.nicheType == Token::NicheType::MACRO_UNIQUE) {
      auto str = std::make_unique<std::string>('.' + getMangledName() + std::string(token.value));
      fpass.m_tokens.emplace_back(std::string_view(str->data(), str->size()), Token::Type::IDENTIFIER, token.location);
      translationUnit.m_stringOwner.push_back(std::move(str));
    } else {
      fpass.m_tokens.push_back(token);
    }
  }

  while (!fpass.isAtEnd()) {
    const auto& token = fpass.consume();
    const auto it = macroMap.find(token.value);
    if (it != macroMap.end()) {
      const auto temporary = processMacroInvocation(logger, translationUnit,it->second.get(), fpass, macroMap);
      tokenHolder.m_tokens.insert(
        tokenHolder.m_tokens.end(),
        temporary.m_tokens.begin(),
        temporary.m_tokens.end()
      );
    } else {
      tokenHolder.m_tokens.push_back(token);
    }
  }

  invokeCount++;

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
        logError(macro.second->definitionToken, std::format("Macro \"{}\" is already defined in \"{}\".", macro.second->name, path.string()));
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