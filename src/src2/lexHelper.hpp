#pragma once

#include <string>
#include <unordered_set>
#include <cassert>

#include "debugHelpers.hpp"

class LexHelper {
  public:
  size_t m_index = 0;
  size_t m_column = 1;
  size_t m_line = 1;
  const std::string m_source;
  const std::unordered_set<std::string>* m_keywords;
  Debug::MessageLogger* m_errorLogger;

  LexHelper(const std::string source, const std::unordered_set<std::string>* keywords = nullptr, Debug::MessageLogger* errorLogger = nullptr) : 
    m_source(source),
    m_keywords(keywords),
    m_errorLogger(errorLogger) {}

  inline char peek(size_t distance = 0) {
    return m_index + distance < m_source.size() ? m_source[m_index+distance] : '\0';
  };
  inline char consume() {
    bool notOutOfRange = m_index < m_source.size();
    m_column += notOutOfRange ? 1 : 0;
    char c = notOutOfRange ? m_source[m_index] : '\0';
    m_index += notOutOfRange ? 1 : 0;
    if (c == '\n') {m_column = 1; m_line++;}
    return c;
  };
  inline bool notAtEnd() const {return m_index<m_source.size();}
  inline bool skipWhitespace(bool insertOnNewline = false) {
    bool insert = false;
    if (insertOnNewline) {
      while (peek() == ' '|| peek() =='\n') {
        if (peek() == '\n') {
          insert = true;
        }
        consume();
      }
    } else {
      while (peek() == ' '|| peek() =='\n') {consume();}
    }
    return insert;
  };
  inline bool isWordBoundary(char c) {
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
  };

  inline bool spasmIsWordBoundary(char c) {
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
      case ':':
      case '.':
      case '+':
      case '-':
      case '=':
      case '!':
      case '^':
      case '&':
      case '|':
      case '*':
      case '/':
        return true;
      default:
        return std::isspace(c);
    }
  };

  inline void skipComment(bool isMultiline) {
    if (isMultiline) {
      consume();
      consume();
      while (notAtEnd() && !(peek() == '*' && peek(1) == ';')) {consume();}
      consume();
      consume();
    } else {
      while (notAtEnd() && peek() != '\n') {consume();}
    }
  };
  inline bool isKeyword(const std::string& kword) {
    return m_keywords == nullptr ? false : m_keywords->find(kword) != m_keywords->end();
  };
  inline std::string consumeString(const char delimiter) {
  std::string value;
  consume();
  while (notAtEnd() && peek() != delimiter) {
    char c = consume();
    
    if (c == '\\' && notAtEnd()) {
      char next = consume();
      switch (next) {
        case 'n':  c = '\n'; break;
        case '\\': c = '\\'; break;
        case '\'': c = '\''; break;
        case '"':  c = '"';  break;
        default: if (m_errorLogger!=nullptr) {m_errorLogger->logMessage("Unknown escaped character \"\\" + std::to_string(next) + "\" at line " + std::to_string(m_line) + " column " + std::to_string(m_column));}; break; //should error somehow
      }
    }
    value.push_back(c);
  }
  consume();
  return value;
}
  inline std::string getUntilWordBoundary() {
    std::string str;
    while (notAtEnd() && !isWordBoundary(peek())) {
      str.push_back(consume());
    }
    return str;
  }

  inline std::string spasmGetUntilWordBoundary() {
    std::string str;
    while (notAtEnd() && !spasmIsWordBoundary(peek())) {
      str.push_back(consume());
    }
    return str;
  }
};

// defines to shorthand lexHelper!
// #define notAtEnd() lexHelper.notAtEnd()
// #define skipWhitespace(arg) lexHelper.skipWhitespace(arg)
// #define peek(arg) lexHelper.peek(arg)
// #define line lexHelper.m_line
// #define column lexHelper.m_column
// #define isWordBoundary(arg) lexHelper.isWordBoundary(arg)
// #define consume() lexHelper.consume()
// #define isKeyword(arg) lexHelper.isKeyword(arg)
// #define skipComment(arg) lexHelper.skipComment(arg)
// #define consumeString(arg) lexHelper.consumeString(arg)
// #define getUntilWordBoundary() lexHelper.getUntilWordBoundary()

// #undef notAtEnd
// #undef skipWhitespace
// #undef peek
// #undef line 
// #undef column 
// #undef isWordBoundary
// #undef consume
// #undef isKeyword
// #undef skipComment
// #undef consumeString
// #undef getUntilWordBoundary