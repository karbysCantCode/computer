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

char LexerBase::peek(int distance) const {
  const int val = distance + p_index;
  return val < p_source.size() ? p_source[val] : '\0';
}

bool LexerBase::match(char c, int distance) const {
  return peek(distance) == c;
}