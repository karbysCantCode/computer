#include "lexer.hpp"

#include <cctype>
#include <iostream>
#include <cassert>
#include <fstream>

Lexer::Lexer(std::shared_ptr<std::string> src, const std::shared_ptr<std::unordered_set<std::string>> Keywords) : source(src) {
  if (Keywords != nullptr) {
    keywords = Keywords;
  }
}

Lexer::Lexer(const std::shared_ptr<std::unordered_set<std::string>> Keywords) {
  if (Keywords != nullptr) {
    keywords = Keywords;
  }
}

std::vector<Token::Token> Lexer::lexAsm() {
  if (source == nullptr) {return {};}
  std::vector<Token::Token> tokens;
  pos = 0;
  line = 1;
  column = 1;

  if (keywords == nullptr) {assert(true);} //keywords shouldnt be nullptr if calling lexAsm.

  while (!isAtEnd()) {
    skipWhitespace();
    if (isAtEnd()) {break;}

    char c = peek();
    std::cout << c << std::endl;
    
    if (isdigit(c)) {
      //number
      Token::Token token(Token::TokenTypes::NUMBER, line, column);
      std::string value;
      while ((isdigit(peek()) || peek() == '.') && !isAtEnd() && !isWordBoundary(peek())) {
        value.push_back(advance());
      }
      token.SetString(value);
      tokens.push_back(token);

    } else if (isalpha(c) || c == '.') {
      //identifier/keyword
      Token::Token token(Token::TokenTypes::IDENTIFIER, line, column);
      std::string value;

      while (!isAtEnd() && !isWordBoundary(peek())) {
        value.push_back(advance());
      }
      
      if (isKeyword(value)) {
        token.SetType(Token::TokenTypes::KEYWORD);
      }

      token.SetString(value);
      tokens.push_back(token);


    } else if (c == ';') {
      skipComment(peek(1) == '*');
    } else if (c == '@') {
      //directive
      Token::Token token(Token::TokenTypes::DIRECTIVE, line, column);
      std::string value;
      while (!isAtEnd() && !isWordBoundary(peek())) {
        value.push_back(advance());
      }
      token.SetString(value);
      tokens.push_back(token);
    } else if (c == '\'' || c == '"') {
      Token::Token token(Token::TokenTypes::STRING, line, column);
      std::string value;
      advance();
      const char start = c;
      while (!isAtEnd() && peek() != start) {
        char c = advance();
        
        if (c == '\\' && !isAtEnd()) {
          char next = advance();
          switch (next) {
            case 'n':  c = '\n'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            default: break;
          }
        }
        value.push_back(c);
      }
      advance();
      token.SetString(value);
      tokens.push_back(token);
    } else {
      Token::Token token(std::string(1, c),Token::TokenTypes::UNASSIGNED, line, column);
      advance();
      switch (c) {
        case '(':
          token.type = Token::TokenTypes::OPENPAREN;
          break;
        case ')':
          token.type = Token::TokenTypes::CLOSEPAREN;
          break;
        case ',':
          token.type = Token::TokenTypes::COMMA;
          break;
        case '[':
          token.type = Token::TokenTypes::OPENSQUARE;
          break;
        case ']':
          token.type = Token::TokenTypes::CLOSESQUARE;
          break;
        case '{':
          token.type = Token::TokenTypes::OPENBLOCK;
          break;
        case '}':
          token.type = Token::TokenTypes::CLOSEBLOCK;
          break;
        default:
          break;
      
      }

      tokens.push_back(token);
    }

  
  }
  return tokens;
}

std::vector<ArchToken::ArchToken> Lexer::lexArch() {
  if (source == nullptr) {return {};}
  std::vector<ArchToken::ArchToken> tokens;
  pos = 0;
  line = 1;
  column = 1;

  while (!isAtEnd()) {
    if (skipWhitespace(true)) {tokens.emplace_back(std::string{'\n'}, ArchToken::ArchTokenTypes::NEWLINE, line, column);}
    if (isAtEnd()) {break;}

    char c = peek();
    
    if (c == ';') {
      skipComment(false);
    } else if (c == '.') {
      advance();
      tokens.emplace_back(getUntilWordBoundary(),ArchToken::ArchTokenTypes::KEYWORD, line, column);
    } else if (c == '\n') {
      tokens.emplace_back(std::string{'\n'}, ArchToken::ArchTokenTypes::NEWLINE, line, column);
    } else {
      const std::string value = getUntilWordBoundary();
      if (value == "REG" || value == "IMM") {
        tokens.emplace_back(value,ArchToken::ArchTokenTypes::ARGUMENTTYPE, line, column);
      } else {
        tokens.emplace_back(value,ArchToken::ArchTokenTypes::IDENTIFIER, line, column);
      }
      
    }
  }
  return tokens;
}

char Lexer::peek(size_t lookAhead) const noexcept {
  size_t i = pos + lookAhead;
  return i < source->size() ? (*source)[i] : '\0';
}

void Lexer::setTokenFilePosition(Token::Token& token) const {
  token.line = line;
  token.column = column;
}

char Lexer::advance() noexcept {

  if (pos >= source->length()) {
        return '\0';
    }
  char c = (*source)[pos];
  if (c == '\n') {
    column = 1;
    line++;
  } else {
    column++;
  }
  pos++;
  return c;
}

bool Lexer::skipWhitespace(bool insertOnNewline) {
  bool insert = false;
  if (insertOnNewline) {
    while (peek() == ' '|| peek() =='\n') {
      if (peek() == '\n') {
        insert = true;
      }
      advance();
    }
  } else {
    while (peek() == ' '|| peek() =='\n') {advance();}
  }
  return insert;
  
}

void Lexer::skipComment(bool isMultiline) {
  if (isMultiline) {
    advance();
    advance();
    while (!isAtEnd() && !(peek() == '*' && peek(1) == ';')) {advance();}
    advance();
    advance();
  } else {
    while (!isAtEnd() && peek() != '\n') {advance();}
  }
}

bool Lexer::isWordBoundary(char c) noexcept {
  switch (c) {
    case '(':
    case ')':
    case ',':
    case '\'':
    case '"':
    case '[':
    case ']':
    case '{':
    case '}':
      return true;
    default:
      return std::isspace(c);
  }
}

std::string Lexer::getUntilWordBoundary() {
  std::string str;
  while (!isAtEnd() && !isWordBoundary(peek())) {
    str.push_back(advance());
  }
  return str;
}

bool Lexer::isKeyword(std::string& str) {
  bool iskword = std::find(keywords->begin(), keywords->end(), str) != keywords->end();
  std::cout << "is keyword? \"" << str << '\"' << '=' << iskword << std::endl;
  return iskword;
}

std::string Lexer::readFile(const std::string& absFilePath) {
  std::ifstream file(absFilePath);
  if (!file.is_open()) {
      std::cerr << "Failed to open the file \"" << absFilePath << '\"' << std::endl;
      return std::string{};
  }

  // Read the entire file content into a std::string using iterators
  std::string file_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return file_contents;
}
