#pragma once

#include "Spasm/Spasm.hpp"
#include "Spasm/Lexer.hpp"
#include "Helpers/Debug.hpp"
#include "Arch/Arch.hpp"

namespace Spasm {



class Parser {
  public:

  Program ParseTokens(TokenHolder&, Arch::Architecture& arch, Debug::FullLogger* logger = nullptr);
  private:
  Debug::FullLogger* p_logger;

  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}

  void parseIdentifier(TokenHolder&, Arch::Architecture& arch, Program& program);
  void parseInstruction(TokenHolder&, const Token&, const Arch::Architecture&, const Arch::Architecture::InstructionDefinition&, Program& program);
  void parseLabel(TokenHolder&);

  std::pair<const Arch::Architecture::RegisterDefinition*, Token> parseExpectRegister(TokenHolder&, const Arch::Architecture&);
  std::unique_ptr<Program::Operand> parseExpectImmediate(TokenHolder&);
  std::unique_ptr<Program::Operand> convertConstantStringToOperand(const Arch::Architecture&, const Arch::Architecture::ConstantStringOperand&);

  std::unique_ptr<Program::Operand> makeErrorOperand(const Token&, const std::string&);
  std::unique_ptr<Program::Expr> makeErrorExpression(const Token&, const std::string&);
  
  int parseNumberString(const Token&);
  std::unique_ptr<Program::Expr> parseSquareExpression(TokenHolder&);
  std::unique_ptr<Program::Expr> parseOr(TokenHolder&);
  std::unique_ptr<Program::Expr> parseXor(TokenHolder&);
  std::unique_ptr<Program::Expr> parseAnd(TokenHolder&);
  std::unique_ptr<Program::Expr> parseShift(TokenHolder&);
  std::unique_ptr<Program::Expr> parseAdditive(TokenHolder&);
  std::unique_ptr<Program::Expr> parseMultiplicative(TokenHolder&);
  std::unique_ptr<Program::Expr> parseUnary(TokenHolder&);
  std::unique_ptr<Program::Expr> parsePrimary(TokenHolder&);

  void parseLabelDefinition(TokenHolder&, Program&);
  bool getOrCreateLabel(TokenHolder&, Program&, Program::LabelObject*&, bool, Program::LabelSymbol*); //returns success
  std::unique_ptr<Program::Expr> parseIdentifierToExpression(TokenHolder&);
  void parseDataTypeDeclaration(TokenHolder&, Program&);
  void parseNonArrayDataType(TokenHolder&, Program&, size_t);
  void parseArrayDataType(TokenHolder&, Program&, bool);

  void parseElementsOfArray(TokenHolder&, Program&, Program::DataObject*);
  void parseTextData(TokenHolder&, Program&, Program::DataObject*);
};
}