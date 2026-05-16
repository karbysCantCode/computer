#include "preprocessor.hpp"
#include "c.hpp"
#include "Helpers/FileHelper.hpp"
namespace C {

TokenHolder Preprocessor::run(TranslationUnit& translationUnit, TokenHolder& holder) {
  TokenHolder returnHolder;

  holder.reset();
  while (holder.notAtEnd()) {
    //if (!holder.matchCategory(Token::Category::PREPROCESSOR)) returnHolder.m_tokens.push_back(holder.consume());
    const Token& token = holder.consume();
    switch (token.type)
    {
    case Token::Type::PP_DEFINE: {
      if (!holder.match(Token::Type::IDENTIFIER)) {
        logError(token, "Expected an identifier.");
        break;
      }
      const Token& identifierToken = holder.consume();
      const auto preexistingIt = translationUnit.macros.find(identifierToken.value);
      if (preexistingIt != translationUnit.macros.end()) {
        logError(identifierToken, std::format("Macro redefinition of \"{}\", originally defined at {}", identifierToken.value, preexistingIt->second.location.toString()));
        auto* tk = &holder.peek();
        while (tk->type != Token::Type::NEWLINE && holder.notAtEnd()) {
          if (tk->type == Token::Type::OT_BACKSLASH) {
            holder.skipUntilAfterType(Token::Type::NEWLINE);
            tk = &holder.peek();
          } else {
            tk = &holder.consume();
          }
        }
        break;
      } 
      //construct a macro

      Macro macro(token.location, identifierToken.value);

      if (holder.match(Token::Type::OT_OPENPAREN)) {
        bool exit = false;
        macro.hasArgs = true;
        while (!holder.match(Token::Type::OT_CLOSEPAREN) && holder.notAtEnd() && !exit) {
          holder.skip();
          if (!holder.match(Token::Type::IDENTIFIER)) {
            logError(holder.peek(), std::format("Expected an identifier."));
            break;
          }

          macro.args.push_back(&holder.consume());

          switch (holder.peek().type) {
            case Token::Type::OT_CLOSEPAREN:
            exit = true;
            break;
            case Token::Type::OT_COMMA:
            holder.skip();
            break;
            default:
            logError(holder.peek(), std::format("Expected ',' or ')', got {}", holder.peek().value));
            exit = true;
            break;
          }
        }
        holder.skipUntilAfterType(Token::Type::OT_CLOSEPAREN); // skip ')'
      } 
      //consume body
      bool exit = false;
      while (!exit && holder.notAtEnd()) {
        switch (holder.peek().type) {
          case Token::Type::NEWLINE:
            exit = true;
            break;
          case Token::Type::OT_BACKSLASH:
            holder.skipUntilAfterType(Token::Type::NEWLINE);
            break;
          default:
            macro.contents.m_tokens.push_back(holder.consume());
            break;
        }
      }

      translationUnit.macros.emplace(macro.name, std::move(macro));
      break;
    }
    case Token::Type::PP_ELIF:
      logError(token, "ELIF PREPROCESSOR UNSUPPORTED.");
      break;
    case Token::Type::PP_ELSE:
      logError(token, "ELSE PREPROCESSOR UNSUPPORTED.");
      break;
    case Token::Type::PP_ENDIF:
      logError(token, "ENDIF PREPROCESSOR UNSUPPORTED.");
      break;
    case Token::Type::PP_IF:
      logError(token, "IF PREPROCESSOR UNSUPPORTED.");
      break;
    case Token::Type::PP_IFDEF:
      logError(token, "IFDEF PREPROCESSOR UNSUPPORTED.");
      break;
    case Token::Type::PP_IFNDEF:
      logError(token, "IFNDEF PREPROCESSOR UNSUPPORTED.");
      break;
    case Token::Type::PP_INCLUDE: {
      if (!holder.match(Token::Type::STRING)) {
        logError(token, "Expected a filepath.");
        break;
      }

      const std::filesystem::path includePath = std::filesystem::path(holder.consume().value);
      const std::filesystem::path finalPath = translationUnit.searchincludedDirectoriesForFile(includePath);
      
      if (finalPath.empty()) {
        logError(token, std::format("File \"{}\" included here, but not found.", includePath.string()));
        break;
      }

      const auto it = translationUnit.includedFilesToNotReinclude.find(finalPath);
      if (it != translationUnit.includedFilesToNotReinclude.end()) {
        break;
      }

      TranslationUnit tempUnit(translationUnit);
      tempUnit.sourcePath = finalPath;
      tempUnit.source = FileHelper::openFileToString(finalPath);

      const auto tempHolder = preprocessStage(tempUnit);
      if (tempUnit.pragmaOnce) translationUnit.includedFilesToNotReinclude.insert(finalPath);
      const auto backit = returnHolder.m_tokens.end();
      returnHolder.m_tokens.resize(tempHolder.m_tokens.size() + returnHolder.m_tokens.size());
      returnHolder.m_tokens.insert(backit, tempHolder.m_tokens.begin(), tempHolder.m_tokens.end());
      break;
    }
    case Token::Type::PP_UNDEF:
      if (!holder.match(Token::Type::IDENTIFIER)) {
        logError(holder.peek(), std::format("Expected an identifier."));
        break;
      }

      translationUnit.macros.erase(holder.consume().value);
      break;
    case Token::Type::PP_LINE:
      logError(token, "LINE PREPROCESSOR UNSUPPORTED.");
      break;
    case Token::Type::PP_PRAGMA: {
      if (!holder.match(Token::Type::IDENTIFIER)) {
        logError(token, "Expected a pragma.");
        break;
      }
      const auto pragma = holder.consume().value;
      if (pragma == "once") translationUnit.pragmaOnce = true;
      else {
        logError(token, std::format("Pragma \"{}\" unknown or unsupported.", pragma));
        break;
      }

      break;
    }
    case Token::Type::PP_UNRECOGNISED:
      logError(token, std::format("Unrecognised preprocessor directive \"{}\"", token.value));
      break;
    
    default:
      const auto it = translationUnit.macros.find(token.value);
      if (it == translationUnit.macros.end()) {
        returnHolder.m_tokens.push_back(token);
        break;
      }
      invokeMacro(it->second, translationUnit, holder, returnHolder);
      break;
    }
  }

  return returnHolder;
}

// identifier already consumed
void Preprocessor::invokeMacro(Macro& topMacro, TranslationUnit& translationUnit, TokenHolder& input, TokenHolder& output) {
  std::unordered_set<Macro*> invoked;
  std::stack<TokenHolder> bodyStack;
  std::stack<std::unordered_map<std::string_view, std::vector<Token*>>> argumentStack;
  std::stack<Macro*> macroStack;

  invoked.insert(&topMacro);
  macroStack.push(&topMacro);
  argumentStack.push(parseMacroArguments(topMacro, input));
  bodyStack.push(substituteMacroContents(argumentStack.top(), *macroStack.top()));


  while (!bodyStack.empty()) {
    auto& currentHolder = bodyStack.top();

    while (currentHolder.notAtEnd()) {
      const auto& token = currentHolder.consume();
      const auto it = translationUnit.macros.find(token.value);

      if (it == translationUnit.macros.end()) {
        output.m_tokens.push_back(token);
        continue;
      }

      if (invoked.find(&it->second) != invoked.end()) {
        logError(token, std::format("Recursive macro usage of \"{}\"", token.value));
        if (it->second.hasArgs) {
          currentHolder.skipUntilAfterType(Token::Type::OT_CLOSEPAREN, true);
        }
        continue;
      }

      invoked.emplace(&it->second);
      macroStack.push(&it->second);
      argumentStack.push(parseMacroArguments(it->second, currentHolder));
      bodyStack.push(substituteMacroContents(argumentStack.top(), it->second));
      break;
    }

    if (currentHolder.isAtEnd() && &bodyStack.top() == &currentHolder) {
      bodyStack.pop();
      argumentStack.pop();
      invoked.erase(macroStack.top());
      macroStack.pop();
    }
  }
}

TokenHolder Preprocessor::substituteMacroContents(std::unordered_map<std::string_view, std::vector<Token*>>& args, Macro& macro) {
  TokenHolder holder;
  holder.reset();

  for (const auto& token : macro.contents.m_tokens) {
    const auto it = args.find(token.value);
    if (it != args.end()) {
      for (const auto tokPtr : it->second) {
        holder.m_tokens.push_back(*tokPtr);
      }
    } else {
      holder.m_tokens.push_back(token);
    }
  }

  return std::move(holder);
}

std::unordered_map<std::string_view, std::vector<Token*>> Preprocessor:: parseMacroArguments(Macro& macro, TokenHolder& toParse) {
  std::unordered_map<std::string_view, std::vector<Token*>> retMap;

  if (!macro.hasArgs) return retMap;
  if (!toParse.match(Token::Type::OT_OPENPAREN)) {
    logError(toParse.peek(), std::format("Expected '(', got \"{}\"", toParse.peek().value));
    return retMap;
  }
  toParse.skip(); // skip first '('
  size_t argc = 0;
  while (toParse.notAtEnd() && argc < macro.args.size()) {
    auto [it, success] = retMap.emplace(macro.args[argc]->value, std::vector<Token*>());
    Token::Type end = macro.args.size() - argc <= 1 ? Token::Type::OT_CLOSEPAREN : Token::Type::OT_COMMA;

    toParse.skipUntilAfterType(end, it->second, true);
    // if (!toParse.match({Token::Type::OT_CLOSEPAREN, Token::Type::OT_COMMA})) {
    //   logError(toParse.peek(), std::format("Expected an argument, got \"{}\"", toParse.peek().value));
    //   toParse.skipUntilAfterType(Token::Type::OT_CLOSEPAREN, true);
    // }
  }

  if (!(macro.args.size() > 0)) {
    toParse.skipUntilAfterType(Token::Type::OT_CLOSEPAREN);
  }

  return std::move(retMap);
}
}