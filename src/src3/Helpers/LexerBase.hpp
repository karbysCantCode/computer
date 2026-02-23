#pragma once
#include <string>

class LexerBase {
public:
LexerBase(const std::string& source) : p_source(source) {}
LexerBase() {}

protected:
std::string p_source;
size_t p_index = 0;
size_t p_column = 1;
size_t p_line = 1;

inline void reset() {p_index = 0; p_line = 1; p_column = 1;}
char peek(size_t distance = 0) const;
char consume();
bool match(char c, size_t distance = 0) const;
inline bool notAtEnd() const {return p_index < p_source.size();}
inline bool isAtEnd() const {return !(p_index < p_source.size());}

};