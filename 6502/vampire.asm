;-------------------------------------------------------------------------------
; ATARI 2600 "Vampire Survivor-ish" Demo
; Version incorporating fixes based on debugging session.
;-------------------------------------------------------------------------------

    processor 6502

    include "vcs.h"     ; Include VCS equates (redundant if above defined, but good practice)
    include "macro.h"   ; Include standard macros (MUST BE PRESENT)

;-------------------------------------------------------------------------------
; ROM Header / Setup
;-------------------------------------------------------------------------------
    seg.u Variables     ; Uninitialized RAM segment (128 bytes total)
    org $80             ; Start user RAM variables here

PlayerX     ds 1        ; Player horizontal position
PlayerY     ds 1        ; Player vertical position (approximate)
EnemyX      ds 1        ; Enemy horizontal position
EnemyY      ds 1        ; Enemy vertical position (approximate)
EnemyActive ds 1        ; 0 = inactive/dead, 1 = active
MissileX    ds 1        ; Missile horizontal position
MissileY    ds 1        ; Missile vertical position (approximate)
MissileActive ds 1      ; 0 = inactive, 1 = active
Score       ds 1        ; Simple score counter (0-255)
Temp1       ds 1        ; General purpose temporary variable for input
Temp2       ds 1        ; General purpose temporary variable
FrameCnt    ds 1        ; Frame counter for timing

; --- Constants ---
PLAYER_SPEED  equ 1
ENEMY_SPEED   equ 1     ; Enemy speed (pixels per frame approx)
MISSILE_SPEED equ 2     ; Missile speed (Reduced slightly)
SCREEN_LEFT   equ 15    ; Adjusted boundaries slightly
SCREEN_RIGHT  equ 145
SCREEN_TOP    equ 30
SCREEN_BOTTOM equ 180
ENEMY_SPAWN_DELAY equ 120 ; Frames between enemy spawns (Increased)

;-------------------------------------------------------------------------------
; CODE Segment
;-------------------------------------------------------------------------------
    seg Code
    org $F000           ; Standard 4K cartridge start address

;===============================================================================
; RESET Entry Point
;===============================================================================
Reset:
    CLEAN_START         ; Macro to clear RAM, TIA registers, set stack pointer

    ; --- Initial Game State ---
    lda #80             ; Center player horizontally (approx)
    sta PlayerX
    lda #100            ; Center player vertically
    sta PlayerY

    lda #0              ; No enemy initially
    sta EnemyActive
    sta EnemyX          ; Position doesn't matter yet
    sta EnemyY

    lda #0              ; No missile initially
    sta MissileActive
    sta MissileX        ; Position doesn't matter yet
    sta MissileY

    lda #0              ; Reset score
    sta Score
    sta FrameCnt

    ; --- Initial TIA Setup ---
    lda #%00000010      ; Playfield control: Score=off(0), Reflect=on(1), Prio=Normal(0)
    sta CTRLPF          ; Use standard CTRLPF name
    lda #0              ; Playfield graphics all off
    sta PF0
    sta PF1
    sta PF2

    lda #$0E            ; Player color (e.g., white)
    sta COLUP0
    lda #$28            ; Enemy color (e.g., red)
    sta COLUP1
    lda #$02            ; Missile/Ball color (shared with playfield)
    sta COLUPF
    lda #$00            ; Background color (Black)
    sta COLUBK

    lda #%00000000      ; *** FIX: Ensure Missile 0 graphics are OFF initially ***
    sta ENAM0           ; MissileActive flag controls actual use during frame draw
    lda #%00000000      ; Disable Missile 1
    sta ENAM1
    lda #%00000000      ; Disable Ball
    sta ENABL

    lda #%00000000      ; Player 0 size = 1x width
    sta NUSIZ0
    lda #%00000000      ; Player 1 size = 1x width
    sta NUSIZ1


;===============================================================================
; MAIN GAME LOOP Start
;===============================================================================
MainLoop:
    ; --- Vertical Blank Section ---
    lda #2              ; Signal start of VBLANK to TIA
    sta VSYNC
    sta WSYNC           ; Wait for scanline 0
    sta WSYNC
    sta WSYNC           ; 3 scanlines of VSYNC signal

    lda #0              ; End VSYNC signal
    sta VSYNC

    ; === VBLANK Tasks ===
    ; Do updates *before* waiting for VBLANK to end

    ; --- Read Input ---
ReadInput:
    lda SWCHA           ; Read Joystick 0 Port A ONCE (P1 Joystick)
    sta Temp1           ; Store it

    ; Check Up (%00010000)
    lda Temp1
    and #%00010000
    beq ReadInput_NotUp ; Branch if bit is 0 (not pressed)
    ; Move Up
    lda PlayerY
    cmp #SCREEN_TOP
    bcc ReadInput_NotUp ; Already at top, don't move further
    dec PlayerY
ReadInput_NotUp:

    ; Check Down (%00100000)
    lda Temp1
    and #%00100000
    beq ReadInput_NotDown ; Branch if bit is 0
    ; Move Down
    lda PlayerY
    cmp #SCREEN_BOTTOM
    bcs ReadInput_NotDown ; Already at bottom, don't move further
    inc PlayerY
ReadInput_NotDown:

    ; Check Left (%01000000)
    lda Temp1
    and #%01000000
    beq ReadInput_NotLeft ; Branch if bit is 0
    ; Move Left (Loop for speed > 1)
    ldx #PLAYER_SPEED
MoveLeftLoop:
    lda PlayerX
    cmp #SCREEN_LEFT
    bcc ReadInput_NotLeft ; At boundary
    dec PlayerX
    dex
    bne MoveLeftLoop
ReadInput_NotLeft:

    ; Check Right (%10000000)
    lda Temp1
    and #%10000000
    beq ReadInput_Done ; Branch if bit is 0
    ; Move Right (Loop for speed > 1)
    ldx #PLAYER_SPEED
MoveRightLoop:
    lda PlayerX
    cmp #SCREEN_RIGHT
    bcs ReadInput_Done ; At boundary
    inc PlayerX
    dex
    bne MoveRightLoop
ReadInput_Done:

    ; --- Update Enemy ---
UpdateEnemy:
    lda EnemyActive
    beq SpawnEnemyCheck ; If enemy not active, check if we should spawn one

    ; === Enemy Active Logic ===
    ; Move enemy towards player (simple version)
    ; Horizontal Movement
    lda PlayerX
    cmp EnemyX          ; Compare PlayerX and EnemyX
    beq EnemyVertMove   ; If equal, skip horizontal move

    ; Need to move right? (PlayerX > EnemyX)
    bcs EnemyMaybeRight ; If PlayerX >= EnemyX, check if needs right move
EnemyMoveLeft:          ; PlayerX < EnemyX
    ldx #ENEMY_SPEED
EnemyLeftLoop:
    lda EnemyX
    cmp #SCREEN_LEFT - 5 ; Give a little buffer
    bcc EnemyVertMove   ; Prevent going too far left
    dec EnemyX
    dex
    bne EnemyLeftLoop
    jmp EnemyVertMove   ; Go check vertical after horizontal move

EnemyMaybeRight:
    ; We know PlayerX >= EnemyX. If equal, skip to vertical.
    beq EnemyVertMove
    ; PlayerX > EnemyX, move right
    ldx #ENEMY_SPEED
EnemyRightLoop:
    lda EnemyX
    cmp #SCREEN_RIGHT + 5 ; Give a little buffer
    bcs EnemyVertMove   ; Prevent going too far right
    inc EnemyX
    dex
    bne EnemyRightLoop
    ; Fall through to EnemyVertMove

EnemyVertMove:
    ; Vertical Movement
    lda PlayerY
    cmp EnemyY          ; Compare PlayerY and EnemyY
    beq EnemyMoveDone   ; If equal, done moving

    ; Need to move down? (PlayerY > EnemyY)
    bcs EnemyMaybeDown  ; If PlayerY >= EnemyY, check if needs down move
EnemyMoveUp:            ; PlayerY < EnemyY
    lda EnemyY
    cmp #SCREEN_TOP - 5
    bcc EnemyMoveDone   ; Prevent going too far up
    dec EnemyY
    jmp EnemyMoveDone

EnemyMaybeDown:
    ; We know PlayerY >= EnemyY. If equal, skip.
    beq EnemyMoveDone
    ; PlayerY > EnemyY, move down
    lda EnemyY
    cmp #SCREEN_BOTTOM + 5
    bcs EnemyMoveDone   ; Prevent going too far down
    inc EnemyY
    ; Fall through to EnemyMoveDone

EnemyMoveDone:
    jmp UpdateMissile   ; Skip spawn check if enemy was already active

SpawnEnemyCheck:
    ; === Enemy Spawning Logic ===
    inc FrameCnt
    lda FrameCnt
    cmp #ENEMY_SPAWN_DELAY
    bcc UpdateMissile   ; Not time to spawn yet
    lda #0
    sta FrameCnt        ; Reset timer
    lda #1              ; Enemy is now active
    sta EnemyActive
    lda Score           ; Use score to alternate sides (even/odd)
    and #1
    beq SpawnLeft
SpawnRight:
    lda #SCREEN_RIGHT + 8 ; Spawn off right edge
    sta EnemyX
    jmp SetEnemyY
SpawnLeft:
    lda #SCREEN_LEFT - 8 ; Spawn off left edge
    sta EnemyX
SetEnemyY:
    ; Randomize Y slightly? For now, fixed Y.
    lda #100            ; Simple Y spawn: near middle
    sta EnemyY
    ; Initialize enemy graphics register here? No, kernel handles it.

    ; --- Update Missile ---
UpdateMissile:
    lda MissileActive
    beq FireMissileCheck ; If missile not active, check if we should fire

    ; === Active Missile Logic ===
    ; Move active missile (simple: always move right)
    ldx #MISSILE_SPEED
MoveMissileLoop:
    lda MissileX
    cmp #SCREEN_RIGHT + 15 ; Check if well off screen right
    bcs MissileOffScreen   ; If off screen, despawn it
    inc MissileX
    dex
    bne MoveMissileLoop
    jmp CollisionCheck  ; Still on screen, check collisions

MissileOffScreen:       ; Missile went off-screen
    lda #0
    sta MissileActive   ; Deactivate missile
    jmp CollisionCheck  ; Still need to check Player/Enemy collision

FireMissileCheck:
    ; === Firing Logic ===
    ; Auto-fire: if missile is inactive, fire a new one from player pos
    ; Could add button check here (e.g., lda INPT4 / and #%10000000)
    lda #1              ; Fire missile
    sta MissileActive
    lda PlayerX         ; Get player position
    clc                 ; Clear carry before adding
    adc #4              ; Start missile slightly ahead of player center? Adjust as needed.
    sta MissileX        ; Start X
    lda PlayerY
    clc
    adc #4              ; Align vertically with player center (approx for 8 line sprite)
    sta MissileY        ; Start Y (kernel uses this for vertical check)

    ; Play Fire Sound (simple tone) - Optional
    ;lda #4              ; Noise - Tonal
    ;sta AUDC0
    ;lda #8              ; Volume
    ;sta AUDV0
    ;lda #10             ; Pitch
    ;sta AUDF0

    ; --- Collision Detection ---
CollisionCheck:
    sta CXCLR           ; Reset Collision Latches

    ; Check Missile 0 vs Player 1 (Enemy)
    lda MissileActive   ; Only check if missile is active
    beq CheckPlayerCollision
    lda EnemyActive     ; Only check if enemy is active
    beq CheckPlayerCollision

    lda CXM0P           ; Read collision register M0-P1
    and #%10000000      ; Mask for M0-P1 collision bit (Bit 7)
    beq CheckPlayerCollision ; If no collision, check player collision

    ; *** Missile hit enemy! ***
    lda #0
    sta EnemyActive     ; Deactivate enemy
    sta MissileActive   ; Deactivate missile
    inc Score           ; Increase score

    ; Play Enemy Hit Sound (noise) - Optional
    ;lda #8              ; Noise - poly 4
    ;sta AUDC0
    ;lda #10             ; Volume
    ;sta AUDV0
    ;lda #5              ; Pitch/Noise type
    ;sta AUDF0

    ; Fall through to CheckPlayerCollision (even if M0P1 hit, check P0P1)

CheckPlayerCollision:
    lda EnemyActive     ; Only check if enemy is active
    beq VblankTimingWait

    lda CXPPMM          ; Read P0-P1 collision bits
    and #%10000000      ; Mask for P0-P1 collision bit (Bit 7)
    beq VblankTimingWait ; If no collision, continue

    ; *** Player hit enemy! Game Over ***
GameOver:
    ; Play Player Hit Sound (low tone) - Optional
    ;lda #4              ; Noise - Tonal
    ;sta AUDC0
    ;lda #12             ; Volume
    ;sta AUDV0
    ;lda #2              ; Pitch (low)
    ;sta AUDF0

    ; Simple Game Over: Freeze everything
GameOverLoop:
    jmp GameOverLoop    ; Infinite loop

VblankTimingWait:
    ; Wait for VBLANK timer to finish
    lda #37             ; Number of VBLANK scanlines (adjust as needed for timing)
    sta TIM64T          ; Set timer for VBLANK duration (use T64)

VBlankWaitLoop:
    lda INTIM           ; Read Timer status
    bne VBlankWaitLoop  ; Loop until timer finishes (reaches 0)

    sta WSYNC           ; Ensure we are ready for the first visible scanline

    ; Turn off sounds after a short burst (if sounds were used)
    ;lda #0
    ;sta AUDV0
    ;sta AUDV1


;===============================================================================
; KERNEL - Picture Drawing Routine
;===============================================================================
DrawFrame:
    sta WSYNC
    sta HMOVE           ; Apply RESPx values and clear motion registers for the new frame

    ; --- Set Horizontal Positions (Coarse RESP Strobe) ---
    ; Fine positioning (HMOVE) could be added here if needed

    ; Position Player 0
    lda PlayerX
    sta RESP0           ; Strobe player 0 position

    ; Position Player 1 (Enemy) - Only if active
    lda EnemyActive
    beq SkipEnemyPos
    lda EnemyX
    sta RESP1           ; Strobe player 1 position
SkipEnemyPos:

    ; Position Missile 0 - Only if active
    lda MissileActive
    beq SkipMissilePos
    lda MissileX
    sta RESM0           ; Strobe missile 0 position
SkipMissilePos:


    ; --- Visible Scanlines Loop (192 lines) ---
    ldx #192            ; Number of scanlines to draw

ScanLoop:
    ; --- Set Object Sizes --- (Can be done once before loop if static)
    ; It's often safer here to avoid glitches if values change mid-frame
    lda #%00000000      ; Player 0 size = 1x width
    sta NUSIZ0
    lda #%00000000      ; Player 1 size = 1x width
    sta NUSIZ1

    ; --- Player 0 Vertical Positioning / Graphics ---
    txa                 ; Transfer scanline counter (192..1) to A
    sec                 ; Set carry for subtraction
    sbc PlayerY         ; A = Scanline - PlayerY (approx)
    cmp #8              ; Is it within the 8 lines of the sprite height?
    bcs BlankPlayerGfx  ; If A >= 8 (unsigned), blank it (was bcc before)
LoadPlayerGfx:
    lda PlayerShape     ; Load player graphics byte from memory
    sta GRP0            ; Write to player 0 graphics register
    jmp CheckEnemyGfx
BlankPlayerGfx:
    lda #0              ; Outside range, blank graphics
    sta GRP0

CheckEnemyGfx:
    ; --- Enemy (Player 1) Vertical Positioning / Graphics ---
    lda EnemyActive
    beq SkipEnemyGfx    ; Use branch to save cycles
    txa
    sec
    sbc EnemyY
    cmp #8
    bcs BlankEnemyGfx   ; If A >= 8, blank it
LoadEnemyGfx:
    lda EnemyShape      ; Load enemy graphics byte
    sta GRP1
    jmp CheckMissileGfx
BlankEnemyGfx:
    lda #0
    sta GRP1
    jmp CheckMissileGfx ; Still need to check missile even if enemy blanked

SkipEnemyGfx:
    lda #0              ; Ensure enemy gfx are blank if inactive
    sta GRP1            ; Write blank to GRP1 if enemy inactive


CheckMissileGfx:
    ; --- Missile 0 Vertical Positioning / Graphics ---
    lda MissileActive
    beq SkipMissileGfx  ; Use branch
    txa
    sec
    sbc MissileY
    cmp #2              ; Check if near the missile's Y (allow a small range, e.g., 0 or 1)
    bcs BlankMissileGfx ; If A >= 2, disable missile gfx for this line
EnableMissile:
    lda #%00001000      ; Enable Missile 0 graphics hardware
    sta ENAM0
    jmp ScanLoopDecrement
BlankMissileGfx:
    lda #0              ; Disable missile graphics if not on the right scanline
    sta ENAM0
    jmp ScanLoopDecrement

SkipMissileGfx:
    lda #0
    sta ENAM0           ; Ensure missile GFX hardware is disabled if missile inactive


ScanLoopDecrement:
    sta WSYNC           ; Wait for next scanline (critical placement!)
    dex                 ; Decrement scanline counter
    bne ScanLoop        ; Loop until all scanlines are drawn


    ; --- Overscan Period ---
    lda #2              ; Signal Start of VBLANK to TIA for Overscan
    sta VBLANK

    ; Clear graphics during overscan
    lda #0
    sta GRP0
    sta GRP1
    sta ENAM0

    ldx #30             ; Number of overscan lines

OverScanLoop:
    sta WSYNC           ; Wait for next scanline
    dex
    bne OverScanLoop

    lda #0              ; End VBLANK signal
    sta VBLANK

    ; --- End of Frame ---
    jmp MainLoop        ; Go back to VBLANK for the next frame


;===============================================================================
; DATA
;===============================================================================
PlayerShape:
    ;.byte %00111100 ; ..XXXX.. (Original)
    .byte %00011000 ; ..##.... (Simpler block shape, 2 pixels wide)

EnemyShape:
    .byte %10000010 ; X......X (Two vertical dots/lines when drawn repeatedly)


;===============================================================================
; ROM Footer / Interrupt Vectors
;===============================================================================
    org $FFFC
    .word Reset         ; NMI Vector point to Reset
    .word Reset         ; RESET Vector point to Reset
    .word Reset         ; IRQ Vector point to Reset (BRK instruction)