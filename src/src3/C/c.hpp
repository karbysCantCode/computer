#pragma once
#include <filesystem>

#include "lexer.hpp"
#include "translationUnit.hpp"

namespace C {
void compileFile(std::filesystem::path& path, std::filesystem::path& output, std::vector<std::filesystem::path>& includes);
TokenHolder preprocessStage(TranslationUnit& translationUnit);
}