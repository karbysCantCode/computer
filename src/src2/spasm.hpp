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
      std::string* m_fileName;

      Token(const std::string& val, Type type, int line = -1, int col = -1, std::string* fileName = nullptr) :
        m_value(val),
        m_type(type),
        m_line(line),
        m_column(col),
        m_fileName(fileName) {}
      Token(Type type, int line = -1, int col = -1, std::string* fileName = nullptr) :
        m_type(type),
        m_line(line),
        m_column(col),
        m_fileName(fileName) {}

      std::string positionToString() const;
      inline void setString(const std::string& newString) {m_value=newString;}
      inline void setType(const Type& newType) {m_type=newType;}
      inline void setNicheType(const NicheType& newType) {m_nicheType=newType;}
      bool isOneOf(const std::initializer_list<Type> list) const {return std::any_of(list.begin(), list.end(), [this](Type t){return this->m_type == t;});}
    };

    std::vector<Token> lex(std::filesystem::path path, std::unordered_set<std::string>& keywords, Debug::FullLogger* logger = nullptr);
  
  }

  namespace Program {

    namespace Expressions {
      struct Label; struct Declaration;
      
      namespace Operands {
      struct Operand {
        virtual ~Operand() = default;
      };

      struct NumberLiteral : Operand {
        int m_value;

        NumberLiteral(int value) : m_value(value) {}
        NumberLiteral() {}
      };

      struct StringLiteral : Operand {
        std::string m_value;
      };

      struct RegisterLiteral : Operand {
        Arch::RegisterIdentity* m_register;

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
        std::unique_ptr<Operand> LHS;
        std::unique_ptr<Operand> RHS;
        ConstantExpression* parentExpression;
      };

      //ie a label or data declaration
      struct MemoryAddressIdentifier : Operand {
        std::variant<Label*, Declaration*> m_identifier;
      };

      }

      struct Statement {
        virtual ~Statement() = default;
      };

      struct Label : Statement {
        std::string m_name;
        Label* m_parent = nullptr;
        std::unordered_map<std::string, Label*> m_children;

        Label(std::string name, Label* parent = nullptr) : m_name(name), m_parent(parent) {}
      };

      struct Declaration : Statement {
        std::string m_declaredName;
        Arch::DataType* m_dataType;
        std::unique_ptr<Operands::Operand> m_declaration;
      };

      struct Instruction : Statement {
        Arch::Instruction::Instruction* m_instruction;
        std::vector<std::unique_ptr<Operands::Operand>> m_operands;

        Instruction(Arch::Instruction::Instruction* instruction) : m_instruction(instruction) {}
      };
    }

    class ProgramForm {
      public:
      std::filesystem::path m_sourceFilePath;
      std::vector<std::unique_ptr<Expressions::Statement>> m_statements;
      std::unordered_map<std::string, ProgramForm*> m_includedPrograms;
      std::unordered_map<std::string, Expressions::Label*> m_globalLabels;
      //std::unordered_map<std::string, > m_
    };
    
    ProgramForm parseProgram(std::vector<Spasm::Lexer::Token>& tokens, Arch::Architecture& arch, Debug::FullLogger* logger = nullptr, std::filesystem::path path = "");


  }
}