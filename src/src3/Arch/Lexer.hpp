#pragma once
#include "Helpers/LexerBase.hpp"
#include "Helpers/SourceLocation.hpp"
#include "Helpers/TokenBase.hpp"

namespace Arch {


struct Token : TokenBase {
  enum class Type {
    KEYWORD,
    ARGUMENTTYPE,
    IDENTIFIER,
    NEWLINE,
    UNASSIGNED
  };

  Type type = Type::UNASSIGNED;

  Token(std::string_view val, Type t, SourceLocation loc) : type(t), TokenBase(val, loc) {}
};

class TokenHolder : public TokenBaseHolder<Token> {
public:
inline bool match(Token::Type type, size_t distance = 0) const {return peek(distance).type == type;}
};


class ArchLexer : LexerBase {
public:
TokenHolder run(const std::string& source, const std::filesystem::path& sourcePath);

ArchLexer() {}
ArchLexer(const std::string& source) : LexerBase(source) {}



private:

bool isAtWordBoundary();
};

}