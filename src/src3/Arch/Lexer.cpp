#include "Arch/Lexer.hpp"

Arch::TokenHolder Arch::ArchLexer::run(const std::string& source, const std::filesystem::path& sourcePath) {
  p_source = source;
  
  TokenHolder output;

  size_t sliceStartIndex = p_index;
  SourceLocation sliceStartLocation(sourcePath, p_line, p_column);
  Token::Type sliceType = Token::Type::UNASSIGNED;

  #define sliceStartPtr p_source.data() + sliceStartIndex

  while (notAtEnd()) {
    sliceStartIndex = p_index;
    sliceStartLocation.line = p_line;
    sliceStartLocation.column = p_column;
    const char c = peek();

    switch (c) {
      case ';': {
      if (match('*', 1)) {
        while (notAtEnd() && !match('*') && !match(';', 1)) {
          if (match('\n')) {
            output.m_tokens.emplace_back(
              std::string_view{p_source.data() + p_index, 1},
              Token::Type::NEWLINE, 
              SourceLocation(sourcePath, p_line, p_column)
            );
          }
          consume();
        }
        consume(); // eat *
        consume(); // eat ;
      } else {
        while (notAtEnd() && !match('\n')) {
          consume();
        }
      }
      }
      break;
      case '.': {

      //keyword
      sliceType = Token::Type::KEYWORD;
      consume();
      while (notAtEnd() && !isAtWordBoundary()) {
        consume();
      }

      const size_t length = p_index - sliceStartIndex;
      output.m_tokens.emplace_back(
        std::string_view{sliceStartPtr + 1, length}, //+1 exclude dot.
        sliceType, 
        sliceStartLocation
      );
      }
      break;




      case '\n':
      {
      //newline
      sliceType = Token::Type::NEWLINE;
      consume();
      const size_t length = p_index - sliceStartIndex;
      output.m_tokens.emplace_back(
        std::string_view{sliceStartPtr + 1, length}, //+1 exclude dot.
        sliceType, 
        sliceStartLocation
      );
      }
      break;





      default:
      sliceType = Token::Type::IDENTIFIER;
      while (notAtEnd() && !isAtWordBoundary()) {
        consume();
      }

      const size_t length = p_index - sliceStartIndex;
      const std::string_view currentSlice(sliceStartPtr, length);
      if (currentSlice == "REG"
        ||currentSlice == "NON"
        ||currentSlice == "IMM") 
      {
        sliceType = Token::Type::ARGUMENTTYPE;
      }
      output.m_tokens.emplace_back(
        currentSlice,
        sliceType, 
        sliceStartLocation
      );
      break;
    }
  }

  #undef sliceStartPtr

  return output;
}

bool Arch::ArchLexer::isAtWordBoundary() {
  switch (peek()) {
    case ' ':
    case '.':
    case '\n':
    case '\0':
    case '\t':
    return true;
    default:
    return false;
  }
}