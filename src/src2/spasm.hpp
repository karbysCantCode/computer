#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_set>
#include <memory>
#include <variant>

#include "debugHelpers.hpp"
#include "arch.hpp"



namespace Spasm {

  namespace Lexer {
    struct Token
    {
      enum class Type {
        IDENTIFIER,
        KEYWORD,
        OPENPAREN,
        CLOSEPAREN,
        STRING,
        NUMBER,
        COMMA,
        COLON,
        PERIOD,
        OPENBLOCK,
        CLOSEBLOCK,
        OPENSQUARE,
        CLOSESQUARE,
        UNASSIGNED,
        DIRECTIVE,

        BITWISEOR,
        BITWISEXOR,
        BITWISEAND,
        LEFTSHIFT,
        RIGHTSHIFT,
        ADD,
        SUBTRACT,
        MULTIPLY,
        DIVIDE,
        MOD,
        BITWISENOT,
        
        NEWLINE,
        MACRONEWLINE
      };
      enum class NicheType {
        UNASSIGNED,
        DIRECTIVE_DEFINE,
        DIRECTIVE_ENTRY,
        DIRECTIVE_INCLUDE,

        NUMBER_HEX,
        NUMBER_DEC,
        NUMBER_BIN


      };

      Type m_type = Type::UNASSIGNED;
      NicheType m_nicheType = NicheType::UNASSIGNED;
      std::string m_value;
      int m_line = -1;
      int m_column = -1;
      const std::filesystem::path* m_fileName;

      Token(const std::string& val, Type type, const std::filesystem::path* fileName, int line = -1, int col = -1) :
        m_value(val),
        m_type(type),
        m_line(line),
        m_column(col),
        m_fileName(fileName) {}
      Token(Type type, const std::filesystem::path* fileName, int line = -1, int col = -1) :
        m_type(type),
        m_line(line),
        m_column(col),
        m_fileName(fileName) {}

      constexpr const char* typeToString() const {
        switch (m_type) {
          case Type::IDENTIFIER:   return "IDENTIFIER";
          case Type::KEYWORD:      return "KEYWORD";
          case Type::OPENPAREN:    return "OPENPAREN";
          case Type::CLOSEPAREN:   return "CLOSEPAREN";
          case Type::STRING:       return "STRING";
          case Type::NUMBER:       return "NUMBER";
          case Type::COMMA:        return "COMMA";
          case Type::COLON:        return "COLON";
          case Type::PERIOD:       return "PERIOD";
          case Type::OPENBLOCK:    return "OPENBLOCK";
          case Type::CLOSEBLOCK:   return "CLOSEBLOCK";
          case Type::OPENSQUARE:   return "OPENSQUARE";
          case Type::CLOSESQUARE:  return "CLOSESQUARE";
          case Type::UNASSIGNED:   return "UNASSIGNED";
          case Type::DIRECTIVE:    return "DIRECTIVE";

          case Type::BITWISEOR:    return "BITWISEOR";
          case Type::BITWISEXOR:   return "BITWISEXOR";
          case Type::BITWISEAND:   return "BITWISEAND";
          case Type::LEFTSHIFT:    return "LEFTSHIFT";
          case Type::RIGHTSHIFT:   return "RIGHTSHIFT";
          case Type::ADD:          return "ADD";
          case Type::SUBTRACT:     return "SUBTRACT";
          case Type::MULTIPLY:     return "MULTIPLY";
          case Type::DIVIDE:       return "DIVIDE";
          case Type::MOD:          return "MOD";
          case Type::BITWISENOT:   return "BITWISENOT";

          case Type::NEWLINE:      return "NEWLINE";
          case Type::MACRONEWLINE: return "MACRONEWLINE";
        }

        return "UNKNOWN"; // fallback safety
      }
      std::string positionToString() const;
      inline void setString(const std::string& newString) {m_value=newString;}
      inline void setType(const Type& newType) {m_type=newType;}
      inline void setNicheType(const NicheType& newType) {m_nicheType=newType;}
      bool isOneOf(const std::initializer_list<Type> list) const {return std::any_of(list.begin(), list.end(), [this](Type t){return this->m_type == t;});}
      inline bool isDirective()  const {return m_type == Type::DIRECTIVE; }
      inline bool isIdentifier() const {return m_type == Type::IDENTIFIER;}
      inline bool isCloseBlock() const {return m_type == Type::CLOSEBLOCK;}
      inline bool isCloseParen() const {return m_type == Type::CLOSEPAREN;}
      inline bool isComma()      const {return m_type == Type::COMMA; }
      inline bool isOpenParen()  const {return m_type == Type::OPENPAREN;}
      inline bool isOpenBlock()  const {return m_type == Type::OPENBLOCK;}
      inline bool isKeyword()    const {return m_type == Type::KEYWORD;}

    };

    std::vector<Token> lex(const std::filesystem::path& path, std::unordered_set<std::string>& keywords, Debug::FullLogger* logger = nullptr);
    struct TokenHolder {
      public:
      std::vector<Token> m_tokens;

      inline bool isAtEnd() const {return !(p_index < m_tokens.size());}
      inline bool notAtEnd() const {return (p_index < m_tokens.size());}
      inline const Token& peek(size_t distance = 0) const;
      inline const Token& consume();
      inline void skip(size_t distance = 1);
      inline void skipUntilType(Token::Type);
      private:
      size_t p_index;
      std::filesystem::path p_filepath;
    };
  }

  namespace Program {

    namespace Expressions {
      struct Label; struct Declaration;
      
      namespace Operands {
      struct Operand {
        virtual ~Operand() = default;

        virtual std::string toString() const {assert(false); return "";}
      };

      struct NumberLiteral : Operand {
        int m_value;

        NumberLiteral(int value) : m_value(value) {}
        NumberLiteral() {}

        std::string toString() const override;
      };

      struct StringLiteral : Operand {
        std::string m_value;

        std::string toString() const override;
      };

      struct RegisterLiteral : Operand {
        Arch::RegisterIdentity* m_register;
        std::string toString() const override;
        RegisterLiteral(Arch::RegisterIdentity* reg) : m_register(reg) {}
      };

      struct ConstantExpression : Operand {
        enum class Type {
          MULTIPLY,
          DIVIDE,
          MODULO,
          ADDITION,
          SUBTRACTION,
          AND,
          OR,
          XOR,
          NOT,
          LEFTSHIFT,
          RIGHTSHIFT
        };

        bool m_resolved = false;
        int m_value = 0;
        Type m_type;
        std::unique_ptr<Operand> m_LHS;
        std::unique_ptr<Operand> m_RHS;
        ConstantExpression* m_parentExpression;

        std::string toString() const override;

        ConstantExpression(Type type, std::unique_ptr<Operand> lhs, std::unique_ptr<Operand> rhs) : m_type(type),m_LHS(std::move(lhs)),m_RHS(std::move(rhs)) {}
        // ConstantExpression(ConstantExpression& expr) :
        //    m_resolved(expr.m_resolved), 
        //    m_value(expr.m_value),
        //    m_type(expr.m_type),
        //    m_LHS(std::move(expr.m_LHS)),
        //    m_RHS(std::move(expr.m_RHS)),
        //    m_parentExpression(expr.m_parentExpression) {}
      };

      //ie a label or data declaration
      struct MemoryAddressIdentifier : Operand {
        std::variant<Label*, Declaration*> m_identifier;

        std::string toString() const override;

        MemoryAddressIdentifier(std::variant<Label*, Declaration*> identifier) : m_identifier(identifier) {}
        MemoryAddressIdentifier() {}
      };

      }

      struct Statement {
        virtual ~Statement() = default;

        virtual std::string toString(size_t padding = 0, size_t indent = 0, size_t minArgWidth = 5, bool basicData = true) const {assert(false); return "";};
      };

      /*
      unowned labels (implied existence to be determined at link time)
      are owned by the programs unowned list
      until they are defined or linked to a definition from another file
      if they already exist (unowned) but are being defined, they are moved to 
      the top (or bottom?) of the statement array.
      
      */
      struct Label : Statement {
        std::string m_name;
        Label* m_parent = nullptr;
        std::unordered_map<std::string, Label*> m_children;
        bool m_declared = false;

        Label(std::string name, Label* parent = nullptr, bool declared = false) : m_name(name), m_parent(parent), m_declared(declared) {}
      
        std::string toString(size_t padding = 0, size_t indent = 0, size_t minArgWidth = 5, bool basicData = true) const override;
      };

      struct Declaration : Statement {
        std::string m_declaredName;
        Arch::DataType* m_dataType;
        std::unique_ptr<Operands::Operand> m_declaration;

        std::string toString(size_t padding = 0, size_t indent = 0, size_t minArgWidth = 5, bool basicData = true) const override;
      };

      struct Instruction : Statement {
        Arch::Instruction::Instruction* m_instruction;
        std::vector<std::unique_ptr<Operands::Operand>> m_operands;

        std::string toString(size_t padding = 0, size_t indent = 0, size_t minArgWidth = 5, bool basicData = true) const override;

        Instruction(Arch::Instruction::Instruction* instruction) : m_instruction(instruction) {}
      };
    }

    class ProgramForm {
      public:
      std::filesystem::path m_sourceFilePath;
      std::vector<std::unique_ptr<Expressions::Statement>> m_statements;
      std::unordered_map<std::filesystem::path, std::unique_ptr<Spasm::Program::ProgramForm>*> m_includedPrograms;
      std::unordered_map<std::string, Expressions::Label*> m_globalLabels;
      std::unordered_map<std::string, Expressions::Declaration*> m_dataDeclarations;
      std::unordered_map<std::string, std::unique_ptr<Expressions::Label>> m_unownedLabels;

    };
    
    class ProgramParser {
      public:
      ProgramParser(Debug::FullLogger* logger) : p_logger(logger) {}
      bool run(Spasm::Lexer::TokenHolder& tokenHolder, Arch::Architecture& arch, Spasm::Program::ProgramForm& program, std::filesystem::path path = "");
    
      private:
      Debug::FullLogger* p_logger;
      inline void logError(const Spasm::Lexer::Token& errToken, const std::string& message) {
        if (p_logger != nullptr) p_logger->Errors.logMessage(errToken.positionToString() + ": error: " + message);
      };
      inline void logWarning(const Spasm::Lexer::Token& errToken, const std::string& message) {
        if (p_logger != nullptr) p_logger->Warnings.logMessage(errToken.positionToString() + ": warning: " + message);
      };
      inline void logDebug(const Spasm::Lexer::Token& errToken, const std::string& message) {
        if (p_logger != nullptr) p_logger->Debugs.logMessage(errToken.positionToString() + ": debug: " + message);
      };

      int resolveNumber(const Spasm::Lexer::Token& token) const;
      using operandUniquePtr = std::unique_ptr<Program::Expressions::Operands::Operand>;
      operandUniquePtr parsePrimary(Spasm::Lexer::TokenHolder&, Spasm::Program::ProgramForm&);
      operandUniquePtr parseNot(Spasm::Lexer::TokenHolder&, Spasm::Program::ProgramForm&);
      operandUniquePtr parseMultiplicative(Spasm::Lexer::TokenHolder&, Spasm::Program::ProgramForm&);
      operandUniquePtr parseAdditive(Spasm::Lexer::TokenHolder&, Spasm::Program::ProgramForm&);
      operandUniquePtr parseShift(Spasm::Lexer::TokenHolder&, Spasm::Program::ProgramForm&);
      operandUniquePtr parseAnd(Spasm::Lexer::TokenHolder&, Spasm::Program::ProgramForm&);
      operandUniquePtr parseXor(Spasm::Lexer::TokenHolder&, Spasm::Program::ProgramForm&);
      operandUniquePtr parseOr(Spasm::Lexer::TokenHolder&, Spasm::Program::ProgramForm&);
      operandUniquePtr parseExpression(Spasm::Lexer::TokenHolder&, Spasm::Program::ProgramForm&);

      void handleInstruction(Spasm::Lexer::TokenHolder&, Spasm::Program::ProgramForm&, Arch::Architecture&, Arch::Instruction::Instruction&);
      void handlenDatatype(Spasm::Lexer::TokenHolder&);
      //void handleLabel(Spasm::Lexer::TokenHolder&);

      Program::Expressions::Label* consumeTokensForLabel(Lexer::Token::Type delimiter, Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& currentProgram, bool delcNewIfNotExist = false, bool delimOnNonPeriod = false);
    };
    bool parseProgram(std::vector<Spasm::Lexer::Token>& tokens, Arch::Architecture& arch, Spasm::Program::ProgramForm& program, Debug::FullLogger* logger = nullptr, std::filesystem::path path = "");

  };
}