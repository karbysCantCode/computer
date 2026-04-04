#include "Spasm/Lexer.hpp"

#include <cassert>

namespace Spasm {


TokenHolder SpasmLexer::run(const std::string& source, const std::filesystem::path& path) {
  //keywords are not used because otherwise they'd have to be
  //  matched for the keyword detection, and linking to an
  //  architecture object.

  p_source = source;

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
      case ';':
      //comments
      {
        if (peek(1) == '*') {
          consume();
          consume();
          while (notAtEnd() && !(match('*') && match(';', 1))) consume();
          consume();
          consume();
        } else {
          while (notAtEnd() && !match('\n')) {consume();}
        }
        break;
      }
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
        while (!match('"') && notAtEnd()) {
          consume();
        }
        const size_t length = p_index - sliceStartIndex;
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr, length}, 
          Token::Type::STRING, 
          sliceStartLocation
        );
        consume();

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
        setSliceStart;
        while (!isAtWordBoundary()) {
          consume();
        }
        const size_t length = p_index - sliceStartIndex;
        const std::string_view directive{sliceStartPtr, length};
        Token::NicheType nt = Token::NicheType::UNASSIGNED;
        if (directive == "include") {
          nt = Token::NicheType::DIRECTIVE_INCLUDE;
        } else if (directive == "define") {
          nt = Token::NicheType::DIRECTIVE_DEFINE;
        } else if (directive == "entry") {
          nt = Token::NicheType::DIRECTIVE_ENTRY;
        }
        output.m_tokens.emplace_back(
          directive, 
          Token::Type::DIRECTIVE, 
          sliceStartLocation,
          nt
        );
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
          Token::Type::RIGHTSHIFT, 
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
        // if (isdigit(peek(1))) {
        //   const auto nt = getNicheTypeAndSetSliceOverNumber();
        //   const size_t length = p_index - sliceStartIndex;
        //   output.m_tokens.emplace_back(
        //     std::string_view{sliceStartPtr,length}, 
        //     Token::Type::NUMBER,
        //     sliceStartLocation,
        //     nt
        //   );
        // } else {
          consume();
          output.m_tokens.emplace_back(
            std::string_view{sliceStartPtr,1}, 
            Token::Type::SUBTRACT, 
            sliceStartLocation
          );
        // }

        // commented out because switch to ast parsing of numbers
        
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
      default: {
        if (isdigit(peek())) {
          const auto nt = getNicheTypeAndSetSliceOverNumber();
          const size_t length = p_index - sliceStartIndex;
          output.m_tokens.emplace_back(
            std::string_view{sliceStartPtr, length}, 
            Token::Type::NUMBER, 
            sliceStartLocation,
            nt
          );
        } else {
          while (!isAtWordBoundary() && notAtEnd()) {
            consume();
          }
          const size_t length = p_index - sliceStartIndex;
          output.m_tokens.emplace_back(
            std::string_view{sliceStartPtr, length}, 
            Token::Type::IDENTIFIER, 
            sliceStartLocation
          );
        }
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
    case '\0':
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

void SpasmLexer::consumeUntilNotNumber() {
  while (isdigit(peek()) && notAtEnd()) {consume();}
}

Token::NicheType SpasmLexer::getNicheTypeAndSetSliceOverNumber() {
  Token::NicheType type = Token::NicheType::UNASSIGNED;
  
  if (match('0')) {
    consume();
    if (match('x')) {
      type = Token::NicheType::NUMBER_HEX;
      consume();
      consumeUntilNotNumber();
    } else if (match('b')) {
      type = Token::NicheType::NUMBER_BIN;
      consume();
      consumeUntilNotNumber();
    } else if (isdigit(peek()) || isAtWordBoundary()) {
      type = Token::NicheType::NUMBER_DEC;
      consumeUntilNotNumber();
    } else {
      //err
      assert(false);

    }
  } else if (isdigit(peek())) {
    type = Token::NicheType::NUMBER_DEC;
    consumeUntilNotNumber();
  }

  return type;
}

}