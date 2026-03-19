#pragma once

#include <filesystem>
#include <unordered_set>

#include "SMake/SMake.hpp"
// each target needs to be one program
// each file should be compiled and then combined


namespace Spasm {
  class Program {
    public:
    std::filesystem::path m_sourcePath;
    std::unordered_set<std::filesystem::path> m_includedFilesByPath; // absolute path
    void parseTokens();

    private:
    
  };

  void spasmPipeline(SMake::Project&);
}