; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0012
; Annotation: Lab12: STOP/GO Display (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 12：交互顯示 例一 (pp. 136-137)
;   Alternates "  GO  " (1026h..102bh) and "-StOP-" (1020h..1025h) every 0.5s
;   (50 passes of SCD&K1 at 10 ms/pass).
; ==============================================================================

scd_k1      equ     04cdh           ; single 10 ms display multiplex pass

            org     1000h

begin:      ld      ix,1026h        ; [1000] [dd 21 26 10] IX -> "  GO  " pattern
            ld      b,32h           ; [1004] [06 32]       50 scan passes (0.50s)
loop1:      call    scd_k1          ; [1006] [cd cd 04]    refresh 6-digit display
            djnz    loop1           ; [1009] [10 fb]
            ld      ix,1020h        ; [100b] [dd 21 20 10] IX -> "-StOP-" pattern
            ld      b,32h           ; [100f] [06 32]       50 scan passes (0.50s)
loop2:      call    scd_k1          ; [1011] [cd cd 04]    refresh 6-digit display
            djnz    loop2           ; [1014] [10 fb]
            jp      begin           ; [1016] [c3 00 10]    repeat indefinitely

            defs    1020h - $, 00h
stop_pat:   db      02h, 1fh, 0a3h, 87h, 0aeh, 02h ; [1020..1025] "-StOP-"
go_pat:     db      00h, 02h, 8fh, 0adh, 02h, 00h   ; [1026..102b] " -GO- "
