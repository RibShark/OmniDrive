.arm.little
.thumb

.include "version.asm"

.Open "../firmware/HL-DT-ST_BD-RE_BU40N_1.00.bin","../patched_firmware/HL-DT-ST_BD-RE_BU40N_1.00_OmniDrive-" + VersionMajor + "." + VersionMinor + "." + VersionPatch + ".bin",0

; Free Space
.definedatalabel FreeSpaceStart,0x1A6AE0
.definedatalabel GdFreeSpaceStart,0x1A7400
.definedatalabel GdFreeSpaceEnd,0x1B0000
.definedatalabel FreeSpaceEnd,0x1B001F

; Command Table
.definedatalabel CommandTableDVDRead1,0x14FFD0
.definedatalabel CommandTableDVDRead2,0x14FFD8
.definedatalabel CommandTableDVDRead3,0x14FFE0
.definedataLabel CommandTableBDRead1,0x150248
.definedataLabel CommandTableBDRead2,0x150250
.definedatalabel CommandTableEnd,0x14FEA4

; Data
.definedatalabel StockTimeBelow,0x068A9B
.definedatalabel StockSeekRecover,0x06A9C9
.definedatalabel StockSeekStep,0x06D5E5
.definedatalabel seekRecoverReturn,0x01FA6B6B
.definedatalabel seekLoopReturn,0x01FA6BC5
.definedatalabel timeBelowVeneer,0x01FE8404
.definedatalabel seekRecoverVeneer,0x01FE8EE8
.definedatalabel seekStepVeneer,0x01FE907C
.definedatalabel trackingFlags,0x01FF9E03
.definedatalabel mediaType,0x01FF9E04
.definedatalabel readState,0x01FF9E34
.definedatalabel jumpClock,0x01FF9E74
.definedatalabel dataPllPreset,0x01FFA104
.definedatalabel BDDIOffset,0x01FFA38C
.definedatalabel coilCounter,0x01FFA398
.definedatalabel hdMinuteMap,0x01FFA5D5
.definedatalabel seekTargetMsf,0x01FFA87D
.definedatalabel pauseRequests,0x01FFBD28
.definedatalabel decoderFlags,0x01FFBD37
.definedatalabel hitTestMode,0x01FFBE02
.definedatalabel hitTestStart,0x01FFBE04
.definedatalabel forceUnitAccess,0x01FFBE0D
.definedatalabel refillDone,0x01FFBE0F
.definedatalabel seekLayer,0x01FFBE1D
.definedatalabel isDiscPTP,0x01FFBE23
.definedatalabel transferCount,0x01FFBE70
.definedatalabel playerStatus,0x02000B00
.definedatalabel spindleMode,0x02000B01
.definedatalabel ejectPending,0x02000B05
.definedatalabel driveSpeed,0x02000B0B
.definedatalabel driveSpeedCap,0x02000B0C
.definedatalabel pauseFlags,0x02000B11
.definedatalabel jumpFlags,0x02000B15
.definedatalabel servoFlags,0x02000B16
.definedatalabel jumpTracks,0x02000B2C
.definedatalabel commandActive,0x02000C1B
.definedatalabel commandAbort,0x02000C65
.definedatalabel powerState,0x02000C67
.definedatalabel hostCommand,0x02000C6A
.definedatalabel memoryStart,0x02000C78 // 0x1C08000
.definedatalabel discStructOffset,0x02000C7C // 0x327C80
.definedatalabel senseCode,0x02000C80
.definedatalabel seekRequest,0x02000C86
.definedatalabel transferFlags,0x02000C9B
.definedatalabel lastSector,0x02000CA0
.definedatalabel startAddress,0x02000CA4
.definedatalabel transferLength,0x02000CB0
.definedatalabel readTimeCounter,0x02000CC4
.definedatalabel transferBlock,0x02000CD4
.definedatalabel transferMaxBlocks,0x02000CD6
.definedatalabel bufferBlocks,0x02000CD8
.definedatalabel readActive,0x02000D2A
.definedatalabel cdb,0x02000D38
.definedatalabel layer0End,0x02000DA8
.definedatalabel layer1End,0x02000DAC
.definedatalabel layer2End,0x02000DB0

; Functions
.definethumblabel HostCommandClear,0x02111A
.definethumblabel DataPllRate,0x027D64
.definethumblabel ClockDividerSelect,0x02A4FE
.definethumblabel AtaStatusSet,0x03DFAE
.definethumblabel TransferReset,0x03E1D4
.definethumblabel HostReasonSet,0x03E434
.definethumblabel HostStatusSend,0x03EA60
.definethumblabel DecoderStart,0x0411EE
.definethumblabel DecoderStop,0x0413B4
.definethumblabel BufferCountSet,0x04229C
.definethumblabel ChangeDiscRWMode,0x043CB0
.definethumblabel SetErrorMode,0x044A08
.definethumblabel RfGainGet,0x05BD7A
.definethumblabel RfGainSet,0x05BF12
.definethumblabel TimerWait,0x05EAD8
.definethumblabel DataPllHold,0x0614EE
.definethumblabel DataPllRelease,0x06150A
.definethumblabel FocusOffsetMeasure,0x0673D4
.definethumblabel TimeAbove,0x068A7E
.definethumblabel TimeBelow,0x068A9A
.definethumblabel SeekRecover,0x06A9C8
.definethumblabel SeekStep,0x06D5E4
.definethumblabel ServoStateStep,0x07771C
.definethumblabel FocusSearch,0x083E48
.definethumblabel FocusOff,0x083EEA
.definethumblabel TrackJump,0x089720
.definethumblabel SpindleCavSet,0x08AFA8
.definethumblabel ReadDiscStructMemDWORD,0x0A210C
.definethumblabel PowerModeSet,0x0A2BEA
.definethumblabel ReturnSense,0x0A2D6A
.definethumblabel DriveSpeedApply,0x0A34F0
.definethumblabel BDReadCmd,0x0AE90C
.definethumblabel ReadDiscData,0x0BF872
.definethumblabel ReadCDDA,0x0C903E
.definethumblabel BlockToMsf,0x0C9F46
.definethumblabel MSFtoLBA,0x0C9FAA
.definethumblabel SledMove,0x0D865A
.definethumblabel SledServoSet,0x0EA0FA
.definethumblabel DpdEqualizer,0x0F8822
.definethumblabel TrackingOn,0x0F9D76
.definethumblabel ServoSpeedSet,0x0FAFE8
.definethumblabel DpdTracking,0x10A1CC
.definethumblabel DVDReadCmd,0x11798C
.definethumblabel Seek10Command,0x12FDB6
.definearmlabel   StockUnsignedDivide,0x13C274
.definethumblabel DVDCheckLayer,0x13EC1C
.definethumblabel ReadBDData,0x13F7F8
.definethumblabel CacheHitTest,0x142500
.definethumblabel ReadCommandHandler,0x142E44
.definethumblabel SetCDType,0x143184
.definethumblabel ReadDVDData,0x1448EE
.definethumblabel ReadDVDRAMData,0x146DC8
.definearmlabel   SeekTargetSet,0x14870C
.definearmlabel   SetBDCharacteristics,0x148E90
.definearmlabel   ReadDVDTOC,0x148F20
.definearmlabel   ReadFromDVDSector,0x149388
.definearmlabel   DVDMinusReadCheck,0x1495F0
.definearmlabel   CopySectorToDiscStructMem,0x149660

; Compressed Functions
.definedatalabel DVDCharacteristicsPatchAddr,0x01F9A802
.definedatalabel DVDTOCReadPatchAddr,0x01F9B008

; Inline patches
.definedatalabel ReadSpeedPatchAddr,0x01BB06
.definedatalabel ReadCommandTrueAddr,0x01BB14
.definedatalabel ReadCommandFalseAddr,0x01BC06
.definedatalabel CDDataSpeedPatchAddr,0x01F348
.definedatalabel GdRecoveryHookAddr,0x0840F6
.definedatalabel GdCavSpeedUpHookAddr1,0x08B164
.definedatalabel GdCavSpeedUpHookAddr2,0x08B194
.definedatalabel GdZcavHookAddr,0x08E0A8
.definedatalabel SetBDCharacteristicsHookAddr,0x09E9BC
.definedatalabel ReadDVDTOCHookAddr,0x09F316
.definedatalabel ScrambleHookAddr,0x0A1D48
.definedatalabel GdLoopNextAddr,0x0C9264
.definedatalabel GdServoStepHookAddr,0x0C928A
.definedatalabel GdSpeedApplyHookAddr,0x0C929C
.definedatalabel GdSpeedApplySkipAddr,0x0C92A8
.definedatalabel GdRetryTimerHookAddr,0x0C9502
.definedatalabel GdSeekTimerHookAddr,0x0C9520
.definedatalabel GdSeekRequestHookAddr,0x0C9586
.definedatalabel GdDecoderStopHookAddr,0x0C9702
.definedatalabel GdDecoderStopSkipAddr,0x0C97AC
.definedatalabel GdSpeedChangeHookAddr,0x0C97AE
.definedatalabel GdSpeedChangeSkipAddr,0x0C980A
.definedatalabel GdSeekMsfHookAddr,0x0C993E
.definedatalabel GdSeekMsfReturnAddr,0x0C9942
.definedatalabel GdSeekStatusHookAddr,0x0C9954
.definedatalabel GdDecoderStartHookAddr,0x0C9994
.definedatalabel GdLoopBottomHookAddr,0x0C99CE
.definedatalabel GdLoopEndAddr,0x0C99D8
.definedatalabel GdFgSpeedUpHookAddr,0x0DB9CE
.definedatalabel BDIdentifierPatchAddr1,0x0E0D70 // nop
.definedatalabel BDIdentifierPatchAddr2,0x0E0D77 // branch
.definedatalabel BDIdentifierPatchAddr3,0x0E0E56 // nop
.definedatalabel BDIdentifierPatchAddr4,0x0E0E90 // nop
.definedatalabel GdClvSpeedHookAddr,0x0EBB8A
.definedatalabel CDLeadOutPatchAddr0,0x12EB5C
.definedataLabel DVDLeadOutPatchAddr0,0x13EC63
.definedatalabel DVDLeadOutPatchAddr1,0x13ECA2
.definedatalabel DVDLeadOutPatchAddr2,0x13ECB8
.definedatalabel DVDLeadOutPatchAddr3,0x13ECC6
.definedatalabel DVDLeadOutPatchAddr4,0x13ECDA
.definedatalabel DVDLeadOutPatchAddr5,0x13ECEC
.definedatalabel DVDLeadOutPatchAddr6,0x13ECFC
.definedatalabel BDLeadOutPatchAddr,0x13F711
.definedatalabel BDScramblePatchAddr,0x13F870
.definedatalabel BDScrambleHookAddr,0x13F874
.definedatalabel BDEDCHookAddr,0x140374
.definedatalabel CDLeadOutPatchAddr1,0x14283C
.definedatalabel CDLeadOutPatchAddr2,0x142870 // nop
.definedatalabel CDLeadOutPatchAddr3,0x142878 // nop
.definedatalabel CDLeadOutPatchAddr4,0x142880 // nop
.definedatalabel CDLeadOutPatchAddr5,0x14291A // nop
.definedatalabel CDLeadOutPatchAddr6,0x142934 // nop
.definedataLabel DVDMinusReadCheckHookAddr,0x1449B2
.definedataLabel DVDScramblePatchAddr1,0x144A28
.definedataLabel DVDScramblePatchAddr2,0x144A32
.definedatalabel DVDScrambleHookAddr,0x144A36
.definedatalabel DVDEDCHookAddr,0x144D8E
.definedatalabel InquiryDataPatch,0x14F83E

; offsets
TocOffsetValue equ 0x8D70
GdLeadInOffsetValue equ 567
GdLeadInLandingFadValue equ 675000
GdLeadInScaleValue equ 102
GdLeadInMarginValue equ 1200
GdLeadInAimHighValue equ 640
GdLeadInAimMaxValue equ 760
GdReferenceLowValue equ 20
GdReferenceHighValue equ 160
GdReferenceToleranceValue equ 5
GdModelOriginValue equ 26561
GdModelScaleValue equ 8389
GdSledStepX16Value equ 125
GdLeadInPlayTicksValue equ 0x400000
GdJumpMarginValue equ 66
GdJumpRecoveriesValue equ 2
GdProgramSeekMaxValue equ 4160
GdTrackingBandHighValue equ 260
GdSpeedUpInnerValue equ 0x4F
GdClvSpeedIndexValue equ 0x6

.include "main.asm"
.include "gd_bu40n.asm"
.Close