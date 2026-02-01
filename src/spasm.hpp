#include <string>
#include <vector>

#define BYTES_PER_INSTRUCTION 2

namespace Spasm {

  // a struct that represents a line of assembly- 
  struct IntermediateLine {

  };

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
  }


}