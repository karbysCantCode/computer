#include <vector>
#include <unordered_map>
//#include "arch.hpp"
#include "token.hpp"

struct LineColPair {
  int line = -1;
  int col = -1;

  LineColPair(int ln, int cl) : line(ln), col(cl) {}
};

struct macro {
  std::string target;
};

struct functionMacro : macro {
  std::unordered_map<std::string, size_t> args;
  std::vector<Token::Token> replacement;

  size_t fillWithArgs(size_t fillIndex, std::vector<Token::Token>& replacementStream) {
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

    return currentIndex - fillIndex;
  }
};

struct replacementMacro : macro {
  std::string replacement;
};  

bool preprocessSpasm(std::vector<Token::Token>& spasmTokens) {
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

  std::unordered_map<std::string, macro*> macroMap;

  while (!isAtEnd()) {
    Token::Token& token = peek();
    advance();
    if (!(token.type == Token::TokenTypes::DIRECTIVE && token.value == "@define")) {continue;}

    
  }



  return true;
}