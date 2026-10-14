; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0019
; Annotation: Lab19: Electronic Organ (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 19：電子琴 (pp. 154-155)
;   16-key live electronic organ: keys 0..F index into pitch table 1023h..1032h
;   and drive speaker (Port C bit 7) while the key remains depressed.
; ==============================================================================

scd_k       equ     04a6h           ; wait for debounced key press (A = key code)
blank_rom   equ     0780h           ; 6 zero bytes in Monitor ROM (blanks display)
port_a      equ     80h             ; 8255 Port A (keypad row input)
port_c      equ     82h             ; 8255 Port C (speaker bit 7 & column strobes)

            org     1000h

start:      ld      ix,blank_rom    ; [1000] [dd 21 80 07] IX -> blank display
            call    scd_k           ; [1004] [cd a6 04]    wait for key press (A = 00h..0fh)
            ld      hl,orgtab       ; [1007] [21 23 10]    HL -> 16-note pitch table
            add     a,l             ; [100a] [85]          index into table by key code
            ld      l,a             ; [100b] [6f]
            ld      a,0c0h          ; [100c] [3e c0]       pull all 6 column strobes low (PC0..5=0)
loop:       out     (port_c),a      ; [100e] [d3 82]       drive speaker & column strobes
            ld      b,(hl)          ; [1010] [46]          load half-period delay count
delay:      nop                     ; [1011] [00]          fine pitch calibration delay
            nop                     ; [1012] [00]
            nop                     ; [1013] [00]
            djnz    delay           ; [1014] [10 fb]
            xor     80h             ; [1016] [ee 80]       invert speaker bit 7
            ld      c,a             ; [1018] [4f]          save Port C state in C
            in      a,(port_a)      ; [1019] [db 80]       read keypad rows (PA0..PA5)
            or      0c0h            ; [101b] [f6 c0]       mask unused bits PA6..PA7 high
            inc     a               ; [101d] [3c]          if no key pressed, FFh + 1 = 00h (Z=1)
            ld      a,c             ; [101e] [79]          restore Port C state
            jr      z,start         ; [101f] [28 df]       key released -> wait for next key
            jr      loop            ; [1021] [18 eb]       key still held -> continue tone

orgtab:     db      0b2h, 0a8h, 96h, 85h, 78h, 70h, 64h, 59h ; [1023..102a] keys 0..7
            db      54h, 4ah, 42h, 38h, 37h, 31h, 2ch, 29h   ; [102b..1032] keys 8..F
