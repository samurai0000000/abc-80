; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0007
; Annotation: Lab07: Max & Min Search (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 7：找尋最大值、最小值 例二 (pp. 120-122)
;   Scans 5 bytes at 1052h..1056h (69h, 39h, 96h, 9ah, 47h; count 05h at 1051h)
;   and stores the maximum (9ah) at 1050h.
; Enhancement: Sweeps each array element on the 6-digit display (1052 69 ..
;   1056 47) and highlights the winning maximum (1050 9A) with speaker ticks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
adrsdp      equ     0504h           ; DE -> left 4 address digits
dadp        equ     0511h           ; A  -> right 2 data digits
dispbf      equ     17c6h           ; system 6-digit display buffer

            org     1000h

start:      ; --- Book Example 2: Find Maximum in Array (1051h..1056h) ---
            ld      hl,1051h        ; [1000] [21 51 10] HL -> count (05h)
            ld      b,(hl)          ; [1003] [46]       B = 05h
            sub     a               ; [1004] [97]       A = 00h (initial max candidate)
next_max:   inc     hl              ; [1005] [23]       advance to next element
            cp      (hl)            ; [1006] [be]       compare current max A with (HL)
            jr      nc,decnt        ; [1007] [30 01]    if A >= (HL), keep A
            ld      a,(hl)          ; [1009] [7e]       else update max A = (HL)
decnt:      djnz    next_max        ; [100a] [10 f9]    loop over all 5 elements
            ld      (1050h),a       ; [100c] [32 50 10] store max (9Ah) at 1050h

            ; --- Lively 7-Segment & Speaker Array Inspection Loop ---
            ld      ix,dispbf       ; [100f] [dd 21 c6 17]
            ld      de,1052h        ; [1013] [11 52 10] DE -> first array element
            ld      c,05h           ; [1016] [0e 05]    5 elements to display
arr_lp:     ld      a,(de)          ; [1018] [1a]
            ld      b,18h           ; [1019] [06 18]    24 scan passes (~0.24s)
            call    show            ; [101b] [cd 2c 10]
            inc     de              ; [101e] [13]
            dec     c               ; [101f] [0d]
            jr      nz,arr_lp       ; [1020] [20 f6]

            ; --- Highlight Winner: 1050 9A ---
            ld      de,1050h        ; [1022] [11 50 10]
            ld      a,(de)          ; [1025] [1a]       A = 9Ah
            call    ms3k17          ; [1026] [cd 64 04] extra speaker chime on max
            ld      b,38h           ; [1029] [06 38]    hold winner for 56 passes (~0.56s)
            call    show            ; [102b] [cd 2c 10]
            jr      start           ; [102e] [18 d0]

show:       push    bc              ; [1030] [c5]
            call    dadp            ; [1031] [cd 11 05]
            call    adrsdp          ; [1034] [cd 04 05]
            call    ms3k17          ; [1037] [cd 64 04]
            pop     bc              ; [103a] [c1]
scan:       call    scd_k1          ; [103b] [cd cd 04]
            djnz    scan            ; [103e] [10 fb]
            ret                     ; [1040] [c9]

            defs    1050h - $, 00h
            db      00h             ; [1050] destination for maximum (becomes 9Ah)
            db      05h             ; [1051] array length = 5
            db      69h, 39h, 96h, 9ah, 47h ; [1052..1056] book candidate table
