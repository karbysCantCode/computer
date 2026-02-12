#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_set>

#include "debugHelpers.hpp"



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
        OPENBLOCK,
        CLOSEBLOCK,
        OPENSQUARE,
        CLOSESQUARE,
        UNASSIGNED,
        DIRECTIVE,

        ADD,
        SUBTRACT,
        MULTIPLY,
        DIVIDE,
        BITWISEAND,
        BITWISEOR,
        BITWISEXOR,
        BITWISENOT,
        LEFTSHIFT,
        RIGHTSHIFT,

        MACRONEWLINE
      };
      enum class NicheType {
        UNASSIGNED,
        DIRECTIVE_DEFINE,
        DIRECTIVE_ENTRY,
        DIRECTIVE_INCLUDE
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

      std::string positionToString();
      inline void setString(const std::string& newString) {m_value=newString;}
      inline void setType(const Type& newType) {m_type=newType;}
      inline void setNicheType(const NicheType& newType) {m_nicheType=newType;}
    };

    std::vector<Token> lex(std::filesystem::path path, std::unordered_set<std::string>& keywords, Debug::FullLogger* logger = nullptr);
  }
}