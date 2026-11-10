; gd_asus.asm
;
; The hooks of the high-density read in the stock code, the C0 entry, and the
; numbers of the firmware. No hook trusts a RAM flag alone: GdRam holds random
; bytes before the first C0 command. A hook in ReadCDDA keeps only the registers
; that ReadCDDA reads after it (ReadCDDA never uses r12).

.definelabel GdSpeedUpByte, 0x01F22800 + 4          ; GdRam + 4: the FG speed-up of the high-density read

; set Z when the CDB mirror holds C0 03; reg and tmp change
.macro GD_TEST_HD,reg,tmp
    ldr  reg,=cdb
    ldrb tmp,[reg]
    cmp  tmp,#0xC0
    bne  @@End
    ldrb tmp,[reg,#0x1]
    mov  reg,#0x03
    and  tmp,reg
    cmp  tmp,#0x03
@@End:
.endmacro

.org GdFreeSpaceStart
.area GdFreeSpaceEnd - GdFreeSpaceStart

.importobj "../build/libOmniDriveGD.a"
.align 4

; our FG speed-up in the stock ZCAV set-up (0: GdSpeedUpInnerValue)
GdHookZcav:
    ldrb r2,[r2,r6]
    push {r1,r3}
    GD_TEST_HD r1,r3
    bne  @@Done
    ldr  r1,=GdSpeedUpByte
    ldrb r2,[r1]
    cmp  r2,#0x0
    bne  @@Done
    mov  r2,#GdSpeedUpInnerValue
@@Done:
    pop  {r1,r3}
    orr  r0,r2
    bx   lr

; our FG speed-up in the stock CAV setter
GdHookCavSpeedUp:
    push {lr}
    bl   GdSpeedUpBits
    str  r0,[r5,#0x38]
    pop  {pc}

; our FG speed-up in the stock FG writer: r2 | r3 to [r1, #0x38]; r0 and r1 stay
GdHookFgSpeedUp:
    push {r0,r1,lr}
    mov  r0,r2
    mov  r1,r3
    bl   GdSpeedUpBits
    mov  r2,r0
    pop  {r0,r1}
    str  r2,[r1,#0x38]
    pop  {pc}

; r0 | r1, with our FG speed-up in bits 8..0 in a C0 03 read
GdSpeedUpBits:
    orr  r0,r1
    push {r1,r2}
    GD_TEST_HD r1,r2
    bne  @@Done
    ldr  r1,=GdSpeedUpByte
    ldrb r2,[r1]
    cmp  r2,#0x0
    bne  @@Set
    mov  r2,#GdSpeedUpInnerValue
@@Set:
    lsr  r0,r0,#0x9
    lsl  r0,r0,#0x9
    orr  r0,r2
@@Done:
    pop  {r1,r2}
    bx   lr

; the speed index GdClvSpeedIndexValue for the stock CLV set-up in a C0 03 read
GdHookClvSpeed:
    push {r2,r3}
    GD_TEST_HD r2,r3
    bne  @@Done
    mov  r0,#GdClvSpeedIndexValue
@@Done:
    pop  {r2,r3}
    mov  r4,r0
    ldrb r0,[r6]
    bx   lr

; in a C0 03 read, no servo set-up in the stock error recovery: playerStatus 0, r0 = 0x0B
GdHookRecovery:
    push {r1,r2}
    GD_TEST_HD r1,r2
    pop  {r1,r2}
    bne  @@Stock
    ldr  r0,=playerStatus
    mov  r1,#0x0
    strb r1,[r0]
    mov  r0,#0x0B
    bx   lr
@@Stock:
    ldr  r0,=ServoSpeedSet+1
    bx   r0

; the servo step of ReadCDDA, unless GdServoStepSkip skips it
GdHookServoStep:
    push {r0-r3,lr}
    bl   GdServoStepSkip
    cmp  r0,#0x0
    pop  {r0-r3}
    bne  @@Skip
    bl   ServoStateStep
    pop  {pc}
@@Skip:
    ldr  r0,=playerStatus
    ldrb r0,[r0]
    pop  {pc}

; the speed step of ReadCDDA, unless GdSpeedApplySkip skips it
GdHookSpeedApply:
    push {r0-r3,lr}
    bl   GdSpeedApplySkip
    cmp  r0,#0x0
    pop  {r0-r3}
    bne  @@Skip
    bl   DriveSpeedApply
    pop  {pc}
@@Skip:
    add  sp,#0x4
    ldr  r0,=GdSpeedApplySkipAddr+1
    bx   r0

; the retry timer of ReadCDDA, through GdRetryTimerHook
GdHookRetryTimer:
    push {lr}
    bl   TimeAbove
    bl   GdRetryTimerHook
    pop  {pc}

; the 20 ms timer of ReadCDDA, through GdSeekTimerHook
GdHookSeekTimer:
    push {lr}
    bl   TimeAbove
    bl   GdSeekTimerHook
    pop  {pc}

; in a C0 03 read, no second seek of ReadCDDA
GdHookSeekRequest:
    GD_TEST_HD r2,r3
    ldr  r1,=seekRequest
    bne  @@Stock
    mov  r0,#0x0
    strb r0,[r1]
    mov  r1,#0x0
    bx   lr
@@Stock:
    ldrb r1,[r1]
    bx   lr

; the decoder stop of ReadCDDA, unless GdServoStepSkip skips it
GdHookDecoderStop:
    push {r0-r3,lr}
    bl   GdServoStepSkip
    cmp  r0,#0x0
    pop  {r0-r3}
    bne  @@Skip
    bl   DecoderStop
    pop  {pc}
@@Skip:
    add  sp,#0x4
    ldr  r1,=GdDecoderStopSkipAddr+1
    bx   r1

; the speed change of ReadCDDA, unless GdSpeedChangeSkip skips it
GdHookSpeedChange:
    push {lr}
    bl   GdSpeedChangeSkip
    cmp  r0,#0x0
    bne  @@Skip
    ldr  r0,=transferCount
    ldrh r0,[r0]
    pop  {pc}
@@Skip:
    add  sp,#0x4
    ldr  r0,=GdSpeedChangeSkipAddr+1
    bx   r0

; in a C0 03 read, no seek to the MSF of the next block
GdHookSeekMsf:
    push {r0-r2}
    GD_TEST_HD r1,r2
    pop  {r0-r2}
    beq  @@Skip
    blx  SeekTargetSet
    ldr  r1,=GdSeekMsfReturnAddr+1
    bx   r1
@@Skip:
    bx   lr

; the decoder start of ReadCDDA, only when GdDecoderStartHook asks for it
GdHookDecoderStart:
    push {r0,lr}
    bl   GdDecoderStartHook
    cmp  r0,#0x0
    pop  {r0}
    beq  @@Done
    mov  r1,#0x3
    bl   DecoderStart
@@Done:
    pop  {pc}

; GdLoopBottom, then the next pass or the end of the read loop
GdHookLoopBottom:
    bl   GdLoopBottom
    cmp  r0,#0x0
    beq  @@End
    ldr  r0,=GdLoopNextAddr+1
    bx   r0
@@End:
    ldr  r0,=GdLoopEndAddr+1
    bx   r0

    .pool

; .arm: in .thumb mode a label gives the C code its address + 1
.align 4
.arm
GdLeadInOffset:         .d32 GdLeadInOffsetValue
GdLeadInLandingFad:     .d32 GdLeadInLandingFadValue
GdLeadInScale:          .d32 GdLeadInScaleValue
GdLeadInMargin:         .d32 GdLeadInMarginValue
GdLeadInAimHigh:        .d32 GdLeadInAimHighValue
GdLeadInAimMax:         .d32 GdLeadInAimMaxValue
GdReferenceLow:         .d32 GdReferenceLowValue
GdReferenceHigh:        .d32 GdReferenceHighValue
GdReferenceTolerance:   .d32 GdReferenceToleranceValue
GdModelOrigin:          .d32 GdModelOriginValue
GdModelScale:           .d32 GdModelScaleValue
GdSledStepX16:          .d32 GdSledStepX16Value
GdLeadInPlayTicks:      .d32 GdLeadInPlayTicksValue
GdJumpMargin:           .d32 GdJumpMarginValue
GdJumpRecoveries:       .d32 GdJumpRecoveriesValue
GdProgramSeekMax:       .d32 GdProgramSeekMaxValue
GdTrackingBandHigh:     .d32 GdTrackingBandHighValue
UnsignedDivide:         .d32 StockUnsignedDivide
.thumb
.endarea

.org CommandTableCustom + 4                         ; the C0 entry of the command table
    .d32 GdReadDiscRaw + 1
.org GdZcavHookAddr
    bl   GdHookZcav
.org GdCavSpeedUpHookAddr1
    bl   GdHookCavSpeedUp
.org GdCavSpeedUpHookAddr2
    bl   GdHookCavSpeedUp
.org GdFgSpeedUpHookAddr
    bl   GdHookFgSpeedUp
.org GdClvSpeedHookAddr
    bl   GdHookClvSpeed
.org GdRecoveryHookAddr
    bl   GdHookRecovery
.org GdServoStepHookAddr
    bl   GdHookServoStep
.org GdSpeedApplyHookAddr
    bl   GdHookSpeedApply
.org GdRetryTimerHookAddr
    bl   GdHookRetryTimer
.org GdSeekTimerHookAddr
    bl   GdHookSeekTimer
.org GdSeekRequestHookAddr
    bl   GdHookSeekRequest
.org GdDecoderStopHookAddr
    bl   GdHookDecoderStop
.org GdSpeedChangeHookAddr
    bl   GdHookSpeedChange
.org GdSeekMsfHookAddr
    bl   GdHookSeekMsf
.org GdSeekStatusHookAddr                           ; in place of the stock test of playerStatus; the stock branch stays
    bl   GdSeekStatusHook
    cmp  r0,#0x0
.org GdDecoderStartHookAddr
    bl   GdHookDecoderStart
    nop
.org GdLoopBottomHookAddr
    bl   GdHookLoopBottom
    nop
