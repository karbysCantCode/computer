#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

#include "lexer.hpp"
#include "archBuilder.hpp"
#include "arch.hpp"
#include "spasmPreprocessor.hpp"

int main(int argc, char* argv[]) {
  std::filesystem::path fpath("resources/macroTest.spasm"); //"/Users/karbys/logisim projects/16bitcomputer/CppCompiler/resources/test.spasm";
  std::filesystem::path archpath("resources/test.arch");
  
  ArchBuilder archBuilder;
  Lexer archLexer;
  std::string archSource = archLexer.readFile(archpath.string());
  archLexer.source = &archSource;
  auto archTokens = archLexer.lexArch();
  for (const auto& token : archTokens) {
    token.print(0);
  }

  Architecture::Architecture arch = archBuilder.build(archTokens);
  for (const auto& instr : arch.m_instructionSet) {
    instr.second.print(0);
  }
  std::cout << "name set" << std::endl;
  for (const auto& thing : arch.m_instructionSet) {
    std::cout << thing.second.name << std::endl;
  }
  std::cout << "name end with: " << arch.m_instructionSet.size() << " instructions."<< std::endl;

  Lexer asmLexer(&arch.m_instructionNameSet);
  std::string asmSource = asmLexer.readFile(fpath.string());
  asmLexer.source = &asmSource;
  auto asmTokens = asmLexer.lexAsm();
  for (const auto& token : asmTokens) {
    token.print(0);
  }

  std::vector<std::string> preprocessorErrors;
  std::cout << "S" << std::endl;
  bool success = preprocessSpasm(asmTokens, &preprocessorErrors);
  std::cout << "S" << std::endl;
  std::cout << preprocessorErrors.size() << std::endl;
  if (preprocessorErrors.size() != 0) {
    std::cout << "Preprocessor errors:" << std::endl;
    for (const auto& err : preprocessorErrors) {
      std::cout << err << std::endl;
    }
  }

  for (const auto& token : asmTokens) {
    std::cout << token.value << '\n';
  }


  
  

  return 0;
}