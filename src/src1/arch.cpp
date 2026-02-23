#include "arch.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

std::string Architecture::Instruction::Instruction::toString(size_t padding) const {
  std::ostringstream str;
  std::string indent(padding, ' ');
  std::string indentArg(padding + 8, ' ');
  str <<       indent << "Instruction name: " << this->name << '\n'
            << indent << "    " << "Opcode: \"" << this->opcode << "\"" << '\n'
            << indent << "    " << "Format alias: " << this->formatAlias << '\n'
            << indent << "    " << "Arguments: " << '\n'; 

  for (const auto& arg : this->arguments) {
    str << indentArg << "    " << arg.toString(padding + 8);
  }

  str << std::endl;
  return str.str();
}

std::string Architecture::Instruction::Argument::toString(size_t padding) const {
  return "Argument Alias: " + alias + '\n' + std::string(padding, ' ') + "    Type: " + typeToString() + '\n' + std::string(padding, ' ') + "    Range: " + range.get()->toString() + '\n';
}

void Architecture::Architecture::Format::print(size_t padding) const {
  std::cout << std::setw(padding) << "Format name: " << this->alias << "\n    Operand bit widths:\n        ";
  for (const auto& operandBits : this->operandBitwidth) {
    std::cout << std::setw(padding) << operandBits << ", ";
  }
  std::cout << std::endl;
}

void Architecture::DataType::print(size_t padding) const {
  std::cout << std::setw(padding) << "Name: " << name << '\n';
  if (autoLength) {
    std::cout << std::setw(padding + 4) << "Length: Auto Length";
  } else {
    std::cout << std::setw(padding + 4) << "Length: " << length;
  }
  std::cout << std::endl;
}