#pragma once

#include <string>
#include <vector>
#include <iostream>

#include "SourceLocation.hpp"

struct TokenBase {
  std::string_view value;
  SourceLocation location;

  TokenBase(std::string_view val, SourceLocation loc) : location(loc), value(val) {}
};

template<typename TokenT>
struct TokenBaseHolder {
public:
std::vector<TokenT> m_tokens;
size_t m_index = 0;

inline bool isAtEnd() const {return m_index >= m_tokens.size();}
inline bool notAtEnd() const {return m_index < m_tokens.size();}
inline TokenT peek(int distance = 0) const {
  int pos = m_index + distance;
  pos = pos > m_tokens.size() ? m_tokens.size()-1 : pos;
  pos = pos < 0 ? 0 : pos;
  return m_tokens[pos];
}
inline TokenT consume() {
  return notAtEnd() ? m_tokens[m_index++] : m_tokens[m_tokens.size()-1];
}
inline void skip(size_t distance = 1) {m_index += distance;}
inline void reset() {m_index = 0;}
bool match(int distance = 0) const; 
};