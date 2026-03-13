#include "Arch/Arch.hpp"

#include "Helpers/Debug.hpp"
#include <format>

namespace Arch {


Architecture::Architecture(TokenHolder& sourceHolder, Debug::FullLogger* logger) {
  p_logger = logger;

  while (sourceHolder.notAtEnd()) {
    switch (sourceHolder.peek().type) {
      case Token::Type::KEYWORD:
      {
        consumeKeyword(sourceHolder);
      }
      break;
      case Token::Type::NEWLINE:
      {
        sourceHolder.skip();
      }
      break;
      case Token::Type::UNASSIGNED:
      {
        logError(sourceHolder.peek(), "Got unassigned token.");
      }
      break;
      case Token::Type::IDENTIFIER:
      {
        consumeInstruction(sourceHolder);
      }
      break;
      default:
      logError(sourceHolder.peek(), std::format("Got unexpected \"{}\".", sourceHolder.peek().value));
      sourceHolder.skip();
      break;
    }
  }

}

void Architecture::consumeKeyword(TokenHolder& sourceHolder) {
  if (sourceHolder.peek().value == "reg") {
    consumeRegister(sourceHolder);
  } else if (sourceHolder.peek().value == "format") {
    consumeFormat(sourceHolder);
  } else if (sourceHolder.peek().value == "bitwidth") {
    consumeBitwidth(sourceHolder);
  } else if (sourceHolder.peek().value == "ctr") {
    consumeControlSignal(sourceHolder);
  } else {
    logError(sourceHolder.peek(), std::format("Unknown keyword \"{}\"", sourceHolder.peek().value));
    sourceHolder.skip();
  }
}

void Architecture::consumeControlSignal(TokenHolder& sourceHolder) {
  sourceHolder.skip(); //skip keyword
  const auto& identifierToken = sourceHolder.peek();
  const auto& bitIndexToken = sourceHolder.peek(1);

  ControlSignalDefinition controlSignal(std::string(identifierToken.value), (size_t)std::stoi(std::string(bitIndexToken.value)));
  m_controlSignalSet.emplace(controlSignal.m_controlSignalName, std::move(controlSignal));
  sourceHolder.skip(2);
}

void Architecture::consumeRegister(TokenHolder& sourceHolder) {
  sourceHolder.skip(); //skip keyword
  const auto& identifierToken = sourceHolder.peek();
  const auto& bitwidthToken = sourceHolder.peek(1);
  const auto& minimumMachineOperandValueToken = sourceHolder.peek(2);

  const size_t bitwidth = std::stoi(std::string(bitwidthToken.value));
  const size_t minimumMachineOperandValue = std::stoi(std::string(minimumMachineOperandValueToken.value));

  //parse the identifier token.
  const auto info = parseRegisterRange(std::string{identifierToken.value}, identifierToken);

  // insert registers
  if (info.hasDash) {
    //insert multiple
    size_t currentIndex = info.lowValue;
    size_t offset = 0;
    while (currentIndex <= info.highValue) {
      RegisterDefinition reg(info.prefix + std::to_string(currentIndex), minimumMachineOperandValue+offset, bitwidth);
      m_registerSet.emplace(reg.m_registerName, std::move(reg));
      offset++;
      currentIndex++;
    }
  } else {
    //insert one
    RegisterDefinition reg(std::string(identifierToken.value), minimumMachineOperandValue, bitwidth);
    m_registerSet.emplace(reg.m_registerName, std::move(reg));
  }

  sourceHolder.skip(3);
}

void Architecture::consumeDatatype(TokenHolder& sourceHolder) {
  //static_assert(false);
  sourceHolder.skip(); //skip keyword
  const auto& identifierToken = sourceHolder.peek();
  const auto& bitIndexToken = sourceHolder.peek(1);
}

void Architecture::consumeFormat(TokenHolder& sourceHolder) {
  sourceHolder.skip(); //skip keyword
  const auto& identifierToken = sourceHolder.consume();

  FormatDefinition format(std::string(identifierToken.value));
  while (sourceHolder.match(Token::Type::IDENTIFIER)) {
    format.insertOperandSize(std::stoi(std::string(sourceHolder.consume().value)));
  }

  m_formatSet.emplace(format.m_formatName, std::move(format));

}

void Architecture::consumeBitwidth(TokenHolder& sourceHolder) {
  sourceHolder.skip(); //skip keyword
  const auto& bitwidthToken = sourceHolder.peek();

  m_bitwidth = std::stoi(std::string(bitwidthToken.value));

  sourceHolder.skip();
}

void Architecture::consumeInstruction(TokenHolder& sourceHolder) {
  const auto& identifierToken = sourceHolder.peek();
  const auto& byteLengthToken = sourceHolder.peek(1);
  const auto& opcodeToken = sourceHolder.peek(2);
  const auto& formatToken = sourceHolder.peek(3);
  sourceHolder.skip(4);

  auto it = m_formatSet.find(std::string(formatToken.value));
  if (it == m_formatSet.end()) {
    logError(formatToken, std::format("Unknown format \"{}\" referenced.", formatToken.value));
    return;
  }
  InstructionDefinition instruction(std::string(identifierToken.value), 
                                    std::stoi(std::string(opcodeToken.value)),
                                    std::stoi(std::string(byteLengthToken.value)),
                                    it->second);

  m_instructionSet.emplace(instruction.m_name, std::move(instruction));


  //get arguments
  while (sourceHolder.match(Token::Type::ARGUMENTTYPE)) {
    if (sourceHolder.peek().value == "REG") {
      sourceHolder.skip();
      const auto& aliasToken = sourceHolder.consume();
      const auto& rangeToken = sourceHolder.consume();
      RegisterOperand operand(std::string(aliasToken.value));

      size_t rangeIndex = 0;
      std::string currentRange;

      auto processRange = [&]() {
        const auto info = parseRegisterRange(currentRange, rangeToken);
        if (info.hasDash) {
          size_t indx = info.lowValue;
          size_t offset = 0;
          while (indx <= info.highValue) {
            operand.acceptedRegisterNames.emplace(info.prefix + std::to_string(indx + offset));
            offset++;
            indx++;
          }
        } else {
          operand.acceptedRegisterNames.emplace(currentRange);

        }
        currentRange.clear();
      };
      
      while (rangeIndex < rangeToken.value.size()) {
        const char c = rangeToken.value[rangeIndex];
        rangeIndex++;
        if (c == '<') {
          processRange();
        } else {
          currentRange.push_back(c);
        }
      }
      processRange();

      instruction.m_operands.push_back(std::move(operand));
      
    } else if (sourceHolder.peek().value == "IMM") {
      sourceHolder.skip();
      const auto& aliasToken = sourceHolder.consume();
      const auto& immediateToken = sourceHolder.consume();

      size_t indx = 0;
      bool hasHalfed = false;
      size_t min = 0;
      size_t max = 0;
      std::string currentSlice;
      while (indx < immediateToken.value.size()) {
        const char c = immediateToken.value[indx];

        if (c == ':') {
          if (hasHalfed) {
            logError(immediateToken,"Immediate range has multiple colons.");
            return;
          }
          hasHalfed = true;
          min = std::stoi(currentSlice);
          currentSlice.clear();
        } else {
          currentSlice.push_back(c);
        }
        indx++;
      }
      max = std::stoi(currentSlice);
      if (!hasHalfed) {
        logError(immediateToken, "Immediate does not have valid range");
        return;
      }
      ImmediateOperand operand(std::string(aliasToken.value), min, max);

      instruction.m_operands.push_back(std::move(operand));
      
    } else if (sourceHolder.peek().value == "CONST") {
      sourceHolder.skip();
      const auto& immediateToken = sourceHolder.consume();

      if (immediateToken.value.size() > 0 && isdigit(immediateToken.value[0])) {
        ConstantIntOperand operand(std::stoi(std::string(immediateToken.value)));

        instruction.m_operands.push_back(std::move(operand));

      } else {
        ConstantStringOperand operand(std::string(immediateToken.value));
        instruction.m_operands.push_back(std::move(operand));
      }
      
    } else if (sourceHolder.peek().value == "NON") {
      sourceHolder.skip();

      ConstantIntOperand operand(0); //i guess just fill the field with 0 probably.

      instruction.m_operands.push_back(std::move(operand));
      
    } else {
      logError(sourceHolder.consume(), "Unknown argument type.");
      return;
    }
  }


  

}

Architecture::RegisterRangeInfo Architecture::parseRegisterRange(const std::string& string, const Token& errToken) const {
  RegisterRangeInfo info;

  size_t index = 0;
  std::string currentValue;
  while (index < string.size()) {
    const char c = string[index];
    index++;
    if (c >= '0' && c <= '9') {
      currentValue.push_back(c);
    } else if (c >= 'a' && c <= 'Z') {
      if (info.hasDash) {continue;}
      info.prefix.push_back(c);
    } else if (c == '-') {
      if (info.hasDash) {
        logError(errToken, "Register range has multiple dashes in definition.");
      } else {
        info.lowValue = std::stoi(currentValue);
        info.hasDash = true;
        currentValue.clear();
      }
    } else {
      logError(errToken, "Got unexpected character in register range definition.");
    }
  }

  info.highValue = std::stoi(currentValue);

  return info;
}


}