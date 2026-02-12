#pragma once
#include <iostream>
#include <fstream>
#include <filesystem>

int dumpString(std::filesystem::path& filePath, std::string& fileContents) {
  std::cout << "Dumped to: \"" << filePath.lexically_normal() << "\"\n";
  std::ofstream file(filePath);

  if (!file.is_open()) {
        return 1; // failed to open
    }

    file << fileContents;
    return 0;
}