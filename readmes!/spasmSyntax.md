# Spasm Syntax

## Basic Syntax

#### A note about the lexer.

  It delimits non-special characters (Ie: + - * / { } [ ] , . ) by whitespace. ~~It doesn't tokenise newline characters.~~

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

- ### Constants

  The compiler allows binary, hex, or decimal constants (via 0b, 0x, or standard number). All of which can be specified as negative by a preceeding '-', the compiler will convert the number to its negative form.  
  The compiler also allows for mathematical expressions for constants, as long as they are enclosed by square brackets like so.  
  ```
  ADI r0,r1,[8*2]
  ```
  Preprocessor definitions which are replaced by numbers can also be used.
  ```
  @define BYTES 2
  ADI r0,r1,[8*BYTES]
  ```  
  Labels or declared data can also be used (will be interpreted as their memory address).
  ```
  WORD halfInt, 0
  ...
  halfIntMove:
  ADI r0,r1,[halfInt - 2]
  ...
  SBI r0,r1,[halfIntMove + 2]
  ```
  *Do beware labels do need to be referred to at their global scope (ie even if you are in main.helper, you need to refer to main.helper.loop, not .loop or etc.)*  


  Mathematical order of operations DOES apply.

- ### Data Declaraction

  - #### Where does the variable name point?
    The variable name represents the memory address of the lowest byte.

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
  ARRAY  numbers, 8, 2, {52,385,209,295}
  ```

  Where the arguments for TEXT are:  
  >TEXT variableName, byteLength, initialisingValue/characterArray (if quoted)

  The arguments for ARRAY are:
  >ARRAY variableName, byteLength, bytesPerElement, {Element0, Element1, ..., ElementN}  

  *for a maximum of byteLength/bytesPerElement elements.*

  "byteLength" can be replaced with the keyword AUTO where the compiler will determine the length in bytes of the string or array.  
  *(the compiler will throw an error for TEXT with initialising value instead of string)*

  ```
  TEXT byteArray, AUTO, 0                   
  ; Compilation error (will fail)

  TEXT byteArray, AUTO, "Hello World!!!!!" 
  ; Will be 16 bytes of ascii

  ARRAY  numbers, AUTO, 2, {52,385,209,295}
  ; Will be 8 bytes (4x2 byte elements)
  ```  

  TEXT types with an initialising value or ARRAY types can be marked as using signed integers by adding a ", SIGNED" after the initial arguments. This will ensure that all numbers are in the range of the specified byte length's signed range and will throw errors if not.  
  Using a '-' before numbers in a NON signed declaration will convert THAT number only to the byte length's signed form, but will NOT ensure that other numbers are in the signed range (non-signed declared TEXT or ARRAY will throw errors if numbers are NOT in the unsigned range of the byte length.)


## Macros

- ### Function Macros

  Function macros are USED with syntax like instructions, where each argument is taken and replaced relatively inside of the macro before the macro inserts itself.  
  Define them like so:

  ```
  @define macroName(Arg0,Arg1,...,ArgN) { 
    ADD Arg0, Arg1, Arg0
    SUB Arg1, Arg1, Arg0
    ...
  }
  ```

  The code between the curly braces is what the macro will expand to after replacing arguments.  
  Here is an example:
  ```
  @define MUL(rd,rA,rB) {
    PUSH r15         ; Push r15 to stack
    MOV r0, r15      ; r15 = 0
    MOV r0, rd       ; rd = 0
    ADD rd, rA, rd   ; rd = rd + rA
    ADI r15, 1       ; r15 += 1
    SUB r0, rB, r15  ; equivelant to CMP rB, r15
    BRIS NE, 3       ; Branch subtract 3 if not equal
    POP r15
  }

  /// code or etc... ///

  MUL r2, r1, r5

  /// would expand to: ///

  PUSH r15       
  MOV r0, r15    
  MOV r0, r2     
  ADD r2, r1, r2
  ADI r15, 1     
  SUB r0, r5, r15
  BRIS NE, 3     
  POP r15
  ```

  *Function macros can have NO arguments, to just insert a code block or etc.*

- ### Replacement Macros

  Replacement macros simply replace tokens where the value (string) is equal to the name of the macro, with the macro value.  
  For example:

  ```
  @define BYTEMAX 0xff

  /// code or etc... ///

  ADI r1, BYTEMAX

  /// would expand to ///

  ADI r1, 0xff
  ```

  The syntax is as follows:

  ```
  @define macroName macroValue
  ```