#include "Spasm/Parser.hpp"

#include <utility>

namespace Spasm {
Program Parser::ParseTokens(TokenHolder& tokenHolder, Arch::Architecture& arch, Debug::FullLogger* logger) {
  //implementation list
  /*
  [X] Directives
  [Y]   Define
  [Y]     Replacement
  [Y]     Function
  [Y]       Field replacement
  [X]   Include
  [X]   Entry
  [X] Instructions
  [Y]   Keywords/instruction names
  [Y]   Registers
  [P]   Staticly compiled numbers
  [Y]   Constants
  [X]   Field range checking
  [X] Labels
  [X]   Basic inheritance
  [X]   Relative labels
  [X] Datatypes
  [X]   Text
  [X]   Array
  [X]   Auto
  [X]   Size checking
  */

  p_logger = logger;
  Program program;

  while (tokenHolder.notAtEnd()) {
    switch (tokenHolder.peek().type) {
      using TY = Token::Type;
      case TY::DIRECTIVE: {
        logError(tokenHolder.peek(), "Preproccessor didn't handle directive.");
        tokenHolder.skip();
        break;
      }
      case TY::IDENTIFIER: {
        parseIdentifier(tokenHolder, arch, program);
        break;
      }
      default: {
        logError(tokenHolder.peek(), "Unexpected token type.");
        tokenHolder.skip();
        break;
      }
    }
  }

  return program;
}

void Parser::parseIdentifier(TokenHolder& tokenHolder, Arch::Architecture& arch, Program& program) {
  // probably doesnt actually need to differ register and format,
  //   and could probably actually use datatypes instead...
  const Token& initToken = tokenHolder.consume();
  switch (arch.getKeywordTypeOfWord(initToken.value))
  {
  case Arch::Architecture::KeywordType::INSTRUCTION:
  {
    /* code */
    const auto& instructionDefinition = arch.m_instructionSet.find((std::string)initToken.value);
    if (instructionDefinition == arch.m_instructionSet.end()) {logError(initToken, std::format("Unknown instruction \"{}\" referenced.", initToken.value));return;}
    parseInstruction(tokenHolder, initToken, arch, instructionDefinition->second, program);
    break;
  }
  case Arch::Architecture::KeywordType::DATATYPE:
    parseDataTypeDeclaration(tokenHolder, program);
    break;
  default:
    if (tokenHolder.match(Token::Type::IDENTIFIER)) {
      parseLabelDefinition(tokenHolder, program);
    } else {
      logError(tokenHolder.consume(), "Unexpected keyword type (expected instruction or datatype)");
    }
    break;
  }
}

void Parser::parseInstruction(TokenHolder& tokenHolder, const Token& instrToken, const Arch::Architecture& arch, const Arch::Architecture::InstructionDefinition& instruction, Program& program) {
  auto instructionSymbol = std::make_unique<Program::InstructionSymbol>(instrToken.location, instruction);
  instructionSymbol->operands.reserve(instruction.m_operands.size()); // reserve to prevent resizing overhead
  
  for (const auto& operand : instruction.m_operands) {

    std::visit([&](auto& op) {
      using T = std::decay_t<decltype(op)>;

      if constexpr (std::is_same_v<T, Arch::Architecture::ConstantIntOperand>) {
        auto programOperand = std::make_unique<Program::ConstantOperand>(instrToken.location, op.constant);
        instructionSymbol->operands.push_back(std::move(programOperand));
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ConstantStringOperand>) {
        //this is kind of innecifiaent, because per instruction, the constant str will be converted
        instructionSymbol->operands.push_back(std::move(convertConstantStringToOperand(arch, op)));
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::RegisterOperand>) {
        
        const auto [regptr, identifierToken] = parseExpectRegister(tokenHolder, arch);
        
        std::unique_ptr<Program::Operand> programOperand;

        if (!regptr) {
          programOperand = makeErrorOperand(identifierToken, "Unknown register");
        }
        else if (op.acceptedRegisterNames.count(regptr->m_registerName)){
          programOperand = makeErrorOperand(identifierToken, "Register out of range for this field.");
        }
        else {
          programOperand = std::make_unique<Program::RegisterOperand>(identifierToken.location, *regptr);
        }

        instructionSymbol->operands.push_back(std::move(programOperand));

      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ImmediateOperand>) {
        auto programOperand = parseExpectImmediate(tokenHolder);
        instructionSymbol->operands.push_back(std::move(programOperand));
      }
    }, operand);

    if (tokenHolder.match(Token::Type::COMMA)) {
      tokenHolder.skip();
    }
  }

  program.m_statementVector.push_back(std::move(instructionSymbol));
}

std::pair<const Arch::Architecture::RegisterDefinition*, Token> Parser::parseExpectRegister(TokenHolder& tokenHolder, const Arch::Architecture& arch) {
  const auto regToken = tokenHolder.consume();
  const auto it = arch.m_registerSet.find((std::string)regToken.value);
    
  return {
      it != arch.m_registerSet.end() ? &it->second : nullptr,
      regToken
  };
}

std::unique_ptr<Program::Operand> Parser::parseExpectImmediate(TokenHolder& tokenHolder) {
  switch (tokenHolder.peek().type) {
    case Token::Type::OPENSQUARE: {
      const auto startToken = tokenHolder.consume();
      auto expr = std::make_unique<Program::ExpressionOperand>(startToken.location, parseSquareExpression(tokenHolder));
      if (!tokenHolder.match(Token::Type::CLOSESQUARE)) {
        return makeErrorOperand(tokenHolder.peek(), std::format("Expected ']', got \"{}\"", tokenHolder.peek().value));
      }
      tokenHolder.skip();
      return expr;
      break;
    }
    case Token::Type::IDENTIFIER: {
      auto expr = std::make_unique<Program::IdentifierExpr>(tokenHolder.peek().value);
      return std::make_unique<Program::ExpressionOperand>(tokenHolder.consume().location, std::move(expr));
    }
    case Token::Type::NUMBER: {
      auto expr = std::make_unique<Program::NumberExpr>(parseNumberString(tokenHolder.peek()));
      return std::make_unique<Program::ExpressionOperand>(tokenHolder.consume().location, std::move(expr));
    }
    case Token::Type::SUBTRACT: {
      return std::make_unique<Program::ExpressionOperand>(tokenHolder.consume().location, parseUnary(tokenHolder));
    }
    default:
      return makeErrorOperand(tokenHolder.peek(), std::format("Expected a constant, got \"{}\"", tokenHolder.consume().value));
    break;
  }
}

std::unique_ptr<Program::Expr> Parser::parseSquareExpression(TokenHolder& tokenHolder) {
  return parseOr(tokenHolder);
}

std::unique_ptr<Program::Operand> Parser::convertConstantStringToOperand(const Arch::Architecture& arch, const Arch::Architecture::ConstantStringOperand& constOp) {
  const auto it = arch.m_registerSet.find(constOp.constant);
  if (it != arch.m_registerSet.end()) {
    return std::make_unique<Program::RegisterOperand>(SourceLocation("ARCH DEFINED CONST", -1, -1), it->second);
  }
  assert(false);
  //undefined case
}

std::unique_ptr<Program::Operand> Parser::makeErrorOperand(const Token& errToken, const std::string& message) {
  logError(errToken, message);
  return std::make_unique<Program::ConstantOperand>(errToken.location, 0);
}

int Parser::parseNumberString(const Token& token) {
  if (!(token.value.size() > 0)) {logError(token, "Number token size is 0"); return 0;}

  switch (token.nicheType)
  {
  case Token::NicheType::NUMBER_DEC:
    return std::stoi((std::string)token.value, nullptr, 10);
    break;
  case Token::NicheType::NUMBER_HEX:
    return std::stoi((std::string)token.value, nullptr, 16);
    break;
  case Token::NicheType::NUMBER_BIN:
    return std::stoi((std::string)token.value, nullptr, 2);
    break;
  
  default:
    logError(token, "Expected number to have niche type.");
    return 0;
    break;
  }
}

std::unique_ptr<Program::Expr> Parser::parseOr(TokenHolder& tokenHolder) {
  auto lhs = parseXor(tokenHolder);

  while (tokenHolder.match(Token::Type::BITWISEOR) && tokenHolder.notAtEnd()) {
    tokenHolder.skip();
    auto rhs = parseXor(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      Token::Type::BITWISEOR,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseXor(TokenHolder& tokenHolder) {
  auto lhs = parseAnd(tokenHolder);

  while (tokenHolder.match(Token::Type::BITWISEXOR) && tokenHolder.notAtEnd()) {
    tokenHolder.skip();
    auto rhs = parseAnd(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      Token::Type::BITWISEXOR,
      std::move(lhs), std::move(rhs)
    );
  }

  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseAnd(TokenHolder& tokenHolder) {
  auto lhs = parseShift(tokenHolder);

  while (tokenHolder.match(Token::Type::BITWISEAND) && tokenHolder.notAtEnd()) {
    tokenHolder.skip();
    auto rhs = parseShift(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      Token::Type::BITWISEAND,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseShift(TokenHolder& tokenHolder) {
  auto lhs = parseAdditive(tokenHolder);

  while ((tokenHolder.match(Token::Type::LEFTSHIFT) 
       || tokenHolder.match(Token::Type::RIGHTSHIFT)) 
      && tokenHolder.notAtEnd()) {
    auto op = tokenHolder.consume().type;
    auto rhs = parseAdditive(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      op,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseAdditive(TokenHolder& tokenHolder) {
  auto lhs = parseMultiplicative(tokenHolder);

  while ((tokenHolder.match(Token::Type::ADD) 
       || tokenHolder.match(Token::Type::SUBTRACT)) 
      && tokenHolder.notAtEnd()) {
    auto op = tokenHolder.consume().type;
    auto rhs = parseMultiplicative(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      op,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseMultiplicative(TokenHolder& tokenHolder) {
  auto lhs = parseUnary(tokenHolder);

  while ((tokenHolder.match(Token::Type::MULTIPLY) 
       || tokenHolder.match(Token::Type::DIVIDE)
       || tokenHolder.match(Token::Type::MOD)) 
      && tokenHolder.notAtEnd()) {
    auto op = tokenHolder.consume().type;
    auto rhs = parseUnary(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      op,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseUnary(TokenHolder& tokenHolder) {
  if (tokenHolder.match(Token::Type::SUBTRACT) || tokenHolder.match(Token::Type::BITWISENOT)) {
    tokenHolder.skip();
    auto operand = parseUnary(tokenHolder);
    return operand;
  }

  return parsePrimary(tokenHolder);
}
std::unique_ptr<Program::Expr> Parser::parsePrimary(TokenHolder& tokenHolder) {
  switch (tokenHolder.peek().type) {    
    case Token::Type::IDENTIFIER: {
      return parseIdentifierToExpression(tokenHolder);
    }
    case Token::Type::NUMBER: {
      return std::make_unique<Program::NumberExpr>(parseNumberString(tokenHolder.consume()));
    }
    default:
    return makeErrorExpression(tokenHolder.peek(), std::format("Expected number or identifier token, got \"{}\"", tokenHolder.consume().value));
  }
}

std::unique_ptr<Program::Expr> Parser::makeErrorExpression(const Token& errToken, const std::string& message) {
  logError(errToken, message);
  return std::make_unique<Program::NumberExpr>(0);
}

void Parser::parseLabelDefinition(TokenHolder& tokenHolder, Program& program) {
  auto label = std::make_unique<Program::LabelObject>();

  auto symbolObject = std::make_unique<Program::LabelSymbol>(tokenHolder.peek().location);
  auto symbol = symbolObject.get();
  program.m_statementVector.push_back(std::move(symbolObject));

  Program::LabelObject* parentLabel = nullptr;

  while (tokenHolder.match(Token::Type::PERIOD, 1) && tokenHolder.notAtEnd()) {
    if (getOrCreateLabel(tokenHolder, program, parentLabel, false, symbol)) {
      tokenHolder.skip(); //skip '.'
    } else {
      tokenHolder.skipUntilAfterType(Token::Type::COLON);
      return;
    }
    
  }

  if (!tokenHolder.match(Token::Type::COLON, 1)) {
    logError(tokenHolder.peek(1), std::format("Expected ':', got \"{}\"", tokenHolder.peek(1).value));
    return;
  }

  if (getOrCreateLabel(tokenHolder, program, parentLabel, true, symbol)) {
    tokenHolder.skip(2); // skip idenitifer and ':'
  } else {
    tokenHolder.skip(); // error consumes, skip ':'
  }
}

std::unique_ptr<Program::Expr> Parser::parseIdentifierToExpression(TokenHolder& tokenHolder) {
  auto expr = std::make_unique<Program::IdentifierExpr>();
  while (tokenHolder.match(Token::Type::PERIOD, 1) && tokenHolder.notAtEnd()) {
    expr->identifierPath.push_back(tokenHolder.consume().value);
    tokenHolder.skip();
  }
  return expr;
}

bool Parser::getOrCreateLabel(TokenHolder& tokenHolder, Program& program, Program::LabelObject*& parentLabel, bool createLabel, Program::LabelSymbol* symbol) {
  const auto currentSegmentToken = tokenHolder.peek();
  if (parentLabel == nullptr) {
    const auto it = program.m_identifierMap.find(currentSegmentToken.value);
    if (it == program.m_identifierMap.end()) {
      //create the reference object, without a symbol
      auto reference = createLabel 
        ? std::make_unique<Program::LabelObject>(currentSegmentToken.value, symbol) 
        : std::make_unique<Program::LabelObject>(currentSegmentToken.value);

      program.m_identifierMap.emplace(currentSegmentToken.value, std::move(reference));


    } else if (it->second->isIdentifier()) {
      logError(tokenHolder.consume(), std::format("Identifier \"{}\" is referenced as a label, but is defined a data object.", currentSegmentToken.value));
      return false;
    } else if (it->second->isLabel()) {
      if (createLabel) {
        //refdefined
        logError(tokenHolder.consume(), std::format("Identifier \"{}\" is redefined as a label.", currentSegmentToken.value));
        return false;
      } else {
        parentLabel = static_cast<Program::LabelObject*>(it->second.get());
      }
    }
  } else {
    const auto it = parentLabel->children.find(currentSegmentToken.value);
    if (it == parentLabel->children.end()) {
      //create the reference object, without a symbol
      auto reference = createLabel
        ? std::make_unique<Program::LabelObject>(currentSegmentToken.value, parentLabel, symbol)
        : std::make_unique<Program::LabelObject>(currentSegmentToken.value, parentLabel);
      program.m_identifierMap.emplace(currentSegmentToken.value, std::move(reference));
    } else {
      if (createLabel) {
        logError(tokenHolder.consume(), std::format("Identifier \"{}\" is redefined as a label.", currentSegmentToken.value));
        return false;
      } else {
        parentLabel = it->second.get();
      }
    }
  }

  return true;
}

void Parser::parseDataTypeDeclaration(TokenHolder&, Program&) {

}



}