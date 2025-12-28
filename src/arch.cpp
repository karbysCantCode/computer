#include "arch.hpp"
#include <iostream>
#include <iomanip>

void Instruction::Instruction::print(size_t padding) const {
  std::cout << std::setw(padding) << "Instruction name: " << this->name << '\n'
            << std::setw(padding) << "    " << "Opcode: \"" << this->opcode << "\"" << '\n'
            << std::setw(padding) << "    " << "Format alias: " << this->formatAlias << '\n'
            << std::setw(padding) << "    " << "Arguments: " << '\n';

  for (const auto& arg : this->arguments) {
    std::cout << std::setw(padding + 8) << "    " << arg.toString(padding + 8);
  }

  std::cout << std::endl;
}

std::string Instruction::Argument::toString(size_t padding) const {
  return "Argument Alias: " + this->alias + '\n' + std::string(padding, ' ') + "    Type: " + this->typeToString() + '\n' + std::string(padding, ' ') + "    Range: " + this->range.get()->toString() + '\n';
}

void Architecture::Format::print(size_t padding) const {
  std::cout << std::setw(padding) << "Format name: " << this->alias << "\n    Operand bit widths:\n        ";
  for (const auto& operandBits : this->operandBitwidth) {
    std::cout << std::setw(padding) << operandBits << ", ";
  }
  std::cout << std::endl;
}