

#include "Helpers/CLIOptions.hpp"
#include "Arch/Arch.hpp"
#include "SMake/SMake.hpp"
#include "Spasm/Spasm.hpp"
#include "C/c.hpp"

#define cachedArchFileName "__CACHED__ARCH__FILE__"

int main(int argc, char* argv[]) {
  CLIOptions options;
  options.newArchPath = cachedArchFileName;
  if (!options.evaluate(argc, argv)) return -1;

  

  if (options.newArch) {
    std::filesystem::copy(options.newArchPath, std::filesystem::path(cachedArchFileName), std::filesystem::copy_options::overwrite_existing);
  }

  Debug::FullLogger archLogger;
  auto architecture =  Arch::architecturePipeline(options.newArchPath, &archLogger);
  if (!options.silent) {
    while (!archLogger.Errors.isEmpty()) {
      std::cout << archLogger.Errors.consumeMessage() << '\n';
    }
  }
  
  if (options.debugs) {
    architecture.debugPrint();
  }

  if (options.regexArchDump) {
    std::string instructionString = "(";
    for (const auto& instruction : architecture.m_instructionSet) {
      instructionString.append(instruction.second.m_name);
      instructionString.push_back('|');
    }
    instructionString.pop_back();
    instructionString.push_back(')');

    std::string registerString = "(";
    for (const auto& registr : architecture.m_registerSet) {
      registerString.append(registr.second.m_registerName);
      registerString.push_back('|');
    }
    registerString.pop_back();
    registerString.push_back(')');

    std::cout << "REGEX REGISTER SET:" << registerString << std::endl;
    std::cout << "REGEX INSTRUCTION SET:" << instructionString << std::endl;
  }

  
  
  Debug::FullLogger smakeLogger;
  SMake::Project project = SMake::smakePipeline(options.smakePath, &smakeLogger);
  if (!options.silent) {
    while (!smakeLogger.Errors.isEmpty()) {
      std::cout << smakeLogger.Errors.consumeMessage() << '\n';
    }
  }
  if (options.warns) {
    while (!smakeLogger.Warnings.isEmpty()) {
      std::cout << smakeLogger.Warnings.consumeMessage() << '\n';
    }
  }
  if (options.debugs) {
    while (!smakeLogger.Debugs.isEmpty()) {
      std::cout << smakeLogger.Debugs.consumeMessage() << '\n';
    }
  }
  if (options.debugs) {
    SMake::printProject(project);
  }

  //Spasm::spasmPipeline(project, architecture, options);
  
  if (options.c) {
    C::compileFile(options.cPath, options.cPath, options.cIncPaths);
  }

  return 0;
}