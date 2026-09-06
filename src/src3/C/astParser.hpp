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
void setLogger(Debug::FullLogger* ptr) {p_logger = ptr;}
private:

Debug::FullLogger* p_logger;
inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}

std::unique_ptr<ASTDeclarationStatement> parseDeclaration(TokenHolder& holder, ASTScope& scope);
ASTPrimitiveType ParsePrimitiveType(TokenHolder& holder, ASTScope& scope);
bool ParsePrimitiveTypeInner(TokenHolder& holder, ASTScope& scope, ASTPrimitiveType& type);
std::unique_ptr<ASTDeclarator> parseDeclarator(TokenHolder& holder,  ASTScope& scope);
std::unique_ptr<ASTDeclarator> parsePointerDeclarator(TokenHolder& holder,  ASTScope& scope, std::unique_ptr<ASTDeclarator> inner);
std::unique_ptr<ASTDeclarator> parseDirectDeclarator(TokenHolder& holder,  ASTScope& scope);
std::unique_ptr<ASTDeclarator> parseSuffixDeclarator(TokenHolder& holder, ASTScope& scope, std::unique_ptr<ASTDeclarator> inner);

std::unique_ptr<ASTExpression> parseExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseAssignmentExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseConditionalExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseLogicalOrExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseLogicalAndExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseInclusiveOrExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseExclusiveOrExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseAndExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseEqualityExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseComparisonExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseShiftExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseAdditiveExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseMultiplicativeExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseCastExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parseUnaryExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parsePostfixExpression(TokenHolder& holder, ASTScope& scope, std::unique_ptr<ASTExpression> inner);
std::unique_ptr<ASTExpression> parsePrepostExpression(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTExpression> parsePrimaryExpression(TokenHolder& holder, ASTScope& scope);

std::unique_ptr<ASTNode> parseScopeUntilCloseBlock(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTNode> parseNode(TokenHolder& holder, ASTScope& scope);

std::unique_ptr<ASTNode> parseLabelStatement(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTNode> parseExpressionStatement(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTNode> parseCaseStatement(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTNode> parseSelectionStatement(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTNode> parseIterationStatement(TokenHolder& holder, ASTScope& scope);
std::unique_ptr<ASTNode> parseJumpStatement(TokenHolder& holder, ASTScope& scope);
};
} // namespace C
