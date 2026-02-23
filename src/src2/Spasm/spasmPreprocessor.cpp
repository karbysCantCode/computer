#include "spasmPreprocessor.hpp"

enum class DEFINEDIRECTIVETYPE {
  INLINE,
  FUNCTION
};

using pp = Preprocessor;

Spasm::Lexer::TokenHolder pp::run(
  Spasm::Lexer::TokenHolder& inputHolder,
  Spasm::Program::ProgramForm& program
) {
  Spasm::Lexer::TokenHolder output;
  output.m_tokens.reserve(inputHolder.m_tokens.size());
  p_stack.emplace(inputHolder);


  while (!p_stack.empty()) {
    auto& currentHolder = p_stack.top();

    if (currentHolder.isAtEnd()) {
      p_stack.pop();
      continue;
    }

    handleTokenHolder(currentHolder, output.m_tokens, program);
  }

  //cleanup
  p_macroMap.clear();
  while (!p_stack.empty()) {p_stack.pop();}

  return output;
}

void pp::handleTokenHolder(Spasm::Lexer::TokenHolder& holder, std::vector<Spasm::Lexer::Token>& output, Spasm::Program::ProgramForm& program) {
  while (!holder.isAtEnd()) {
    const auto& token = holder.peek();
    if (!token.isDirective()) {
      //check if macro
      const auto& it = p_macroMap.find(token.m_value);
      if (it != p_macroMap.end()) {
        holder.skip(); // skip macro identifier
        expandMacroIfExists(holder, it->second);
        break;
      }

      output.push_back(holder.consume());
      continue;
    }

    switch (token.m_nicheType) {
      using ty = Spasm::Lexer::Token::NicheType;
      case ty::DIRECTIVE_DEFINE: {
        handleDefine(holder);
        break;
      }
      case ty::DIRECTIVE_INCLUDE: {
        handleInclude(holder, program);
        break;
      }
      case ty::DIRECTIVE_ENTRY: {

        break;
      }
      default:
        logError(token, "Unknown directive type \"" + token.m_value + '"');
        holder.skip();
        break;
    }
  }
}

void pp::handleDefine(Spasm::Lexer::TokenHolder& holder) {

  const auto& defineStartToken = holder.consume(); //skip @define token

  std::string macroName = holder.consume().m_value;
  auto newMacroIt = p_macroMap.find(macroName);

  DEFINEDIRECTIVETYPE type = DEFINEDIRECTIVETYPE::INLINE;

  if (holder.peek().m_type == Spasm::Lexer::Token::Type::OPENPAREN) {
    type = DEFINEDIRECTIVETYPE::FUNCTION;
    holder.skip(1); // skip open paren
  }

  if (newMacroIt != p_macroMap.end()) { 
    logError(defineStartToken, "Redefinition of macro \"" + macroName + '"');
    if (type == DEFINEDIRECTIVETYPE::FUNCTION) {
      while (holder.peek().m_type != Spasm::Lexer::Token::Type::CLOSEBLOCK) {holder.skip(1);}
    } else {
      while (holder.peek().m_type != Spasm::Lexer::Token::Type::NEWLINE) {holder.skip(1);}
    }
    return;
  }

  switch (type)
  {
  case DEFINEDIRECTIVETYPE::FUNCTION: {
    auto macro = std::make_unique<Macro::FunctionMacro>();
    bool isEnd = false;
    size_t argCount = 0;
    while (!holder.isAtEnd() && !holder.peek().isCloseParen()) {
      //get args

      macro->args[holder.consume().m_value] = argCount;
      argCount++;

      if (holder.peek().isComma()) {
        holder.skip();
        continue;

      } else if (!holder.peek().isCloseParen()) {
        logError(holder.peek(),"Unexpected token type (comma or close paren expected), got " + std::string(holder.peek().typeToString()) + '"');
        break;

      } else {
        holder.skip();
        break;
      }
    }
    //
    if (!holder.peek().isOpenBlock()) {
      logError(holder.peek(), "Function block missing, expected curly brace, got \"" + holder.peek().m_value + '"');
      p_macroMap[macroName] = std::move(macro);
      return;
    }

    holder.skip(1); // skip '{'
    while (!holder.peek().isCloseBlock() && !holder.isAtEnd()) {
      if (holder.peek().m_value == macroName) {
        logError(holder.peek(), "Macroname not allowed inside macro definition.");
        while (!holder.peek().isCloseBlock() && !holder.isAtEnd()) {holder.skip();}
        break;
      } else {
        macro->replacement.push_back(holder.consume());
      }
    }
    holder.skip(1);

    p_macroMap[macroName] = std::move(macro);
    break;
  }
  
  case DEFINEDIRECTIVETYPE::INLINE: {
    auto macro = std::make_unique<Macro::ReplacementMacro>();
    while (!holder.isAtEnd() && holder.peek().m_type != Spasm::Lexer::Token::Type::NEWLINE) {
      macro->replacementBody.push_back(holder.consume());
    }
    p_macroMap[macroName] = std::move(macro);
    break;
  }

  default:
    assert(false);
    //shouldnt reach because type is set by if ELSE.
    break;
  }
}
void pp::handleInclude(Spasm::Lexer::TokenHolder& holder, Spasm::Program::ProgramForm& program) {
  holder.skip();

  const auto& fileToken = holder.consume();
  program.m_includedPrograms.emplace(fileToken.m_value, fileToken);
  // auto filePath = target.searchForPathInIncluded(fileToken.m_value).lexically_normal();
  // if (filePath.empty()) {
  //   logError(fileToken, "Expanded to empty path, include \"" + fileToken.m_value + "\" could not be resolved.");
  //   return;
  // }
  
  // std::unique_ptr<Spasm::Program::ProgramForm>* includedProgramUniquePtrPtr = nullptr;
  
  // auto progMapIt = target.m_filePathProgramMap.find(filePath);
  // if (progMapIt == target.m_filePathProgramMap.end()) {
  //   auto newUniqueProgramPtr = std::make_unique<Spasm::Program::ProgramForm>();
  //   auto [newit, inserted] = target.m_filePathProgramMap.emplace(filePath, std::move(newUniqueProgramPtr));
  //   includedProgramUniquePtrPtr = &newit->second;
  // } else {
  //   includedProgramUniquePtrPtr = &progMapIt->second;
  // }

  // program.m_includedPrograms.emplace(filePath, includedProgramUniquePtrPtr);
}
bool pp::expandMacroIfExists(Spasm::Lexer::TokenHolder& holder, std::unique_ptr<Macro::Macro>& macro) {
  p_stack.push(macro->getReplacment(holder));
  return true;
}


void pp::logError(const Spasm::Lexer::Token& errToken, const std::string& message) const {
  if (m_logger != nullptr) {
    m_logger->Errors.logMessage(errToken.positionToString() + ": error: " + message);
  }
}
void pp::logWarning(const Spasm::Lexer::Token& errToken, const std::string& message) const {
  if (m_logger != nullptr) {
    m_logger->Warnings.logMessage(errToken.positionToString() + ": warning: " + message);
  }
}
void pp::logDebug(const Spasm::Lexer::Token& errToken, const std::string& message) const {
  if (m_logger != nullptr) {
    m_logger->Debugs.logMessage(errToken.positionToString() + ": debug: " + message);
  }
}