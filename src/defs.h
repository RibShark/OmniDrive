#ifndef __DEFS_H__
#define __DEFS_H__

#define DiscIsBD() (mediaType & 0xC0) == 0xC0
#define DiscIsDVD() (mediaType & 0xC0) == 0x40
#define DiscIsDVDRAM() (mediaType & 0xF0) == 0x50
#define DiscIsCD() (mediaType & 0xC4) == 0x04

#define DiscIsMultiLayer() ((mediaType & 0xC4) == 0x44) || ((mediaType & 0xCC) == 0xC4)
#define DiscIsTripleLayer() (~(mediaType & 0xC8) == 0)

typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;

extern BYTE StockTimeBelow[], StockSeekRecover[], StockSeekStep[]; 
extern BYTE seekRecoverReturn[]; 
extern BYTE seekLoopReturn[]; 
extern volatile DWORD timeBelowVeneer, seekRecoverVeneer, seekStepVeneer; 
extern volatile BYTE trackingFlags; 
extern BYTE mediaType;
extern volatile BYTE readState; 
extern volatile WORD jumpClock; 
extern volatile WORD dataPllPreset;
extern DWORD BDDIOffset;
extern volatile WORD coilCounter; 
extern volatile BYTE hdMinuteMap; 
extern volatile BYTE seekTargetMsf[3];
extern volatile BYTE pauseRequests[8]; 
extern volatile BYTE decoderFlags;
extern volatile BYTE hitTestMode;
extern volatile DWORD hitTestStart;
extern BYTE forceUnitAccess;
extern volatile BYTE refillDone;
extern BYTE seekLayer;
extern BYTE isDiscPTP;
extern volatile BYTE playerStatus; // 0: ready
extern volatile BYTE spindleMode; // bits 2..0: 2 = CAV
extern volatile BYTE ejectPending;
extern volatile BYTE driveSpeed, driveSpeedCap;
extern volatile BYTE pauseFlags; // bit 2: pause
extern volatile BYTE jumpFlags;
extern volatile BYTE servoFlags; // bit 4: the DPD method is on; bit 6: the disc was started
extern volatile WORD jumpTracks;
extern volatile BYTE commandActive;
extern volatile BYTE commandAbort;
extern volatile BYTE powerState; // bits 3..0: 0 = active, bit 1: asleep
extern volatile BYTE hostCommand[4]; // [0] 1: the host waits for this commandm, [2] a new command waits
extern DWORD memoryStart;
extern DWORD discStructOffset;
extern volatile DWORD senseCode;
extern volatile BYTE seekRequest;
extern volatile BYTE transferFlags;
extern DWORD lastSector;
extern DWORD startAddress;
extern DWORD transferLength;
extern volatile WORD transferBlock;
extern volatile BYTE transferMaxBlocks[2];
extern volatile WORD bufferBlocks;
extern volatile BYTE readActive;
extern BYTE cdb[16];
extern DWORD layer0End;
extern DWORD layer1End;
extern DWORD layer2End;
extern WORD readTimeCounter;

extern void HostCommandClear(void);
extern void DataPllRate(DWORD rate);
extern void ClockDividerSelect(DWORD divider); 
extern void AtaStatusSet(DWORD error, DWORD ready); 
extern void TransferReset(DWORD unknown);
extern void HostReasonSet(DWORD reason); // HOST_CONTROL bits 23..16
extern void HostStatusSend(void);
extern void DecoderStart(DWORD mode, DWORD unknown);
extern void DecoderStop(DWORD mode, DWORD unknown);
extern void BufferCountSet(DWORD unknown1, DWORD unknown2);
extern void ChangeDiscRWMode(DWORD mode);
extern DWORD SetErrorMode(DWORD discType, DWORD mask, DWORD mode);
extern DWORD RfGainGet(DWORD input);
extern void RfGainSet(DWORD gain, DWORD input);
extern void TimerWait(DWORD units);
extern void DataPllHold(void);
extern void DataPllRelease(void);
extern void FocusOffsetMeasure(DWORD units);
extern DWORD TimeBelow(DWORD r0, DWORD r1, DWORD r2, DWORD r3);
extern DWORD SeekRecover(DWORD r0, DWORD r1, DWORD r2, DWORD r3);
extern DWORD SeekStep(DWORD r0, DWORD r1, DWORD r2, DWORD r3); 
extern void FocusSearch(void);
extern void FocusOff(void);
extern void TrackJump(void); 
extern void SpindleCavSet(DWORD rpm);
extern DWORD ReadDiscStructMemDWORD(DWORD offset);
extern void PowerModeSet(DWORD mode);
extern void ReturnSense(int key, int code, int qualifier);
extern void BDReadCmd();
extern DWORD ReadDiscData(DWORD discArea, BYTE sectorCount);
extern void ReadCDDA();
extern DWORD BlockToMsf(DWORD block);
extern DWORD MSFtoLBA(DWORD msf);
extern void SledMove(DWORD position);
extern void SledServoSet(DWORD mode); 
extern void DpdEqualizer(DWORD value);
extern DWORD TrackingOn(void); 
extern void DpdTracking(DWORD on);
extern void DVDReadCmd();
extern void Seek10Command(void); 
extern DWORD DVDCheckLayer(DWORD discType);
extern void ReadBDData(DWORD fua);
extern DWORD CacheHitTest(void); 
extern void ReadCommandHandler(void); 
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