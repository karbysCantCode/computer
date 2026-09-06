#pragma once

#include <vector>
#include <memory>
#include <cassert>
#include <unordered_map>
#include <variant>
#include "lexer.hpp"

namespace C {

struct ASTNode {
  virtual ~ASTNode() = default;
};

enum ASTPrimitiveKind {
  VOID,
  CHAR,
  INT,
  FLOAT,
  DOUBLE,
  STRUCT,
  UNION,
  ENUM,
  UNDEFINED
};

struct ASTPrimitiveModifiers {
  char mods = 0;
  void setSigned() {mods |= 0b00000001;}
  void setUnsigned() {mods |= 0b00000010;}
  void setShort() {mods |= 0b00000100;}
  void setLong() {mods |= 0b00001000;}
  void setLongLong() {mods |= 0b00010000;}

  bool isSigned() const {return mods & 0b00000001;}
  bool isUnsigned() const {return mods & 0b00000010;}
  bool isShort() const {return mods & 0b00000100;}
  bool isLong() const {return mods & 0b00001000;}
  bool isLongLong() const {return mods & 0b00010000;}
};


struct ASTStorageClassSpecifiers {
  char scss = 0;
  void setTypeDef() {scss |= 0b00000001;}
  void setExtern() {scss |= 0b00000010;}
  void setStatic() {scss |= 0b00000100;}
  void setAuto() {scss |= 0b00001000;}
  void setRegister() {scss |= 0b00010000;}
 
  bool isTypeDef() const {return scss & 0b00000001;}
  bool isExtern() const {return scss & 0b00000010;}
  bool isStatic() const {return scss & 0b00000100;}
  bool isAuto() const {return scss & 0b00001000;}
  bool isRegister() const {return scss & 0b00010000;}

  bool isNone() const {return scss == 0;}
};

struct ASTTypeQualifiers {
  char tqs = 0;
  void setConst() {tqs |= 0b00000001;}
  void setRestrict() {tqs |= 0b00000010;}
  void setVolatile() {tqs |= 0b00000100;}
 
  bool isConst() const {return tqs & 0b00000001;}
  bool isRestrict() const {return tqs & 0b00000010;}
  bool isVolatile() const {return tqs & 0b00000100;}
};

struct ASTFunctionSpecifiers {
  char fss = 0;
  void setInline() {fss |= 0b00000001;}
  bool isInline() {return fss & 0b00000001;}
};

struct ASTPrimitiveType {
  ASTPrimitiveKind kind = ASTPrimitiveKind::UNDEFINED;
  Token* userKind = nullptr;
  ASTPrimitiveModifiers mod;
  ASTStorageClassSpecifiers scs;
  ASTTypeQualifiers tq;
  ASTFunctionSpecifiers fs;
};

struct ASTExpression : ASTNode {
  virtual ~ASTExpression() = default;
};

struct ASTInitializer {

};

struct ASTDeclarator {
  enum Kind {
    ARRAY,
    FUNCTION,
    POINTER,
    OBJECT
  };
  virtual ~ASTDeclarator() = default;
  virtual const Token* getIdentifierToken() const = 0;
  virtual constexpr Kind getKind() const = 0;
};

struct ASTDeclarationStatement : ASTNode {
  std::vector<std::unique_ptr<ASTDeclarator>> declarators;
  ASTPrimitiveType primitiveType;
  std::unique_ptr<ASTNode> initialisingNode;
  ASTDeclarationStatement() {}
  ASTDeclarationStatement(std::vector<std::unique_ptr<ASTDeclarator>>& decl,ASTPrimitiveType& primtype) : declarators(std::move(decl)), primitiveType(std::move(primtype)) {}
};

struct ASTStrucctOrUnionMember : ASTNode {
  std::unique_ptr<ASTDeclarationStatement> declarator;
  std::unique_ptr<ASTExpression> bitwidth;
};
struct ASTEnumEnumerator : ASTExpression {
  Token* enumerator;
  std::unique_ptr<ASTExpression> expression;
};

struct ASTStructMembers : ASTNode  {
  std::vector<std::unique_ptr<ASTStrucctOrUnionMember>> members;
};

struct ASTUnionMembers : ASTNode  {
  std::vector<std::unique_ptr<ASTStrucctOrUnionMember>> members;
};


struct ASTEnumMembers : ASTNode {
  std::vector<std::unique_ptr<ASTEnumEnumerator>> enumerators;
};

struct ASTType {
  ASTPrimitiveType type;
  std::unique_ptr<ASTDeclarator> declarator;
};

struct ASTBinaryExpression : ASTExpression {
  Token* opTypeToken;
  std::unique_ptr<ASTExpression> left;
  std::unique_ptr<ASTExpression> right;
};

struct ASTUnaryExpression : ASTExpression {
  Token* opTypeToken;
  std::unique_ptr<ASTExpression> left;
};

struct ASTCastExpression : ASTExpression {
  ASTType type;
  std::unique_ptr<ASTExpression> right;
};

struct ASTAssignmentExpression : ASTExpression {
  Token* opTypeToken;
  std::unique_ptr<ASTExpression> left;
  std::unique_ptr<ASTExpression> right;
};

struct ASTConditionalExpression : ASTExpression {
  std::unique_ptr<ASTExpression> eval;
  std::unique_ptr<ASTExpression> left;
  std::unique_ptr<ASTExpression> right;
};


struct ASTConstantExpression : ASTExpression {
  Token* constant;
};
struct ASTIdentifierExpression : ASTExpression {
  Token* identifier;
};

struct ASTStringExpression : ASTExpression {
  Token* string;
};

// describes that inside the []...: myStruct[.member]
// . or ->
struct ASTMemberAccessExpression : ASTExpression {
  //check before access
  Token* memberName;
  Token* opTypeToken;
  std::unique_ptr<ASTExpression> accessee;
};

struct ASTArrayAccessExpression : ASTExpression {
  std::unique_ptr<ASTExpression> array;
  std::unique_ptr<ASTExpression> index;
};

struct ASTCallExpression : ASTExpression {
  std::unique_ptr<ASTExpression> callee;
  std::vector<std::unique_ptr<ASTExpression>> arguments;
};

struct ASTPostIncDecExpression : ASTExpression {
  Token* opTypeToken;
  std::unique_ptr<ASTExpression> expr;
};


struct ASTListInitializer : ASTInitializer {
  std::vector<std::unique_ptr<ASTInitializer>> initializers;
};

struct ASTSizeOfExpression : ASTExpression {
  ASTType type;
};
struct ASTCompoundLiteralExpression : ASTExpression {
  ASTType type;
  ASTListInitializer list;
};

struct ASTExpressionInitializer : ASTInitializer {
  std::unique_ptr<ASTExpression> expression;
  ASTExpressionInitializer(std::unique_ptr<ASTExpression> expr) : expression(std::move(expr)) {}
};




struct ASTArrayDeclarator : ASTDeclarator {
  std::unique_ptr<ASTDeclarator> declarator;
  std::unique_ptr<ASTExpression> lengthExpression;
  ASTArrayDeclarator(std::unique_ptr<ASTDeclarator> decl) : declarator(std::move(decl)) {}
  virtual constexpr Kind getKind() const {return Kind::ARRAY;}
  virtual const Token* getIdentifierToken() const {return declarator ? declarator->getIdentifierToken() : nullptr;}
};
struct ASTFunctionDeclarator : ASTDeclarator {
  std::unique_ptr<ASTDeclarator> declarator;
  std::vector<std::unique_ptr<ASTType>> arguments;
  ASTFunctionDeclarator(std::unique_ptr<ASTDeclarator> decl) : declarator(std::move(decl)) {}
  virtual constexpr Kind getKind() const {return Kind::FUNCTION;}
  virtual const Token* getIdentifierToken() const {return declarator ? declarator->getIdentifierToken() : nullptr;}
};
struct ASTPointerDeclarator : ASTDeclarator {
  std::unique_ptr<ASTDeclarator> declarator;
  ASTPointerDeclarator(std::unique_ptr<ASTDeclarator> decl) : declarator(std::move(decl)) {}
  virtual constexpr Kind getKind() const {return Kind::POINTER;}
  virtual const Token* getIdentifierToken() const {return declarator ? declarator->getIdentifierToken() : nullptr;}
};
struct ASTIdentifierDeclarator : ASTDeclarator {
  const Token* identifierToken;
  ASTIdentifierDeclarator(const Token* token) : identifierToken(token) {}
  virtual constexpr Kind getKind() const {return Kind::OBJECT;}
  virtual const Token* getIdentifierToken() const {return identifierToken;}
};
struct ASTLabelStatement : ASTNode {
  Token* identifier;
};

struct ASTCaseStatement : ASTNode {
  std::unique_ptr<ASTExpression> expression;
  bool isDefault = false;
};

struct ASTSelectionStatement : ASTNode {
  std::unique_ptr<ASTNode> statement;
  std::unique_ptr<ASTExpression> expression;

  enum Kind {
    IF,
    IFELSE,
    SWITCH,
    NONE
  };

  Kind k = Kind::NONE;
};
struct ASTIterationStatement : ASTNode {

};
struct ASTJumpStatement : ASTNode {

};

struct ASTScope : ASTNode {
  std::vector<std::unique_ptr<ASTNode>> statements;
  std::unordered_map<std::string, ASTDeclarationStatement*> symbolMap;
  ASTScope* parent = nullptr;

  void addSymbol(const Token* token, ASTDeclarationStatement* statement) {
    if (token == nullptr) return;
    symbolMap.emplace(token->getString(), statement);
  }

  bool isTypename(const std::string& str) {
    ASTScope* scope = this;
    while (scope != nullptr) {
      if (searchScope(scope, str)) return true;
      scope = scope->parent;
    }
    return false;
  }
  private:
  bool searchScope(const ASTScope* scope, const std::string& str) {
    return scope->symbolMap.end() != scope->symbolMap.find(str);
  }
};





/*
declares:
  struct
  unions
  functions
  variables
  enum
  
blocks
expressions
labels

assignment
function calls
control flow (for,while,return,if)

*/

struct ASTObject {
  ASTScope globalScope;

  ASTObject() {
    //global.typenameMap.emplace("int", );
  }
};

}