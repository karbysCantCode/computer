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
    struct IdentifierObject;
    struct DataObject;
    struct LabelObject;
    

    struct StatementSymbol {
      const SourceLocation location;
      std::string_view source;

      enum class Kind {
        LABEL,
        DEFINITION,
        INSTRUCTION
      };

      StatementSymbol(const SourceLocation loc) : location(loc) {}
      virtual ~StatementSymbol() = default;
      virtual Kind getKind() const {assert(false);}

      // virtual void generate() = 0;
    };
    struct LabelSymbol : StatementSymbol {
      std::string_view name;
      LabelObject* labelObject;

      // void generate() override {}
      Kind getKind() const override {return Kind::LABEL;}

      LabelSymbol(const SourceLocation loc, const std::string_view lblName, LabelObject* object) : name(lblName), StatementSymbol(loc), labelObject(object) {}
      LabelSymbol(const SourceLocation loc, LabelObject* object) : StatementSymbol(loc), labelObject(object) {}

    };
    struct DefinitionSymbol : StatementSymbol {
      std::string_view name;
      DataObject* dataObject;

      // void generate() override {}
      Kind getKind() const override {return Kind::DEFINITION;}
      DefinitionSymbol(const SourceLocation& loc) : StatementSymbol(loc) {}
      DefinitionSymbol(const SourceLocation& loc, std::string_view nm, DataObject* object) : StatementSymbol(loc), name(nm), dataObject(object) {}
    
    };

    using IdentifierMapType = std::unordered_map<std::string_view, std::shared_ptr<IdentifierObject>>;
    using NonOwningIdentifierMapType = std::unordered_map<std::string_view, IdentifierObject*>;
    using EvaluatePairType = std::pair<int, std::string>;
    struct Expr {
      int value = 0;
      bool evaluated = false;

      SourceLocation location;

      void setValue(const int val) {value = val;}
      int  getValue() {return value;}
      bool isEvaluated() const {return evaluated;}
      void setEvaluated() {evaluated = true;}
      virtual EvaluatePairType evaluate(NonOwningIdentifierMapType&) {assert(false); return {0, "ASSERTED FALSE"};}
      virtual ~Expr() = default;
      Expr(const SourceLocation& sLoc) : location(sLoc) {}
      Expr(const SourceLocation& sLoc, int val) : location(sLoc), value(val) {}
    };

    struct IdentifierObject {
      // handle when assembling parent child etc
      std::vector<std::string_view> nameSegments;
      std::string_view fullName() const;
      std::string_view name() const;
      std::string_view getNDepthName(size_t depth) const;
      std::vector<std::string_view> getNDepthNameVector(size_t depth) const;

      const std::filesystem::path& sourcePath;
      size_t address = 0;
      bool addressResolved = false;
      IdentifierObject* parent;
      IdentifierMapType children;
      
      virtual bool isLabel() const {return false;}
      virtual bool isIdentifier() const {return false;}
      virtual ~IdentifierObject() = default;
      
      IdentifierObject(const std::filesystem::path& srcPath, IdentifierObject* parnt) : sourcePath(srcPath), parent(parnt) {}
    };
    struct LabelObject : IdentifierObject {
      LabelSymbol* symbolObject;
      
      virtual bool isLabel() const override {return true;}
      virtual bool isIdentifier() const override {return false;}

      LabelObject(const std::filesystem::path& srcPath) : IdentifierObject(srcPath, nullptr) {}
      LabelObject(const std::filesystem::path& srcPath, LabelObject* parnt) : IdentifierObject(srcPath, parnt) {}
      LabelObject(const std::filesystem::path& srcPath, StatementSymbol* symbol) : IdentifierObject(srcPath, nullptr), symbolObject(dynamic_cast<LabelSymbol*>(symbol)) {}
      LabelObject(const std::filesystem::path& srcPath, LabelObject* parnt, StatementSymbol* symbol) : IdentifierObject(srcPath, parnt), symbolObject(dynamic_cast<LabelSymbol*>(symbol)) {}
    };
    struct DataObject : IdentifierObject {
      size_t elementCount; // in elements
      size_t elementSize; // in bytes
      std::vector<uint8_t> data;
      bool rawDataValid = false;
      std::vector<std::unique_ptr<Expr>> exprData;
      DefinitionSymbol* symbolObject;
      
      virtual bool isLabel() const override {return false;}
      virtual bool isIdentifier() const override {return true;}
      
      DataObject(const std::filesystem::path& srcPath, IdentifierObject* parnt) : IdentifierObject(srcPath, parnt) {}
      DataObject(
        const std::filesystem::path& srcPath,
        IdentifierObject* parnt,
        StatementSymbol* symbolObj,
        const size_t elemSize,
        const size_t elemCount = 0)
        : IdentifierObject(srcPath, parnt),
        elementCount(elemCount),
        elementSize(elemSize),
        symbolObject(dynamic_cast<DefinitionSymbol*>(symbolObj)) {}
    };
    
    class TranslationUnit {
      public:
      std::unique_ptr<std::string> m_source;
      std::filesystem::path m_sourcePath;
      std::vector<std::unique_ptr<StatementSymbol>> m_statementVector;
      std::vector<std::unique_ptr<DefinitionSymbol>> m_definitionVector;
      std::unordered_set<std::filesystem::path> m_includedFiles;
      IdentifierMapType m_identifierMap;
      size_t m_dependenciesResolved = 0;
      size_t m_dependenciesResolvedForDefinitions = 0;
      std::queue<Expr*> m_unresolvedExpressions;
      TokenHolder processedTokens;
    };
    
    
    std::stack<std::filesystem::path> m_filePathsToCreateTranslationUnitsOfAndPreprocess;
    std::stack<TranslationUnit*> m_translationUnitsToParseAndLink;
    std::unordered_map<std::filesystem::path, std::unique_ptr<TranslationUnit>> m_translationUnits;
    

    void debugPrint() const;



    struct NumberExpr : Expr {
      // int value;
      NumberExpr(const SourceLocation& sLoc, int val) : Expr(sLoc, val) {}

      virtual EvaluatePairType evaluate(NonOwningIdentifierMapType&) override {setEvaluated(); return {value, ""};}
    };

    struct IdentifierExpr : Expr {
      std::queue<std::string_view> identifierPath;
      IdentifierExpr(const SourceLocation& sLoc, const std::string_view iden) : Expr(sLoc) {
        identifierPath.push(iden);
      }
      IdentifierExpr(const SourceLocation& sLoc) : Expr(sLoc) {}

      virtual EvaluatePairType evaluate(NonOwningIdentifierMapType&) override;
    };

    struct UnaryExpr : Expr {
      Token::Type op;
      std::unique_ptr<Expr> right;

      UnaryExpr(const SourceLocation& sLoc,
        Token::Type o,
        std::unique_ptr<Expr> rhs)
        : Expr(sLoc), op(o), right(std::move(rhs)) {}

      virtual EvaluatePairType evaluate(NonOwningIdentifierMapType&) override;
    };

    struct BinaryExpr : Expr {
      Token::Type op;
      std::unique_ptr<Expr> left;
      std::unique_ptr<Expr> right;

      BinaryExpr(const SourceLocation& sLoc,
        Token::Type o,
        std::unique_ptr<Expr> lhs,
        std::unique_ptr<Expr> rhs)
        : Expr(sLoc), op(o), left(std::move(lhs)),right(std::move(rhs)) {}

      virtual EvaluatePairType evaluate(NonOwningIdentifierMapType&) override;
    };

    struct Operand {
      const SourceLocation location;

      enum class Kind {
        REGISTER,
        EXPRESSION,
        CONSTANT
      };

      Operand(const SourceLocation& loc) : location(loc) {}
      
      virtual int getValue() const = 0;
      virtual Kind getKind() const = 0;
      virtual ~Operand() = default;
    };

    struct RegisterOperand : Operand {
      const Arch::Architecture::RegisterDefinition& reg;
      virtual int getValue() const override {return reg.m_operandValue;}
      virtual Kind getKind() const override {return Kind::REGISTER;}
      RegisterOperand(const SourceLocation& loc, const Arch::Architecture::RegisterDefinition& r) : reg(r), Operand(loc) {}
    };

    struct ExpressionOperand : Operand {
      std::unique_ptr<Expr> expression;
      virtual int getValue() const override {return expression->value;}
      virtual Kind getKind() const override {return Kind::EXPRESSION;}
      ExpressionOperand(const SourceLocation& loc) : Operand(loc) {}
      ExpressionOperand(const SourceLocation& loc, std::unique_ptr<Expr> expr) : expression(std::move(expr)), Operand(loc) {}
    };

    struct ConstantOperand : Operand {
      int value;
      virtual int getValue() const override {return value;}
      virtual Kind getKind() const override {return Kind::CONSTANT;}
      ConstantOperand(const SourceLocation& loc, const int v) : value(v), Operand(loc) {}
    };

    struct InstructionSymbol : StatementSymbol {
      int opcode = -1;
      size_t address = 0;
      std::vector<std::unique_ptr<Operand>> operands;
      const Arch::Architecture::InstructionDefinition& instruction;
      // void generate() override {}
      Kind getKind() const override {return Kind::INSTRUCTION;}

      InstructionSymbol(const SourceLocation loc, const Arch::Architecture::InstructionDefinition& instr, std::vector<std::unique_ptr<Operand>>& initOperands) : operands(std::move(initOperands)), StatementSymbol(loc), instruction(instr) {}
      InstructionSymbol(const SourceLocation loc, const Arch::Architecture::InstructionDefinition& instr) : StatementSymbol(loc), instruction(instr) {}
      
    };
    private:

    void debugPrintStatement(const StatementSymbol* stmt, int indent) const;
    void debugPrintIdentifier(const IdentifierObject* obj, int indent) const;
    void debugPrintExpr(const Expr* expr, int indent) const;
    void debugPrintOperand(const Operand* op, int indent) const;

    static void indent(int count);
    
  };

  void spasmPipeline(SMake::Project&, Arch::Architecture&, CLIOptions&);
  
}