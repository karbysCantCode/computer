#include "spasm.hpp"

#include "fileHelper.hpp"
#include "lexHelper.hpp"


//tokens
std::string Spasm::Lexer::Token::positionToString() const {
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
    if (skipWhitespace(true)) {
      tokens.emplace_back(Token::Type::NEWLINE, line, column);
    }
    if (!notAtEnd()) {break;}

    char c = peek();
    
    if (isdigit(c) || (c == '-' && isdigit(peek(1)))) {
      //number
      Spasm::Lexer::Token token(Spasm::Lexer::Token::Type::NUMBER, line, column);
      token.m_nicheType = Token::NicheType::NUMBER_DEC;
      std::string value;
      if (c == '-' && isdigit(peek(1))) {
        value.push_back(consume());
      }

      if (peek() == '0') {
        switch (peek(1)) {
          case 'x':
            token.m_nicheType = Token::NicheType::NUMBER_HEX;
            consume();
            consume();
            break;
          case 'b':
            token.m_nicheType = Token::NicheType::NUMBER_BIN;
            consume();
            consume();
            break;
          default:
          break;
        }
      }

      bool hasDecPoint = false;

      while (notAtEnd()) {
        char c = peek();
        if (c == '.') {
          if (hasDecPoint || token.m_nicheType != Token::NicheType::NUMBER_DEC) {
            logError("Incorrectly formatted number (either too many, or wrong type for '.') in file \"" + path.generic_string() + "\" at line " + std::to_string(line) + " column " + std::to_string(column));
            break;
          }
          value.push_back('.');
          continue;
        }

        if (token.m_nicheType == Token::NicheType::NUMBER_DEC) {
          if (isdigit(c)) {
            value.push_back(c);
          }
          else break;
        } else if (token.m_nicheType == Token::NicheType::NUMBER_HEX) {
          if (isxdigit(c)) {
            value.push_back(c);
          }
          else break;
        } else if (token.m_nicheType == Token::NicheType::NUMBER_BIN) {
          if (c == '1' || c == '0') {
            value.push_back(c);
          }
          else break;
        } else {
          std::cerr << "Token has non-number niche type. This shouldn't happen. (Spasm Lexer) in \"" << __FILE__  << "\" at line "<< __LINE__;
          assert(false);
        }
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
      tokens.emplace_back(Token::Type::NEWLINE, line, column);
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
        case '.':
          token.m_type = Spasm::Lexer::Token::Type::PERIOD;
          break;
        case ':':
          token.m_type = Spasm::Lexer::Token::Type::COLON;
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

  #undef notAtEnd
  #undef skipWhitespace
  #undef peek
  #undef line 
  #undef column 
  #undef isWordBoundary
  #undef consume
  #undef isKeyword
  #undef skipComment
  #undef consumeString
  #undef getUntilWordBoundary
  #undef logError
  #undef logWarning
  #undef logDebug
}

Spasm::Program::ProgramForm Spasm::Program::parseProgram(std::vector<Spasm::Lexer::Token>& tokens, Arch::Architecture& arch, Debug::FullLogger* logger, std::filesystem::path path) {
  using namespace Spasm;
  Program::ProgramForm program;

  size_t pos = 0;

  #define logError(message)   if (logger != nullptr) logger->Errors.logMessage(message);
  #define logWarning(message) if (logger != nullptr) logger->Warnings.logMessage(message);
  #define logDebug(message)   if (logger != nullptr) logger->Debugs.logMessage(message);

  auto peek = [&](size_t distance = 0) -> Lexer::Token {
    return (pos + distance) < tokens.size() ? tokens[pos + distance] : Lexer::Token(Lexer::Token::Type::UNASSIGNED);
  };
  auto consume = [&]() -> Lexer::Token {
    auto ret = (pos) < tokens.size() ? tokens[pos] : Lexer::Token(Lexer::Token::Type::UNASSIGNED);
    pos++;
    return ret;
  };
  auto skip = [&](size_t distance = 1) -> void {
    pos+=distance;
  };
  auto notAtEnd = [&]() -> bool {
    return pos < tokens.size();
  };
  auto isAtEnd = [&]() -> bool {
    return !(pos < tokens.size());
  };
  #define skipUntilTrue(condition) while(!(condition)) {skip();}
  //peek() needs to be the top level identifier token
  auto consumeTokensForLabel = [&](const std::initializer_list<Lexer::Token::Type>& list, bool delcNewIfNotExist = false, bool exclusiveList = false) -> Program::Expressions::Label* {
    if (peek(1).m_type == Lexer::Token::Type::PERIOD) {
      //get the global
      const auto it = program.m_globalLabels.find(peek().m_value);
      if (it != program.m_globalLabels.end()) {
        //walk the path of .
        Program::Expressions::Label* currentLabel = it->second;
        skip(2);
        bool walking = true;
        while (walking && currentLabel != nullptr) {
          const auto it = currentLabel->m_children.find(peek().m_value);
          if (it != currentLabel->m_children.end()) {
            //keep walking
            currentLabel = it->second;
          } else if (exclusiveList ^ peek(1).isOneOf(list)) {
            //add label here.
            walking = false;
            if (delcNewIfNotExist) {
              auto label = std::make_unique<Program::Expressions::Label>(peek().m_value,currentLabel);
              auto labelPtr = label.get();
              currentLabel->m_children.emplace(label->m_name, label.get());
              program.m_statements.push_back(std::move(label));
              skip(2);
              return labelPtr;
            } else {
              skip(2);
              return currentLabel;
            }
          } else {
            logWarning("Weird position in label walker reached " + peek().positionToString());
            skip(2);
            return nullptr;
            //dont really know what to do here
          }
          skip(2);
        }

      } else {
        logError("Unknown label \"" + peek().m_value + "\" at " + peek().positionToString());
        skipUntilTrue((exclusiveList ^ peek(1).isOneOf(list))
                    || peek().m_type == Lexer::Token::Type::NEWLINE);
      }
    } else if (exclusiveList ^ peek(1).isOneOf(list)) {
      const auto it = program.m_globalLabels.find(peek().m_value);
      if (it != program.m_globalLabels.end()) {
        skip(2);
        return it->second;
      } else {
        if (delcNewIfNotExist) {
          auto label = std::make_unique<Program::Expressions::Label>(peek().m_value);
          auto labelptr = label.get();
          program.m_globalLabels.emplace(label->m_name, label.get());
          program.m_statements.push_back(std::move(label));
          skip(2);
          return labelptr;
        } else {
          logError("Referenced label \"" + peek().m_value + "\" is not declared, " + peek().positionToString());
          skip(2);
          return nullptr;
        }
      }
    } else {
      logError("Reached not delimiter on, supposedly, a global label reference at " + peek().positionToString());
      skip(1);
      return nullptr;
    }
    return nullptr;
  };
  auto resolveNumber = [&](const Lexer::Token& token) -> int {
    size_t base = 10;
    switch (token.m_nicheType) {
      case Lexer::Token::NicheType::NUMBER_BIN:
        base = 2;
      break;
      case Lexer::Token::NicheType::NUMBER_HEX:
        base = 16;
      break;
      case Lexer::Token::NicheType::NUMBER_DEC:
        base = 10;
      break;
      default:
      logError("Non number niche type (lexer should specify) at " + token.positionToString());
      return 0;
      break;
    }
    return std::stoi(token.m_value,nullptr,base);
  };
  //descent
  auto parsePrimary = [&]() -> std::unique_ptr<Program::Expressions::Operands::Operand> {
    const auto& token = peek();
    if (token.m_type == Lexer::Token::Type::NUMBER) {
      return std::make_unique<Program::Expressions::Operands::NumberLiteral>(resolveNumber(token));
    } else if (token.m_type == Lexer::Token::Type::IDENTIFIER) {
      consumeTokensForLabel({Lexer::Token::Type::},false, true);
      //if ()
      //return std::make_unique<Program::Expressions::Operands::MemoryAddressIdentifier>()
      assert(false);
    } else if (token.m_type == Lexer::Token::Type::OPENPAREN) {
      
    }
  };
  auto parseNot = [&]() -> std::unique_ptr<Program::Expressions::Operands::Operand> {
    auto lhs = parsePrimary();
  };
  auto parseMultiplicative = [&]() -> std::unique_ptr<Program::Expressions::Operands::Operand> {
    auto lhs = parseNot();
  };
  auto parseAdditive = [&]() -> std::unique_ptr<Program::Expressions::Operands::Operand> {
    auto lhs = parseMultiplicative();
  };
  auto parseShift = [&]() -> std::unique_ptr<Program::Expressions::Operands::Operand> {
    auto lhs = parseAdditive();
  };
  auto parseAnd = [&]() -> std::unique_ptr<Program::Expressions::Operands::Operand> {
    auto lhs = parseShift();
  };
  auto parseXor = [&]() -> std::unique_ptr<Program::Expressions::Operands::Operand> {
    auto lhs = parseAnd();
  };
  auto parseOr = [&]() -> std::unique_ptr<Program::Expressions::Operands::Operand> {
    auto lhs = parseXor();
  };
  auto parseExpression = [&]() -> std::unique_ptr<Program::Expressions::Operands::Operand> {
    parseOr();
  };

  while(notAtEnd()) {
    //check if is keyword
    auto keyToken = peek();
    auto instrIt = arch.m_instructionSet.find(keyToken.m_value);
    auto dataIt = arch.m_dataTypes.find(keyToken.m_value);

    if (instrIt != arch.m_instructionSet.end()) {
      //is instruction
      auto instruction = std::make_unique<Program::Expressions::Instruction>(&instrIt->second);
      for (const auto& arg : instruction->m_instruction->m_arguments) {
        skip(); //to skip preceeding comma or the instr token!

        switch (arg.m_type)
        {
        case Arch::Instruction::Argument::Type::IMMEDIATE:
          { 
            const auto& firstToken = peek();
            switch (firstToken.m_type) {
              case Lexer::Token::Type::OPENSQUARE:
              {
                
              }
              break;
              case Lexer::Token::Type::IDENTIFIER:
                //label or data decl
              break;
              case Lexer::Token::Type::NUMBER: {
                auto arg = std::make_unique<Program::Expressions::Operands::NumberLiteral>(resolveNumber(firstToken));
                instruction->m_operands.push_back(std::move(arg));
              }
              break;
              default:

              break;
            }
          }
          break;
        case Arch::Instruction::Argument::Type::REGISTER:{
          auto it = arch.m_registers.find(peek().m_value);
          if (it != arch.m_registers.end()) {
            auto reg = std::make_unique<Program::Expressions::Operands::RegisterLiteral>(&it->second);
            instruction->m_operands.push_back(std::move(reg));
          } else {
            logError("Unknown register \"" + peek().m_value + "\" referenced as argument at " + peek().positionToString());
          }

          skip();
        }
          break;
        case Arch::Instruction::Argument::Type::NONE:
          skipUntilTrue(peek().m_type == Lexer::Token::Type::NEWLINE);
          break;
        
        default:

          break;
        }
      }

    } else if (dataIt != arch.m_dataTypes.end()) {
      //is datatype

    } else if (peek().m_type == Lexer::Token::Type::IDENTIFIER 
            &&  (peek(1).m_type == Lexer::Token::Type::COLON 
              || peek(1).m_type == Lexer::Token::Type::PERIOD)) {
      //is label
      consumeTokensForLabel(Lexer::Token::Type::COLON,true);
    } else {
      logError("Expected keyword or label, got \"" + keyToken.m_value + "\" at " + keyToken.positionToString());
      skip();
    }
  }
  return program;
}