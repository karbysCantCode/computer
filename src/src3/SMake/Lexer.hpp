#pragma once

#include <string>
#include <filesystem>

#include "Helpers/LexerBase.hpp"
#include "Helpers/TokenBase.hpp"

namespace SMake {

struct Token : TokenBase {
  enum class Type {
    UNASSIGNED,
    KEYWORD,
    OPENPAREN,
    CLOSEPAREN,
    OPENBLOCK,
    CLOSEBLOCK,
    STRING,
    IDENTIFIER,
    COMMA,
    NEWLINE,
    INVALID
  };

  Type type = Type::UNASSIGNED;
  
  Token(std::string_view val, Type t, SourceLocation loc) : type(t), TokenBase(val, loc) {}
  constexpr const char* typeToString() const {
    switch (type) {
      case Type::UNASSIGNED: return "UNASSIGNED";
      case Type::KEYWORD:    return "KEYWORD";
      case Type::OPENPAREN:  return "OPENPAREN";
      case Type::CLOSEPAREN: return "CLOSEPAREN";
      case Type::OPENBLOCK:  return "OPENBLOCK";
      case Type::CLOSEBLOCK: return "CLOSEBLOCK";
      case Type::STRING:     return "STRING";
      case Type::IDENTIFIER: return "IDENTIFIER";
      case Type::COMMA:      return "COMMA";
      case Type::NEWLINE:      return "NEWLINE";
      case Type::INVALID:    return "INVALID";
      default: return "UNKNOWN";
    }
  }
};

struct TokenHolder : TokenBaseHolder<Token> {
  public:
  inline bool match(Token::Type type, size_t distance = 0) const {return peek(distance).type == type;}
};

class SMakeLexer : LexerBase {
public:
TokenHolder run(const std::string& source, const std::filesystem::path& sourcePath);
SMakeLexer() {}
SMakeLexer(const std::string& source) : LexerBase(source) {}

private:

bool isAtWordBoundary();
bool isWhitespace();
};
}