#pragma once

#include "CLIHelper.hpp"

int runArchPipeline(CLIOptions&, Arch::Architecture**);

int runSMakePipeline(CLIOptions&, Arch::Architecture&);

int compileSpasm();