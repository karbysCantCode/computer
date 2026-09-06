#include "astParser.hpp"

namespace C
{

ASTObject ASTParser::run(TokenHolder& holder, TranslationUnit& translationUnit) {
  ASTObject object;

  ASTScope& topScope = object.globalScope;

  while (holder.notAtEnd()) {
    topScope.statements.push_back(std::move(parseNode(holder, topScope)));
  };

  return object;
}

std::unique_ptr<ASTNode> ASTParser::parseNode(TokenHolder& holder, ASTScope& scope) {
  const auto& topToken = holder.peek();

  if (topToken.isKeyword()) {
    if (topToken.isKWDS()) {
      return parseDeclaration(holder, scope);
    } else if (topToken.isKWLBL()) {
      return parseCaseStatement(holder,scope);
    } else if (topToken.isKWSEL()) {
      return parseSelectionStatement(holder,scope);
    } else if (topToken.isKWIT()) {
      return parseIterationStatement(holder,scope);
    } else if (topToken.isKWJMP()) {
      return parseJumpStatement(holder,scope);
    } 
  } else if (topToken.type == Token::Type::IDENTIFIER) {
    if (scope.isTypename(topToken.getString())) {
      return parseDeclaration(holder, scope);
    } else if (holder.match(Token::Type::OT_COLON,1)) {
      return parseLabelStatement(holder,scope);
    } else {
      return parseExpressionStatement(holder, scope);
    }
  } else if (topToken.type == Token::Type::OT_SEMICOLON) {
    holder.skip();
    return nullptr; // TODO idk if this is good handling
  }
  return parseExpressionStatement(holder, scope);
}

std::unique_ptr<ASTNode> ASTParser::parseLabelStatement(TokenHolder& holder, ASTScope& scope) {
  auto label = std::make_unique<ASTLabelStatement>();
  label->identifier = &holder.consume();
  holder.skip();
  return label;
}
std::unique_ptr<ASTNode> ASTParser::parseExpressionStatement(TokenHolder& holder, ASTScope& scope) {
  auto expr = parseExpression(holder,scope);
  if (!holder.match(Token::Type::OT_SEMICOLON)) {
    logError(holder.peek(), std::format("Expected ';', got '{}'", holder.peek().value));
    return expr;
  }
  holder.skip();
  return expr;
}
std::unique_ptr<ASTNode> ASTParser::parseCaseStatement(TokenHolder& holder, ASTScope& scope) {
  auto caseStatement = std::make_unique<ASTCaseStatement>();
  switch (holder.peek().type) {
    case Token::Type::KW_LBL_CASE:
      holder.skip();
      caseStatement->expression = parseExpression(holder, scope);
      break;
    case Token::Type::KW_LBL_DEFAULT:
      caseStatement->isDefault = true;
      break;
    default:
      logError(holder.peek(), std::format("Unknown case statement '{}'.", holder.peek().value));
      return nullptr;
      break;
  }

  if (!holder.match(Token::Type::OT_COLON)) {
    logError(holder.peek(), std::format("Expected ':', got '{}'", holder.peek().value));
    return caseStatement;
  }
  holder.skip();
  return caseStatement;
}
std::unique_ptr<ASTNode> ASTParser::parseSelectionStatement(TokenHolder& holder, ASTScope& scope) {
  auto statement = std::make_unique<ASTSelectionStatement>();
  
  Token& tToken = holder.peek();
  if (!holder.match(Token::Type::OT_OPENPAREN)) {
    logError(holder.peek(), std::format("Expected '(', got '{}'", holder.peek().value));
  }
  statement->expression = parseExpression(holder, scope);

  if (!holder.match(Token::Type::OT_OPENBLOCK)) {
    logError(holder.peek(), std::format("Expected '{{', got '{}'", holder.peek().value));
    return statement;
  }
  holder.skip();
  statement->statement = parseScopeUntilCloseBlock(holder, scope);

  switch (tToken.type) {
    case Token::Type::KW_SEL_IF:
      statement->k = ASTSelectionStatement::Kind::IF;
      break;
    case Token::Type::KW_SEL_ELSE:
      logError(tToken, "Else not expected here.");
      break;
    case Token::Type::KW_SEL_SWITCH:
      statement->k = ASTSelectionStatement::Kind::SWITCH;
      break;
    default:
      logError(holder.peek(), std::format("Unknown selection statement '{}'.", tToken.value));
      return nullptr;
      break;  
  }
  if (holder.match(Token::Type::KW_SEL_ELSE)) {
    statement->k = ASTSelectionStatement::Kind::IFELSE;
  }
  return statement;
}
std::unique_ptr<ASTNode> ASTParser::parseIterationStatement(TokenHolder& holder, ASTScope& scope) {
  holder.skip();
  return nullptr;
}
std::unique_ptr<ASTNode> ASTParser::parseJumpStatement(TokenHolder& holder, ASTScope& scope) {
  holder.skip();
  return nullptr;

}

std::unique_ptr<ASTNode> ASTParser::parseScopeUntilCloseBlock(TokenHolder& holder, ASTScope& scope) {
  std::unique_ptr<ASTScope> expr = std::make_unique<ASTScope>();
  expr->parent = &scope;

  while (holder.notAtEnd() && !holder.match(Token::Type::OT_CLOSEBLOCK)) {
    expr->statements.push_back(parseNode(holder, *expr));
  }

  return expr;
}

bool ASTParser::ParsePrimitiveTypeInner(TokenHolder& holder, ASTScope& scope, ASTPrimitiveType& type) {
  while (holder.peek().isKWDS()) {
    const Token& token = holder.consume();
    switch (token.type) {
      case Token::Type::KW_FS_INLINE:
        type.fs.setInline();
      break;
      case Token::Type::KW_TQ_CONST:
        type.tq.setConst();
      break;
      case Token::Type::KW_TQ_RESTRICT:
        type.tq.setRestrict();
      break;
      case Token::Type::KW_TQ_VOLATILE:
        type.tq.setVolatile();
      break;
      case Token::Type::KW_TY_VOID:
        if (type.kind != ASTPrimitiveKind::UNDEFINED) {
          logError(token, "A declaration cannot have multiple primitive type qualifiers.");
          break;
        }
        type.kind = ASTPrimitiveKind::VOID;
      break;
      case Token::Type::KW_TY_CHAR:
        if (type.kind != ASTPrimitiveKind::UNDEFINED) {
          logError(token, "A declaration cannot have multiple primitive type qualifiers.");
          break;
        }
        type.kind = ASTPrimitiveKind::CHAR;
      break;
      case Token::Type::KW_TY_INT:
        if (type.kind != ASTPrimitiveKind::UNDEFINED) {
          logError(token, "A declaration cannot have multiple primitive type qualifiers.");
          break;
        }
        type.kind = ASTPrimitiveKind::INT;
      break;
      case Token::Type::KW_TY_FLOAT:
        if (type.kind != ASTPrimitiveKind::UNDEFINED) {
          logError(token, "A declaration cannot have multiple primitive type qualifiers.");
          break;
        }
        type.kind = ASTPrimitiveKind::FLOAT;
      break;
      case Token::Type::KW_TY_DOUBLE:
        if (type.kind != ASTPrimitiveKind::UNDEFINED) {
          logError(token, "A declaration cannot have multiple primitive type qualifiers.");
          break;
        }
        type.kind = ASTPrimitiveKind::DOUBLE;
      break;
      case Token::Type::KW_TY_LONG:
        if (type.mod.isLong()) {
          type.mod.setLongLong();
          break;
        }
        type.mod.setLong();
      break;
      case Token::Type::KW_TY_SHORT:
        type.mod.setShort();
      break;
      case Token::Type::KW_TY_SIGNED:
        type.mod.setSigned();
      break;
      case Token::Type::KW_TY_UNSIGNED:
        type.mod.setUnsigned();
      break;
      case Token::Type::KW_TY_SU_STRUCT:
        if (type.kind != ASTPrimitiveKind::UNDEFINED) {
          logError(token, "A declaration cannot have multiple primitive type qualifiers.");
          break;
        }
        type.kind = ASTPrimitiveKind::STRUCT;
      break;
      case Token::Type::KW_TY_SU_UNION:
        if (type.kind != ASTPrimitiveKind::UNDEFINED) {
          logError(token, "A declaration cannot have multiple primitive type qualifiers.");
          break;
        }
        type.kind = ASTPrimitiveKind::UNION;
      break;
      case Token::Type::KW_TY_E_ENUM:
        if (type.kind != ASTPrimitiveKind::UNDEFINED) {
          logError(token, "A declaration cannot have multiple primitive type qualifiers.");
          break;
        }
        type.kind = ASTPrimitiveKind::ENUM;
      break;
      case Token::Type::KW_SCS_AUTO:
        if (type.scs.isNone()) {
          type.scs.setAuto();
          break;
        }
        logError(token, "more than one storage class may not be specified");
      break;
      case Token::Type::KW_SCS_EXTERN:
                if (type.scs.isNone()) {
          type.scs.setExtern();
          break;
        }
        logError(token, "more than one storage class may not be specified");
      break;
      case Token::Type::KW_SCS_REGISTER:
                if (type.scs.isNone()) {
          type.scs.setRegister();
          break;
        }
        logError(token, "more than one storage class may not be specified");
      break;
      case Token::Type::KW_SCS_STATIC:
                if (type.scs.isNone()) {
          type.scs.setStatic();
          break;
        }
        logError(token, "more than one storage class may not be specified");
      break;
      case Token::Type::KW_SCS_TYPEDEF:
                if (type.scs.isNone()) {
          type.scs.setTypeDef();
          break;
        }
        logError(token, "more than one storage class may not be specified");
      break;
      default:
      break;
    }
  }
  if (scope.isTypename(holder.peek().getString())) {
    type.userKind = &holder.consume();
    return true;
  }
  return false;
}
ASTPrimitiveType ASTParser::ParsePrimitiveType(TokenHolder& holder, ASTScope& scope) {
  ASTPrimitiveType ptype;
  while (ParsePrimitiveTypeInner(holder, scope, ptype));
  return ptype;
}

std::unique_ptr<ASTDeclarationStatement> ASTParser::parseDeclaration(TokenHolder& holder, ASTScope& scope) {
  auto statement = std::make_unique<ASTDeclarationStatement>();
  statement->primitiveType = ParsePrimitiveType(holder, scope);
  statement->declarators.push_back(parseDeclarator(holder, scope));
  while (holder.match(Token::Type::OT_COMMA)) {
    holder.skip();
    statement->declarators.push_back(parseDeclarator(holder, scope));
  }

  // type def
  if (statement->primitiveType.scs.isTypeDef()) {
    for (const auto& decl : statement->declarators) {
      scope.addSymbol(decl->getIdentifierToken(), statement.get());
    }

    if (!holder.match(Token::Type::OT_SEMICOLON)) {
      logError(holder.peek(), std::format("Expected ';', got '{}'", holder.peek().value));
      holder.skipUntilAfterType(Token::Type::OT_SEMICOLON);
    } else {
      holder.skip();
    }
  }
  //struct
  else if (statement->primitiveType.kind == ASTPrimitiveKind::STRUCT) {
    auto structMembers = std::make_unique<ASTUnionMembers>();
    auto structMembersRef = structMembers.get();
    statement->initialisingNode = std::move(structMembers);

    if (!holder.match(Token::Type::OT_OPENBLOCK)) {
      logError(holder.peek(), std::format("Expected '{{', got '{}'", holder.peek().value));
      holder.skipUntilAfterType(Token::Type::OT_OPENBLOCK);
      return statement;
    }

    while (holder.notAtEnd() && !holder.match(Token::Type::OT_CLOSEBLOCK)) {
      auto member = std::make_unique<ASTStrucctOrUnionMember>();
      member->declarator = parseDeclaration(holder, scope);
      if (holder.match(Token::Type::OT_COLON)) {
        holder.skip();
        member->bitwidth = parseExpression(holder, scope);
      }
      structMembersRef->members.push_back(std::move(member));


      if (!holder.match(Token::Type::OT_SEMICOLON)) {
        logError(holder.peek(), std::format("Expected ';', got '{}'", holder.peek().value));
        holder.skipUntilAfterType(Token::Type::OT_SEMICOLON);
        return statement;
      }
      holder.skip();
    }

    if (!holder.match(Token::Type::OT_CLOSEBLOCK)) {
      logError(holder.peek(), std::format("Expected '}}', got '{}'", holder.peek().value));
      holder.skipUntilAfterType(Token::Type::OT_CLOSEBLOCK);
      return statement;
    }
    holder.skip();
  }
  //union
  else if (statement->primitiveType.kind == ASTPrimitiveKind::UNION) {
    auto unionMembers = std::make_unique<ASTUnionMembers>();
    auto unionMembersRef = unionMembers.get();
    statement->initialisingNode = std::move(unionMembers);

    if (!holder.match(Token::Type::OT_OPENBLOCK)) {
      logError(holder.peek(), std::format("Expected '{{', got '{}'", holder.peek().value));
      holder.skipUntilAfterType(Token::Type::OT_OPENBLOCK);
      return statement;
    }

    while (holder.notAtEnd() && !holder.match(Token::Type::OT_CLOSEBLOCK)) {
      auto member = std::make_unique<ASTStrucctOrUnionMember>();
      member->declarator = parseDeclaration(holder, scope);
      if (holder.match(Token::Type::OT_COLON)) {
        holder.skip();
        member->bitwidth = parseExpression(holder, scope);
      }
      unionMembersRef->members.push_back(std::move(member));
    }

    if (!holder.match(Token::Type::OT_CLOSEBLOCK)) {
      logError(holder.peek(), std::format("Expected '}}', got '{}'", holder.peek().value));
      holder.skipUntilAfterType(Token::Type::OT_CLOSEBLOCK);
      return statement;
    }
    holder.skip();
  }
  //enum
  else if (statement->primitiveType.kind == ASTPrimitiveKind::ENUM) {
    // parse enum
    //hould be {
    auto enumMembers = std::make_unique<ASTEnumMembers>();
    auto enumMembersRef = enumMembers.get();
    statement->initialisingNode = std::move(enumMembers);

    if (!holder.match(Token::Type::OT_OPENBLOCK)) {
      logError(holder.peek(), std::format("Expected '{{', got '{}'", holder.peek().value));
      holder.skipUntilAfterType(Token::Type::OT_OPENBLOCK);
      return statement;
    }

    while (holder.notAtEnd() && !holder.match(Token::Type::OT_CLOSEBLOCK)) {
      auto enumerator = std::make_unique<ASTEnumEnumerator>();
      enumerator->enumerator = &holder.consume();
      if (holder.match(Token::Type::AS_BASIC)) {
        holder.skip();
        enumerator->expression = parseExpression(holder, scope);
        //parse expression to enumenumoertaot expresion
      }
      enumMembersRef->enumerators.push_back(std::move(enumerator));
    }
  }
  // standard declarator
  else if (holder.match(Token::Type::AS_BASIC)) {
    holder.skip();
    statement->initialisingNode = parseExpression(holder, scope);
  } 
  // function declarator
  else if (holder.match(Token::Type::OT_OPENBLOCK)) {
    //validate cond 4 func
    if (statement->declarators.size() != 1) {
      if (statement->declarators.size() < 1) {
        logError(holder.peek(), "Expected one declarator for function (open block parentheses) definition, got none.");
      } else {
        const auto ptr = statement->declarators.front()->getIdentifierToken();
        if (ptr) {
          logError(*ptr, "Expected one declarator for function (open block parentheses) definition, got multiple.");
        } else {
          logError(holder.peek(), "Expected one declarator for function (open block parentheses) definition, got multiple.");
        }
      }
      return statement;
    }
    
    holder.skip();
    statement->initialisingNode = parseScopeUntilCloseBlock(holder, scope);
    if (!holder.match(Token::Type::OT_CLOSEBLOCK)) {
      logError(holder.peek(), std::format("Expected '}}', got '{}'.", holder.peek().value));
    } else {
      holder.skip();
    }
  }
  return statement;
}

std::unique_ptr<ASTExpression> ASTParser::parseExpression(TokenHolder& holder, ASTScope& scope) {
  return std::move(parseAssignmentExpression(holder, scope));
};
std::unique_ptr<ASTExpression> ASTParser::parseAssignmentExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseConditionalExpression(holder, scope);
  while (holder.peek().isAssignment()) {
    auto expr = std::make_unique<ASTAssignmentExpression>();
    expr->left = std::move(left);
    expr->opTypeToken = &holder.consume();
    expr->right = parseConditionalExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseConditionalExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseLogicalOrExpression(holder, scope);
  while (holder.match(Token::Type::OT_QUESTION)) {
    auto expr = std::make_unique<ASTConditionalExpression>();
    expr->eval = std::move(left);
    expr->left = parseExpression(holder, scope);
    if (!holder.match(Token::Type::OT_COLON)) {
      logError(holder.peek(), std::format("Expected ':', got '{}'", holder.peek().value));
    } else {
      holder.skip();
    }
    expr->right = parseLogicalOrExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseLogicalOrExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseLogicalAndExpression(holder, scope);
  while (holder.match(Token::Type::OP_LOGICAL_OR)) {
    auto expr = std::make_unique<ASTBinaryExpression>();
    expr->left = std::move(left);
    expr->opTypeToken = &holder.consume();
    expr->right = parseLogicalAndExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseLogicalAndExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseInclusiveOrExpression(holder, scope);
  while (holder.match(Token::Type::OP_LOGICAL_AND)) {
    auto expr = std::make_unique<ASTBinaryExpression>();
    expr->left = std::move(left);
    expr->opTypeToken = &holder.consume();
    expr->right = parseInclusiveOrExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseInclusiveOrExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseExclusiveOrExpression(holder, scope);
  while (holder.match(Token::Type::OP_INCLUSIVE_OR)) {
    auto expr = std::make_unique<ASTBinaryExpression>();
    expr->left = std::move(left);
    expr->opTypeToken = &holder.consume();
    expr->right = parseExclusiveOrExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseExclusiveOrExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseAndExpression(holder, scope);
  while (holder.match(Token::Type::OP_XOR)) {
    auto expr = std::make_unique<ASTBinaryExpression>();
    expr->left = std::move(left);
    expr->opTypeToken = &holder.consume();
    expr->right = parseAndExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseAndExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseEqualityExpression(holder, scope);
  while (holder.match(Token::Type::MU_AND)) {
    auto expr = std::make_unique<ASTBinaryExpression>();
    expr->left = std::move(left);
    expr->opTypeToken = &holder.consume();
    expr->right = parseEqualityExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseEqualityExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseComparisonExpression(holder, scope);
  while (holder.match({Token::Type::OP_EQUAL,Token::Type::OP_NOTEQUAL})) {
    auto expr = std::make_unique<ASTBinaryExpression>();
    expr->left = std::move(left);
    expr->opTypeToken = &holder.consume();
    expr->right = parseComparisonExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseComparisonExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseShiftExpression(holder, scope);
  while (holder.match({Token::Type::OP_LESSTHAN,Token::Type::OP_LESSTHANOREQUAL,Token::Type::OP_GREATERTHAN,Token::Type::OP_GREATERTHANOREQUAL})) {
    auto expr = std::make_unique<ASTBinaryExpression>();
    expr->left = std::move(left);
    expr->opTypeToken = &holder.consume();
    expr->right = parseShiftExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseShiftExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseAdditiveExpression(holder, scope);
  while (holder.match({Token::Type::OP_SHL,Token::Type::OP_SHR})) {
    auto expr = std::make_unique<ASTBinaryExpression>();
    expr->left = std::move(left);
    expr->opTypeToken = &holder.consume();
    expr->right = parseAdditiveExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseAdditiveExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseMultiplicativeExpression(holder, scope);
  while (holder.match({Token::Type::OP_ADD,Token::Type::OP_SUB})) {
    auto expr = std::make_unique<ASTBinaryExpression>();
    expr->left = std::move(left);
    expr->opTypeToken = &holder.consume();
    expr->right = parseMultiplicativeExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseMultiplicativeExpression(TokenHolder& holder, ASTScope& scope) {
  auto left = parseCastExpression(holder, scope);
  while (holder.match({Token::Type::MU_ASTRIX,Token::Type::OP_DIV,Token::Type::OP_REM})) {
    auto expr = std::make_unique<ASTBinaryExpression>();
    expr->left = std::move(left);
    expr->opTypeToken =&holder.consume();
    expr->right = parseCastExpression(holder, scope);
    left = std::move(expr);
  }
  return left;
}
std::unique_ptr<ASTExpression> ASTParser::parseCastExpression(TokenHolder& holder, ASTScope& scope) {
  if (holder.match(Token::Type::OT_OPENPAREN)
    &&(holder.peek(1).isKWDS()
     ||scope.isTypename(holder.peek(1).getString())
    )) {
    // continue if postfix cast type
    size_t off = 1;
    size_t depth = 1;
    while (holder.notAtEnd(off) && depth > 0) {
      if (holder.match(Token::Type::OT_OPENPAREN, off)) depth ++;
      else if (holder.match(Token::Type::OT_CLOSEPAREN, off)) depth--;
      off++;
    }
    if (holder.match(Token::Type::OT_OPENBLOCK, off)) return parseUnaryExpression(holder, scope);
    //not, continue!
    holder.skip();
    auto expr = std::make_unique<ASTCastExpression>();
    expr->type.type = ParsePrimitiveType(holder, scope);
    expr->type.declarator = parseDeclarator(holder, scope);
    if (!holder.match(Token::Type::OT_CLOSEPAREN)) {
      logError(holder.peek(), std::format("Expected ')', got'{}'.", holder.peek().value));
    } else {
      holder.skip();
    }
    expr->right = parseCastExpression(holder, scope);
    return expr;
  }
  return parseUnaryExpression(holder, scope);
}
std::unique_ptr<ASTExpression> ASTParser::parseUnaryExpression(TokenHolder& holder, ASTScope& scope) {
  Token& token = holder.peek();
  switch (token.type) {
    case Token::Type::OP_NOT:
    case Token::Type::MU_ASTRIX:
    case Token::Type::OP_ADD:
    case Token::Type::OP_SUB:
    case Token::Type::MU_AND:
    case Token::Type::OP_TILDE: {
      holder.skip();
      auto expr = std::make_unique<ASTUnaryExpression>();
      expr->opTypeToken = &token;
      expr->left = parsePostfixExpression(holder, scope, std::move(parsePrimaryExpression(holder, scope)));
      return std::move(expr);
    } 
    case Token::Type::OP_INC:
    case Token::Type::OP_DEC: {
      holder.skip();
      auto expr = std::make_unique<ASTUnaryExpression>();
      expr->opTypeToken = &token;
      expr->left = parseUnaryExpression(holder, scope);
      return std::move(expr);
    }
    case Token::Type::KW_SIZEOF: {
      holder.skip();
      if (! holder.match(Token::Type::OT_OPENPAREN)) {
        logError(holder.peek(), std::format("Expected '(', got '{}'", holder.peek().value));
        holder.skipUntilAfterType(Token::Type::OT_CLOSEPAREN, true);
        return parsePrimaryExpression(holder, scope);
      }
      auto expr = std::make_unique<ASTSizeOfExpression>();
      expr->type.type = ParsePrimitiveType(holder, scope);
      expr->type.declarator = parseDeclarator(holder, scope);
      return std::move(expr);
    }
    default:
    return parsePrimaryExpression(holder, scope);
  }
}
std::unique_ptr<ASTExpression> ASTParser::parsePostfixExpression(TokenHolder& holder, ASTScope& scope, std::unique_ptr<ASTExpression> inner) {
  const Token& token = holder.peek();
  switch (token.type) {
    case Token::Type::OP_INC:
    case Token::Type::OP_DEC: {
      auto expr = std::make_unique<ASTPostIncDecExpression>();
      expr->opTypeToken = &holder.consume();
      expr->expr = std::move(inner);
      inner = std::move(expr);
      break;
    }
    case Token::Type::MA_PERIOD:
    case Token::Type::MA_POINTER: {
      auto expr = std::make_unique<ASTMemberAccessExpression>();
      expr->opTypeToken = &holder.consume();
      if (holder.match(Token::Type::IDENTIFIER)) {
        logError(holder.peek(), std::format("Expected identifier, got '{}'", holder.peek().value));
      } else {
        expr->memberName = &holder.consume();
      }
      inner = std::move(expr);
      break;
    }
    case Token::Type::MA_OPENSQUARE: {
      holder.skip();
      auto expr = std::make_unique<ASTArrayAccessExpression>();
      expr->array = std::move(inner);
      expr->index = parseExpression(holder, scope);
      if (!holder.match(Token::Type::MA_CLOSESQUARE)) {
        logError(holder.peek(), std::format("Expected ']', got '{}'", holder.peek().value));
      } else {
        holder.skip();
      }
      inner = std::move(expr);
      break;
    }
    case Token::Type::OT_OPENPAREN: {
      holder.skip();
      auto expr = std::make_unique<ASTCallExpression>();
      expr->callee = std::move(inner);
      while (!holder.match(Token::Type::OT_CLOSEPAREN) && holder.notAtEnd()) {
        expr->arguments.push_back(parseAssignmentExpression(holder, scope));
        if (holder.match(Token::Type::OT_COMMA)) {
          holder.skip();
        }
      }
      inner = std::move(expr);
      break;
    }
    default:
    break;
  }
  return inner;
}
std::unique_ptr<ASTExpression> ASTParser::parsePrimaryExpression(TokenHolder& holder, ASTScope& scope) {
  Token& token = holder.peek();
  switch (token.type)
  {
  case Token::Type::IDENTIFIER: {
    holder.skip();
    auto expr = std::make_unique<ASTIdentifierExpression>();
    expr->identifier = &token;
    return parsePostfixExpression(holder, scope, std::move(expr));
    break;
  }
  case Token::Type::NM_DEC:
  case Token::Type::NM_BIN:
  case Token::Type::NM_HEX: {
    holder.skip();
    auto expr = std::make_unique<ASTConstantExpression>();
    expr->constant = &token;
    return parsePostfixExpression(holder, scope, std::move(expr));
    break;
  }
  case Token::Type::STRING: {
    holder.skip();
    auto expr = std::make_unique<ASTStringExpression>();
    expr->string = &token;
    return parsePostfixExpression(holder, scope, std::move(expr));
    break;
  }
  case Token::Type::OT_OPENPAREN: {
    holder.skip();
    auto expr = parseExpression(holder, scope);
    if (!holder.match(Token::Type::OT_CLOSEPAREN)) {
      logError(holder.peek(), std::format("Expected ')', got '{}'", holder.peek().value));
    } else {
      holder.skip();
    }
    return parsePostfixExpression(holder, scope, std::move(expr));
    break;
  }

  
  default:
    logError(token, std::format("Expected '(', string, or number, got '{}'", holder.peek().value));
    holder.skip();
    auto expr = std::make_unique<ASTStringExpression>();
    expr->string = &token;
    return parsePostfixExpression(holder, scope, std::move(expr));
    break;
  }
}
std::unique_ptr<ASTExpression> ASTParser::parsePrepostExpression(TokenHolder& holder, ASTScope& scope) {
  if (holder.match(Token::Type::OT_OPENPAREN)
    &&(holder.peek(1).isKWDS()
    ||scope.isTypename(holder.peek(1).getString())
  )) {
    size_t off = 1;
    size_t depth = 1;
    while (holder.notAtEnd(off) && depth > 0) {
      if (holder.match(Token::Type::OT_OPENPAREN, off)) depth ++;
      else if (holder.match(Token::Type::OT_CLOSEPAREN, off)) depth--;
      off++;
    }
    if (!holder.match(Token::Type::OT_OPENBLOCK, off)) return parsePrimaryExpression(holder, scope);

    auto expr = std::make_unique<ASTCompoundLiteralExpression>();
    holder.skip();
    expr->type.type = ParsePrimitiveType(holder, scope);
    expr->type.declarator = parseDeclarator(holder, scope);
    while (holder.notAtEnd() && !holder.match(Token::Type::OT_CLOSEBLOCK)) {
      expr->list.initializers.push_back(std::make_unique<ASTExpressionInitializer>(parseExpression(holder, scope)));
      if (holder.match(Token::Type::OT_COMMA)) holder.skip();
    }
    if (!holder.match(Token::Type::OT_CLOSEBLOCK)) {
      logError(holder.peek(), std::format("Expected '}}', got '{}'", holder.peek().value));
    } else {
      holder.skip();
    }
    return expr;
  }

  return parsePrimaryExpression(holder, scope);
}
/*
brackets
array / function
pointer
identifier
*/

std::unique_ptr<ASTDeclarator> ASTParser::parseDeclarator(TokenHolder& holder,  ASTScope& scope) {
  auto inner = parseDirectDeclarator(holder, scope);
  return parsePointerDeclarator(holder, scope, std::move(inner));
}

std::unique_ptr<ASTDeclarator> ASTParser::parsePointerDeclarator(TokenHolder& holder,  ASTScope& scope, std::unique_ptr<ASTDeclarator> inner) {
  while (holder.match(Token::Type::MU_ASTRIX)) {
    holder.skip();
    if (inner == nullptr) {
      inner = parseDirectDeclarator(holder, scope);
    }
    inner = std::make_unique<ASTPointerDeclarator>(std::move(inner));
  }
  return std::move(inner);
}

std::unique_ptr<ASTDeclarator> ASTParser::parseDirectDeclarator(TokenHolder& holder,  ASTScope& scope) {
  switch (holder.peek().type) {
    case Token::Type::OT_OPENPAREN: {
      holder.skip();
      std::unique_ptr<ASTDeclarator> inner = parseDeclarator(holder, scope);
      if (holder.peek().type != Token::Type::OT_CLOSEPAREN) {
        logError(holder.peek(), "Expected ')'");
        return inner;
      }
      holder.skip();
      return parseSuffixDeclarator(holder, scope, std::move(inner));
    }
    case Token::Type::IDENTIFIER: {
      auto inner = std::make_unique<ASTIdentifierDeclarator>(&holder.consume());
      return parseSuffixDeclarator(holder, scope, std::move(inner));
      break;
    }
    default:
      //logError(holder.peek(), std::format("Expected parentheses or an identifier, got {}", holder.peek().value));
      return nullptr;
  }
}

std::unique_ptr<ASTDeclarator> ASTParser::parseSuffixDeclarator(TokenHolder& holder, ASTScope& scope, std::unique_ptr<ASTDeclarator> inner) {
  switch (holder.peek().type)
  {
  case Token::Type::OT_OPENPAREN: {
    auto func = std::make_unique<ASTFunctionDeclarator>(std::move(inner));
    auto funcPtr = func.get();
    inner = std::move(func);
    holder.skip();
    while (holder.notAtEnd() && !holder.match(Token::Type::OT_CLOSEPAREN)) {
      auto arg = std::make_unique<ASTType>();
      arg->type = ParsePrimitiveType(holder, scope);
      arg->declarator = parseDeclarator(holder, scope);
      if (holder.match(Token::Type::OT_COMMA)) {
        holder.skip();
      }
      funcPtr->arguments.push_back(std::move(arg));
    }
    if (!holder.match(Token::Type::OT_CLOSEPAREN)) {
      logError(holder.peek(), std::format("Expected ')', got '{}'", holder.peek().value));
    } else {
      holder.skip();
    }
    return inner;
    break;
  }

  case Token::Type::MA_OPENSQUARE: {
    auto arr = std::make_unique<ASTArrayDeclarator>(std::move(inner));
    auto arrPtr = arr.get();
    inner = std::move(arr);

    holder.skip();
    arrPtr->lengthExpression = parseExpression(holder, scope);
    if (holder.peek().type != Token::Type::MA_CLOSESQUARE) {
      logError(holder.peek(), "Expected ']'");
      return inner;
    }
    holder.skip();
    return inner;
    break;
  }
  default:
    return inner;
    break;
  }
}
}