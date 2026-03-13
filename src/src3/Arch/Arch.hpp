#pragma once

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <variant>

#include "Arch/Lexer.hpp"
#include "Helpers/Debug.hpp"

namespace Arch {


class Architecture {
  public:

  struct RegisterDefinition {
    const std::string m_registerName;
    const int m_operandValue;
    const int m_bitwidthValue;

    RegisterDefinition(
      const std::string& registerName, int operandValue, int bitwidthValue) 
      : m_registerName(registerName), m_operandValue(operandValue), m_bitwidthValue(bitwidthValue) {}
  };

  struct ControlSignalDefinition {
    const std::string m_controlSignalName;
    const size_t m_assertBitIndex;

    ControlSignalDefinition(const std::string& name, const size_t assertBitIndex) : m_controlSignalName(name), m_assertBitIndex(assertBitIndex) {}
  };

  struct DataTypeDefinition {
    const std::string m_dataTypeName;
    const size_t m_byteLength;

    DataTypeDefinition(const std::string& name, const size_t byteLength) : m_dataTypeName(name), m_byteLength(byteLength) {}
  };

  struct FormatDefinition {
    std::string m_formatName;
    std::vector<size_t> m_formatOperandSizes;

    void inline insertOperandSize(size_t size) {m_formatOperandSizes.push_back(size);}

    FormatDefinition(const std::string& name) : m_formatName(name) {}
  };

  struct RegisterOperand {
    std::string alias;
    std::unordered_set<std::string> acceptedRegisterNames;

    RegisterOperand(const std::string& alia) : alias(alia) {}
  };
  struct ImmediateOperand {
    std::string alias;
    int min;
    int max;

    ImmediateOperand(const std::string& alia, int in, int ax) : alias(alia), min(in), max(ax) {}
  };
  struct ConstantIntOperand {
    int constant;

    ConstantIntOperand(int constt) : constant(constt) {}
  };
  struct ConstantStringOperand {
    std::string constant;

    ConstantStringOperand(const std::string& constt) : constant(constt) {}
  };

  struct InstructionDefinition {  
    public:
    std::string m_name;
    std::vector<std::variant<RegisterOperand, ImmediateOperand, ConstantIntOperand, ConstantStringOperand>> m_operands;
    int m_opcode = -1;
    int m_byteLength = 2;

    const FormatDefinition& m_format;

    InstructionDefinition(const std::string& name, int opcode, int byteLength, const FormatDefinition& format) : m_name(name), m_opcode(opcode), m_byteLength(byteLength), m_format(format) {}
  };

  Architecture(TokenHolder& sourceHolder, Debug::FullLogger* logger);

  std::unordered_map<std::string, InstructionDefinition> m_instructionSet;
  std::unordered_map<std::string, RegisterDefinition> m_registerSet;
  std::unordered_map<std::string, ControlSignalDefinition> m_controlSignalSet;
  std::unordered_map<std::string, FormatDefinition> m_formatSet;

  size_t m_bitwidth;
  
  private:
  Debug::FullLogger* p_logger;

  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logError(const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(message);}}

  void consumeKeyword(TokenHolder&);
  void consumeControlSignal(TokenHolder&);
  void consumeRegister(TokenHolder&);
  void consumeDatatype(TokenHolder&);
  void consumeFormat(TokenHolder&);
  void consumeBitwidth(TokenHolder&);
  void consumeInstruction(TokenHolder&);

  struct RegisterRangeInfo {
    bool hasDash = false;
    std::string prefix;
    size_t lowValue = 0;
    size_t highValue = 0;
  };

  RegisterRangeInfo parseRegisterRange(const std::string&, const Token&) const;
};

}