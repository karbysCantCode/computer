#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <variant>

//#include "arch.hpp"
#include "spasm.hpp"
#include "debugHelpers.hpp"

struct LineColPair {
  int line = -1;
  int col = -1;

  LineColPair(int ln, int cl) : line(ln), col(cl) {}
};

struct Macro {
  std::string target;

  virtual ~Macro() = default;

  //returns the index after the replacement
  virtual size_t replace(size_t fillIndex, std::vector<Spasm::Lexer::Token>& replacementStream) {return 0;}
};

struct FunctionMacro : Macro {
  std::unordered_map<std::string, size_t> args;
  std::vector<Spasm::Lexer::Token> replacement;

  size_t replace(size_t fillIndex, std::vector<Spasm::Lexer::Token>& replacementStream) override {
    std::vector<std::vector<Spasm::Lexer::Token>> localArgs;
    std::vector<LineColPair> localArgBlame;

    size_t argCount = 0;
    bool isBlame = true;
    size_t currentIndex = fillIndex + 1;

    std::vector<Spasm::Lexer::Token> currentArg;
    while (localArgs.size() < args.size() && currentIndex < replacementStream.size()) {
      const Spasm::Lexer::Token& token = replacementStream[currentIndex];
      currentIndex++;
      if (token.m_type == Spasm::Lexer::Token::Type::COMMA) {argCount++; isBlame = true; localArgs.push_back(currentArg); currentArg.clear(); continue;}
      if (isBlame) {isBlame = false; localArgBlame.emplace_back(token.m_line, token.m_column);}
      currentArg.push_back(token);
    }
    localArgs.push_back(currentArg);

    replacementStream.erase(replacementStream.begin() + fillIndex, replacementStream.begin() + currentIndex);

    currentIndex = fillIndex;
    for (const auto& token : replacement) {
      auto it = args.find(token.m_value);
      if (it != args.end()) {
        replacementStream.insert(replacementStream.begin() + currentIndex, localArgs[it->second].begin(), localArgs[it->second].end());
        currentIndex += localArgs[it->second].size();
      } else {
        replacementStream.insert(replacementStream.begin() + currentIndex, token);
        currentIndex++;
      }
    }

    return currentIndex;
  }
};

struct ReplacementMacro : Macro {
  Spasm::Lexer::Token replacementToken{Spasm::Lexer::Token::Type::UNASSIGNED};

  size_t replace(size_t fillIndex, std::vector<Spasm::Lexer::Token>& replacementStream) override {
    replacementStream[fillIndex] = replacementToken;
    return fillIndex + 1;
  }

  ReplacementMacro() {}
};  

// 
bool preprocessSpasm(std::vector<Spasm::Lexer::Token>& spasmTokens, Debug::FullLogger* logger = nullptr) {
  size_t index = 0;
  auto isAtEnd = [&]() {
    return !(index<spasmTokens.size());
  };
  auto peek = [&](size_t peekDistance = 0) {
    return index+peekDistance < spasmTokens.size() ? spasmTokens[index+peekDistance] : Spasm::Lexer::Token("EOF", Spasm::Lexer::Token::Type::UNASSIGNED, -1,-1);
  };
  auto advance = [&]() {
    auto retVal = isAtEnd() ? Spasm::Lexer::Token("EOF", Spasm::Lexer::Token::Type::UNASSIGNED, -1,-1) : spasmTokens[index];
    index++;
    return retVal;
  };

  #define logError(errorMessage) \
    if (logger != nullptr) {logger->Errors.logMessage(errorMessage);}


  std::unordered_map<std::string, std::unique_ptr<Macro>> macroMap;
  
  #define ContinueAndLogIfAtEnd(errorMessage) \
  if (isAtEnd()) {logError(errorMessage); continue;}
  #define EOF_BEFORE_COMPLETE_ERR "Macro \"" + macroName + "\" hit EOF before definition finished."

  while (!isAtEnd()) {
    Spasm::Lexer::Token token = peek();


    auto macroIt = macroMap.find(token.m_value);
    if (macroIt != macroMap.end()) 
      {index = macroIt->second->replace(index, spasmTokens); continue;}

    if (token.m_type == Spasm::Lexer::Token::Type::DIRECTIVE && !isAtEnd()) {
      switch (token.m_nicheType) {

        case Spasm::Lexer::Token::NicheType::DIRECTIVE_DEFINE: {
          size_t defineStartIndex = index;
          advance();
          std::string macroName = advance().m_value;
          ContinueAndLogIfAtEnd(EOF_BEFORE_COMPLETE_ERR);
          if (peek().m_type == Spasm::Lexer::Token::Type::OPENPAREN) {
            advance();
            // is a function macro
            auto macro = std::make_unique<FunctionMacro>();
            bool isEnd = false;
            size_t argCount = 0;
            while (!isAtEnd() && !isEnd) {
              //get args
              if (peek().m_type == Spasm::Lexer::Token::Type::CLOSEPAREN) {advance(); break;}
              macro->args[advance().m_value] = argCount;
              argCount++;
              switch (peek().m_type) {
                case Spasm::Lexer::Token::Type::COMMA:
                advance();
                continue;
                break;
                case Spasm::Lexer::Token::Type::CLOSEPAREN:
                advance();
                isEnd = true;
                break;
                default:
                isEnd = true;
                logError("Unexpected token type (comma or close paren expected), got " + std::string(peek().typeToString()) + " at " + peek().positionToString());
                break;
              } 
            }
            //
            if (peek().m_type == Spasm::Lexer::Token::Type::OPENBLOCK) {
              advance();
              while (peek().m_type != Spasm::Lexer::Token::Type::CLOSEBLOCK && !isAtEnd()) {
                macro->replacement.push_back(advance());
              }
              advance();
            } else {
              logError("Function block missing, expected curly brace, got " + std::string(peek().typeToString()) + " at " + peek().positionToString() + " value: \"" + peek().m_value + "\"");
            }
            macroMap[macroName] = std::move(macro);
          } else {
            // is a replacement macro
            ContinueAndLogIfAtEnd(EOF_BEFORE_COMPLETE_ERR);
            auto macro = std::make_unique<ReplacementMacro>();
            macro->replacementToken = advance();
            macroMap[macroName] = std::move(macro);
          }

          spasmTokens.erase(spasmTokens.begin() + defineStartIndex, spasmTokens.begin() + index);
          index = defineStartIndex;
        }
        break;

        case Spasm::Lexer::Token::NicheType::DIRECTIVE_INCLUDE: {
          logError("INCLUDE DIRECTIVE NOT IMPLEMENTED FOR PREPROCESSOR");
          advance();
        }
        break;
        case Spasm::Lexer::Token::NicheType::DIRECTIVE_ENTRY: {
          logError("ENTRY DIRECTIVE NOT IMPLEMENTED FOR PREPROCESSOR");
          advance();
        }
        break;
        default:
          logError("Unrecognise directive \"" + token.m_value + "\" at " + token.positionToString());
        break;
      }
    } else {
      advance();
    }
  }
  std::cout << "Start" << std::endl;
  for (const auto& entry : macroMap) {
    std::cout << entry.first << std::endl;
  }

  std::cout << "S3" << std::endl;


  return true;
}