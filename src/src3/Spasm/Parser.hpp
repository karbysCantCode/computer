#pragma once

#include "Spasm/Spasm.hpp"
#include "Spasm/Lexer.hpp"
#include "Helpers/Debug.hpp"
#include "Arch/Arch.hpp"

namespace Spasm {



class Parser {
  public:

  void ParseTokens(TokenHolder&, Arch::Architecture& arch, Program::TranslationUnit& translationUnit, Program& program, Debug::FullLogger* logger = nullptr);
  private:
  Debug::FullLogger* p_logger;

  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}
  inline void logError(const SourceLocation& sourceLocation, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(sourceLocation.toString() + message);}}
  inline void logWarning(const SourceLocation& sourceLocation, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(sourceLocation.toString() + message);}}
  inline void logDebug(const SourceLocation& sourceLocation, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(sourceLocation.toString() + message);}}

  void parseIdentifier(TokenHolder&, Arch::Architecture& arch, Program::TranslationUnit& translationUnit, Program& program);
  std::unique_ptr<Program::InstructionSymbol> parseInstruction(TokenHolder&, const Token&, const Arch::Architecture&, Program::TranslationUnit& translationUnit, Program& program, size_t expressionAddressOffset = 0);
  void parseLabel(TokenHolder&);
  void parseRelaxor(TokenHolder&, Arch::Architecture& arch, Program::TranslationUnit& translationUnit, Program& program);

  std::pair<const Arch::Architecture::RegisterDefinition*, Token> parseExpectRegister(TokenHolder&, const Arch::Architecture&);
  std::unique_ptr<Program::Operand> parseExpectImmediate(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Operand> convertConstantStringToOperand(const Arch::Architecture&, const Arch::Architecture::ConstantStringOperand&);

  std::unique_ptr<Program::Expr> makeErrorExpression(const Token&, const std::string&, size_t*, size_t);
  std::unique_ptr<Program::Operand> makeErrorOperand(const Token&, const std::string&, size_t*, size_t);
  
  int parseNumberString(const Token&);
  std::unique_ptr<Program::Expr> parseSquareExpression(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseBooleanOr(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseBooleanAnd(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseOr(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseXor(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseAnd(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseEquality(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseComparison(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseShift(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseAdditive(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseMultiplicative(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parseUnary(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::Expr> parsePrimary(TokenHolder&, size_t*, size_t);

  void parseLabelDefinition(TokenHolder&, Program::TranslationUnit&, Program&);
  //bool getOrCreateIdentiferObject(TokenHolder&, Program::TranslationUnit&, Program&, Program::LabelObject*&, bool, Program::LabelSymbol*, std::string_view&); //returns success
  bool getOrCreatePartialIdentifier(
    TokenHolder&, 
    Program::TranslationUnit&, 
    Program::LabelObject*&, 
    bool, 
    Program::StatementSymbol*, 
    bool,
    std::vector<std::string_view>&
  );
  std::unique_ptr<Program::Expr> parseIdentifierToExpression(TokenHolder&, size_t*, size_t);
  std::unique_ptr<Program::StatementSymbol> parseIdentifierNameDefiniton(TokenHolder&, Program::TranslationUnit&, Program&, bool);

  void parseDataTypeDeclaration(TokenHolder&, Program::TranslationUnit&, Program&);
  void parseNonArrayDataType(TokenHolder&, Program::TranslationUnit&, Program&, size_t);
  void parseArrayDataType(TokenHolder&, Program::TranslationUnit&, Program&, bool);
  void parseElementsOfArray(TokenHolder&, Program::TranslationUnit&, Program::DataObject*, bool);
  void parseTextData(TokenHolder&, Program::DataObject*, bool);
  bool isRelaxorConditional(Token::NicheType type);
  void parseRelaxorCondition(TokenHolder&, Program::RelaxorDefinition::RelaxorOptionPair&, size_t*, Program::TranslationUnit&);
  void parseRelaxorCodeBlock(TokenHolder&,  Program::RelaxorSymbol&, Program::RelaxorDefinition::RelaxorOptionPair&, Arch::Architecture&, Program::TranslationUnit&, Program&);
};
}