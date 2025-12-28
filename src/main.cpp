#include <iostream>
#include <fstream>
#include <string>

#include "lexer.hpp"
#include "archBuilder.hpp"
#include "arch.hpp"

int main(int argc, char* argv[]) {
  const char* fpath = "D:\\logisim prohects\\16bit cpu\\test.spasm";
  const char* archpath = "D:\\logisim prohects\\16bit cpu\\test.arch";
  
  ArchBuilder archBuilder;
  Lexer archLexer;
  archLexer.source = std::make_shared<std::string>(archLexer.readFile(archpath));
  auto archTokens = archLexer.lexArch();
  for (const auto& token : archTokens) {
    token.print(0);
  }

  Architecture arch = archBuilder.build(archTokens);
  for (const auto& instr : arch.m_instructionSet) {
    instr.second.print(0);
  }
  std::cout << "name set" << std::endl;
  for (const auto& thing : arch.m_instructionNameSet) {
    std::cout << thing << std::endl;
  }
  std::cout << "name end" << std::endl;

  Lexer asmLexer(std::make_shared<std::unordered_set<std::string>>(arch.m_instructionNameSet));
  asmLexer.source = std::make_shared<std::string>(asmLexer.readFile(fpath));
  auto asmTokens = asmLexer.lexAsm();
  for (const auto& token : asmTokens) {
    token.print(0);
  }

  return 0;
}