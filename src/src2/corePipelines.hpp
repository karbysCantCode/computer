#pragma once

#include "CLIHelper.hpp"
#include "Arch/arch.hpp"
#include "Spasm/spasm.hpp"

int runArchPipeline(CLIOptions&, Arch::Architecture**);

int runSMakePipeline(CLIOptions&, Arch::Architecture&);

Spasm::Program::ProgramForm compileSpasm(std::filesystem::path path);

struct pathTargetPair {
  std::filesystem::path m_path;
  SMake::Target& m_target;

  pathTargetPair(std::filesystem::path path, SMake::Target& target) : m_path(path), m_target(target) {}
};