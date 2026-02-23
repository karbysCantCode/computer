#pragma once

#include "Arch/Arch.hpp"

#include "Helpers/Debug.hpp"

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
      logError(sourceHolder.peek(), std::format("Got unexpected \"{}\".", sourceHolder.peek().value);
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
  size_t index = 0;
  std::string prefix;
  size_t lowValue = 0;
  size_t highValue = 0;
  std::string currentValue;
  bool hasDash = false;
  while (index < identifierToken.value.size()) {
    const char c = identifierToken.value[index];
    index++;
    if (c >= '0' && c <= '9') {
      currentValue.push_back(c);
    } else if (c >= 'a' && c <= 'Z') {
      if (hasDash) {continue;}
      prefix.push_back(c);
    } else if (c == '-') {
      if (hasDash) {
        logError(identifierToken, "Register range has multiple dashes in definition.");
      } else {
        lowValue = std::stoi(currentValue);
        hasDash = true;
        currentValue.clear();
      }
    } else {
      logError(identifierToken, "Got unexpected character in register range definition.");
    }
  }

  highValue = std::stoi(currentValue);

  // insert registers
  if (hasDash) {
    //insert multiple
    size_t currentIndex = lowValue;
    size_t offset = 0;
    while (currentIndex <= highValue) {
      RegisterDefinition reg(prefix + std::to_string(currentIndex), minimumMachineOperandValue+offset, bitwidth);
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
  static_assert(false);
  sourceHolder.skip(); //skip keyword
  const auto& identifierToken = sourceHolder.peek();
  const auto& bitIndexToken = sourceHolder.peek(1);
}

void Architecture::consumeFormat(TokenHolder& sourceHolder) {
  static_assert(false);
  sourceHolder.skip(); //skip keyword
  const auto& identifierToken = sourceHolder.peek();
  const auto& bitIndexToken = sourceHolder.peek(1);
}

void Architecture::consumeBitwidth(TokenHolder& sourceHolder) {
  static_assert(false);
  sourceHolder.skip(); //skip keyword
  const auto& identifierToken = sourceHolder.peek();
  const auto& bitIndexToken = sourceHolder.peek(1);
}

void Architecture::consumeInstruction(TokenHolder& sourceHolder) {
  static_assert(false);

}



}