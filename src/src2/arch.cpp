#include "arch.hpp"

#include <sstream>

std::string Arch::Instruction::Instruction::toString(size_t padding) const {
  std::ostringstream str;
  std::string indent(padding, ' ');
  std::string indentArg(padding + 8, ' ');
  str <<       indent << "Instruction name: " << this->m_name << '\n'
            << indent << "    " << "Opcode: \"" << this->m_opcode << "\"" << '\n'
            << indent << "    " << "Format alias: " << this->m_formatAlias << '\n'
            << indent << "    " << "Arguments: " << '\n'; 

  for (const auto& arg : this->m_arguments) {
    str << indentArg << "    " << arg.toString(padding + 8);
  }

  str << std::endl;
  return str.str();
}

std::string Arch::Instruction::Argument::toString(size_t padding) const {
  return "Argument Alias: " + m_alias + '\n' + std::string(padding, ' ') + "    Type: " + typeToString() + '\n' + std::string(padding, ' ') + "    Range: " + m_range.get()->toString() + '\n';
}

std::string Arch::Format::toString(size_t padding) const {
  std::ostringstream str;
  std::string indent(padding, ' ');
 str << indent << "Format name: " << this->m_alias << "\n    Operand bit widths:\n        ";
  for (const auto& operandBits : this->m_operandBitwidth) {
    str << indent << operandBits << ", ";
  }
  str << '\n';
  return str.str();
}

std::string Arch::DataType::toString(size_t padding) const {
  std::ostringstream str;
  std::string indent(padding, ' ');
  std::string indent4(padding + 4, ' ');
  str << indent << "Name: " << m_name << '\n';
  if (m_autoLength) {
    str << indent4 << "Length: Auto Length";
  } else {
    str << indent4 << "Length: " << m_length;
  }
  str << '\n';
  return str.str();
}

constexpr const char* Arch::Token::typeToString() {
  switch (m_type) {
    case Type::UNASSIGNED: return "UNASSIGNED";
    case Type::KEYWORD:    return "KEYWORD";
    case Type::OPENPAREN:  return "OPENPAREN";
    case Type::CLOSEPAREN: return "CLOSEPAREN";
    case Type::OPENBLOCK:  return "OPENBLOCK";
    case Type::CLOSEBLOCK: return "CLOSEBLOCK";
    case Type::STRING:     return "STRING";
    case Type::IDENTIFIER: return "IDENTIFIER";
    case Type::COMMA:      return "COMMA";
    case Type::INVALID:    return "INVALID";
    default: return "UNKNOWN";
  }
}

std::string Arch::Token::toString(size_t padding) const {
  std::
  std::cout << std::setw(padding) << "Token Type: " << toString(this->type) << '\n'
            << std::setw(padding) << "    " << "Token Value: \"" << this->value << "\"" << '\n'
            << std::setw(padding) << "    " << "Line: " << this->line << '\n'
            << std::setw(padding) << "    " << "Col:  " << this->column << std::endl;
}

