#include "spasmPreprocessor.hpp"

enum class DEFINEDIRECTIVETYPE {
  INLINE,
  FUNCTION
};

using pp = Preprocessor;

Spasm::Lexer::TokenHolder pp::run(
  std::vector<Spasm::Lexer::Token>& inputTokens,
  Spasm::Program::ProgramForm& program, 
  SMake::Target& target
) {
  Spasm::Lexer::TokenHolder output;
  output.m_tokens.reserve(inputTokens.size());
  p_stack.emplace(TokenStream(inputTokens));


  while (!p_stack.empty()) {
    auto& currentStream = p_stack.top();

    if (!(currentStream.m_index < currentStream.m_tokens.size())) {
      p_stack.pop();
      continue;
    }

    handleTokenStream(currentStream, output.m_tokens, target, program);
  }

  //cleanup
  p_macroMap.clear();
  while (!p_stack.empty()) {p_stack.pop();}

  return output;
}

void pp::handleTokenStream(TokenStream& stream, std::vector<Spasm::Lexer::Token>& output, SMake::Target& target, Spasm::Program::ProgramForm& program) {
  while (!stream.isAtEnd()) {
    const auto& token = stream.peek();
    if (!token.isDirective()) {
      //check if macro
      const auto& it = p_macroMap.find(token.m_value);
      if (it != p_macroMap.end()) {
        stream.skip(); // skip macro identifier
        expandMacroIfExists(stream, it->second);
        break;
      }

      output.push_back(stream.advance());
      continue;
    }

    switch (token.m_nicheType) {
      using ty = Spasm::Lexer::Token::NicheType;
      case ty::DIRECTIVE_DEFINE: {
        handleDefine(stream);
        break;
      }
      case ty::DIRECTIVE_INCLUDE: {
        handleInclude(stream, target, program);
        break;
      }
      case ty::DIRECTIVE_ENTRY: {

        break;
      }
      default:
        logError(token, "Unknown directive type \"" + token.m_value + '"');
        stream.skip();
        break;
    }
  }
}

void pp::handleDefine(TokenStream& stream) {

  const auto& defineStartToken = stream.advance(); //skip @define token

  std::string macroName = stream.advance().m_value;
  auto newMacroIt = p_macroMap.find(macroName);

  DEFINEDIRECTIVETYPE type = DEFINEDIRECTIVETYPE::INLINE;

  if (stream.peek().m_type == Spasm::Lexer::Token::Type::OPENPAREN) {
    type = DEFINEDIRECTIVETYPE::FUNCTION;
    stream.skip(1); // skip open paren
  }

  if (newMacroIt != p_macroMap.end()) { 
    logError(defineStartToken, "Redefinition of macro \"" + macroName + '"');
    if (type == DEFINEDIRECTIVETYPE::FUNCTION) {
      while (stream.peek().m_type != Spasm::Lexer::Token::Type::CLOSEBLOCK) {stream.skip(1);}
    } else {
      while (stream.peek().m_type != Spasm::Lexer::Token::Type::NEWLINE) {stream.skip(1);}
    }
    return;
  }

  switch (type)
  {
  case DEFINEDIRECTIVETYPE::FUNCTION: {
    auto macro = std::make_unique<Macro::FunctionMacro>();
    bool isEnd = false;
    size_t argCount = 0;
    while (!stream.isAtEnd() && !stream.peek().isCloseParen()) {
      //get args

      macro->args[stream.advance().m_value] = argCount;
      argCount++;

      if (stream.peek().isComma()) {
        stream.skip();
        continue;

      } else if (!stream.peek().isCloseParen()) {
        logError(stream.peek(),"Unexpected token type (comma or close paren expected), got " + std::string(stream.peek().typeToString()) + '"');
        break;

      } else {
        stream.skip();
        break;
      }
    }
    //
    if (!stream.peek().isOpenBlock()) {
      logError(stream.peek(), "Function block missing, expected curly brace, got \"" + stream.peek().m_value + '"');
      p_macroMap[macroName] = std::move(macro);
      return;
    }

    stream.skip(1); // skip '{'
    while (!stream.peek().isCloseBlock() && !stream.isAtEnd()) {
      if (stream.peek().m_value == macroName) {
        logError(stream.peek(), "Macroname not allowed inside macro definition.");
        while (!stream.peek().isCloseBlock() && !stream.isAtEnd()) {stream.m_index++;}
        break;
      } else {
        macro->replacement.push_back(stream.advance());
      }
    }
    stream.skip(1);

    p_macroMap[macroName] = std::move(macro);
    break;
  }
  
  case DEFINEDIRECTIVETYPE::INLINE: {
    auto macro = std::make_unique<Macro::ReplacementMacro>();
    while (!stream.isAtEnd() && stream.peek().m_type != Spasm::Lexer::Token::Type::NEWLINE) {
      macro->replacementBody.push_back(stream.advance());
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
void pp::handleInclude(TokenStream& stream, SMake::Target& target, Spasm::Program::ProgramForm& program, std::stack<parseInfo>& parseStack, std::unordered_set<std::filesystem::path>& parsedSet) {
  stream.skip();

  const auto& fileToken = stream.advance();
  auto filePath = target.searchForPathInIncluded(fileToken.m_value).lexically_normal();
  if (filePath.empty()) {
    logError(fileToken, "Expanded to empty path, include \"" + fileToken.m_value + "\" could not be resolved.");
    return;
  }
  
  std::unique_ptr<Spasm::Program::ProgramForm>* includedProgramUniquePtrPtr = nullptr;
  
  auto progMapIt = target.m_filePathProgramMap.find(filePath);
  if (progMapIt == target.m_filePathProgramMap.end()) {
    auto newUniqueProgramPtr = std::make_unique<Spasm::Program::ProgramForm>();
    auto [newit, inserted] = target.m_filePathProgramMap.emplace(filePath, std::move(newUniqueProgramPtr));
    includedProgramUniquePtrPtr = &newit->second;
  } else {
    includedProgramUniquePtrPtr = &progMapIt->second;
  }

  if (parsedSet.find(filePath) == parsedSet.end()) {
    parseStack.emplace(target, filePath);
  }
  program.m_includedPrograms.emplace(filePath, includedProgramUniquePtrPtr);
}
bool pp::expandMacroIfExists(TokenStream& stream, std::unique_ptr<Macro::Macro>& macro) {
  p_stack.push(macro->getReplacment(stream.m_index, stream.m_tokens));
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