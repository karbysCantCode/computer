#pragma once

#include "Helpers/LexerBase.hpp"
#include "Helpers/TokenBase.hpp"
#include "Helpers/Debug.hpp"
#include <filesystem>

namespace Spasm {
  struct Token : TokenBase {
    enum class Type {
      IDENTIFIER,
      OPENPAREN, //y
      CLOSEPAREN, //y
      STRING, //y
      NUMBER,
      COMMA, //y
      COLON, //y
      PERIOD, //y
      OPENBLOCK, //y
      CLOSEBLOCK, //y 
      OPENSQUARE,//y 
      CLOSESQUARE,//y
      UNASSIGNED,
      DIRECTIVE, //y

      BITWISEOR, //y
      BITWISEXOR, //y
      BITWISEAND, //y
      LEFTSHIFT, //y
      RIGHTSHIFT, //y
      ADD, //y
      SUBTRACT, //y
      MULTIPLY, //y
      DIVIDE, //y
      MOD, //y
      BITWISENOT, //y
      
      NEWLINE,
      MACRONEWLINE
    };

    enum class NicheType {
      UNASSIGNED,
      DIRECTIVE_DEFINE,
      DIRECTIVE_ENTRY,
      DIRECTIVE_INCLUDE,

      NUMBER_HEX,
      NUMBER_DEC,
      NUMBER_BIN
    };

    Type type = Type::UNASSIGNED;
    NicheType nicheType = NicheType::UNASSIGNED;

    Token(const std::string_view val, Type t, const SourceLocation& srcLoc, NicheType nt = NicheType::UNASSIGNED) : TokenBase(val, srcLoc), type(t), nicheType(nt) {}
  };

  struct TokenHolder : TokenBaseHolder<Token> {
    inline bool match(Token::Type type, size_t distance = 0) const {return peek(distance).type == type;}
    inline void skipUntilAfterType(Token::Type type) {while (!match(type)) {skip();} skip();}
  };

  class SpasmLexer : LexerBase {
    public:
    TokenHolder run(const std::string& source, const std::filesystem::path& path);
    SpasmLexer() {}

    private:
    Debug::FullLogger* p_logger;

    bool isWhitespace();
    bool isAtWordBoundary();
    void consumeUntilNotNumber();
    void consumeUntilNotHex();
    Token::NicheType getNicheTypeAndSetSliceOverNumber();
  };
}