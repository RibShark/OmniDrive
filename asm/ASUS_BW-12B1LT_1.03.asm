.arm.little
.thumb

.include "version.asm"

; MT1939 (LiteOn) drive: the dispatcher, checksum and hook sites differ from
; the MT1959 profiles, so the native patches are defined below instead of
; including main.asm. scripts/update_cmac.py sets native word sums, not a CMAC.
OriginalFirmware equ "../firmware/ASUS_BW-12B1LT_1.03.bin"
.if filesize(OriginalFirmware) != 0x200000
    .error "Expected a decoded 2 MiB BW-12B1LT 1.03 firmware"
.endif
.if readu32(OriginalFirmware, 0x9cb76) != 0x80302060
    .error "Unexpected original INQUIRY instructions"
.endif

.Open OriginalFirmware,"../patched_firmware/ASUS_BW-12B1LT_1.03_OmniDrive-" + VersionMajor + "." + VersionMinor + "." + VersionPatch + ".bin",0

; Free Space (erased tail of the profile block, covered by the profile sum)
.definedatalabel FreeSpaceStart,0x1DEA00
.definedatalabel FreeSpaceEnd,0x1DFF00

; Data
.definedatalabel layer0End,0x02000400
.definedatalabel MtSense,0x02000602
.definedatalabel MtPacketReady,0x02000611
.definedatalabel readTimeCounter,0x02000634
.definedatalabel memoryStart,0x02000644
.definedatalabel discStructOffset,0x02000648
.definedatalabel lastSector,0x020006B8
.definedatalabel startAddress,0x020006BC
.definedatalabel transferLength,0x020006C4
.definedatalabel forceUnitAccess,0x020006FD
.definedatalabel seekLayer,0x0200071E
.definedatalabel MtDVDLayers,0x0200072A
.definedatalabel MtDVDPTP,0x0200072B
.definedatalabel isDiscPTP,0x0200072B
.definedatalabel cdb,0x02000D20
.definedatalabel mediaType,0x01FFABF0
.definedatalabel BDDIOffset,0x01FFAFDC

; MMIO
.definedatalabel SCRAMBLE_REGISTER_UNK1,0x04032048
.definedatalabel SCRAMBLE_REGISTER_UNK2,0x04050010
.definedatalabel MtCopyStatus,0x04042000
.definedatalabel DISC_SECTOR_REGISTER,0x04050064

; Functions
.definethumblabel MtNativeReadPrepare,0x000398 ; native low alias of flash 0x010398
.definethumblabel MtNativeReadAccount,0x01CA56 ; READ(10/12) counter/timestamp body
.definethumblabel MtNativeReady,0x01AFB4
.definethumblabel ChangeDiscRWMode,0x045B8A
.definethumblabel SetErrorMode,0x046918
.definethumblabel ReadDiscStructMemDWORD,0x0A646A
.definethumblabel BDReadCmd,0x0ACE90
.definethumblabel SetBDCharacteristics,0x0B0FE6
.definethumblabel MtNativeReadBDData,0x0B5828
.definethumblabel DVDReadCmd,0x0C8404
.definethumblabel MtNativeDVDPSN,0x0CB78A
.definethumblabel MtNativeDVDCharacteristics,0x0CB7E2
.definethumblabel ReadDVDTOC,0x0CC184
.definethumblabel MtNativeReadDiscData,0x0CD454
.definethumblabel MtNativeReadDVDRAMData,0x0E3370
.definethumblabel MSFtoLBA,0x0E8BA0
.definethumblabel CopySectorToDiscStructMem,0x17BAC8
.definethumblabel DVDMinusReadCheck,0x17D57C
.definethumblabel ReadCDDA,0x18AE2C
.definethumblabel MtNativeDVDCheckLayer,0x1BBC1C
.definethumblabel SetCDType,0x1BD8D8
.definethumblabel MtNativeReadDVDData,0x1BF38C

; Inline patches
.definedatalabel ReceiveReadClassifyAddr,0x0101A2 ; runs through low alias 0x1A2
.definedatalabel ReceiveReadSetupAddr,0x0101B4
.definedatalabel ReceiveReadSetupEndAddr,0x0101BE
.definedatalabel BDReadCmdHookAddr,0x01B43C
.definedatalabel DVDReadCmdHookAddr,0x01B47E
.definedatalabel DispatchHookAddr,0x01BBA4 // default arm: sense 05/20/00
.definedatalabel DispatchReturnAddr,0x01BBBE
.definedatalabel CopyTransferModeHookAddr,0x044484
.definedatalabel CopyStatusLiteral,0x044864
.definedatalabel CopyModeLiteral,0x044868
.definedatalabel SetBDCharacteristicsHookAddr,0x0A1E10
.definedatalabel ReadDVDTOCHookAddr,0x0A24D4
.definedatalabel ScrambleHookAddr,0x0A59C6
.definedatalabel BDScramblePatchAddr,0x0B5888
.definedatalabel BDScrambleHookAddr,0x0B588C
.definedatalabel BDEDCHookAddr,0x0B6668
.definedatalabel DVDCharacteristicsHookAddr,0x0CBB50 // MT1959: patch at compressed 0x01F96DAA
.definedatalabel DVDTOCReadPatchAddr,0x0CC16A // MT1959: compressed 0x01F976C2
.definedatalabel CDLeadOutPatchAddr0,0x0E4A28
.definedatalabel BDIdentifierPatchAddr1,0x133926 // nop
.definedatalabel BDIdentifierPatchAddr2,0x13392D // branch
.definedatalabel BDIdentifierPatchAddr3,0x1339D4 // nop
.definedatalabel DVDLeadOutPatchAddr0,0x1BBC63
.definedatalabel DVDLeadOutPatchAddr1,0x1BBCA0
.definedatalabel DVDLeadOutPatchAddr2,0x1BBCB2
.definedatalabel DVDLeadOutPatchAddr3,0x1BBCBE
.definedatalabel DVDLeadOutPatchAddr4,0x1BBCD0
.definedatalabel DVDLeadOutPatchAddr5,0x1BBCDE
.definedatalabel DVDLeadOutPatchAddr6,0x1BBCEE
.definedatalabel BDLeadOutPatchAddr,0x1BC453
.definedatalabel CDLeadOutPatchAddr1,0x1BD770
.definedatalabel CDLeadOutPatchAddr2,0x1BD7D4 // nop
.definedatalabel CDLeadOutPatchAddr3,0x1BD7DC // nop
.definedatalabel CDLeadOutPatchAddr4,0x1BD7E4 // nop
.definedatalabel CDLeadOutPatchAddr5,0x1BD890 // nop
.definedatalabel CDLeadOutPatchAddr6,0x1BD8AA // nop
.definedatalabel DVDMinusReadCheckHookAddr,0x1BF3BA
.definedatalabel DVDScrambleHookAddr,0x1BF410
.definedatalabel DVDRAMScrambleHookAddr,0x0E3384
.definedatalabel DVDContinuationHookAddr,0x1C05F8
.definedatalabel DVDRAMContinuationHookAddr,0x0E3D2A
.definedatalabel BDContinuationHookAddr,0x0B6B3A
.definedatalabel DVDEDCHookAddr,0x1BF5AE
.definedatalabel DVDTransferModeHookAddr,0x1BF9FE
.definedatalabel DVDTransferModeLiteral,0x1BFC84
.definedatalabel InquiryDataPatch,0x1C6F82 // standard INQUIRY byte 74

; offsets
TocOffsetValue equ 0x9F5A

; MT1939 counterpart of main.asm. The shared payload (libOmniDrive.a) and the
; platform layer (libOmniDriveMT1939.a, src/mt1939.c) go into the free
; space; everything below only branches to C or patches native instructions.
; Patches follow main.asm in order; MT1939-only hooks are marked as such.

.org FreeSpaceStart
.region (FreeSpaceEnd - FreeSpaceStart)
.importobj "../build/libOmniDrive.a"
.importobj "../build/libOmniDriveMT1939.a"

.align 4
.arm ; otherwise values are offset by 1
    TocOffset:
        .d32 TocOffsetValue
    ; Only main.c's ReadDVDTOCHook writes through these; MT1939 has no
    ; compressed code and uses Mt1939ReadDVDTOCHook, so they stay unused.
    DVDCharacteristicsPatchPtr:
        .d32 0
    DVDTOCReadPatchPtr:
        .d32 0
.thumb
.endregion

; Native packet reception clears the low half of 0x040400C4 for READs.
; Include C0 at the same point, before dispatch/readiness. Removing duplicate
; opcode loads fits the extra comparison in the original 18 bytes. Branches
; are PC-relative, so they also work when this block runs at its low alias.
.org ReceiveReadClassifyAddr
.area 18
    ldrb r0,[r4]
    cmp r0,#0x28
    beq ReceiveReadSetupAddr
    cmp r0,#0xA8
    beq ReceiveReadSetupAddr
    cmp r0,#0xBE
    beq ReceiveReadSetupAddr
    cmp r0,#0xC0
    bne ReceiveReadSetupEndAddr
.endarea

; READ DISC RAW (C0h): no command table, so use the dispatcher's default arm
.org DispatchHookAddr
.area 8
    bl Mt1939DispatchHook
    b DispatchReturnAddr
.endarea

.org InquiryDataPatch
    .ascii "OmniDrive"
    .d8 VersionMajor ; major
    .d8 VersionMinor ; minor
    .d8 VersionPatch ; patch

.org DVDReadCmdHookAddr ; READ(10), READ(12) and READ CD
    bl DVDReadCmdHook
.org BDReadCmdHookAddr ; READ(10) and READ(12)
    bl BDReadCmdHook

.org DVDCharacteristicsHookAddr
    bl Mt1939DVDCharacteristicsHook ; unknown book types use the DVD-ROM case
.org DVDTOCReadPatchAddr
    .d8 0x01 ; control data read failure returns success

.org DVDLeadOutPatchAddr0
    .d8 0xe0 ; bls -> b
.org DVDLeadOutPatchAddr1
    nop
.org DVDLeadOutPatchAddr2
    nop
.org DVDLeadOutPatchAddr3
    nop
.org DVDLeadOutPatchAddr4
    nop
.org DVDLeadOutPatchAddr5
    nop
.org DVDLeadOutPatchAddr6
    nop

.org BDLeadOutPatchAddr
    .d8 0xE0 ; bne -> b

.org CDLeadOutPatchAddr0
    .d8 0x06 ; check if gte 6 rather than 5, 5 is lead-out
.org CDLeadOutPatchAddr1
    mov r0,#0x00
.org CDLeadOutPatchAddr2
    nop
.org CDLeadOutPatchAddr3
    nop
.org CDLeadOutPatchAddr4
    nop
.org CDLeadOutPatchAddr5
    nop
.org CDLeadOutPatchAddr6
    nop

.org ScrambleHookAddr
    bl ChangeDiscRWModeHook

.org DVDMinusReadCheckHookAddr
    bl Mt1939DVDMinusReadCheckHook

.org BDScramblePatchAddr
    nop
.org BDScrambleHookAddr
    bl ChangeDiscRWModeHook

; The MT1939 DVD reader always changes the read mode (no DVDScramblePatch)
.org DVDScrambleHookAddr
    bl ChangeDiscRWModeHook
.org DVDRAMScrambleHookAddr
    bl ChangeDiscRWModeHook

; Finish a raw request before the native fast path consumes another packet.
; The following cmp/beq and native cleanup remain in place.
.org DVDContinuationHookAddr
    bl Mt1939ContinuationPendingHook
.org DVDRAMContinuationHookAddr
    bl Mt1939ContinuationPendingHook
.org BDContinuationHookAddr
    bl Mt1939ContinuationPendingHook

.org ReadDVDTOCHookAddr
    bl Mt1939ReadDVDTOCHook

.org DVDEDCHookAddr
    bl SetErrorModeHook

.org BDEDCHookAddr
    bl SetErrorModeHook

.org BDIdentifierPatchAddr1
    nop
.org BDIdentifierPatchAddr2
    .d8 0xE0 ; beq -> b
.org BDIdentifierPatchAddr3
    nop

.org SetBDCharacteristicsHookAddr
    bl Mt1939SetBDCharacteristicsHook

; MT1939 only: native code re-enables descrambling; reload the clobbered bases
.org DVDTransferModeHookAddr
.area 10
    bl Mt1939DVDTransferModeHook ; r0 is live and returned unchanged
    ldr r1,[DVDTransferModeLiteral]
    nop
    nop
.endarea
.org CopyTransferModeHookAddr
.area 14
    ldr r1,[sp,#0x28] ; copy mode, fifth argument
    bl Mt1939CopyTransferModeHook ; r0 is live and returned unchanged
    ldr r1,[CopyStatusLiteral]
    ldr r2,[CopyModeLiteral]
    nop
    nop
.endarea
.Close
