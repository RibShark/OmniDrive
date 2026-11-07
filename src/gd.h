// gd.h
//
// The declarations that the files of the high-density read share: the
// registers, the numbers of the read, the state of the read in RAM (GdRam), and
// the functions that one file calls in another. src/defs.h declares the stock
// functions and variables, and the drive file gives their addresses.

#ifndef GD_H
#define GD_H

#include "defs.h"
#include "mmio.h"

typedef signed int SDWORD;
typedef signed short SWORD;
typedef signed char SBYTE;

#define REG32(address)          (*(volatile DWORD *)(address))
#define REG16(address)          (*(volatile WORD *)(address))
#define REG8(address)           (*(volatile BYTE *)(address))

// the ARM functions of the ROM page (the same on every drive)
#define InterruptsOff           ((DWORD (*)(DWORD))0x29C)           // OUT the old CPSR
#define InterruptsRestore       ((void (*)(DWORD))0x294)            // msr cpsr_c, r0

// the registers
#define RTC_COUNT               REG32(0x04002078)       // the free-running clock
#define TICKS_MS                529u                    //   529 ticks a millisecond
#define POWER_DOWN              REG32(0x04000004)       // bit 30: the DPD block off
#define CLOCK_DIVIDER           REG8(0x04000028)        // bits 6..4: the clock divider of the data channel
#define ANALOG_PLL              REG8(0x0420C07E)
// the buffer
#define BUFFER_BLOCKS           REG32(0x040300A0)
#define BUFFER_UNSENT           REG32(0x040300A4)       // the blocks the decoder wrote and the host did not get
#define BUFFER_UNSENT16         REG16(0x040300A4)
#define BUFFER_WRITE            0x040300C0u             // + 0: the write block; + 4, + 0x14, + 0x18: its set-up
#define BUFFER_READ_BLOCK       REG32(0x040300CC)
// the host
#define HOST_EVENTS             REG32(0x04040064)
#define HOST_CONTROL            0x040400C0u             // + 4, + 0xC, + 0x10
#define TRANSFER_EVENTS         REG8(0x04042000)        // bit 6: the transfer ended
#define TRANSFER_CONTROL        REG32(0x04042120)
#define TRANSFER_AUTO           REG32(0x04042130)
#define TRANSFER_SECTORS        REG8(0x04042134)
#define TRANSFER_LENGTH         REG32(0x04042138)       // bits 23..0: the blocks the host still waits for
#define TRANSFER_COUNT          REG32(0x04042154)       // it changes with each transfer
#define DATA_PATH_CONTROL       REG32(0x04050060)
#define DATA_PATH_LENGTH        REG32(0x0405006C)
#define DATA_PATH_LENGTH8       REG8(0x0405006C)
#define DATA_PATH_LBA           REG32(0x0405008C)       // bits 27..0: the LBA of the first unsent block
// the decoder and the Q channel
#define Q_SEARCH_CONTROL        REG32(0x04082004)
#define Q_SEARCH_LATCH          REG32(0x04082010)
#define Q_SEARCH_TARGET         REG32(0x04082100)       // bits 23..0: the target MSF of the Q search (BCD)
#define DECODER_TRIGGER         REG32(0x040C0000)       // 0x101: the CD decoder start or stop
#define DECODER_TARGET          REG32(0x040C002C)       // bits 23..0: the MSF where the decoder starts
#define DECODER_STATUS          REG8(0x040C0041)
#define DECODER_CONTROL         REG32(0x040C2000)
#define DECODER_CONTROL8        REG8(0x040C2000)
#define DECODER_AUDIO           REG32(0x040C2008)
#define DECODER_CIRC            REG32(0x040C200C)
#define DECODER_MODE            REG32(0x040C2010)       // bit 1: the C2 form; bit 2: the start at DECODER_TARGET
#define Q_CRC_CONTROL           REG32(0x040C2018)
#define Q_CHANNEL0              REG32(0x040C2034)       // the decoded Q: bytes 0..3 (CONTROL/ADR, TNO, INDEX, MIN)
#define Q_CHANNEL0_LOW          REG16(0x040C2034)
#define Q_CHANNEL1              REG32(0x040C2038)       //   bytes 4..7 (SEC, FRAME, ZERO, AMIN)
#define Q_CHANNEL2              REG32(0x040C203C)       //   bytes 8..11 (ASEC, AFRAME, CRC)
// the servo and the spindle
#define SERVO_BASE              0x04200000u
#define SPINDLE_BASE            0x04205000u             // + 0x24, + 0x30, + 0x40
#define SPINDLE_DRIVE           REG16(0x0420500C)       // 0: the spindle is stopped
#define SPINDLE_CONTROL         REG32(0x04205020)       // bit 13: the clamp
#define SPINDLE_CLAMP           REG32(0x04205028)       // bits 23..16: the clamp of the kick
#define SPINDLE_SPEEDUP         REG32(0x04205038)       // bits 8..0: the FG speed-up
#define SPINDLE_FG_LIMIT        REG32(0x0420506C)
#define SPINDLE_LOAD            REG32(0x04205070)       // 4: the new spindle values apply
#define SPINDLE_COUNT           REG32(0x04205080)       // bits 15..8: the FG frequency
#define SERVO_DSP               0x04206000u             // the servo DSP: the command word, + 4 its status, + 0x10 its data
#define SERVO_STATUS            REG32(0x04206080)
#define DATA_FREQ_DETECT        REG32(0x04207500)       // bits 20..16: the target of the frequency detector
#define DATA_STATUS             REG32(0x04209000)       // bit 12: the data PLL is locked
#define RF_ZC_HYSTERESIS        REG32(0x0420B110)       // the hysteresis of the RF zero cross
#define RF_ZC_HYSTERESIS2       REG32(0x0420B168)
#define RF_POWER_DOWN           REG32(0x0420C000)       // bit 6: the RF front end of DPD off

#define SERVO_TRACK_DRIVE_HIGH  (1u << 3)               // SERVO_STATUS: the tracking drive is over its limit
#define SERVO_TRACKING_ON       (1u << 6)
#define SERVO_SEEK_OK           (1u << 7)
#define SERVO_FOCUS_ON          (1u << 13)
#define SERVO_FOCUS_OK          (1u << 14)
#define FOCUS_OK()              (SERVO_STATUS & SERVO_FOCUS_OK)
#define SEEK_OK()               (SERVO_STATUS & SERVO_SEEK_OK)
#define SPINDLE_FG()            ((BYTE)(SPINDLE_COUNT >> 8))    // the FG: the pulse rate of the spindle motor

extern void CmdOmniDriveReadDiscRaw(void);              // the C0 handler of src/main.c

// The stock unsigned divide is ARM code. The C code calls it through its address, a data word of the gd asm file,
// so that each call is a blx with a register: the blx that armips makes from a direct call is wrong at an address that
// is 2 more than a multiple of 4.
extern DWORD (* const UnsignedDivide)(DWORD number, DWORD divisor);

// the command
#define GD_LBA_LEADIN_FIRST     674850u                 // FAD 675,000: the first sector of the high-density lead-in
#define GD_LBA_LEADIN_LAST      681524u                 // FAD 681,674
#define GD_LBA_PROGRAM_FIRST    44850u                  // the pregap of track 3 (FAD 45,000)
#define GD_LBA_PROGRAM_LAST     549299u                 // 150 sectors into the high-density lead-out
#define GD_SD_LAST_SECTOR       13949u                  // a disc whose last single-density sector lies lower is a GD disc
#define GD_RETRIES              9                       // the most attempts after the first
#define GD_COMMAND_TICKS        10582000u               // 20 seconds: no attempt starts later
#define GD_IDLE_TICKS           211600u                 // 400 ms: after a longer gap the reference is measured again
// the reference position and the model
#define GD_REF_AGE              15870000u               // 30 seconds: an older reference is not used again
#define GD_FIT_LAMBDA           10486                   // the weight of the prior of the fit
#define GD_FIT_FAR_FRAMES       131072                  // a landing at FAD 169,547 or more (FAD 38,475 + this) shows the density
// the position check and the track jump
#define GD_CHECK_TICKS          (300 * TICKS_MS)        // the wait for a pair of Q
#define GD_FAR_FRAMES           1000                    // a position farther below the target is refused,
#define GD_FAR_TICKS            0x180000u               //   in the first 3 seconds of the command
#define GD_JUMP_MAX_TRACKS      150
#define GD_JUMP_Q_TICKS         211600u                 // 400 ms: the wait for a pair of Q after a jump
// the address search, the read-ahead and the cache hits
#define GD_READAHEAD_BLOCKS     1000                    // the most blocks of a read-ahead
#define GD_HIT_IDLE             (1000 * TICKS_MS)       // no request for 1 second ends the cache hits
// the spindle and the stock seek
#define GD_ZCAV_SPEED           12                      // the speed of a program read (12x)
#define GD_RPM                  2000                    // the spindle of a lead-in read
#define GD_SEEK_LIMIT           0x1A0                   // the sled limit of a stock seek: position 416

// the numbers that differ from drive to drive: data words of the gd asm file, with the values of the drive file
extern const SDWORD GdLeadInOffset;                     // the sled position of FAD GdLeadInLandingFad, above the reference
extern const SDWORD GdLeadInLandingFad;
extern const SDWORD GdLeadInScale;                      // the frames of one sled count in the first lead-in aim
extern const SDWORD GdLeadInMargin;                     // a lead-in seek aims this many frames below the target
extern const SDWORD GdLeadInAimHigh, GdLeadInAimMax;    // the highest lead-in aim: above the reference, and in all
extern const SDWORD GdReferenceLow, GdReferenceHigh;    // the range of a good reference
extern const SDWORD GdReferenceTolerance;               // two readings that agree within this settle the reference
extern const SDWORD GdModelOrigin, GdModelScale;        // the radius (um) of sled position 0; 65,536 / the sled step (um)
extern const SDWORD GdSledStepX16;                      // the sled step (um) x 16
extern const DWORD GdLeadInPlayTicks;                   // the longest play through the lead-in that a pregap check ignores
extern const SDWORD GdJumpMargin;                       // the jump target lies this many frames before the target
extern const DWORD GdJumpRecoveries;                    // the most focus-ons and pull-ins after a jump with no pair of Q
extern const DWORD GdProgramSeekMax;                    // the highest sled position of a program seek
extern const DWORD GdTrackingBandHigh;                  // the highest sled band (position / 16) of a tracking pull-in

enum {                                                  // why the cache hits ended (gd.hits.why)
    GD_END_IDLE = 1,                                    // no request for 1 second
    GD_END_OWED,                                        // the read-ahead stopped while the waiting command needs a block
    GD_END_NEW };                                       // the waiting command runs as a new command

// the RAM
// GdRam is free in the BU40N firmware, it doesn't look like the stock code uses it for anything.
#define GD_BUFFER               0x01C08000u             // block k of the buffer: GD_BUFFER + k x 0xB00
#define GD_RAM_BASE             0x01F22800u
#define GD_RAM_SIZE             0x800u

// The state of the read is in GdRam, at GD_RAM_BASE. The C code has no global
// variables.
struct GdRef {                                          // the reference position: the sled position at LBA 9,000
    BYTE failed;                                        // no reference for this read (03/02/00)
    BYTE checks;                                        // gd.check.count at the reference
    BYTE clockFixes;                                    // the clock fixes after a start (GdClockAfterStart)
    SWORD value;
    WORD pll;                                           // dataPllPreset at the last measured reference
    DWORD time;
};

struct GdFit {                                          // the fit of the reference and the density from up to 32 positions found
    BYTE count, rejects, starts;                        // the positions; the refusals in a row; gd.ref.clockFixes at the first
    SWORD ref, slope, prior;                            // the reference; the density x 4; the density before the fit x 4
    SDWORD sumEffect, sumMeasured, sumProduct;          // the sums: effect, measured, effect x measured,
    DWORD sumEffectSquared;                             //   and effect x effect
};

struct GdCheck {                                        // the position check of a program read
    BYTE count, countAtReturn;                          // the position checks; their count at the last sled return
    BYTE refocus;                                       // gd.check.count at the last refocus (one a check)
    SBYTE density;                                      // the density of the disc: the correction of the model
    WORD radius;                                        // the radius of the target (um)
    DWORD fad;                                          // the FAD of the position found
    DWORD seekQ;                                        // the program Q at the seek or the jump (0: the Q moved on)
    DWORD leadInQ, leadInQTime;                         // the last lead-in Q of the wait for a pair, and its time
};

struct GdJump {                                         // the track jump of a position check
    BYTE retries;                                       // at most 2
    DWORD fad;                                          // the position: the landing, then the first Q of each new pair
    DWORD servo[18];                                    // the servo values that the stock track jump can change
};

struct GdLeadIn {                                       // the seek attempts of a lead-in read
    WORD target, raise;                                 // the next sled target; how far the target moved up (0..4)
    BYTE count;
    DWORD firstQ;                                       // the first new lead-in Q of this attempt (0: none)
    DWORD lastFirstQ;                                   // the one of the attempt before (only in the first four)
};

struct GdSearch {                                       // the address search: the decoder starts when the Q reaches X
    BYTE qBelow;                                        // a Q below the start point came
    WORD armQ;                                          // the Q (low 16 bits) when it armed
    WORD outside;                                       // valid Qs in a row outside the lead-in
    DWORD start;
    DWORD stallTime, stallQ, stallStart;                // the stall test: the last time the Q moved on, the last Q
};

struct GdReadAhead {                                    // the decoder reads on after the start, for the cache hits
    BYTE active;
    DWORD start;                                        // the time of the start, 0 before it
};

struct GdHits {                                         // the requests served from the buffer during the read-ahead
    BYTE why, served;                                   // why: GD_END_*, 0 while the cache hits run
    DWORD lastHit;
};

struct GdSpindle {                                      // the stock spindle values, kept during the high-density read
    BYTE saved;
    BYTE reg40, reg28, reg24;                           // SPINDLE_BASE + 0x40 bits 11..8, + 0x28 bits 7..0, + 0x24 bit 0
    DWORD reg30;                                        // SPINDLE_BASE + 0x30
    WORD reg38;                                         // SPINDLE_SPEEDUP bits 10..0
    BYTE stockSpeedUp;                                  // the stock FG speed-up (0: none)
    BYTE capSaved, capSet;                              // the stock speed cap; the cap we set
};

struct GdZcav {                                         // ZCAV: the spindle speed of a program read on a pressed disc
    BYTE on, speed, goal;                               // the speed now (x); the speed at the end of the ramp
    WORD rpm;                                           // the speed of the zone
    DWORD nextStep, zoneEnd;                            // FADs
};

struct GdLoop {                                         // the read loop: in GdRam, not on the stack, so that the stack
    DWORD passes, start, sent;                          //   under ReadCDDA stays small
};

struct GdState {
    DWORD key;                                          // GD_RAM_KEY after the first C0 command
    BYTE speedUp;                                       // + 4: the FG speed-up of the speed hooks (asm)
    BYTE hdRead;                                        // + 5: inside a high-density read
    BYTE searchArmed;                                   // + 6: the address search waits for its Q
    BYTE leadIn;                                        // a lead-in read
    BYTE recordable;                                    // 0x20: a CD-R (a GD-R) that the read shows as a CD-ROM
    BYTE noSeek;                                        // the request lies just ahead in the buffer: one pass, no seek
    BYTE firstBlock, budgetEnded, bufferWrite;          // this pass
    BYTE startEarly;                                    // a program start missed: the decoder starts one frame earlier
    BYTE runAgain;                                      // the end of the cache hits keeps the waiting command
    BYTE limitWaits;                                    // the seek limit: its waits for the spindle
    BYTE seekTargetSaved, seekTarget[3];                // the stock seek target before the read
    WORD rfGain10, rfGain0;                             // the RF gains before DPD
    DWORD targetFad;                                    // the FAD of the request
    DWORD commandTime;                                  // the start of the command (its 20 seconds)
    DWORD readEnd, readGap;                             // the end of the last read; the gap before this one
    DWORD startFad;                                     // the FAD where the address search started the decoder (0: no)
    DWORD seekMsf;                                      // X - 3 with the minute 0x14: the seek target in the read loop
    DWORD budgetStart;                                  // the start of the time budget of the read loop
    DWORD firstBlockTime;
    DWORD focusLossStart, focusLossLast;
    struct GdRef ref;
    struct GdFit fit;
    struct GdCheck check;
    struct GdJump jump;
    struct GdLeadIn attempt;
    struct GdSearch search;
    struct GdReadAhead readAhead;
    struct GdHits hits;
    struct GdSpindle spindle;
    struct GdZcav zcav;
    struct GdLoop loop;
};
#define gd (*(struct GdState *)GD_RAM_BASE)

_Static_assert(sizeof(struct GdState) <= GD_RAM_SIZE, "GdState must fit in its 2 KB");
_Static_assert(__builtin_offsetof(struct GdState, speedUp) == 4, "the speed hooks read GdRam + 4");
_Static_assert(__builtin_offsetof(struct GdState, hdRead) == 5, "the test of the hook sites writes GdRam + 5");
_Static_assert(__builtin_offsetof(struct GdState, searchArmed) == 6, "the test of the hook sites writes GdRam + 6");

int GdLoopBottom(void);
// The key changes with the code and with the size of GdState: a new build zeroes GdRam at its first C0 command, also
// when the drive did not lose power after the flash.
#define GD_RAM_KEY              (0x5AA5C0DEu + (DWORD)GdLoopBottom + (DWORD)sizeof(struct GdState))

#define GD_SYMBOL               __attribute__((noinline, noclone))
#define GD_OUTLINE              __attribute__((noinline))       // its frame is not under the read loop's deepest calls

#define GdTrackingOpen()        GdServoCommand(0xC600, 7)       // the tracking servo loop open
#define GdTrackingClose()       GdServoCommand(0xC600, 0x11)    //   and closed
#define GdSledPosition()        ((SDWORD)(SWORD)GdServoRead(0x300))   // the position of the sled
#define GdLensShift()           ((SDWORD)(SWORD)GdServoRead(0x302))   // how far the lens has moved from its centre

/*
 * Function Name:  GdAbs
 *
 * Get the absolute value of a signed number.
 *
 * Parameters:
 *        SDWORD value         The number you want.
 *
 * Returns:
 *        DWORD                The absolute value of the number.
 */
static inline DWORD GdAbs(SDWORD value) {
    return value < 0 ? -(DWORD)value : (DWORD)value;    // unsigned, so the lowest SDWORD is also correct
}

/*
 * Function Name:  GdReadAheadEnd
 *
 * End the cache hits and keep the reason.
 *
 * Parameters:
 *        BYTE why             GD_END_IDLE, GD_END_OWED or GD_END_NEW.
 *
 * Returns:     Nothing.
 */
static inline void GdReadAheadEnd(BYTE why) {
    gd.hits.why = why;
}

// gd_command.c
int GdInHdRead(void);
int GdInProgramRead(void);
int GdCdbIsHd(void);
DWORD GdCdbLba(void);
void GdCalibrationOff(void);

// gd_seek.c
void GdSeekLimitOn(void);
void GdSeekLimitOff(void);
void GdReadStart(void);
SDWORD GdRoundDivide(SDWORD number, DWORD divisor);
SDWORD GdLeadInLimit(SDWORD target);
DWORD GdSeekTarget(void);
void GdSeekClose(void);
int GdPositionCheck(void);

// gd_spindle.c
void GdSpindleSave(void);
void GdZcavStart(void);
void GdZcavStep(DWORD fad);
void GdSpindleWait(void);
void GdSpindleRestore(void);

// gd_servo.c
DWORD GdServoRead(DWORD index);
int GdWaitFocus(int want, DWORD ticks);
void GdFocusOn(void);
void GdSeekDoneWait(void);
void GdServoClose(void);
DWORD GdTrackingOn(void);
void GdPullInAgain(void);
void GdRefocus(void);
void GdDataPllSet(DWORD rate);
void GdClockAfterStart(void);
void GdDpdOn(void);

// gd_qchannel.c
DWORD GdFadToMsf(DWORD fad);
DWORD GdQFad(void);
DWORD GdQProgramFad(void);
DWORD GdBlockHeaderFad(DWORD block);
DWORD GdBlockFad(DWORD block);

// gd_read.c
void GdReadLoop(void);
void GdSledReturn(void);

// gd_cache.c
void GdReadAheadStep(void);
void GdNewCommand(void);

// The command table
void GdReadDiscRaw(void);                               // the C0 entry of the command table
void ReadGDRaw(void);                                   // the C0 handler of main.c calls it for the GD type
int GdServoStepSkip(void);
int GdSpeedApplySkip(void);
DWORD GdRetryTimerHook(DWORD timer);
DWORD GdSeekTimerHook(DWORD timer);
int GdSpeedChangeSkip(void);
unsigned long long GdSeekStatusHook(void);
int GdDecoderStartHook(void);
DWORD GdSeekTimeTest(DWORD r0, DWORD r1, DWORD r2, DWORD r3);   // the veneer words of the seek limit
DWORD GdSeekStep(DWORD r0, DWORD r1, DWORD r2, DWORD r3);
DWORD GdSeekRecover(DWORD status, DWORD r1, DWORD r2, DWORD r3);
void GdServoWrite(DWORD index, DWORD value);
void GdServoCommand(DWORD reg, DWORD value);
void GdServoSave(int restore);
extern const WORD GdServoSaveTable[18];

#endif
