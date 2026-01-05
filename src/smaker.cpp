#include "smaker.hpp"
#include <cassert>

smake::SmakeTokenParser::SmakeTokenParser(const std::vector<SmakeToken::SmakeToken>& tks, SMakeProject& targetProject) : tokens(tks), project(targetProject) {
  while (!isAtEnd()) {
    const SmakeToken::SmakeToken& token = tokens[pos];
    if (token.type == SmakeToken::SmakeTokenTypes::KEYWORD) {
      
    } else {
      error(false); //baddddd
    }
  }
}

template <size_t N>
bool smake::SmakeTokenParser::validateTokenTypes(const SmakeToken::SmakeTokenTypes (&tokenTypes)[N]) const {
  int i = 0;
  for (const auto& type : tokenTypes) {
    if (peek(i).type != type.type) return false;
    i++;
  }
  return true;
}

SmakeToken::SmakeToken smake::SmakeTokenParser::advance() {
  if (isAtEnd()) return SmakeToken::SmakeToken(std::string{}, SmakeToken::SmakeTokenTypes::INVALID, -1, -1);
  const auto& token = tokens[pos];
  pos++;
  return token;
}

SmakeToken::SmakeToken smake::SmakeTokenParser::peek(const size_t dist) const {
  if (!(pos+dist < tokens.size())) return SmakeToken::SmakeToken(std::string{}, SmakeToken::SmakeTokenTypes::INVALID, -1, -1);
  return tokens[pos+dist];
}

bool smake::SmakeTokenParser::isAtEnd() const {
  return !(pos < tokens.size());
}