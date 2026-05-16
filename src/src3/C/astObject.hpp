#pragma once

#include <vector>
#include <memory>
#include <cassert>

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

class DeclarationStatement : AbstractStatement {
public:
  Kind getKind() const override {return Kind::DECLARATION;}
};

class AbstractDeclaration {
public:
  std::string name;

  virtual ~AbstractDeclaration() = default;
};

class FunctionDeclaration : AbstractDeclaration {

};
class TypeDeclaration : AbstractDeclaration {

};
class StructDeclaration : AbstractDeclaration {

};
class VariableDeclaration : AbstractDeclaration {

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