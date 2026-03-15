#include "Spasm/Lexer.hpp"

namespace Spasm {


TokenHolder SpasmLexer::run(const std::string& source, const std::filesystem::path& path) {
  //keywords are not used because otherwise they'd have to be
  //  matched for the keyword detection, and linking to an
  //  architecture object.

  TokenHolder output;

  size_t sliceStartIndex = p_index;
  SourceLocation sliceStartLocation(path, p_line, p_column);
  Token::Type sliceType = Token::Type::UNASSIGNED;
  #define sliceStartPtr p_source.data() + sliceStartIndex
  while (notAtEnd()) {
    while (notAtEnd() && isWhitespace()) {consume();}
    sliceStartIndex = p_index;
    sliceStartLocation.line = p_line;
    sliceStartLocation.column = p_column;
    const char c = peek();

    switch (c) {
      case '\n':
      {
        continue;
        break;
      }
      case '(':
      {
        continue;
        break;
      }
      case ')':
      {
        continue;
        break;
      }
      case '"':
      {
        continue;
        break;
      }
      case ',':
      {
        continue;
        break;
      }
      case ':':
      {
        continue;
        break;
      }
      case '.':
      {
        continue;
        break;
      }
      case '{':
      {
        continue;
        break;
      }
      case '}':
      {
        continue;
        break;
      }
      case '[':
      {
        continue;
        break;
      }
      case ']':
      {
        continue;
        break;
      }
      case '@':
      {
        continue;
        break;
      }
      case '|':
      {
        continue;
        break;
      }
      case '^':
      {
        continue;
        break;
      }
      case '&':
      {
        continue;
        break;
      }
      case '<':
      {
        if (!match('<', 1)) {break;} //fallthrough to identifier parsing ig

        continue;
        break;
      }
      case '>':
      {
        if (!match('>', 1)) {break;} //fallthrough to identifier parsing ig

        continue;
        break;
      }
      case '+':
      {
        continue;
        break;
      }
      case '-':
      {
        continue;
        break;
      }
      case '*':
      {
        continue;
        break;
      }
      case '/':
      {
        continue;
        break;
      }
      case '%':
      {
        continue;
        break;
      }
      case '!':
      {
        continue;
        break;
      }
      
    }

  }

  output.reset();
  return output;
}

bool SpasmLexer::isWhitespace() {
  switch(peek()) {
    case ' ':
    case '\t':
    return true;
    default:
    return false;
  }
}



}