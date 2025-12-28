#string variable values convert each char to ascii byte value and then are a list of bytes

from __future__ import annotations
import regex as re
import sys
from enum import Enum
from typing import List, Tuple, Optional, Union
import os

class Compiler():
  def __init__(self):
    self.warnings: List[str] = []
    self.errors: List[tuple[str, bool]] = []
    self.debugs: list[str] = []
    self.workingDirectory: str = ""

  def clearErrors(self):
    self.errors.clear()

  def clearDebugs(self):
    self.debugs.clear()

  def clearWarnings(self):
    self.warnings.clear()
  
  def consumeWarning(self):
    if len(self.warnings) > 0:
      return self.warnings.pop(0)
    return None
  
  def consumeError(self):
    if len(self.errors) > 0:
      return self.errors.pop(0)
    return None
  
  def consumeDebug(self):
    if len(self.debugs) > 0:
      return self.debugs.pop(0)
    return None
  
  def logWarning(self, message: str):
    self.warnings.append(message)

  def logError(self, message: str, kill: bool = False):
    self.errors.append((message, kill))

  def logDebug(self, message: str):
    self.debugs.append(message)

  def setWorkingDirectory(self, path: str):
    self.workingDirectory = path

  def compile(self, source: str, dumpIncludedPaths: bool = False, dumpRawFile: bool = False) -> bool:
    preproc = preprocessor(self)
    compilation_success = True
    parsed_lines, success = preproc.parseSource(source)
    if dumpIncludedPaths:
      print("INCLUDED FILE PATHS")
      preproc.dump_included_filepaths("    ")
      print()
    if dumpRawFile:
      print("RAW PROCESSED FILE")
      width = len(str(len(parsed_lines)))
      for index,line in enumerate(parsed_lines):
        print(f"{str(index).rjust(width)}    " + line.line)
      print()
    if not success:
      return False
    asm = assembler(self)
    asm.assemble(parsed_lines)


    #further compilation steps would go here
    if compilation_success:
      print("Compilation succeeded.")
    return True
  
  class Line:
    def __init__(self, line: str, number: int, filepath: str) -> None:
      self.line = line
      self.number = number
      self.filepath = filepath
class preprocessor:
  def __init__(self, compiler: Compiler):
    self.compiler = compiler
    self.included_files: set[str] = set()
    self.entry_label: str = ""

  def parseSource(self, source: str) -> Tuple[List[Compiler.Line], bool]:
    source_lines, success = self._readSource(source)
    if not success:
      return [], False
    self._include_file(source)
    parsed_lines, success = self._parseSource(source_lines, source)
    if not success:
      return [], False
    return parsed_lines, True
  
  def dump_included_filepaths(self, prefix: str):
    for filepath in self.included_files:
      print(prefix + filepath)

  def _include_file(self, filepath: str):
    full_path = os.path.join(self.compiler.workingDirectory, filepath)
    self.included_files.add(full_path)

  def _readSource(self, source: str) -> Tuple[List[Compiler.Line], bool]:
    print("READING FILE:"+source)
    source_lines: List[Compiler.Line] = []
    filepath = os.path.join(self.compiler.workingDirectory, source)
    try:

      with open(filepath, 'r') as file:
        source_file = file.read()
        #strip comments
        mlm = re.compile( #multi line match ;3
            r'("(\\"|[^"])*")'  # string literal
            r'|(;\*(.|\n)*?\*;)' # multi-line comment
        )
        single_line_match = re.compile(
            r'("(\\"|[^"])*")'  # string literal
            r'|(;[^*][^\n]*$)',      # single-line comment
            re.MULTILINE
        )
        
        def replacer(match: re.Match) -> str:
          if match.group(1):  # string literal
            return match.group(1)
          else:
              comment_text = match.group(0)
              return re.sub(r'[^\n]*', ' ', comment_text)
          
        def replacer(match: re.Match) -> str:
            comment_text = match.group(0)
            return re.sub(r'[^\n]*', ' ', comment_text)
          
        rematch = mlm.sub(replacer,source_file)
        rematch = single_line_match.sub(replacer,rematch)

        stringless = mlm.sub("", source_file)
        stringless = single_line_match.sub("", stringless)
        for index, line in enumerate(rematch.splitlines()):
          source_lines.append(Compiler.Line(line,index+1,filepath))

    except Exception as e:
      self.compiler.logError(f"Failed to read source file: \"{source}\", with error: {e}", True)
      return [], False
    return source_lines, True

  def _parseSource(self, source_lines: List[Compiler.Line], source: str, required_tree: Optional[List[str]] = None) -> Tuple[List[Compiler.Line], bool]:
    temp_source_lines = source_lines.copy()
    success = True
    for index, line in enumerate(temp_source_lines.copy()):
      # 1 = preprocessor, 
      # 2 = label, 
      special_match = re.match(r"(^[@])|(^[.])", line.line)
      if special_match is None:
        #normal code line, skip for now
        continue

      #preprocessor directives 
      elif special_match.group(1):
        rematch = re.match(r"^@(\w+)", line.line)

        match rematch:
          case None:
            self.compiler.logError(f"Invalid preprocessor directive on line {line.number} of file \"{line.filepath}\": \"{line.line.strip()}\"")
            success = False
            temp_source_lines.remove(line)
            continue

          case m:
            command = m.group(1).lower()

            match command:
              case "define":
                entry_match = re.match(r"(^@entry\s+)(\.?[A-Za-z_]\w+)", line.line)
              case "entry":
                entry_match = re.match(r"(^@entry\s+)(\.?[A-Za-z_]\w+)", line.line)
                if entry_match and entry_match.group(2):
                  self.entry_label = entry_match.group(2)

              case "include":
                include_match = re.match(r"^@include\s+\"(.+)\"\s*$", line.line) # with syntax @include "filename"
                if include_match:
                  include_filename = include_match.group(1)
                  #process required tree
                  if required_tree is None:
                    required_tree = []
                  elif include_filename in required_tree:
                    #circular dependency detected
                    self.compiler.logError(f"Circular dependency detected when including file \"{include_filename}\" on line {line.number} of file \"{line.filepath}\"")
                    success = False
                    continue
                  
                  #process include
                  new_required_tree = required_tree.copy()
                  new_required_tree.append(source)

                  #add extension if missing
                  extension = os.path.splitext(include_filename)[1]
                  if extension == "":
                    include_filename += ".spasm"

                  included_lines, included_success = self._readSource(include_filename)
                  if not included_success:
                    self.compiler.logError(f"Failed to include file \"{include_filename}\"\n    on line {line.number} of file \"{line.filepath}\"\n    from include tree: {format(' -> '.join(new_required_tree))}") # type: ignore
                    success = False
                    continue
                  else:
                    self._include_file(include_filename)
                    temp_source_lines.remove(line)
                    newlines, parsing_success = self._parseSource(included_lines, include_filename, new_required_tree)
                    if not parsing_success:
                      self.compiler.logError(f"Failed to parse included file \"{include_filename}\"\n    on line {line.number} of file \"{line.filepath}\"\n    from include tree: {format(' -> '.join(new_required_tree))}") # type: ignore
                      success = False
                    for included_line in reversed(newlines): #reverse to maintain order when inserting
                      temp_source_lines.insert(line.number, included_line)
                else:
                  self.compiler.logError(f"Invalid include syntax on line {line.number} of file \"{line.filepath}\": \"{line.line.strip()}\"")
                  success = False
                  continue
      
      #labels
      elif special_match.group(2):
        label_match = re.match(r"^\.([A-Za-z_]\w*):?$", line.line)
        if label_match:
          #valid label, keep it for now
          continue
        else:
          self.compiler.logError(f"Invalid label syntax on line {line.number} of file \"{line.filepath}\": \"{line.line.strip()}\"")
          success = False
          temp_source_lines.remove(line)
          continue





    
    return temp_source_lines, success

  class StringRepresentation:
    def __init__(self,string:str,index:int, end:int):
      self.string = string
      self.index = index
      self.end = end

REGISTERS = {
    "R0": 0,
    "R1": 1,
    "R2": 2,
    "R3": 3,
    "R4": 4,
    "R5": 5,
    "R6": 6,
    "R7": 7,
    "R8": 8,
    "R9": 9,
    "R10": 10,
    "R11": 11,
    "R12": 12,
    "R13": 13,
    "R14": 14,
    "R15": 15,
    "W1": 21,
    "W2": 22,
    "W3": 23,
    "W4": 24,
    "IC": 16,
    "SP": 17,
    "FP": 18,
    "IR": 19,
    "FLAGS": 20
  }
class assembler:
  def __init__(self, compiler: Compiler, entry_label: str = "__main"):
    self.compiler = compiler 
    #labels by mem addr
    self.parsedLines: List[Compiler.Line] = []
    self.currentLabel: Optional[assembler.Label] = None
    self.labels: List[assembler.Label] = []
    self.entry_label = entry_label
  class ArgumentType(Enum):
    REGISTER = 1
    IMMEDIATE = 2
    LABEL = 3

  class Label:
    def __init__(self, name: str, address: int, parent_label: Optional[assembler.Label]):
      self.name = name
      self.address = address
      self.resolved = False
      self.parent_label = parent_label
      self.local_namespace: set[str] = set()
      self.local_variables: dict[str, assembler.Variable] = {}
      self.scope: List[Union[assembler.Instruction, assembler.Variable]] = []
      #probably need like a couple passes to refer locally and then globally or something idk

    def checkNameAvailability(self, name: str):
      if name in self.local_namespace:
        return False, self.local_variables[name]
      if self.parent_label is not None:
        return self.parent_label.checkNameAvailability(name)
      return True, None

  class Variable:
    def __init__(self, name: str, address: int, size: int, definition_line: Compiler.Line, value: Optional[Union[int, str]], parent_label: Optional[assembler.Label]):
      self.name = name
      self.address = address
      self.size = size
      self.value = value
      self.type: assembler.VariableType = assembler.VariableType.UNASSIGNED
      self.parent_label = parent_label
      self.definition_line = definition_line

  class VariableType(Enum):
    CHAR = 1
    WORD = 2
    DWORD = 4
    QWORD = 8
    TEXT = 0
    UNASSIGNED = -1
    
  class InstructionType:
    def __init__(self, opcode: str, bitcode: int, flagbits: int, operands: List[Union[assembler.ArgumentType, Tuple[assembler.ArgumentType, assembler.ArgumentType]]], operand_ranges: List[Union[assembler.ArgumentRange, Tuple[assembler.ArgumentRange,assembler.ArgumentRange]]]):
      self.opcode = opcode
      self.bitcode = bitcode
      self.flagbits = flagbits
      self.operands = operands
      self.operand_ranges = operand_ranges
    
    def getArgCount(self) -> int:
      return len(self.operands)
    
  class Instruction:
    def __init__(self, instruction_type: assembler.InstructionType, arguments: List[Union[int, str]]):
      self.instruction_type = instruction_type
      self.arguments = arguments
    
    def pushArgument(self, argument: Union[int, str]):
      self.arguments.append(argument)

    def buildMachineCode(self) -> int:
      machine_code = 0xffff #default to nop
      return machine_code
    
    def prettyHumanPrint(self) -> str:
      arg_strings = []
      for arg in self.arguments:
        arg_strings.append(str(arg))
      return f"{self.instruction_type.opcode} " + ", ".join(arg_strings)
  
  class Argument:
    def __init__(self, argument_type: assembler.ArgumentType, value: Union[int, str]):
      self.argument_type = argument_type
      self.value = value

  class ArgumentRange:
    def __init__(self, argument_types: assembler.ArgumentType):
      self.argument_types = argument_types
      self.immediate_min = 0
      self.immediate_max = 0
      self.valid_registers: List[str] = []
    
    def addImmediateRange(self, min_value: int, max_value: int):
      self.immediate_min = min_value
      self.immediate_max = max_value
      return self
    
    def addValidRegister(self, register: str):
      if register in REGISTERS:
        self.valid_registers.append(register)
        return self
      assert register in REGISTERS, f"Register \"{register}\" is not a valid register."
      
    def addValidRegisters(self, registers: Union[List[str], Tuple[str, ...], str]):
      if isinstance(registers, str):
        pattern = re.compile(r'\w+\-\w+|\w+')
        matches = pattern.findall(registers)
        for match in matches:
          if '-' in match:
            start_reg, end_reg = match.split('-')
            start_index = REGISTERS.get(start_reg.upper(), None)
            end_index = REGISTERS.get(end_reg.upper(), None)
            if start_index is not None and end_index is not None and start_index <= end_index:
              for reg, idx in REGISTERS.items():
                if start_index <= idx <= end_index:
                  self.addValidRegister(reg)
          else:
            self.addValidRegister(match)
      else:
        for register in registers:
          self.addValidRegister(register)
      return self
    
    def isValid(self, argument) -> bool:
      if self.argument_types == assembler.ArgumentType.IMMEDIATE:
        try:
          return self.immediate_min <= int(argument) <= self.immediate_max
        except ValueError:
          return False
      elif self.argument_types == assembler.ArgumentType.REGISTER:
        return argument in self.valid_registers
      return False #was not correct
    
  INSTRUCTIONS = {
    "HALT":  InstructionType("HALT",  0b00000, 0b0000000000000000, [], []),
    "IADD":  InstructionType("IADD",  0b00001, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "ISUB":  InstructionType("ISUB",  0b00010, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "IMUL":  InstructionType("IMUL",  0b00011, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "IDIV":  InstructionType("IDIV",  0b00100, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "IMOD":  InstructionType("IMOD",  0b00100, 0b0000000000000001, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "SIADD": InstructionType("SIADD", 0b00001, 0b0000000000000010, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "SISUB": InstructionType("SISUB", 0b00010, 0b0000000000000010, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "SIMUL": InstructionType("SIMUL", 0b00011, 0b0000000000000010, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "SIDIV": InstructionType("SIDIV", 0b00100, 0b0000000000000010, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "SIMOD": InstructionType("SIMOD", 0b00100, 0b0000000000000011, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "NOT":   InstructionType("NOT",   0b00101, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER],                                                  [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "AND":   InstructionType("AND",   0b00110, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "OR":    InstructionType("OR",    0b00111, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "FADD":  InstructionType("FADD",  0b01000, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "FSUB":  InstructionType("FSUB",  0b01001, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "FMUL":  InstructionType("FMUL",  0b01010, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "FDIV":  InstructionType("FDIV",  0b01011, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, ArgumentType.REGISTER],                           [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7")]),
    "SHL":   InstructionType("SHL",   0b01100, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, (ArgumentType.REGISTER, ArgumentType.IMMEDIATE)], [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"),(ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7").addImmediateRange(0,15))]),
    "SHR":   InstructionType("SHR",   0b01101, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.REGISTER, (ArgumentType.REGISTER, ArgumentType.IMMEDIATE)], [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"),(ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7"),ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r0-r7").addImmediateRange(0,15))]),
    "ADI":   InstructionType("ADI",   0b01110, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.IMMEDIATE],                                                 [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.IMMEDIATE).addImmediateRange(-127,255)]),
    "LDIL":  InstructionType("LDIL",  0b01111, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.IMMEDIATE],                                                 [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.IMMEDIATE).addImmediateRange(-127,255)]),
    "LDIH":  InstructionType("LDIH",  0b10000, 0b0000000000000000, [ArgumentType.REGISTER, ArgumentType.IMMEDIATE],                                                 [ArgumentRange(ArgumentType.REGISTER).addValidRegisters("r1-r7"), ArgumentRange(ArgumentType.IMMEDIATE).addImmediateRange(-127,255)]),
  }

  VARIABLETYPES = {
    "CHAR" : VariableType.CHAR,
    "WORD" : VariableType.WORD,
    "DWORD": VariableType.DWORD,
    "QWORD": VariableType.QWORD,
    "TEXT" : VariableType.TEXT
  }

  def assemble(self, parsed_lines: List[Compiler.Line]) -> Tuple[List[int], bool]:
    self.parsedLines = parsed_lines
    machine_code: List[int] = []
    success = True

    for index, line in enumerate(parsed_lines):
      line_result = self._parseLine(line, index + 1)
      if line_result is None:
        continue
      else:
        print(line_result.prettyHumanPrint())

    return machine_code, success
  def _tokenizeLine(self, line: str, max_tokens: int = -1) -> List[str]:   
    return line.split(maxsplit=max_tokens)
  
  def _eatImmediate(self, string:str, line: int = -1):
    immediate = 0
    type_rematch = re.match(r"(0x)|(0b)", string)
    refined = re.sub("_","",string)
    try:
      if type_rematch:
        if type_rematch.group(1):
          immediate = int(refined,16)
        elif type_rematch.group(2):
          immediate = int(refined,2)
        return immediate, True
      return int(immediate), True
    except ValueError:
      self.compiler.logError(f"Invalid immediate on line {line}")
      return 0, False
        
  def _eatString(self, line: int, index: int):
    string: str = ""
    stringing = True
    
    rematch = re.search(r"(\"[^\"\n]*\")|(\".*$)", self.parsedLines[line].line, pos=index)
    if rematch:
      if rematch.group(1):
        stringing = False
        string = rematch.group(1)
        self.parsedLines[line].line = self.parsedLines[line].line[:index]
        return string, True
      elif rematch.group(2):
        string += rematch.group(2)
        self.parsedLines[line].line = self.parsedLines[line].line[:index]
    else:
      self.compiler.logError(f"Attempted to consume string without string on initial line, on line {self.parsedLines[line].number} in file {self.parsedLines[line].filepath}")
    line += 1

    while stringing:
      if len(self.parsedLines) <= line:
        return "", False
      rematch = re.match(r"([^\"]*\")|([^\"]*)", self.parsedLines[line].line)
      if rematch:
        if rematch.group(1):
          stringing = False
          string += rematch.group(1)
          self.parsedLines[line].line = self.parsedLines[line].line[rematch.end():]
        if rematch.group(2):
          self.parsedLines[line].line = ""
          string += rematch.group(2)
      line += 1
    self.compiler.logDebug(f"Eaten multiline string: {string}")
    return string, True
  
  def _parseLine(self, line: Compiler.Line, line_number: int, source: str = "[[UNKNOWN FILE]]") -> Optional[Instruction]:
    label_match = re.match(r"^(\.)?([A-Za-z_]\w*)(:)?$", line.line)
    if not label_match:
      #normal code 
      instruction_match = re.match(r"(CHAR|WORD|DWORD|QWORD|TEXT)|(^\s*([A-Za-z]+))|(\s+)", line.line)

      if instruction_match:
        if instruction_match.group(3):
          #instruction type
          instruction_type = assembler.INSTRUCTIONS.get(instruction_match.group(2).upper(), None)
          if instruction_type is not None:
  
            tokens = self._tokenizeLine(line.line)
            #ensure matching number of args
            if len(tokens) - 1 != instruction_type.getArgCount():
              self.compiler.logError(f"Incorrect number of arguments for instruction \"{instruction_type.opcode}\" on line {line.number} of file \"{line.filepath}\": expected {instruction_type.getArgCount()}, got {len(tokens) - 1}")
              return None
            
            instruction: assembler.Instruction = assembler.Instruction(instruction_type, [])
            
            #parse arguments
            for arg_index in range(instruction_type.getArgCount()):
              #get argument token
              argument_token = tokens[arg_index + 1]
              
              #validate argument against range
              operand_range = instruction_type.operand_ranges[arg_index]
              range_valid = False
              if isinstance(operand_range, assembler.ArgumentRange):
                range_valid = operand_range.isValid(argument_token)
              elif isinstance(operand_range, tuple):
                for subrange in operand_range:
                  range_valid = range_valid or subrange.isValid(argument_token)

              if not range_valid:
                self.compiler.logError(f"Invalid argument [{arg_index+1}]: \"{argument_token}\" for instruction \"{instruction_type.opcode}\" on line {line.number} of file \"{line.filepath}\"")
                return None
              
              #process argument
              match instruction_type.operands[arg_index]:
                case assembler.ArgumentType.REGISTER:
                  if argument_token in REGISTERS:
                    instruction.pushArgument(argument_token)


                case assembler.ArgumentType.IMMEDIATE:            
                  try:
                    immediate_value = int(argument_token) #TODO use eatimmediate function
                    instruction.pushArgument(immediate_value)
                  except ValueError:
                    self.compiler.logError(f"Invalid immediate value \"{argument_token}\" for instruction \"{instruction_type.opcode}\" on line {line.number} of file \"{line.filepath}\"")
                    return None
            
            return instruction
          else:
            self.compiler.logError(f"Unknown instruction on line {line.number} of file \"{line.filepath}\": \"{line.line.strip()}\"")
            return None
        elif instruction_match.group(1):
          #variable type
          #var type - size/none - name - value
          if not self.currentLabel:
            assert False, "BAD BAD BADB AD"
          variable = assembler.Variable("",-1,-1,line, -1, self.currentLabel)
          auto_sized = False
          
          #tokenise
          tokens = self._tokenizeLine(line.line, 3)
          value_token: str = ""
          variableType = assembler.VARIABLETYPES.get(tokens[0],None)

          #ensure that the variable type exists
          if not variableType:
            self.compiler.logError(f"Unkown variable type at line {line.number} in file {line.filepath}")
            return None
          variable.type = variableType

          #defined sized variable initiation
          if variableType.value > 0:
            variable.size = variableType.value
            #ensure valid #args
            if len(tokens) != 3:
              self.compiler.logError(f"Invalid number of arguments for variable declaration on line {line.number}, expected 3, got {len(tokens)}")
              return None
            name_is_available, occupying_variable = self.currentLabel.checkNameAvailability(tokens[1])
            if name_is_available:
              variable.name = tokens[1]
            else:
              self.compiler.logError(f"Variable name \"{tokens[1]}\" is already defined on line {occupying_variable.definition_line.number} in file {occupying_variable.definition_line.filepath}, error from line {line.number} in file {line.filepath}") # type: ignore
            #set value token
            value_token = tokens[2]
          else:
          #non defined size variable initiation
            tokens = self._tokenizeLine(line.line, 4)
            
            #ensure valid #args
            if len(tokens) != 4:
              self.compiler.logError(f"Invalid number of arguments for variable declaration on line {line.number} of file {line.filepath}, expected 4, got {len(tokens)}")
              return None
            
            #validate name in namespace
            name_is_available, occupying_variable = self.currentLabel.checkNameAvailability(tokens[2])
            if name_is_available:
              variable.name = tokens[2]
            else:
              self.compiler.logError(f"Variable name \"{tokens[2]}\" is already defined on line {occupying_variable.definition_line.number} in file {occupying_variable.definition_line.filepath}, error from line {line.number} in file {line.filepath}") # type: ignore
            
            #set value token and sort byte length
            value_token = tokens[3]
            byte_length_match = re.match(r"(0)|([0-9]+)|(auto)", tokens[1])
            if not byte_length_match:
              self.compiler.logError(f"Invalid byte length for variable type {variableType.name} on line {line.number} of file {line.filepath}")
              return None
            if byte_length_match.group(1) == "auto":
              auto_sized = True
            else:
              variable.size = int(byte_length_match.group(1))
          
          #solve value
          value: Optional[Union[str, int]] = None
          if value_token[0] == '"':
            #sort string value
            value, success = self._eatString(line_number-1,0)
            
            self.compiler.logWarning(f"eating string from index 0, could be dangerous, on line:{sys._getframe().f_lineno}")
            if not success or not value:
              self.compiler.logError(f"String value failed to parse on line {line.number} of file {line.filepath}")
              return None
            if auto_sized:
              #sort autosized
              variable.size = len(value)
          else:
            #sort immediate value
            value, success = self._eatImmediate(value_token)
            if not success:
              self.compiler.logError(f"Immediate value failed to parse on line {line.number} of file {line.filepath}")
              return None
            if auto_sized:
              #sort autosized
              if value == 0:
                variable.size = 1
              bits = value.bit_length()
              if value < 0:
                  bits += 1
              variable.size = (bits + 7)//8

          #assign elements of variable
          variable.value = value

          variable.parent_label = self.currentLabel
          self.currentLabel.scope.append(variable)
          self.currentLabel.local_namespace.add(variable.name)
          self.currentLabel.local_variables[variable.name] = variable

            
        elif not instruction_match.group(4):
          self.compiler.logError(f"Invalid instruction syntax on line {line.number} of file \"{line.filepath}\": \"{line.line.strip()}\"")
          return None
          
    elif label_match.group(3) and label_match.group(2):
      #label definition

      label = assembler.Label(label_match.group(2),-1,None) #for now, parent gets assigned upon first reference
      self.currentLabel = label
      return None
    else:
      self.compiler.logError(f"Unkown element in: {line.line} @ {line.number} in {line.filepath}")
      #assert False, f"Something went wrong at line {line.number} in {line.filepath}"
    return None
  

        
compiler = Compiler()
compiler.setWorkingDirectory("D:\\logisim prohects\\16bit cpu")
compiler.compile("test.spasm", True, True)

warning = compiler.consumeWarning()
error = compiler.consumeError()
debug = compiler.consumeDebug()
while warning is not None:
  print(f"[WARNING]: {warning}")
  warning = compiler.consumeWarning()

while error is not None:
  print(f"[ERROR]: {error[0]}")
  error = compiler.consumeError()

while debug is not None:
  print(f"[DEBUG]: {debug}")
  debug = compiler.consumeDebug()

          


