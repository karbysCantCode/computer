#pragma once

#include <vector>
#include <memory>
#include <cassert>
#include <unordered_map>

namespace C {

class Symbol {

};

struct AbstractType {
  std::string_view name;


};

struct PrimitiveType : AbstractType{
  enum class Type {
    BOOL,
    CHAR,
    UCHAR,
    SCHAR,
    SHORT,
    USHORT,
    INT,
    UINT,
    LINT,
    ULINT,
    LLINT,
    ULLINT,
    FLOAT,
    DOUBLE,
    LDOUBLE
  };
};

struct UserType : AbstractType{
  //std::unordered_map<std::string_view, AbstractType*> members;
};

struct AliasType : AbstractType {
  AbstractType* coreType;

};

/*
types:
primitive
structs
typedef

declares:

*/

class AbstractStatement {
public:
  AbstractStatement* parent = nullptr;

  enum class Kind {
    DECLARATION,
    EXPRESSION,
    COMPOUND,
    SELECTION,
    ITERATION,
    JUMP,
    LABEL
  };

  virtual Kind getKind() const = 0;
  virtual ~AbstractStatement() = default;
};

class AbstractIdentifier {
public:
  std::string name;
  virtual ~AbstractIdentifier() = default;
};

class VariableIdentifier : AbstractIdentifier {
public:
  AbstractType* type;

};


class DeclarationStatement : AbstractStatement {
  public:
  Kind getKind() const override {return Kind::DECLARATION;}
};

class AbstractDeclaration {
  public:
  std::string name;
  
  virtual ~AbstractDeclaration() = default;
};

class ExpressionStatement : AbstractStatement {
public:
  Kind getKind() const override {return Kind::EXPRESSION;}
};

class CompoundStatement : AbstractStatement {
public:
  CompoundStatement* parent;
  std::vector<std::unique_ptr<AbstractStatement>> statements;
  std::unordered_map<std::string_view, AbstractDeclaration*> typenameMap;


  inline AbstractDeclaration* findDeclarationName(const std::string_view& str) const {
    const auto it = typenameMap.find(str);
    if (it == typenameMap.end()) {
      return nullptr;
    }
    return it->second;
  }
  Kind getKind() const override {return Kind::COMPOUND;}
};
class SelectionStatement : AbstractStatement {
public:
  Kind getKind() const override {return Kind::SELECTION;}
};
class IterationStatement : AbstractStatement {
public:
  Kind getKind() const override {return Kind::ITERATION;}
};
class JumpStatement : AbstractStatement {
public:
  Kind getKind() const override {return Kind::JUMP;}
};
class LabelStatement : AbstractStatement {
public:
  Kind getKind() const override {return Kind::LABEL;}
};

class FunctionDeclaration : AbstractDeclaration {
public:
  std::unique_ptr<CompoundStatement> body;
};
class FunctionIdentifier : AbstractIdentifier {
public:
  FunctionDeclaration* declaration;
};
class TypeDeclaration : AbstractDeclaration {
public:

};
class StructDeclaration : AbstractDeclaration {
public:

};
class VariableDeclaration : AbstractDeclaration {
public:

};

class AbstractExpression {
public:
  virtual ~AbstractExpression() = default;
};

class BinaryExpression : AbstractExpression {
public:

};

class UnaryExpression : AbstractExpression {
public:

};

class AssignmentExpression : AbstractExpression {
public:

};

class AccessExpression : AbstractExpression {
public:

};

class FunctionExpression : AbstractExpression {
public:

};

class PrimaryExpression : AbstractExpression {
public:

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
  CompoundStatement global;

  ASTObject() {
    //global.typenameMap.emplace("int", );
  }
};

}