#pragma once

#include "Spasm/Spasm.hpp"
#include "Spasm/Lexer.hpp"
#include "Helpers/Debug.hpp"
#include "Arch/Arch.hpp"

namespace Spasm {



class Parser {
  public:

  Spasm::Program ParseTokens(TokenHolder&, Arch::Architecture& arch, Debug::FullLogger* logger = nullptr);
  private:
  Debug::FullLogger* p_logger;

  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}

  void parseIdentifier(TokenHolder&, Arch::Architecture& arch);
  void parseInstruction(TokenHolder&, Arch::Architecture::InstructionDefinition&);
  void parseLabel(TokenHolder&);
};

}