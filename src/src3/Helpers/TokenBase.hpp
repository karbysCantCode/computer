#pragma once

#include <string>
#include <vector>

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
size_t m_index;

inline bool isAtEnd() const {return m_index >= m_tokens.size();}
inline bool notAtEnd() const {return m_index < m_tokens.size();}
inline TokenT peek(size_t distance = 0) const {
  const size_t pos = m_index + distance;
  return pos < m_tokens.size() ? m_tokens[pos] : m_tokens[m_tokens.size()-1];
}
inline TokenT consume() {
  return notAtEnd() ? m_tokens[m_index] : m_tokens[m_tokens.size()-1];
}
inline void skip(size_t distance = 1) {m_index++;}
inline void reset() {m_index = 0;}
};