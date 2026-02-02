#include <string>
#include <vector>
#include <arch.hpp>

// spasm goes from lexer > preprocessor > parser > machine code

#define BYTES_PER_INSTRUCTION 2

namespace Spasm {

  // a struct that represents a line of assembly- not neccesarily an instruction due to datatype declarations
  struct IntermediateLine {

    // the type of assembly the object represents
    enum class Type {
      UNASSIGNED,
      INSTRUCTION,
      DATADECLARACTION
    };

    Type lineType = Type::UNASSIGNED;
    InstructionLiteral instruction;
  };

  struct InstructionLiteral {
    Architecture::Instruction::Instruction& coreInstruction;
  }

  struct Variable {
    bool locationResolved = false;
    size_t location = 0;
    std::string name;
    size_t byteSize = 0;
  };

  struct Label {
    bool locationResolved = false;
    size_t location = 0;
    Label* parentLabel = nullptr;
    std::vector<Variable> stackVariables;
    std::vector<IntermediateLine> assembly;

    size_t byteLength() const {return BYTES_PER_INSTRUCTION * assembly.size();}
  };


}