#include "Spasm/Parser.hpp"

#include <utility>
#include "Helpers/SafeStoi.hpp"

namespace Spasm {
void Parser::ParseTokens(TokenHolder& tokenHolder, Arch::Architecture& arch, Program::TranslationUnit& translationUnit, Program& program, Debug::FullLogger* logger) {
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
  tokenHolder.reset();
  p_logger = logger;

  while (tokenHolder.notAtEnd()) {
    switch (tokenHolder.peek().type) {
      using TY = Token::Type;
      case TY::DIRECTIVE: {
        logError(tokenHolder.peek(), "Preproccessor didn't handle directive.");
        tokenHolder.skip();
        break;
      }
      case TY::IDENTIFIER: {
        parseIdentifier(tokenHolder, arch, translationUnit, program);
        break;
      }
      case TY::NEWLINE: {
        tokenHolder.skip();
        break;
      }
      default: {
        logError(tokenHolder.peek(), "Unexpected token type.");
        tokenHolder.skip();
        break;
      }
    }
  }
}

void Parser::parseIdentifier(TokenHolder& tokenHolder, Arch::Architecture& arch, Program::TranslationUnit& translationUnit, Program& program) {
  // probably doesnt actually need to differ register and format,
  //   and could probably actually use datatypes instead...
  const Token initToken = tokenHolder.peek();
  switch (arch.getKeywordTypeOfWord(initToken.value))
  {
  case Arch::Architecture::KeywordType::INSTRUCTION:
  {
    tokenHolder.skip();
    const auto instructionDefinition = arch.m_instructionSet.find((std::string)initToken.value);
    if (instructionDefinition == arch.m_instructionSet.end()) {logError(initToken, std::format("Unknown instruction \"{}\" referenced.", initToken.value));return;}
    parseInstruction(tokenHolder, initToken, arch, instructionDefinition->second, translationUnit, program);
    break;
  }
  case Arch::Architecture::KeywordType::DATATYPE:
    parseDataTypeDeclaration(tokenHolder, translationUnit, program);
    break;
  default:
    if (initToken.type == Token::Type::IDENTIFIER) {
      parseLabelDefinition(tokenHolder, translationUnit, program);
    } else {
      tokenHolder.skip();
      logError(initToken, std::format("Unexpected keyword type (expected instruction or datatype), \"{}\"", initToken.value));;
    }
    break;
  }
}

void Parser::parseInstruction(TokenHolder& tokenHolder, const Token& instrToken, const Arch::Architecture& arch, const Arch::Architecture::InstructionDefinition& instruction, Program::TranslationUnit& translationUnit, Program& program) {
  auto instructionSymbol = std::make_unique<Program::InstructionSymbol>(instrToken.location, instruction);
  instructionSymbol->operands.reserve(instruction.m_operands.size()); // reserve to prevent resizing overhead
  
  for (const auto& operand : instruction.m_operands) {
    bool skipComma = true;

    std::visit([&](auto& op) {
      using T = std::decay_t<decltype(op)>;

      if constexpr (std::is_same_v<T, Arch::Architecture::ConstantIntOperand>) {
        skipComma = false;
        auto programOperand = std::make_unique<Program::ConstantOperand>(instrToken.location, op.constant);
        instructionSymbol->operands.push_back(std::move(programOperand));
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ConstantStringOperand>) {
        //this is kind of innecifiaent, because per instruction, the constant str will be converted
        skipComma = false;
        instructionSymbol->operands.push_back(std::move(convertConstantStringToOperand(arch, op)));
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::RegisterOperand>) {
        
        const auto [regptr, identifierToken] = parseExpectRegister(tokenHolder, arch);
        
        std::unique_ptr<Program::Operand> programOperand;

        if (!regptr) {
          programOperand = makeErrorOperand(identifierToken, "Unknown register");
        }
        else if (op.acceptedRegisterNames.count(regptr->m_registerName) < 1){
          programOperand = makeErrorOperand(identifierToken, "Register out of range for this field.");
        }
        else {
          programOperand = std::make_unique<Program::RegisterOperand>(identifierToken.location, *regptr);
        }

        instructionSymbol->operands.push_back(std::move(programOperand));

      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ImmediateOperand>) {
        auto programOperand = parseExpectImmediate(tokenHolder);
        //get a ptr to the expression of program operand to give it to the unresolved list
        program.m_unresolvedExpressions.push(static_cast<Program::ExpressionOperand*>(programOperand.get())->expression.get());
        instructionSymbol->operands.push_back(std::move(programOperand));
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ExternalImmediateOperand>) {
        auto programOperand = parseExpectImmediate(tokenHolder);
        //get a ptr to the expression of program operand to give it to the unresolved list
        program.m_unresolvedExpressions.push(static_cast<Program::ExpressionOperand*>(programOperand.get())->expression.get());
        instructionSymbol->operands.push_back(std::move(programOperand));
      }
    }, operand);

    if (tokenHolder.match(Token::Type::COMMA) && skipComma) {
      tokenHolder.skip();
    }
  }

  translationUnit.m_statementVector.push_back(std::move(instructionSymbol));
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
  case Token::NicheType::NUMBER_DEC:{
    auto [num, errm] = std::safe_stoi((std::string)token.value, 10);
    if (!errm.empty()) {
      logError(token, errm);
    }
    return num;}
    break;
  case Token::NicheType::NUMBER_HEX:{
    auto [num, errm] = std::safe_stoi((std::string)token.value, 16);
    if (!errm.empty()) {
      logError(token, errm);
    }
    return num;}
    break;
  case Token::NicheType::NUMBER_BIN:{
    auto [num, errm] = std::safe_stoi((std::string)token.value, 2);
    if (!errm.empty()) {
      logError(token, errm);
    }
    return num;}
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

void Parser::parseLabelDefinition(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, Program& program) {
  
  auto symbolObject = std::make_unique<Program::LabelSymbol>(tokenHolder.peek().location);
  auto symbol = symbolObject.get();
  
  Program::LabelObject* parentLabel = nullptr;
  std::string_view fullName;
  
  while (tokenHolder.match(Token::Type::PERIOD, 1) && tokenHolder.notAtEnd()) {
    if (getOrCreateIdentiferObject(tokenHolder, translationUnit, program, parentLabel, false, symbol, fullName)) {
      tokenHolder.skip(); //skip '.'
    } else {
      tokenHolder.skipUntilAfterType(Token::Type::COLON);
      return;
    }
    
  }
  
  if (!tokenHolder.match(Token::Type::COLON, 1)) {
    logError(tokenHolder.peek(1), std::format("Expected ':', got \"{}\"", tokenHolder.peek(1).value));
    tokenHolder.skip();
    return;
  }
  const auto identifierToken = tokenHolder.peek();
  
  symbol->name = identifierToken.value;

  //dont need????? already done in getOrCreateLabel()????
  // auto label = std::make_unique<Program::LabelObject>(identifierToken.value, translationUnit.m_sourcePath);
  // label->symbolObject = symbol;
  // translationUnit.m_identifierMap.emplace(label->name, std::move(label));

  if (getOrCreateIdentiferObject(tokenHolder, translationUnit, program, parentLabel, true, symbol, fullName)) {
    tokenHolder.skip(2); // skip idenitifer and ':'
  } else {
    tokenHolder.skip(); // error consumes, skip ':'
  }
  translationUnit.m_statementVector.push_back(std::move(symbolObject));
}

//if not isLabel, is definition
void Parser::parseIdentifierNameDefiniton(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, Program& program, bool isLabel) {
  Program::LabelObject* parentLabel = nullptr;
  std::string_view fullName;

  auto shouldContinue = [&]() {
    return isLabel ?
      tokenHolder.notAtEnd() && (!tokenHolder.match(Token::Type::COLON, 1))
    : tokenHolder.notAtEnd() && tokenHolder.match(Token::Type::PERIOD, 1); 
  };

  //

  while (shouldContinue()) {

  }
}

bool Parser::getOrCreatePartialIdentifier(
  TokenHolder& tokenHolder, 
  Program::TranslationUnit& translationUnit, 
  Program& program, 
  Program::LabelObject*& parentLabel, 
  bool creating, 
  Program::StatementSymbol* symbol, 
  std::string_view& fullName,
  bool isLabel
) {
  const auto identifierToken = tokenHolder.peek();
  const auto& name = identifierToken.value;
  //extend the full name to the end of the current segment
  fullName = std::string_view(fullName.data(), name.data() - fullName.data() + name.size());

  auto* pool = parentLabel == nullptr ?
     &translationUnit.m_identifierMap
    :&parentLabel->children;

  const auto it = pool->find(name);
  if (it != pool->end()) {
    //exists
    if (creating) {
      logError(identifierToken, std::format("Identifier \"{}\" already exists.", identifierToken.value));
      return false;
    }    
  } else {
    // doesnt exist
    if (creating) {
      std::unique_ptr<Program::IdentifierObject> reference;
      if (isLabel) {
        reference = std::make_unique<Program::LabelObject>(
          name,
          translationUnit.m_sourcePath,
          fullName,
          symbol
        );
      } else {
        reference = std::make_unique<Program::DataObject>(
          name,
          translationUnit.m_sourcePath,
          fullName,
          symbol
        );
      }
      pool->emplace(name, std::move(reference));
      return true;
    }
  }

  //add symbol-less to pool
  parentLabel = dynamic_cast<Program::LabelObject*>(it->second.get());
  if (parentLabel == nullptr) {
    logError(identifierToken, std::format("Identifier \"{}\" is a data object, and cannot have children.", identifierToken.value));
    return false;
  }
  
  auto reference = std::make_unique<Program::LabelObject>(
    name,
    translationUnit.m_sourcePath,
    fullName
  );
  pool->emplace(name, std::move(reference));

  return true;
  
  // if (parentLabel == nullptr) {
  //   const auto it = translationUnit.m_identifierMap.find(identifierToken.value);
  //   // if is not in the global pool 
  //   if (it == translationUnit.m_identifierMap.end()) {
  //     //create with no symbol
  //     auto reference = std::make_unique<Program::LabelObject>(
  //       identifierToken.value,
  //       translationUnit.m_sourcePath
  //     );

  //     parentLabel = reference.get();

  //     translationUnit.m_identifierMap.emplace(
  //       identifierToken.value, 
  //       std::move(reference)
  //     );
  //   } else {
  //     // if is in the global pool
  //     if (creating) {
  //       // already exists in global pool
  //       logError(identifierToken, std::format("Identifier \"{}\" already exists.", identifierToken.value));
  //       return false;
  //     } else {
  //       // already exists in global pool, but just set to parent
  //       parentLabel = dynamic_cast<Program::LabelObject*>(it->second.get());
  //       if (parentLabel == nullptr) {
  //         logError(identifierToken, std::format("Identifier \"{}\" is a data object, and cannot have children.", identifierToken.value));
  //         return false;
  //       }
  //     }
  //   }
  // } else {
  //   // parent
  //   const auto it = parentLabel->children.find(identifierToken.value);
  //   if (it == parentLabel->children.end()) {
  //     //doesnt exist
  //     if (creating) {
  //       std::unique_ptr<Program::IdentifierObject> reference = isLabel ?
  //         std::make_unique<Program::LabelObject>(
  //           identifierToken.value,
  //           translationUnit.m_sourcePath,
  //           symbol
  //         )
  //       : std::make_unique<Program::DataObject>(
  //           identifierToken.value,
  //           translationUnit.m_sourcePath,
  //           symbol
  //       );

  //       parentLabel->children.emplace(
  //         identifierToken.value, 
  //         std::move(reference)
  //       );
  //     } else {
  //       auto reference =
  //         std::make_unique<Program::LabelObject>(
  //           identifierToken.value,
  //           translationUnit.m_sourcePath
  //       );
        
  //       parentLabel->children.emplace(
  //         identifierToken.value, 
  //         std::move(reference)
  //       );
  //     }
  //   } else {
  //     //does exist
  //     if (creating) {
  //       logError(identifierToken, std::format("Identifier \"{}\" already exists.", identifierToken.value));
  //     } else {
  //       // already exists in parent pool, but just set to parent
  //       parentLabel = dynamic_cast<Program::LabelObject*>(it->second.get());
  //       if (parentLabel == nullptr) {
  //         logError(identifierToken, std::format("Identifier \"{}\" is a data object, and cannot have children.", identifierToken.value));
  //         return false;
  //       }
  //     }
  //   }
  // }

  // return true;
}

std::unique_ptr<Program::Expr> Parser::parseIdentifierToExpression(TokenHolder& tokenHolder) {
  auto expr = std::make_unique<Program::IdentifierExpr>();
  while (tokenHolder.match(Token::Type::PERIOD, 1) && tokenHolder.notAtEnd()) {
    expr->identifierPath.push_back(tokenHolder.consume().value);
    tokenHolder.skip();
  }
  expr->identifierPath.push_back(tokenHolder.consume().value);
  return expr;
}

bool Parser::getOrCreateLabel(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, Program& program, Program::LabelObject*& parentLabel, bool createLabel, Program::LabelSymbol* symbol, std::string_view& fullName) {
  const auto currentSegmentToken = tokenHolder.peek();
  //extend the full name to the end of the current segment
  fullName = std::string_view(fullName.data(), currentSegmentToken.value.data() - fullName.data() + currentSegmentToken.value.size());
  if (parentLabel == nullptr) {
    const auto it = translationUnit.m_identifierMap.find(currentSegmentToken.value);
    if (it == translationUnit.m_identifierMap.end()) {
      //create the reference object, without a symbol
      auto reference = createLabel 
        ? std::make_unique<Program::LabelObject>(currentSegmentToken.value, translationUnit.m_sourcePath, symbol) 
        : std::make_unique<Program::LabelObject>(currentSegmentToken.value, translationUnit.m_sourcePath);

      reference->fullName = fullName;
      translationUnit.m_identifierMap.emplace(currentSegmentToken.value, std::move(reference));


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
      reference->fullName = fullName;
      translationUnit.m_identifierMap.emplace(currentSegmentToken.value, std::move(reference));
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

void Parser::parseDataTypeDeclaration(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, Program& program) {
  if (tokenHolder.peek().value == "BYTE") {
    parseNonArrayDataType(tokenHolder,translationUnit,program,1);
  } else if (tokenHolder.peek().value == "WORD") {
    parseNonArrayDataType(tokenHolder,translationUnit,program,2);
  } else if (tokenHolder.peek().value == "DWORD") {
    parseNonArrayDataType(tokenHolder,translationUnit,program,4);
  } else if (tokenHolder.peek().value == "TEXT") {
    parseArrayDataType(tokenHolder,translationUnit, program, false);
  } else if (tokenHolder.peek().value == "ARRAY") {
    parseArrayDataType(tokenHolder,translationUnit,program, true);
  } else {
    logError(tokenHolder.peek(), "Invalid or unimplemented datatype.");
  }
}

void Parser::parseNonArrayDataType(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, Program& program, size_t byteSize) {
  tokenHolder.skip();
  if (!tokenHolder.match(Token::Type::IDENTIFIER)) {
    logError(tokenHolder.peek(), std::format("Expected identifier, got \"{}\"", tokenHolder.peek().value));
    return;
  }
  
  const auto identifierToken = tokenHolder.consume();

  const auto it = translationUnit.m_identifierMap.find(identifierToken.value);
  if (it != translationUnit.m_identifierMap.end()) {
    logError(identifierToken, std::format("\"{}\" redeclared as datatype.", identifierToken.value));
    return;
  }

  auto dataObject = std::make_unique<Program::DataObject>(identifierToken.value, nullptr, byteSize, 1);
  auto dataPtr = dataObject.get();
  auto dataStatement = std::make_unique<Program::DefinitionSymbol>(identifierToken.location);
  dataPtr->symbolObject = dataStatement.get();
  dataStatement->name = identifierToken.value;
  

  translationUnit.m_statementVector.push_back(std::move(dataStatement));
  translationUnit.m_identifierMap.emplace(identifierToken.value, std::move(dataObject));

  if (!tokenHolder.match(Token::Type::COMMA)) {
    logError(tokenHolder.peek(), std::format("Expected ',', got \"{}\"", tokenHolder.peek().value));
    return;
  }
  tokenHolder.skip();

  bool isSquare = false;
  if (tokenHolder.match(Token::Type::OPENSQUARE)) {
    isSquare = true;
    tokenHolder.skip();
  }
  auto expr = parseSquareExpression(tokenHolder);
  if (isSquare) {
    if (tokenHolder.match(Token::Type::CLOSESQUARE)) {
      tokenHolder.skip();
    } else {
      logError(tokenHolder.peek(), std::format("Expected ']', got \"{}\"", tokenHolder.peek().value));
      return;
    }
  }
  program.m_unresolvedExpressions.push(expr.get());
  dataPtr->exprData.push_back(std::move(expr));
}
//if not array, is text
void Parser::parseArrayDataType(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, Program& program, bool isArray) {
  tokenHolder.skip();
  if (!tokenHolder.match(Token::Type::IDENTIFIER)) {
    logError(tokenHolder.peek(), std::format("Expected identifier, got \"{}\"", tokenHolder.peek().value));
    return;
  }
  const auto identifierToken = tokenHolder.consume();

  const auto it = translationUnit.m_identifierMap.find(identifierToken.value);
  if (it != translationUnit.m_identifierMap.end()) {
    logError(identifierToken, std::format("\"{}\" redeclared as datatype.", identifierToken.value));
    return;
  }

  auto dataObject = std::make_unique<Program::DataObject>(identifierToken.value, nullptr, 0, 1);
  auto dataPtr = dataObject.get();
  auto dataStatement = std::make_unique<Program::DefinitionSymbol>(identifierToken.location);
  dataPtr->symbolObject = dataStatement.get();

  translationUnit.m_statementVector.push_back(std::move(dataStatement));
  translationUnit.m_identifierMap.emplace(identifierToken.value, std::move(dataObject));

  if (!tokenHolder.match(Token::Type::COMMA)) {
    logError(tokenHolder.peek(), std::format("Expected ',', got \"{}\"", tokenHolder.peek().value));
    return;
  }
  tokenHolder.skip();

  //element count section
  bool isAuto = false;
  if (tokenHolder.match(Token::Type::NUMBER)) {
    dataPtr->elementCount = parseNumberString(tokenHolder.consume());
  } else if (tokenHolder.peek().value == "AUTO") {
    isAuto = true;
  } else {
    logError(tokenHolder.peek(), std::format("Expected identifier, got \"{}\"", tokenHolder.peek().value));
    return;
  }

  if (!tokenHolder.match(Token::Type::COMMA)) {
    logError(tokenHolder.peek(), std::format("Expected ',', got \"{}\"", tokenHolder.peek().value));
    return;
  }
  tokenHolder.skip();

  if (isArray) {
    //consume element size
    if (tokenHolder.match(Token::Type::NUMBER)) {
    dataPtr->elementSize = parseNumberString(tokenHolder.consume());
    } else {
      logError(tokenHolder.peek(), std::format("Expected identifier, got \"{}\"", tokenHolder.peek().value));
      return;
    }

    if (!tokenHolder.match(Token::Type::COMMA)) {
    logError(tokenHolder.peek(), std::format("Expected ',', got \"{}\"", tokenHolder.peek().value));
    return;
    }
    tokenHolder.skip();

    parseElementsOfArray(tokenHolder, program, dataPtr);
  } else {
    parseTextData(tokenHolder, program, dataPtr, isAuto);

  }

}

void Parser::parseElementsOfArray(TokenHolder& tokenHolder, Program& program, Program::DataObject* dataPtr) {
  assert(dataPtr!=nullptr);

  if (!tokenHolder.match(Token::Type::OPENBLOCK)) {
  logError(tokenHolder.peek(), std::format("Expected '{{', got \"{}\"", tokenHolder.peek().value));
  return;
  }
  tokenHolder.skip();
  
  while (true) {
    auto expr = parseSquareExpression(tokenHolder);
    program.m_unresolvedExpressions.push(expr.get());
    dataPtr->exprData.push_back(std::move(expr));

    if (!tokenHolder.match(Token::Type::COMMA))
      break;

    tokenHolder.skip();
  }

  if (!tokenHolder.match(Token::Type::CLOSEBLOCK)) {
    logError(tokenHolder.peek(), std::format("Expected '}}', got \"{}\"", tokenHolder.peek().value));
    return;
  }
}
void Parser::parseTextData(TokenHolder& tokenHolder, Program& program, Program::DataObject* dataPtr, bool isAuto) {
  assert(dataPtr!=nullptr);

  const auto valueToken = tokenHolder.peek();
  switch (valueToken.type) {
    case Token::Type::NUMBER: {
      tokenHolder.skip();

      if (isAuto) {
        logError(valueToken, "Auto length cannot be used on a TEXT object with an initialising value.");
        return;
      }

      const auto number = parseNumberString(valueToken);
      dataPtr->data.resize(dataPtr->elementCount * dataPtr->elementSize);
      
      for (int i = 0; i < dataPtr->elementCount; i++) {
        std::memcpy(
          &dataPtr->data[i * dataPtr->elementSize],
          &number,
          std::min(sizeof(number), dataPtr->elementSize)
        );
      }

      break;
    }
    case Token::Type::STRING: {
      tokenHolder.skip();

      size_t pos = 0;
      while (pos < valueToken.value.size()) {
        dataPtr->data.push_back(valueToken.value[pos++]);
      }

      break;
    }
    default: {
      logError(valueToken, std::format("Expected a number or string, got \"{}\"", valueToken.value));
      break;
    }
  }
}

}