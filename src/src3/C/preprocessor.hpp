#pragma once

#include "lexer.hpp"
#include "Helpers/Debug.hpp"
#include "translationUnit.hpp"

namespace C {

class Preprocessor {
public:
  TokenHolder run(TranslationUnit& translationUnit, TokenHolder& holder); 

private:
  Debug::FullLogger* p_logger = nullptr;
  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}

  void invokeMacro(Macro&, TranslationUnit& translationUnit, TokenHolder& input, TokenHolder& output);
  std::unordered_map<std::string_view, std::vector<Token*>> parseMacroArguments(Macro& macro, TokenHolder& toParse);
  TokenHolder substituteMacroContents(std::unordered_map<std::string_view, std::vector<Token*>>& args, Macro& macro);
};

}