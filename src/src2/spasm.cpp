#include "spasm.hpp"

#include "fileHelper.hpp"
#include "lexHelper.hpp"


//tokens
std::string Spasm::Lexer::Token::positionToString() {
  if (m_fileName != nullptr) {
    return *m_fileName + ": line " + std::to_string(m_line) + ", column " + std::to_string(m_column);
  }
  return "line " + std::to_string(m_line) + ", column " + std::to_string(m_column);
}

//lexer
std::vector<Spasm::Lexer::Token> Spasm::Lexer::lex(std::filesystem::path path, std::unordered_set<std::string>& keywords, Debug::FullLogger* logger) {
  //setup for the file to lex
  std::string source = FileHelper::openFileToString(path, logger);
  if (source.empty()) {return std::vector<Spasm::Lexer::Token>();}
  LexHelper lexHelper(source, &keywords);
  std::vector<Spasm::Lexer::Token> tokens;

  //helper functions
  #define logError(message)   if (logger != nullptr) logger->Errors.logMessage(message);
  #define logWarning(message) if (logger != nullptr) logger->Warnings.logMessage(message);
  #define logDebug(message)   if (logger != nullptr) logger->Debugs.logMessage(message);

  #define notAtEnd() lexHelper.notAtEnd()
  #define skipWhitespace(arg) lexHelper.skipWhitespace(arg)
  #define peek(arg) lexHelper.peek(arg)
  #define line lexHelper.m_line
  #define column lexHelper.m_column
  #define isWordBoundary(arg) lexHelper.isWordBoundary(arg)
  #define consume() lexHelper.consume()
  #define isKeyword(arg) lexHelper.isKeyword(arg)
  #define skipComment(arg) lexHelper.skipComment(arg)
  #define consumeString(arg) lexHelper.consumeString(arg)
  
  //lex the file!
  while (notAtEnd())
  {
    skipWhitespace();
    if (!notAtEnd()) {break;}

    char c = peek();
    
    if (isdigit(c)) {
      //number
      Spasm::Lexer::Token token(Spasm::Lexer::Token::Type::NUMBER, line, column);
      std::string value;
      while ((isdigit(peek()) || peek() == '.') && notAtEnd() && !isWordBoundary(peek())) {
        value.push_back(consume());
      }
      token.setString(value);
      tokens.push_back(token);

    } else if (isalpha(c) || c == '.') {
      //identifier/keyword
      Spasm::Lexer::Token token(Spasm::Lexer::Token::Type::IDENTIFIER, line, column);
      std::string value;

      while (notAtEnd() && !isWordBoundary(peek())) {
        value.push_back(consume());
      }
      
      if (isKeyword(value)) {
        token.m_type = Spasm::Lexer::Token::Type::KEYWORD;
      }

      token.setString(value);
      tokens.push_back(token);


    } else if (c == ';') {
      skipComment(peek(1) == '*');
    } else if (c == '@') {
      //directive
      Spasm::Lexer::Token token(Spasm::Lexer::Token::Type::DIRECTIVE, line, column);
      std::string value;
      while (notAtEnd() && !isWordBoundary(peek())) {
        value.push_back(consume());
      }
      if (value == "@entry") {
        token.setNicheType(Token::NicheType::DIRECTIVE_ENTRY);
      } else if (value == "@define") {
        token.setNicheType(Token::NicheType::DIRECTIVE_DEFINE);
      } else if (value == "@include") {
        token.setNicheType(Token::NicheType::DIRECTIVE_INCLUDE);
      }
      token.setString(value);
      tokens.push_back(token);
    } else if (c == '\'' || c == '"') {
      Spasm::Lexer::Token token(Spasm::Lexer::Token::Type::STRING, line, column);
      token.setString(consumeString(c));
      tokens.push_back(token);
    } else {
      Spasm::Lexer::Token token(std::string(1, c),Spasm::Lexer::Token::Type::UNASSIGNED, line, column);
      consume();
      switch (c) {
        case '(':
          token.m_type = Spasm::Lexer::Token::Type::OPENPAREN;
          break;
        case ')':
          token.m_type = Spasm::Lexer::Token::Type::CLOSEPAREN;
          break;
        case ',':
          token.m_type = Spasm::Lexer::Token::Type::COMMA;
          break;
        case '[':
          token.m_type = Spasm::Lexer::Token::Type::OPENSQUARE;
          break;
        case ']':
          token.m_type = Spasm::Lexer::Token::Type::CLOSESQUARE;
          break;
        case '{':
          token.m_type = Spasm::Lexer::Token::Type::OPENBLOCK;
          break;
        case '}':
          token.m_type = Spasm::Lexer::Token::Type::CLOSEBLOCK;
          break;
        case '+':
          token.m_type = Spasm::Lexer::Token::Type::ADD;
          break;
        case '-':
          token.m_type = Spasm::Lexer::Token::Type::SUBTRACT;
          break;
        case '*':
          token.m_type = Spasm::Lexer::Token::Type::MULTIPLY;
          break;
        case '/':
          token.m_type = Spasm::Lexer::Token::Type::DIVIDE;
          break;
        case '|':
          token.m_type = Spasm::Lexer::Token::Type::BITWISEOR;
          break;
        case '&':
          token.m_type = Spasm::Lexer::Token::Type::BITWISEAND;
          break;
        case '^':
          token.m_type = Spasm::Lexer::Token::Type::BITWISEXOR;
          break;
        case '~':
          token.m_type = Spasm::Lexer::Token::Type::BITWISENOT;
          break;
        case '<':
          if (peek(1) != '<') {break;}
          token.m_type = Spasm::Lexer::Token::Type::LEFTSHIFT;
          break;
        case '>':
        if (peek(1) != '>') {break;}
          token.m_type = Spasm::Lexer::Token::Type::RIGHTSHIFT;
          break;
        case '\\':
          token.m_type = Spasm::Lexer::Token::Type::MACRONEWLINE;
          break;
        default:
          break;
      }

      tokens.push_back(token);
    }

  

    
  }
  return tokens;
}