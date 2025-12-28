#pragma once

#include <vector>
#include <unordered_set>
#include <memory>

#include "token.hpp"


class Lexer {
  public:
  std::shared_ptr<std::string> source = nullptr;


  Lexer(std::shared_ptr<std::string> source, const std::shared_ptr<std::unordered_set<std::string>> Keywords);
  Lexer(const std::shared_ptr<std::unordered_set<std::string>> Keywords);
  Lexer() {}

  std::vector<Token::Token> lexAsm();
  std::vector<ArchToken::ArchToken> lexArch();

  std::string readFile(const std::string& absFilePath);
  private:
  
  std::shared_ptr<std::unordered_set<std::string>> keywords = nullptr;
  size_t pos = 0;
  size_t line = 1;
  size_t column = 1;

  bool isAtEnd() noexcept {return pos >= source->length();}

  char advance() noexcept;
  char peek(size_t lookAhead = 0) const noexcept;
  std::string getUntilWordBoundary();
  std::string getUntilDelimiter(char delimiter);

  void setTokenFilePosition(Token::Token& token) const;
  bool skipWhitespace(bool insertOnNewline = false);
  void skipComment(bool isMultiline);
  bool isWordBoundary(char c) noexcept;
  bool isKeyword(std::string& str);

};