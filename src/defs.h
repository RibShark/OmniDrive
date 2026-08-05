#ifndef __DEFS_H__
#define __DEFS_H__

#define DiscIsBD() ((mediaType & 0xC0) == 0xC0)
#define DiscIsDVD() ((mediaType & 0xC0) == 0x40)
#define DiscIsDVDRAM() ((mediaType & 0xF0) == 0x50)
#define DiscIsCD() ((mediaType & 0xC4) == 0x04)

#define DiscIsMultiLayer() (((mediaType & 0xC4) == 0x44) || ((mediaType & 0xCC) == 0xC4))
#define DiscIsTripleLayer() ((~mediaType & 0xC8) == 0)

typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;

extern BYTE mediaType;
extern DWORD BDDIOffset;
extern BYTE forceUnitAccess;
extern BYTE seekLayer;
extern BYTE isDiscPTP;
extern DWORD memoryStart;
extern DWORD discStructOffset;
extern DWORD lastSector;
extern DWORD startAddress;
extern DWORD transferLength;
extern BYTE cdb[16];
extern DWORD layer0End;
extern DWORD layer1End;
extern DWORD layer2End;
extern WORD readTimeCounter;

extern void ChangeDiscRWMode(DWORD mode);
extern DWORD SetErrorMode(DWORD discType, DWORD mask, DWORD mode);
extern DWORD ReadDiscStructMemDWORD(DWORD offset);
extern void ReturnSense(int key, int code, int qualifier);
extern void BDReadCmd();
extern DWORD ReadDiscData(DWORD discArea, BYTE sectorCount);
extern void ReadCDDA();
extern DWORD MSFtoLBA(DWORD msf);
extern void DVDReadCmd();
extern DWORD DVDCheckLayer(DWORD discType);
extern void ReadBDData(DWORD fua);
extern void SetCDType(BYTE type, DWORD subQ);
extern void ReadDVDData(DWORD fua);
extern void ReadDVDRAMData(DWORD fua);
extern DWORD SetBDCharacteristics();
extern DWORD ReadDVDTOC(DWORD unk);
extern DWORD ReadFromDVDSector(DWORD block, DWORD frame, DWORD offset);
extern DWORD DVDMinusReadCheck();
extern void CopySectorToDiscStructMem(DWORD dest, WORD sectorNum);

extern DWORD TocOffset;
extern BYTE* DVDCharacteristicsPatchPtr;
extern BYTE* DVDTOCReadPatchPtr;

#endif
