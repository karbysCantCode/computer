#include "Spasm/spasm.hpp"

#include "fileHelper.hpp"
#include "lexHelper.hpp"

#include <functional>
#include <sstream>


//tokens
std::string Spasm::Lexer::Token::positionToString() const {
  if (m_fileName != nullptr) {
    return m_fileName->string() + ":" + std::to_string(m_line) + ":" + std::to_string(m_column);
  }
  return "line " + std::to_string(m_line) + ", column " + std::to_string(m_column);
}

namespace Spasm::Program::Expressions {
 std::string Label::toString(size_t padding, size_t indent, size_t minArgWidth, bool basicData) const {
  std::stringstream str;
  str << std::string(indent, ' ') << std::left << std::setw(minArgWidth) << "[Label]" << std::left << std::setw(minArgWidth) << m_name << ", Declared = " << m_declared << '\n';
  if (basicData) return str.str();

  //full data 
  if (m_parent != nullptr) {
    str << std::string(padding + indent, ' ') << "Parent label: " << m_parent->m_name << '\n';
  }
  for (const auto& child : m_children) {
    if (child.second != nullptr) {
      str << std::string(padding + indent, ' ') << "Child: " << child.second->m_name << '\n';
    }
  }
  return str.str();
 }

 std::string Instruction::toString(size_t padding, size_t indent, size_t minArgWidth, bool basicData) const {
  std::stringstream str;
  str << "[Instr]" << std::left << std::setw(minArgWidth);
  if (m_instruction != nullptr) {
    str << m_instruction->m_name;
  } else {
    str << "Unnamed";
  }

  for (const auto& op : m_operands) {
    str << op->toString() << ' ';
  }

  str << std::endl;
  
  if (basicData) return str.str();

  //full data 

  return str.str();
 }

 std::string Declaration::toString(size_t padding, size_t indent, size_t minArgWidth, bool basicData) const {
  std::stringstream str;

  str << std::string(indent, ' ') << std::left << std::setw(minArgWidth) << m_declaredName;
  if (m_dataType != nullptr) {
    str << ' ' << std::left << std::setw(minArgWidth) << m_dataType->m_name;
  }
  str << ' ' << m_declaration->toString();
  str << std::endl;
  
  if (basicData) return str.str();

  //full data 

  return str.str();
 }

 namespace Operands {
  std::string NumberLiteral::toString() const {
    return std::to_string(m_value);
  }

  std::string StringLiteral::toString() const {
    return m_value;
  }

  std::string RegisterLiteral::toString() const {
    if (m_register != nullptr) {
      return m_register->m_name;
    } else {
      return "NULLPTR";
    }
    
  }

  std::string ConstantExpression::toString() const {
    std::stringstream str;
    
    str << '(' << m_LHS->toString() << ' ';
    switch (m_type)
    {
    case Type::MULTIPLY:
      str << '*';
      break;
    case Type::DIVIDE:
      str << '/';
      break;
    case Type::MODULO:
      str << '%';
      break;
    case Type::ADDITION:
      str << '+';
      break;
    case Type::SUBTRACTION:
      str << '-';
      break;
    case Type::AND:
      str << '&';
      break;
    case Type::OR:
      str << '|';
      break;
    case Type::XOR:
      str << '^';
      break;
    case Type::NOT:
      str << '!';
      break;
    case Type::LEFTSHIFT:
      str << "<<";
      break;
    case Type::RIGHTSHIFT:
      str << ">>";
      break;
    
    default:
      break;
    }
    str << ' ' << m_RHS->toString() << ')';

    return str.str();
  }

  std::string MemoryAddressIdentifier::toString() const {
    std::stringstream str;

    std::visit([&str](auto&& arg){
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T,Label*>) {
            if (arg != nullptr) {
              str << arg->m_name;
            }
        } else if constexpr (std::is_same_v<T,Declaration*>) {
            if (arg != nullptr) {
              str << arg->m_declaredName;
            }
        } else {
            str << "NONETYPE";
        }
    }, m_identifier);
    
    return str.str();
  }
 }
}


bool Spasm::Program::ProgramParser::run(Spasm::Lexer::TokenHolder& tokenHolder, Arch::Architecture& arch, Spasm::Program::ProgramForm& program, std::filesystem::path path) {
  using namespace Spasm;

  tokenHolder.reset(); //ensure holder is at start.

  while(tokenHolder.notAtEnd()) {
    //check if is keyword
    auto keyToken = tokenHolder.peek();
    std::cout << keyToken.m_value << std::endl;
    auto instrIt = arch.m_instructionSet.find(keyToken.m_value);
    auto dataIt = arch.m_dataTypes.find(keyToken.m_value);

    if (instrIt != arch.m_instructionSet.end()) {
      //is instruction
      handleInstruction(tokenHolder, program, arch, instrIt->second);
      
    } else if (dataIt != arch.m_dataTypes.end()) {
      //is datatype

    } else if (tokenHolder.peek().m_type == Lexer::Token::Type::IDENTIFIER 
            &&  (tokenHolder.peek(1).m_type == Lexer::Token::Type::COLON 
              || tokenHolder.peek(1).m_type == Lexer::Token::Type::PERIOD)) {
      //is label
      std::cout << "islabel:" << tokenHolder.peek().m_value << '\n';
      consumeTokensForLabel(Lexer::Token::Type::COLON, tokenHolder, program, true, false);


    } else if (tokenHolder.peek().m_type == Lexer::Token::Type::DIRECTIVE) {
      std::cout << tokenHolder.consume().m_value << std::endl;


    } else if (tokenHolder.peek().m_type != Lexer::Token::Type::NEWLINE) {
      logError(keyToken, "Expected keyword or label, got \"" + keyToken.m_value + '"');
      tokenHolder.skip();
    } else {
      tokenHolder.skip();
    }
  }
  return true;
}

void Spasm::Program::ProgramParser::handleInstruction(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program, Arch::Architecture& arch, Arch::Instruction::Instruction& archInstruction) {
  auto instruction = std::make_unique<Program::Expressions::Instruction>(&archInstruction);
  for (const auto& arg : instruction->m_instruction->m_arguments) {

    switch (arg->m_type)
    {
    case Arch::Instruction::Argument::Type::IMMEDIATE:
      tokenHolder.skip();//to skip preceeding comma or the instr token!
      { 
        const auto& firstToken = tokenHolder.peek();
        switch (firstToken.m_type) {
          case Lexer::Token::Type::OPENSQUARE:
          {
            tokenHolder.skip();
            instruction->m_operands.push_back(std::move(parseExpression(tokenHolder,program)));
            if (tokenHolder.peek().m_type == Lexer::Token::Type::CLOSESQUARE) {
              tokenHolder.skip();
            } else {
              //error?
              logWarning(tokenHolder.peek(), "expected ']', got '" + tokenHolder.peek().m_value + '\'');
              tokenHolder.skip();
            }
          }
          break;
          case Lexer::Token::Type::IDENTIFIER: {
            //label or data decl
            
            auto identifier = std::make_unique<Program::Expressions::Operands::MemoryAddressIdentifier>();


            auto dataIt = program.m_dataDeclarations.find(tokenHolder.peek().m_value);
            if (tokenHolder.peek(1).isOneOf({Lexer::Token::Type::PERIOD,Lexer::Token::Type::COLON})) { //  || dataIt == program.m_dataDeclarations.end()
              //label
              const auto& topToken = tokenHolder.peek();
              identifier->m_identifier = consumeTokensForLabel(Lexer::Token::Type::UNASSIGNED, tokenHolder, program, false, true);
              if (auto ptr = std::get_if<Program::Expressions::Label*>(&identifier->m_identifier)) {
                  if (*ptr == nullptr) {
                      // stored Label* is null
                      logError(topToken,"Unknown label referenced at ");
                      instruction->m_operands.push_back(std::make_unique<Program::Expressions::Operands::MemoryAddressIdentifier>((Program::Expressions::Label*)nullptr));
                      break;
                    }
              }
            } else {
              //data decl
              identifier->m_identifier = dataIt->second;
            }

            instruction->m_operands.push_back(std::move(identifier));
          }
            
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
    case Arch::Instruction::Argument::Type::REGISTER:
      tokenHolder.skip();//to skip preceeding comma or the instr token!
    {
      auto it = arch.m_registers.find(tokenHolder.peek().m_value);
      if (it != arch.m_registers.end()) {
        auto reg = std::make_unique<Program::Expressions::Operands::RegisterLiteral>(&it->second);
        instruction->m_operands.push_back(std::move(reg));
      } else {
        logError(tokenHolder.peek(),"Unknown register \"" + tokenHolder.peek().m_value + "\" referenced as argument.");
      }

      tokenHolder.skip();
    }
      break;
    case Arch::Instruction::Argument::Type::NONE:
      tokenHolder.skip();//to skip preceeding comma or the instr token!  
      tokenHolder.skipUntilType(Lexer::Token::Type::NEWLINE);
      break;
    case Arch::Instruction::Argument::Type::CONSTANT:
      continue;
      break;
    default:
      logError(tokenHolder.peek(), "Unhandled argument type (from arch) in instruction \"" + instruction->m_instruction->m_name + '"' + std::string(1,'\n') + arg->toString(2,2));
      tokenHolder.skipUntilType(Lexer::Token::Type::NEWLINE);
      break;
    }
  }

  // if (instruction->m_instruction->m_arguments.size() < 1) {
  //   skip();
  // }
  tokenHolder.skipUntilType(Lexer::Token::Type::NEWLINE);

  program.m_statements.push_back(std::move(instruction));

}
void Spasm::Program::ProgramParser::handlenDatatype(Spasm::Lexer::TokenHolder&) {

}
// void Spasm::Program::ProgramParser::handleLabel(Spasm::Lexer::TokenHolder&) {

// }
Spasm::Program::Expressions::Symbol* Spasm::Program::ProgramParser::consumeTokensForSymbol(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program) {
  // if (delcNewIfNotExist && delimOnNonPeriod) {assert(false);}

  enum class SymbolType {
    ANONYMOUS,
    LABEL,
    DECLARATION
  };

  SymbolType type = SymbolType::ANONYMOUS;
  Spasm::Program::Expressions::Symbol* topSymbol;

  const auto& globToken = tokenHolder.peek();
  auto globIt = program.m_globalSymbols.find(globToken.m_value);

  if (globIt != program.m_globalSymbols.end()) {
    switch (globIt->second->m_type)
    {
    case Spasm::Program::Expressions::Symbol::Type::LABEL:
      type = SymbolType::LABEL;
      break;
    case Spasm::Program::Expressions::Symbol::Type::DECLARATION:
      type = SymbolType::DECLARATION;
      return globIt->second;
      break;
    
    default:
      break;
    }
  } else {
    if (tokenHolder.peek(1).isOneOf({Spasm::Lexer::Token::Type::COLON,Spasm::Lexer::Token::Type::PERIOD})) {
      type = SymbolType::LABEL;
    }
  }

  





}

Spasm::Program::Expressions::Label* Spasm::Program::ProgramParser::consumeTokensForLabel(Lexer::Token::Type delimiter, Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program, bool delcNewIfNotExist, bool delimOnNonPeriod)
{
  if (delcNewIfNotExist && delimOnNonPeriod) {assert(false);}
  
  Program::Expressions::Label* topLabel = nullptr;
  Program::Expressions::Label* currentLabel = nullptr;

  //parse "global" label of stack
  const auto& globalToken = tokenHolder.peek();
  auto it = program.m_globalSymbols.find(globalToken.m_value);
  if (it == program.m_globalSymbols.end()) {
    //make new label
    
    auto label = std::make_unique<Program::Expressions::Label>( tokenHolder.peek().m_value, currentLabel);
    auto labelptr = label.get();
    program.m_globalSymbols.emplace(labelptr->m_name, labelptr);
    topLabel = labelptr;
    currentLabel = labelptr;

    if ( tokenHolder.peek(1).m_type == delimiter && delcNewIfNotExist) {
      
      program.m_statements.push_back(std::move(label));
       tokenHolder.skip();
    } else {
      program.m_unownedSymbols.emplace(labelptr->m_name, std::move(label));
    }
  } else {
    if (it->second->m_type == Spasm::Program::Expressions::Symbol::Type::DECLARATION) {
      logError(globalToken, "")
    }
    //already exists, update
    const bool nextIsEnd = (delimOnNonPeriod && tokenHolder.peek(1).m_type != Lexer::Token::Type::PERIOD) ||  tokenHolder.peek(1).m_type == delimiter;
    topLabel = it->second;
    currentLabel = it->second;
    if (nextIsEnd && delcNewIfNotExist) {
      auto it = program.m_unownedLabels.find(globalToken.m_value);
      if (it != program.m_unownedLabels.end()) {
        it->second->m_declared = true;
        program.m_statements.push_back(std::move(it->second));
      }
       tokenHolder.skip();
    } 
  }

   tokenHolder.skip();

  while (tokenHolder.notAtEnd() &&  tokenHolder.peek().m_type == Lexer::Token::Type::PERIOD &&  tokenHolder.peek(1).m_type == Lexer::Token::Type::IDENTIFIER) {
     tokenHolder.skip(); //skip period

    const auto& identifierToken =  tokenHolder.peek();
    const auto& nextToken =  tokenHolder.peek(1);
    const bool nextIsEnd = (delimOnNonPeriod && nextToken.m_type != Lexer::Token::Type::PERIOD) || nextToken.m_type == delimiter;
    auto it = currentLabel->m_children.find(identifierToken.m_value);
    if (it == currentLabel->m_children.end()) {
      //not child
      //CREATE
      auto label = std::make_unique<Program::Expressions::Label>(identifierToken.m_value, currentLabel);
      auto labelptr = label.get();
      currentLabel->m_children.emplace(labelptr->m_name, labelptr);
      labelptr->m_parent = currentLabel;
      currentLabel = labelptr;

      if (nextIsEnd && delcNewIfNotExist) {
        program.m_statements.push_back(std::move(label));
        tokenHolder.skip(2);
        break;
      } else {
        program.m_unownedLabels.emplace(labelptr->getNameToParent(), std::move(label));
      }
    } else {
      it->second->m_parent = currentLabel;
      currentLabel = it->second;
      if (nextIsEnd && delcNewIfNotExist) {
        auto uit = program.m_unownedLabels.find(currentLabel->getNameToParent());
        if (uit != program.m_unownedLabels.end()) {
          //child of label but is unowned and is being owned here.
          uit->second->m_declared = true;
          program.m_statements.push_back(std::move(uit->second));
        } else {
          assert(false); //literally what
        }

         tokenHolder.skip();
      }
    }

     tokenHolder.skip(); // consume label identifier ()
  }

  return currentLabel;
}

using operandUniquePtr = std::unique_ptr<Spasm::Program::Expressions::Operands::Operand>;
operandUniquePtr Spasm::Program::ProgramParser::parsePrimary(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program) {
  const auto& token = tokenHolder.peek();
  if (token.m_type == Lexer::Token::Type::NUMBER) {
    tokenHolder.skip();
    return std::make_unique<Program::Expressions::Operands::NumberLiteral>(resolveNumber(token));
  
  
  } else if (token.m_type == Lexer::Token::Type::IDENTIFIER) {
    static_assert(false); //implement
    const auto& topToken = tokenHolder.peek();
    auto labelptr = consumeTokensForLabel(Lexer::Token::Type::IDENTIFIER, tokenHolder, program, false, true); //token type has NOTHING to do with delimiter since 3rd arg is true
    if (labelptr == nullptr) {
      logError(topToken, "Unknown label referenced.");
      return std::make_unique<Program::Expressions::Operands::NumberLiteral>(0);
    }
    return std::make_unique<Program::Expressions::Operands::MemoryAddressIdentifier>(labelptr);

  
  } else if (token.m_type == Lexer::Token::Type::OPENPAREN) {
    tokenHolder.skip(); // consume '['
    auto expr = parseExpression(tokenHolder,program);
    if (tokenHolder.peek().m_type != Lexer::Token::Type::CLOSEPAREN) {
      logError(tokenHolder.peek(), "Expected ')' after expression.");
      return expr; // recover
    }

    tokenHolder.skip(); // consume ')'

    return expr;
  } else {
    logError(token, "Unexpected type (expected number, identifier, or parentheses) at ");
    return std::make_unique<Program::Expressions::Operands::NumberLiteral>(0);
  }
}

operandUniquePtr Spasm::Program::ProgramParser::parseNot(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program) {
  if (tokenHolder.peek().m_type == Lexer::Token::Type::BITWISENOT) {
    tokenHolder.skip();
    auto operand = parseNot(tokenHolder,program);
    return std::make_unique<Program::Expressions::Operands::ConstantExpression>(Program::Expressions::Operands::ConstantExpression::Type::NOT, std::move(operand), nullptr);
  }

  return parsePrimary(tokenHolder,program);
}

operandUniquePtr Spasm::Program::ProgramParser::parseMultiplicative(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program) {
  auto lhs = parseNot(tokenHolder,program);
  
  while (tokenHolder.peek().isOneOf({
    Lexer::Token::Type::MULTIPLY,
    Lexer::Token::Type::DIVIDE,
    Lexer::Token::Type::MOD
  })) {

    Program::Expressions::Operands::ConstantExpression::Type op;
    switch (tokenHolder.peek().m_type)
    {
    case Lexer::Token::Type::MULTIPLY:
      op = Program::Expressions::Operands::ConstantExpression::Type::MULTIPLY;
    break;
    case Lexer::Token::Type::DIVIDE:
      op = Program::Expressions::Operands::ConstantExpression::Type::DIVIDE;
    break;
    case Lexer::Token::Type::MOD:
      op = Program::Expressions::Operands::ConstantExpression::Type::MODULO;
    break;
    
    default:
      logError(tokenHolder.peek(), "Shouldn't be an error.");
      assert(false);
    break;
    }
    tokenHolder.skip();

    auto rhs = parseNot(tokenHolder,program);

    lhs = std::make_unique<
        Program::Expressions::Operands::ConstantExpression
    >(op, std::move(lhs), std::move(rhs));
  }

  return lhs;
}

operandUniquePtr Spasm::Program::ProgramParser::parseAdditive(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program) {
  auto lhs = parseMultiplicative(tokenHolder,program);

  while (tokenHolder.peek().isOneOf({
    Lexer::Token::Type::ADD,
    Lexer::Token::Type::SUBTRACT
  })) {
    Program::Expressions::Operands::ConstantExpression::Type op;
    switch (tokenHolder.peek().m_type) {
      case Lexer::Token::Type::ADD:
        op = Program::Expressions::Operands::ConstantExpression::Type::ADDITION;
      break;
      case Lexer::Token::Type::SUBTRACT:
        op = Program::Expressions::Operands::ConstantExpression::Type::SUBTRACTION;
      break;
      
      default:
        logError(tokenHolder.peek(), "Shouldn't be an error.");
        assert(false);
      break;
    }
    tokenHolder.skip();
    auto rhs = parseMultiplicative(tokenHolder,program);

    lhs = std::make_unique<
        Program::Expressions::Operands::ConstantExpression
    >(op, std::move(lhs), std::move(rhs));
  }

  return lhs;
}

operandUniquePtr Spasm::Program::ProgramParser::parseShift(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program) {
  auto lhs = parseAdditive(tokenHolder,program);

  while (tokenHolder.peek().isOneOf({
    Lexer::Token::Type::LEFTSHIFT,
    Lexer::Token::Type::RIGHTSHIFT
  })) {
    Program::Expressions::Operands::ConstantExpression::Type op;
    switch (tokenHolder.peek().m_type) {
      case Lexer::Token::Type::LEFTSHIFT:
        op = Program::Expressions::Operands::ConstantExpression::Type::LEFTSHIFT;
      break;
      case Lexer::Token::Type::RIGHTSHIFT:
        op = Program::Expressions::Operands::ConstantExpression::Type::RIGHTSHIFT;
      break;
      
      default:
        logError(tokenHolder.peek(), "Shouldn't be an error.");
        assert(false);
      break;
    }
    tokenHolder.skip();
    auto rhs = parseAdditive(tokenHolder,program);

    lhs = std::make_unique<
        Program::Expressions::Operands::ConstantExpression
    >(op, std::move(lhs), std::move(rhs));
  }

  return lhs;
}

operandUniquePtr Spasm::Program::ProgramParser::parseAnd(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program) {
  auto lhs = parseShift(tokenHolder,program);

  while (tokenHolder.peek().m_type == Lexer::Token::Type::BITWISEAND) {
    Program::Expressions::Operands::ConstantExpression::Type op = Program::Expressions::Operands::ConstantExpression::Type::AND;
    tokenHolder.skip();
    auto rhs = parseShift(tokenHolder,program);

    lhs = std::make_unique<
        Program::Expressions::Operands::ConstantExpression
    >(op, std::move(lhs), std::move(rhs));
  }

  return lhs;
}

operandUniquePtr Spasm::Program::ProgramParser::parseXor(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program) {
  auto lhs = parseAnd(tokenHolder,program);

  while (tokenHolder.peek().m_type == Lexer::Token::Type::BITWISEXOR) {
    Program::Expressions::Operands::ConstantExpression::Type op = Program::Expressions::Operands::ConstantExpression::Type::XOR;
    tokenHolder.skip();
    auto rhs = parseAnd(tokenHolder,program);

    lhs = std::make_unique<
        Program::Expressions::Operands::ConstantExpression
    >(op, std::move(lhs), std::move(rhs));
  }

  return lhs;
}

operandUniquePtr Spasm::Program::ProgramParser::parseOr(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program) {
  auto lhs = parseXor(tokenHolder,program);

  while (tokenHolder.peek().m_type == Lexer::Token::Type::BITWISEOR) {
    Program::Expressions::Operands::ConstantExpression::Type op = Program::Expressions::Operands::ConstantExpression::Type::OR;
    tokenHolder.skip();
    auto rhs = parseXor(tokenHolder,program);

    lhs = std::make_unique<
        Program::Expressions::Operands::ConstantExpression
    >(op, std::move(lhs), std::move(rhs));
  }

  return lhs;
}

operandUniquePtr Spasm::Program::ProgramParser::parseExpression(Spasm::Lexer::TokenHolder& tokenHolder, Spasm::Program::ProgramForm& program) {
   return parseOr(tokenHolder,program);
}

int Spasm::Program::ProgramParser::resolveNumber(const Spasm::Lexer::Token& token) {
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
      logError(token, "Non number niche type (lexer should specify).");
      return 0;
      break;
    }
    return std::stoi(token.m_value,nullptr,base);
}

Spasm::Lexer::TokenHolder Spasm::Lexer::Lexer::run(const std::filesystem::path& path, std::unordered_set<std::string>& keywords) {
  TokenHolder tokenHolder;

  p_lexHelper.reset();
  p_lexHelper.m_source = FileHelper::openFileToString(path);
  p_lexHelper.m_keywords = &keywords;
  
  while (p_lexHelper.notAtEnd())
  {
    if (p_lexHelper.skipWhitespace(true)) {
      tokenHolder.m_tokens.emplace_back(Token::Type::NEWLINE, &path, p_lexHelper.m_line, p_lexHelper.m_column);
    }
    if (!p_lexHelper.notAtEnd()) {break;}

    char c = p_lexHelper.peek();
    
    if (isdigit(c) || (c == '-' && isdigit(p_lexHelper.peek(1)))) {
      //number
      Spasm::Lexer::Token token(Spasm::Lexer::Token::Type::NUMBER, &path, p_lexHelper.m_line, p_lexHelper.m_column);
      token.m_nicheType = Token::NicheType::NUMBER_DEC;
      std::string value;
      if (c == '-' && isdigit(p_lexHelper.peek(1))) {
        value.push_back(p_lexHelper.consume());
      }

      if (p_lexHelper.peek() == '0') {
        switch (p_lexHelper.peek(1)) {
          case 'x':
            token.m_nicheType = Token::NicheType::NUMBER_HEX;
            p_lexHelper.consume();
            p_lexHelper.consume();
            break;
          case 'b':
            token.m_nicheType = Token::NicheType::NUMBER_BIN;
            p_lexHelper.consume();
            p_lexHelper.consume();
            break;
          default:
          break;
        }
      }

      bool hasDecPoint = false;

      while (p_lexHelper.notAtEnd()) {
        char c = p_lexHelper.peek();
        if (c == '.') {
          if (hasDecPoint || token.m_nicheType != Token::NicheType::NUMBER_DEC) {
            logError("Incorrectly formatted number (either too many, or wrong type for '.').");
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
        p_lexHelper.consume();
      }
      token.setString(value);
      tokenHolder.m_tokens.push_back(token);

    } else if (isalpha(c) || c == '.') {
      //identifier/keyword
      Spasm::Lexer::Token token(Spasm::Lexer::Token::Type::IDENTIFIER, &path, p_lexHelper.m_line, p_lexHelper.m_column);
      std::string value;

      while (p_lexHelper.notAtEnd() && !p_lexHelper.isWordBoundary(p_lexHelper.peek())) {
        value.push_back(p_lexHelper.consume());
      }
      
      if (p_lexHelper.isKeyword(value)) {
        token.m_type = Spasm::Lexer::Token::Type::KEYWORD;
      }

      token.setString(value);
      tokenHolder.m_tokens.push_back(token);


    } else if (c == ';') {
      tokenHolder.m_tokens.emplace_back(Token::Type::NEWLINE, &path, p_lexHelper.m_line, p_lexHelper.m_column);
      p_lexHelper.skipComment(p_lexHelper.peek(1) == '*');
    } else if (c == '@') {
      //directive
      Spasm::Lexer::Token token(Spasm::Lexer::Token::Type::DIRECTIVE, &path, p_lexHelper.m_line, p_lexHelper.m_column);
      std::string value;
      while (p_lexHelper.notAtEnd() && !p_lexHelper.isWordBoundary(p_lexHelper.peek())) {
        value.push_back(p_lexHelper.consume());
      }
      if (value == "@entry") {
        token.setNicheType(Token::NicheType::DIRECTIVE_ENTRY);
      } else if (value == "@define") {
        token.setNicheType(Token::NicheType::DIRECTIVE_DEFINE);
      } else if (value == "@include") {
        token.setNicheType(Token::NicheType::DIRECTIVE_INCLUDE);
      }
      token.setString(value);
      tokenHolder.m_tokens.push_back(token);
    } else if (c == '\'' || c == '"') {
      Spasm::Lexer::Token token(Spasm::Lexer::Token::Type::STRING, &path, p_lexHelper.m_line, p_lexHelper.m_column);
      token.setString(p_lexHelper.consumeString(c));
      tokenHolder.m_tokens.push_back(token);
    } else {
      Spasm::Lexer::Token token(std::string(1, c),Spasm::Lexer::Token::Type::UNASSIGNED, &path, p_lexHelper.m_line, p_lexHelper.m_column);
      p_lexHelper.consume();
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
          if (p_lexHelper.peek(1) != '<') {break;}
          token.m_type = Spasm::Lexer::Token::Type::LEFTSHIFT;
          break;
        case '>':
        if (p_lexHelper.peek(1) != '>') {break;}
          token.m_type = Spasm::Lexer::Token::Type::RIGHTSHIFT;
          break;
        case '\\':
          token.m_type = Spasm::Lexer::Token::Type::MACRONEWLINE;
          break;
        default:
          break;
      }

      tokenHolder.m_tokens.push_back(token);
    }

  

    
  }
  return tokenHolder;
}


std::string Spasm::Program::Expressions::Label::getNameToParent() {
  std::stack<const std::string&> nameStack;

  Spasm::Program::Expressions::Label* currentLabel = this;

  while (currentLabel != nullptr) {
    nameStack.push(currentLabel->m_name);
    currentLabel = currentLabel->m_parent;
  }

  std::string fullName;

  while (!nameStack.empty()) {
    fullName.append(nameStack.top() + '.');
    nameStack.pop();
  }

  return fullName;
}
