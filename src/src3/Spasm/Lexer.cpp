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
        while (!match('"') && notAtEnd()) {
          consume();
        }
        consume();
        const size_t length = p_index - sliceStartIndex;
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr, length}, 
          Token::Type::STRING, 
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
      default: {
        int number = 0;
        if (isdigit(peek()) || match('-')) {
          //number
          //zero prefixed number
          Token::NicheType nt = Token::NicheType::UNASSIGNED;
          if (peek() == '0') {
            consume();
            if (match('x')) {
              nt = Token::NicheType::NUMBER_HEX;
              consume();
              setSliceStart;
              number = consumeNumber(16, sliceStartIndex);
            } else if (match('b')) {
              nt = Token::NicheType::NUMBER_BIN;
              consume();
              setSliceStart;
              number = consumeNumber(2, sliceStartIndex);
            } else if (isdigit(peek()) || !isAtWordBoundary()) {
              nt = Token::NicheType::NUMBER_DEC;
              //continue with normal consumption, number just starts with zero
              number = consumeNumber(10, sliceStartIndex);
            } else {

              //error
            }
          } else {
            number = consumeNumber(10, sliceStartIndex);
          }
        }
        if (peek)
        consume();
        setSliceStart;
        while (!match('"') && notAtEnd()) {
          consume();
        }
        consume();
        const size_t length = p_index - sliceStartIndex;
        output.m_tokens.emplace_back(
          std::string_view{sliceStartPtr, length}, 
          Token::Type::STRING, 
          sliceStartLocation
        );

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

int SpasmLexer::consumeNumber(int base, size_t startIndex) {
  while (isdigit(peek()) && notAtEnd()) {consume();}
  return std::stoi(p_source.substr(startIndex, p_index - startIndex), nullptr, base);
}

Token::NicheType SpasmLexer::getNicheTypeAndSetSliceOverNumber() {
  Token::NicheType type = Token::NicheType::UNASSIGNED;
  ?????????????????????
  if (match('-'))

  return type;
}

}