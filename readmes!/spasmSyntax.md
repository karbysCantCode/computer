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

## Macros

- ### Function Macros

- ### Replacement Macros