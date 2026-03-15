#pragma once

#include <filesystem>
// each target needs to be one program
// each file should be compiled and then combined


namespace Spasm {
  class Program {
    public:
    std::filesystem::path m_sourcePath;

    void parseTokens();

    private:
  };

  void spasmPipeline();
}