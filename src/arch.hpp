#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <vector>
#include <cassert>
#include <iostream>

namespace Instruction {
  enum class RangeType {
    IMMEDIATE,
    REGISTER
  };

  struct Range {
    virtual ~Range() = default;
    const RangeType type;

    Range(RangeType t) : type(t) {};
    virtual bool isInRange(int value) const {std::cout << "ASSERTED_arch.hpp A" << std::endl; assert(false); return false;} // shoukld be overriden if called.
    virtual bool isInRange(std::string& reg) const {std::cout << "ASSERTED_arch.hpp B" << std::endl; assert(false); return false;} // shoukld be overriden if called.

    virtual void addToRegisterRange(const std::string& reg) {std::cout << "ASSERTED_arch.hpp C" << std::endl; assert(false);} // shoukld be overriden if called.
    virtual void setMinimum(int value) {std::cout << "ASSERTED_arch.hpp D" << std::endl; assert(false);} // shoukld be overriden if called.
    virtual void setMaximum(int value) {std::cout << "ASSERTED_arch.hpp E" << std::endl; assert(false);} // shoukld be overriden if called.

    virtual std::string toString() {std::cout << "ASSERTED_arch.hpp F" << std::endl; assert(false); return std::string{};} // shoukld be overriden if called.
  };

  struct ImmediateRange : Range {
    int min;
    int max;

    bool isInRange(int value) const override {return min <= value && value <= max;}
    void setMinimum(int value) override {min = value;}
    void setMaximum(int value) override {max = value;}
    std::string toString() override {return "min: " + std::to_string(min) + "\n max: " + std::to_string(max);}

    ImmediateRange(int min, int max)
      : Range(RangeType::IMMEDIATE), min(min), max(max) {};
  };

  struct RegisterRange : Range {
    std::unordered_set<std::string> registers;
    RegisterRange(const std::unordered_set<std::string>& regs)
      : Range(RangeType::REGISTER), registers(regs) {};
    RegisterRange()
      : Range(RangeType::REGISTER) {};
    
    std::string toString() override {std::string ret; for (const auto& reg : registers) {ret.append(reg + ", ");} return ret;}
    void addToRegisterRange(const std::string& reg) override {registers.insert(reg);}
    bool isInRange(std::string& reg) const override {return registers.find(reg) != registers.end();};
  };

  enum class ArgumentTypes {
    REGISTER,
    IMMEDIATE,
    INVALID,
    UNKNOWN
  };
  
  struct Argument {
    ArgumentTypes type = ArgumentTypes::UNKNOWN;
    std::shared_ptr<Range> range;
    std::string alias;

    std::string toString(size_t padding = 0) const;
    constexpr const char* typeToString() const {
      switch (this->type) {
          case ArgumentTypes::IMMEDIATE:    return "IMMEDIATE";
          case ArgumentTypes::REGISTER:   return "REGISTER";
          case ArgumentTypes::INVALID:     return "INVALID";
          case ArgumentTypes::UNKNOWN:     return "UNKNOWN";
          default:                    return "UNKNOWN";
        }
    }
  };
  
  struct Instruction {
    std::string name = "UNNAMED";
    std::vector<Argument> arguments;
    std::string formatAlias;
    int opcode = -1;

    void print(size_t padding = 0) const;
  };
}

struct DataType {
  std::string name;
  size_t length;
  bool autoLength;

  void print(size_t padding = 0) const;
};

class Architecture {
  public:
  struct Format {
    std::string alias;
    std::vector<int> operandBitwidth;

    void print(size_t padding = 0) const;
  };

  std::unordered_map<std::string, Instruction::Instruction> m_instructionSet;
  std::unordered_set<std::string> m_instructionNameSet;
  std::unordered_set<std::string> m_nameSet;
  std::unordered_map<std::string, DataType> m_dataTypes;
  std::unordered_map<std::string, Format> m_formats;
  int m_bitwidth = -1;

  bool isInstruction(const std::string& instructionName) {
    return m_instructionSet.find(instructionName) != m_instructionSet.end();
  }
};
