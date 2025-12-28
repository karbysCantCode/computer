  #pragma once
  #include <string>

namespace ArchToken {
  enum class ArchTokenTypes {
    KEYWORD,
    ARGUMENTTYPE,
    IDENTIFIER,
    NEWLINE,
    UNASSIGNED
  };

  constexpr const char* toString(ArchTokenTypes t) {
    switch (t) {
      case ArchTokenTypes::KEYWORD:    return "KEYWORD";
      case ArchTokenTypes::IDENTIFIER:    return "IDENTIFIER";
      case ArchTokenTypes::ARGUMENTTYPE:     return "ARGUMENTTYPE";
      case ArchTokenTypes::UNASSIGNED:     return "UNASSIGNED";
      case ArchTokenTypes::NEWLINE:     return "NEWLINE";
      default:                    return "UNKNOWN";
    }
  }

  struct ArchToken {
    std::string value;
    ArchTokenTypes type = ArchTokenTypes::UNASSIGNED;
    int line = -1;
    int column = -1;

    inline void SetString(const std::string& newString) {value=newString;}
    inline void SetType(const ArchTokenTypes& newType) {type=newType;}
    void print(size_t padding) const;
    std::string positionToString() const {return "line " + std::to_string(line) + ", column " + std::to_string(column);}

    ArchToken(
        std::string val = {},
        ArchTokenTypes t = ArchTokenTypes::UNASSIGNED,
        int ln = -1,
        int col = -1
    )
        : value(std::move(val)), type(t), line(ln), column(col) {}
  };
}
namespace Token {

  enum class TokenTypes {
    IDENTIFIER,
    KEYWORD,
    OPENPAREN,
    CLOSEPAREN,
    STRING,
    NUMBER,
    COMMA,
    OPENBLOCK,
    CLOSEBLOCK,
    OPENSQUARE,
    CLOSESQUARE,
    UNASSIGNED,
    DIRECTIVE
  };

  constexpr const char* toString(TokenTypes t) {
    switch (t) {
        case TokenTypes::IDENTIFIER: return "IDENTIFIER";
        case TokenTypes::KEYWORD:    return "KEYWORD";
        case TokenTypes::OPENPAREN:  return "OPENPAREN";
        case TokenTypes::CLOSEPAREN: return "CLOSEPAREN";
        case TokenTypes::STRING:     return "STRING";
        case TokenTypes::NUMBER:     return "NUMBER";
        case TokenTypes::COMMA:     return "COMMA";
        case TokenTypes::OPENBLOCK:     return "OPENBLOCK";
        case TokenTypes::CLOSEBLOCK:     return "CLOSEBLOCK";
        case TokenTypes::OPENSQUARE:     return "OPENSQUARE";
        case TokenTypes::CLOSESQUARE:     return "CLOSESQUARE";
        case TokenTypes::UNASSIGNED:     return "UNASSIGNED";
        case TokenTypes::DIRECTIVE:     return "DIRECTIVE";
        default:                    return "UNKNOWN";
    }
  }

  struct Token {
    std::string value;
    TokenTypes type = TokenTypes::UNASSIGNED;
    int line = -1;
    int column = -1;

    inline void SetString(const std::string& newString) {value=newString;}
    inline void SetType(const TokenTypes& newType) {type=newType;}
    void print(size_t padding) const;
    std::string positionToString() const {return "line " + std::to_string(line) + ", column " + std::to_string(column);}

    Token(
        std::string val = {},
        TokenTypes t = TokenTypes::UNASSIGNED,
        int ln = -1,
        int col = -1
    )
        : value(std::move(val)), type(t), line(ln), column(col) {}

    Token(TokenTypes t)
    : type(t) {}

    Token(TokenTypes t, int ln, int col)
    : type(t), line(ln), column(col) {}
  };
}