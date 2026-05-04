#include "Spasm/OutputGenerator.hpp"

#include "Helpers/FileHelper.hpp"

namespace Spasm {
void OutputGenerator::run(
  SMake::Target& target,
  Linker& linker,
  Linker::LinkedResult& linkedResult,
  size_t entrySymbolJumpByteLength,
  Debug::FullLogger* logger
) {
  p_logger = logger;

  if (linkedResult.maxAddress == 0) {
    return;
  }

  std::vector<uint8_t> binaryData;
  binaryData.resize(linkedResult.maxAddress);

  // jump to entry symbol bytes
  Spasm::Program::IdentifierMapStringType::iterator it;
  bool foundIt = false;
  for (const auto& translationUnit : linker.m_allTranslationUnits) {
    auto localIt = translationUnit->m_identifierFullNameMap.find(std::string(target.m_entrySymbol + '.'));
    if (localIt != translationUnit->m_identifierFullNameMap.end()) {
      it = localIt;
      foundIt = true;
      break;
    }
  }
  if (foundIt) {
    size_t address = linkedResult.addressHolder[(*it->second)->addressIndex];
    const uint8_t header[] = { 
      0x40, 0x6C, 
        static_cast<uint8_t>( address        & 0xff ),
        static_cast<uint8_t>((address >> 8 ) & 0xff ),
      0x80, 0x6C, 
        static_cast<uint8_t>((address >> 16) & 0xff ),
        static_cast<uint8_t>((address >> 24) & 0xff ),
      0x01, 0x6B,
      0x82, 0x6B,
      0x10, 0xA8
    };
    std::memcpy(binaryData.data(), header, entrySymbolJumpByteLength);
  }


  while (!linkedResult.translationUnitQueue.empty()) {
    const auto& translationUnit = *linkedResult.translationUnitQueue.front();
    linkedResult.translationUnitQueue.pop();

    fillBytesFromDataDeclarations(linkedResult, translationUnit, binaryData);

    for (const auto& statement : translationUnit.m_statementVector) {
      switch (statement->getKind())
      {
      case Program::StatementSymbol::Kind::INSTRUCTION: {
        auto& instructionStatement = *static_cast<Program::InstructionSymbol*>(statement.get());
        fillInstruction(linkedResult, instructionStatement, binaryData);
        break;
      }
      case Program::StatementSymbol::Kind::RELAXOR: {
        auto& relaxorStatement = *static_cast<Program::RelaxorSymbol*>(statement.get());
        size_t instructionByteOffset = linkedResult.addressHolder[relaxorStatement.addressIndex];
        while (!p_logger->Errors.isEmpty()) {
          std::cout << p_logger->Errors.consumeMessage() << std::endl;
        }
        for (const auto& instruction : relaxorStatement.relaxor.options[relaxorStatement.optionIndex].optionStatements) {
          assert(instruction);
          if (instruction->getKind() != Program::StatementSymbol::Kind::INSTRUCTION) {
            logError(instruction->location, "Non instruction statement forbidden inside relaxor.");
            continue;
          }

          auto& instructionStatement = *static_cast<Program::InstructionSymbol*>(instruction.get());
          fillInstruction(linkedResult, instructionStatement, binaryData, instructionByteOffset);
          instructionByteOffset += instructionStatement.byteSize;
          //align if not aligned
          instructionByteOffset += (instructionByteOffset % 2) == 1;
        }
        
        break;
      }
      
      default:
        break;
      }
    }
  }

  std::cout << "\nHex Dump:";
  for (size_t byteIndex = 0; byteIndex < binaryData.size(); byteIndex++) {

    if (! (byteIndex % 8) ) {
      std::cout << '\n' << std::hex << std::setw(4) << std::setfill('0') << byteIndex << ":";
    }
    std::cout << ' ' 
              << std::hex << std::uppercase
              << std::setw(2) << std::setfill('0')
              << static_cast<int>(binaryData[byteIndex]);
  }

  std::cout << "\nRaw Hex Dump:\n";
  for (const auto& byte : binaryData) {
    std::cout << std::hex << std::uppercase
              << std::setw(2) << std::setfill('0')
              << static_cast<int>(byte);
  }
 
  std::cout << std::dec << std::endl;
  
  FileHelper::writeBytesToFile(binaryData, std::filesystem::weakly_canonical(std::filesystem::path(target.m_outputDirectory) / std::filesystem::path(target.m_outputName)), p_logger);
}

void OutputGenerator::fillBytesFromDataDeclarations(const Linker::LinkedResult& linkedResult, const Program::TranslationUnit& translationUnit, std::vector<uint8_t>& binaryData) {
  for (const auto& statement : translationUnit.m_definitionVector) {
    auto& dataObject = *statement->dataObject;
    if (!dataObject.rawDataValid) {
      //make raw data valid
      dataObject.data.resize(dataObject.elementCount * dataObject.elementSize, 0);

      size_t exprIndex = 0;
      for (const auto& expr : dataObject.exprData) {
        //convert int to bytes
        const uint32_t unsignedValue = (uint32_t)expr->getValue();
        if (unsignedValue > std::pow(2,8*dataObject.elementSize)) {
          logError(expr->location, std::format("Number \"{}\" exceeds the unsigned limit of {} bytes", expr->getValue(), dataObject.elementSize));
        } else {
          std::memcpy(dataObject.data.data() + exprIndex * dataObject.elementSize, &unsignedValue, std::min(sizeof(uint32_t),dataObject.elementSize));
        }
        exprIndex++;
      }
    }
    //raw data now valid
    //pack binary
    std::memcpy(binaryData.data() + linkedResult.addressHolder[dataObject.addressIndex], dataObject.data.data(), dataObject.data.size());
  }
    
}

void OutputGenerator::fillInstruction(Linker::LinkedResult& linkedResult, Program::InstructionSymbol& instructionStatement, std::vector<uint8_t>& binaryData, int addressOverride) {
  auto& instructionDefinition = instructionStatement.instruction;
  
  uint16_t mainInstructionBits = 0;
  mainInstructionBits |= instructionDefinition.m_opcode << 10; //place opcode in msb
  std::vector<uint8_t> extraData;
  int fieldShiftAmount = 10;
  for (size_t operandIndex = 0; operandIndex < instructionDefinition.m_operands.size(); operandIndex++) {
    const size_t opPlusOne = operandIndex+1;
    size_t fieldSize = 0;
    if (instructionDefinition.m_format.m_formatOperandSizes.size() > opPlusOne) {
      fieldSize = instructionDefinition.m_format.m_formatOperandSizes[operandIndex+1];
      fieldShiftAmount -= fieldSize; //+1 to skip the opcode field
    } else {
      fieldShiftAmount = -1;
    }
    

    uint16_t value = 0;

    const auto& operandPtr = instructionStatement.operands[operandIndex];
    if (!operandPtr) continue;
    
    //imm and immx need validation, others can already B trusted
    
      std::visit([&](auto& opDef) {
      using T = std::decay_t<decltype(opDef)>;

      if constexpr (std::is_same_v<T, Arch::Architecture::ConstantIntOperand>) {
        value = operandPtr->getValue();
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ConstantStringOperand>) {
        value = operandPtr->getValue();
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::RegisterOperand>) {
        value = operandPtr->getValue();
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ImmediateOperand>) {
        value = operandPtr->getValue();
        if (value < opDef.min || value > opDef.max) {
          logError(instructionStatement.location, std::format("Evaluated expression out of field bounds, evaluated: {}, expected {} < value < {}", value, opDef.min, opDef.max));
          value = 0;
        }
        
        //make twos comp of field size if negative
        if (value < 0) {
          uint32_t mask = (1u << fieldSize) - 1;
          value = static_cast<uint32_t>(value) & mask;
        }
      }
      else if constexpr (std::is_same_v<T, Arch::Architecture::ExternalImmediateOperand>) { 
        unsigned long long sValue = operandPtr->getValue();
        if (sValue < opDef.min || sValue > opDef.max) {
          logError(instructionStatement.location, std::format("Evaluated expression out of field bounds, evaluated: {}, expected {} < value < {}", value, opDef.min, opDef.max));
          sValue = 0;
        }
        
        //make twos comp of field size if negative
        if (sValue < 0) {
          unsigned long long mask = (1ull << fieldSize) - 1;
          sValue &= mask;
        }

        const size_t extraDataOgSize = extraData.size();
        extraData.resize(extraDataOgSize + opDef.byteLength);
        std::memcpy(extraData.data() + extraDataOgSize, &sValue, std::min(sizeof(unsigned long long),opDef.byteLength));
      }
    }, instructionDefinition.m_operands[operandIndex]);
    
    if (fieldShiftAmount >= 0) {
      //should exclude immx
      mainInstructionBits |= (value << fieldShiftAmount);
    }
  }

  const size_t& address = -1 < addressOverride ? addressOverride : linkedResult.addressHolder[instructionStatement.addressIndex];

  std::string extraString = " ";
  extraString.reserve(extraData.size() * 2);
  if (extraData.size() == 0)
    extraString += "    ";
  for (uint8_t byte : extraData)
    extraString += std::format("{:02x}", byte);
  std::string debugString = std::format("{:07x}: {:04x}{}   ; {}", address, mainInstructionBits, extraString, instructionStatement.source);
  std::cout << debugString;


  
  std::memcpy(binaryData.data() + address, &mainInstructionBits, 2);
  if (!extraData.empty()) {
    std::memcpy(binaryData.data() + address + 2, extraData.data(), extraData.size());
  }
}
}