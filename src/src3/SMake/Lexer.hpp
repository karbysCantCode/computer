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
    INVALID
  };

  Type type = Type::UNASSIGNED;
  
  Token(std::string_view val, Type t, SourceLocation loc) : type(t), TokenBase(val, loc) {}
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
};
}