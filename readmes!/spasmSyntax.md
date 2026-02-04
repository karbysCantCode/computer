# Spasm Syntax

## Basic Syntax

#### A note about the lexer.

  It delimits non-special characters (Ie: + - * / { } [ ] , . ) by whitespace. It doesn't tokenise newline characters.

  Non-special or number tokens are expected to start with a word character or underscore and will be parsed until whitespace or special character.

- ### Instructions

  Typically a newline denotes a new instruction, although 
  the lexer does not tokenise newline characters, and the parser only expects to find the sufficient number of argument expressions following an instruction keyword.

  The format is as follows:

  >InstructionName Arg1, Arg2, ..., ArgN

  Specifically, the first arg is seperated from the instruction by whitespace, where following arguments are comma seperated.

- ### Labels

  Labels are defined by an identifier followed by a colon, for example:

  >LabelName:

  Labels local to other labels are defined by prefixing a the parent label followed by a dot to the label declaration - for example:

  >; *declaration of loop*  
  main.loop:

  This can be repeated with no limit, ie:

  >Main.Helper.Save:

  or

  >Main.MainRoutine.SaveProcess.IOHelper.DeviceScan.RecieveRoutine:

  *The point being there is no limit to how deep this can go.*  
    
  #### A Note About Scopes
  variables theoretically have a local scope but um lowkey why arent you just managing it urself u lazy twat

- ### Data Declaraction

  Data can be declared in multiple ways.  
  First by keywords with constant size:

  ```
  >BYTE isTrue, 0            ; 8 bits  / 1 byte  
  >WORD number, 0xffff       ; 16 bits / 2 bytes 
  >DWORD fullInt, 0xffffffff ; 32 bits / 4 bytes
  ```

  Alternatively you can declare a set number of bytes:
  ```
  TEXT byteArray, 64, 0                  ; sets all 64 bytes to 0
  TEXT byteArray, 16, "Hello World!!!!!" ; parsed as ascii
  ARRAY  numbers, 8, 2, [52,385,209,295]
  AUTO 
  ```

  Where the arguments for TEXT are:  
  >TEXT variableName, byteLength, initialisingValue/characterArray (if quoted)

  The arguments for ARRAY are:
  >ARRAY variableName, byteLength, bytesPerElement, [Element0, Element1, ..., ElementN]  

  *for a maximum of byteLength/bytesPerElement elements.*

## Macros

- ### Function Macros

- ### Replacement Macros