; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0018
; Annotation: Lab18: Computer Music (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 18：電腦音樂 (pp. 152-153)
;   Loads IY = 1008h and calls Monitor ROM MUSIC (0411h) to play a 26-note
;   melody score terminated by 40h (continuous repeat marker).
; ==============================================================================

music       equ     0411h           ; Monitor ROM score player (IY = score ptr)

            org     1000h

start:      ld      iy,score        ; [1000] [fd 21 08 10] IY -> melody score
            call    music           ; [1004] [cd 11 04]    play melody (loops on 40h)
            halt                    ; [1007] [76]          halt if score ends with 80h

score:      db      0ch, 06h, 0ch, 02h, 0ch, 04h, 15h, 03h ; [1008..100f]
            db      13h, 01h, 11h, 10h, 13h, 06h, 13h, 02h ; [1010..1017]
            db      13h, 04h, 15h, 03h, 11h, 01h, 13h, 10h ; [1018..101f]
            db      15h, 06h, 11h, 02h, 11h, 04h, 0fh, 04h ; [1020..1027]
            db      16h, 06h, 15h, 02h, 13h, 08h, 10h, 06h ; [1028..102f]
            db      0ch, 02h, 0ch, 03h, 0fh, 01h, 10h, 03h ; [1030..1037]
            db      13h, 01h, 11h, 10h, 40h                ; [1038..103c] 40h = repeat
