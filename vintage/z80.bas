' Z-80 Machine Language Compiler
' Charles Chiou
' Date: 8-11-95
'
' This program converts Z80 opcodes (assembly language) stored in text files
' into Z80 machine codes in another text file.
' Format of the input file:
' The first line must be the beginning address of the machine codes.
' If there is error, the compiler returns the line number where the first
' error occured and no machine codes are converted.
' Format of the output file:
' The first line of the file corresponds to the start of the address
' the rest of the file contains the actual hex-decimal machine codes of the
' program.

DECLARE SUB z80code ()

z80code

SUB z80code

OPEN "z80.opc" FOR INPUT AS #1
DO
LINE INPUT #1, line$
   



LOOP UNTIL (EOF(1))
CLOSE #1

END SUB

