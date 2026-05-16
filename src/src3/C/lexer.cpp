#include "lexer.hpp"

namespace C {

TokenHolder CLexer::run() {
  reset();
  TokenHolder holder;
  holder.reset();
  size_t sliceStart = 0;
  SourceLocation sliceLoc(p_path, 1,1);
  sliceLoc.column = 1;
  sliceLoc.line = 1;
  #define SETSLICEHERE           \
    sliceStart = p_index;        \
    sliceLoc.column = p_column;  \
    sliceLoc.line = p_line;

  #define SUBMITTOKEN(TYPE)    \
    holder.m_tokens.emplace_back(      \
      sliceLoc,                        \
      TYPE,                            \
      std::string_view(p_source.data() + sliceStart, p_index - sliceStart) \
    );                                 

  #define SUBMITTOKENANDRESET(TYPE)    \
    SUBMITTOKEN(TYPE)                  \
    SETSLICEHERE

  while (p_index < p_source.size()) {
    while (isWhitespace()) consume();
    SETSLICEHERE;
    const char c = consume();
    switch (c)
    {
    case '*':
      switch (peek()) {
        case '=':
          consume();
          SUBMITTOKENANDRESET(Token::Type::AS_MUL);
          break;
        default:
          SUBMITTOKENANDRESET(Token::Type::MU_ASTRIX);
          break;
      }
      break;
    case '/':
      switch (peek()) {
        case '/':
          while (consume() != '\n' && notAtEnd());
          consume();
          SETSLICEHERE;
          break;
        case '*':
          consume();
          consume();
          while ((!(peek() == '*' && peek(1) == '/')) && notAtEnd()) consume();
          consume();
          consume();
          SETSLICEHERE;
          break;
        case '=':
          consume();
          SUBMITTOKENANDRESET(Token::Type::AS_DIV);
          break;
        default:
          SUBMITTOKENANDRESET(Token::Type::OP_DIV);
          break;
      }
      break;
    case '+':
      switch (peek()) {
        case '+':
          consume();
          SUBMITTOKENANDRESET(Token::Type::OP_INC);
          break;
        case '=':
          consume();
          SUBMITTOKENANDRESET(Token::Type::AS_ADD);
          break;
        default:
          SUBMITTOKENANDRESET(Token::Type::OP_ADD);
          break;
      }
      break;
    case '-':
      switch (peek()) {
        case '-':
          consume();
          SUBMITTOKENANDRESET(Token::Type::OP_DEC);
          break;
        case '=':
          consume();
          SUBMITTOKENANDRESET(Token::Type::AS_SUB);
          break;
        case '>':
          consume();
          SUBMITTOKENANDRESET(Token::Type::MA_POINTER);
          break;
        default:
          SUBMITTOKENANDRESET(Token::Type::OP_SUB);
          break;
      }
      break;
    case '%':
      if (peek() == '=') {
        consume();
        SUBMITTOKENANDRESET(Token::Type::AS_REM);
      } else {
        SUBMITTOKENANDRESET(Token::Type::OP_REM);
      }
      break;
    case '!':
      if (peek() == '=') {
        consume();
        SUBMITTOKENANDRESET(Token::Type::OP_NOTEQUAL);
      } else {
        SUBMITTOKENANDRESET(Token::Type::OP_NOT);
      }
      break;
    case '|': 
      switch (peek()) {
        case '|':
          consume();
          SUBMITTOKENANDRESET(Token::Type::OP_COMPARISONOR);
          break;
        case '=':
          consume();
          SUBMITTOKENANDRESET(Token::Type::AS_OR);
          break;
        default:
          SUBMITTOKENANDRESET(Token::Type::OP_OR);
          break;
      }
      break;
    case '^':
      if (peek() == '=') {
        consume();
        SUBMITTOKENANDRESET(Token::Type::AS_XOR);
      } else {
        SUBMITTOKENANDRESET(Token::Type::OP_XOR);
      }
      break;
    case '&':
      switch (peek()) {
        case '&':
          consume();
          SUBMITTOKENANDRESET(Token::Type::OP_COMPARISONAND);
          break;
        case '=':
          consume();
          SUBMITTOKENANDRESET(Token::Type::AS_AND);
          break;
        default:
          SUBMITTOKENANDRESET(Token::Type::MU_AND);
          break;
      }
      break;
    case '<':
      switch (peek()) {
        case '<':
          consume();
          if (peek() == '=') {
            consume();
            SUBMITTOKENANDRESET(Token::Type::AS_SHL);
          } else {
            SUBMITTOKENANDRESET(Token::Type::OP_SHL);
          }
          break;
        case '=':
          consume();
          SUBMITTOKENANDRESET(Token::Type::OP_LESSTHANOREQUAL);
          break;
        default:
          SUBMITTOKENANDRESET(Token::Type::OP_LESSTHAN);
          break;
      }
      break;
    case '>':
      switch (peek()) {
        case '>':
          consume();
          if (peek() == '=') {
            consume();
            SUBMITTOKENANDRESET(Token::Type::AS_SHR);
          } else {
            SUBMITTOKENANDRESET(Token::Type::OP_SHR);
          }
          break;
        case '=':
          consume();
          SUBMITTOKENANDRESET(Token::Type::OP_GREATERTHANOREQUAL);
          break;
        default:
          SUBMITTOKENANDRESET(Token::Type::OP_GREATERTHAN);
          break;
      }
      break;
    case '=':
      if (peek() == '=') {
          consume();
          SUBMITTOKENANDRESET(Token::Type::OP_EQUAL);
        } else {
          SUBMITTOKENANDRESET(Token::Type::AS_BASIC);
        }
      break;
    case '[':
      SUBMITTOKENANDRESET(Token::Type::MA_OPENSQUARE);
      break;
    case ']':
      SUBMITTOKENANDRESET(Token::Type::MA_CLOSESQUARE);
      break;
    case '.':
      SUBMITTOKENANDRESET(Token::Type::MA_PERIOD);
      break;
    case '#': {
      SETSLICEHERE
      while (!isAtWordBoundary()) consume();
      const std::string_view str(p_source.data() + sliceStart, p_index - sliceStart);
      const auto it = ppKeywords.find(str);
      if (it != ppKeywords.end()) {
        SUBMITTOKENANDRESET(it->second);
      } else {
        SUBMITTOKENANDRESET(Token::Type::PP_UNRECOGNISED);
      }
      break;
    }
    case '?':
      SUBMITTOKENANDRESET(Token::Type::OT_QUESTION);
      break;
    case ',':
      SUBMITTOKENANDRESET(Token::Type::OT_COMMA);
      break;
    case ':':
      SUBMITTOKENANDRESET(Token::Type::OT_COLON);
      break;
    case ';':
      SUBMITTOKENANDRESET(Token::Type::OT_SEMICOLON);
      break;
    case '(':
      SUBMITTOKENANDRESET(Token::Type::OT_OPENPAREN);
      break;
    case ')':
      SUBMITTOKENANDRESET(Token::Type::OT_CLOSEPAREN);
      break;
    case '{':
      SUBMITTOKENANDRESET(Token::Type::OT_OPENBLOCK);
      break;
    case '}':
      SUBMITTOKENANDRESET(Token::Type::OT_CLOSEBLOCK);
      break;
    case '\\':
      SUBMITTOKENANDRESET(Token::Type::OT_BACKSLASH);
      break;
    case '\n':
      SUBMITTOKENANDRESET(Token::Type::NEWLINE);
      break;
    case '"':
      consumeString('"');
      SUBMITTOKEN(Token::Type::STRING);
      consume();
      SETSLICEHERE;
      break;
    case '\'':
      consumeString('\'');
      SUBMITTOKEN(Token::Type::CSTRING);
      consume();
      SETSLICEHERE;
      break;
    default:
      if (c == '0') {
        switch (peek())
        {
        case 'x':
          consume();
          while ((std::isxdigit(peek()) || match('\'')) && notAtEnd()) consume();
          SUBMITTOKENANDRESET(Token::Type::NM_HEX);
          break;
        
        case 'b':
          consume();
          while ((peek() == '0' || peek() == '1' || match('\'')) && notAtEnd()) consume();
          SUBMITTOKENANDRESET(Token::Type::NM_BIN);
          break;
        default:
          while (std::isdigit(peek()) && notAtEnd()) consume();
          SUBMITTOKENANDRESET(Token::Type::NM_DEC);
          break;
        }
      } else if (std::isdigit(c)) {
        while ((std::isdigit(peek())) && notAtEnd()) consume();
        SUBMITTOKENANDRESET(Token::Type::NM_DEC);
      } else if (std::isalpha(c) || (c >= '_')) {
        while ((std::isalnum(peek()) || (c == '_')) && notAtEnd()) consume();
        SUBMITTOKENANDRESET(Token::Type::IDENTIFIER);
      }

      break;
    }
  }
  #undef SETSLICEHERE
  #undef SUBMITTOKEN
  #undef SUBMITTOKENANDRESET  

  return holder;
}

bool CLexer::isWhitespace() {
  switch (peek()) {
    case '\t':
    case '\v':
    //case '\n':
    case '\f':
    case '\r':
    case ' ':
      std::cout << "TRUEEWHILESPACE:'" << peek() << "'" << '\n';
      return true;
    default:
      std::cout << "FALSEWHILESPACE:'" << peek() << "'" << '\n';
      return false;
  }
  // return std::isspace(peek());
}
bool CLexer::isAtWordBoundary() {
  return !std::isalnum(peek());
}
void CLexer::consumeUntilNotNumber() {
  while (!std::isdigit(peek()) && notAtEnd()) consume();
}
void CLexer::consumeUntilNotHex() {
  while (!std::isxdigit(peek()) && notAtEnd()) consume();
}

const std::unordered_map<std::string_view, Token::Type> CLexer::ppKeywords = {
  {"define",  Token::Type::PP_DEFINE},
  {"elif",    Token::Type::PP_ELIF},
  {"else",    Token::Type::PP_ELSE},
  {"endif",   Token::Type::PP_ENDIF},
  {"if",      Token::Type::PP_IF},
  {"ifdef",   Token::Type::PP_IFDEF},
  {"ifndef",  Token::Type::PP_IFNDEF},
  {"undef",  Token::Type::PP_UNDEF},
  {"include", Token::Type::PP_INCLUDE},
  {"line",    Token::Type::PP_LINE},
  {"pragma",  Token::Type::PP_PRAGMA},
};

const std::unordered_map<std::string_view, Token::Type> CLexer::keywords = {
  {"alignas",        Token::Type::KW_AS_ALIGNAS},
  {"alignof",        Token::Type::KW_ALIGNOF},
  {"auto",           Token::Type::KW_SCS_AUTO},
  {"bool",           Token::Type::KW_TY_BOOL},
  {"break",          Token::Type::KW_BREAK},
  {"case",           Token::Type::KW_CASE},
  {"char",           Token::Type::KW_TY_CHAR},
  {"const",          Token::Type::KW_TQ_CONST},
  //{"constexpr",      Token::Type::KW_CONSTEXPR}, //from c23 ie unsupported
  {"continue",       Token::Type::KW_CONTINUE},
  {"default",        Token::Type::KW_DEFAULT},
  {"do",             Token::Type::KW_DO},
  {"double",         Token::Type::KW_TY_DOUBLE},
  {"else",           Token::Type::KW_ELSE},
  {"enum",           Token::Type::KW_TY_ENUM},
  {"extern",         Token::Type::KW_SCS_EXTERN},
  {"false",          Token::Type::KW_FALSE},
  {"float",          Token::Type::KW_FLOAT},
  {"for",            Token::Type::KW_FOR},
  {"fortran",        Token::Type::KW_FORTRAN}, // unsupported
  {"goto",           Token::Type::KW_GOTO},
  {"if",             Token::Type::KW_IF},
  {"inline",         Token::Type::KW_FS_INLINE},
  {"int",            Token::Type::KW_TY_INT},
  {"long",           Token::Type::KW_TY_LONG},
  {"nullptr",        Token::Type::KW_NULLPTR},
  {"register",       Token::Type::KW_SCS_REGISTER},
  {"restrict",       Token::Type::KW_TQ_RESTRICT},
  {"return",         Token::Type::KW_RETURN},
  {"short",          Token::Type::KW_TY_SHORT},
  {"signed",         Token::Type::KW_TY_SIGNED},
  {"sizeof",         Token::Type::KW_SIZEOF},
  {"static",         Token::Type::KW_SCS_STATIC},
  {"static_assert",  Token::Type::KW_STATIC_ASSERT},
  {"struct",         Token::Type::KW_TY_STRUCT},
  {"switch",         Token::Type::KW_SWITCH},
  {"thread_local",   Token::Type::KW_SCS_THREAD_LOCAL},
  {"true",           Token::Type::KW_TRUE},
  {"typedef",        Token::Type::KW_SCS_TYPEDEF},
  {"typeof",         Token::Type::KW_TYPEOF},
  {"typeof_unqual",  Token::Type::KW_TYPEOF_UNQUAL},
  {"union",          Token::Type::KW_TY_UNION},
  {"unsigned",       Token::Type::KW_TY_UNSIGNED},
  {"void",           Token::Type::KW_TY_VOID},
  {"volatile",       Token::Type::KW_TQ_VOLATILE},
  {"while",          Token::Type::KW_WHILE},
};
}