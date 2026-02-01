#include "archBuilder.hpp"
#include <cassert>
#include <algorithm>


Architecture ArchBuilder::build(std::vector<ArchToken::ArchToken>& tks) {
  Architecture arch;
  tokens = &tks;
  pos = 0;

  while (!isAtEnd()) {
    const auto& token = peek();
    std::cout << "ARCHBUILDER.CPP ANNOUNCE: the following if statements need to be completed so that the offsets arent all out of wack for the instr" << std::endl;
    if (token.type == ArchToken::ArchTokenTypes::KEYWORD) {
      if (token.value == "format") {
        advance();
        Architecture::Format format;
        if (isAtEnd() || peek().type != ArchToken::ArchTokenTypes::IDENTIFIER) {this->logError("Invalid format at " + token.positionToString()); continue;}
        
        const auto& formatToken = advance();
        format.alias = formatToken.value;
        
        int bitwidth = 0;
        while (!isAtEnd() && peek().type == ArchToken::ArchTokenTypes::IDENTIFIER) {
          const int operandBitwidth = std::stoi(advance().value);
          bitwidth += operandBitwidth;
          format.operandBitwidth.push_back(operandBitwidth);
        }
        if (bitwidth != arch.m_bitwidth) {this->logError("Invalid bitwidth of format \"" + format.alias + "\" at " + formatToken.positionToString());}
        arch.m_formats.emplace(format.alias, std::move(format));

      } else if (token.value == "bitwidth") {
        advance();
        if (!isAtEnd() && peek().type == ArchToken::ArchTokenTypes::IDENTIFIER) {
          arch.m_bitwidth = std::stoi(advance().value);
        }
      } else if (token.value == "layout") {
        advance();
      } else if (token.value == "datatype") {
        advance();
        if (isAtEnd() || peek().type != ArchToken::ArchTokenTypes::IDENTIFIER) {this->logError("Invalid data typename at " + token.positionToString()); continue;}
        const std::string dataTypeName = peek().value;
        advance();
        if (isAtEnd() || peek().type != ArchToken::ArchTokenTypes::IDENTIFIER) {this->logError("Invalid data type byte length at " + token.positionToString()); continue;}
        //if ()
        assert(false);
        // uncomplete
      } else {
        std::cout << "ASSERTED_A" << std::endl;
        assert(false);
        //bad!
      }
    } else {
      //new instruction hopefully
      Instruction::Instruction instruction;
      instruction.opcode = arch.m_instructionSet.size();
      const auto& nameToken = advance();
      if (nameToken.type == ArchToken::ArchTokenTypes::NEWLINE) {
        arch.m_nameSet.emplace(instruction.name);
        arch.m_instructionSet.emplace(instruction.name, std::move(instruction));
        continue;
      }
      instruction.name = nameToken.value;
      if (isAtEnd()) {logError("incomplete instruction @ " + token.positionToString()); continue;}

      const auto& formatToken = advance();
      if (formatToken.type == ArchToken::ArchTokenTypes::NEWLINE) {
        arch.m_nameSet.emplace(instruction.name);
        arch.m_instructionSet.emplace(instruction.name, std::move(instruction));
        continue;
      }
      instruction.formatAlias = formatToken.value;
      while (peek().type == ArchToken::ArchTokenTypes::ARGUMENTTYPE) {
        instruction.arguments.emplace_back(parseArgument());
      }

      arch.m_nameSet.emplace(instruction.name);
        arch.m_instructionSet.emplace(instruction.name, std::move(instruction));
    }
  }

  return arch;
}

Instruction::Argument ArchBuilder::parseArgument() {
  Instruction::Argument argument;
  const ArchToken::ArchToken& typeToken = advance();
  if (typeToken.value == "REG") {
    argument.type = Instruction::ArgumentTypes::REGISTER;
  } else if (typeToken.value == "IMM") {
    argument.type = Instruction::ArgumentTypes::IMMEDIATE;
  } else {
    argument.type = Instruction::ArgumentTypes::INVALID;
  }

  if (isAtEnd() || peek().type != ArchToken::ArchTokenTypes::IDENTIFIER) {logError("incomplete argument @ " + typeToken.positionToString()); return argument;}

  argument.alias = advance().value;

  if (isAtEnd() || peek().type != ArchToken::ArchTokenTypes::IDENTIFIER) {logError("incomplete argument @ " + typeToken.positionToString()); return argument;}

  //parse the range :(

  const ArchToken::ArchToken& rangeToken = advance();
  size_t tokenPos = 0;
  argument.range = std::make_shared<Instruction::RegisterRange>();
  while (tokenPos < rangeToken.value.length()) {
    const std::string currentRange = parseRange(rangeToken.value,&tokenPos);
    std::string lowerStr;
    std::string lowerNumber;
    std::string higherNumber;
    std::string prefix;
    std::string postfix;
    size_t dashPos = 0;
    bool isRange = false;
    while (dashPos < currentRange.length()) {
      const char c = currentRange[dashPos];
      if (c == '-') {isRange = true;}
      else if (!isdigit(c) && !isRange) {prefix.push_back(c);}
      else if (!isdigit(c) && isRange) {postfix.push_back(c);}
      else if (isdigit(c) && !isRange) {lowerNumber.push_back(c);}
      else if (isdigit(c) && isRange) {higherNumber.push_back(c);}
      lowerStr.push_back(c);
      dashPos++;
    }
    
    if (isRange) {
      if (prefix != postfix) {logError("mismatched range prefix \"" + prefix + "\" and \"" + postfix + "\" @ " + rangeToken.positionToString()); return argument;}
      
      const int minReg = std::stoi(lowerNumber);
      const int maxReg = std::stoi(higherNumber);
      if (minReg > maxReg) {logError("invalid range of registers (err is: min > max) @ " + rangeToken.positionToString()); return argument;}
      
      for (int i = minReg; i <= maxReg; i++) {
        argument.range.get()->addToRegisterRange(prefix + std::to_string(i));
      }
    } else {
      argument.range.get()->addToRegisterRange(currentRange);
    }
  }

  return argument;
}

const std::string ArchBuilder::parseRange(const std::string& str, size_t* pos) const {
  std::string range;
  while ((*pos) < str.length() && str[*pos] != '<') {
    range.push_back(str[*pos]);
    (*pos)++;
  }
  if (*pos < str.length() && str[*pos] == '<') {(*pos)++;}
  return range;
}

std::string ArchBuilder::consumeError() {
  if (errors.empty()) return {};
  std::string last = std::move(errors.back());
  errors.pop_back();
  return last;
}

std::string ArchBuilder::consumeWarning() {
  if (warnings.empty()) return {};
  std::string last = std::move(warnings.back());
  warnings.pop_back();
  return last;
}