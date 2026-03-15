

#include "Helpers/CLIOptions.hpp"
#include "Arch/Arch.hpp"
#include "SMake/SMake.hpp"

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
  SMake::printProject(project);
  
  return 0;
}