#pragma once

#include <filesystem>
#include <unordered_set>
#include <cassert>
#include <queue>
#include <Arch/Arch.hpp>
#include <Spasm/Lexer.hpp>
#include <sstream>
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
      size_t addressIndex = 0;
      size_t byteSize = 0;

      enum class Kind {
        LABEL,
        DEFINITION,
        INSTRUCTION,
        RELAXOR
      };

      StatementSymbol(const SourceLocation loc) : location(loc) {}
      virtual ~StatementSymbol() = default;
      virtual Kind getKind() const {assert(false); return Kind::LABEL;}

      // virtual void generate() = 0;
    };

    struct LabelSymbol : StatementSymbol {
      std::string_view name;
      std::unique_ptr<LabelObject> labelObject;

      /*
      label and definition symbols own their objects
      identifierobjects (label and data) are only 'real' (defined) if
        they actually have a statement symbol attatched to them.
      */

      // void generate() override {}
      Kind getKind() const override {return Kind::LABEL;}

      LabelSymbol(const SourceLocation loc, const std::string_view lblName, LabelObject* object) : name(lblName), StatementSymbol(loc), labelObject(object) {}
      LabelSymbol(const SourceLocation loc, LabelObject* object) : StatementSymbol(loc), labelObject(object) {}

    };
    struct DefinitionSymbol : StatementSymbol {
      std::string_view name;
      std::unique_ptr<DataObject> dataObject;

      // void generate() override {}
      Kind getKind() const override {return Kind::DEFINITION;}
      DefinitionSymbol(const SourceLocation& loc) : StatementSymbol(loc) {}
      DefinitionSymbol(const SourceLocation& loc, std::string_view nm, DataObject* object) : StatementSymbol(loc), name(nm), dataObject(object) {}
    
    };  

    using IdentifierMapType = std::unordered_map<std::string_view, IdentifierObject**>;
    using IdentifierMapStringType = std::unordered_map<std::string, IdentifierObject**>;
    //using NonOwningIdentifierMapType = std::unordered_map<std::string_view, IdentifierObject*>;
    
    struct EvaluateTriple {
      std::unordered_set<std::string> mentionedLabels;
      int value;
      std::string error;

      EvaluateTriple(int v, std::string err, std::unordered_set<std::string> mentionedLbls) : value(v), error(err), mentionedLabels(mentionedLbls) {}
      EvaluateTriple() {}
    };

    struct Expr {
      int value = 0;
      bool evaluated = false;
      size_t* addressIndexPtr = nullptr;
      size_t relativeAddressOffset = 0;
      std::unordered_set<std::string> mentionedLabels;
      IdentifierMapType* identifierMap = nullptr;

      SourceLocation location;

      void setValue(const int val) {value = val;}
      int  getValue() {return value;}
      bool isEvaluated() const {return evaluated;}
      void setEvaluated() {evaluated = true;}
      
      virtual EvaluateTriple evaluate(std::vector<size_t>&, bool getMentionedLabels = false) {assert(false); return {0, "ASSERTED FALSE", {}};}

      virtual ~Expr() = default;

      virtual void print(std::ostream& os) const = 0;

      std::string toString() const {
        std::ostringstream oss;
        print(oss);
        return oss.str();
      }

      Expr(const SourceLocation& sLoc, size_t* addressIndex, size_t expressionOffset, IdentifierMapType* idenMap) : identifierMap(idenMap), location(sLoc), addressIndexPtr(addressIndex), relativeAddressOffset(expressionOffset) {}
      Expr(const SourceLocation& sLoc, int val, size_t* addressIndex, size_t expressionOffset, IdentifierMapType* idenMap) : identifierMap(idenMap), location(sLoc), value(val), addressIndexPtr(addressIndex), relativeAddressOffset(expressionOffset) {}
    };

    struct IdentifierObject {
      // handle when assembling parent child etc
      std::vector<std::string_view> nameSegments;
      std::string fullName() const;
      std::string_view name() const;
      std::string getNDepthName(size_t depth) const;
      std::vector<std::string_view> getNDepthNameVector(size_t depth) const;

      const std::filesystem::path& sourcePath;
      size_t addressIndex = 0;
      IdentifierObject* parent = nullptr;
      IdentifierMapType children;
      
      virtual inline bool isLabel() const {return false;}
      virtual inline bool isIdentifier() const {return false;}
      virtual inline bool hasSymbolObject() const {return false;}
      virtual inline StatementSymbol* getAbstractSymbolPointer() const {return nullptr;}
      virtual ~IdentifierObject() = default;
      
      IdentifierObject(const std::filesystem::path& srcPath, IdentifierObject* parnt) : sourcePath(srcPath), parent(parnt) {}
      void assimilate(IdentifierObject& identifier);
    };
    struct LabelObject : IdentifierObject {
      LabelSymbol* symbolObject = nullptr;
      
      virtual inline bool isLabel() const override {return true;}
      virtual inline bool isIdentifier() const override {return false;}
      virtual inline bool hasSymbolObject() const override {return symbolObject != nullptr;}
      virtual inline StatementSymbol* getAbstractSymbolPointer() const override {return symbolObject;}

      LabelObject(const std::filesystem::path& srcPath) : IdentifierObject(srcPath, nullptr) {}
      LabelObject(const std::filesystem::path& srcPath, LabelObject* parnt) : IdentifierObject(srcPath, parnt) {}
      LabelObject(const std::filesystem::path& srcPath, StatementSymbol* symbol) : IdentifierObject(srcPath, nullptr), symbolObject(dynamic_cast<LabelSymbol*>(symbol)) {}
      LabelObject(const std::filesystem::path& srcPath, LabelObject* parnt, StatementSymbol* symbol) : IdentifierObject(srcPath, parnt), symbolObject(dynamic_cast<LabelSymbol*>(symbol)) {}
    };
    struct DataObject : IdentifierObject {
      size_t elementCount; // in elements
      std::unique_ptr<Expr> elementCountExpression;
      size_t elementSize; // in bytes
      std::unique_ptr<Expr> elementSizeExpression;
      std::vector<uint8_t> data;
      bool rawDataValid = false;
      std::vector<std::unique_ptr<Expr>> exprData;
      DefinitionSymbol* symbolObject = nullptr;
      
      virtual inline bool isLabel() const override {return false;}
      virtual inline bool isIdentifier() const override {return true;}
      virtual inline bool hasSymbolObject() const override {return symbolObject != nullptr;}
      virtual inline StatementSymbol* getAbstractSymbolPointer() const override {return symbolObject;}

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

    struct RelaxorDefinition {
      //OWNING options - condition
      struct RelaxorOptionPair {
        std::unique_ptr<Expr> conditionExpr;
        std::vector<std::unique_ptr<StatementSymbol>> optionStatements;
        size_t sumByteSizeOfOption() {if (cachedSum >= 0) return cachedSum; size_t v = 0; for (const auto& statement : optionStatements) {v += statement->byteSize;} cachedSum = v; return v;}
        void setCachedSize(size_t size) {cachedSum = size;}
        private:
        int cachedSum = -1;
      };

      std::vector<RelaxorOptionPair> options;
      size_t worstCaseSize = 0;

      std::unordered_set<std::string> getLabelsReferencedFromConditionsInAllOptions();
    };

    struct RelaxorSymbol : StatementSymbol {
      int optionIndex = -1;
      RelaxorDefinition relaxor;


      Kind getKind() const override {return Kind::RELAXOR;}

      RelaxorSymbol(const SourceLocation loc) : StatementSymbol(loc) {}
    };
    
    class TranslationUnit {
      public:
      std::unique_ptr<std::string> m_source;
      std::filesystem::path m_sourcePath;
      std::vector<std::unique_ptr<StatementSymbol>> m_statementVector;
      std::vector<std::unique_ptr<DefinitionSymbol>> m_definitionVector;
      std::unordered_set<std::filesystem::path> m_includedFiles;
      IdentifierMapType m_identifierMap;
      IdentifierMapStringType m_identifierFullNameMap;
      std::vector<std::unique_ptr<IdentifierObject*>>m_identifierObjectPtrHolder;
      std::vector<std::unique_ptr<IdentifierObject>> m_identifierObjectUndefinedHolder;
      std::vector<std::unique_ptr<std::string>> m_stringOwner;
      size_t m_dependenciesResolved = 0;
      size_t m_dependenciesResolvedForDefinitions = 0;
      std::queue<Expr*> m_unresolvedExpressions;
      TokenHolder processedTokens;
    };
    
    std::vector<RelaxorSymbol*> m_relaxorPointerVector;
    std::stack<std::filesystem::path> m_filePathsToCreateTranslationUnitsOfAndPreprocess;
    std::stack<TranslationUnit*> m_translationUnitsToParseAndLink;
    std::unordered_map<std::filesystem::path, std::unique_ptr<TranslationUnit>> m_translationUnits;
    

    void debugPrint() const;



    struct NumberExpr : Expr {
      // int value;
      NumberExpr(const SourceLocation& sLoc, int val, size_t* addressIndex, size_t expressionOffset, IdentifierMapType* idenMap) : Expr(sLoc, val, addressIndex, expressionOffset, idenMap) {}
      
      void print(std::ostream& os) const override {
        os << value;
      }
      virtual EvaluateTriple evaluate(std::vector<size_t>&, bool getMentionedLabels) override {setEvaluated(); return {value, "", {}};}
    };

    struct IdentifierExpr : Expr {
      std::queue<std::string_view> identifierPath;
      IdentifierExpr(const SourceLocation& sLoc, const std::string_view iden, size_t* addressIndex, size_t expressionOffset, IdentifierMapType* idenMap) : Expr(sLoc, addressIndex, expressionOffset, idenMap) {
        identifierPath.push(iden);
      }
      IdentifierExpr(const SourceLocation& sLoc, size_t* addressIndex, size_t expressionOffset, IdentifierMapType* idenMap) : Expr(sLoc, addressIndex, expressionOffset, idenMap) {}

      void print(std::ostream& os) const override {
        auto ipathcopy = identifierPath;
        while (!ipathcopy.empty()) {
          os << ipathcopy.front();
          ipathcopy.pop();
          if (!ipathcopy.empty()) {
            os << '.';
          }
        }
      }
      virtual EvaluateTriple evaluate(std::vector<size_t>&, bool getMentionedLabels) override;
    };

    struct UnaryExpr : Expr {
      Token::Type op;
      std::unique_ptr<Expr> right;

      UnaryExpr(const SourceLocation& sLoc,
        size_t* addressIndex,
        size_t expressionOffset,
        IdentifierMapType* idenMap,
        Token::Type o,
        std::unique_ptr<Expr> rhs)
        : Expr(sLoc, addressIndex, expressionOffset, idenMap), op(o), right(std::move(rhs)) {}
      
      static std::string tokenToString(Token::Type type) {
        switch (type) {
          case Token::Type::RELATIVEOPERATOR: return "$";
          case Token::Type::ABSOLUTE: return "$$";
          case Token::Type::SUBTRACT: return "-";
          case Token::Type::BITWISENOT: return "~";
          default: return "?";
        }
      }

      void print(std::ostream& os) const override {
        os << "(" << tokenToString(op);
        right->print(os);
        os << ")";
      }

      virtual EvaluateTriple evaluate(std::vector<size_t>&, bool getMentionedLabels) override;
    };

    struct BinaryExpr : Expr {
      Token::Type op;
      std::unique_ptr<Expr> left;
      std::unique_ptr<Expr> right;

      BinaryExpr(const SourceLocation& sLoc,
        size_t* addressIndex,
        size_t expressionOffset,
        IdentifierMapType* idenMap,
        Token::Type o,
        std::unique_ptr<Expr> lhs,
        std::unique_ptr<Expr> rhs)
        : Expr(sLoc, addressIndex, expressionOffset, idenMap), op(o), left(std::move(lhs)),right(std::move(rhs)) {}
      
      void print(std::ostream& os) const override {
        os << "(";
        left->print(os);
        os << " " << tokenToString(op) << " ";
        right->print(os);
        os << ")";
      }

      static std::string tokenToString(Token::Type type) {
        switch (type) {
          case Token::Type::BITWISEOR: return "|";
          case Token::Type::BITWISEAND: return "&";
          case Token::Type::BITWISEXOR: return "^";
          case Token::Type::LEFTSHIFT: return "<<";
          case Token::Type::RIGHTSHIFT: return ">>";
          case Token::Type::ADD: return "+";
          case Token::Type::SUBTRACT: return "-";
          case Token::Type::MULTIPLY: return "*";
          case Token::Type::DIVIDE: return "/";
          case Token::Type::MOD: return "%";
          case Token::Type::LESSTHAN: return "<";
          case Token::Type::GREATERTHAN: return ">";
          case Token::Type::LESSTHANOREQUAL: return "<=";
          case Token::Type::GREATERTHANOREQUAL: return ">=";
          case Token::Type::EQUAL: return "==";
          case Token::Type::NOTEQUAL: return "!=";
          case Token::Type::COMPARISONOR: return "||";
          case Token::Type::COMPARISONAND: return "&&";
          default: return "?";
        }
      }


      virtual EvaluateTriple evaluate(std::vector<size_t>&, bool getMentionedLabels) override;
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
      size_t relativeOffset = 0;
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