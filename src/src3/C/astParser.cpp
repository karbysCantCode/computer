#include "astParser.hpp"

namespace C
{

ASTObject ASTParser::run(TokenHolder& holder, TranslationUnit& TranslationUnit) {
  ASTObject object;

  std::stack<CompoundStatement*> scopeStack;
  scopeStack.push(&object.global);

  while (holder.notAtEnd()) {
    const auto& topToken = holder.consume();

    if (topToken.isKeyword()) {
      if (topToken.isKWDS()) {
        parseDeclaration(topToken, holder, *scopeStack.top());
      } 
    } else if (topToken.type == Token::Type::IDENTIFIER) {
      AbstractDeclaration* declaration = scopeStack.top()->findDeclarationName(topToken.value);
      if (declaration != nullptr) {
        parseDeclaration(topToken, holder, *scopeStack.top(), declaration);
      }
    }
  };

  return object;
}

bool ASTParser::isType(CompoundStatement& scope, Token& token) const {

}

void ASTParser::parseDeclaration(const Token& firstToken, TokenHolder& holder, CompoundStatement& scope, AbstractDeclaration* declaration) {
  DeclarationSpecifierHolder specs(declaration);

  bool wasSpecErr = false;
  if (specs.decl == nullptr) {
    auto [error, end] = parseDeclarationSpecifier(
      firstToken, 
      scope, 
      specs
    );
    bool exit = end || error;
    wasSpecErr = error;
    while (specs.decl == nullptr && !exit) {
      holder.skip();
      auto [ierror, iend] = parseDeclarationSpecifier(
        holder.peek(), 
        scope, 
        specs
      );
      exit = iend || ierror;
      wasSpecErr = ierror;
    }
  }

  auto declarators = parseDeclarators(holder);

}

std::unique_ptr<ASTParser::AbstractDeclarator> ASTParser::parseDeclarator(TokenHolder& holder) {
  auto ret = parseDirectDeclarator(holder);
  return parsePointerDeclarator(holder, std::move(ret));
}
std::unique_ptr<ASTParser::AbstractDeclarator> ASTParser::parsePointerDeclarator(TokenHolder& holder, std::unique_ptr<ASTParser::AbstractDeclarator> inner) {
  while (holder.match(Token::Type::MU_ASTRIX)) {
    inner = std::make_unique<PointerDeclarator>(std::move(inner));
  }
  return inner;
}
std::unique_ptr<ASTParser::AbstractDeclarator> ASTParser::parseDirectDeclarator(TokenHolder& holder) {
  std::unique_ptr<ASTParser::AbstractDeclarator> ret;
  switch (holder.peek().type)
  {
  case Token::Type::OT_OPENPAREN:
    break;
  case Token::Type::MA_OPENSQUARE:
    break;
  case Token::Type::IDENTIFIER:
    break;
  case Token::Type::OT_OPENPAREN:
    break;
  
  default:
    break;
  }
}


// leaves you at the initializer, so either = or ;
std::vector<std::unique_ptr<ASTParser::AbstractDeclarator>> ASTParser::parseDeclarators(TokenHolder& holder) {
  std::vector<std::unique_ptr<ASTParser::AbstractDeclarator>> declarators;

  bool escape = false;
  while (!escape) { //outer, for comma decls.

    bool innerend = false;
    int right = 0;
    int left = 0;

    while (!holder.match(Token::Type::IDENTIFIER,right) && holder.notAtEnd(right)) 
      right++;
    if (holder.isAtEnd(right)) {
      escape = true;
      break;
    }
    left = right == 0 ? 0 : right - 1;
    const Token& identifier = holder.peek(right);
    std::unique_ptr<ASTParser::AbstractDeclarator> currentTop = std::make_unique<IdentifierDeclarator>(&identifier);
    right++;

    while (!escape && !innerend) {
      switch (holder.peek(right).type) {
        case Token::Type::OT_CLOSEPAREN: {
          if (holder.match(Token::Type::MU_ASTRIX, left)) {
            currentTop = std::make_unique<PointerDeclarator>(&currentTop);
          }
          right++; // go over the ) and look at the next
          int pdepth = 0;
          while (left >= 0 && pdepth >= 0) {
            switch (holder.peek(left).type) {
              case Token::Type::OT_CLOSEPAREN:
                pdepth++;
                break;
              case Token::Type::OT_OPENPAREN:
                pdepth--;
                break;
              default:
                break;
            }
            left--;
          }
          if (!(pdepth < 0)) {
            logError(holder.peek(right-1), "Missing '('");
            break; // ???????
          }

          

          break;
        }
        case Token::Type::OT_OPENPAREN: {
          right++;
          size_t begin = holder.getIndex(right);
          int pdepth = 0;
          while (holder.notAtEnd(right) && pdepth >= 0) {
            switch (holder.peek(right).type) {
              case Token::Type::OT_CLOSEPAREN:
                pdepth--;
                break;
              case Token::Type::OT_OPENPAREN:
                pdepth++;
                break;
              default:
                break;
            }
            right++;
          }
          size_t length = right == begin ? 0 : right - begin;
          currentTop = std::make_unique<FunctionDeclarator>(&currentTop, begin, length);
          if (holder.match(Token::Type::MU_ASTRIX, left)) {
            currentTop = std::make_unique<PointerDeclarator>(&currentTop);
            left--;
          }
          break;
        }
        case Token::Type::MA_OPENSQUARE: {
          right++;
          size_t begin = holder.getIndex(right);
          int pdepth = 0;
          while (holder.notAtEnd(right) && pdepth >= 0) {
            switch (holder.peek(right).type) {
              case Token::Type::MA_CLOSESQUARE:
                pdepth++;
                break;
              case Token::Type::MA_OPENSQUARE:
                pdepth--;
                break;
              default:
                break;
            }
            right++;
          }
          size_t length = right == begin ? 0 : right - begin;
          currentTop = std::make_unique<ArrayDeclarator>(&currentTop, nullptr, begin, length);
          if (holder.match(Token::Type::MU_ASTRIX, left)) {
            currentTop = std::make_unique<PointerDeclarator>(&currentTop);
            left--;
          }

          break;
        }
        default:
          if (holder.match(Token::Type::MU_ASTRIX, left)) {
            currentTop = std::make_unique<PointerDeclarator>(&currentTop);
            left--;
          }
          innerend = true;


      }
    }
    
    declarators.push_back(std::move(currentTop));
    if (holder.match(Token::Type::OT_COMMA, right)) {
      holder.skip(right+1);
    } else {
      escape = true;
    }
  }

  return declarators;
}

TokenHolder ASTParser::collectDeclaratorsInPrecedence(TokenHolder& holder) {

  while (true) {

  }

  return std::move(holder);
}
// return if error, is at end (ie declaration pointer parsing time)
std::pair<bool, bool> ASTParser::parseDeclarationSpecifier(const Token& token, CompoundStatement& scope, DeclarationSpecifierHolder& specs) {
  if (token.isKWSCS()) {
    switch (token.type) {
      case Token::Type::KW_SCS_AUTO:
        if (specs.storageSpecifier != Token::Type::INVALID) {
          logError(token, "auto not allowed here.");
          return {true, false};
        }
        specs.storageSpecifier = Token::Type::KW_SCS_AUTO;
        return {false, false};
        break;
      case Token::Type::KW_SCS_EXTERN:
        if (specs.storageSpecifier != Token::Type::INVALID) {
          logError(token, "extern not allowed here.");
          return {true, false};
        }
        specs.storageSpecifier = Token::Type::KW_SCS_EXTERN;
        return {false, false};
        break;
      case Token::Type::KW_SCS_REGISTER:
        if (specs.storageSpecifier != Token::Type::INVALID) {
          logError(token, "register not allowed here.");
          return {true, false};
        }
        specs.storageSpecifier = Token::Type::KW_SCS_REGISTER;
        return {false, false};
        break;
      case Token::Type::KW_SCS_STATIC:
        if (specs.storageSpecifier != Token::Type::INVALID) {
          logError(token, "static not allowed here.");
          return {true, false};
        }
        specs.storageSpecifier = Token::Type::KW_SCS_STATIC;
        return {false, false};
        break;
      case Token::Type::KW_SCS_THREAD_LOCAL:
        logWarning(token, "_thread_local unsupported, ignored for now.");
        return {false, false};
        break;
      case Token::Type::KW_SCS_TYPEDEF:
        if (specs.storageSpecifier != Token::Type::INVALID) {
          logError(token, "typedef not allowed here.");
          return {true, false};
        }
        specs.storageSpecifier = Token::Type::KW_SCS_TYPEDEF;
        return {false, false};
        break;
      default:
        logError(token, "INTERNAL ERROR: AST PARSER.CPP, UNHANDLED STORAGE CLASS SPECIFIER.");
        return {true, false};
        break;
    }
  } else if (token.isKWTQ()) {
    switch (token.type)
    {
    case Token::Type::KW_TQ_RESTRICT:
      logError(token, std::format("restrict not allowed here. Also not supported by this compiler."));
      return {true, false};
      break;
    case Token::Type::KW_TQ_CONST:
      specs._const = true;
      return {false, false};
      break;
    case Token::Type::KW_TQ_VOLATILE:
      specs._volatile = true;
      return {false, false};
      break;
    
    default:
      logError(token, "INTERNAL ERROR: AST PARSER.CPP, UNHANDLED KW TQ.");
      return {true, false}; 
      break;
    }
  } else if (token.isKWTY()) {
    if (specs.decl != nullptr) {
      logError(token, std::format("{} not allowed here.", token.value));
      return {true, false};
    }

    if (token.isKWTYMULTI()) {
      // conflicting multis will error outside of here
      switch (token.type)
      {
      case Token::Type::KW_TY_LONG:
        switch (specs.typeSpecifier)
        {
        case Token::Type::INVALID:
          specs._longA = true;
          specs.typeSpecifier = Token::Type::KW_TY_LONG;
          break;
        case Token::Type::KW_TY_LONG:
          specs._longB = true;
          break;
        
        default:
          logError(token, "long not allowed here.");
          return {true, false};
          break;
        }
        return {false, false};
        break;
      case Token::Type::KW_TY_SHORT:
        switch (specs.typeSpecifier) {
          case Token::Type::INVALID:
            specs._short = true;
            specs.typeSpecifier = Token::Type::KW_TY_SHORT;
            break;
          case Token::Type::KW_TY_SIGNED:
          case Token::Type::KW_TY_UNSIGNED:
            specs._short = true;
            specs.typeSpecifier = Token::Type::KW_TY_SHORT;
            break;
          default:
            logError(token, "short not allowed here.");
            return {true, false};
            break;
        }
        return {false, false};
        break;
      case Token::Type::KW_TY_SIGNED:
        if (specs.typeSpecifier != Token::Type::INVALID) {
          logError(token, "short not allowed here.");
          return {true, false};
        }
        specs._signed = true;
        return {false, false};
        break;
      case Token::Type::KW_TY_UNSIGNED:
        if (specs.typeSpecifier != Token::Type::INVALID) {
          logError(token, "short not allowed here.");
          return {true, false};
        }
        specs._unsigned = true;
        return {false, false};
        break;
      
      default:
        logError(token, "INTERNAL ERROR: AST PARSER.CPP, UNACCOUNTED MULTI TYPE SPECIFIER.");
        return {true, false};
        break;
      }
    } else if (token.type == Token::Type::KW_TY_STRUCT) {
      specs._struct = true;
      return {false, true};
    } else if (token.type == Token::Type::KW_TY_UNION) {
      specs._union = true;
      return {false, true};
    } else {
      auto* declaration = scope.findDeclarationName(token.value);
      if (declaration == nullptr) {
        logError(token, "INTERNAL ERROR: AST PARSER.CPP, PRIMITIVE TYPE TAGGED BUT NOT FOUND IN SCOPE.");
        return {true, false};
      }
      if (specs.decl != nullptr) {
        logError(token, "Expected identifier");
        return {true,false};
      }
      specs.decl = declaration;
      return {false, true};
    }
    
  } else if (token.isIdentifier()) {
    auto* declaration = scope.findDeclarationName(token.value);
    if (declaration == nullptr) return {false,true};
    if (specs.decl != nullptr) {
      logError(token, "Expected identifier");
      return {true,false};
    }

    specs.decl = declaration;
    return {false, true};
  } else {
    return {true,false};
  }

  return {true, false};
}
  
} // namespace C
