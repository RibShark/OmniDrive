.arm.little
.thumb

.include "version.asm"

.Open "../firmware/ASUS_BW-16D1HT_3.02.bin","../patched_firmware/ASUS_BW-16D1HT_3.02_OmniDrive-" + VersionMajor + "." + VersionMinor + "." + VersionPatch + ".bin",0

; Free Space
.definedatalabel GdFreeSpaceStart,0x156900
.definedatalabel GdFreeSpaceEnd,0x15C000
.definedatalabel FreeSpaceStart,0x1B5000
.definedatalabel FreeSpaceEnd,0x1B7FFF

; Command Table
.definedatalabel CommandTableDVDRead1,0x152E14
.definedatalabel CommandTableDVDRead2,0x152E1C
.definedatalabel CommandTableDVDRead3,0x152E24
.definedataLabel CommandTableBDRead1,0x15308C
.definedataLabel CommandTableBDRead2,0x153094
.definedatalabel CommandTableEnd,0x152CE8 // 00 03 00 00 00 00 00 00

; Data
.definedatalabel StockTimeBelow,0x069775
.definedatalabel StockSeekRecover,0x06B1D1
.definedatalabel StockSeekStep,0x06DC01
.definedatalabel seekRecoverReturn,0x01FA26D1
.definedatalabel seekLoopReturn,0x01FA272B
.definedatalabel timeBelowVeneer,0x01FEE30C
.definedatalabel seekRecoverVeneer,0x01FEED8C
.definedatalabel seekStepVeneer,0x01FEEF1C
.definedatalabel trackingFlags,0x01FF9DFD
.definedatalabel mediaType,0x01FF9DFE
.definedatalabel readState,0x01FF9E34
.definedatalabel jumpClock,0x01FF9E70
.definedatalabel dataPllPreset,0x01FFA0FC
.definedatalabel BDDIOffset,0x01FFA37C
.definedatalabel hdMinuteMap,0x01FFA5B9
.definedatalabel seekTargetMsf,0x01FFA83C
.definedatalabel coilCounter,0x01FFB240
.definedatalabel pauseRequests,0x01FFBCFC
.definedatalabel decoderFlags,0x01FFBD0A
.definedatalabel hitTestMode,0x01FFBDC1
.definedatalabel hitTestStart,0x01FFBDC4
.definedatalabel forceUnitAccess,0x01FFBDCD
.definedatalabel refillDone,0x01FFBDCF
.definedatalabel seekLayer,0x01FFBDDD
.definedatalabel isDiscPTP,0x01FFBDE3
.definedatalabel transferCount,0x01FFBE30
.definedatalabel playerStatus,0x02000B00
.definedatalabel spindleMode,0x02000B01
.definedatalabel ejectPending,0x02000B04
.definedatalabel driveSpeed,0x02000B0A
.definedatalabel driveSpeedCap,0x02000B0B
.definedatalabel pauseFlags,0x02000B10
.definedatalabel jumpFlags,0x02000B14
.definedatalabel servoFlags,0x02000B15
.definedatalabel jumpTracks,0x02000B24
.definedatalabel commandActive,0x02000BF6
.definedatalabel commandAbort,0x02000C21
.definedatalabel powerState,0x02000C23
.definedatalabel hostCommand,0x02000C26
.definedatalabel memoryStart,0x02000C34 // 0x01C08000
.definedatalabel discStructOffset,0x02000C38 // 0x321C80
.definedatalabel senseCode,0x02000C3C
.definedatalabel seekRequest,0x02000C42
.definedatalabel transferFlags,0x02000C57
.definedatalabel lastSector,0x02000C5C
.definedatalabel startAddress,0x02000C60
.definedatalabel transferLength,0x02000C6C
.definedatalabel readTimeCounter,0x02000C80
.definedatalabel transferBlock,0x02000C90
.definedatalabel transferMaxBlocks,0x02000C92
.definedatalabel bufferBlocks,0x02000C94
.definedatalabel readActive,0x02000CEE
.definedatalabel cdb,0x02000CFC
.definedatalabel layer0End,0x02000D6C
.definedatalabel layer1End,0x02000D70
.definedatalabel layer2End,0x02000D74

; Functions
.definethumblabel HostCommandClear,0x01E336
.definethumblabel DataPllRate,0x0252F6
.definethumblabel ClockDividerSelect,0x027D76
.definethumblabel AtaStatusSet,0x03B97E
.definethumblabel TransferReset,0x03BBA4
.definethumblabel HostReasonSet,0x03BD82
.definethumblabel HostStatusSend,0x03C372
.definethumblabel DecoderStart,0x03EAE6
.definethumblabel DecoderStop,0x03ECAC
.definethumblabel BufferCountSet,0x03FB8C
.definethumblabel ChangeDiscRWMode,0x0415FC // 10 B5 04 00 00 20
.definethumblabel SetErrorMode,0x04230C
.definethumblabel RfGainGet,0x059A5E
.definethumblabel RfGainSet,0x059BF6
.definethumblabel TimerWait,0x05C7C4
.definethumblabel DataPllHold,0x05FE82
.definethumblabel DataPllRelease,0x05FE9E
.definethumblabel FocusOffsetMeasure,0x065BB6
.definethumblabel TimeAbove,0x069758
.definethumblabel TimeBelow,0x069774
.definethumblabel SeekRecover,0x06B1D0
.definethumblabel SeekStep,0x06DC00
.definethumblabel ServoStateStep,0x078D68
.definethumblabel FocusSearch,0x08469A
.definethumblabel FocusOff,0x08473C
.definethumblabel TrackJump,0x08A210
.definethumblabel SpindleCavSet,0x08BAF0
.definethumblabel ReadDiscStructMemDWORD,0x0A1D14 // 09 68 12 68 89 18 (-2)
.definethumblabel PowerModeSet,0x0A27E6
.definethumblabel ReturnSense,0x0A296A // 00 04 00 23 18 43 09 02 08 43 ?? 49 10 43 08 60 70 47
.definethumblabel DriveSpeedApply,0x0A30E0
.definethumblabel BDReadCmd,0x0AE574
.definethumblabel ReadDiscData,0x0BF244 // F3 B5 C8 20
.definethumblabel ReadCDDA,0x0C89DA // F0 B5 ?? 48 8D B0 
.definethumblabel BlockToMsf,0x0C98FA
.definethumblabel MSFtoLBA,0x0C995E
.definethumblabel SledMove,0x0D8684
.definethumblabel SledServoSet,0x0E99F6
.definethumblabel DpdEqualizer,0x0F7B30
.definethumblabel TrackingOn,0x0F90E8
.definethumblabel ServoSpeedSet,0x0FA248
.definethumblabel DpdTracking,0x10BF30
.definethumblabel DVDReadCmd,0x11A61C
.definethumblabel Seek10Command,0x132F82
.definearmlabel   StockUnsignedDivide,0x13EEBC
.definethumblabel DVDCheckLayer,0x1418A8 //  64 1E 00 22 (-0xE)
.definethumblabel ReadBDData,0x14241C
.definethumblabel CacheHitTest,0x1456F8
.definethumblabel ReadCommandHandler,0x14603C
.definethumblabel SetCDType,0x14637C // 03 00 07 22 17 48 12 02 00 B5 
.definethumblabel ReadDVDData,0x147AF6 // 00 0A ? ? ? 04 C9 18 (-0x38)
.definethumblabel ReadDVDRAMData,0x149EB8 // F1 B5 00 26 00 22
.definearmlabel   SeekTargetSet,0x14B800
.definearmlabel   SetBDCharacteristics,0x14BF2C
.definearmlabel   ReadDVDTOC,0x14BFBC // branch at FF 21 01 70 28 78 (-8)
.definearmlabel   ReadFromDVDSector,0x14C414 // branch at 00 90 00 20 6B 46 D8 70 18 78 (-4)
.definearmlabel   DVDMinusReadCheck,0x14C6AC
.definearmlabel   CopySectorToDiscStructMem,0x14C714 // branch ? ? ? 04 88 42 E8 D1 (+whatever)

; Compressed Functions
.definedatalabel DVDCharacteristicsPatchAddr,0x01F96DAA
.definedatalabel DVDTOCReadPatchAddr,0x01F976C2

; Inline patches
.definedatalabel ReadSpeedPatchAddr,0x018AE6
.definedatalabel ReadCommandTrueAddr,0x018AF4
.definedatalabel ReadCommandFalseAddr,0x018BE6
.definedatalabel CDDataSpeedPatchAddr,0x01C328
.definedatalabel GdFgSpeedUpHookAddr,0x06920E
.definedatalabel GdRecoveryHookAddr,0x084960
.definedatalabel GdCavSpeedUpHookAddr1,0x08BC00
.definedatalabel GdCavSpeedUpHookAddr2,0x08BC30
.definedatalabel GdZcavHookAddr,0x08DD30
.definedatalabel SetBDCharacteristicsHookAddr,0x09E6BC
.definedatalabel ReadDVDTOCHookAddr,0x09F000
.definedatalabel ScrambleHookAddr,0x0A1990
.definedatalabel GdLoopNextAddr,0x0C8C12
.definedatalabel GdServoStepHookAddr,0x0C8C38
.definedatalabel GdSpeedApplyHookAddr,0x0C8C4A
.definedatalabel GdSpeedApplySkipAddr,0x0C8C56
.definedatalabel GdRetryTimerHookAddr,0x0C8EB2
.definedatalabel GdSeekTimerHookAddr,0x0C8ED0
.definedatalabel GdSeekRequestHookAddr,0x0C8F36
.definedatalabel GdDecoderStopHookAddr,0x0C90B2
.definedatalabel GdDecoderStopSkipAddr,0x0C915C
.definedatalabel GdSpeedChangeHookAddr,0x0C915E
.definedatalabel GdSpeedChangeSkipAddr,0x0C91BA
.definedatalabel GdSeekMsfHookAddr,0x0C92EE
.definedatalabel GdSeekMsfReturnAddr,0x0C92F2
.definedatalabel GdSeekStatusHookAddr,0x0C9304
.definedatalabel GdDecoderStartHookAddr,0x0C9344
.definedatalabel GdLoopBottomHookAddr,0x0C937E
.definedatalabel GdLoopEndAddr,0x0C9388
.definedatalabel BDIdentifierPatchAddr1,0x0E0442 // nop
.definedatalabel BDIdentifierPatchAddr2,0x0E0449 // branch
.definedatalabel BDIdentifierPatchAddr3,0x0E0522 // nop
.definedatalabel BDIdentifierPatchAddr4,0x0E055E // nop
.definedatalabel GdClvSpeedHookAddr,0x0EB526
.definedatalabel CDLeadOutPatchAddr0,0x131D28
.definedataLabel DVDLeadOutPatchAddr0,0x1418EF
.definedatalabel DVDLeadOutPatchAddr1,0x14192E
.definedatalabel DVDLeadOutPatchAddr2,0x141944
.definedatalabel DVDLeadOutPatchAddr3,0x141952
.definedatalabel DVDLeadOutPatchAddr4,0x141966
.definedatalabel DVDLeadOutPatchAddr5,0x141978
.definedatalabel DVDLeadOutPatchAddr6,0x141988
.definedatalabel BDLeadOutPatchAddr,0x142335
.definedatalabel BDScramblePatchAddr,0x142494
.definedatalabel BDScrambleHookAddr,0x142498
.definedatalabel BDEDCHookAddr,0x142F6A
.definedatalabel CDLeadOutPatchAddr1,0x145A34
.definedatalabel CDLeadOutPatchAddr2,0x145A68 // nop
.definedatalabel CDLeadOutPatchAddr3,0x145A70 // nop
.definedatalabel CDLeadOutPatchAddr4,0x145A78 // nop
.definedatalabel CDLeadOutPatchAddr5,0x145B12 // nop
.definedatalabel CDLeadOutPatchAddr6,0x145B2C // nop
.definedataLabel DVDMinusReadCheckHookAddr,0x147BB8
.definedataLabel DVDScramblePatchAddr1,0x147C2E
.definedataLabel DVDScramblePatchAddr2,0x147C38
.definedatalabel DVDScrambleHookAddr,0x147C3C
.definedatalabel DVDEDCHookAddr,0x147F8C
.definedatalabel InquiryDataPatch,0x152686

; offsets
TocOffsetValue equ 0x8D80
GdLeadInOffsetValue equ 479
GdLeadInLandingFadValue equ 675800
GdLeadInScaleValue equ 128
GdLeadInMarginValue equ 640
GdLeadInAimHighValue equ 532
GdLeadInAimMaxValue equ 700
GdReferenceLowValue equ 5
GdReferenceHighValue equ 145
GdReferenceToleranceValue equ 2
GdModelOriginValue equ 26564
GdModelScaleValue equ 6996
GdSledStepX16Value equ 150
GdLeadInPlayTicksValue equ 0x200000
GdJumpMarginValue equ 106
GdJumpRecoveriesValue equ 0
GdProgramSeekMaxValue equ 3600
GdTrackingBandHighValue equ 240
GdSpeedUpInnerValue equ 0x4F
GdClvSpeedIndexValue equ 0x6

.include "main.asm"
.include "gd_asus.asm"
.Close