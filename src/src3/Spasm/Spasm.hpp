#pragma once

#include <filesystem>
#include <unordered_set>
#include <cassert>
#include <Arch/Arch.hpp>
#include <Spasm/Lexer.hpp>
#include "Helpers/CLIOptions.hpp"

#include "SMake/SMake.hpp"
// each target needs to be one program
// each file should be compiled and then combined


namespace Spasm {
  class Program {
    public:


    struct StatementSymbol {
      const SourceLocation location;

      StatementSymbol(const SourceLocation loc) : location(loc) {}
      virtual ~StatementSymbol() = default;

      virtual void generate() = 0;
    };
    struct LabelSymbol : StatementSymbol {
      std::string_view name;

      void generate() override {}

      LabelSymbol(const SourceLocation loc, const std::string_view& lblName) : name(lblName), StatementSymbol(loc) {}
      LabelSymbol(const SourceLocation loc) : StatementSymbol(loc) {}

    };
    struct DefinitionSymbol : StatementSymbol {
      std::string_view name;

      void generate() override {}

      DefinitionSymbol(const SourceLocation& loc) : StatementSymbol(loc) {}
    };

    struct Expr {
      int value = 0;
      bool evaluated = false;

      void setValue(const int val) {value = val;}
      int  getValue() {return value;}
      bool isEvaluated() const {return evaluated;}
      void setEvaluated() {evaluated = true;}
      virtual ~Expr() = default;
    };

    struct IdentifierObject {
      std::string_view name;
      size_t address = 0;
      bool addressResolved = false;

      virtual bool isLabel() const {return false;}
      virtual bool isIdentifier() const {return false;}
      virtual ~IdentifierObject() = default;

      IdentifierObject() {}
      IdentifierObject(const std::string_view& nm) : name(nm) {}
    };
    struct LabelObject : IdentifierObject {
      LabelObject* parent;
      std::unordered_map<std::string_view, std::unique_ptr<LabelObject>> children;
      LabelSymbol* symbolObject;

      virtual bool isLabel() const override {return true;}
      virtual bool isIdentifier() const override {return false;}
      LabelObject() {}
      LabelObject(const std::string_view& nm) : IdentifierObject(nm) {}
      LabelObject(const std::string_view& nm, LabelObject* parnt) : IdentifierObject(nm), parent(parnt) {}
      LabelObject(const std::string_view& nm, LabelSymbol* symbol) : IdentifierObject(nm), symbolObject(symbol) {}
      LabelObject(const std::string_view& nm, LabelObject* parnt, LabelSymbol* symbol) : IdentifierObject(nm), parent(parnt), symbolObject(symbol) {}
    };
    struct DataObject : IdentifierObject {
      size_t elementCount; // in elements
      size_t elementSize; // in bytes
      std::vector<unsigned char> data;
      std::vector<std::unique_ptr<Expr>> exprData;
      DefinitionSymbol* symbolObject;

      virtual bool isLabel() const override {return false;}
      virtual bool isIdentifier() const override {return true;}

      DataObject(const std::string_view nm) : IdentifierObject(nm) {}
      DataObject(
        const std::string_view nm,
        DefinitionSymbol* symbolObj,
        const size_t elemSize,
        const size_t elemCount = 0)
        : IdentifierObject(nm),
          elementCount(elemCount),
          elementSize(elemSize),
          symbolObject(symbolObj) {}
    };

    std::filesystem::path m_sourcePath;
    std::unordered_set<std::filesystem::path> m_includedFilesByPath; // absolute path
    std::vector<std::unique_ptr<StatementSymbol>> m_statementVector;

    std::unordered_map<std::string_view, std::unique_ptr<IdentifierObject>> m_identifierMap;
    std::stack<Expr*> m_unresolvedExpressions;


    void parseTokens();



    struct NumberExpr : Expr {
      int value;
      NumberExpr(int val) : value(val) {}
    };

    struct IdentifierExpr : Expr {
      std::vector<std::string_view> identifierPath;
      IdentifierExpr(const std::string_view& iden) {
        identifierPath.push_back(iden);
      }
      IdentifierExpr() {}
    };

    struct UnaryExpr : Expr {
      Token::Type op;
      std::unique_ptr<Expr> right;

      UnaryExpr(Token::Type o,
                std::unique_ptr<Expr> rhs)
        : op(o), right(std::move(rhs)) {}
    };

    struct BinaryExpr : Expr {
      Token::Type op;
      std::unique_ptr<Expr> left;
      std::unique_ptr<Expr> right;

      BinaryExpr(Token::Type o,
                std::unique_ptr<Expr> lhs,
                std::unique_ptr<Expr> rhs)
        : op(o), left(std::move(lhs)),right(std::move(rhs)) {}
    };

    struct Operand {
      const SourceLocation location;

      Operand(const SourceLocation& loc) : location(loc) {}
      
      virtual ~Operand() = default;
    };

    struct RegisterOperand : Operand {
      const Arch::Architecture::RegisterDefinition& reg;

      RegisterOperand(const SourceLocation& loc, const Arch::Architecture::RegisterDefinition& r) : reg(r), Operand(loc) {}
    };

    struct ExpressionOperand : Operand {
      std::unique_ptr<Expr> expression;

      ExpressionOperand(const SourceLocation& loc) : Operand(loc) {}
      ExpressionOperand(const SourceLocation& loc, std::unique_ptr<Expr> expr) : expression(std::move(expr)), Operand(loc) {}
    };

    struct ConstantOperand : Operand {
      int value;

      ConstantOperand(const SourceLocation& loc, const int v) : value(v), Operand(loc) {}
    };

    struct InstructionSymbol : StatementSymbol {
      int opcode = -1;
      std::vector<std::unique_ptr<Operand>> operands;
      const Arch::Architecture::InstructionDefinition& instruction;
      void generate() override {}

      InstructionSymbol(const SourceLocation loc, const Arch::Architecture::InstructionDefinition& instr, std::vector<std::unique_ptr<Operand>>& initOperands) : operands(std::move(initOperands)), StatementSymbol(loc), instruction(instr) {}
      InstructionSymbol(const SourceLocation loc, const Arch::Architecture::InstructionDefinition& instr) : StatementSymbol(loc), instruction(instr) {}
      
    };
    private:
    
  };

  void spasmPipeline(SMake::Project&, Arch::Architecture&, CLIOptions&);
}