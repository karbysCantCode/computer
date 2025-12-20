
from __future__ import annotations
import regex as re
import sys
from enum import Enum
from typing import List, Tuple, Optional, Union, TypeAlias
import os
import csv
import argparse

class Compiler:
  def __init__(self):
    self.warnings: List[str] = []
    self.errors: List[tuple[str, bool]] = []
    self.debugs: list[str] = []
    self.workingDirectory: str = ""
    self.instructionSetDirectory: str = ""
    self.instructionSetParser = Compiler.InstructionSetParser(self)

  def clearErrors(self):
    self.errors.clear()

  def clearDebugs(self):
    self.debugs.clear()

  def clearWarnings(self):
    self.warnings.clear()

  def areErrors(self):
    return len(self.errors) > 0
  
  def areWarnings(self):
    return len(self.warnings) > 0
  
  def areDebugs(self):
    return len(self.debugs) > 0
  
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

  def compile(self, target:MakeParser.Target, isa_file_path:Optional[str]):
    if isa_file_path:
      self.instructionSetParser.setSource(isa_file_path)
    self.instructionSetParser.readISA()

    tksr = tokeniser(self)
    preproc = preprocessor(self, tksr)
    target_tokens: List[Compiler.Token] = []
    for filepath in target.build_files:
      if filepath is None: self.logError("Target file was not found"); return False
      first_file_tokens, preprocessor_token_indexes = tksr.tokenise(filepath)
      if first_file_tokens:
        for token in first_file_tokens:
          print(f"[R]{token.value}")
      if preprocessor_token_indexes:
        for token in preprocessor_token_indexes:
          print(f"[{token}]: {first_file_tokens[token].value}")
      processed_tokens = preproc.process(first_file_tokens,preprocessor_token_indexes, target)
      for token in processed_tokens:
        print(f"[P]{token.value}[P]")
      target_tokens.extend(processed_tokens)
    

  class Token:
    def __init__(self, value:str, line:int, filepath:str):
      self.value = value
      self.line = line
      self.filepath = filepath
      self.dead = False
    
    def getFileLocation(self) -> str:
      return f"\"{self.filepath}\" @ line {self.line}"

  class MakefileParser:
    def __init__(self, makefile_path:str):
      self.makefile_path = makefile_path
      

  class InstructionSetParser:
    def __init__(self, compiler: Compiler):
      self.compiler = compiler
      self.known_registers: set[str] = set()
      self.instructions: dict[str, assembler.InstructionType] = {}
      self.source_file_path: Optional[str] = None
    
    def setSource(self, source_file:str):
      self.source_file_path = source_file

    def _consumeArgument(self,argument_index:int, row:dict[str,str]):
      #work on thsi function it has just een started
      multipattern = r"(\w+:)"
      index = 0
      arg_type: Optional[assembler.ArgumentType] = None
      arg_type_set:set[assembler.ArgumentType] = set()
      arg_string = row[f"ARGUMENT_{argument_index}_TYPE"]
      while True:
        match = re.search(multipattern,arg_string, pos=index)
        if match:
          index = match.end(1)
          try:
            arg_type_set.add(assembler.ArgumentType[match.group(1)])
          except KeyError:
            self.compiler.logError(f"Invalid argument type in argument {argument_index} of row [dump]: {row}")
        else:
          try:
            arg_type = assembler.ArgumentType[arg_string[index:]]
          except KeyError:
            self.compiler.logError(f"Invalid argument type in argument {argument_index} of row [dump]: {row}")
          break
      
      argument_range = assembler.ArgumentRange(assembler.ArgumentType.REGANDIMM)
      immediate: str = ""
      range_string = row[f"ARGUMENT_{argument_index}_RANGE"]
      if (arg_type is not None and arg_type == assembler.ArgumentType.IMMEDIATE) or (assembler.ArgumentType.IMMEDIATE in arg_type_set):
        #immediate 
        immediate_pattern = r"-?[0-9]+:-?[0-9]+"
        def repl(match:re.Match):
          nonlocal immediate
          immediate = match.group(0)  # save what got substituted
          return ""
        range_string = re.sub(immediate_pattern,repl,range_string)
        number_pattern = r"-?[0-9]+"
        minmatch = re.search(number_pattern, immediate)
        min = 0
        max = 0
        if minmatch:
          min = minmatch.group(0)
        else:
          self.compiler.logError(f"Invalid min range type in argument {argument_index} of row [dump]: {row}")
          return
        
        maxmatch = re.search(number_pattern,immediate, pos=minmatch.end(0))
        if maxmatch:
          max = maxmatch.group(0)
        else:
          self.compiler.logError(f"Invalid max range type in argument {argument_index} of row [dump]: {row}")
          return
        
        try:
          min = int(min)
          max = int(max)
        except ValueError:
          self.compiler.logError(f"Invalid immediate range in argument {argument_index} of row [dump]: {row}")
          return
        
        argument_range.addImmediateRange(min,max)
        
      if (arg_type is not None and arg_type == assembler.ArgumentType.REGISTER) or (assembler.ArgumentType.REGISTER in arg_type_set):
        regpattern = r"((r[01][0-5]|r[0-9])-(r[01][0-5]|r[0-9])(?=[^0-9]|$))|((?<![-])([a-zA-Z][a-zA-Z0-9]+)(;|$|\s))"
        index = 0
        while True:
          match = re.search(regpattern,range_string,pos=index)
          if match:
            if match.group(1):
              index = match.end(1)
              try:
                min_gpr = int(match.group(2)[1:])
                max_gpr = int(match.group(3)[1:])
              except ValueError:
                self.compiler.logError(f"Invalid register range in argument {argument_index} of row [dump]: {row}")
                return
              if min_gpr > max_gpr:
                min_gpr,max_gpr = max_gpr,min_gpr
              i = min_gpr
              while i <= max_gpr:
                self.known_registers.add(f"r{i}")
                argument_range.addValidRegister(f"r{i}")
                i += 1

            if match.group(5):
              index = match.end(5)
              self.known_registers.add(match.group(5))
              argument_range.addValidRegister(match.group(5))
          else:
            break

          
        
        pass
      

      type_arg = None
      if arg_type:
        type_arg = arg_type
      else:
        type_arg = arg_type_set
      try:
        bit_length = int(row[f"ARGUMENT_{argument_index}_BIT_LENGTH"])
      except ValueError:
        self.compiler.logError(f"Invalid bit length in argument {argument_index} of row [dump]: {row}")
        return
      new_argument = assembler.InstructionArgument(type_arg,argument_range,bit_length)
      return new_argument

    def readISA(self, source_file:Optional[str]=None):
      if source_file:
        self.setSource(source_file)
      if not self.source_file_path:
        self.compiler.logError("NO ISA SOURCE PATH SET.")
        return
      path = os.path.relpath(self.source_file_path)
      with open(path, 'r', newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        if not reader.fieldnames:
          return
        for row in reader:
          new_instruction = assembler.InstructionType('','',0,0)
          new_instruction.name = row["INSTRUCTION_NAME"]
          new_instruction.opcode = row["OPCODE"]
          new_instruction.flagbits = int(re.sub("_",'',row["BITFLAGS"]))
          argument_index = 1
          while row.get(f"ARGUMENT_{argument_index}_BIT_LENGTH", "") != "":
            new_argument = self._consumeArgument(argument_index,row)
            if new_argument:
              new_instruction.arguments.append(new_argument)
            else:
              self.compiler.logError(f"Failed to create argument {argument_index} of row [dump]: {row}")
            argument_index += 1
          
          self.instructions[new_instruction.name] = new_instruction
          
      for name, instruction in self.instructions.items():
        print(instruction)
      


  

class assembler:
  def __init__(self):

    pass

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
  
  class InstructionType:
    def __init__(self,name:str, opcode: str, bitcode: int, flagbits: int):
      self.opcode = opcode
      self.bitcode = bitcode
      self.flagbits = flagbits
      self.name = name
      self.arguments: List[assembler.InstructionArgument] = []
    
    def getArgCount(self) -> int:
      return len(self.arguments)
    
    def __str__(self):
      seperator = "\n        "
      return f"\"{self.name}\", OPCODE {self.opcode}, FLAGBITS {str(self.flagbits).zfill(16)},\n    ARGUMENTS:\n        {seperator.join([str(x) for x in self.arguments])}"
    
  class InstructionArgument:
    def __init__(self, type:Union[set[assembler.ArgumentType],assembler.ArgumentType],range:assembler.ArgumentRange, bit_length:int):
      self.type = type
      self.range = range
      self.bit_length = bit_length

    def __str__(self):
      if isinstance(self.type, set):
        return f"TYPES: {','.join([x.name for x in assembler.ArgumentType if x in self.type])}, RANGE: {str(self.range)}, BITLENGTH: {self.bit_length}"
      else:
        return f"TYPE: {self.type}, RANGE: {str(self.range)}, BITLENGTH: {self.bit_length}"

  class Argument:
    def __init__(self, argument_type: assembler.ArgumentType, value: Union[int, str]):
      self.argument_type = argument_type
      self.value = value

  class ArgumentType(Enum):
    REGISTER = 1
    IMMEDIATE = 2
    LABEL = 3
    VARIABLE = 4
    REGANDIMM = -1

  class ArgumentRange:
    def __init__(self, argument_types: assembler.ArgumentType):
      self.argument_types = argument_types
      self.immediate_min = 0
      self.immediate_max = 0
      self.valid_registers: List[str] = []

    def __str__(self):
      return f"{self.argument_types.name} {self.immediate_min}-{self.immediate_max},{','.join(self.valid_registers)}"
    
    def addImmediateRange(self, min_value: int, max_value: int):
      self.immediate_min = min_value
      self.immediate_max = max_value
      return self
    
    def addValidRegister(self, register: str):
      self.valid_registers.append(register)
      return self
      
    def addValidRegisters(self, registers: Union[List[str], Tuple[str, ...], str]):
      '''not good! it wont work if you call it :/ itll probably break things too'''
      if isinstance(registers, str):
        pattern = re.compile(r'\w+\-\w+|\w+')
        matches = pattern.findall(registers)
        for match in matches:
          if '-' in match:
            start_reg, end_reg = match.split('-')
            #commented to quiet errors, not that it is deprecated
            #start_index = REGISTERS.get(start_reg.upper(), None)
            #end_index = REGISTERS.get(end_reg.upper(), None)
            #if start_index is not None and end_index is not None and start_index <= end_index:
            #  for reg, idx in REGISTERS.items():
            #    if start_index <= idx <= end_index:
            #      self.addValidRegister(reg)
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


class preprocessor:
  def __init__(self, compiler:Compiler, tokeniser:tokeniser):
    self.compiler = compiler
    self.tokeniser = tokeniser
    self.entry_label_token: Optional[Compiler.Token] = None


  @staticmethod
  def _swapDefined(swap_target:str, swap_value:Union[str,List[Compiler.Token]], define_index:int, tokens: List[Compiler.Token], pattern:str, swap_index:int, tokens_to_replace:dict[Compiler.Token,int]):
    if isinstance(swap_value, str):
      for token in tokens[define_index:]:
        if re.search(pattern, token.value):
          token.value = re.sub(swap_target,lambda _: swap_value,token.value)
    else:
      for token in tokens[define_index:]:
        if re.search(pattern, token.value):
          tokens_to_replace[token] = swap_index
    return tokens
  
  def process(self, tokens:List[Compiler.Token], preprocessor_token_indexes:set[int], target:MakeParser.Target):
    tokens_to_replace: dict[Compiler.Token, int] = {}
    token_replacements: List[List[Compiler.Token]] = []
    new_tokens = tokens.copy()

    if target.entry_symbol is not None:
      self.entry_label_token = Compiler.Token(target.entry_symbol,-1,"SMAKE DEFINED")

    for definition, label in target.definitions:
      new_tokens = self._swapDefined(definition,label,0,new_tokens, f"\\b{definition}\\b", 0, {})
    for index in preprocessor_token_indexes:
      match tokens[index].value:
        case "@include":
          if len(new_tokens) < index+2:
            self.compiler.logError(f"Not enough arguments for @include in {tokens[index].getFileLocation()}")
            continue
          #tokenise and preprocess file
          filepath_token = tokens[index+1]
          filepath = filepath_token.value.strip().strip("\"")
          if os.path.splitext(filepath)[1] != ".spasm":
            filepath += ".spasm"

          found_file = False
          if not os.path.exists(filepath):
            
            for include_directory in target.include_directories:
              new_path = os.path.join(include_directory,filepath)
              if os.path.exists(new_path):
                filepath = new_path
                found_file = True
                break

            if target.working_directory and not found_file:
              new_path = os.path.join(target.working_directory, filepath)
              if os.path.exists(new_path):
                  filepath = new_path
                  found_file = True
          else:
            found_file = True

          if not found_file:
            self.compiler.logError(f"Failed to find file from @include directive with filepath \"{filepath}\", from {tokens[index].getFileLocation()}") 
            continue


          file_tokens, preproc_token_indexes = self.tokeniser.tokenise(filepath)
          processed_tokens = self.process(file_tokens, preproc_token_indexes,target)
          filepath_token.dead = True
          replacement_index = len(token_replacements)
          token_replacements.append(processed_tokens)
          tokens_to_replace[tokens[index]] = replacement_index

        case "@define":
          #define code replacements with @define LABEL \ {code} \
          if len(tokens) <= index+2:
            self.compiler.logError(f"Not enough arguments for @define in {tokens[index].getFileLocation()}")
            continue
          label_token = tokens[index+1]
          value_token = tokens[index+2]
          if value_token.value == '\\':
            #code swap
            swap_tokens: List[Compiler.Token] = []
            token_index = index+3
            while tokens[token_index].value != "\\":
              token_to_copy = tokens[token_index]
              swap_tokens.append(Compiler.Token(token_to_copy.value,token_to_copy.line,token_to_copy.filepath))#to avoid copying tokens that are made dead just below
              token_index += 1
              if not token_index < len(tokens):
                token_index -= 1
                self.compiler.logError(f"Ran out of tokens consuming block definition in {tokens[index].getFileLocation()}")
                break
            swap_index = len(token_replacements)
            token_replacements.append(swap_tokens) 
            new_tokens = self._swapDefined(label_token.value,swap_tokens,index+2,new_tokens, f"^{label_token.value}$", swap_index,tokens_to_replace)
            for token in new_tokens[index:token_index+1]:
              token.dead = True
          else:
            #value swap
            if re.match(r"[A-Za-z_]\w*", label_token.value):
              new_tokens = self._swapDefined(label_token.value,value_token.value,index+2,new_tokens, f"\\b{label_token.value}\\b", 0, {})
              for token in new_tokens[index:index+3]:
                token.dead = True
            else:
              self.compiler.logError(f"@define swap target not correctly formatted in {tokens[index].getFileLocation()}")
        case "@entry":
          if self.entry_label_token:
            self.compiler.logError(f"@entry redefinition in {tokens[index].getFileLocation()},\n    initial definition in {self.entry_label_token.getFileLocation()}")
          else:
            if index+1 < len(tokens):
              self.entry_label_token = tokens[index+1]
              target.entry_symbol = self.entry_label_token.value
            else:
              self.compiler.logError(f"No arguments for @entry in {tokens[index].getFileLocation()}")
          pass
    
    #handle replacement of defined blocks
    swapped_tokens: List[Compiler.Token] = []
    for token in new_tokens:
      if token in tokens_to_replace:
        replacement_index = tokens_to_replace[token]
        swapped_tokens.extend(token_replacements[replacement_index])
      else:
        swapped_tokens.append(token)
    
    #remove dead tokens (preprocessor stuffs)
    alive_tokens: List[Compiler.Token] = [token for token in swapped_tokens if not token.dead]


    

    return alive_tokens
    

class tokeniser:
  def __init__(self, compiler:Compiler):
    self.compiler = compiler

  def _getline(self,index:int,source:str):
    return source[:index].count('\n')+1
  
  def _read(self,filepath:str):
    try:
      with open(filepath, "r") as file:
        return file.read()
    except Exception as e:
      self.compiler.logError(f"Tokeniser failed opening file \"{filepath}\" with error \"{e}\"")
      return ""

  def tokenise(self, filepath:str):
    tokens: List[Compiler.Token] = []
    preprocessor_token_indexes: set[int] = set()
    def addToken(value:str,line:int):
      nonlocal tokens
      nonlocal filepath
      nonlocal index
      index += len(value)+1
      tokens.append(Compiler.Token(value.strip(),line,filepath))
    
    source_text = self._read(filepath)

    index = 0
    token_pattern = r"(\s*;[^\*].*\n)|(\s*;\*(.|\n)*?\*;)|(\s*\*;)|(\s*\"(.|\n)*?(?:[^\"\\]|\\.)*\")|(\s*\()|(\s*\))|(\s*\[)|(\s*\])|(\s*\{)|(\s*\})|(\s*@\S+)|(\s*\S+)"
    while len(source_text) > index:
      rematch = re.search(token_pattern,source_text,pos=index)
      if not rematch:
        self.compiler.logDebug(f"Reached NO MATCH (eof?) in {filepath}")
        break

      if rematch.group(14):
        #nonspecial
        addToken(rematch.group(14),self._getline(index,source_text))

      elif rematch.group(1):
        #single line comment
        index = rematch.end(1)

      elif rematch.group(2):
        #multi open
        index = rematch.end(2)

      elif rematch.group(4):
        assert False, "eek!"
        pass

      elif rematch.group(5):
        #string char
        addToken(rematch.group(5),self._getline(index,source_text))

      elif rematch.group(13):
        #preprocessor directives
        preprocessor_token_indexes.add(len(tokens))
        addToken(rematch.group(13),self._getline(index,source_text))

      elif rematch.group(7):
        #lbracket
        assert False, "unhandeled bracked in tokeniser"
        pass

      elif rematch.group(8):
        #rbracket
        assert False, "unhandeled bracked in tokeniser"
        pass

      elif rematch.group(9):
        #lsquare
        assert False, "unhandeled bracked in tokeniser"
        pass

      elif rematch.group(10):
        #rsquare
        assert False, "unhandeled bracked in tokeniser"
        pass

      elif rematch.group(11):
        #lcurly
        assert False, "unhandeled bracked in tokeniser"
        pass

      elif rematch.group(12):
        #rcurly
        assert False, "unhandeled bracked in tokeniser"
        pass
    
    return tokens, preprocessor_token_indexes

class ArgHelper:
  def __init__(self):
    pass

  def help(self):
    pass

class MakeParser:
  def __init__(self):
    self.last_task_error_holder: Optional[MakeParser.ErrorHolder] = None
    pass
  
  @staticmethod
  def _read(filepath:str):
    try:
      with open(filepath, "r") as file:
        return file.read()
    except Exception as e:
      print(f"MAKEFILE PARSER FAILED TO READ FILE WITH ERROR: {e}\nCOMPILATION ABORTED")
      return None
    
  class Token:
    class Type(Enum):
      UNKNOWN = 0
      DIRECTIVE = 1
      STRING = 2
      IDENTIFIER = 3
      OPENPAREN = 4
      CLOSEPAREN = 5
      COMMA = 6
      OPENBLOCK = 7
      CLOSEBLOCK = 8
    
    def __init__(self, type: MakeParser.Token.Type, value: str, line:int, col:int):
      self.type = type
      self.value = value
      self.line = line 
      self.col = col

    def getLocation(self):
      return f"LINE: {self.line}, COL: {self.col}"
    
  @staticmethod
  def _get_line_col(text: str, index: int) -> tuple[int, int]:
    line = text.count("\n", 0, index) + 1
    col = index - (text.rfind("\n", 0, index) + 1)
    return line, col + 1  # 1-based col
  
  @staticmethod
  def _tokenise(file: str):
    index = 0
    pattern = r'''
      (\.\w+\b)                | # directives like .target
      (\()                     | # open paren
      (\))                     | # close paren
      (\".*?\")                | # string literal (non-greedy)
      (,)                      | # comma
      (\b\w+\b)                | # identifiers
      ({)                      | # open block
      (})                      | # close block
      (;.*?(?:\n|$))           | # line comments
      (;\*.*?\*;)                # block comments
    '''
    repattern = re.compile(pattern, re.MULTILINE | re.DOTALL | re.VERBOSE)
    tokens: List[MakeParser.Token] = []

    while index < len(file):
      match = re.search(repattern,file,pos=index)
      if not match: break

      if match.group(1):
        index = match.end(1)
        line,col =  MakeParser._get_line_col(file,match.start(1))
        tokens.append(MakeParser.Token(MakeParser.Token.Type.DIRECTIVE, match.group(1).strip(),line,col))

      elif match.group(2):
        index = match.end(2)
        line,col = MakeParser._get_line_col(file,match.start(2))
        tokens.append(MakeParser.Token(MakeParser.Token.Type.OPENPAREN, match.group(2).strip(),line,col))

      elif match.group(3):
        index = match.end(3)
        line,col = MakeParser._get_line_col(file,match.start(3))
        tokens.append(MakeParser.Token(MakeParser.Token.Type.CLOSEPAREN, match.group(3).strip(),line,col))

      elif match.group(4):
        index = match.end(4)
        line,col = MakeParser._get_line_col(file,match.start(4))
        tokens.append(MakeParser.Token(MakeParser.Token.Type.STRING, match.group(4).strip(),line,col))

      elif match.group(5):
        index = match.end(5)
        line,col = MakeParser._get_line_col(file,match.start(5))
        tokens.append(MakeParser.Token(MakeParser.Token.Type.COMMA, match.group(5).strip(),line,col))

      elif match.group(6):
        index = match.end(6)
        line,col = MakeParser._get_line_col(file,match.start(6))
        tokens.append(MakeParser.Token(MakeParser.Token.Type.IDENTIFIER, match.group(6).strip(),line,col))
      
      elif match.group(7):
        index = match.end(7)
        line,col = MakeParser._get_line_col(file,match.start(7))
        tokens.append(MakeParser.Token(MakeParser.Token.Type.OPENBLOCK, match.group(7).strip(),line,col))

      elif match.group(8):
        index = match.end(8)
        line,col = MakeParser._get_line_col(file,match.start(8))
        tokens.append(MakeParser.Token(MakeParser.Token.Type.CLOSEBLOCK, match.group(8).strip(),line,col))

      elif match.group(9):
        index = match.end(9)

      elif match.group(10):
        index = match.end(10)

      # elif match.group(11):
      #   index = match.end(11)
      #   line,col = MakeParser._get_line_col(file,match.start(11))
      #   tokens.append(MakeParser.Token(MakeParser.Token.Type.UNKNOWN, match.group(11).strip(),line,col))

      

      else:
        assert False, "MAKE PARSER MATCH BUT NO GROUP...?"

    return tokens

  @staticmethod
  def _parse(makefile_path: str):
    file = MakeParser._read(makefile_path)
    if file is None: return None
    return MakeParser._tokenise(file)

  class ErrorHolder:
      def __init__(self):
        self.Debug: List[str] = []
        self.Errors: List[str] = []
        self.Warnings: List[str] = []

      def areWarnings(self):
        return len(self.Warnings) > 0
      
      def areDebugs(self):
        return len(self.Debug) > 0
      
      def areErrors(self):
        return len(self.Errors) > 0

      def consumeWarning(self):
        if len(self.Warnings) < 1: return None
        return self.Warnings.pop()
      
      def consumeErrors(self):
        if len(self.Errors) < 1: return None
        return self.Errors.pop()
      
      def consumeDebug(self):
        if len(self.Debug) < 1: return None
        return self.Debug.pop()
      
      def error(self, message:str):
        self.Errors.append(message)

      def warn(self, message:str):
        self.Warnings.append(message)

      def debug(self, message:str):
        self.Debug.append(message)
      
  class Target:
    class Format(Enum):
      BIN = 1
      HEX = 2
      ELF = 3

    def __init__(self):
      self.dependencies: list[MakeParser.Target] = []
      self.built = False
      self.include_directories: set[str] = set()
      self.entry_symbol: Optional[str] = None
      self.build_files: set[str] = set()
      self.name: Optional[str] = None
      self.output_directory: Optional[str] = None
      self.output_name: Optional[str] = None
      self.working_directory: Optional[str] = None
      self.definitions: set[Tuple[str,str]] = set()
      '''definition, value'''
      self.format: Optional[MakeParser.Target.Format] = None
      pass

    def verify(self):
      errors: MakeParser.ErrorHolder = MakeParser.ErrorHolder()
      if self.entry_symbol is None: errors.Errors.append("NO ENTRY")
      if len(self.build_files) < 1: errors.Errors.append("NO BUILD FILES")
      if self.name is None: errors.Errors.append("NO NAME")
      if self.output_directory is None: errors.Errors.append("NO OUTPUT DIRECTORY")
      if len(self.dependencies) < 1: errors.Debug.append("NO DEPENDENCIES")
      if len(self.include_directories) < 1: errors.Debug.append("NO INCLUDE DIRECTORIES")
      if not self.built: errors.Warnings.append("NOT BUILT")
      if self.working_directory is None: errors.warn("NO WORKING DIRECTORY")
      if len(self.definitions) < 1: errors.debug("NO DEFINITIONS")

      return errors
    
    def __str__(self):
      #ty ai :3
      indent = "    "
      lines = []

      def fmt(value):
          return "NONE" if value is None else value

      lines.append(f'Target "{fmt(self.name)}"')
      lines.append(f'{indent}Built: {self.built}')

      # Dependencies
      lines.append(f'{indent}Dependencies:')
      if self.dependencies:
          for dep in self.dependencies:
              lines.append(f'{indent * 2}- {dep}')
      else:
          lines.append(f'{indent * 2}NONE')

      # Include directories
      lines.append(f'{indent}Include directories:')
      if self.include_directories:
          for d in sorted(self.include_directories):
              lines.append(f'{indent * 2}- "{d}"')
      else:
          lines.append(f'{indent * 2}NONE')

      # Entry symbol
      lines.append(f'{indent}Entry symbol: "{fmt(self.entry_symbol)}"')

      # Build files
      lines.append(f'{indent}Build files:')
      if self.build_files:
          for f in sorted(self.build_files):
              lines.append(f'{indent * 2}- "{f}"')
      else:
          lines.append(f'{indent * 2}NONE')

      # Name
      lines.append(f'{indent}Name: "{fmt(self.name)}"')

      # Output
      lines.append(f'{indent}Output directory: "{fmt(self.output_directory)}"')
      lines.append(f'{indent}Output name: "{fmt(self.output_name)}"')

      # Working directory
      lines.append(f'{indent}Working directory: "{fmt(self.working_directory)}"')

      # Definitions
      lines.append(f'{indent}Definitions:')
      if self.definitions:
          for key, value in sorted(self.definitions):
              lines.append(f'{indent * 2}- {key} = {value}')
      else:
          lines.append(f'{indent * 2}NONE')

      # Format
      lines.append(f'{indent}Format: "{fmt(self.format)}"')

      return "\n".join(lines)


  class SearchType(Enum):
    SHALLOW = 1
    CDEPTH = 2
    ALL = 3

  class FileList:
    def __init__(self):
      self.name: Optional[str] = None
      self.list: set[str] = set()

    def clear(self):
      self.list.clear()

    def add(self, file:str):
      self.list.add(file)
    
    def __str__(self):
      indent = "    "
      lines = []

      name = self.name if self.name is not None else "NONE"

      lines.append(f'FileList "{name}"')

      lines.append(f'{indent}Files:')
      if self.list:
          for f in sorted(self.list):
              lines.append(f'{indent * 2}- "{f}"')
      else:
          lines.append(f'{indent * 2}NONE')

      return "\n".join(lines)



  @staticmethod
  def _consumeToken(tokens:List[Token]):
    if len(tokens) < 1: return None
    return tokens.pop(0)

  @staticmethod
  def _searchHandler(directory:str, extensions:set[str], remaining_search_depth:Optional[int]=None, error:Optional[MakeParser.ErrorHolder]=None):
    files: set[str] = set()
    for entry in os.listdir(directory):
      fullpath = os.path.normpath(os.path.join(directory, entry))
      # if error:
      #   print(f"IN DIR: \"{directory}\", ENTRY: \"{entry}\", FULLPATH: \"{fullpath}\"")

      if os.path.isfile(fullpath) and os.path.splitext(entry)[1] in extensions:
        files.add(fullpath)
      elif os.path.isdir(fullpath):
        if remaining_search_depth and remaining_search_depth > 0:
          files.update(MakeParser._searchHandler(fullpath,extensions,remaining_search_depth-1))
        elif remaining_search_depth is None:
          files.update(MakeParser._searchHandler(fullpath,extensions,None))
    return files
              

  @staticmethod
  def _searchDirectory(directory:str, search_type:SearchType, extensions: set[str], cdepth:Optional[int]=None, working_directory:Optional[str]=None, error:Optional[ErrorHolder]=None):
    directory_to_search:Optional[str] = None
    if directory.strip() == "":
      directory = "."
    
    dir_is_abs = os.path.isabs(directory)
    if not dir_is_abs and working_directory is None:
      if error:
        error.error(f"{directory} IS NOT ABSOLUTE AND NO WORKING DIRECTORY WAS SPECIFIED")
      return None
    elif not dir_is_abs and working_directory:
      directory_to_search = os.path.join(working_directory, directory)
    elif dir_is_abs:
      directory_to_search = directory
    else:
      assert False, "UNHANDLED SEARCH DIRECTORY CASE"

    if directory_to_search is None or not os.path.exists(directory_to_search):
      if error:
        error.error(f"GIVEN {directory}, NO SEARCHABLE DIRECTORY WAS FOUND.")
      return None
    
    files: set[str] = set()

    depth = {
    MakeParser.SearchType.SHALLOW: 0,
    MakeParser.SearchType.CDEPTH: cdepth,
    MakeParser.SearchType.ALL: None
    }.get(search_type)

    files.update(MakeParser._searchHandler(directory_to_search, extensions, depth, error=error))
    return files
  TokenTypeSpec: TypeAlias = Union[
      Token.Type,
      Tuple[Union[Token.Type, Tuple[Token.Type, ...]], ...]
  ]
    
  def build(self, makefile_path: str, arg_labels:Optional[set[str]]=None, strict:bool=True, dump:bool=False, dump_to_file:bool=False):
    tokens = self._parse(makefile_path)
    if tokens is None: return None, None
    makefile_dir = os.path.dirname(os.path.abspath(makefile_path))

    labels:set[str] = set()
    if arg_labels is not None: labels.update(arg_labels)
    targets: dict[str, MakeParser.Target] = {}
    flists: dict[str, MakeParser.FileList] = {}
    errors: MakeParser.ErrorHolder = MakeParser.ErrorHolder()
    self.last_task_error_holder = errors

    def _verifyTokenType(expected:Union[MakeParser.Token.Type, Tuple[MakeParser.Token.Type,...]], blame_directive:Optional[MakeParser.Token]=None):
      nonlocal tokens
      nonlocal errors
      nonlocal moving_to_next_directive
      if not tokens: 
        moving_to_next_directive = True
        return False, None
      token = MakeParser._consumeToken(tokens)
      if token is None:
        errors.error("TOKEN WAS NONE?")
        moving_to_next_directive = True
        return False, None
      if isinstance(expected, tuple):
        if token.type not in expected:
          error_string = f"EXPECTED TYPES: {','.join([x.name for x in expected])}; GOT {token.type.name}, AT {token.getLocation()}"
          if blame_directive:
            error_string += f", blame directive \"{blame_directive.value}\" @ {blame_directive.getLocation()}"
          errors.error(error_string)
          moving_to_next_directive = True
          return False, token
      else:
        if token.type is not expected:
          error_string = f"EXPECTED TYPE: {expected}, GOT {token.type.name}, AT {token.getLocation()}"
          if blame_directive:
            error_string += f", blame directive \"{blame_directive.value}\" @ {blame_directive.getLocation()}"
          errors.error(error_string)
          moving_to_next_directive = True
          return False, token

      return True, token

    def _handleSearch():
      nonlocal moving_to_next_directive
      ok, result_tokens = getAndVerifyTokens((MakeParser.Token.Type.OPENPAREN,
                                              MakeParser.Token.Type.IDENTIFIER,
                                              MakeParser.Token.Type.COMMA,
                                              MakeParser.Token.Type.IDENTIFIER,
                                              MakeParser.Token.Type.COMMA,
                                              MakeParser.Token.Type.STRING,
                                              MakeParser.Token.Type.COMMA
                                              ), blame_directive=token)
      btoken,flist_token,ctoken,search_type_token,ctoken,extensions_token,ctoken = result_tokens
      
      if not ok:  return None, None, False
      flist = flists.get(flist_token.value) # type: ignore
      if flist is None: errors.error(f"FLIST DOES NOT EXIST AT {flist_token.getLocation()}"); moving_to_next_directive=True; return None, None, False # type: ignore

      SEARCH_TYPE_PATTERN = re.compile(r'''^(?:
        (shallow)       | #shallow
        (cdepth([0-9]+))| #cdepth
        (all)             #all
        
        )''', re.VERBOSE)

      match = re.match(SEARCH_TYPE_PATTERN,search_type_token.value) # type: ignore
      if not match: errors.error(f"UNKNOWN SEARCH TYPE \"{search_type_token.value}\" AT {search_type_token.getLocation()}"); return None, None, False # type: ignore

      search_type:Optional[MakeParser.SearchType] = None
      search_depth:Optional[int] = None
      if match.group(1):
        search_type = MakeParser.SearchType.SHALLOW
      elif match.group(2):
        search_type = MakeParser.SearchType.CDEPTH
        try:
          search_depth = int(match.group(3))
        except ValueError:
          errors.error(f"NON-INT SEARCH DEPTH (lol how did you do that the regex shouldve caught that!!!)")
          moving_to_next_directive=True; return None, None, False
      elif (match.group(4)):
        search_type = MakeParser.SearchType.ALL

      if search_type is None: errors.error(f"SEARCH TYPE COULDNT BE SPECIFIED AT {search_type_token.getLocation()}");moving_to_next_directive = True;return None, None, False # type: ignore

      extensions:set[str] = set([x.strip() for x in extensions_token.value.strip("\"").split(",")]) # type: ignore

      directories:set[str] = set()

      err = False
      while not err:
        ok,result_tokens = getAndVerifyTokens((MakeParser.Token.Type.STRING,
                                                 (MakeParser.Token.Type.COMMA,
                                                  MakeParser.Token.Type.CLOSEPAREN)
                                              ), blame_directive=token)
        extension_token, ctoken = result_tokens
        if not ok: err = True; break

        directories.add(extension_token.value.strip("\"").strip()) # type: ignore

        if ctoken.type == MakeParser.Token.Type.CLOSEPAREN: break # type: ignore
      if err:  return None, None, False
      
      files:set[str] = set()
      for directory in directories:
        dir_result = MakeParser._searchDirectory(directory,search_type,extensions,search_depth,makefile_dir,error=errors)
        if dir_result is None: moving_to_next_directive=True; return None, None, False
        files.update(dir_result)

      return files, flist, True
    
    def getAndVerifyTokens(expected_series:MakeParser.TokenTypeSpec, blame_directive:Optional[MakeParser.Token]=None)->tuple[bool,tuple[Optional[MakeParser.Token],...]]:
      if isinstance(expected_series, tuple):
        whole_ok = True
        tokens:List[Optional[MakeParser.Token]] = []
        for expected in expected_series:
          ok,token = _verifyTokenType(expected,blame_directive=blame_directive)
          whole_ok = ok and whole_ok
          tokens.append(token)
        return whole_ok, tuple(tokens)
      else:
        ok, token = _verifyTokenType(expected_series,blame_directive=blame_directive)
        return ok, (token,)

    def getTarget(target_token):
      nonlocal moving_to_next_directive
      target = targets.get(target_token.value) # type: ignore
      if target is None: errors.error(f"Unknown target at {target_token.getLocation()}"); moving_to_next_directive=True # type: ignore
      return target

    def discardUntilType(expected:Union[MakeParser.Token.Type,tuple[MakeParser.Token.Type,...]], source_token:MakeParser.Token):
      nonlocal tokens
      nonlocal errors
      if tokens is None:
        assert False, "uuuuuugggggghhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      found = False
      if not isinstance(expected,tuple):
        expected = (expected,)
      while not found:
        token = MakeParser._consumeToken(tokens)
        if token is None: errors.error(f"COULD NOT CONSUME TOKEN WHILE CONDITIONALLY DISCARDING, FROM {source_token.getLocation()}"); return
        if token.type in expected:
          found = True



    moving_to_next_directive = False
    while len(tokens) > 0:
      token = self._consumeToken(tokens)
      if token is None: errors.error("TOKEN WAS NONE?"); return None, errors
      if token.type is not MakeParser.Token.Type.DIRECTIVE:
        if not moving_to_next_directive:
          errors.error(f"EXPECTED DIRECTIVE, GOT {token.type.name}, AT {token.getLocation()}")
        continue
      moving_to_next_directive = False
      directive = token.value[1:]
      match directive:
        case "target":
          ok, id_token = _verifyTokenType(MakeParser.Token.Type.IDENTIFIER, blame_directive=token)
          if not ok or id_token is None:  continue
          
          if id_token.value in labels: errors.error(f"Redefinition of already declared label \"{id_token.value}\" at {id_token.getLocation()}"); moving_to_next_directive=True; continue
          target = MakeParser.Target()
          target.name = id_token.value
          target.working_directory = makefile_dir
          target.output_name = target.name
          targets[target.name] = target
          labels.add(id_token.value)

        case "include_directory":
          ok, result_tokens = getAndVerifyTokens((MakeParser.Token.Type.OPENPAREN,MakeParser.Token.Type.IDENTIFIER,MakeParser.Token.Type.COMMA), blame_directive=token)
          btoken, target_token, ctoken = result_tokens
          if not ok: continue

          directory_tokens: set[MakeParser.Token] = set()
          err = False
          while not err:
            ok, result_tokens = getAndVerifyTokens((MakeParser.Token.Type.STRING,(MakeParser.Token.Type.COMMA,MakeParser.Token.Type.CLOSEPAREN)), blame_directive=token)
            dir_token,endtoken = result_tokens
            if not ok: err = True; break
            directory_tokens.add(dir_token) # type: ignore
            if endtoken.type == MakeParser.Token.Type.CLOSEPAREN: break # type: ignore
          if err:  continue

          directories: set[str] = set()
          for dir_token in directory_tokens:
            dir_path = dir_token.value.strip('"')
            if os.path.isabs(dir_path):
              directories.add(dir_path)
            else:
              directories.add(os.path.join(makefile_dir, dir_path))


          target = getTarget(target_token)
          if target is None: continue
          target.include_directories.update(directories)

        case "working_directory":
          assert False, f"UNIMPLEMENTED DIRECTIVE FOUND AT {token.getLocation}"

        case "flist":
          ok, id_token = _verifyTokenType(MakeParser.Token.Type.IDENTIFIER, blame_directive=token)
          if not ok or id_token is None:  continue

          if id_token.value in labels: errors.error(f"Redefinition of already declared label \"{id_token.value}\" at {id_token.getLocation()}"); moving_to_next_directive=True; continue
          flist = MakeParser.FileList()
          flist.name = id_token.value
          flists[flist.name] = flist
          labels.add(id_token.value)

        case "search_set":
          files, flist, ok = _handleSearch()
          if not ok or files is None or flist is None:moving_to_next_directive = True; continue
          flist.list = files
        case "search_add":
          files, flist, ok = _handleSearch()
          if not ok or files is None or flist is None:moving_to_next_directive = True; continue
          flist.list.update(files)
        case "add_target":
          ok,result_tokens = getAndVerifyTokens((MakeParser.Token.Type.OPENPAREN,MakeParser.Token.Type.IDENTIFIER,MakeParser.Token.Type.COMMA), blame_directive=token)
          btoken,target_token,ctoken = result_tokens
          if not ok: continue
          
          target = getTarget(target_token)
          if target is None: continue

          raw_directories:set[str]=set()
          raw_flists:set[MakeParser.FileList]=set()

          err = False
          while not err:
            ok, id_token = _verifyTokenType((MakeParser.Token.Type.STRING,MakeParser.Token.Type.IDENTIFIER), blame_directive=token)
            if not ok or id_token is None: err=True; break
            match id_token.type:
              case MakeParser.Token.Type.STRING:
                directory = id_token.value.strip("\"").strip()
                #check is abs
                if os.path.isabs(directory):
                  raw_directories.add(directory) 
                else:
                  #check relative to makefile
                  makefile_rel_path = os.path.join(makefile_dir,directory)
                  if os.path.exists(makefile_rel_path):
                    raw_directories.add(makefile_rel_path)
                  else:
                    #check relative to include
                    success = False
                    for include_directory in target.include_directories:
                      include_rel_path = os.path.join(include_directory,directory)
                      if os.path.exists(include_rel_path):
                        success = True
                        raw_directories.add(include_rel_path)

                    if not success: errors.error(f"COULD NOT FIND FILE {directory} ANYWHERE!! >:(, REFERENCED AT {id_token.getLocation()}")

              case MakeParser.Token.Type.IDENTIFIER:
                flist = flists.get(id_token.value)
                if flist is None: 
                  errors.error(f"FLIST {id_token.value} DOESNT EXIST, REFERENCED AT {id_token.getLocation()}")
                else:
                  raw_flists.add(flist)

            ok, ctoken = _verifyTokenType((MakeParser.Token.Type.COMMA,MakeParser.Token.Type.CLOSEPAREN), blame_directive=token)
            if not ok or ctoken is None: err=True; break
            if ctoken.type == MakeParser.Token.Type.CLOSEPAREN: break
          if err:  continue

          target.build_files.update(raw_directories)
          for flist in raw_flists:
            target.build_files.update(flist.list)

        case "define":
          ok,result_tokens = getAndVerifyTokens((MakeParser.Token.Type.OPENPAREN,
                                                 MakeParser.Token.Type.IDENTIFIER,
                                                 MakeParser.Token.Type.COMMA,
                                                 MakeParser.Token.Type.STRING,
                                                 MakeParser.Token.Type.COMMA,
                                                 MakeParser.Token.Type.STRING,
                                                 MakeParser.Token.Type.CLOSEPAREN), blame_directive=token)
          btoken,target_token,ctoken,define_token,ctoken,value_token,btoken = result_tokens
          if not ok: continue

          target = getTarget(target_token)
          if target is None: continue

          definition = define_token.value.strip("\"").strip() # type: ignore
          
          value = value_token.value.strip("\"").strip() # type: ignore

          if any(t[0] == definition for t in target.definitions): errors.error(f"PREPROCESSOR DEFINITION \"{definition}\" ALREADY DEFINED IN TARGET: {target.name}, SOURCE {define_token.getLocation()}"); moving_to_next_directive = True; continue # type: ignore
          target.definitions.add((definition,value))

        case "entry":
          ok,result_tokens = getAndVerifyTokens((MakeParser.Token.Type.OPENPAREN,
                                                 MakeParser.Token.Type.IDENTIFIER,
                                                 MakeParser.Token.Type.COMMA,
                                                 MakeParser.Token.Type.STRING,
                                                 MakeParser.Token.Type.CLOSEPAREN), blame_directive=token)
          btoken,target_token,ctoken,symbol_token,btoken = result_tokens
          if not ok: continue

          target = getTarget(target_token)
          if target is None: continue

          entry_symbol = symbol_token.value.strip("\"").strip() # type: ignore
          if target.entry_symbol is not None: errors.error(f"REDEFINITION OF ENTRY SYMBOL AT {symbol_token.getLocation()}"); moving_to_next_directive = True; continue # type: ignore
          target.entry_symbol = entry_symbol

        case "output":
          ok, result_tokens = getAndVerifyTokens((MakeParser.Token.Type.OPENPAREN,
                                                  MakeParser.Token.Type.IDENTIFIER,
                                                  MakeParser.Token.Type.COMMA,
                                                  MakeParser.Token.Type.STRING,
                                                  (MakeParser.Token.Type.CLOSEPAREN,MakeParser.Token.Type.COMMA),
                                                  ), blame_directive=token)
          btoken,target_token,ctoken,path_token,end_token = result_tokens
          if not ok: continue

          target = getTarget(target_token)
          if target is None: continue

          if end_token.type == MakeParser.Token.Type.COMMA: # type: ignore
            ok, result_tokens = getAndVerifyTokens((MakeParser.Token.Type.STRING,
                                                    MakeParser.Token.Type.CLOSEPAREN
                                                    ), blame_directive=token)
            target_name_token,btoken = result_tokens
            if not ok: continue
            target_name = target_name_token.value.strip("\"").strip() # type: ignore
            target.output_name = target_name
          
          path = path_token.value.strip("\"").strip() # type: ignore
          opath = path
          if not os.path.abspath(path):
            path = os.path.join(makefile_dir,path)
            if not os.path.exists(path):
              try:
                os.makedirs(path, exist_ok=True)  # create all intermediate dirs
                errors.debug(f"CREATED PATH BECAUSE IT DIDNT EXIST! {path} from {opath}, on {path_token.getLocation()}") # type: ignore
              except Exception as e:
                errors.error(f"Failed to create directory '{path}': {e}")
                moving_to_next_directive = True
                continue
          target.output_directory = path

        case "format":
          ok, result_tokens = getAndVerifyTokens((MakeParser.Token.Type.OPENPAREN,
                                                  MakeParser.Token.Type.IDENTIFIER,
                                                  MakeParser.Token.Type.COMMA,
                                                  MakeParser.Token.Type.STRING,
                                                  MakeParser.Token.Type.CLOSEPAREN
                                                  ), blame_directive=token)
          btoken,target_token,ctoken,type_token,btoken = result_tokens
          if not ok: continue

          target = getTarget(target_token)
          if target is None: continue

          match type_token.value.strip("\"").strip():  # type: ignore
            case "bin":
              target.format = MakeParser.Target.Format.BIN
            case "hex":
              target.format = MakeParser.Target.Format.HEX
            case "elf":
              target.format = MakeParser.Target.Format.ELF

        case "depends":
          ok, result_tokens = getAndVerifyTokens((MakeParser.Token.Type.OPENPAREN,
                                                  MakeParser.Token.Type.IDENTIFIER,
                                                  MakeParser.Token.Type.COMMA
                                                  ), blame_directive=token)
          btoken,target_token,ctoken = result_tokens
          if not ok: continue

          target = getTarget(target_token)
          if target is None: continue

          err = False
          while not err:
            ok, result_tokens = getAndVerifyTokens((MakeParser.Token.Type.STRING,
                                                    (MakeParser.Token.Type.COMMA, MakeParser.Token.Type.CLOSEPAREN)
                                                    ), blame_directive=token)
            dependency_token,ctoken = result_tokens
            if not ok: err = True; break

            dependency_name = dependency_token.value.strip("\"").strip() # type: ignore
            dependency_target = targets.get(dependency_name)
            if dependency_target is None: err = True; errors.error(f"DEPENDENCY OF {target.name} WAS NOT FOUND, from {dependency_token.getLocation()}"); break # type: ignore
            if target in dependency_target.dependencies:
              #circular dependency
              errors.error(f"CIRCULAR DEPENDENCY FOUND BETWEEN {target.name} AND {dependency_target.name}, at {target_token.getLocation()}") # type: ignore
              err = True; break
            if dependency_target in target.dependencies:
              errors.warn(f"DEPENDENCY {dependency_target.name} OF {target.name} IS ALREADY DEPENDANT (dependency readded)")
            else:
              target.dependencies.append(dependency_target)

            if ctoken.type == MakeParser.Token.Type.CLOSEPAREN: # type: ignore
              break
          if err: continue
          #assert False, f"UNIMPLEMENTED DIRECTIVE FOUND AT {token.getLocation}"
        case "label":
          ok,label_token = _verifyTokenType(MakeParser.Token.Type.IDENTIFIER, blame_directive=token)
          if not ok or label_token is None: continue

          label = label_token.value
          if label in labels and strict:
            errors.error(f"(STRICT) LABEL REDEFINITION OF \"{label}\" AT {label_token.getLocation()}")
            continue
          elif label in targets:
            errors.error(f"LABEL REDEFINITION OF A TARGET \"{label}\" AT {label_token.getLocation()}")
            continue
          elif label in flists:
            errors.error(f"LABEL REDEFINITION OF A FLIST \"{label}\" AT {label_token.getLocation()}")
            continue
          labels.add(label)

        


          #strict?
          #assert False, f"UNIMPLEMENTED DIRECTIVE FOUND AT {token.getLocation}"
        case "ifdef":
          #discardUntilType
          ok, result_tokens = getAndVerifyTokens((MakeParser.Token.Type.IDENTIFIER,
                                                  MakeParser.Token.Type.OPENBLOCK
                                                  ), blame_directive=token)
          label_token, btoken = result_tokens
          if not ok: continue

          label = label_token.value # type: ignore
          if not label in labels:
            discardUntilType(MakeParser.Token.Type.CLOSEBLOCK,token)

        case "ifndef":
          #discardUntilType
          ok, result_tokens = getAndVerifyTokens((MakeParser.Token.Type.IDENTIFIER,
                                                  MakeParser.Token.Type.OPENBLOCK
                                                  ), blame_directive=token)
          label_token, btoken = result_tokens
          if not ok: continue

          label = label_token.value # type: ignore
          if label in labels:
            discardUntilType(MakeParser.Token.Type.CLOSEBLOCK,token)

        case _:
          errors.error(f"Unknown directive \"{directive}\" at {token.getLocation()}")

    if dump or dump_to_file:
      dump_string:str = ''
      dump_string += "[[[]]] BUILD DUMP [[[]]]\n"
      dump_string += "Labels:\n"
      for label in labels:
        dump_string += f"    {label}\n"
      dump_string += '\n'
      dump_string += "File lists:\n"
      for flist_name, flist in flists.items():
        dump_string += f"{str(flist)}\n"
      dump_string += '\n'
      dump_string += "Targets:\n"
      for target_name, target in targets.items():
         dump_string += f"{str(target)}\n"
      dump_string += '\n'
      dump_string +="[[[]]] END OF BUILD DUMP [[[]]]"

      if dump:
        print(dump_string)
      if dump_to_file:
        try:
          path = os.path.join(makefile_dir,"_smake_build_dump_.txt")
          with open(path, "w", encoding="utf-8") as f:
            f.write(dump_string)
            f.close()
        except Exception as e:
          errors.error(f"Failed to dump smake to file with error: {e}")
    
    return targets, errors




    








parser = argparse.ArgumentParser()
parser.add_argument("--input", "-i", help=" file")
parser.add_argument("--debug", "-d", action="store_true", help="Enable debug mode")
parser.add_argument("--level", "-l", type=int, default=1, help="Optimization level")
args = parser.parse_args()


instruction_set_file_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),"instructionset.csv")
      
makefiler = MakeParser()
targets, errors = makefiler.build("D:\\logisim prohects\\16bit cpu\\test.smake", dump=True,dump_to_file=True)
if targets is None:
  print("NO TARGETS FROM MAKEFILE COMPILATION")
  raise SystemExit
if errors:

  while errors.areDebugs():
    print(errors.consumeDebug())
  while errors.areWarnings():
    print(errors.consumeWarning())
  while errors.areErrors():
    print(errors.consumeErrors())

compiler = Compiler()
compiler.setWorkingDirectory("D:\\logisim prohects\\16bit cpu")
for target_name, target in targets.items():
  compiler.compile(target, instruction_set_file_path)

while compiler.areWarnings():
  print(f"[WARNING]: {compiler.consumeWarning()}")

while compiler.areErrors():
  print(f"[ERROR]: {compiler.consumeError()[0]}") # type: ignore

while compiler.areDebugs():
  print(f"[DEBUG]: {compiler.consumeDebug()}")

# pattern = r"(;)|(\")"
# src = re.search(pattern,"(;)|(\")", pos=2)
# if src:
#   print(src.group(1))
#   print(src.group(2))