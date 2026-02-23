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
      DATADECLARACTION,
      LABEL
    };

    Type lineType = Type::UNASSIGNED;

    virtual ~IntermediateLine() = default;
  };
  struct InstructionLiteral : IntermediateLine {
    Architecture::Instruction::Instruction* coreInstruction = nullptr;
    std::vector<Argument> args;
  };

  struct DataDeclarationLiteral : IntermediateLine {
    Variable* variableObject;
    std::vector<Argument> args;
  };



  

  struct Argument {
    virtual ~Argument() = default;
  };

  struct Literal : Argument {
    virtual ~Literal() = default;
  };


  struct Expression : Literal{
    enum class Method {
      ADD,
      SUB,
      MUL,
      DIV,
      AND,
      NOT,
      OR,
      XOR
    };

    Method methodType;
    std::unique_ptr<Literal> right;
    std::unique_ptr<Literal> left;

  };

  struct Constant : Literal {
    int value;
  };

  struct Register : Argument {
    //point to arch reg opbject 
    Architecture::Registers::RegisterIdentity* registerIdentity;
  };

  struct Variable : Argument {
    bool locationResolved = false;
    size_t location = 0;
    std::string name;
    size_t byteSize = 0;
  };

  struct Label : Argument {
    bool locationResolved = false;
    size_t location = 0;
    Label* parentLabel = nullptr;
    std::string name;
  };
}