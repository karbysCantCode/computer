#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <cassert>

namespace Arch {

struct Token {
  enum class Type {
    UNASSIGNED,
    KEYWORD,
    OPENPAREN,
    CLOSEPAREN,
    OPENBLOCK,
    CLOSEBLOCK,
    STRING,
    IDENTIFIER,
    COMMA,
    INVALID
  };
  std::string m_value;
  Type m_type = Type::UNASSIGNED;
  int m_line = -1;
  int m_column = -1;

  inline void setString(const std::string& newString) {m_value=newString;}
  inline void setType(const Type& newType) {m_type=newType;}
  std::string toString(size_t padding) const;
  std::string positionToString() const {return m_line < 0 || m_column < 0 ? std::string("END OF FILE") : "line " + std::to_string(m_line) + ", column " + std::to_string(m_column);}
  constexpr const char* typeToString();
  
  Token(const std::string& val, const Type t, int ln, int col) :
    m_value(val), m_type(t), m_line(ln), m_column(col) {}
  
  Token(const char val, const Type t, int ln, int col) :
    m_value{val}, m_type(t), m_line(ln), m_column(col) {}
};
namespace Instruction {

  struct Range {
    enum class Type {
      IMMEDIATE,
      REGISTER
    };
    virtual ~Range() = default;
    const Type m_type;

    Range(Type t) : m_type(t) {};
    virtual bool isInRange(int value) const {std::cout << "ASSERTED_arch.hpp A" << std::endl; assert(false); return false;} // shoukld be overriden if called.
    virtual bool isInRange(std::string& reg) const {std::cout << "ASSERTED_arch.hpp B" << std::endl; assert(false); return false;} // shoukld be overriden if called.

    virtual void addToRegisterRange(const std::string& reg) {std::cout << "ASSERTED_arch.hpp C" << std::endl; assert(false);} // shoukld be overriden if called.
    virtual void setMinimum(int value) {std::cout << "ASSERTED_arch.hpp D" << std::endl; assert(false);} // shoukld be overriden if called.
    virtual void setMaximum(int value) {std::cout << "ASSERTED_arch.hpp E" << std::endl; assert(false);} // shoukld be overriden if called.

    virtual std::string toString() {std::cout << "ASSERTED_arch.hpp F" << std::endl; assert(false); return std::string{};} // shoukld be overriden if called.
  };

  struct ImmediateRange : Range {
    int m_min;
    int m_max;

    bool isInRange(int value) const override {return m_min <= value && value <= m_max;}
    void setMinimum(int value) override {m_min = value;}
    void setMaximum(int value) override {m_max = value;}
    std::string toString() override {return "min: " + std::to_string(m_min) + ", max: " + std::to_string(m_max);}

    ImmediateRange(int min, int max)
      : Range(Type::IMMEDIATE), m_min(min), m_max(max) {};
  };

  struct RegisterRange : Range {
    std::unordered_set<std::string> m_registers;
    RegisterRange(const std::unordered_set<std::string>& regs)
      : Range(Type::REGISTER), m_registers(regs) {};
    RegisterRange()
      : Range(Type::REGISTER) {};
    
    std::string toString() override {std::string ret; for (const auto& reg : m_registers) {ret.append(reg + ", ");} return ret;}
    void addToRegisterRange(const std::string& reg) override {m_registers.insert(reg);}
    bool isInRange(std::string& reg) const override {return m_registers.find(reg) != m_registers.end();};
  };

  struct Argument {
    enum class Type {
      REGISTER,
      IMMEDIATE,
      NONE,
      INVALID,
      UNKNOWN
    };
      
    Type m_type = Type::UNKNOWN;
    std::shared_ptr<Range> m_range;
    std::string m_alias;

    std::string toString(size_t padding = 0) const;
    constexpr const char* typeToString() const {
      switch (this->m_type) {
          case Type::IMMEDIATE:    return "IMMEDIATE";
          case Type::REGISTER:   return "REGISTER";
          case Type::INVALID:     return "INVALID";
          case Type::UNKNOWN:     return "UNKNOWN";
          default:                    return "UNKNOWN";
        }
      }
  };
  
  struct Instruction {
    std::string m_name = "UNNAMED";
    std::vector<Argument> m_arguments;
    std::string m_formatAlias;
    int m_opcode = -1;

    std::string toString(size_t padding = 0) const;
  };
}

struct RegisterIdentity {
  std::string m_name;
  size_t m_bitWidth = 0;
  size_t m_machineCodeOperandValue = 0;
};

struct DataType {
  std::string m_name;
  size_t m_length;
  bool m_autoLength;

  std::string toString(size_t padding = 0) const;
};

struct Format {
  std::string m_alias;
  std::vector<int> m_operandBitwidth;

  std::string toString(size_t padding = 0) const;
};

class Architecture {
  public:
  

  std::unordered_map<std::string, Instruction::Instruction> m_instructionSet;
  std::unordered_set<std::string> m_instructionNameSet;
  std::unordered_set<std::string> m_nameSet;
  std::unordered_map<std::string, RegisterIdentity> m_registers;
  std::unordered_map<std::string, DataType> m_dataTypes;
  std::unordered_map<std::string, Format> m_formats;
  int m_bitwidth = -1;

  bool isInstruction(const std::string& instructionName) {
    return m_instructionSet.find(instructionName) != m_instructionSet.end();
  }
};

std::vector<
}