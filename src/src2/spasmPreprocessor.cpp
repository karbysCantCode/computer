#include "spasmPreprocessor.hpp"

enum class DEFINEDIRECTIVETYPE {
  INLINE,
  FUNCTION
};

using pp = Preprocessor;

std::vector<Spasm::Lexer::Token> pp::run(
  std::vector<Spasm::Lexer::Token>& inputTokens,
  Spasm::Program::ProgramForm& program, 
  SMake::Target& target, 
  std::unordered_map<
    std::filesystem::path, 
    std::unique_ptr<Spasm::Program::ProgramForm>
  >& filePathProgramMap
) {
  std::vector<Spasm::Lexer::Token> output;
  p_stack.emplace(TokenStream(inputTokens));

  while (!p_stack.empty()) {
    auto& currentStream = p_stack.top();

    if (!(currentStream.m_index < currentStream.m_tokens.size())) {
      p_stack.pop();
      continue;
    }

    handleTokenStream(currentStream, output);
  }

  //cleanup
  p_macroMap.clear();
  while (!p_stack.empty()) {p_stack.pop();}

  return output;
}

void pp::handleTokenStream(TokenStream& stream, std::vector<Spasm::Lexer::Token>& output) {
  auto isAtEnd = [&]() {
      return !(stream.m_index<stream.m_tokens.size());
    };
  auto peek = [&](size_t peekDistance = 0) {
    return stream.m_index+peekDistance < stream.m_tokens.size() ? stream.m_tokens[stream.m_index+peekDistance] : Spasm::Lexer::Token("EOF", Spasm::Lexer::Token::Type::UNASSIGNED, -1,-1);
  };
  auto advance = [&]() {
    auto retVal = isAtEnd() ? Spasm::Lexer::Token("EOF", Spasm::Lexer::Token::Type::UNASSIGNED, -1,-1) : stream.m_tokens[stream.m_index];
    stream.m_index++;
    return retVal;
  };
  auto skip = [&](size_t distance = 1) {
    stream.m_index += distance;
  };
  auto& index = stream.m_index;

  while (!isAtEnd()) {
    const auto& token = peek();
    if (!token.isDirective()) {
      //check if macro
      const auto& it = p_macroMap.find(token.m_value);
      if (it != p_macroMap.end()) {
        expandMacroIfExists(stream, it->second);
        continue;
      }


      output.push_back(advance());
      continue;
    }

    switch (token.m_nicheType) {
      using ty = Spasm::Lexer::Token::NicheType;
      case ty::DIRECTIVE_DEFINE: {
        handleDefine(stream);
        break;
      }
      case ty::DIRECTIVE_INCLUDE: {

        break;
      }
      case ty::DIRECTIVE_ENTRY: {

        break;
      }
      default:
        logError("Unknown directive type \"" + token.m_value + "\" at " + token.positionToString());
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
    logError("Redefinition of macro \"" + macroName + "\" at " + defineStartToken.positionToString());
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
        logError("Unexpected token type (comma or close paren expected), got " + std::string(stream.peek().typeToString()) + " at " + stream.peek().positionToString());
        break;

      } else {
        stream.skip();
        break;
      }
    }
    //
    if (!stream.peek().isOpenBlock()) {
      logError("Function block missing, expected curly brace, got " + std::string(stream.peek().typeToString()) + " at " + stream.peek().positionToString() + " value: \"" + stream.peek().m_value + "\"");
      p_macroMap[macroName] = std::move(macro);
      return;
    }

    stream.skip(1); // skip '{'
    while (stream.peek().m_type != Spasm::Lexer::Token::Type::CLOSEBLOCK && !stream.isAtEnd()) {
      if (stream.peek().m_value == macroName) {
        logError("Macroname not allowed inside macro definition. at " + stream.peek().positionToString());
        while (stream.peek().m_type != Spasm::Lexer::Token::Type::CLOSEBLOCK && !stream.isAtEnd()) {stream.m_index++;}
        break;
      } else {
        macro->replacement.push_back(stream.advance());
      }
    }
    stream.skip(1);
  
    __BlockParseEnd:
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
void pp::handleInclude(TokenStream&) {

}
bool pp::expandMacroIfExists(TokenStream& stream, std::unique_ptr<Macro::Macro>& macro) {
  p_stack.push(macro->getReplacment(stream.m_index, stream.m_tokens));
  return true;
}


void pp::logError(const std::string& message) const {
  if (m_logger != nullptr) {
    m_logger->Errors.logMessage(message);
  }
}
void pp::logWarning(const std::string& message) const {
  if (m_logger != nullptr) {
    m_logger->Warnings.logMessage(message);
  }
}
void pp::logDebug(const std::string& message) const {
  if (m_logger != nullptr) {
    m_logger->Debugs.logMessage(message);
  }
}