#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <variant>

//#include "arch.hpp"
#include "token.hpp"

struct LineColPair {
  int line = -1;
  int col = -1;

  LineColPair(int ln, int cl) : line(ln), col(cl) {}
};

struct Macro {
  std::string target;

  virtual ~Macro() = default;

  //returns the index after the replacement
  virtual size_t replace(size_t fillIndex, std::vector<Token::Token>& replacementStream) {return 0;}
};

struct FunctionMacro : Macro {
  std::unordered_map<std::string, size_t> args;
  std::vector<Token::Token> replacement;

  size_t replace(size_t fillIndex, std::vector<Token::Token>& replacementStream) override {
    std::vector<std::vector<Token::Token>> localArgs;
    std::vector<LineColPair> localArgBlame;

    size_t argCount = 0;
    bool isBlame = true;
    size_t currentIndex = fillIndex + 1;

    std::vector<Token::Token> currentArg;
    while (localArgs.size() < args.size() && currentIndex < replacementStream.size()) {
      const Token::Token& token = replacementStream[currentIndex];
      currentIndex++;
      if (token.type == Token::TokenTypes::COMMA) {argCount++; isBlame = true; localArgs.push_back(currentArg); currentArg.clear(); continue;}
      if (isBlame) {isBlame = false; localArgBlame.emplace_back(token.line, token.column);}
      currentArg.push_back(token);
    }
    localArgs.push_back(currentArg);

    replacementStream.erase(replacementStream.begin() + fillIndex, replacementStream.begin() + currentIndex);

    currentIndex = fillIndex;
    for (const auto& token : replacement) {
      auto it = args.find(token.value);
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
  Token::Token replacementToken;

  size_t replace(size_t fillIndex, std::vector<Token::Token>& replacementStream) override {
    replacementStream[fillIndex] = replacementToken;
    return fillIndex + 1;
  }
};  

// 
bool preprocessSpasm(std::vector<Token::Token>& spasmTokens, std::vector<std::string>* errList = nullptr) {
  size_t index = 0;
  auto isAtEnd = [&]() {
    return !(index<spasmTokens.size());
  };
  auto peek = [&](size_t peekDistance = 0) {
    return index+peekDistance < spasmTokens.size() ? spasmTokens[index+peekDistance] : Token::Token("EOF", Token::TokenTypes::UNASSIGNED, -1,-1);
  };
  auto advance = [&]() {
    auto retVal = isAtEnd() ? Token::Token("EOF", Token::TokenTypes::UNASSIGNED, -1,-1) : spasmTokens[index];
    index++;
    return retVal;
  };

  auto logError = [&](std::string err) {
    if (errList != nullptr) {errList->push_back(err);}
  };

  std::unordered_map<std::string, std::unique_ptr<Macro>> macroMap;
  
  #define ContinueAndLogIfAtEnd(errorMessage) \
  if (isAtEnd()) {logError(errorMessage); continue;}
  #define EOF_BEFORE_COMPLETE_ERR "Macro \"" + macroName + "\" hit EOF before definition finished."

  std::cout << "S2" << std::endl;
  while (!isAtEnd()) {
    Token::Token token = peek();


    auto macroIt = macroMap.find(token.value);
    if (macroIt != macroMap.end()) 
      {index = macroIt->second->replace(index, spasmTokens); continue;}

    if (token.type == Token::TokenTypes::DIRECTIVE && !isAtEnd()) {
      switch (token.nicheType) {

        case Token::NicheType::DIRECTIVE_DEFINE: {
          size_t defineStartIndex = index;
          advance();
          std::string macroName = advance().value;
          ContinueAndLogIfAtEnd(EOF_BEFORE_COMPLETE_ERR);
          if (peek().type == Token::TokenTypes::OPENPAREN) {
            advance();
            // is a function macro
            auto macro = std::make_unique<FunctionMacro>();
            bool isEnd = false;
            size_t argCount = 0;
            while (!isAtEnd() && !isEnd) {
              //get args
              if (peek().type == Token::TokenTypes::CLOSEPAREN) {advance(); break;}
              macro->args[advance().value] = argCount;
              argCount++;
              switch (peek().type) {
                case Token::TokenTypes::COMMA:
                continue;
                break;
                case Token::TokenTypes::CLOSEPAREN:
                advance();
                isEnd = true;
                break;
                default:
                isEnd = true;
                logError("Unexpected token type (comma or close paren expected), got " + std::string(Token::toString(peek().type)) + " at " + peek().positionToString());
                break;
              } 
            }
            //
            if (peek().type == Token::TokenTypes::OPENBLOCK) {
              advance();
              while (peek().type != Token::TokenTypes::CLOSEBLOCK && !isAtEnd()) {
                macro->replacement.push_back(advance());
              }
              advance();
            } else {
              logError("Function block missing, expected curly brace, got " + std::string(Token::toString(peek().type)) + " at " + peek().positionToString() + " value: \"" + peek().value + "\"");
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

        case Token::NicheType::DIRECTIVE_INCLUDE: {
          logError("INCLUDE DIRECTIVE NOT IMPLEMENTED FOR PREPROCESSOR");
          advance();
        }
        break;
        case Token::NicheType::DIRECTIVE_ENTRY: {
          logError("ENTRY DIRECTIVE NOT IMPLEMENTED FOR PREPROCESSOR");
          advance();
        }
        break;
        default:
          logError("Unrecognise directive \"" + token.value + "\" at " + token.positionToString());
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