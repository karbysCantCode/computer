#pragma once
#include <string>
#include <filesystem>

class LexerBase {
public:
LexerBase(const std::string& source) : p_source(source) {}
LexerBase(const std::string& source, const std::filesystem::path& path) : p_path(path), p_source(source) {}
LexerBase() {}

protected:
std::string_view p_source;
std::filesystem::path p_path;
size_t p_index = 0;
size_t p_column = 1;
size_t p_line = 1;

inline void reset() {p_index = 0; p_line = 1; p_column = 1;}
char peek(int distance = 0) const;
char consume();
bool match(char c, int distance = 0) const;
void consumeString(const char exitChar);
inline bool notAtEnd() const {return p_index < p_source.size();}
inline bool isAtEnd() const {return !(p_index < p_source.size());}

};