#pragma once

#include "Spasm/Spasm.hpp"
#include "Helpers/Debug.hpp"
#include "Spasm/Linker.hpp"
#include "Spasm/Lexer.hpp"

namespace Spasm {

class OutputGenerator {
public:
  void run(
    SMake::Target&,
    Linker&,
    Linker::LinkedResult&,
    size_t,
    Debug::FullLogger*
  );

private:
  Debug::FullLogger* p_logger;
  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}
  inline void logError(const SourceLocation& sLoc, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(sLoc.toString() + message);}}
  inline void logWarning(const SourceLocation& sLoc, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(sLoc.toString() + message);}}
  inline void logDebug(const SourceLocation& sLoc, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(sLoc.toString() + message);}}

  void fillBytesFromDataDeclarations(const Linker::LinkedResult& linkedResult, const Program::TranslationUnit& translationUnit, std::vector<uint8_t>& binaryData);
  void fillInstruction(Linker::LinkedResult& linkedResult, Program::InstructionSymbol& instructionStatement, std::vector<uint8_t>& binaryData, int addressOverride = -1);
};

}