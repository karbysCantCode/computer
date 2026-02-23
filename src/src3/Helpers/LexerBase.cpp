#include "LexerBase.hpp"

char LexerBase::consume() {
  char ret = p_index < p_source.size() ? p_source[p_index] : '\0';
  p_index++;
  p_column++;
  
  if (ret == '\n') {
    p_line++;
    p_column = 1;
  }
  return ret;
}

char LexerBase::peek(size_t distance) const {
  const size_t val = distance + p_index;
  return val < p_source.size() ? p_source[val] : '\0';
}

bool LexerBase::match(char c, size_t distance) const {
  return peek(distance) == c;
}