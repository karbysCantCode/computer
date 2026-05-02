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
      case TY::RELAXOR: {
        parseRelaxor(tokenHolder, arch, translationUnit, program);
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
    auto instructionStatement = parseInstruction(tokenHolder, initToken, arch, translationUnit, program);
    if (!instructionStatement) return;
    translationUnit.m_statementVector.push_back(std::move(instructionStatement));
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

std::unique_ptr<Program::InstructionSymbol> Parser::parseInstruction(TokenHolder& tokenHolder, const Token& instrToken, const Arch::Architecture& arch, Program::TranslationUnit& translationUnit, Program& program, size_t expressionAddressOffset) {
  const auto instructionDefinition = arch.m_instructionSet.find((std::string)instrToken.value);
  if (instructionDefinition == arch.m_instructionSet.end()) {
    logError(instrToken, std::format("Unknown instruction \"{}\" referenced.", instrToken.value));
    return {};
  }
  
  auto instructionSymbol = std::make_unique<Program::InstructionSymbol>(instrToken.location, instructionDefinition->second);
  instructionSymbol->operands.resize(instructionDefinition->second.m_operands.size()); // reserve to prevent resizing overhead

  size_t instructionIndex = 0;
  for (const auto& operand : instructionDefinition->second.m_operands) {
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
          programOperand = makeErrorOperand(identifierToken, "Unknown register", &instructionSymbol->addressIndex, 0, &translationUnit.m_identifierMap);
        }
        else if (op.acceptedRegisterNames.count(regptr->m_registerName) < 1){
          programOperand = makeErrorOperand(identifierToken, "Register out of range for this field.", &instructionSymbol->addressIndex, 0, &translationUnit.m_identifierMap);
        }
        else {
          programOperand = std::make_unique<Program::RegisterOperand>(identifierToken.location, *regptr);
        }

        instructionSymbol->operands[instructionIndex] = std::move(programOperand);

      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ImmediateOperand>) {
        auto programOperand = parseExpectImmediate(tokenHolder, translationUnit, &instructionSymbol->addressIndex, expressionAddressOffset);
        //get a ptr to the expression of program operand to give it to the unresolved list
        auto expressionPtr = static_cast<Program::ExpressionOperand*>(programOperand.get())->expression.get();
        expressionPtr->relativeAddressOffset = expressionAddressOffset;
        translationUnit.m_unresolvedExpressions.push(expressionPtr);
        instructionSymbol->operands[instructionIndex] = std::move(programOperand);
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ExternalImmediateOperand>) {
        auto programOperand = parseExpectImmediate(tokenHolder, translationUnit, &instructionSymbol->addressIndex, expressionAddressOffset);
        //get a ptr to the expression of program operand to give it to the unresolved list
        auto expressionPtr = static_cast<Program::ExpressionOperand*>(programOperand.get())->expression.get();
        expressionPtr->relativeAddressOffset = expressionAddressOffset;
        translationUnit.m_unresolvedExpressions.push(expressionPtr);
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
  return instructionSymbol;
}

std::pair<const Arch::Architecture::RegisterDefinition*, Token> Parser::parseExpectRegister(TokenHolder& tokenHolder, const Arch::Architecture& arch) {
  const auto regToken = tokenHolder.consume();
  const auto it = arch.m_registerSet.find((std::string)regToken.value);
    
  return {
      it != arch.m_registerSet.end() ? &it->second : nullptr,
      regToken
  };
}

std::unique_ptr<Program::Operand> Parser::parseExpectImmediate(TokenHolder& tokenHolder, Program::TranslationUnit& translationUnit, size_t* addressIndex, size_t expressionOffset) {
  #define ExpressionArguments addressIndex, expressionOffset, &translationUnit.m_identifierMap
  
  switch (tokenHolder.peek().type) {
    case Token::Type::OPENSQUARE: {
      const auto startToken = tokenHolder.consume();
      auto expr = std::make_unique<Program::ExpressionOperand>(startToken.location, parseSquareExpression(tokenHolder, ExpressionArguments));
      if (!tokenHolder.match(Token::Type::CLOSESQUARE)) {
        return makeErrorOperand(tokenHolder.peek(), std::format("Expected ']', got \"{}\"", tokenHolder.peek().value), ExpressionArguments);
      }
      tokenHolder.skip();
      return expr;
      break;
    }
    case Token::Type::IDENTIFIER: {
      auto expr = std::make_unique<Program::IdentifierExpr>(tokenHolder.peek().location, tokenHolder.peek().value, ExpressionArguments);
      return std::make_unique<Program::ExpressionOperand>(tokenHolder.consume().location, std::move(expr));
    }
    case Token::Type::NUMBER: {
      auto expr = std::make_unique<Program::NumberExpr>(tokenHolder.peek().location,parseNumberString(tokenHolder.peek()), ExpressionArguments);
      return std::make_unique<Program::ExpressionOperand>(tokenHolder.consume().location, std::move(expr));
    }
    case Token::Type::SUBTRACT: {
      return std::make_unique<Program::ExpressionOperand>(tokenHolder.peek().location, parseUnary(tokenHolder, ExpressionArguments));
    }
    case Token::Type::BITWISENOT: {
      return std::make_unique<Program::ExpressionOperand>(tokenHolder.peek().location, parseUnary(tokenHolder, ExpressionArguments));
    }
    case Token::Type::RELATIVEOPERATOR: {
      return std::make_unique<Program::ExpressionOperand>(tokenHolder.peek().location, parseUnary(tokenHolder, ExpressionArguments));
    }
    case Token::Type::ABSOLUTE: {
      return std::make_unique<Program::ExpressionOperand>(tokenHolder.peek().location, parseUnary(tokenHolder, ExpressionArguments));
    }
    default:
      return makeErrorOperand(tokenHolder.peek(), std::format("Expected a constant, got \"{}\"", tokenHolder.consume().value), ExpressionArguments);
    break;
  }

  #undef ExpressionArguments
}

std::unique_ptr<Program::Expr> Parser::parseSquareExpression(ExpressionParserArgumentTypes) {
  return parseBooleanOr(ExpressionParserArguments);
}

std::unique_ptr<Program::Operand> Parser::convertConstantStringToOperand(const Arch::Architecture& arch, const Arch::Architecture::ConstantStringOperand& constOp) {
  const auto it = arch.m_registerSet.find(constOp.constant);
  if (it != arch.m_registerSet.end()) {
    return std::make_unique<Program::RegisterOperand>(SourceLocation("ARCH DEFINED CONST", -1, -1), it->second);
  }
  assert(false);
  //undefined case
}

std::unique_ptr<Program::Operand> Parser::makeErrorOperand(const Token& errToken, const std::string& message, size_t* addressIndex, size_t expressionOffset, Program::IdentifierMapType* identifierMap) {
  logError(errToken, message);
  return std::make_unique<Program::ExpressionOperand>(errToken.location, std::make_unique<Program::NumberExpr>(errToken.location, 0, addressIndex, expressionOffset, identifierMap));
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

std::unique_ptr<Program::Expr> Parser::parseBooleanOr(ExpressionParserArgumentTypes) {
  auto lhs = parseBooleanAnd(ExpressionParserArguments);

  while (tokenHolder.match(Token::Type::COMPARISONOR) && tokenHolder.notAtEnd()) {
    const auto locTok = tokenHolder.consume();
    auto rhs = parseBooleanAnd(ExpressionParserArguments);

    lhs = std::make_unique<Program::BinaryExpr>(
      locTok.location,
      addressIndex,
      expressionOffset,
      identifierMap,
      Token::Type::COMPARISONOR,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}

std::unique_ptr<Program::Expr> Parser::parseBooleanAnd(ExpressionParserArgumentTypes) {
  auto lhs = parseOr(ExpressionParserArguments);

  while (tokenHolder.match(Token::Type::COMPARISONAND) && tokenHolder.notAtEnd()) {
    const auto locTok = tokenHolder.consume();
    auto rhs = parseOr(ExpressionParserArguments);

    lhs = std::make_unique<Program::BinaryExpr>(
      locTok.location,
      ExpressionParserConstructorArguments,
      Token::Type::COMPARISONAND,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}

std::unique_ptr<Program::Expr> Parser::parseOr(ExpressionParserArgumentTypes) {
  auto lhs = parseXor(ExpressionParserArguments);

  while (tokenHolder.match(Token::Type::BITWISEOR) && tokenHolder.notAtEnd()) {
    const auto locTok = tokenHolder.consume();
    auto rhs = parseXor(ExpressionParserArguments);

    lhs = std::make_unique<Program::BinaryExpr>(
      locTok.location,
      ExpressionParserConstructorArguments,
      Token::Type::BITWISEOR,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseXor(ExpressionParserArgumentTypes) {
  auto lhs = parseAnd(ExpressionParserArguments);

  while (tokenHolder.match(Token::Type::BITWISEXOR) && tokenHolder.notAtEnd()) {
    const auto locTok = tokenHolder.consume();
    auto rhs = parseAnd(ExpressionParserArguments);

    lhs = std::make_unique<Program::BinaryExpr>(
      locTok.location,
      ExpressionParserConstructorArguments,
      Token::Type::BITWISEXOR,
      std::move(lhs), std::move(rhs)
    );
  }

  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseAnd(ExpressionParserArgumentTypes) {
  auto lhs = parseEquality(ExpressionParserArguments);

  while (tokenHolder.match(Token::Type::BITWISEAND) && tokenHolder.notAtEnd()) {
    const auto locTok = tokenHolder.consume();
    auto rhs = parseEquality(ExpressionParserArguments);

    lhs = std::make_unique<Program::BinaryExpr>(
      locTok.location,
      ExpressionParserConstructorArguments,
      Token::Type::BITWISEAND,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseEquality(ExpressionParserArgumentTypes) {
  auto lhs = parseComparison(ExpressionParserArguments);

  while ((tokenHolder.match(Token::Type::NOTEQUAL) 
       || tokenHolder.match(Token::Type::EQUAL))
      && tokenHolder.notAtEnd()) {
    auto opTok = tokenHolder.consume();
    auto rhs = parseComparison(ExpressionParserArguments);

    lhs = std::make_unique<Program::BinaryExpr>(
      opTok.location,
      ExpressionParserConstructorArguments,
      opTok.type,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseComparison(ExpressionParserArgumentTypes) {
  auto lhs = parseShift(ExpressionParserArguments);

  while ((tokenHolder.match(Token::Type::LESSTHAN) 
       || tokenHolder.match(Token::Type::LESSTHANOREQUAL)
       || tokenHolder.match(Token::Type::GREATERTHAN)
       || tokenHolder.match(Token::Type::GREATERTHANOREQUAL)) 
      && tokenHolder.notAtEnd()) {
    auto opTok = tokenHolder.consume();
    auto rhs = parseShift(ExpressionParserArguments);

    lhs = std::make_unique<Program::BinaryExpr>(
      opTok.location,
      ExpressionParserConstructorArguments,
      opTok.type,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseShift(ExpressionParserArgumentTypes) {
  auto lhs = parseAdditive(ExpressionParserArguments);

  while ((tokenHolder.match(Token::Type::LEFTSHIFT) 
       || tokenHolder.match(Token::Type::RIGHTSHIFT)) 
      && tokenHolder.notAtEnd()) {
    auto opTok = tokenHolder.consume();
    auto rhs = parseAdditive(ExpressionParserArguments);

    lhs = std::make_unique<Program::BinaryExpr>(
      opTok.location,
      ExpressionParserConstructorArguments,
      opTok.type,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseAdditive(ExpressionParserArgumentTypes) {
  auto lhs = parseMultiplicative(ExpressionParserArguments);

  while ((tokenHolder.match(Token::Type::ADD) 
       || tokenHolder.match(Token::Type::SUBTRACT)) 
      && tokenHolder.notAtEnd()) {
    auto opTok = tokenHolder.consume();
    auto rhs = parseMultiplicative(ExpressionParserArguments);

    lhs = std::make_unique<Program::BinaryExpr>(
      opTok.location,
      ExpressionParserConstructorArguments,
      opTok.type,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseMultiplicative(ExpressionParserArgumentTypes) {
  auto lhs = parseUnary(ExpressionParserArguments);

  while ((tokenHolder.match(Token::Type::MULTIPLY) 
       || tokenHolder.match(Token::Type::DIVIDE)
       || tokenHolder.match(Token::Type::MOD)) 
      && tokenHolder.notAtEnd()) {
    auto opTok = tokenHolder.consume();
    auto rhs = parseUnary(ExpressionParserArguments);

    lhs = std::make_unique<Program::BinaryExpr>(
      opTok.location,
      ExpressionParserConstructorArguments,
      opTok.type,
      std::move(lhs), std::move(rhs)
    );
  }
  return lhs;
}
std::unique_ptr<Program::Expr> Parser::parseUnary(ExpressionParserArgumentTypes) {
  if (tokenHolder.match(Token::Type::ABSOLUTE) || tokenHolder.match(Token::Type::RELATIVEOPERATOR) || tokenHolder.match(Token::Type::SUBTRACT) || tokenHolder.match(Token::Type::BITWISENOT)) {
    auto opTok = tokenHolder.consume();
    auto operand = parseUnary(ExpressionParserArguments);
    return std::make_unique<Program::UnaryExpr>(opTok.location, ExpressionParserConstructorArguments, opTok.type, std::move(operand));
  }

  return parsePrimary(ExpressionParserArguments);
}

std::unique_ptr<Program::Expr> Parser::parsePrimary(ExpressionParserArgumentTypes) {
  switch (tokenHolder.peek().type) {    
    case Token::Type::IDENTIFIER: {
      return parseIdentifierToExpression(ExpressionParserArguments);
    }
    case Token::Type::NUMBER: {
      return std::make_unique<Program::NumberExpr>(tokenHolder.peek().location, parseNumberString(tokenHolder.consume()), ExpressionParserConstructorArguments);
    }
    case Token::Type::OPENPAREN: {
      tokenHolder.skip();

      auto expr = parseSquareExpression(ExpressionParserArguments);

      if (!tokenHolder.match(Token::Type::CLOSEPAREN)) {
        logError(tokenHolder.peek(), std::format("Expected '(', got \"{}\"", tokenHolder.peek().value));
        return std::make_unique<Program::NumberExpr>(tokenHolder.peek().location, 0, ExpressionParserConstructorArguments);
      }
      tokenHolder.skip();

      return expr;
    }
    default:
    return makeErrorExpression(tokenHolder.peek(), std::format("Expected number or identifier token, got \"{}\"", tokenHolder.consume().value), ExpressionParserArguments);
  }
}

std::unique_ptr<Program::Expr> Parser::makeErrorExpression(const Token& errToken, const std::string& message, ExpressionParserArgumentTypes) {
  logError(errToken, message);
  return std::make_unique<Program::NumberExpr>(errToken.location, 0, addressIndex, expressionOffset, identifierMap);
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
    : &parentLabel->children;

  const auto it = pool->find(name);
  if (it != pool->end()) {
    //exists
    if (creating) {
      logError(identifierToken, std::format("Identifier \"{}\" already exists.", identifierToken.value));
      return false;
    }  

    parentLabel = dynamic_cast<Program::LabelObject*>(*it->second);
    if (parentLabel == nullptr) {
      logError(identifierToken, std::format("Identifier \"{}\" is a data object, and cannot have children. (DB1)", identifierToken.value));
      return false;
    }

    return true;
    
    
  } else {
    // doesnt exist
    if (creating) {
      if (isLabel) {
        auto reference = std::make_unique<Program::LabelObject>(
          translationUnit.m_sourcePath,
          symbol
        );
        reference->nameSegments = std::move(nameSegments);
        auto uniquePtr = std::make_unique<Program::IdentifierObject*>(reference.get());
        pool->emplace(name, uniquePtr.get());
        translationUnit.m_identifierFullNameMap.emplace(reference->fullName(), uniquePtr.get());
        translationUnit.m_identifierObjectPtrHolder.push_back(std::move(uniquePtr));
        if (auto labSym = dynamic_cast<Program::LabelSymbol*>(symbol)) {
          labSym->labelObject = std::move(reference);
        }
      } else {
        auto reference = std::make_unique<Program::DataObject>(
          translationUnit.m_sourcePath,
          parentLabel,
          symbol,
          0
        );

        reference->nameSegments = std::move(nameSegments);
        
        
        auto uniquePtr = std::make_unique<Program::IdentifierObject*>(reference.get());
        pool->emplace(name, uniquePtr.get());
        translationUnit.m_identifierFullNameMap.emplace(reference->fullName(), uniquePtr.get());
        translationUnit.m_identifierObjectPtrHolder.push_back(std::move(uniquePtr));
        if (auto defSym = dynamic_cast<Program::DefinitionSymbol*>(symbol)) {
          defSym->dataObject = std::move(reference);
        }
      }
      //pool->emplace(name, std::move(reference));
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
    translationUnit.m_sourcePath,
    parentLabel
  );

  assert(reference.get() != nullptr);

  parentLabel = reference.get();
  assert(reference.get() != nullptr);
  reference->nameSegments = nameSegments; //copy
  assert(reference.get() != nullptr);
  auto uniquePtr = std::make_unique<Program::IdentifierObject*>(reference.get());
  assert(reference.get() != nullptr);
  pool->emplace(name, uniquePtr.get());
  assert(reference.get() != nullptr);
  std::cout << "reference ptr = " << reference << "\n";
  const auto nme = reference->fullName();
  translationUnit.m_identifierFullNameMap.emplace(
    nme, 
    uniquePtr.get()
  );
  assert(reference.get() != nullptr);
  translationUnit.m_identifierObjectPtrHolder.push_back(std::move(uniquePtr));
  translationUnit.m_identifierObjectUndefinedHolder.push_back(std::move(reference));

  return true;
}

std::unique_ptr<Program::Expr> Parser::parseIdentifierToExpression(ExpressionParserArgumentTypes) {
  auto expr = std::make_unique<Program::IdentifierExpr>(tokenHolder.peek().location, ExpressionParserConstructorArguments);
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

  dataPtr->dataObject->addressIndex = dataPtr->addressIndex;
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
  auto expr = parseSquareExpression(tokenHolder, &dataStatement->addressIndex, 0, &translationUnit.m_identifierMap);
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
  auto dataPtr = dataStatPtr->dataObject.get();

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
    auto expr = parseSquareExpression(tokenHolder, &dataPtr->symbolObject->addressIndex, 0, &translationUnit.m_identifierMap);
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
      
      for (size_t i = 0; i < dataPtr->elementCount; i++) {
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

void Parser::parseRelaxor(TokenHolder& tokenHolder, Arch::Architecture& arch, Program::TranslationUnit& translationUnit, Program& program) {
  if (!tokenHolder.matchNiche(Token::NicheType::RELAXOR_IF)) {
    logError(tokenHolder.peek(), "Relaxor statements can only begin with 'if'");
    tokenHolder.skip();
    return;
  }

  auto relaxor = std::make_unique<Program::RelaxorSymbol>(tokenHolder.peek().location);
  program.m_relaxorPointerVector.push_back(relaxor.get());
  
  while (isRelaxorConditional(tokenHolder.peek().nicheType)) {
    tokenHolder.skip();
    Program::RelaxorDefinition::RelaxorOptionPair& option = relaxor->relaxor.options.emplace_back();
    parseRelaxorCondition(tokenHolder, option, &relaxor->addressIndex, translationUnit);
    parseRelaxorCodeBlock(tokenHolder, *relaxor.get(), option, arch, translationUnit, program);
  }
  
  if (tokenHolder.peek().nicheType == Token::NicheType::RELAXOR_ELSE) {
    tokenHolder.skip();
    Program::RelaxorDefinition::RelaxorOptionPair& option = relaxor->relaxor.options.emplace_back();
    
    option.conditionExpr = std::make_unique<Program::NumberExpr>(relaxor->location, 0, &relaxor->addressIndex, 0, &translationUnit.m_identifierMap);
    option.conditionExpr->value = 1;
    option.conditionExpr->setEvaluated();

    parseRelaxorCodeBlock(tokenHolder, *relaxor.get(), option, arch, translationUnit, program);
  }

  translationUnit.m_statementVector.push_back(std::move(relaxor));
}

bool Parser::isRelaxorConditional(Token::NicheType type) {
  return type == Token::NicheType::RELAXOR_IF ||
          type == Token::NicheType::RELAXOR_ELIF;
}

void Parser::parseRelaxorCondition(TokenHolder& tokenHolder, Program::RelaxorDefinition::RelaxorOptionPair& option, size_t* addressIndex, Program::TranslationUnit& translationUnit) {
  if (!tokenHolder.match(Token::Type::OPENPAREN)) {
    logError(tokenHolder.peek(), std::format("Expected '(', got \"{}\"", tokenHolder.peek().value));
    return;
  }

  tokenHolder.skip(); // skip init '('

  option.conditionExpr = parseSquareExpression(tokenHolder, addressIndex, 0, &translationUnit.m_identifierMap);
  // std::ostringstream ss;
  // option.conditionExpr->print(ss);
  std::cout << "EXPR:" << option.conditionExpr->toString() << std::endl;
  translationUnit.m_unresolvedExpressions.push(option.conditionExpr.get());
  if (!tokenHolder.match(Token::Type::CLOSEPAREN)) {
    logError(tokenHolder.peek(), std::format("Expected ')', got \"{}\"", tokenHolder.peek().value));
    return;
  }

  tokenHolder.skip(); // skip final ')'
}

void Parser::parseRelaxorCodeBlock(TokenHolder& tokenHolder, Program::RelaxorSymbol& relaxor, Program::RelaxorDefinition::RelaxorOptionPair& option, Arch::Architecture& arch, Program::TranslationUnit& translationUnit, Program& program) {
  if (!tokenHolder.match(Token::Type::OPENBLOCK)) {
    logError(tokenHolder.peek(), std::format("Expected '{{', got \"{}\"", tokenHolder.peek().value));
    return;
  }

  tokenHolder.skip();

  size_t currentCaseByteSize = 0;

  while (tokenHolder.notAtEnd() && !tokenHolder.match(Token::Type::CLOSEBLOCK)) {
    const auto& instrtoken = tokenHolder.peek();
    if (instrtoken.type != Token::Type::IDENTIFIER) {
      tokenHolder.skip();
      continue;
    }

    std::unique_ptr<Program::StatementSymbol> statement;
    if (tokenHolder.match(Token::Type::PERIOD, 1)) {
      //label defintion
      statement = parseIdentifierNameDefiniton(tokenHolder, translationUnit, program, true);
    } else {
      //instruction
      tokenHolder.skip();
      statement = parseInstruction(tokenHolder, instrtoken, arch, translationUnit, program, currentCaseByteSize);
      if (!statement) continue;
      currentCaseByteSize += static_cast<Program::InstructionSymbol*>(statement.get())->instruction.m_byteLength;
    }
    if (statement) {
      statement->addressIndex = relaxor.addressIndex;
      option.optionStatements.push_back(std::move(statement));
    }
  }

  option.setCachedSize(currentCaseByteSize);

  relaxor.relaxor.worstCaseSize = std::max(currentCaseByteSize, relaxor.relaxor.worstCaseSize);

  if (!tokenHolder.match(Token::Type::CLOSEBLOCK)) {
    logError(tokenHolder.peek(), std::format("Expected '}}', got \"{}\"", tokenHolder.peek().value));
    return;
  }
  tokenHolder.skip();
}

}