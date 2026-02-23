#include <vector>

#include "token.hpp"
#include "arch.hpp"

struct parsedSpasm {
  //std::vector<>
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  std::vector<std::string> debugs;

  void logError(std::string str) {errors.push_back(str);}
  void logWarning(std::string str) {warnings.push_back(str);}
  void logDebug(std::string str) {debugs.push_back(str);}

  bool isErrors() {return !errors.empty();}
  bool isWarnings() {return !warnings.empty();}
  bool isDebugs() {return !debugs.empty();}

  std::string consumeError() {const auto str = errors.back(); errors.pop_back(); return str;}
  std::string consumeWarnings() {const auto str = warnings.back(); warnings.pop_back(); return str;}
  std::string consumeDebugs() {const auto str = debugs.back(); debugs.pop_back(); return str;}
};


parsedSpasm parseSpasm(std::vector<Token::Token>& spasmTokens, Architecture::Architecture& architecture) {
  parsedSpasm parsed;
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



  return parsed;
}