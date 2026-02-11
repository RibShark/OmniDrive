.arm.little
.thumb

.Open "../firmware/DE_LG BU40N_1.00.bin","../patched_firmware/DE_LG BU40N_1.00_OmniDrive.bin",0

; Free Space
.definedatalabel FreeSpaceStart,0x1A6AE0
.definedatalabel FreeSpaceEnd,0x1B001F

; Command Table
.definedatalabel CommandTableDVDRead1,0x14FFD0
.definedatalabel CommandTableDVDRead2,0x14FFD8
.definedatalabel CommandTableDVDRead3,0x14FFE0
.definedataLabel CommandTableBDRead1,0x150248
.definedataLabel CommandTableBDRead2,0x150250
.definedatalabel CommandTableEnd,0x14FEA4

; Data
.definedatalabel mediaType,0x01FF9E04
.definedatalabel BDDIOffset,0x01FFA38C
.definedatalabel forceUnitAccess,0x01FFBE0D
.definedatalabel seekLayer,0x01FFBE1D
.definedatalabel isDiscPTP,0x01FFBE23
.definedatalabel memoryStart,0x02000C78 // 0x1C08000
.definedatalabel discStructOffset,0x02000C7C // 0x327C80
.definedatalabel lastSector,0x02000CA0
.definedatalabel startAddress,0x02000CA4
.definedatalabel transferLength,0x02000CB0
.definedatalabel cdb,0x02000D38
.definedatalabel layer0End,0x02000DA8
.definedatalabel layer1End,0x02000DAC
.definedatalabel layer2End,0x02000DB0

; Functions
.definethumblabel ChangeDiscRWMode,0x043CB0
.definethumblabel SetErrorMode,0x044A08
.definethumblabel ReadDiscStructMemDWORD,0x0A210C
.definethumblabel ReturnSense,0x0A2D6A
.definethumblabel BDReadCmd,0x0AE90C
.definethumblabel ReadDiscData,0x0BF872
.definethumblabel ReadCDDA,0x0C903E
.definethumblabel MSFtoLBA,0x0C9FAA
.definethumblabel DVDReadCmd,0x11798C
.definethumblabel DVDCheckLayer,0x13EC1C
.definethumblabel ReadBDData,0x13F7F8
.definethumblabel SetCDType,0x143184
.definethumblabel ReadDVDData,0x1448EE
.definethumblabel ReadDVDRAMData,0x146DC8
.definearmlabel   SetBDCharacteristics,0x148E90
.definearmlabel   ReadDVDTOC,0x148F20
.definearmlabel   ReadFromDVDSector,0x149388
.definearmlabel   CopySectorToDiscStructMem,0x149660

; Compressed Functions
.definedatalabel DVDCharacteristicsPatchAddr,0x01F9A802

; Inline patches
.definedatalabel SetBDCharacteristicsHookAddr,0x09E9BC
.definedatalabel ReadDVDTOCHookAddr,0x09F316
.definedatalabel ScrambleHookAddr,0x0A1D48
.definedatalabel BDIdentifierPatchAddr1,0x0E0D70 // nop
.definedatalabel BDIdentifierPatchAddr2,0x0E0D77 // branch
.definedatalabel BDIdentifierPatchAddr3,0x0E0E56 // nop
.definedatalabel BDIdentifierPatchAddr4,0x0E0E90 // nop
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
;.definedatalabel CDLeadOutPatchAddr2,0x14293C
.definedatalabel CDLeadOutPatchAddr2,0x142870 // nop
.definedatalabel CDLeadOutPatchAddr3,0x142878 // nop
.definedatalabel CDLeadOutPatchAddr4,0x142880 // nop
.definedatalabel CDLeadOutPatchAddr5,0x14291A // nop
.definedatalabel CDLeadOutPatchAddr6,0x142934 // nop

.definedataLabel DVDScramblePatchAddr,0x144A32
.definedatalabel DVDScrambleHookAddr,0x144A36
.definedatalabel DVDEDCHookAddr,0x144D8E
.definedatalabel InquiryDataPatch,0x14F83E

; offsets
TocOffsetValue equ 0x8D70

.include "main.asm"
.Close