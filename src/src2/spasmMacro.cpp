#include "spasmMacro.hpp"

std::vector<Spasm::Lexer::Token> Macro::FunctionMacro::getReplacment(
  size_t& index, 
  std::vector<Spasm::Lexer::Token>& parsingStream
) {
  std::vector<std::vector<Spasm::Lexer::Token>> localArgs;

  size_t argCount = 0;
  bool isBlame = true;

  std::vector<Spasm::Lexer::Token> currentArg;
  while (localArgs.size() < args.size() && index < parsingStream.size()) {
    const Spasm::Lexer::Token& token = parsingStream[index];
    index++;
    
    if (token.isOneOf({
          Spasm::Lexer::Token::Type::COMMA,
          Spasm::Lexer::Token::Type::NEWLINE,
          Spasm::Lexer::Token::Type::CLOSEPAREN})) {
      argCount++; 
      isBlame = true; 
      localArgs.push_back(currentArg); 
      currentArg.clear(); 
      continue;
    }
    
    currentArg.push_back(token);
  }

  //replacementStream.erase(replacementStream.begin() + fillIndex, replacementStream.begin() + currentIndex);
  std::vector<Spasm::Lexer::Token> output;
  for (const auto& token : replacement) {
    auto it = args.find(token.m_value);
    //auto insertPos = replacementStream.begin() + currentIndex;


    if (it != args.end()) {
      const auto& expansion = localArgs[it->second];

      output.insert(output.end(), expansion.begin(), expansion.end());
      
    } else {
      output.push_back(token);
    }
  }

  return output;
}