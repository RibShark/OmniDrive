// gd_read.c
//
// These functions read the requested sectors. They call the stock read loop
// (ReadCDDA), watch the Q channel for the sector that the read asks for (the
// address search), and start the decoder there. A lead-in read gets new seek
// attempts here. The hooks in ReadCDDA call the functions at the end of the
// file. After each read, GdSledReturn gives the drive back to the stock code.

#include "gd.h"

/*
 * Function Name:  GdSearchEnd
 *
 * End the address search, and end the time budget of the read loop.
 *
 * The time budget is the time limit of one pass of the read loop.
 * GdSeekTimerHook then ends the read loop in its next pass.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdSearchEnd(void) {
    gd.searchArmed = 0;
    gd.budgetStart = RTC_COUNT - (1u << 30);            // 2^30 ticks back: the budget is over
    gd.firstBlockTime = gd.budgetStart;                 //   and the first block cannot start it again
}

/*
 * Function Name:  GdSearchQ
 *
 * Get the address in the Q for the address search.
 *
 * In a lead-in read, 256 valid Qs in a row from outside the lead-in end the
 * search, so that the attempt ends in about 0.5 seconds in place of 12 seconds.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD                The FAD, or 0.
 */
static DWORD GdSearchQ(void) {
    DWORD fad = GdQFad(), qChannel0;
    if (!gd.leadIn)
        return fad;
    if (fad) {
        gd.search.outside = 0;
        if (!gd.attempt.firstQ && (WORD)fad != gd.search.armQ)
            gd.attempt.firstQ = fad;                    // the first new lead-in Q: GdLeadInNext aims with it
        return fad;
    }
    // A Q with no address (ADR not 1), or a Q of the lead-in that GdQFad refused, does not count as outside.
    qChannel0 = Q_CHANNEL0;
    if ((qChannel0 & 0xF) != 1 ||
        (((qChannel0 >> 8) & 0xFF) == 0 && (qChannel0 >> 24) >= 0xF0 && (qChannel0 >> 24) <= 0xF1))
        return 0;
    if (++gd.search.outside >= 256)
        GdSearchEnd();
    return 0;
}

/*
 * Function Name:  GdSearchStall
 *
 * End a search when the Q stops moving forward.
 *
 * A program search ends after 30 ms with no forward Q and the tracking drive
 * over its limit: the laser has slid. A lead-in search ends after 60 ms with
 * no forward Q and the lens shift at 32,000 or more: the lens is at its limit.
 *
 * Parameters:
 *        DWORD fad            The address from GdSearchQ.
 *
 * Returns:
 *        DWORD                fad, or 0 when the search ended here.
 */
static DWORD GdSearchStall(DWORD fad) {
    struct GdSearch *search = &gd.search;
    DWORD now = RTC_COUNT;
    int program = GdInProgramRead(), moved = 0;
    if (search->stallStart != search->start) {          // a new search
        search->stallStart = search->start;
        search->stallTime = now;
        search->stallQ = fad;
        return fad;
    }
    if (fad && fad != search->stallQ) {
        moved = fad - search->stallQ - 1 <= 63;         // forward: 1 to 64 frames
        search->stallQ = fad;
    }
    if (moved || !(SERVO_STATUS & SERVO_TRACK_DRIVE_HIGH)) {
        search->stallTime = now;
        // In the first 3 seconds of a program search, each forward Q moves the time budget on, so that a landing
        // far below the target can still reach it.
        if (moved && program && now - search->start < 1587302)
            gd.budgetStart = now;
        return fad;
    }
    if (program) {
        if (now - search->stallTime >= 15873) {         // 30 ms
            GdTrackingOpen();
            GdSearchEnd();
            return 0;
        }
    } else if (now - search->stallTime >= 31746 && GdAbs(GdLensShift()) >= 32000) {   // 60 ms
        if (SERVO_STATUS & SERVO_TRACKING_ON)
            GdTrackingOpen();
        GdSearchEnd();
        return 0;
    }
    return fad;
}

/*
 * Function Name:  GdDecoderStartAt
 *
 * Tell the decoder where to start, and start it.
 *
 * The decoder makes sectors from the signal and writes them to the buffer. In
 * a lead-in read it waits for the sector header of the target. In a program
 * read it runs in its audio form, so no header can match, and the hardware
 * searches the Q for the target instead.
 *
 * Preconditions:   gd.targetFad and gd.startEarly are set.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdDecoderStartAt(void) {
    DWORD msf;
    // FAD 675,000 is F0:00:00, and GdHdAttempt keeps a lead-in target below F2:00:00. The Q comes 3 frames ahead
    // of the data.
    if (gd.leadIn)
        msf = GdFadToMsf(gd.targetFad - 675000) + (0xF0u << 16);
    else
        msf = GdFadToMsf(gd.targetFad - gd.startEarly - 3);
    DECODER_TARGET = (DECODER_TARGET & 0xFF000000) | msf;
    DECODER_MODE |= 4;
    if (gd.leadIn) {
        DecoderStart(0, 3);
        return;
    }
    Q_SEARCH_TARGET = (Q_SEARCH_TARGET & 0xFF000000) | (DECODER_TARGET & 0x00FFFFFF);
    Q_CRC_CONTROL |= 7;
    Q_SEARCH_CONTROL = (Q_SEARCH_CONTROL & ~(3u << 18)) | (2u << 18);
    DECODER_MODE &= ~4u;
    if (gd.startEarly) {                                // after a missed start: one frame earlier, and the buffer
        DWORD block;                                    //   one block back, as the stock code after a missed target
        REG32(BUFFER_WRITE + 0x14) = 0x3FFF;
        REG32(BUFFER_WRITE + 0x18) = 2;
        block = REG32(BUFFER_WRITE + 4) & 0xFFFF;
        REG32(BUFFER_WRITE + 4) = block;
        if (block == 0)
            block = bufferBlocks;
        REG32(BUFFER_WRITE + 4) = (block - 1) & 0xFFFF;
        BufferCountSet(1, 7);
    }
    DecoderStart(1, 3);
}

/*
 * Function Name:  GdSearchArm
 *
 * Start the address search of this pass, or start the decoder again.
 *
 * The stock read loop calls this where it would start the decoder. We start the
 * decoder later, in GdSearchStep. If the decoder already started in this read,
 * it starts again at the same target.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdSearchArm(void) {
    if (gd.startFad) {
        GdDecoderStartAt();
        return;
    }
    if (gd.searchArmed)
        return;
    gd.search.qBelow = 0;
    gd.search.outside = 0;
    // The Q now can be left over from before the seek, so GdSearchStep ignores it.
    gd.search.armQ = (WORD)GdQFad();
    gd.search.start = RTC_COUNT;
    gd.searchArmed = 1;
}

/*
 * Function Name:  GdSearchStep
 *
 * Read the Q in each pass of the read loop. Start the decoder at the target.
 *
 * The start point lies 10 frames before the target (in a lead-in read, 6),
 * because the decoder needs a few frames to begin. A Q from there up to 5
 * frames on (in a lead-in read, 2) starts the decoder, if a Q below the start
 * point came first. Any other Q past the start point ends the search: the
 * laser is already past the target.
 *
 * A missed start shows as a buffer count below 0. It happens when the Q that
 * the hardware searches for has a bad CRC on the disc. The pass ends, and the
 * next attempt starts one frame earlier.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdSearchStep(void) {
    struct GdSearch *search = &gd.search;
    DWORD fad, point;
    if (!gd.searchArmed) {
        // a missed start of a program read: the buffer count is below 0
        if (GdInProgramRead() && gd.startFad && (((BUFFER_BLOCKS + gd.startEarly) << 16) & 0x80000000u)) {
            GdSearchEnd();
            gd.startEarly = 1;                          // the next attempt starts one frame earlier
        }
        return;
    }
    fad = GdSearchStall(GdSearchQ());
    if ((WORD)fad == search->armQ)                      // left over from before the seek
        return;
    point = gd.targetFad - (gd.leadIn ? 6 : 10) - gd.startEarly;   // the start point
    if (fad < point) {
        if (fad)
            search->qBelow = 1;
        return;
    }
    if (!search->qBelow || fad > point + (gd.leadIn ? 2 : 5)) {   // the laser is already past the target
        GdSearchEnd();
        return;
    }
    gd.startFad = GdQFad();
    gd.searchArmed = 0;
    GdDecoderStartAt();
}

/*
 * Function Name:  GdPassesDone
 *
 * Tell if the read loop has a result, so that it needs no more passes.
 *
 * Parameters:
 *        DWORD sent           TRANSFER_COUNT at the start of the read loop.
 *
 * Returns:
 *        int                  1 when the passes end, else 0.
 */
static int GdPassesDone(DWORD sent) {
    if (gd.leadIn && gd.startFad)                       // a lead-in read started its decoder
        return 1;
    if (!GdCdbIsHd() || GdCdbLba() + 150 != gd.targetFad)   // the CDB mirror no longer holds this request
        return 1;
    // the transfer count changed, the transfer ended, or the host no longer waits
    return TRANSFER_COUNT != sent || (TRANSFER_EVENTS & 0x40) || !hostCommand[0];
}

/*
 * Function Name:  GdLeadInNext
 *
 * Aim the next seek attempt of a lead-in read, and move the sled there.
 *
 * We want a landing GdLeadInMargin frames below the target. The first Q of the
 * last attempt shows where it landed, and the error moves the sled target.
 *
 * Preconditions:   The decoder did not start. GdPassesDone ends the passes
 *                  of a lead-in read that started it.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
GD_OUTLINE static void GdLeadInNext(void) {
    DWORD first = gd.attempt.firstQ;
    SDWORD step;
    if (!first) {
        // No first Q. A program Q means that the sled is too far out: 8 counts inward. A first Q in the attempt
        // before means that the aim is near: no move. Else 8 counts outward.
        DWORD qChannel0 = Q_CHANNEL0, tno = (qChannel0 >> 8) & 0xFF;
        step = ((qChannel0 & 0xF) == 1 && tno >= 3 && tno <= 0x99) ? 8 : gd.attempt.lastFirstQ ? 0 : -8;
    } else {
        if (first > gd.targetFad && gd.attempt.raise < 4)   // a landing above the target lowers the inner limit
            gd.attempt.raise++;
        // one count for each 300 frames of error, at most 6 counts inward and 16 counts outward
        step = GdRoundDivide((SDWORD)first - (SDWORD)(gd.targetFad - GdLeadInMargin), 300);
        step = step > 6 ? 6 : step < -16 ? -16 : step;
    }
    gd.attempt.target = (WORD)GdLeadInLimit((SDWORD)gd.attempt.target - step);
    gd.attempt.lastFirstQ = gd.attempt.count < 4 ? first : 0;
    gd.attempt.count++;                                 // at most 7 (8 passes)
    gd.attempt.firstQ = 0;
    gd.searchArmed = 0;
    SledMove(gd.attempt.target);
    GdSeekClose();
}

/*
 * Function Name:  GdReadLoop
 *
 * Run the stock read loop. In a lead-in read, try again up to 7 times.
 *
 * Any other read runs one pass. A lead-in read runs up to 8 passes in 16
 * seconds, and each later pass is a new seek attempt (GdLeadInNext). If no
 * pass gives a result, the host gets 03/02/00.
 *
 * Preconditions:   gd.targetFad, gd.leadIn and gd.noSeek are set for the read.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 *
 * Notes: gd.loop holds the values of the loop, so that the stack under
 * ReadCDDA stays small.
 */
void GdReadLoop(void) {
    struct GdLoop *loop = &gd.loop;
    int done;
    loop->passes = (gd.leadIn && !gd.noSeek) ? 8 : 1;
    loop->start = RTC_COUNT;
    loop->sent = TRANSFER_COUNT;
    GdCalibrationOff();
    gd.budgetStart = RTC_COUNT;
    ReadCDDA();
    while (!(done = GdPassesDone(loop->sent)) && RTC_COUNT - loop->start < (0x20u << 18) && --loop->passes) {
        gd.budgetStart = RTC_COUNT;
        gd.firstBlock = gd.bufferWrite = gd.budgetEnded = 0;
        commandActive = 1;
        startAddress = gd.targetFad;
        GdLeadInNext();
        ReadCDDA();
    }
    if (!done)
        ReturnSense(0x03, 0x02, 0x00);
}

/*
 * Function Name:  GdLoopContinue
 *
 * Decide if the stock read loop runs one more pass.
 *
 * It runs while the command is active. After that a read-ahead keeps it
 * running, and a new command goes to GdNewCommand. The read-ahead ends with
 * commandAbort, an eject, or the end of the time budget. GdReadAheadStep holds
 * its other limits.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  1 for one more pass, 0 for the end of the loop.
 */
static int GdLoopContinue(void) {
    struct GdReadAhead *readAhead = &gd.readAhead;
    if (commandActive)
        return 1;
    if (!GdInHdRead() || !readAhead->active || commandAbort || ejectPending || gd.budgetEnded)
        return 0;
    if (hostCommand[2] && !hostCommand[0])              // a new command waits
        GdNewCommand();
    GdReadAheadStep();
    return readAhead->active;
}

/*
 * Function Name:  GdSledReturn
 *
 * Give the drive back to the stock code at the end of a read.
 *
 * The decoder stops, and the sled goes to position 16. After a position
 * check, a lost focus comes back first. After a seek, the stock tracking
 * method and the stock RF gains come back. Last, the servo loops close and the
 * stock spindle values come back. A read with no seek that finds the sled at
 * 15 to 17 leaves the decoder and the sled as they are.
 *
 * The sled goes back because the next reference seek starts from here. On the
 * ASUS firmware by default, with no return, the stock servo moved the sled by
 * itself, and commands waited 4 to 5 seconds.
 *
 * We deal with the focus first because a sled move with no focus begins a read
 * with no data specifically for GD-Rs.
 *
 * Preconditions:   gd.hdRead is 0, so that the hooks give the stock result.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
GD_OUTLINE void GdSledReturn(void) {
    if (!gd.noSeek || (DWORD)(GdSledPosition() - 15) > 2) {
        DecoderStop(0, 1);
        if (gd.check.count != gd.check.countAtReturn && coilCounter == 0)   // not while the coil protection counts
            GdFocusOn();
        gd.check.countAtReturn = gd.check.count;
        SledMove(0x10);
    }
    if (!gd.noSeek) {
        DpdTracking(0);
        RfGainSet(gd.rfGain10, 0x10);                   // with our gain, the stock tracking closes slowly
        RfGainSet(gd.rfGain0, 0);
    }
    GdServoClose();
    GdSpindleRestore();
}

/*
 * Function Name:  GdServoStepSkip
 *
 * Tell ReadCDDA to skip its servo step and its decoder stop.
 *
 * They are skipped in a program read after the decoder start, once no command
 * is active. A read-ahead with cache hits also ends here.
 *
 * We do this because after the transfer, the stock loop starts a stock seek to
 * the target. That seek did not return before the host gave up.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  1 to skip, else 0.
 */
int GdServoStepSkip(void) {
    if (!GdInProgramRead() || !gd.startFad || commandActive)
        return 0;
    if (gd.readAhead.active && !gd.hits.why)
        GdReadAheadEnd(GD_END_OWED);
    return 1;
}

/*
 * Function Name:  GdFocusLossTest
 *
 * End the time budget when the focus stays lost for 0.5 seconds.
 *
 * A gap of 0.5 seconds between two calls starts the count again, so that a
 * time from an earlier command is not used. The end of a pass does not clear
 * the count, so the next attempt ends at its first test if the focus is still
 * lost.
 *
 * We do this because on the ASUS firmware, I noticed a read lost the focus in
 * its coarse seek, and it then ran for 24 seconds with no focus. This should
 * prevent that from happening.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  1 when this call ended the budget, else 0.
 */
static int GdFocusLossTest(void) {
    DWORD now = RTC_COUNT, last = gd.focusLossLast;
    gd.focusLossLast = now;
    if (((now - last) >> 18) || FOCUS_OK()) {           // a gap of 0.5 seconds, or the focus is there
        gd.focusLossStart = 0;
        return 0;
    }
    if (!gd.focusLossStart) {                           // the first test with no focus
        gd.focusLossStart = now;
        return 0;
    }
    if (((now - gd.focusLossStart) >> 18) == 0)         // less than 0.5 seconds with no focus
        return 0;
    gd.budgetStart = now - (1u << 30);
    return 1;
}

/*
 * Function Name:  GdSeekTimerHook
 *
 * Answer ReadCDDA's 20 ms timer in a high-density read.
 *
 * The stock read asks here if it must seek again. A stock seek would pull the
 * laser back to the single-density area, so in our read the answer is no.
 *
 * Each call writes gd.seekMsf to the stock seek target. Without this write the
 * stock seek code moves its target through the places of the TOC, and the sled
 * goes back. It seems that reads can only hold the track with it.
 *
 * While the transfer runs, the time budget continues, for 4 seconds at most
 * after the first block. At the end of the budget the pass ends. If the host
 * still waits for blocks, the stock transfer resets once.
 *
 * Parameters:
 *        DWORD timer          The answer of the stock timer.
 *
 * Returns:
 *        DWORD                timer outside a high-density read, else 0.
 */
DWORD GdSeekTimerHook(DWORD timer) {
    if (!GdInHdRead())
        return timer;
    seekTargetMsf[0] = (BYTE)(gd.seekMsf >> 16);
    seekTargetMsf[1] = (BYTE)(gd.seekMsf >> 8);
    seekTargetMsf[2] = (BYTE)gd.seekMsf;
    if (!GdFocusLossTest() && gd.firstBlock && !(TRANSFER_EVENTS & 0x40) && RTC_COUNT - gd.firstBlockTime < (8u << 18))
        gd.budgetStart = RTC_COUNT;
    if (RTC_COUNT - gd.budgetStart > ((gd.leadIn ? 0x18u : 4u) << 18)) {   // lead-in 12 seconds, program 2 seconds
        if (!gd.budgetEnded && (TRANSFER_LENGTH << 8))  // the host still waits for blocks
            TransferReset(0);
        commandActive = 0;
        readActive = 0;
        gd.budgetEnded = 1;
    }
    return 0;
}

/*
 * Function Name:  GdRetryTimerHook
 *
 * Give ReadCDDA no retry in a high-density read.
 *
 * Parameters:
 *        DWORD timer          The answer of the stock retry timer.
 *
 * Returns:
 *        DWORD                timer outside a high-density read, else 0.
 */
DWORD GdRetryTimerHook(DWORD timer) {
    return GdInHdRead() ? 0 : timer;
}

/*
 * Function Name:  GdDecoderStartHook
 *
 * Take the place of ReadCDDA's decoder start in a high-density read.
 *
 * We start the decoder later, at the target (GdSearchStep). Here the address
 * search starts (GdSearchArm).
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  1 when the stock decoder start must run, else 0.
 */
int GdDecoderStartHook(void) {
    if (!GdInHdRead())
        return 1;
    if (gd.leadIn) {
        DWORD control;
        if (gd.noSeek)                                  // its decoder already runs
            return 0;
        DECODER_TRIGGER = 0x101;                        // the decoder stops
        control = Q_SEARCH_CONTROL & ~1u;               // the Q search starts again: bit 0 cleared, then set
        Q_SEARCH_CONTROL = control;
        Q_SEARCH_CONTROL = control | 1;
        // the CD-ROM form of the decoder: its audio form never finds the sync mark of a sector
        DECODER_CIRC = (DECODER_CIRC & 0xFFFFF070u) | 0x200;
        DECODER_AUDIO = (DECODER_AUDIO & ~0x37u) | 0x28;
    }
    GdSearchArm();
    return 0;
}

/*
 * Function Name:  GdSeekStatusHook
 *
 * Take the place of ReadCDDA's test of playerStatus after its seek request.
 *
 * Outside a high-density read, r0 and r1 get playerStatus, as the stock code
 * has them. In a high-density read, r0 is 0, so the test passes and the code
 * that starts the decoder runs. r1 is 1, so the next stock call sets ENSDF,
 * bit 2 of 0x04209030.
 *
 * Parameters:  None.
 *
 * Returns:
 *        unsigned long long   r0 in the low word, r1 in the high word. The site
 *                             tests r0.
 */
unsigned long long GdSeekStatusHook(void) {
    DWORD r0 = 0, r1 = 1;
    if (!GdInHdRead())
        r0 = r1 = playerStatus;
    return ((unsigned long long)r1 << 32) | r0;
}

/*
 * Function Name:  GdSpeedChangeSkip
 *
 * Tell ReadCDDA to skip its speed change.
 *
 * Our code sets the spindle, so a high-density read skips the stock change. A
 * lead-in read after a seek lets it run until its first block.
 *
 * A stock speed change takes up to 2 seconds with no pass of the read loop,
 * and in that time the Q passes the point where the decoder must start.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  1 to skip, else 0.
 */
int GdSpeedChangeSkip(void) {
    return GdInHdRead() && (gd.noSeek || !gd.leadIn || gd.firstBlock);
}

/*
 * Function Name:  GdSpeedApplySkip
 *
 * Tell ReadCDDA to skip its speed step in a high-density read.
 *
 * Our code sets the spindle in a high-density read.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  1 to skip, else 0.
 */
int GdSpeedApplySkip(void) {
    return GdInHdRead();
}

/*
 * Function Name:  GdSlideCheck
 *
 * End the address search and the cache hits when the laser slides in a program
 * read.
 *
 * A slide is a move of the laser across the tracks while the tracking is on. It
 * shows as a lens shift above 10,000 with the tracking drive over its limit.
 * The tracking drive rarely goes over its limit, but it can happen. So we read
 * the lens shift only then.
 *
 * With the tracking on during a slide, the coil count rises, and then the
 * stock coil protection latches the drive.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdSlideCheck(void) {
    DWORD servo = SERVO_STATUS;
    if (GdInProgramRead() && (servo & SERVO_TRACK_DRIVE_HIGH) && GdAbs(GdLensShift()) > 10000) {
        if (servo & SERVO_TRACKING_ON)
            GdTrackingOpen();
        if (gd.searchArmed)
            GdSearchEnd();
        GdReadAheadEnd(GD_END_OWED);
    }
}

/*
 * Function Name:  GdLoopBottom
 *
 * Do our work at the bottom of each pass of the stock read loop.
 *
 * ReadCDDA also serves stock reads, so the work runs only in a high-density
 * read with a C0 in the CDB mirror.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  1 for one more pass, 0 for the end of the loop.
 */
int GdLoopBottom(void) {
    if (GdInHdRead() && cdb[0] == 0xC0) {
        GdSearchStep();
        GdSlideCheck();
        if (GdCdbIsHd()) {                              // a new command can come during the pass: test again
            if ((DATA_STATUS & (1u << 12)) && !gd.bufferWrite) {   // the first lock of the data PLL:
                gd.bufferWrite = 1;                     //   the buffer write starts
                DECODER_CONTROL |= 1;
                DECODER_MODE |= 4;
            }
            Q_SEARCH_CONTROL |= 0x101;                  // the Q search stays on
        }
        GdReadAheadStep();
        // No decoder start yet, and the transfer engine is idle: a transfer of one block, so that the first block
        // goes to the host.
        if (!gd.startFad && GdCdbIsHd() && (TRANSFER_EVENTS & 0x50) == 0x10) {
            DATA_PATH_CONTROL = (DATA_PATH_CONTROL & ~0x00030701u) | 0x00020000u;
            DATA_PATH_LENGTH8 = 1;
            TRANSFER_SECTORS = 1;
            TRANSFER_CONTROL &= ~2u;
            TRANSFER_AUTO = (TRANSFER_AUTO & ~3u) + 1;
        }
        if (GdCdbIsHd()) {
            if (gd.leadIn && (!gd.noSeek || gd.firstBlock))   // a lead-in read: the tracking closes
                GdTrackingClose();
            if (!gd.firstBlock && (SDWORD)(BUFFER_BLOCKS << 16) > 0) {   // the first block
                gd.firstBlock = 1;
                gd.firstBlockTime = RTC_COUNT;
            }
            playerStatus = 0;                           // our read has no stock seek
        }
    }
    return GdLoopContinue();
}
