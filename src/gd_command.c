// gd_command.c
//
// These functions handle the C0 03 command. A C0 03 command specifically reads
// the high-density area of a GD disc: its lead-in or its program area, as raw
// CD sectors with the C2 bits and the subcode.

#include "gd.h"

static void GdHdRead(void);
static void GdRead(void);

/*
 * Function Name:  GdInHdRead
 *
 * Determine if a high-density read is currently running.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  Not 0 when a high-density read is running.
 */
int GdInHdRead(void) {
    return gd.key == GD_RAM_KEY && gd.hdRead;           // GdRam holds random bytes before the first C0 command
}

/*
 * Function Name:  GdInProgramRead
 *
 * Determine if a high-density read of the program area is running and not a
 * lead-in read.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  Not 0 during a read of the program area.
 */
int GdInProgramRead(void) {
    return GdInHdRead() && !gd.leadIn;
}

/*
 * Function Name:  GdCdbIsHd
 *
 * Tell if cdb[] holds a C0 03 command.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  Not 0 when cdb[] holds a C0 03.
 */
int GdCdbIsHd(void) {
    return cdb[0] == 0xC0 && (cdb[1] & 0x03) == 0x03;
}

/*
 * Function Name:  GdCdbLba
 *
 * Get the LBA that the command in cdb[] is requesting.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD                The LBA in cdb[2..5], highest byte first.
 */
DWORD GdCdbLba(void) {
    return (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
}

/*
 * Function Name:  GdCalibrationOff
 *
 * Clear the request for the stock calibration at the start of a read.
 *
 * The calibration moves the sled far out on the disc, and on a GD disc it
 * measures in the high-density area. On the ASUS firmware it left a wrong
 * tracking gain, and the drive then refused every command.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdCalibrationOff(void) {
    if (readState & 0x80)                               // bit 7: the calibration is requested
        readState &= 0x7F;
}

/*
 * Function Name:  GdReadDiscRaw
 *
 * Check each raw read request (C0) from the host before the C0 handler of
 * main.c runs it.
 *
 * cdb[1] parameters:
 *
 *   bit 5 or bit 6 set            refused with 05/24/00
 *   low two bits = 0, a GD disc   normal read, with the sled limit on and
 *                                 the stock calibration off
 *   anything else                 the C0 handler of main.c. For the low two
 *                                 bits = 3, the GigaDisc (GD) type, it calls
 *                                 ReadGDRaw
 *
 * The C0 handler of main.c would ignore bits 5 and 6 and make a plain read. If
 * the host sends a command that sets them, we return a sense error instead.
 *
 * Postconditions:  GdRam is valid. The first C0 command after power-up clears
 *                  it and writes its key.
 *
 * Parameters:  None. The command is in cdb[].
 *
 * Returns:     Nothing. The answer goes out through the stock code.
 */
void GdReadDiscRaw(void) {
    if (gd.key != GD_RAM_KEY) {                         // the first C0 command of this build after power-up
        DWORD address;
        for (address = GD_RAM_BASE; address < GD_RAM_BASE + GD_RAM_SIZE; address += 4)
            REG32(address) = 0;
        gd.key = GD_RAM_KEY;
    }
    if (cdb[1] & 0x60) {                                // bit 5 or bit 6
        ReturnSense(0x05, 0x24, 0x00);                  // INVALID FIELD IN CDB
        return;
    }
    // A CD read of a GD disc, whose single-density area ends below GD_SD_LAST_SECTOR. A very short normal CD also
    // passes, but all of it lies inside the sled limit.
    if ((cdb[1] & 0x03) == 0 && lastSector - 1 < GD_SD_LAST_SECTOR) {
        if (driveSpeedCap >= driveSpeed)                // above the cap, the stock code makes a speed step instead
            GdCalibrationOff();
        GdSeekLimitOn();
        CmdOmniDriveReadDiscRaw();
        GdSeekLimitOff();
        return;
    }
    CmdOmniDriveReadDiscRaw();
}

/*
 * Function Name:  GdHdAttempt
 *
 * Make one attempt at the read that the command asks for.
 *
 * A disc that is not a CD gets 05/30/02. An address outside the area gets
 * 05/24/00, so the sled never goes where our model of the disc does not reach.
 * Bit 4 of cdb[10] determines the area:
 *
 *   the lead-in        LBA 674,850..681,524 (FAD 675,000..681,674)
 *   the program area   LBA 44,850..549,299 (the pregap of track 3 to 150
 *                      sectors into the lead-out)
 *
 * Preconditions:   GdReadDiscRaw has run, so GdRam is valid.
 * Postconditions:  senseCode holds the result of this attempt.
 *
 * Parameters:  None. The command is in cdb[].
 *
 * Returns:
 *        int  1               The read ran. senseCode describes how it ended.
 *        int  0               The command is refused.
 */
static int GdHdAttempt(void) {
    DWORD lba = GdCdbLba();
    readTimeCounter = 0;
    if (!(DiscIsCD())) {
        ReturnSense(0x05, 0x30, 0x02);                  // CANNOT READ MEDIUM - INCOMPATIBLE FORMAT
        return 0;
    }
    if ((cdb[10] & 0x10) ? (lba < GD_LBA_LEADIN_FIRST || lba > GD_LBA_LEADIN_LAST)
                         : (lba < GD_LBA_PROGRAM_FIRST || lba > GD_LBA_PROGRAM_LAST)) {
        ReturnSense(0x05, 0x24, 0x00);                  // INVALID FIELD IN CDB
        return 0;
    }
    // The time since the last read ended. After a long gap, GdReferenceChoose measures the reference again.
    gd.readGap = RTC_COUNT - gd.readEnd;
    if (servoFlags & 0x40)                              // bit 6: the stock code spun up the disc since the last attempt
        GdClockAfterStart();
    GdHdRead();
    return 1;
}

/*
 * Function Name:  GdPauseRequestsClear
 *
 * Stop the stock idle code from moving the laser after our read.
 *
 * By default, a normal read leaves a pause request behind, and a few seconds
 * later the stock idle loop acts on it. To get around this, we clear the
 * request at the start and at the end of each C0 03 command. At the end we
 * also stop the stock jump to the lead-out. If we don't do this, the stock code
 * would move the laser away from where the next read would start. For
 * instance, on the ASUS firmware, 0.2 seconds of idle time was enough to move
 * the sled right back into the single-density area immediately after a read.
 * This should prevent that from happening.
 *
 * Parameters:
 *        int atEnd            1 at the end of the command, 0 at the beginning.
 *
 * Returns:     Nothing.
 */
static void GdPauseRequestsClear(int atEnd) {
    if (atEnd && !refillDone) {                         // the stock idle loop then skips its jump to the lead-out
        Q_SEARCH_CONTROL &= ~(1u << 8);
        Q_SEARCH_LATCH = 3;
        refillDone = 1;
    }
    pauseRequests[3] = 0;                               // the two pause requests of the idle loop
    pauseRequests[4] = 0;
    pauseFlags &= ~0x04;                                // bit 2: pause
}

/*
 * Function Name:  GdRetryAllowed
 *
 * Decide if a new attempt may start, and get the focus back for it.
 *
 * An attempt with no focus can't work, and the stock seek inside it can take
 * about 9 seconds.
 *
 * We wait up to 100 ms for the focus. Without it, we close the servos again
 * and wait up to 400 ms, at most two times. A new attempt starts only with the
 * focus, and only when the command has run for less than 20 seconds.
 *
 * Preconditions:   gd.commandTime holds the start of the command.
 * Postconditions:  senseCode is 0 when the answer is 1, so the host does not
 *                  get the sense of the failed attempt.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int  1               A new attempt may start.
 *        int  0               Give up. The host gets the 03/02/00.
 */
static int GdRetryAllowed(void) {
    int i;
    GdWaitFocus(1, 100 * TICKS_MS);                     // wait up to 100 ms for the focus
    for (i = 0; !FOCUS_OK() && i < 2; i++) {
        GdServoClose();
        GdWaitFocus(1, 0x33AB8);                        // 400 ms
    }
    if (!FOCUS_OK() || RTC_COUNT - gd.commandTime >= GD_COMMAND_TICKS)
        return 0;
    senseCode = 0;
    return 1;
}

/*
 * Function Name:  ReadGDRaw
 *
 * Run one C0 03 command. First check its form, then try the read until it
 * works or until we have to give up. The C0 handler of main.c calls this for
 * the GigaDisc (GD) type.
 *
 * A form that this read does not support gets 05/24/00. After an attempt that
 * ends with 03/02/00, up to 9 more attempts run, and none starts after 20
 * seconds. A request that still waits when the read-ahead ends runs here again
 * as a new command (gd.runAgain), with its own set of attempts.
 *
 * The reason is that a bad landing of the coarse seek (the move of the sled,
 * which carries the laser unit, to the area of the target), or a data PLL with
 * no lock, can pass in a moment. A failed attempt has sent nothing to the host,
 * so it is safe to try again. The 20 seconds keeps the answer inside the 45
 * seconds that the host waits, because one attempt can take up to about 16
 * seconds.
 *
 * Preconditions:   GdReadDiscRaw has run, so GdRam is valid. The disc is a CD.
 *
 * Parameters:  None. The command is in cdb[].
 *
 * Returns:     Nothing. senseCode holds the answer.
 *
 * Notes: this loop always ends. Only gd.runAgain starts the count again, and
 * that needs a request that the buffer served in the read before.
 */
void ReadGDRaw(void) {
    BYTE saved[12];
    int i, retries = 0;
    // cdb[1]: raw addressing, descramble, and bit 7, which main.c sets to mark a raw DVD or BD read. cdb[10]: the
    // unused bits.
    if ((cdb[1] & 0x94) || (cdb[10] & 0xE8)) {
        ReturnSense(0x05, 0x24, 0x00);
        return;
    }
    for (i = 0; i < 12; i++)
        saved[i] = cdb[i];
    gd.commandTime = RTC_COUNT;
    gd.startEarly = 0;
    gd.attempt.raise = 0;
    GdPauseRequestsClear(0);
    for (;;) {
        int ran = GdHdAttempt(), mayRepeat;
        if (ran)
            gd.readEnd = RTC_COUNT;
        if (gd.runAgain) {
            gd.runAgain = 0;
            if ((senseCode >> 16) == 0x03) {            // the 03/02/00 of GdWaitingCommandEnd (not for the host)
                senseCode = 0;
                hostCommand[0] = 1;                     // GdWaitingCommandEnd cleared it; this request still waits
                for (i = 0; i < 12; i++)
                    saved[i] = cdb[i];
                gd.commandTime = RTC_COUNT;
                retries = 0;
                continue;
            }
        }
        if (!ran || retries >= GD_RETRIES || senseCode != 0x00030200)
            break;
        // A new attempt must not send a block twice. A failed one-block read has sent nothing. A lead-in
        // request can be up to 16 blocks, while the address search has not started the decoder.
        mayRepeat = (saved[10] & 0x10) ? saved[9] >= 1 && saved[9] <= 16 && !gd.startFad : saved[9] == 1;
        if (saved[6] || saved[7] || saved[8] || !mayRepeat)
            break;
        if (!hostCommand[0]) {                          // the host already has its answer for this command
            senseCode = 0;
            break;
        }
        // A new command is in cdb[]: a new attempt would send the old sector in answer to it.
        for (i = 0; i < 12 && cdb[i] == saved[i]; i++)
            ;
        if (i < 12 || !GdRetryAllowed())
            break;
        retries++;
    }
    GdPauseRequestsClear(1);
}

/*
 * Function Name:  GdHdRead
 *
 * Make one attempt at a read, from the set-up of the drive to the end.
 *
 * It sets up the spindle and the sector format, finds the reference, makes the
 * coarse seek, reads (GdRead), and gives the drive back (GdSledReturn).
 *
 * I'm setting a spindle limit of FG 60 (about 3,670 rpm) for safety reasons,
 * just in case there is a situation that causes the drive to spin the disc to
 * dangerous levels. I'm not completely sure what the drive does in every
 * instance of the spindle, as some might be intentional and some may not be.
 * But better safe than sorry.
 *
 * I'm setting the decoder to the audio type, which gives the raw 2,352 bytes.
 * The audio type also clears bit 5 of the decoder control, and without that
 * bit the decoder writes zeros, so I set it again. On the ASUS firmware, the
 * frequency detector target 24 read with no stall. 14 could not. A recordable
 * disc loads with this detector off.
 *
 * A recordable disc shows as a CD-ROM here because the stock read code treats
 * a CD-R differently. GdReferenceRead shows the real type for the reference
 * seek anyway.
 *
 * Preconditions:   GdHdAttempt has tested the disc and the address.
 *
 * Parameters:  None. The command is in cdb[].
 *
 * Returns:     Nothing. senseCode holds the result.
 */
static void GdHdRead(void) {
    BYTE sub = cdb[10] & 3;
    int i;
    // the spindle limit FG 60 first, before anything can start the motor
    SPINDLE_FG_LIMIT = (SPINDLE_FG_LIMIT & ~0xFFu) | 0x3C | (1u << 16);
    if (!gd.seekTargetSaved) {
        for (i = 0; i < 3; i++)
            gd.seekTarget[i] = seekTargetMsf[i];
        gd.seekTargetSaved = 1;
    }
    GdSpindleSave();
    gd.recordable = mediaType & 0x20 & ~(mediaType << 1);   // a CD-R (0x24) reads as a CD-ROM (0x04)
    mediaType ^= gd.recordable;

    gd.hdRead = 1;
    gd.leadIn = (cdb[10] & 0x10) ? 1 : 0;
    startAddress = GdCdbLba() + 150;
    transferLength = (cdb[6] << 24) | (cdb[7] << 16) | (cdb[8] << 8) | cdb[9];
    gd.targetFad = startAddress;
    hdMinuteMap |= 0x03;
    gd.bufferWrite = gd.firstBlock = gd.budgetEnded = 0;
    gd.startFad = 0;
    gd.attempt.firstQ = 0;
    gd.hits.served = 0;                                 // a new read ends the cache hits of the last one
    GdZcavStart();
    GdReadStart();

    SPINDLE_CLAMP = (SPINDLE_CLAMP & ~(0xFFu << 16)) | (0x30u << 16);   // the kick clamp
    SPINDLE_CONTROL |= 1u << 13;
    SPINDLE_FG_LIMIT = (SPINDLE_FG_LIMIT & ~0xFFu) | 0x3C | (1u << 16);   // the stock seek of the reference can
    GdSpindleWait();                                                      //   change this. Set it again.
    if (cdb[1] & 0x08)
        forceUnitAccess = 0xFF;
    DISC_SECTOR_REGISTER = 0x10;                        // the sector format as ReadCDRaw in main.c writes it
    SetCDType(0, sub == 2);
    if (sub)
        DISC_SECTOR_REGISTER |= 0x01;
    if (cdb[10] & 0x04)
        DISC_SECTOR_REGISTER |= 0x02;
    DATA_FREQ_DETECT = (DATA_FREQ_DETECT & ~(0x1Fu << 16)) | (0x18u << 16);   // the target 24
    DECODER_CONTROL8 |= 0x20;                           // the audio type cleared this bit: no data without it
    GdDpdOn();
    if (!gd.noSeek) {                                   // the coarse seek
        SledMove(GdSeekTarget());
        GdSeekClose();
    }
    gd.seekMsf = (BlockToMsf(startAddress - 3) & 0xFFFF) | (0x14u << 16);
    GdRead();

    gd.hdRead = 0;
    gd.leadIn = 0;
    gd.searchArmed = 0;
    GdSledReturn();
    mediaType |= gd.recordable;
    gd.recordable = 0;
}

/*
 * Function Name:  GdRead
 *
 * Start the read once everything has been set up beforehand.
 *
 * A read ends with 03/02/00 if there's no reference. A program read after a
 * seek runs the position check first, then the read loop. A lead-in read, and
 * a request that the buffer holds, go straight to the read loop.
 *
 * A lead-in read has no position check because its addresses do not fit the
 * model of the program area. Its own seek attempts run in the read loop.
 *
 * Preconditions:   gd.ref.failed, gd.leadIn and gd.noSeek are set for the read.
 * Postconditions:  senseCode holds the result.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 *
 * Notes: GdPositionCheck returns before GdReadLoop starts, so that its stack
 * frame does not sit under the read loop.
 */
static void GdRead(void) {
    if (gd.ref.failed) {
        ReturnSense(0x03, 0x02, 0x00);
    } else if (!gd.leadIn && !gd.noSeek) {
        if (GdPositionCheck())
            GdReadLoop();
    } else
        GdReadLoop();
}
