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
  #define setSliceStart \
    sliceStartIndex = p_index; \
    sliceStartLocation.line = p_line; \
    sliceStartLocation.column = p_column \

  while (notAtEnd()) {
    while (notAtEnd() && isWhitespace()) {consume();}
    setSliceStart;
    const char c = peek();

    switch (c) {
      case '\n':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::NEWLINE, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '(':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::OPENPAREN, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case ')':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::CLOSEPAREN, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '"':
      {
        //STRING
        consume();
        setSliceStart;
        static_assert(false); //pickup here.
        while (!match('"') && notAtEnd()) {
          consume();
        }
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::CLOSEPAREN, 
          sliceStartLocation
        );

        continue;
        break;
      }
      case ',':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::COMMA, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case ':':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::COLON, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '.':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::PERIOD, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '{':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::OPENBLOCK, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '}':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::CLOSEBLOCK, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '[':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::OPENSQUARE, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case ']':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::CLOSESQUARE, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '@':
      {
        //directive
        consume();
        std::string directiveWord;
        while (!isAtWordBoundary() && notAtEnd()) {
          directiveWord.push_back(consume());
        }
        continue;
        break;
      }
      case '|':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::BITWISEOR,
          sliceStartLocation
        );
        continue;
        break;
      }
      case '^':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::BITWISEXOR, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '&':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::BITWISEAND, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '<':
      {
        if (!match('<', 1)) {break;} //fallthrough to identifier parsing ig
        consume();
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,2}, 
          Token::Type::LEFTSHIFT, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '>':
      {
        if (!match('>', 1)) {break;} //fallthrough to identifier parsing ig
        consume();
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,2}, 
          Token::Type::LEFTSHIFT, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '+':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::ADD, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '-':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::SUBTRACT, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '*':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::MULTIPLY, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '/':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::DIVIDE, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '%':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::MOD, 
          sliceStartLocation
        );
        continue;
        break;
      }
      case '!':
      {
        consume();
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr,1}, 
          Token::Type::BITWISENOT, 
          sliceStartLocation
        );
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

bool SpasmLexer::isAtWordBoundary() {
  switch (peek()) {
    case ' ':
    case '\n':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case ',':
    case '.':
    case '|':
    case '&':
    case '^':
    case '%':
    case '*':
    case '/':
    case '!':
    case '>':
    case '<':
    return true;
    default: 
    return false;
  }
  
}



}