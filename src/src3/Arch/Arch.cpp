#include "Arch/Arch.hpp"

#include "Helpers/Debug.hpp"
#include "Helpers/FileHelper.hpp"
#include "Helpers/SafeStoi.hpp"
#include <format>
#include <charconv>


namespace Arch {


Architecture::Architecture(TokenHolder& sourceHolder, Debug::FullLogger* logger) {
  p_logger = logger;
  sourceHolder.reset();
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
        sourceHolder.skip();
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

  auto [num, errm] = std::safe_stoi((std::string)bitIndexToken.value);
  if (!errm.empty()) {
    logError(bitIndexToken, errm);
  }

  ControlSignalDefinition controlSignal(std::string(identifierToken.value), (size_t)num);
  m_controlSignalSet.emplace(controlSignal.m_controlSignalName, std::move(controlSignal));
  sourceHolder.skip(2);
}

void Architecture::consumeRegister(TokenHolder& sourceHolder) {
  sourceHolder.skip(); //skip keyword
  const auto& identifierToken = sourceHolder.peek();
  const auto& bitwidthToken = sourceHolder.peek(1);
  const auto& minimumMachineOperandValueToken = sourceHolder.peek(2);

  auto [bitwidth, errm] = std::safe_stoi((std::string)bitwidthToken.value);
  if (!errm.empty()) {
    logError(bitwidthToken, errm);
  }

  auto [minimumMachineOperandValue, errmb] = std::safe_stoi((std::string)minimumMachineOperandValueToken.value);
  if (!errmb.empty()) {
    logError(minimumMachineOperandValueToken, errmb);
  }

  // const size_t bitwidth = std::stoi(std::string(bitwidthToken.value));
  // const size_t minimumMachineOperandValue = std::stoi(std::string(minimumMachineOperandValueToken.value));

  //parse the identifier token.
  const auto info = parseRegisterRange(std::string{identifierToken.value}, identifierToken);

  // insert registers
  if (info.hasDash) {
    //insert multiple
    size_t currentIndex = info.lowValue;
    size_t offset = 0;
    while (currentIndex <= info.highValue) {
      RegisterDefinition reg(info.prefix + std::to_string(currentIndex), minimumMachineOperandValue+offset, bitwidth);
      if (m_keywordByTypeMap.find(reg.m_registerName) != m_keywordByTypeMap.end()) {
        logError(identifierToken, std::format("Register \"{}\" already defined.", reg.m_registerName));
      } else {
        m_registerSet.emplace(reg.m_registerName, std::move(reg));
        m_keywordByTypeMap.emplace(reg.m_registerName, KeywordType::REGISTER);
      }
      
      offset++;
      currentIndex++;
    }
  } else {
    //insert one
    RegisterDefinition reg(std::string(identifierToken.value), minimumMachineOperandValue, bitwidth);
    if (m_keywordByTypeMap.find(reg.m_registerName) != m_keywordByTypeMap.end()) {
      logError(identifierToken, std::format("Register \"{}\" already defined.", reg.m_registerName));
      return;
    }
    m_registerSet.emplace(reg.m_registerName, std::move(reg));
    m_keywordByTypeMap.emplace(reg.m_registerName, KeywordType::REGISTER);
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
    const auto tk = sourceHolder.consume();
    auto [opsize, errm] = std::safe_stoi((std::string)tk.value);
    if (!errm.empty()) {
      logError(tk, errm);
    }
    format.insertOperandSize(opsize);
  }

  if (m_keywordByTypeMap.find(format.m_formatName) != m_keywordByTypeMap.end()) {
    logError(identifierToken, std::format("Format \"{}\" already defined.", format.m_formatName));
    return;
  }
  m_formatSet.emplace(format.m_formatName, std::move(format));
  m_keywordByTypeMap.emplace(format.m_formatName, KeywordType::FORMAT);
}

void Architecture::consumeBitwidth(TokenHolder& sourceHolder) {
  sourceHolder.skip(); //skip keyword
  const auto& bitwidthToken = sourceHolder.peek();

  auto [bitwidth, errm] = std::safe_stoi((std::string)bitwidthToken.value);
  if (!errm.empty()) {
    logError(bitwidthToken, errm);
  }

  m_bitwidth = bitwidth;

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
  auto [opcodeNumber, errm] = std::safe_stoi((std::string)opcodeToken.value);
  if (!errm.empty()) {
    logError(opcodeToken, errm);
  }
  auto [byteLengthNumber, errmb] = std::safe_stoi((std::string)byteLengthToken.value);
  if (!errmb.empty()) {
    logError(byteLengthToken, errmb);
  }
  std::cout << "Adding instruction: " << identifierToken.value << "\n";
  InstructionDefinition instruction(std::string(identifierToken.value), 
                                    opcodeNumber,
                                    byteLengthNumber,
                                    it->second);

  if (m_keywordByTypeMap.find(instruction.m_name) != m_keywordByTypeMap.end()) {
    logError(identifierToken, std::format("Instruction \"{}\" already defined.", instruction.m_name));
    return;
  }
  m_keywordByTypeMap.emplace(instruction.m_name, KeywordType::INSTRUCTION);
  
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
          //size_t offset = 0;
          while (indx <= info.highValue) {
            operand.acceptedRegisterNames.emplace(info.prefix + std::to_string(indx));
            //offset++;
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
          auto [minNum, errm] = std::safe_stoi(currentSlice);
          if (!errm.empty()) {
            logError(immediateToken, errm);
          }
          min = minNum;
          currentSlice.clear();
        } else {
          currentSlice.push_back(c);
        }
        indx++;
      }
      auto [maxNum, errm] = std::safe_stoi(currentSlice);
      if (!errm.empty()) {
        logError(immediateToken, errm);
      }
      max = maxNum;
      if (!hasHalfed) {
        logError(immediateToken, "Immediate does not have valid range, did you forget an alias?");
        return;
      }
      ImmediateOperand operand(std::string(aliasToken.value), min, max);
      
      instruction.m_operands.push_back(std::move(operand));
      
    } else if (sourceHolder.peek().value == "CONST") {
      sourceHolder.skip();
      const auto immediateToken = sourceHolder.consume();
      
      if (immediateToken.value.size() > 0 && isdigit(immediateToken.value[0])) {
        auto [opnum, errm] = std::safe_stoi((std::string)immediateToken.value);
        if (!errm.empty()) {
          logError(immediateToken, errm);
        }
        ConstantIntOperand operand(opnum);
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
  
  
  m_instructionSet.emplace(instruction.m_name, std::move(instruction));
  
  
}

Architecture::RegisterRangeInfo Architecture::parseRegisterRange(const std::string& string, const Token& errToken) const {
  RegisterRangeInfo info;
  
  size_t index = 0;
  std::string currentValue;
  while (index < string.size()) {
    const char c = string[index++];
    if (std::isdigit(c)) {
      currentValue.push_back(c);
    } else if (std::isalpha(c)) {
      if (info.hasDash) {continue;}
      info.prefix.push_back(c);
    } else if (c == '-') {
      if (info.hasDash) {
        logError(errToken, "Register range has multiple dashes in definition.");
      } else {
        auto [lowValue, errm] = std::safe_stoi(currentValue);
        if (!errm.empty()) {
          logError(errToken, errm);
        }
        info.lowValue = lowValue;
        info.hasDash = true;
        currentValue.clear();
      }
    } else {
      logError(errToken, "Got unexpected character in register range definition.");
    }
  }
  auto [highValue, errm] = std::safe_stoi(currentValue);
  if (!errm.empty() && info.hasDash) {
    logError(errToken, errm);
  }
  info.highValue = highValue;
  return info;
}

Architecture architecturePipeline(std::filesystem::path& archPath, Debug::FullLogger* logger) {
  auto src = FileHelper::openFileToString(archPath);
  ArchLexer lexer(src);
  auto tokens = lexer.run(src, archPath);
  Architecture arch(tokens, logger);
  return arch;
}

Architecture::KeywordType Architecture::getKeywordTypeOfWord(const std::string_view& word) {
  const auto& it = m_keywordByTypeMap.find(std::string(word));
  if (it == m_keywordByTypeMap.end()) {
    return KeywordType::NONE;
  } else {
    return it->second;
  }
}


void Architecture::debugPrint() const {
  std::cout << "========== ARCHITECTURE ==========\n";
  std::cout << "Bitwidth: " << m_bitwidth << "\n\n";

  // ---------------- REGISTERS ----------------
  std::cout << "---- Registers ----\n";
  for (const auto& [name, reg] : m_registerSet) {
    std::cout
      << "Register: " << reg.m_registerName
      << " | OperandValue: " << reg.m_operandValue
      << " | Bitwidth: " << reg.m_bitwidthValue
      << "\n";
  }
  std::cout << "\n";

  // ---------------- CONTROL SIGNALS ----------------
  std::cout << "---- Control Signals ----\n";
  for (const auto& [name, sig] : m_controlSignalSet) {
    std::cout
      << "ControlSignal: " << sig.m_controlSignalName
      << " | AssertBitIndex: " << sig.m_assertBitIndex
      << "\n";
  }
  std::cout << "\n";

  // ---------------- FORMATS ----------------
  std::cout << "---- Formats ----\n";
  for (const auto& [name, format] : m_formatSet) {
    std::cout << "Format: " << format.m_formatName << " | OperandSizes: ";
    for (size_t size : format.m_formatOperandSizes) {
      std::cout << size << " ";
    }
    std::cout << "\n";
  }
  std::cout << "\n";

  // ---------------- INSTRUCTIONS ----------------
  std::cout << "---- Instructions ----\n";
  for (const auto& [name, instr] : m_instructionSet) {
    std::cout
      << "Instruction: " << instr.m_name
      << " | Opcode: " << instr.m_opcode
      << " | ByteLength: " << instr.m_byteLength
      << " | Format: " << instr.m_format.m_formatName
      << "\n";

    std::cout << "  Operands:\n";
    for (const auto& operand : instr.m_operands) {
      std::visit([&](const auto& op) {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, RegisterOperand>) {
          std::cout << "    RegisterOperand: alias=" << op.alias << " | accepts: ";
          for (const auto& regName : op.acceptedRegisterNames)
            std::cout << regName << " ";
          std::cout << "\n";
        } else if constexpr (std::is_same_v<T, ImmediateOperand>) {
          std::cout << "    ImmediateOperand: alias=" << op.alias
                    << " | min=" << op.min
                    << " | max=" << op.max << "\n";
        } else if constexpr (std::is_same_v<T, ConstantIntOperand>) {
          std::cout << "    ConstantIntOperand: value=" << op.constant << "\n";
        } else if constexpr (std::is_same_v<T, ConstantStringOperand>) {
          std::cout << "    ConstantStringOperand: value=\"" << op.constant << "\"\n";
        }
      }, operand);
    }
    std::cout << "\n";
  }

  // ---------------- KEYWORD TYPE MAP ----------------
  std::cout << "---- Keyword Type Map ----\n";
  for (const auto& [word, type] : m_keywordByTypeMap) {
    std::string typeStr;
    switch (type) {
      case KeywordType::NONE: typeStr = "NONE"; break;
      case KeywordType::REGISTER: typeStr = "REGISTER"; break;
      case KeywordType::INSTRUCTION: typeStr = "INSTRUCTION"; break;
      case KeywordType::FORMAT: typeStr = "FORMAT"; break;
      case KeywordType::DATATYPE: typeStr = "DATATYPE"; break;
      default: typeStr = "UNKNOWN"; break;
    }
    std::cout << word << " -> " << typeStr << "\n";
  }
  std::cout << "==================================\n";
}
}