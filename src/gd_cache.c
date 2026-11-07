// gd_cache.c
//
// These functions serve the next requests from the buffer. After the decoder
// starts, they keep it reading ahead, send each requested block that the
// buffer holds to the host with no seek, and answer a command that still
// waits when the read-ahead ends.

#include "gd.h"

static int GdReadAheadKeep(void);

/*
 * Function Name:  GdReadAheadStep
 *
 * Keep the decoder reading after it has started so that the buffer holds the
 * next request. The read loop calls this once in each pass.
 *
 * A request that the buffer serves is considered a 'cache hit'. While there is
 * no cache hit, the read-ahead ends when the decoder has written 1,000 blocks,
 * or after about 4 seconds if the decoder stalls. A program read starts its
 * buffer write just before the end of the buffer, so those positions count as
 * 0. After the first cache hit, GdReadAheadKeep decides what happens next.
 *
 * Preconditions:   gd.hdRead is 1.
 * Postconditions:  gd.readAhead.active says if the read-ahead runs.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdReadAheadStep(void) {
    struct GdReadAhead *readAhead = &gd.readAhead;
    DWORD now = RTC_COUNT, pos;
    if (!gd.startFad) {                                 // the decoder has not started
        readAhead->active = 0;
        readAhead->start = 0;
        return;
    }
    if (!readAhead->start) {                            // the first pass after the decoder start
        readAhead->start = now;
        readAhead->active = 1;
        gd.hits.why = 0;
        return;
    }
    if (!readAhead->active)                             // the read-ahead has ended in this read
        return;
    if (gd.hits.served)
        readAhead->active = GdReadAheadKeep();
    else if (!gd.hits.why) {                            // no cache hit yet
        pos = REG32(BUFFER_WRITE) & 0x1FFF;             // the block that the decoder writes now
        if (!gd.leadIn && pos >= bufferBlocks - 3u - gd.startEarly)
            pos = 0;                                    // a program read starts its write here
        // at most 1,000 blocks, and at most (1,000 + 64) x 2,048 ticks: about 4 seconds
        readAhead->active = pos < GD_READAHEAD_BLOCKS && now - readAhead->start < ((GD_READAHEAD_BLOCKS + 64) << 11);
    } else
        readAhead->active = 0;
}

/*
 * Function Name:  GdHitCheck
 *
 * Make the last check of a cache 'hit' just before it's transferred.
 *
 * The reason is because in a lead-in read, a block starts with a CD sync, and
 * its header must give the FAD of the request. A block with no sync passes. A
 * block with a wrong header ends the cache hits, and the request runs as a new
 * read.
 *
 * A program block gets no check, because the audio form of the decoder leaves
 * its header scrambled. The ZCAV step also runs here when it is due.
 *
 * On the ASUS firmware, a transfer from a wrong block sent 6 reads of wrong
 * bytes with a good status.
 *
 * Preconditions:   CacheHitTest found the request and wrote transferBlock.
 *
 * Parameters:
 *        DWORD fad            The FAD of the request.
 *
 * Returns:
 *        int                  1 means the block can go to the host, else 0.
 */
static int GdHitCheck(DWORD fad) {
    DWORD headerFad;
    if (gd.zcav.on && fad >= gd.zcav.nextStep)          // the next ZCAV step is due
        GdZcavStep(fad);
    if (!gd.leadIn || (headerFad = GdBlockHeaderFad(transferBlock)) == 0 || headerFad == fad)
        return 1;                                       // a program block, a block with no sync, or the right header
    GdReadAheadEnd(GD_END_NEW);                         // a wrong header: the request runs as a new read
    return 0;
}

/*
 * Function Name:  GdHostTransfer
 *
 * Start the transfer of blocks from the buffer to the host.
 *
 * This is a rewrite of the stock code responsible for a 'cache hit' during a
 * stock read command, with three differences:
 *
 *   hostCommand[2] = 0        The command is taken, so the stock dispatcher
 *                             does not run it again.
 *   no speed step             The stock code calls a step here that can ask
 *                             for a speed change.
 *   no bit 1 of readActive    With it, the stock loop counts the hits and can
 *                             move the speed cap.
 *
 * Preconditions:   transferBlock holds the first block.
 *
 * Parameters:
 *        DWORD blocks         The blocks to send.
 *
 * Returns:     Nothing.
 */
static void GdHostTransfer(DWORD blocks) {
    volatile BYTE *command = hostCommand;
    BYTE maxBlocks;
    command[1] = command[2] = command[3] = 0;           // [2] = 0: the command is taken
    command[0] = 1;                                     // the host waits for this command
    commandActive = 1;
    BUFFER_READ_BLOCK = transferBlock;                  // the first block to send
    AtaStatusSet(0, 1);                                 // the ATA status: no error, ready
    REG32(HOST_CONTROL + 0xC) |= 1u << 28;              // sends the status when the transfer ends.
    TRANSFER_LENGTH = (TRANSFER_LENGTH & 0xFF000000) | blocks;   // the blocks that the host waits for
    maxBlocks = transferMaxBlocks[1] >> 3;
    if (maxBlocks == 0)
        maxBlocks = 1;
    transferMaxBlocks[1] = maxBlocks;
    DATA_PATH_LENGTH = (DATA_PATH_LENGTH & ~0x7Fu) | maxBlocks;
    TRANSFER_AUTO = (TRANSFER_AUTO & ~3u) + 1;
    readTimeCounter = 0;
    transferFlags = 0;
}

/*
 * Function Name:  GdCommandTake
 *
 * Take a waiting command so that the read-ahead can answer it.
 *
 * A new command changes cdb[] in an interrupt. With the interrupts off, we
 * test that the mirror still holds this opcode and that the command still
 * waits.
 *
 * Parameters:
 *        DWORD opcode         The opcode of the command that we want to take.
 *
 * Returns:
 *        int                  1 means this call took the command, else 0.
 */
static int GdCommandTake(DWORD opcode) {
    volatile BYTE *command = hostCommand;
    DWORD cpsr = InterruptsOff(0xC0);
    int took = 0;
    if (cdb[0] == opcode && command[0] == 0 && command[2] != 0) {
        command[0] = 1;
        took = 1;
    }
    InterruptsRestore(cpsr);
    return took;
}

/*
 * Function Name:  GdWaitingCommandEnd
 *
 * End a command that still waits when the cache hits end.
 *
 * The status of a served command goes out by itself when its transfer ends. If
 * that status is still set, there are two cases:
 *
 *   blocks pending, no new command     The decoder stops. The answer is
 *                                      03/02/00.
 *   no blocks pending, 1 second idle   The status was lost. The answer is
 *                                      04/08/80.
 *
 * A program request of one block with blocks pending gets no status here.
 * gd.runAgain tells ReadGDRaw to read it as a new command, and the host sees
 * one long command.
 *
 * On the ASUS firmware, a command got no answer in 45 seconds because its
 * status did not go out, and the drive hung as a result. Why the status did
 * not go out is not known, but this prevents it from happening.
 *
 * Preconditions:   gd.hits.why holds the reason for the end.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdWaitingCommandEnd(void) {
    DWORD ata = REG32(HOST_CONTROL + 0xC), held = TRANSFER_LENGTH, cpsr;
    int kind = 0;
    // The status is still set (bit 28), the hardware does not send it by itself (bit 17), and there is no sense.
    if ((ata & (1u << 28)) && !(ata & (1u << 17)) && !(senseCode & 0x00FFFFFF)) {
        if (held & 0x00FFFFFF) {                        // blocks pending
            if (gd.hits.why != GD_END_NEW) {            //   and no new command: the decoder stops
                DECODER_TRIGGER = 0x101;
                kind = 2;
            }
        } else if (gd.hits.why == GD_END_IDLE)          // no blocks pending, 1 second idle: the status was lost
            kind = 1;
    }
    if (!kind)
        return;
    cpsr = InterruptsOff(0xC0);
    // The same test with the interrupts off. The status can have gone out since then, or a new command can have
    // come (bit 8). Then there is nothing to end.
    if (!(REG32(HOST_CONTROL + 0xC) & (1u << 28)) || (HOST_EVENTS & (1u << 8))) {
        InterruptsRestore(cpsr);
        return;
    }
    REG32(HOST_CONTROL + 0xC) &= ~(1u << 28);           // no status when the transfer ends
    TransferReset(0);
    TRANSFER_COUNT = 0;
    TRANSFER_LENGTH &= 0xFF000000;                      // no blocks pending
    if (kind == 2)
        ReturnSense(0x03, 0x02, 0x00);
    else
        ReturnSense(0x04, 0x08, 0x80);
    if (kind == 2 && !(cdb[10] & 0x10) && cdb[9] == 1)  // a program request of one block: ReadGDRaw reads it again
        gd.runAgain = 1;
    else {
        // the status of the error goes to the host, in the same steps as the stock SATA error path
        HostCommandClear();
        REG32(HOST_CONTROL + 0x10) |= 8;
        AtaStatusSet(1, 1);                             // the ATA status: an error, with the sense key
        REG32(HOST_CONTROL + 4) &= 0xFFFF0000;          // the byte count 0
        HostReasonSet(3);
        if ((REG32(HOST_CONTROL + 4) >> 24) == 0xA0)    // the command is a PACKET command
            HostStatusSend();
    }
    hostCommand[0] = hostCommand[1] = hostCommand[2] = hostCommand[3] = 0;
    commandActive = 0;
    decoderFlags = 0;
    InterruptsRestore(cpsr);
}

/*
 * Function Name:  GdReadAheadKeep
 *
 * Decide if the read-ahead continues after a cache hit.
 *
 * It continues while the host keeps requesting. It ends after 1 second with no
 * request, when the buffer holds its size minus 64 unsent blocks, or after
 * GdReadAheadEnd.
 *
 * Preconditions:   The buffer served a request in this read.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  1 while the read-ahead is continuing, else 0.
 *
 * Notes: the 1 second is the bound of the read loop in a dump. If somehow the
 * host pauses the read under 1 second, it doesn't end the read-ahead.
 */
static int GdReadAheadKeep(void) {
    DWORD now = RTC_COUNT;
    if (!gd.hits.why) {
        if (now - gd.hits.lastHit >= GD_HIT_IDLE)       // no request for 1 second
            gd.hits.why = GD_END_IDLE;
        else {
            gd.ref.time = now;                          // the reference stays fresh: a stream needs no new one
            if (BUFFER_UNSENT16 < bufferBlocks - 0x40u) {   // the buffer has room for more than 64 blocks
                gd.budgetStart = now;                   // the time budget stays fresh too
                return 1;
            }
            gd.hits.why = GD_END_OWED;
        }
    }
    GdWaitingCommandEnd();                              // end a command that still waits for its blocks
    gd.hits.served = 0;
    return 0;
}

/*
 * Function Name:  GdCacheServe
 *
 * Serve one request from the buffer, if the buffer holds the requested blocks.
 *
 * The stock cache hit check gets the address and the length of the request. A
 * program read gives it forceUnitAccess 0, so that a read that started with
 * FUA can also serve the next requests.
 *
 * On a cache hit, the transfer starts. New values stay, as in the stock read
 * command. On a miss the old values come back. A request 1 to 32 blocks after
 * the unsent blocks then waits, and the next pass asks again. Any other miss
 * ends the cache hits.
 *
 * Preconditions:   cdb[] holds a C0 03 with a length of 1 to 16.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdCacheServe(void) {
    DWORD fad = GdCdbLba() + 150, blocks = cdb[9], hit, distance;
    DWORD savedStart = startAddress, savedLength = transferLength;
    BYTE fua = forceUnitAccess;
    startAddress = fad;                                 // the stock check reads the request from these two
    transferLength = blocks;
    if (!gd.leadIn)
        forceUnitAccess = 0;
    hit = CacheHitTest();
    forceUnitAccess = fua;
    if (hit && GdHitCheck(fad)) {
        GdHostTransfer(blocks);
        gd.hits.lastHit = RTC_COUNT;
        gd.hits.served = 1;
        return;
    }
    startAddress = savedStart;
    transferLength = savedLength;
    if (hit)                                            // GdHitCheck refused the block and ended the cache hits
        return;
    distance = fad - (DATA_PATH_LBA & 0x0FFFFFFF) - BUFFER_UNSENT16;   // how far after the unsent blocks
    if (distance - 1 > 0x1F)                            // not 1 to 32 blocks after them
        GdReadAheadEnd(GD_END_NEW);
}

/*
 * Function Name:  GdNewCommand
 *
 * Handles a new command that arrives during a read-ahead.
 *
 * For some reason, Windows can send TEST UNIT READY and GET EVENT STATUS
 * NOTIFICATION while the drive is working, and this can cause the drive to
 * seek. So I'm handling some of it here and leaving the rest to the stock
 * handler.
 *
 * Preconditions:   The read-ahead is running and a new command awaits.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 *
 * Notes: GD_OUTLINE keeps its locals out of the frame of GdLoopBottom, because
 * the deepest calls of the read loop (the address search) run under that frame.
 */
GD_OUTLINE void GdNewCommand(void) {
    if (cdb[0] == 0x4A || cdb[0] == 0x00) {             // GET EVENT STATUS NOTIFICATION, TEST UNIT READY
        if (GdCommandTake(cdb[0])) {
            BYTE active = readActive;
            readActive = active | 1;                    // the stock handler answers only while bit 0 is set
            ReadCommandHandler();
            readActive = active;
        }
        return;                                         // the read-ahead continues
    }
    // A C0 03 in the form of this read, for 1 to 16 blocks, goes to GdCacheServe. Any other command ends the cache
    // hits, and it runs as a new command after the read loop.
    if (cdb[0] != 0xC0 || cdb[1] != 0x03 || cdb[10] != (gd.leadIn ? 0x15 : 0x05) || (BYTE)DISC_SECTOR_REGISTER != 0x13)
        GdReadAheadEnd(GD_END_NEW);
    else {
        if (!gd.hits.why) {
            if (!(cdb[6] | cdb[7] | cdb[8]) && (DWORD)(cdb[9] - 1) <= 0xF) {
                GdCacheServe();
                return;
            }
            GdReadAheadEnd(GD_END_NEW);
        }
    }
    ReadCommandHandler();
}
