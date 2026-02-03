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
  virtual size_t replace(size_t fillIndex, std::vector<Token::Token>& replacementStream);
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
    while (argCount < args.size() && currentIndex < replacementStream.size()) {
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
    return !index<spasmTokens.size();
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
  if (!isAtEnd()) {logError(errorMessage); continue;}
  #define EOF_BEFORE_COMPLETE_ERR "Macro \"" + macroName + "\" hit EOF before definition finished."


  while (!isAtEnd()) {
    Token::Token token = peek();
    advance();
    auto macroIt = macroMap.find(token.value);
    if (macroIt != macroMap.end()) 
      {index = macroIt->second->replace(index, spasmTokens); continue;}
    if (token.type == Token::TokenTypes::DIRECTIVE && token.value == "@define" && !isAtEnd()) {
      size_t defineStartIndex = index;
      std::string macroName = advance().value;
      ContinueAndLogIfAtEnd(EOF_BEFORE_COMPLETE_ERR);
      if (peek().type == Token::TokenTypes::OPENPAREN) {
        // is a function macro
        std::unique_ptr<FunctionMacro> macro = std::make_unique<FunctionMacro>();
        bool isEnd = false;
        size_t argCount = 0;
        while (!isAtEnd() && !isEnd) {
          //get args
          macro->args[advance().value] = argCount;
          argCount++;
          switch (peek().type) {
            case Token::TokenTypes::COMMA:
            continue;
            break;
            case Token::TokenTypes::CLOSEPAREN:
            isEnd = true;
            break;
            default:
            isEnd = true;
            logError("Unexpected token type (comma or close paren expected), got " + peek().positionToString());
            break;
          } 
        }
        macroMap[macroName] = std::move(macro);
      } else {
        // is a replacement macro
        ContinueAndLogIfAtEnd(EOF_BEFORE_COMPLETE_ERR);
        std::unique_ptr<ReplacementMacro> macro; 
        macro->replacementToken = advance();
        macroMap[macroName] = std::move(macro);
      }

      spasmTokens.erase(spasmTokens.begin() + defineStartIndex, spasmTokens.begin() + index);
      index = defineStartIndex;
    }
  }



  return true;
}