
' ABC-80 Utilities : On line programming from the PC/AT
' Programmer: Charles Chiou
' Date      : 14/July/1993
' Version   : QuickBasic ver 4.00

 DECLARE SUB ABCpattern ()
 DECLARE SUB OutABC (Address#, ByteOut%)
 DECLARE SUB InABC (Address#)
 DECLARE SUB RunABC (Address#)
 DECLARE SUB InitialABC ()
 DECLARE SUB DisplayTable (Address#)
 DECLARE SUB NewAddress ()
 DECLARE SUB DataInput ()
 DECLARE SUB Insert (Address#)
 DECLARE SUB Delete (Address#)
 DECLARE SUB ColourKeyPressed (WhichKey%)
 DECLARE SUB SaveProg ()
 DECLARE SUB LoadProg ()
 DECLARE SUB HexIn ()

 ON KEY(1) GOSUB Start
Start:
 KEY(1) ON
 InitialABC
 SCREEN 12
 COLOR 7
 CLS
 ABCpattern

  ' drawing a frame
 LOCATE 6, 12:  PRINT "                                           "
 LOCATE 7, 12:  PRINT " ีอออออออออออออออออออัอออออออออออออออออออธ "
 LOCATE 8, 12:  PRINT " ณ     Address       ณ        Byte       ณ "
 LOCATE 9, 12:  PRINT " รฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤลฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤด "
 LOCATE 10, 12: PRINT " ณ                   ณ                   ณ "
 LOCATE 11, 12: PRINT " ณ                   ณ                   ณ "
 LOCATE 12, 12: PRINT " ณ                   ณ                   ณ "
 LOCATE 13, 12: PRINT " รฤ*               *ฤลฤ*               *ฤด "
 LOCATE 14, 12: PRINT " ณ                   ณ                   ณ "
 LOCATE 15, 12: PRINT " ณ                   ณ                   ณ "
 LOCATE 16, 12: PRINT " ณ                   ณ                   ณ "
 LOCATE 17, 12: PRINT " ิอออออออออออออออออออฯอออออออออออออออออออพ "
 LOCATE 18, 12: PRINT "                                           "

 ' Start with address 1000h   
 Address# = &H1000
 DIM ByteDisplay(7)

 DisplayTable Address#

 '                      Main Program
 ' Key definitions :
 ' Numeral keys : 0,1,2,3,4,5,6,7,8,9,A,B,C,D,E,F
 ' Page Up Key  : Retreat one address byte
 ' Page Down key: Advance one address byte
 ' Insert Key   : Insert a byte at current address
 ' Delete key   : Delete a byte at current address
 ' Home Key     : New address input
 ' End Key      : New data input
 ' Shift+F5     : Run at Address
 ' Shift+F9     : Save Program
 ' Shift+F10    : Load Program
 ' Shift+Esc    : Quit
 Null$ = CHR$(0)

 DO
 DetermineKey$ = INKEY$
 IF DetermineKey$ = Null$ + "O" THEN ColourKeyPressed 21: DataInput
 IF DetermineKey$ = Null$ + "G" THEN ColourKeyPressed 18: NewAddress
 IF DetermineKey$ = Null$ + "X" THEN ColourKeyPressed 24: RunABC Address#
 IF DetermineKey$ = Null$ + "I" THEN
    Address# = Address# - 1
    DisplayTable Address#
    ColourKeyPressed 19
 END IF
 IF DetermineKey$ = Null$ + "Q" THEN
    Address# = Address# + 1
    DisplayTable Address#
    ColourKeyPressed 22
    END IF
 IF DetermineKey$ = CHR$(27) THEN CLS : PRINT " Programmed by Charles Chiou": SYSTEM
 LOOP

 SUB ABCpattern

 LOCATE 11, 57: PRINT "ษออออหออออหออออป"
 LOCATE 12, 57: PRINT "บINS บADDsบ ฤ  บ"
 LOCATE 13, 57: PRINT "ฬออออฮออออฮออออน"
 LOCATE 14, 57: PRINT "บDEL บDataบ +  บ"
 LOCATE 15, 57: PRINT "ศออออสออออสออออผ"
 LOCATE 16, 57: PRINT "ษอออออออหออออออป"
 LOCATE 17, 57: PRINT "บ ERROR บ  RUN บ"
 LOCATE 18, 57: PRINT "ศอออออออสออออออผ"
 LOCATE 21, 8
 PRINT "ษอออหอออหอออหอออหอออหอออหอออหอออหอออหอออหอออหอออหอออหอออหอออหอออป"
 LOCATE 22, 8
 PRINT "บ O บ 1 บ 2 บ 3 บ 4 บ 5 บ 6 บ 7 บ 8 บ 9 บ A บ B บ C บ D บ E บ F บ"
 LOCATE 23, 8
 PRINT "ศอออสอออสอออสอออสอออสอออสอออสอออสอออสอออสอออสอออสอออสอออสอออสอออผ"

 COLOR 15
 LOCATE 24, 8: PRINT "Shift+F5 = Run, Shift+F9 = Save, Shift+F10 = Load, Shift+Esc = Quit"

 COLOR 13
 LOCATE 1, 10: PRINT " ÛÛÛÛÛÛ   ÛÛÛÛÛÜ    ÜÛÛÛÛÜ          ÜÛÛÛÛÜ     ÜÛÛÛÛÜ "
 LOCATE 2, 10: PRINT "ÛÛÛ  ÛÛÛ  ÛÛ   ÛÛ  ÛÛ    ÛÛ        ÛÛ    ÛÛ   ÛÛ    ÛÛ"
 LOCATE 3, 10: PRINT "ÛÛÛÛÛÛÛÛ  ÛÛÛÛÛÛ   ÛÛ        ÛÛÛ    ÞÛÛÛÛÝ    ÛÛ    ÛÛ"
 LOCATE 4, 10: PRINT "ÛÛÛ  ÛÛÛ  ÛÛ   ÛÛ  ÛÛ    ÛÛ        ÛÛ    ÛÛ   ÛÛ    ÛÛ"
 LOCATE 5, 10: PRINT "ÛÛÛ  ÛÛÛ  ÛÛÛÛÛ฿    ฿ÛÛÛÛ฿          ฿ÛÛÛÛ฿     ฿ÛÛÛÛ฿ "
 COLOR 12
 LOCATE 25, 10: PRINT "PC/AT <ฤ> ABC-80  LINK via 8255... Designed By  Charles Chiou"
 COLOR 7


 END SUB

 SUB ColourKeyPressed (WhichKey%)

 IF WhichKey% > 0 AND WhichKey% <= 22 THEN SOUND 2000, .1
 IF WhichKey% = 23 THEN SOUND 800, 2: SOUND 200, 5
 IF WhichKey% = 24 THEN PLAY "l16mlfgfgfaba>d<"
 IF WhichKey% > 0 AND WhichKey% <= 16 THEN
  COLOR 10
  LOCATE 22, 9 + 4 * (WhichKey% - 1): PRINT CHR$(219) + HEX$(WhichKey% - 1) + CHR$(219)
  FOR w = 1 TO 150: NEXT w
  COLOR 7
  LOCATE 22, 9 + 4 * (WhichKey% - 1): PRINT CHR$(32) + HEX$(WhichKey% - 1) + CHR$(32)
 END IF
        IF WhichKey% = 17 THEN
                COLOR 10
                LOCATE 12, 58: PRINT "INS" + CHR$(219)
                FOR w = 1 TO 150: NEXT w
                COLOR 7
                LOCATE 12, 58: PRINT "INS" + CHR$(32)
        END IF
        IF WhichKey% = 18 THEN
                COLOR 10
                LOCATE 12, 63: PRINT "ADDs"
                FOR w = 1 TO 150: NEXT w
                COLOR 7
                LOCATE 12, 63: PRINT "ADDs"
        END IF
        IF WhichKey% = 19 THEN
                COLOR 10
                LOCATE 12, 68: PRINT CHR$(219) + "ฤ" + CHR$(219) + CHR$(219)
                FOR w = 1 TO 150: NEXT w
                COLOR 7
                LOCATE 12, 68: PRINT CHR$(32) + "ฤ" + CHR$(32) + CHR$(32)
        END IF
       
        IF WhichKey% = 20 THEN
                COLOR 10
                LOCATE 14, 58: PRINT "DEL" + CHR$(219)
                FOR w = 1 TO 150: NEXT w
                COLOR 7
                LOCATE 14, 58: PRINT "DEL" + CHR$(32)
        END IF
        IF WhichKey% = 21 THEN
                COLOR 10
                LOCATE 14, 63: PRINT "DATA"
                FOR w = 1 TO 150: NEXT w
                COLOR 7
                LOCATE 14, 63: PRINT "DATA"
        END IF
        IF WhichKey% = 22 THEN
                COLOR 10
                LOCATE 14, 68: PRINT CHR$(219) + "+" + CHR$(219) + CHR$(219)
                FOR w = 1 TO 150: NEXT w
                COLOR 7
                LOCATE 14, 68: PRINT CHR$(32) + "+" + CHR$(32) + CHR$(32)
        END IF
       
       
        IF WhichKey% = 23 THEN
                COLOR 12
                LOCATE 17, 58: PRINT CHR$(219) + "ERROR" + CHR$(219)
                FOR w = 1 TO 150: NEXT w
                COLOR 7
                LOCATE 17, 58: PRINT CHR$(32) + "ERROR" + CHR$(32)
        END IF
        IF WhichKey% = 24 THEN
                COLOR 10
                LOCATE 17, 66: PRINT CHR$(219) + CHR$(219) + "RUN" + CHR$(219)
                FOR w = 1 TO 900: NEXT w
                COLOR 7
                LOCATE 17, 66: PRINT CHR$(32) + CHR$(32) + "RUN" + CHR$(32)
        END IF
                        
 END SUB

 SUB DataInput

 SHARED Address#, ByteInfo%
 Null$ = CHR$(0)
 COLOR 9
 LOCATE 8, 14: PRINT "   ": LOCATE 8, 30: PRINT "   "
 LOCATE 8, 34: PRINT "ÛÛÛ": LOCATE 8, 50: PRINT "ÛÛÛ"
 COLOR 7

 DO

 DO
   AdKey$ = INKEY$
   IF AdKey$ = "0" THEN Adval% = 0: GOTO RepeatInput
   IF AdKey$ = "1" THEN Adval% = 1: GOTO RepeatInput
   IF AdKey$ = "2" THEN Adval% = 2: GOTO RepeatInput
   IF AdKey$ = "3" THEN Adval% = 3: GOTO RepeatInput
   IF AdKey$ = "4" THEN Adval% = 4: GOTO RepeatInput
                        IF AdKey$ = "5" THEN Adval% = 5: GOTO RepeatInput
                        IF AdKey$ = "6" THEN Adval% = 6: GOTO RepeatInput
                        IF AdKey$ = "7" THEN Adval% = 7: GOTO RepeatInput
                        IF AdKey$ = "8" THEN Adval% = 8: GOTO RepeatInput
                        IF AdKey$ = "9" THEN Adval% = 9: GOTO RepeatInput
   IF AdKey$ = "A" OR AdKey$ = "a" THEN Adval% = 10: GOTO RepeatInput
   IF AdKey$ = "B" OR AdKey$ = "b" THEN Adval% = 11: GOTO RepeatInput
   IF AdKey$ = "C" OR AdKey$ = "c" THEN Adval% = 12: GOTO RepeatInput
   IF AdKey$ = "D" OR AdKey$ = "d" THEN Adval% = 13: GOTO RepeatInput
  
   IF AdKey$ = "E" OR AdKey$ = "e" THEN Adval% = 14: GOTO RepeatInput
   IF AdKey$ = "F" OR AdKey$ = "f" THEN Adval% = 15: GOTO RepeatInput
   IF AdKey$ = Null$ + "O" THEN ColourKeyPressed 21: GOTO OtherTask
                IF AdKey$ = Null$ + "I" THEN
                Address# = Address# - 1
                DisplayTable Address#
                ColourKeyPressed 19
                GOTO OtherTask
                END IF
                IF AdKey$ = Null$ + "Q" THEN
                Address# = Address# + 1
                DisplayTable Address#
                ColourKeyPressed 22
                GOTO OtherTask
                END IF
   IF AdKey$ = Null$ + "R" THEN Insert Address#: GOTO OtherTask
   IF AdKey$ = Null$ + "S" THEN Delete Address#: GOTO OtherTask
   IF AdKey$ = Null$ + "X" THEN GOTO OutFromData
   IF AdKey$ = Null$ + "G" THEN GOTO OutFromData
   IF AdKey$ = CHR$(27) THEN CLS : PRINT " Programmed by Charles Chiou": SYSTEM
   IF AdKey$ <> "" THEN ColourKeyPressed 23
   LOOP
                        
RepeatInput:
   ' showing the key pressed
   ColourKeyPressed Adval% + 1
   ' converting new data by a formula
   ' shift bits to left and replace lowest bit with Adval%
   InABC Address#
   ByteInfo% = (ByteInfo% - (INT(ByteInfo% / &H10) * &H10)) * &H10 + Adval%
   OutInfo% = ByteInfo%
   OutABC Address#, ByteInfo%
   DisplayTable Address#
   InABC Address#
   IF OutInfo% <> ByteInfo% THEN ColourKeyPressed 23
OtherTask:
   AdKey$ = ""
   LOOP
OutFromData:
 IF AdKey$ = Null$ + "G" THEN ColourKeyPressed 21: NewAddress
 IF AdKey$ = Null$ + "X" THEN ColourKeyPressed 24: RunABC Address#
  
 END SUB

 SUB Delete (Address#)

 FirstAdByte# = INT(Address# / &H100)
 SecondAdByte# = Address# - FirstAdByte# * &H100
 Record1% = FirstAdByte#
 Record2% = SecondAdByte#

 IF Address# >= &H1000 AND Address# < &H1700 THEN
 OutABC &H178A, Record2%
 OutABC &H178B, Record1%
 OUT &H304, &H3
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, &H40
 DO WHILE INP(&H305) <> &HAA: LOOP
 OUT &H304, &H17
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, &H0

 END IF

 IF Address# >= &H3000 AND Address# < &H3800 THEN
 OutABC &H178A, Record2%
 OutABC &H178B, Record1%
 OUT &H304, &H3
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, &H60
 DO WHILE INP(&H305) <> &HAA: LOOP
 OUT &H304, &H17
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, &H0
 END IF
 ColourKeyPressed 20

 IF (Address# < &H3000 OR Address# >= &H3800) AND (Address# < &H1000 OR Address# >= &H1700) THEN ColourKeyPressed 23
                   
 END SUB

 SUB DisplayTable (Address#)

 SHARED ByteInfo%, ByteDisplay()

 FOR a = 1 TO 7
 InABC Address# - 4 + a: ByteDisplay(a) = ByteInfo%
 NEXT a

 FOR a = 1 TO 7
 COLOR 7
 CurrentAdd$ = HEX$(Address# - 4 + a)
 IF LEN(CurrentAdd$) = 4 THEN
                              BufferAdd1$ = MID$(CurrentAdd$, 1, 1)
                              BufferAdd2$ = MID$(CurrentAdd$, 2, 1)
                              BufferAdd3$ = MID$(CurrentAdd$, 3, 1)
                              BufferAdd4$ = MID$(CurrentAdd$, 4, 1)
                                                                END IF
 IF LEN(CurrentAdd$) = 3 THEN
                              BufferAdd1$ = "0"
                              BufferAdd2$ = MID$(CurrentAdd$, 1, 1)
                              BufferAdd3$ = MID$(CurrentAdd$, 2, 1)
                              BufferAdd4$ = MID$(CurrentAdd$, 3, 1)
                                                                END IF
 IF LEN(CurrentAdd$) = 2 THEN
                              BufferAdd1$ = "0"
                              BufferAdd2$ = "0"
                              BufferAdd3$ = MID$(CurrentAdd$, 1, 1)
                              BufferAdd4$ = MID$(CurrentAdd$, 2, 1)
                                                                END IF
 IF LEN(CurrentAdd$) = 1 THEN
                              BufferAdd1$ = "0"
                              BufferAdd2$ = "0"
                              BufferAdd3$ = "0"
                              BufferAdd4$ = MID$(CurrentAdd$, 1, 1)
                                                                END IF
 IF a = 4 THEN COLOR 14
 LOCATE 9 + a, 18: PRINT BufferAdd1$; " "; BufferAdd2$; " ";
                   PRINT BufferAdd3$; " "; BufferAdd4$
 CurrentByte$ = HEX$(ByteDisplay(a))
 IF LEN(CurrentByte$) = 1 THEN
      LOCATE 9 + a, 40: PRINT "0";
 ELSE LOCATE 9 + a, 40: PRINT "";
 END IF
 PRINT HEX$(ByteDisplay(a))
 NEXT a

 END SUB

 SUB HexIn
 
 SHARED Adval%
  DO
   AdKey$ = INKEY$
   IF AdKey$ = "0" THEN Adval% = 0: GOTO ByeFromHexIn
   IF AdKey$ = "1" THEN Adval% = 1: GOTO ByeFromHexIn
   IF AdKey$ = "2" THEN Adval% = 2: GOTO ByeFromHexIn
   IF AdKey$ = "3" THEN Adval% = 3: GOTO ByeFromHexIn
   IF AdKey$ = "4" THEN Adval% = 4: GOTO ByeFromHexIn
                        IF AdKey$ = "5" THEN Adval% = 5: GOTO ByeFromHexIn
                        IF AdKey$ = "6" THEN Adval% = 6: GOTO ByeFromHexIn
                        IF AdKey$ = "7" THEN Adval% = 7: GOTO ByeFromHexIn
                        IF AdKey$ = "8" THEN Adval% = 8: GOTO ByeFromHexIn
                        IF AdKey$ = "9" THEN Adval% = 9: GOTO ByeFromHexIn
   IF AdKey$ = "A" OR AdKey$ = "a" THEN Adval% = 10: GOTO ByeFromHexIn
   IF AdKey$ = "B" OR AdKey$ = "b" THEN Adval% = 11: GOTO ByeFromHexIn
   IF AdKey$ = "C" OR AdKey$ = "c" THEN Adval% = 12: GOTO ByeFromHexIn
   IF AdKey$ = "D" OR AdKey$ = "d" THEN Adval% = 13: GOTO ByeFromHexIn
   IF AdKey$ = "E" OR AdKey$ = "e" THEN Adval% = 14: GOTO ByeFromHexIn
   IF AdKey$ = "F" OR AdKey$ = "f" THEN Adval% = 15: GOTO ByeFromHexIn
 LOOP
ByeFromHexIn:

 END SUB

 SUB InABC (Address#)

 SHARED ByteInfo%

 FirstAdByte# = INT(Address# / &H100)
 SecondAdByte# = Address# - FirstAdByte# * &H100

 FOR w = 1 TO 30: NEXT w        ' IMPORTANT!! w determines the delay time
 OUT &H304, &H2
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, SecondAdByte#
 DO WHILE INP(&H305) <> &HAA: LOOP
 OUT &H304, FirstAdByte#
 FOR w = 1 TO 30: NEXT w        ' IMPORTANT!! w determines the delay time
 ByteInfo% = INP(&H305)
 OUT &H304, &H0

 END SUB

 SUB InitialABC

 SHARED ByteInfo%

 ' set up working mode for 8255 (&h304 - %h307)
 OUT &H307, &H82
 FOR w = 1 TO 1000: NEXT w
 OUT &H304, &H0

 ' checking read/write OK at adress &h1000 and &h3000
 OutABC &H1000, &H0
 OutABC &H1000, &HFF
 InABC &H1000
 IF ByteInfo% <> &HFF THEN PRINT "RAM fail at 1000h...": PRINT ByteInfo%: SYSTEM

 OutABC &H3000, &H0
 OutABC &H3000, &HFF
 InABC &H3000
 IF ByteInfo% <> &HFF THEN PRINT "RAM fail at 1000h...": SYSTEM

 PLAY "l32mlabcdfg"
 PRINT " ABC-80 Connectted successfully..."
 FOR w = 1 TO &HF00: NEXT w

 ' Out for Insert Program
       ' LD BC, 16FEh                   01 FE 16
       ' LD A, (BC)         INSERT      0A
       ' INC BC                         03
       ' LD (BC), A                     02
       ' DEC BC                         0B
       ' DEC BC                         0B
       ' LD HL, (178Ah)                 2A 8A 17
       ' SBC HL, BC                     ED 42
       ' JR NZ,INSERT                   20 F4
       ' INC BC                         03
       ' LD A, 00h                      3E 00
       ' LD (BC), A                     02
       ' LD A, CCh                      3E CC
       ' OUT C0h, A                     D3 C0
       ' JP BEGIN                       C3 00 0B

  InsertProgOut1$ = "01FE160A03020B0B2A8A17ED4220F4033E00023ECCD3C0C3000B"
  InsertProgOut2$ = "01FE370A03020B0B2A8A17ED4220F4033E00023ECCD3C0C3000B"

 Address# = &H1700
 FOR ProgOut = 1 TO 51 STEP 2
 ProgOutByte1$ = MID$(InsertProgOut1$, ProgOut, 1)
 ProgOutByte2$ = MID$(InsertProgOut1$, ProgOut + 1, 1)
 ProgOutByteVal1% = ASC(ProgOutByte1$)
 ProgOutByteVal2% = ASC(ProgOutByte2$)
  IF ProgOutByteVal1% >= 48 AND ProgOutByteVal1% <= 57 THEN
       ProgOutByteVal1% = ProgOutByteVal1% - 48
  ELSE ProgOutByteVal1% = ProgOutByteVal1% - 55
  END IF
  IF ProgOutByteVal2% >= 48 AND ProgOutByteVal2% <= 57 THEN
       ProgOutByteVal2% = ProgOutByteVal2% - 48
  ELSE ProgOutByteVal2% = ProgOutByteVal2% - 55
  END IF
 OutByteVal% = ProgOutByteVal1% * &H10 + ProgOutByteVal2%
 OutABC Address#, OutByteVal%
 Address# = Address# + 1
 NEXT ProgOut

 Address# = &H1720
 FOR ProgOut = 1 TO 51 STEP 2
 ProgOutByte1$ = MID$(InsertProgOut2$, ProgOut, 1)
 ProgOutByte2$ = MID$(InsertProgOut2$, ProgOut + 1, 1)
 ProgOutByteVal1% = ASC(ProgOutByte1$)
 ProgOutByteVal2% = ASC(ProgOutByte2$)
  IF ProgOutByteVal1% >= 48 AND ProgOutByteVal1% <= 57 THEN
       ProgOutByteVal1% = ProgOutByteVal1% - 48
  ELSE ProgOutByteVal1% = ProgOutByteVal1% - 55
  END IF
  IF ProgOutByteVal2% >= 48 AND ProgOutByteVal2% <= 57 THEN
       ProgOutByteVal2% = ProgOutByteVal2% - 48
  ELSE ProgOutByteVal2% = ProgOutByteVal2% - 55
  END IF
 OutByteVal% = ProgOutByteVal1% * &H10 + ProgOutByteVal2%
 OutABC Address#, OutByteVal%
 Address# = Address# + 1
 NEXT ProgOut

 ' Out for Delete Program
       ' LD BC, (178Ah)                 ED 4B 8A 17
       ' INC BC             DELETE      03
       ' LD A, (BC)                     0A
       ' DEC BC                         0B
       ' LD (BC), A                     02
       ' INC BC                         03
       ' LD HL, 16FFh                   21 FF 16
       ' SBC HL, BC                     ED 42
       ' JR NZ,INSERT                   20 F4
       ' LD A, CCh                      3E CC
       ' OUT C0h, A                     D3 C0
       ' JP BEGIN                       C3 00 0B
 
  DeleteProgOut1$ = "ED4B8A17030A0B020321FF16ED4220F43ECCD3C0C3000B"
  DeleteProgOut2$ = "ED4B8A17030A0B020321FF37ED4220F43ECCD3C0C3000B"
                                        
 Address# = &H1740
 FOR ProgOut = 1 TO 45 STEP 2
 ProgOutByte1$ = MID$(DeleteProgOut1$, ProgOut, 1)
 ProgOutByte2$ = MID$(DeleteProgOut1$, ProgOut + 1, 1)
 ProgOutByteVal1% = ASC(ProgOutByte1$)
 ProgOutByteVal2% = ASC(ProgOutByte2$)
  IF ProgOutByteVal1% >= 48 AND ProgOutByteVal1% <= 57 THEN
       ProgOutByteVal1% = ProgOutByteVal1% - 48
  ELSE ProgOutByteVal1% = ProgOutByteVal1% - 55
  END IF
  IF ProgOutByteVal2% >= 48 AND ProgOutByteVal2% <= 57 THEN
       ProgOutByteVal2% = ProgOutByteVal2% - 48
  ELSE ProgOutByteVal2% = ProgOutByteVal2% - 55
  END IF
 OutByteVal% = ProgOutByteVal1% * &H10 + ProgOutByteVal2%
 OutABC Address#, OutByteVal%
 Address# = Address# + 1
 NEXT ProgOut

 Address# = &H1760
 FOR ProgOut = 1 TO 45 STEP 2
 ProgOutByte1$ = MID$(DeleteProgOut2$, ProgOut, 1)
 ProgOutByte2$ = MID$(DeleteProgOut2$, ProgOut + 1, 1)
 ProgOutByteVal1% = ASC(ProgOutByte1$)
 ProgOutByteVal2% = ASC(ProgOutByte2$)
  IF ProgOutByteVal1% >= 48 AND ProgOutByteVal1% <= 57 THEN
       ProgOutByteVal1% = ProgOutByteVal1% - 48
  ELSE ProgOutByteVal1% = ProgOutByteVal1% - 55
  END IF
  IF ProgOutByteVal2% >= 48 AND ProgOutByteVal2% <= 57 THEN
       ProgOutByteVal2% = ProgOutByteVal2% - 48
  ELSE ProgOutByteVal2% = ProgOutByteVal2% - 55
  END IF
 OutByteVal% = ProgOutByteVal1% * &H10 + ProgOutByteVal2%
 OutABC Address#, OutByteVal%
 Address# = Address# + 1
 NEXT ProgOut


 END SUB

 SUB Insert (Address#)

 FirstAdByte# = INT(Address# / &H100)
 SecondAdByte# = Address# - FirstAdByte# * &H100
 Record1% = FirstAdByte#
 Record2% = SecondAdByte#

 IF Address# >= &H1000 AND Address# < &H1700 THEN
 OutABC &H178A, Record2%
 OutABC &H178B, Record1%
 OUT &H304, &H3
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, &H0
 DO WHILE INP(&H305) <> &HAA: LOOP
 OUT &H304, &H17
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, &H0
 
 END IF

 IF Address# >= &H3000 AND Address# < &H3800 THEN
 OutABC &H178A, Record2%
 OutABC &H178B, Record1%
 OUT &H304, &H3
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, &H20
 DO WHILE INP(&H305) <> &HAA: LOOP
 OUT &H304, &H17
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, &H0
 END IF
 ColourKeyPressed 17

 IF (Address# < &H3000 OR Address# >= &H3800) AND (Address# < &H1000 OR Address# >= &H1700) THEN ColourKeyPressed 23

 END SUB

 SUB LoadProg

  LOCATE 20, 6: LINE INPUT "* Load *  file name and path :  "; FileName$
  IF FileName$ = "" THEN GOTO ByeFromLoadProg

 OPEN FileName$ FOR INPUT AS #2
 INPUT #2, Start#
 INPUT #2, ProgIn$
 CLOSE #2

 LOCATE 20, 6: PRINT "Writing to ABC-80 starting address : "; HEX$(Start#)

    Address# = Start#
 FOR LoadProgCount# = Start# TO LEN(ProgIn$) + Start# - 1 STEP 2
     
 ProgOutByte1$ = MID$(ProgIn$, LoadProgCount# - Start# + 1, 1)
 ProgOutByte2$ = MID$(ProgIn$, LoadProgCount# - Start# + 2, 1)
 ProgOutByteVal1% = ASC(ProgOutByte1$)
 ProgOutByteVal2% = ASC(ProgOutByte2$)
  IF ProgOutByteVal1% >= 48 AND ProgOutByteVal1% <= 57 THEN
       ProgOutByteVal1% = ProgOutByteVal1% - 48
  ELSE ProgOutByteVal1% = ProgOutByteVal1% - 55
  END IF
  IF ProgOutByteVal2% >= 48 AND ProgOutByteVal2% <= 57 THEN
       ProgOutByteVal2% = ProgOutByteVal2% - 48
  ELSE ProgOutByteVal2% = ProgOutByteVal2% - 55
  END IF
 LoadToABC% = ProgOutByteVal1% * &H10 + ProgOutByteVal2%
   OutABC Address#, LoadToABC%
   Address# = Address# + 1
 NEXT LoadProgCount#

ByeFromLoadProg:
 LOCATE 19
 PRINT "                                                                        "
 LOCATE 20
 PRINT "                                                                        "

 END SUB

 SUB NewAddress

 SHARED Address#

 Null$ = CHR$(0)
 check = 0
 COLOR 9
 LOCATE 8, 34: PRINT "   ": LOCATE 8, 50: PRINT "   "
 LOCATE 8, 14: PRINT "ÛÛÛ": LOCATE 8, 30: PRINT "ÛÛÛ"
 COLOR 7

 DO
 
 DO
   AdKey$ = INKEY$
   IF AdKey$ = "0" THEN Adval% = 0: GOTO RepeatProcess
   IF AdKey$ = "1" THEN Adval% = 1: GOTO RepeatProcess
   IF AdKey$ = "2" THEN Adval% = 2: GOTO RepeatProcess
   IF AdKey$ = "3" THEN Adval% = 3: GOTO RepeatProcess
   IF AdKey$ = "4" THEN Adval% = 4: GOTO RepeatProcess
                        IF AdKey$ = "5" THEN Adval% = 5: GOTO RepeatProcess
                        IF AdKey$ = "6" THEN Adval% = 6: GOTO RepeatProcess
                        IF AdKey$ = "7" THEN Adval% = 7: GOTO RepeatProcess
                        IF AdKey$ = "8" THEN Adval% = 8: GOTO RepeatProcess
                        IF AdKey$ = "9" THEN Adval% = 9: GOTO RepeatProcess
   IF AdKey$ = "A" OR AdKey$ = "a" THEN Adval% = 10: GOTO RepeatProcess
   IF AdKey$ = "B" OR AdKey$ = "b" THEN Adval% = 11: GOTO RepeatProcess
   IF AdKey$ = "C" OR AdKey$ = "c" THEN Adval% = 12: GOTO RepeatProcess
   IF AdKey$ = "D" OR AdKey$ = "d" THEN Adval% = 13: GOTO RepeatProcess
   IF AdKey$ = "E" OR AdKey$ = "e" THEN Adval% = 14: GOTO RepeatProcess
   IF AdKey$ = "F" OR AdKey$ = "f" THEN Adval% = 15: GOTO RepeatProcess
   IF AdKey$ = Null$ + "O" THEN GOTO TransferToData
   IF AdKey$ = Null$ + "X" THEN GOTO TransferToData
   IF AdKey$ = Null$ + "G" THEN ColourKeyPressed 18: GOTO PressedAddsAgain
   IF AdKey$ = Null$ + "\" THEN SaveProg: GOTO RepeatProcess
   IF AdKey$ = Null$ + "]" THEN LoadProg: GOTO RepeatProcess
   IF AdKey$ = Null$ + "I" THEN
    Address# = Address# - 1
    DisplayTable Address#
    ColourKeyPressed 19
   GOTO TransferToData
   END IF
   IF AdKey$ = Null$ + "Q" THEN
    Address# = Address# + 1
    DisplayTable Address#
    ColourKeyPressed 22
   GOTO TransferToData
   END IF
   IF AdKey$ = CHR$(27) THEN CLS : PRINT " Programmed by Charles Chiou": SYSTEM
   IF AdKey$ <> "" THEN ColourKeyPressed 23
   LOOP
                         
RepeatProcess:
   ' showing the key pressed
   ColourKeyPressed Adval% + 1
   ' converting new address by a formula
   ' shift bits to left and replace lowest bit with Adval%
   Address# = (Address# - (INT(Address# / &H1000) * &H1000)) * &H10 + Adval%
   DisplayTable Address#
PressedAddsAgain:
   LOOP
TransferToData:
 IF AdKey$ = Null$ + "O" THEN ColourKeyPressed 21: DataInput
 IF AdKey$ = Null$ + "X" THEN ColourKeyPressed 24: RunABC Address#

 END SUB

 SUB OutABC (Address#, ByteOut%)

 SHARED ByteInfo%

 FirstAdByte# = INT(Address# / 256)
 SecondAdByte# = Address# - FirstAdByte# * 256

 OUT &H304, &H1
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, SecondAdByte#
 DO WHILE INP(&H305) <> &HAA: LOOP
 OUT &H304, FirstAdByte#
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, ByteOut%
 DO WHILE INP(&H305) <> &HAA: LOOP
 OUT &H304, &H0

 END SUB

 SUB RunABC (Address#)

 SHARED ByteInfo%

 FirstAdByte# = INT(Address# / 256)
 SecondAdByte# = Address# - FirstAdByte# * 256

 OUT &H304, &H3
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, SecondAdByte#
 DO WHILE INP(&H305) <> &HAA: LOOP
 OUT &H304, FirstAdByte#
 DO WHILE INP(&H305) <> &H55: LOOP
 OUT &H304, &H0
 
 END SUB

 SUB SaveProg

 SHARED Adval%
 SHARED ByteInfo%
 LOCATE 19, 6: LINE INPUT "* Save *  file name and path :  "; FileName$
 IF FileName$ = "" THEN GOTO ByeFromSaveProg


 Start# = &H0
 Finish# = &H0
 DO WHILE Start# >= Finish#
   
    LOCATE 20, 6: PRINT "Starting Address :         Finishing Address:        "
   FOR w = 1 TO 4
   HexIn
   SOUND 1000, 1
   LOCATE 20, 25 + w: PRINT HEX$(Adval%)
     DummyVal# = Adval%
     FOR RealVal = 4 TO w + 1 STEP -1
     DummyVal# = DummyVal# * &H10
     NEXT RealVal
   Start# = Start# + DummyVal#
   NEXT w
   FOR w = 1 TO 4
   HexIn
   SOUND 1000, 1
   LOCATE 20, 52 + w: PRINT HEX$(Adval%)
     DummyVal# = Adval%
     FOR RealVal = 4 TO w + 1 STEP -1
     DummyVal# = DummyVal# * &H10
     NEXT RealVal
   Finish# = Finish# + DummyVal#
   NEXT w
 IF Start# > Finish# THEN ColourKeyPressed 23
 LOOP

 LOCATE 19
 PRINT "                                                                        "
 LOCATE 20
 PRINT "                                                                        "
                          
 OPEN FileName$ FOR OUTPUT AS #1
 WRITE #1, Start#

 LOCATE 19, 6: PRINT "Reading from ABC-80..."
   FOR SaveProgCount# = Start# TO Finish#
   InABC SaveProgCount#
   IF LEN(HEX$(ByteInfo%)) = 1 THEN
     ByteSave$ = CHR$(48) + HEX$(ByteInfo%)
   ELSE ByteSave$ = HEX$(ByteInfo%)
   END IF
   ProgOut$ = ProgOut$ + ByteSave$
   NEXT SaveProgCount#
 LOCATE 20, 6: PRINT "Saving to "; FileName$
 WRITE #1, ProgOut$
 CLOSE #1
 LOCATE 20, 40: PRINT " Press any key... "
 DO WHILE INKEY$ = "": LOOP
 ProgOut$ = ""

ByeFromSaveProg:
 LOCATE 19
 PRINT "                                                                        "
 LOCATE 20
 PRINT "                                                                        "

 END SUB

