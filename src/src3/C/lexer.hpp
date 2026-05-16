#pragma once
#include "Helpers/TokenBase.hpp"
#include "Helpers/LexerBase.hpp"
#include "Helpers/Debug.hpp"

namespace C {

struct Token : TokenBase {
  enum class Type {
    KW_BEGIN,
    //KW_ALIGNAS,
    KW_ALIGNOF = KW_BEGIN,
    //KW_AUTO,
    //KW_BOOL,
    KW_BREAK,
    KW_CASE,
    //KW_CHAR,
    //KW_CONST,
    //KW_CONSTEXPR, //unsopported
    KW_CONTINUE,
    KW_DEFAULT,
    KW_DO,
    //KW_DOUBLE,
    KW_ELSE,
    //KW_ENUM,
    //KW_EXTERN,
    KW_FALSE,
    KW_FLOAT,
    KW_FOR,
    KW_FORTRAN, // unsupported
    KW_GOTO,
    KW_IF,
    //KW_INLINE,
    //KW_INT,
    //KW_LONG,
    KW_NULLPTR,
    //KW_REGISTER,
    //KW_RESTRICT,
    KW_RETURN,
    //KW_SHORT,
    //KW_SIGNED,
    KW_SIZEOF,
    //KW_STATIC,
    KW_STATIC_ASSERT,
    // KW_STRUCT,
    KW_SWITCH,
    //KW_THREAD_LOCAL,
    KW_TRUE,
    //KW_TYPEDEF,
    KW_TYPEOF,
    KW_TYPEOF_UNQUAL,
    // KW_UNION,
    //KW_UNSIGNED,
    //KW_VOID,
    //KW_VOLATILE,
    KW_WHILE,
    KW_DS_BEGIN,
    KW_AS_BEGIN = KW_DS_BEGIN,
    KW_AS_ALIGNAS = KW_AS_BEGIN,
    KW_AS_END,
    KW_FS_BEGIN,
    KW_FS_INLINE = KW_FS_BEGIN,
    KW_FS_END,
    KW_TQ_BEGIN,
    KW_TQ_CONST = KW_TQ_BEGIN,
    KW_TQ_RESTRICT, // unsupported
    KW_TQ_VOLATILE,
    KW_TQ_END,
    KW_TY_BEGIN,
    KW_TY_VOID = KW_TY_BEGIN,
    KW_TY_CHAR,
    KW_TY_INT,
    KW_TY_FLOAT,
    KW_TY_DOUBLE,
    KW_TY_MULTI_BEGIN,
    KW_TY_LONG = KW_TY_MULTI_BEGIN,
    KW_TY_SHORT,
    KW_TY_SIGNED,
    KW_TY_UNSIGNED,
    KW_TY_MULTI_END,
    KW_TY_BOOL,
    KW_TY_STRUCT,
    KW_TY_UNION,
    KW_TY_ENUM,
    KW_TY_END,
    KW_SCS_BEGIN,
    KW_SCS_AUTO = KW_SCS_BEGIN,
    KW_SCS_EXTERN,
    KW_SCS_REGISTER,
    KW_SCS_STATIC,
    KW_SCS_THREAD_LOCAL,
    KW_SCS_TYPEDEF,
    KW_SCS_END,
    KW_DS_END,
    KW_END,

    // arithmetic + comparison operators

    OP_BEGIN,
    OP_ADD = OP_BEGIN,
    OP_SUB,
    //OP_MUL,  multiuse
    OP_DIV,
    OP_REM,
    OP_NOT,
    //OP_AND,  multiuse
    OP_OR,
    OP_XOR,
    OP_SHL,
    OP_SHR,
    //OP_REF, uhhhh idk
    //OP_PTR, uhhhh idk
    OP_LESSTHAN,
    OP_LESSTHANOREQUAL,
    OP_GREATERTHAN,
    OP_GREATERTHANOREQUAL,
    OP_EQUAL,
    OP_NOTEQUAL,
    OP_COMPARISONAND,
    OP_COMPARISONOR,
    OP_INC,
    OP_DEC,
    OP_END,

    //assignment operators

    AS_BEGIN,
    AS_BASIC = AS_BEGIN,
    AS_ADD,
    AS_SUB,
    AS_MUL,
    AS_DIV,
    AS_REM,
    AS_AND,
    AS_OR,
    AS_XOR,
    AS_SHL,
    AS_SHR,
    AS_END,

    //member access

    MA_BEGIN,
    MA_OPENSQUARE = MA_BEGIN,
    MA_CLOSESQUARE,
    MA_PERIOD,
    MA_POINTER,

    MA_END,

    //preprocessor directive

    PP_BEGIN,
    PP_DEFINE = PP_BEGIN,
    PP_ELIF,
    PP_ELSE,
    PP_ENDIF,
    PP_IF,
    PP_IFDEF,
    PP_UNDEF,
    PP_IFNDEF,
    PP_INCLUDE,
    PP_LINE,
    PP_PRAGMA,
    PP_UNRECOGNISED,
    PP_END,

    //misc

    OT_BEGIN,
    OT_QUESTION = OT_BEGIN,
    OT_COLON,
    OT_SEMICOLON,
    OT_COMMA,
    OT_OPENPAREN,
    OT_CLOSEPAREN,
    OT_OPENBLOCK,
    OT_CLOSEBLOCK,
    OT_BACKSLASH,
    OT_END,

    //multi use
    MU_BEGIN,
    MU_ASTRIX = MU_BEGIN,
    MU_AND,
    MU_END,

    // literals
    STRING,
    CSTRING,
    NM_BEGIN,
    NM_DEC = NM_BEGIN,
    NM_HEX,
    NM_BIN,
    NM_END,

    IDENTIFIER,
    NEWLINE,
    INVALID

  };

  enum class Category {
    KEYWORD,
    OPERATOR,
    ASSIGNMENT,
    MEMBERACCESS,
    PREPROCESSOR,
    OTHER,
    MULTIUSE
  };

  Type type;

  Token(const SourceLocation& loc, const Type t, const std::string_view& val) : TokenBase(val, loc), type(t) {}
  Token() : TokenBase({}, {"NULL",0,0}) {}
  inline bool isKeyword() const {
    return type >= Type::KW_BEGIN && type < Type::KW_END;
  }
  inline bool isOperator() const {
    return type >= Type::OP_BEGIN && type < Type::OP_END;
  }
  inline bool isAssignment() const {
    return type >= Type::AS_BEGIN && type < Type::AS_END;
  }
  inline bool isMemberAccess() const {
    return type >= Type::MA_BEGIN && type < Type::MA_END;
  }
  inline bool isPreprocessor() const {
    return type >= Type::PP_BEGIN && type < Type::PP_END;
  }
  inline bool isOther() const {
    return type >= Type::OT_BEGIN && type < Type::OT_END;
  }
  inline bool isMultiUse() const {
    return type >= Type::MU_BEGIN && type < Type::MU_END;
  }
  inline bool isNumber() const {
    return type >= Type::NM_BEGIN && type < Type::NM_END;
  }
  inline bool isKWSCS() const {
    return type >= Type::KW_SCS_BEGIN && type < Type::KW_SCS_END;
  }
  inline bool isKWTY() const {
    return type >= Type::KW_TY_BEGIN && type < Type::KW_TY_END;
  }
  inline bool isKWTQ() const {
    return type >= Type::KW_TQ_BEGIN && type < Type::KW_SCS_END;
  }
  inline bool isKWDS() const {
    return type >= Type::KW_DS_BEGIN && type < Type::KW_DS_END;
  }
  inline bool isKWTYMULTI() const {
    return type >= Type::KW_TY_MULTI_BEGIN && type < Type::KW_TY_MULTI_END;
  }
  inline bool isKWTYMULTI(Token::Type t) const {
    return t >= Type::KW_TY_MULTI_BEGIN && t < Type::KW_TY_MULTI_END;
  }
  inline bool isIdentifier() const {
    return type == Type::IDENTIFIER;
  }

  inline bool isDeclaratorToken() const {
    switch (type) {
      case Type::MA_OPENSQUARE:
      case Type::MA_CLOSESQUARE:
      case Type::IDENTIFIER:
      case Type::MU_ASTRIX:
      case Type::KW_TQ_RESTRICT:
      case Type::KW_TQ_CONST:
      case Type::KW_TQ_VOLATILE:
      case Type::OT_OPENPAREN:
      case Type::OT_CLOSEPAREN:
      return true;
      default:
      return false;
    }
  }
};

struct TokenHolder : TokenBaseHolder<Token> {
  inline bool matchCategory(Token::Category category, int distance = 0) {
    switch (category) {
      case Token::Category::ASSIGNMENT:
      return peek(distance).isAssignment();

      case Token::Category::KEYWORD:
      return peek(distance).isKeyword();

      case Token::Category::OPERATOR:
      return peek(distance).isOperator();

      case Token::Category::MEMBERACCESS:
      return peek(distance).isMemberAccess();

      case Token::Category::MULTIUSE:
      return peek(distance).isMultiUse();

      case Token::Category::OTHER:
      return peek(distance).isOther();

      case Token::Category::PREPROCESSOR:
      return peek(distance).isPreprocessor();

    }
  };
  inline bool match(Token::Type type, int distance = 0) {
    return peek(distance).type == type;
  }
  inline bool match(std::initializer_list<Token::Type> list, int distance = 0) {
    const auto& token = peek(distance);
    for (const auto type : list) {
      if (type == token.type) return true;
    }
    return false;
  }
  inline void skipUntilAfterCategory(Token::Category category) {while (!matchCategory(category)) {skip();} skip();}
  inline void skipUntilAfterType(Token::Type type, std::vector<Token*>& out, bool useDepth = false) {
    if (useDepth) {



      
      int parenDepth = 0;
      int blockDepth = 0;
      int squareDepth = 0;
      bool exit = false;
      while (!exit && notAtEnd() && parenDepth >= 0 && blockDepth >= 0 && squareDepth >= 0) {
        Token::Type t = peek().type;
        switch (t)
        {
        case Token::Type::OT_OPENPAREN:
          parenDepth++;
          break;
        case Token::Type::OT_CLOSEPAREN:
          parenDepth--;
          break;
        case Token::Type::OT_OPENBLOCK:
          blockDepth++;
          break;
        case Token::Type::OT_CLOSEBLOCK:
          blockDepth--;
          break;
        case Token::Type::MA_OPENSQUARE:
          squareDepth++;
          break;
        case Token::Type::MA_CLOSESQUARE:
          squareDepth--;
          break;
        
        default:
          break;
        }
        if (t == type 
          && parenDepth <= 0
          && blockDepth <= 0
          && squareDepth <= 0
          ) {
          exit = true;
          skip();
        } else {
          out.push_back(&consume());
        }
      }
    } else {
      while (!match(type) && notAtEnd()) {
        skip();
      }
      skip();
    } 
  }
  inline void skipUntilAfterType(Token::Type type, bool useDepth = false) {
    if (useDepth) {
      
      int parenDepth = 0;
      int blockDepth = 0;
      int squareDepth = 0;
      bool exit = false;
      while (!exit && notAtEnd() && parenDepth >= 0 && blockDepth >= 0 && squareDepth >= 0) {
        Token::Type t = peek().type;
        switch (t)
        {
        case Token::Type::OT_OPENPAREN:
          parenDepth++;
          break;
        case Token::Type::OT_CLOSEPAREN:
          parenDepth--;
          break;
        case Token::Type::OT_OPENBLOCK:
          blockDepth++;
          break;
        case Token::Type::OT_CLOSEBLOCK:
          blockDepth--;
          break;
        case Token::Type::MA_OPENSQUARE:
          squareDepth++;
          break;
        case Token::Type::MA_CLOSESQUARE:
          squareDepth--;
          break;
        
        default:
          break;
        }
        if (t == type 
          && parenDepth <= 0
          && blockDepth <= 0
          && squareDepth <= 0
          ) {
          exit = true;
        }
        skip();
      }
    } else {
      while (!match(type) && notAtEnd()) {
        skip();
      }
    } 
    skip();
}
  inline void skipUntilAfterType(std::initializer_list<Token::Type> list, bool useDepth = false) {
    if (useDepth) {
      int parenDepth = 0;
      int blockDepth = 0;
      int squareDepth = 0;
      bool exit = false;
      while (!exit && notAtEnd() && parenDepth >= 0 && blockDepth >= 0 && squareDepth >= 0) {
        Token::Type t = peek().type;
        switch (t)
        {
        case Token::Type::OT_OPENPAREN:
          parenDepth++;
          break;
        case Token::Type::OT_CLOSEPAREN:
          parenDepth--;
          break;
        case Token::Type::OT_OPENBLOCK:
          blockDepth++;
          break;
        case Token::Type::OT_CLOSEBLOCK:
          blockDepth--;
          break;
        case Token::Type::MA_OPENSQUARE:
          squareDepth++;
          break;
        case Token::Type::MA_CLOSESQUARE:
          squareDepth--;
          break;
        
        default:
          break;
        }
        if (std::find(list.begin(), list.end(), t) != list.end()
          && parenDepth <= 0
          && blockDepth <= 0
          && squareDepth <= 0
          ) {
          exit = true;
        }
        skip();
      }
    } else {
      while (notAtEnd()) {
        const auto& token = peek();
        if (std::find(list.begin(), list.end(), token.type) != list.end()) break;
        skip();
      } 
    }
    skip();
  }

};

class CLexer : LexerBase {
public:
  //TokenHolder run(std::string& source, Debug::FullLogger* logger = nullptr);
  TokenHolder run();
  CLexer(std::string& source, std::filesystem::path& path) : LexerBase(source, path) {}
private:
  // Debug::FullLogger* p_logger;

  bool isWhitespace();
  bool isAtWordBoundary();
  void consumeUntilNotNumber();
  void consumeUntilNotHex();

  static const std::unordered_map<std::string_view, Token::Type> ppKeywords;
  static const std::unordered_map<std::string_view, Token::Type> keywords;

};
}