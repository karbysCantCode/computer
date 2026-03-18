#pragma once

#include "Helpers/Debug.hpp"
#include "Spasm/Spasm.hpp"
#include "Spasm/Lexer.hpp"
#include "SMake/SMake.hpp"

namespace Spasm {
class Preprocessor {
  public:

  TokenHolder run(TokenHolder&, SMake::Target&);
  private:
  Debug::FullLogger* p_logger;

  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}

  void processDefine(TokenHolder&, TokenHolder&);
  void processInclude(TokenHolder&, SMake::Target&);
  void processEntry(TokenHolder&, SMake::Target&);
};
}