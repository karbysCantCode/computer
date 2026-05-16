#pragma once

#include "Helpers/Debug.hpp"
#include "lexer.hpp"
#include "translationUnit.hpp"

#include "astObject.hpp"

namespace C
{

class ASTParser {
public:

ASTObject run(TokenHolder& holder, TranslationUnit& TranslationUnit); 

struct AbstractExpression {

};
private:

Debug::FullLogger* p_logger;
inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}

struct DeclarationSpecifierHolder {
  Token::Type alignmentSpecifier = Token::Type::INVALID;
  Token::Type functionSpecifier = Token::Type::INVALID;
  Token::Type typeSpecifier = Token::Type::INVALID;
  Token::Type storageSpecifier = Token::Type::INVALID;
  bool _signed = false;
  bool _unsigned = false;
  bool _short = false;
  bool _longA = false;
  bool _longB = false;
  bool _volatile = false;
  bool _const = false;
  bool _union = false;
  bool _struct = false;
  AbstractDeclaration* decl = nullptr;

  DeclarationSpecifierHolder() {}
  DeclarationSpecifierHolder(AbstractDeclaration* declaration) : decl(declaration) {}
};

struct AbstractDeclarator {

  virtual ~AbstractDeclarator() = default;
};

struct PointerDeclarator : AbstractDeclarator {
  std::unique_ptr<AbstractDeclarator> declarator;
  PointerDeclarator(std::unique_ptr<AbstractDeclarator> decl) : declarator(std::move(decl)) {}
};
struct ArrayDeclarator : AbstractDeclarator {
  std::unique_ptr<AbstractDeclarator> declarator;
  AbstractExpression* expression;
  size_t argsBegin = 0;
  size_t argLength = 0;
  ArrayDeclarator(std::unique_ptr<AbstractDeclarator> decl, AbstractExpression* expr,size_t argbegin, size_t arglength) : argsBegin(argbegin), argLength(arglength), declarator(std::move(decl)), expression(expr) {}
};
struct ParenthesesDeclarator : AbstractDeclarator {
  std::unique_ptr<AbstractDeclarator> declarator;
  ParenthesesDeclarator(std::unique_ptr<AbstractDeclarator> decl) : declarator(std::move(decl)) {}
};
struct FunctionDeclarator : AbstractDeclarator {
  std::unique_ptr<AbstractDeclarator> declarator;
  size_t argsBegin = 0;
  size_t argLength = 0;
  FunctionDeclarator(std::unique_ptr<AbstractDeclarator> decl, size_t argbegin, size_t arglength) : argsBegin(argbegin), argLength(arglength), declarator(std::move(decl)) {}
};
struct IdentifierDeclarator : AbstractDeclarator {
  const Token* identifierToken;
  IdentifierDeclarator(const Token* token) : identifierToken(token) {}
};

TokenHolder collectDeclaratorsInPrecedence(TokenHolder& holder);

std::unique_ptr<AbstractDeclarator> parseDeclarator(TokenHolder& holder);
std::unique_ptr<AbstractDeclarator> parsePointerDeclarator(TokenHolder& holder, std::unique_ptr<ASTParser::AbstractDeclarator> inner);
std::unique_ptr<AbstractDeclarator> parseDirectDeclarator(TokenHolder& holder);
std::unique_ptr<AbstractDeclarator> parseSuffixDeclarator(TokenHolder& holder);

bool isType(CompoundStatement& statement, Token& token) const;

void parseDeclaration(const Token& firstToken, TokenHolder& holder, CompoundStatement& scope, AbstractDeclaration* declaration = nullptr);
std::pair<bool,bool> parseDeclarationSpecifier(const Token& token, CompoundStatement& scope, DeclarationSpecifierHolder& specs);
std::vector<std::unique_ptr<AbstractDeclarator>> parseDeclarators(TokenHolder& holder);
};
  
} // namespace C
