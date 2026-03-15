#include "SMake/Lexer.hpp"

#include <filesystem>

namespace SMake {

TokenHolder SMakeLexer::run(const std::string& source, const std::filesystem::path& sourcePath) {
  p_source = source;

  TokenHolder output;

  size_t sliceStartIndex = p_index;
  SourceLocation sliceStartLocation(sourcePath, p_line, p_column);
  Token::Type sliceType = Token::Type::UNASSIGNED;

  #define sliceStartPtr p_source.data() + sliceStartIndex

  std::string_view currentString{p_source.data(), 0};

  while (notAtEnd()) {
    while(isWhitespace()) {consume();}
    sliceStartIndex = p_index;
    sliceStartLocation.line = p_line;
    sliceStartLocation.column = p_column;
    currentString = {sliceStartPtr, 0};
    const char c = peek();

    switch (c) {
      case '.': //keyword
      {
      consume();
      sliceStartIndex = p_index;
      while(!isAtWordBoundary() && notAtEnd()) {
        consume();
      }
      const size_t length = p_index - sliceStartIndex;
      output.m_tokens.emplace_back(
        std::string_view{sliceStartPtr,length}, 
        Token::Type::KEYWORD, 
        sliceStartLocation
      );
      break;}
      case '{':
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::OPENBLOCK, 
          sliceStartLocation
        );
      break;
      case '\n':
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::NEWLINE, 
          sliceStartLocation
        );
      break;
      case '}':
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::CLOSEBLOCK, 
          sliceStartLocation
        );
      break;
      case '(':
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::OPENPAREN, 
          sliceStartLocation
        );
      break;
      case ')':
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::CLOSEPAREN, 
          sliceStartLocation
        );
      break;
      case ',':
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::COMMA, 
          sliceStartLocation
        );
      break;
      case '"':{
        consume(); // eat "
        sliceStartIndex = p_index;
        while (!match('"') && notAtEnd()) {
          consume();
        }
        const size_t length = p_index - sliceStartIndex;
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,length}, 
          Token::Type::STRING, 
          sliceStartLocation
        );
        consume(); // eat "
      break;
      }
      case '\0':{
        p_index = p_source.size();
      break;}
      default:
      {
      while(!isAtWordBoundary() && notAtEnd()) {
        consume();
      }
      const size_t length = p_index - sliceStartIndex;
      output.m_tokens.emplace_back(
        std::string_view{sliceStartPtr,length}, 
        Token::Type::IDENTIFIER, 
        sliceStartLocation
      );
      }
    }
  }
  output.reset();
  return output;
}

bool SMakeLexer::isAtWordBoundary() {
  switch (peek()) {
    case ' ':
    case '\n':
    case '\0':
    case '{':
    case '}':
    case '(':
    case ')':
    case ',':
    case '.':
    case '"':
    return true;
    default:
    return false;
  }
}

bool SMakeLexer::isWhitespace() {
  switch (peek()) {
    case ' ':
    case '\n':
    case '\t':
    return true;
    default:
    return false;
  }
}

}