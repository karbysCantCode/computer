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
  instructionSymbol->operands.resize(instruction.m_operands.size()); // reserve to prevent resizing overhead

  size_t instructionIndex = 0;
  for (const auto& operand : instruction.m_operands) {
    bool skipComma = true;

    std::visit([&](auto& op) {
      using T = std::decay_t<decltype(op)>;

      if constexpr (std::is_same_v<T, Arch::Architecture::ConstantIntOperand>) {
        skipComma = false;
        auto programOperand = std::make_unique<Program::ConstantOperand>(instrToken.location, op.constant);
        instructionSymbol->operands[instructionIndex] = std::move(programOperand);
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ConstantStringOperand>) {
        //this is kind of innecifiaent, because per instruction, the constant str will be converted
        skipComma = false;
        instructionSymbol->operands[instructionIndex] = convertConstantStringToOperand(arch, op);
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

        instructionSymbol->operands[instructionIndex] = std::move(programOperand);

      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ImmediateOperand>) {
        auto programOperand = parseExpectImmediate(tokenHolder);
        //get a ptr to the expression of program operand to give it to the unresolved list
        translationUnit.m_unresolvedExpressions.push(static_cast<Program::ExpressionOperand*>(programOperand.get())->expression.get());
        instructionSymbol->operands[instructionIndex] = std::move(programOperand);
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ExternalImmediateOperand>) {
        auto programOperand = parseExpectImmediate(tokenHolder);
        //get a ptr to the expression of program operand to give it to the unresolved list
        translationUnit.m_unresolvedExpressions.push(static_cast<Program::ExpressionOperand*>(programOperand.get())->expression.get());
        instructionSymbol->operands[instructionIndex] = std::move(programOperand);
      }
    }, operand);

    if (tokenHolder.match(Token::Type::COMMA) && skipComma) {
      tokenHolder.skip();
    }
    instructionIndex++;
  }

  const auto initPtr = instrToken.value.data();
  const auto& endStrView = tokenHolder.peek().value;
  instructionSymbol->source = {initPtr, static_cast<size_t>(endStrView.data() + endStrView.size() - initPtr)};
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
      auto expr = std::make_unique<Program::IdentifierExpr>(tokenHolder.peek().location, tokenHolder.peek().value);
      return std::make_unique<Program::ExpressionOperand>(tokenHolder.consume().location, std::move(expr));
    }
    case Token::Type::NUMBER: {
      auto expr = std::make_unique<Program::NumberExpr>(tokenHolder.peek().location,parseNumberString(tokenHolder.peek()));
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
  return std::make_unique<Program::ExpressionOperand>(errToken.location, std::make_unique<Program::NumberExpr>(errToken.location, 0));
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
    const auto locTok = tokenHolder.consume();
    auto rhs = parseXor(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      locTok.location,
      Token::Type::BITWISEOR,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseXor(TokenHolder& tokenHolder) {
  auto lhs = parseAnd(tokenHolder);

  while (tokenHolder.match(Token::Type::BITWISEXOR) && tokenHolder.notAtEnd()) {
    const auto locTok = tokenHolder.consume();
    auto rhs = parseAnd(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      locTok.location,
      Token::Type::BITWISEXOR,
      std::move(lhs), std::move(rhs)
    );
  }

  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseAnd(TokenHolder& tokenHolder) {
  auto lhs = parseShift(tokenHolder);

  while (tokenHolder.match(Token::Type::BITWISEAND) && tokenHolder.notAtEnd()) {
    const auto locTok = tokenHolder.consume();
    auto rhs = parseShift(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      locTok.location,
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
    auto opTok = tokenHolder.consume();
    auto rhs = parseAdditive(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      opTok.location,
      opTok.type,
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
    auto opTok = tokenHolder.consume();
    auto rhs = parseMultiplicative(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      opTok.location,
      opTok.type,
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
    auto opTok = tokenHolder.consume();
    auto rhs = parseUnary(tokenHolder);

    lhs = std::make_unique<Program::BinaryExpr>(
      opTok.location,
      opTok.type,
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
      return std::make_unique<Program::NumberExpr>(tokenHolder.peek().location, parseNumberString(tokenHolder.consume()));
    }
    case Token::Type::OPENPAREN: {
      tokenHolder.skip();

      auto expr = parseOr(tokenHolder);

      if (!tokenHolder.match(Token::Type::CLOSEPAREN)) {
        logError(tokenHolder.peek(), std::format("Expected '(', got \"{}\"", tokenHolder.peek().value));
        return std::make_unique<Program::NumberExpr>(tokenHolder.peek().location, 0);
      }

      return expr;
    }
    default:
    return makeErrorExpression(tokenHolder.peek(), std::format("Expected number or identifier token, got \"{}\"", tokenHolder.consume().value));
  }
}

std::unique_ptr<Program::Expr> Parser::makeErrorExpression(const Token& errToken, const std::string& message) {
  logError(errToken, message);
  return std::make_unique<Program::NumberExpr>(errToken.location, 0);
}

void Parser::parseLabelDefinition(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, Program& program) {
  auto statementSymbol = parseIdentifierNameDefiniton(tokenHolder, translationUnit, program, true);
  if (!statementSymbol) return;
  translationUnit.m_statementVector.push_back(std::move(statementSymbol));
}

//if not isLabel, is definition
std::unique_ptr<Program::StatementSymbol> Parser::parseIdentifierNameDefiniton(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, Program& program, bool isLabel) {
  Program::LabelObject* parentLabel = nullptr;
  std::vector<std::string_view> nameSegments;

  auto shouldContinue = [&]() {
    return isLabel ?
      tokenHolder.notAtEnd() && (!tokenHolder.match(Token::Type::COLON, 1) && tokenHolder.match(Token::Type::PERIOD, 1))
    : tokenHolder.notAtEnd() && tokenHolder.match(Token::Type::PERIOD, 1); 
  };

  //

  while (shouldContinue()) {
    if (!getOrCreatePartialIdentifier(
      tokenHolder,
      translationUnit,
      parentLabel,
      false,
      nullptr,
      isLabel,
      nameSegments
    )) {
      // error, skip until name "should" be passed
      if (isLabel) {
        tokenHolder.skipUntilAfterType(Token::Type::COLON);
      } else {
        while (tokenHolder.match(Token::Type::PERIOD, 1)) {
          tokenHolder.skip(2);
        }
        tokenHolder.skip();
      }
      return nullptr;
    }

    if (tokenHolder.match(Token::Type::PERIOD, 1)) {
      tokenHolder.skip(2);
    }
  }

  std::unique_ptr<Program::StatementSymbol> symbol;
  if (isLabel) {
    symbol = std::make_unique<Program::LabelSymbol>(
      tokenHolder.peek().location,
      tokenHolder.peek().value,
      nullptr
    );
  } else {
    symbol = std::make_unique<Program::DefinitionSymbol>(
      tokenHolder.peek().location,
      tokenHolder.peek().value,
      nullptr
    );
  }
  
  getOrCreatePartialIdentifier(
    tokenHolder,
    translationUnit,
    parentLabel,
    true,
    symbol.get(),
    isLabel,
    nameSegments
  );

  tokenHolder.skip(isLabel ? 2 : 1); 

  return symbol;
}

bool Parser::getOrCreatePartialIdentifier(
  TokenHolder& tokenHolder, 
  Program::TranslationUnit& translationUnit, 
  Program::LabelObject*& parentLabel, 
  bool creating, 
  Program::StatementSymbol* symbol, 
  bool isLabel,
  std::vector<std::string_view>& nameSegments
) {
  const auto identifierToken = tokenHolder.peek();
  const auto& name = identifierToken.value;
  nameSegments.push_back(name);


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

    parentLabel = dynamic_cast<Program::LabelObject*>(it->second.get());
    if (parentLabel == nullptr) {
      logError(identifierToken, std::format("Identifier \"{}\" is a data object, and cannot have children. (DB1)", identifierToken.value));
      return false;
    }

    return true;
    
    
  } else {
    // doesnt exist
    if (creating) {
      std::unique_ptr<Program::IdentifierObject> reference;
      if (isLabel) {
        reference = std::make_unique<Program::LabelObject>(
          translationUnit.m_sourcePath,
          symbol
        );
        reference->nameSegments = std::move(nameSegments);
        if (auto labSym = dynamic_cast<Program::LabelSymbol*>(symbol)) {
          labSym->labelObject = static_cast<Program::LabelObject*>(reference.get());
        }
      } else {
        reference = std::make_unique<Program::DataObject>(
          translationUnit.m_sourcePath,
          parentLabel,
          symbol,
          0
        );

        reference->nameSegments = std::move(nameSegments);
        
        if (auto defSym = dynamic_cast<Program::DefinitionSymbol*>(symbol)) {
          defSym->dataObject = static_cast<Program::DataObject*>(reference.get());
        }
      }
      pool->emplace(name, std::move(reference));
      return true;
    }
  }

  //add symbol-less to pool
  const auto originalParentLabel = parentLabel;
  parentLabel = dynamic_cast<Program::LabelObject*>(parentLabel);
  if (!parentLabel && originalParentLabel) {
    logError(identifierToken, std::format("Identifier \"{}\" is a data object, and cannot have children. (DB2)", identifierToken.value));
    return false;
  }
  
  auto reference = std::make_unique<Program::LabelObject>(
    translationUnit.m_sourcePath
  );

  parentLabel = reference.get();
  reference->nameSegments = nameSegments; //copy
  pool->emplace(name, std::move(reference));

  return true;
}

std::unique_ptr<Program::Expr> Parser::parseIdentifierToExpression(TokenHolder& tokenHolder) {
  auto expr = std::make_unique<Program::IdentifierExpr>(tokenHolder.peek().location);
  while (tokenHolder.match(Token::Type::PERIOD, 1) && tokenHolder.notAtEnd()) {
    expr->identifierPath.push(tokenHolder.consume().value);
    tokenHolder.skip();
  }
  expr->identifierPath.push(tokenHolder.consume().value);
  return expr;
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
  
  auto dataStatement = parseIdentifierNameDefiniton(tokenHolder, translationUnit, program, false);
  if (!dataStatement) return;

  auto dataPtr = dynamic_cast<Program::DefinitionSymbol*>(dataStatement.get());
  if (!dataPtr) return;

  dataPtr->dataObject->elementCount = 1;
  dataPtr->dataObject->elementSize = byteSize;
  
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
  translationUnit.m_unresolvedExpressions.push(expr.get());
  dataPtr->dataObject->exprData.push_back(std::move(expr));
  translationUnit.m_definitionVector.emplace_back(dynamic_cast<Program::DefinitionSymbol*>(dataStatement.release()));
}
//if not array, is text
void Parser::parseArrayDataType(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, Program& program, bool isArray) {
  tokenHolder.skip();
  if (!tokenHolder.match(Token::Type::IDENTIFIER)) {
    logError(tokenHolder.peek(), std::format("Expected identifier, got \"{}\"", tokenHolder.peek().value));
    return;
  }
  // const auto identifierToken = tokenHolder.consume();

  // const auto it = translationUnit.m_identifierMap.find(identifierToken.value);
  // if (it != translationUnit.m_identifierMap.end()) {
  //   logError(identifierToken, std::format("\"{}\" redeclared as datatype.", identifierToken.value));
  //   return;
  // }

  auto dataStatement = parseIdentifierNameDefiniton(tokenHolder,translationUnit,program,false);
  if (!dataStatement) return;
  auto dataStatPtr = dynamic_cast<Program::DefinitionSymbol*>(dataStatement.get());
  auto dataPtr = dataStatPtr->dataObject;

  translationUnit.m_definitionVector.emplace_back(dynamic_cast<Program::DefinitionSymbol*>(dataStatement.release()));

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

  if (!tokenHolder.match(Token::Type::COMMA, 1)) {
    logError(tokenHolder.peek(), std::format("Expected ',', got \"{}\"", tokenHolder.peek().value));
    return;
  }
  tokenHolder.skip(2);

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

    parseElementsOfArray(tokenHolder, translationUnit, dataPtr, isAuto);
  } else {
    parseTextData(tokenHolder, dataPtr, isAuto);

  }

}

void Parser::parseElementsOfArray(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, Program::DataObject* dataPtr, bool isAuto) {
  assert(dataPtr!=nullptr);

  if (!tokenHolder.match(Token::Type::OPENBLOCK)) {
  logError(tokenHolder.peek(), std::format("Expected '{{', got \"{}\"", tokenHolder.peek().value));
  return;
  }
  tokenHolder.skip();
  
  //doesnt need a check bc the comma check -> break will exit if is at end! (bc returns \0)
  while (true) {
    auto expr = parseSquareExpression(tokenHolder);
    translationUnit.m_unresolvedExpressions.push(expr.get());
    dataPtr->exprData.push_back(std::move(expr));

    if (!tokenHolder.match(Token::Type::COMMA))
      break;

    tokenHolder.skip();
  }

  if (!tokenHolder.match(Token::Type::CLOSEBLOCK)) {
    logError(tokenHolder.peek(), std::format("Expected '}}', got \"{}\"", tokenHolder.peek().value));
    return;
  }
  if (isAuto) {
    dataPtr->elementCount = dataPtr->exprData.size();
  } else if (dataPtr->exprData.size() != dataPtr->elementCount) { // not auto & cond
    logError(dataPtr->symbolObject->location, std::format("{} elements expected, {} given.", dataPtr->elementCount, dataPtr->exprData.size()));
  }
}
void Parser::parseTextData(TokenHolder& tokenHolder, Program::DataObject* dataPtr, bool isAuto) {
  assert(dataPtr!=nullptr);
  dataPtr->elementSize = 1;

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

      if (isAuto) {
        dataPtr->elementCount = pos;
      }

      break;
    }
    default: {
      logError(valueToken, std::format("Expected a number or string, got \"{}\"", valueToken.value));
      break;
    }
  }

  dataPtr->rawDataValid = true;
}

}