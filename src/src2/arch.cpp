#include "arch.hpp"

#include <sstream>
#include <charconv>

#include "lexHelper.hpp"
#include "fileHelper.hpp"

std::string Arch::Instruction::Instruction::toString(size_t padding, size_t ident) const {
  std::ostringstream str;
  std::string indent(ident, ' ');
  std::string indent2(padding + ident, ' ');
  str <<       indent << "Instruction name: " << m_name << '\n'
            << indent2 << "Opcode: \"" << m_opcode << "\"" << '\n'
            << indent2 << "Format alias: " << m_formatAlias << '\n'
            << indent2 << "Arguments: " << '\n'; 

  for (const auto& arg : m_arguments) {
    str << arg->toString(padding, ident + padding*2);
  }

  str << '\n';
  return str.str();
}

std::string Arch::Instruction::RangeArgument::toString(size_t padding, size_t ident) const {
  switch (m_type) {
    case Type::NONE:
    return "NONETYPE";
    case Type::UNKNOWN:
    return "UNKNOWNTYPE";
    case Type::INVALID:
    return "INVALIDTYPE";
    default:
    break;
  }
  return std::string(ident, ' ') + "Argument Alias: " + m_alias + '\n'
   + std::string(padding + ident, ' ') + "Type: " + typeToString() + '\n'
   + std::string(padding + ident, ' ') + "Range: " + m_range.get()->toString() + '\n';
}

std::string Arch::Instruction::ConstantArgument::toString(size_t padding, size_t ident) const {
  return std::string(ident, ' ') + "Argument Alias: " + m_alias + '\n'
   + std::string(padding + ident, ' ') + "Type: " + typeToString() + '\n'
   + std::string(padding + ident, ' ') + "Value: " + std::to_string(m_constant) + '\n';
}

std::string Arch::Format::toString(size_t padding, size_t ident) const {
  std::ostringstream str;
  std::string indent(ident, ' ');
  std::string indent2(ident + padding, ' ');
 str << indent << "Format name: " << this->m_alias << '\n'
     << indent2 << "Operand bit widths: ";
  for (const auto& operandBits : this->m_operandBitwidth) {
    str << operandBits << ", ";
  }
  str << '\n';
  return str.str();
}

std::string Arch::DataType::toString(size_t padding, size_t ident) const {
  std::ostringstream str;
  std::string indent(ident, ' ');
  std::string indent2(padding + ident, ' ');
  str << indent << "Name: " << m_name << '\n';
  if (m_autoLength) {
    str << indent2 << "Length: Auto Length";
  } else {
    str << indent2 << "Length: " << m_length;
  }
  str << '\n';
  return str.str();
}

constexpr const char* Arch::Lexer::Token::typeToString() const {
  switch (m_type) {
    case Type::KEYWORD:    return "KEYWORD";
    case Type::IDENTIFIER:    return "IDENTIFIER";
    case Type::ARGUMENTTYPE:     return "ARGUMENTTYPE";
    case Type::UNASSIGNED:     return "UNASSIGNED";
    case Type::NEWLINE:     return "NEWLINE";
    default:                    return "UNKNOWN";
  }
}

std::string Arch::Lexer::Token::toString(size_t padding) const {
  std::ostringstream str;
  std::string indent(padding, ' ');
  str << indent << "Token Type: " << this->typeToString() << '\n'
            << indent << "    " << "Token Value: \"" << this->m_value << "\"" << '\n'
            << indent << "    " << "Line: " << this->m_line << '\n'
            << indent << "    " << "Col:  " << this->m_column << std::endl;
  return str.str();
}

std::vector<Arch::Lexer::Token> Arch::Lexer::lex(std::filesystem::path& sourcePath, Debug::FullLogger* logger) {
  auto errptr = logger == nullptr ? nullptr : &logger->Errors;
  LexHelper lexHelper(FileHelper::openFileToString(sourcePath),nullptr,errptr);
  if (lexHelper.m_source.empty()) {return {};}
  std::vector<Token> tokens;

  #define notAtEnd() lexHelper.notAtEnd()
  #define skipWhitespace(arg) lexHelper.skipWhitespace(arg)
  #define peek(arg) lexHelper.peek(arg)
  #define line lexHelper.m_line
  #define column lexHelper.m_column
  #define consume() lexHelper.consume()
  #define skipComment(arg) lexHelper.skipComment(arg)
  #define getUntilWordBoundary() lexHelper.getUntilWordBoundary()

  while (notAtEnd()) {
    if (skipWhitespace(true)) {tokens.emplace_back(std::string{'\n'}, Lexer::Token::Type::NEWLINE, line, column);}
    if (!notAtEnd()) {break;}

    char c = peek();
    
    if (c == ';') {
      skipComment(false);
    } else if (c == '.') {
      consume();
      tokens.emplace_back(getUntilWordBoundary() ,Lexer::Token::Type::KEYWORD, line, column);
    } else if (c == '\n') {
      tokens.emplace_back(std::string{'\n'}, Lexer::Token::Type::NEWLINE, line, column);
    } else {
      const std::string value = getUntilWordBoundary();
      if (value == "REG" || value == "IMM" || value == "CONST" || value == "NON") {
        tokens.emplace_back(value ,Lexer::Token::Type::ARGUMENTTYPE, line, column);
      } else {
        tokens.emplace_back(value ,Lexer::Token::Type::IDENTIFIER, line, column);
      }
      
    }
  }
  return tokens;
  
  #undef notAtEnd
  #undef skipWhitespace
  #undef peek
  #undef line 
  #undef column 
  #undef consume
  #undef skipComment
  #undef getUntilWordBoundary
}

void Arch::assembleTokens(std::vector<Lexer::Token>& tokens, Architecture& targetArch, Debug::FullLogger* logger) {
  size_t pos = 0;  

  #define logError(ErrorMessage) \
  if (logger != nullptr) (logger->Errors.logMessage(ErrorMessage))
  #define logWarning(ErrorMessage) \
  if (logger != nullptr) (logger->Warnings.logMessage(ErrorMessage))

  #define tokenisingError(tokenType, ErrorMessage)          \
  if (!notAtEnd() || peek().m_type != tokenType)        \
  {logError(ErrorMessage); skip(); continue;}

  auto safe_stoi = [&](const std::string&s, int* out) -> bool {
    if (out == nullptr) {return 0;}
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), *out);
    return ec == std::errc() && ptr == s.data() + s.size();
  };

  auto notAtEnd = [&]() -> bool {
    return pos < tokens.size();
  };
  auto peek = [&](size_t t = 0) -> Lexer::Token {
    return (pos+t < tokens.size()) ? 
      tokens[pos+t] 
      : 
      Lexer::Token();
  };
  auto consume = [&]() -> Lexer::Token {
    const auto token = tokens[pos]; 
    pos++; 
    return token;
  };

  auto skip = [&]() -> void {
    pos++;
  };

  auto parseRange = [&](const std::string& str, size_t* pos) -> std::string {
    std::string range;
    while ((*pos) < str.length() && str[*pos] != '<') {
      range.push_back(str[*pos]);
      (*pos)++;
    }
    if (*pos < str.length() && str[*pos] == '<') {(*pos)++;}
    return range;
  };
  auto parseArgument = [&]() -> std::unique_ptr<Arch::Instruction::Argument> {
    std::unique_ptr<Arch::Instruction::Argument> argument;
    const Lexer::Token& typeToken = consume();
    if (typeToken.m_value == "REG") {
      argument = std::make_unique<Arch::Instruction::RangeArgument>(Arch::Instruction::Argument::Type::REGISTER);
    } else if (typeToken.m_value == "IMM") {
      argument = std::make_unique<Arch::Instruction::RangeArgument>(Arch::Instruction::Argument::Type::IMMEDIATE);
    } else if (typeToken.m_value == "NON") {
      argument = std::make_unique<Arch::Instruction::RangeArgument>(Arch::Instruction::Argument::Type::NONE);
      return argument;
    } else if (typeToken.m_value == "CONST") {
      argument = std::make_unique<Arch::Instruction::ConstantArgument>();
      argument->m_type = Arch::Instruction::Argument::Type::CONSTANT;
      if (!notAtEnd() || peek().m_type != Lexer::Token::Type::IDENTIFIER) {logError("incomplete argument @ " + typeToken.positionToString()); return argument;}
      const auto& valueToken = consume();

      if (!safe_stoi(valueToken.m_value, argument.get()->getConstant())) {
        auto it = targetArch.m_registers.find(valueToken.m_value);
        if (it == targetArch.m_registers.end()) {
          logError("Invalid constant argument type \"" + valueToken.m_value + "\" at " + valueToken.positionToString());
          return argument;
        } else {
          if (it->second.m_machineCodeOperandValue < 0) {
            logWarning("Register with no value (less than 0) \"" + it->second.m_name + "\" used at " + valueToken.positionToString());
          }
          *argument->getConstant() = it->second.m_machineCodeOperandValue;
        }
      }
      return argument;
    } else {
      argument->m_type = Arch::Instruction::Argument::Type::INVALID;
    }

    if (!notAtEnd() || peek().m_type != Lexer::Token::Type::IDENTIFIER) {logError("incomplete argument @ " + typeToken.positionToString()); return argument;}
    
    argument->m_alias = consume().m_value;

    if (!notAtEnd() || peek().m_type != Lexer::Token::Type::IDENTIFIER) {logError("incomplete argument @ " + typeToken.positionToString()); return argument;}

    //parse the range :(
    const Lexer::Token& rangeToken = consume();
    switch (argument->m_type) {
      //register range
      case Arch::Instruction::Argument::Type::REGISTER: {
        size_t tokenPos = 0;
        auto range = std::make_unique<Arch::Instruction::RegisterRange>();
        argument->setRange(std::move(range));
        while (tokenPos < rangeToken.m_value.length()) {
          const std::string currentRange = parseRange(rangeToken.m_value,&tokenPos);
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
              const std::string regName = prefix + std::to_string(i);
              const auto it = targetArch.m_registers.find(regName);
              if (it == targetArch.m_registers.end()) {
                logError("Register \"" + regName + "\" is not declared, but is referenced at " + rangeToken.positionToString());
                continue;
              }
              argument->getRange()->addToRegisterRange(&it->second);
            }
          } else {
            const auto it = targetArch.m_registers.find(currentRange);
              if (it == targetArch.m_registers.end()) {
                logError("Register \"" + currentRange + "\" is not declared, but is referenced at " + rangeToken.positionToString());
                continue;
              }
              argument->getRange()->addToRegisterRange(&it->second);
          }
        }
        break;
      }
      // immediate range
      case Arch::Instruction::Argument::Type::IMMEDIATE: {
        size_t tokenPos = 0;
        int leftValue = 0;
        int rightValue = 0;
        bool secondHalf = false;
        std::string left;
        std::string current;
        while (tokenPos < rangeToken.m_value.length()) {
          const char c = rangeToken.m_value[tokenPos];
          tokenPos++;
          if (c == ':') {
            left = current;
            secondHalf = true;
            continue;
          }
          current.push_back(c);
        }

        if (!(safe_stoi(left,&leftValue) && safe_stoi(current,&rightValue))) {
          logError("Could not parse string to integers at " + rangeToken.positionToString());
          break;
        }

        if (leftValue > rightValue) {
          argument->setRange(std::make_unique<Arch::Instruction::ImmediateRange>(rightValue,leftValue));
        } else {
          argument->setRange(std::make_unique<Arch::Instruction::ImmediateRange>(leftValue,rightValue));
        }
        break;
      }
      default: 
      logError("Expected range type, got " + std::string(argument->typeToString()) + " at " + rangeToken.positionToString());
      break;
    }

    return argument;
  };

  while (notAtEnd()) {
    const auto& token = peek();
    std::cout << token.m_value << std::endl;
    if (token.m_type == Lexer::Token::Type::KEYWORD) {

      if (token.m_value == "format") {
        consume();
        Arch::Format format;
        tokenisingError (Lexer::Token::Type::IDENTIFIER,"Invalid format at " + token.positionToString())
        const auto& formatToken = consume();
        format.m_alias = formatToken.m_value;
        
        int bitwidth = 0;
        while (notAtEnd() && peek().m_type == Lexer::Token::Type::IDENTIFIER) {
          const int operandBitwidth = std::stoi(consume().m_value);
          bitwidth += operandBitwidth;
          format.m_operandBitwidth.push_back(operandBitwidth);
        }
        if (bitwidth != targetArch.m_bitwidth) {logError("Invalid bitwidth of format \"" + format.m_alias + "\" at " + formatToken.positionToString());}
        targetArch.m_formats.emplace(format.m_alias, std::move(format));

      } else if (token.m_value == "bitwidth") {
        consume();
        if (notAtEnd() && peek().m_type == Lexer::Token::Type::IDENTIFIER) {
          targetArch.m_bitwidth = std::stoi(consume().m_value);
        }
      } else if (token.m_value == "layout") {
        consume();
        assert(false);
        // not implemented
      } else if (token.m_value == "datatype") {
        consume();
        tokenisingError (Lexer::Token::Type::IDENTIFIER,"Invalid data typename at " + token.positionToString());
        const std::string dataTypeName = peek().m_value;
        consume();
        tokenisingError (Lexer::Token::Type::IDENTIFIER,"Invalid data type byte length at " + token.positionToString());
        //if ()
        assert(false);
        // not implemented

        //add to keyword set
      } else if (token.m_value == "reg") {
        consume();
        tokenisingError (Lexer::Token::Type::IDENTIFIER,"Invalid register identifier at " + token.positionToString());
        const auto& rangeToken = consume();
        std::string lowerStr;
        std::string lowerNumber;
        std::string higherNumber;
        std::string prefix;
        std::string postfix;
        size_t dashPos = 0;
        bool isRange = false;
        while (dashPos < rangeToken.m_value.length()) {
          const char c = rangeToken.m_value[dashPos];
          if (c == '-') {isRange = true;}
          else if (!isdigit(c) && !isRange) {prefix.push_back(c);}
          else if (!isdigit(c) && isRange) {postfix.push_back(c);}
          else if (isdigit(c) && !isRange) {lowerNumber.push_back(c);}
          else if (isdigit(c) && isRange) {higherNumber.push_back(c);}
          lowerStr.push_back(c);
          dashPos++;
        }
        
        tokenisingError (Lexer::Token::Type::IDENTIFIER,"Invalid register bitwidth at " + peek().positionToString());
        const auto& bitWidthToken = consume();
        tokenisingError (Lexer::Token::Type::IDENTIFIER,"Invalid register operand value at " + peek().positionToString());
        const auto& operandToken = consume();
        
        int bitWidth = -1;
        safe_stoi(bitWidthToken.m_value, &bitWidth);
        int operandValue = -1;
        safe_stoi(operandToken.m_value, &operandValue);
        
        if (isRange) {
          if (prefix != postfix) {logError("mismatched range prefix \"" + prefix + "\" and \"" + postfix + "\" @ " + rangeToken.positionToString()); return;}
          
          const int minReg = std::stoi(lowerNumber);
          const int maxReg = std::stoi(higherNumber);
          if (minReg > maxReg) {logError("invalid range of registers (err is: min > max) @ " + rangeToken.positionToString()); return;}
          
          for (int i = minReg; i <= maxReg; i++) {
            targetArch.m_registers.emplace(prefix + std::to_string(i), Arch::RegisterIdentity(prefix + std::to_string(i), bitWidth, operandValue)) ;
            operandValue++;
          }
        } else {
          targetArch.m_registers.emplace(rangeToken.m_value, Arch::RegisterIdentity(rangeToken.m_value, bitWidth, operandValue)) ;
        }
      } else if (token.m_value == "ctr") {
        skip();
        tokenisingError(Lexer::Token::Type::IDENTIFIER, "Expected identifier, got \"" + peek().m_value + "\" at " + peek().positionToString());
        const auto& identifierToken = consume();
        tokenisingError(Lexer::Token::Type::IDENTIFIER, "Expected identifier, got \"" + peek().m_value + "\" at " + peek().positionToString());
        const auto& valueToken = consume();
        int bitassert = 0;
        safe_stoi(valueToken.m_value, &bitassert);
        targetArch.m_controlSignals.emplace(identifierToken.m_value, Arch::ControlSignal(identifierToken.m_value,(size_t)bitassert));
      } else {
        std::cout << "ASSERTED_A" << std::endl;
        std::cout << token.m_value << std::endl;
        assert(false);
        //bad!
      }
    } else {
      //new instruction hopefully
      Arch::Instruction::Instruction instruction;
      instruction.m_opcode = targetArch.m_instructionSet.size();
      const auto& nameToken = consume();
      if (nameToken.m_type == Lexer::Token::Type::NEWLINE) {
        // after reading this code - wtf is this for>?????
        // targetArch.m_nameSet.emplace(instruction.m_name);
        // targetArch.m_instructionSet.emplace(instruction.m_name, std::move(instruction));
        // targetArch.m_keywordSet.emplace(instruction.m_name);
        continue;
      }
      instruction.m_name = nameToken.m_value;
      if (!notAtEnd()) {logError("incomplete instruction @ " + token.positionToString()); continue;}

      const auto& formatToken = consume();
      if (formatToken.m_type == Lexer::Token::Type::NEWLINE) {
        targetArch.m_nameSet.emplace(instruction.m_name);
        targetArch.m_instructionSet.emplace(instruction.m_name, std::move(instruction));
        targetArch.m_keywordSet.emplace(instruction.m_name);
        continue;
      }
      instruction.m_formatAlias = formatToken.m_value;
      //implement bytelegtnh and opcode setting for instr objects
      const auto& byteLengthToken = consume();
      if (byteLengthToken.m_type == Lexer::Token::Type::NEWLINE) {
        targetArch.m_nameSet.emplace(instruction.m_name);
        targetArch.m_instructionSet.emplace(instruction.m_name, std::move(instruction));
        targetArch.m_keywordSet.emplace(instruction.m_name);
        continue;
      }
      safe_stoi(byteLengthToken.m_value, &instruction.m_byteLength);

      const auto& opcodeToken = consume();
      if (opcodeToken.m_type == Lexer::Token::Type::NEWLINE) {
        targetArch.m_nameSet.emplace(instruction.m_name);
        targetArch.m_instructionSet.emplace(instruction.m_name, std::move(instruction));
        targetArch.m_keywordSet.emplace(instruction.m_name);
        continue;
      }
      safe_stoi(opcodeToken.m_value, &instruction.m_opcode);
      
      while (peek().m_type == Lexer::Token::Type::ARGUMENTTYPE) {
        //if (peek().m_value == "NON") {consume(); break;}
        instruction.m_arguments.push_back(std::move(parseArgument()));
      }

      targetArch.m_nameSet.emplace(instruction.m_name);
      targetArch.m_instructionSet.emplace(instruction.m_name, std::move(instruction));
      targetArch.m_keywordSet.emplace(instruction.m_name);
    }
  }
}

std::string Arch::Architecture::toString() const {
  std::ostringstream str;

  //instruction set
  for (const auto& it : m_instructionSet) {
    str << it.second.toString(2,2);
  }
  //register
  for (const auto& it : m_registers) {
    str << it.second.toString(2,2);
  }
  //datatypes
  for (const auto& it : m_dataTypes) {
    str << it.second.toString(2,2);
  }
  //formats
  for (const auto& it : m_formats) {
    str << it.second.toString(2,2);
  }

  for (const auto& it : m_controlSignals) {
    str << it.second.toString(2,2);
  }

  return str.str();
}

std::string Arch::RegisterIdentity::toString(size_t padding, size_t ident) const {
  std::ostringstream str;
  const std::string indent(ident + padding, ' ');
  const std::string indent2(ident + 2 * padding, ' ');
  str << indent << "Register Name \"" << std::setw(5) << std::left << m_name 
  << "\" Bitwidth: " << m_bitWidth 
  << ", Machine Operand Value: " << m_machineCodeOperandValue 
  << '\n';
  return str.str();
}

std::string Arch::ControlSignal::toString(size_t padding, size_t ident) const {
  std::stringstream str;
  str << "Control Signal [" << std::right << std::setw(3) << m_assertbit << "]: " << m_name << '\n';
  return str.str();
}
