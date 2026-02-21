#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "spasm.hpp"

namespace Macro {
  struct Macro {
  std::string target;

  virtual ~Macro() = default;

  //returns the index after the replacement
  virtual std::vector<Spasm::Lexer::Token> getReplacment(size_t& index, std::vector<Spasm::Lexer::Token>& parsingStream) {return {};}
};

struct FunctionMacro : Macro {
  std::unordered_map<std::string, size_t> args;
  std::vector<Spasm::Lexer::Token> replacement;

  std::vector<Spasm::Lexer::Token> getReplacment(size_t& index, std::vector<Spasm::Lexer::Token>& parsingStream) override;
};

struct ReplacementMacro : Macro {
  std::vector<Spasm::Lexer::Token> replacementBody;

  std::vector<Spasm::Lexer::Token> getReplacment(size_t& index, std::vector<Spasm::Lexer::Token>& parsingStream) override {
    return replacementBody;
  }

  ReplacementMacro() {}
};  
}

