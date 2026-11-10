// gd_seek.c
//
// These functions move the laser to the sector that a read asks for. They
// decide if the read needs a seek, measure the reference position of the sled,
// calculate the sled position of the target from a model of the disc, make the
// coarse seek, find where the laser landed, and jump back to just before the
// target. They also keep the sled inside a limit during a stock seek.

#include "gd.h"

static void GdReference(void);
static int GdJumpToTarget(void);

/*
 * Function Name:  GdVeneerSet
 *
 * Change one veneer word of the stock seek, if it holds the expected value.
 *
 * The stock CD seek calls its helpers through words in RAM (veneers). A new
 * value in such a word redirects the call, and no stock code changes.
 *
 * Parameters:
 *        volatile DWORD *word The veneer word.
 *        DWORD from           The value that it must hold.
 *        DWORD to             The new value.
 *
 * Returns:     Nothing.
 */
static void GdVeneerSet(volatile DWORD *word, DWORD from, DWORD to) {
    if (*word == from)
        *word = to;
}

/*
 * Function Name:  GdSeekLimitOn
 *
 * Turn on the sled limit of the stock seek.
 *
 * The sled carries the laser unit across the disc. While the limit is on, the
 * stock CD seek cannot move the sled past position 416 (GdSeekTimeTest,
 * GdSeekStep), and the first two read errors of the seek get a new try of the
 * step in place of the stock recovery (GdSeekRecover).
 *
 * A stock seek that goes wrong can run the sled far out, onto the unwritten
 * dye of a GD-R, where the laser must not read.
 *
 * Preconditions:   gd.hdRead is 0: a stock seek is about to run.
 * Postconditions:  GdSeekLimitOff must follow before the command ends.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdSeekLimitOn(void) {                       // (a function address has bit 0 set: Thumb)
    gd.limitWaits = 0;
    GdVeneerSet(&timeBelowVeneer, (DWORD)StockTimeBelow, (DWORD)GdSeekTimeTest);
    GdVeneerSet(&seekStepVeneer, (DWORD)StockSeekStep, (DWORD)GdSeekStep);
    GdVeneerSet(&seekRecoverVeneer, (DWORD)StockSeekRecover, (DWORD)GdSeekRecover);
}

/*
 * Function Name:  GdSeekLimitOff
 *
 * Turn off the sled limit: the three veneer words get their stock values back.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdSeekLimitOff(void) {
    GdVeneerSet(&timeBelowVeneer, (DWORD)GdSeekTimeTest, (DWORD)StockTimeBelow);
    GdVeneerSet(&seekStepVeneer, (DWORD)GdSeekStep, (DWORD)StockSeekStep);
    GdVeneerSet(&seekRecoverVeneer, (DWORD)GdSeekRecover, (DWORD)StockSeekRecover);
}

/*
 * Function Name:  GdSeekLimitCheck
 *
 * Move the sled back to position 16 if it has reached the limit, position 416.
 *
 * The limit lies well above the highest position of a good seek, and well
 * below the positions that a seek that runs away reaches.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdSeekLimitCheck(void) {
    if (GdSledPosition() >= GD_SEEK_LIMIT)
        SledMove(0x10);
}

/*
 * Function Name:  GdSeekTimeTest
 *
 * Run the stock time test, and check the sled limit if the seek loop called it.
 *
 * Parameters:
 *        DWORD r0, r1, r2, r3 The registers of the call. They go to the stock
 *                             test as they are.
 *
 * Returns:
 *        DWORD                The result of the stock test.
 */
DWORD GdSeekTimeTest(DWORD r0, DWORD r1, DWORD r2, DWORD r3) {
    DWORD caller = (DWORD)__builtin_return_address(0), result = TimeBelow(r0, r1, r2, r3);
    if (caller == (DWORD)seekLoopReturn)                // 56 places use this test: only the seek loop needs the limit
        GdSeekLimitCheck();
    return result;
}

/*
 * Function Name:  GdSeekStep
 *
 * Run the stock seek step, then check the sled limit.
 *
 * Parameters:
 *        DWORD r0, r1, r2, r3 The registers of the call. They go to the stock
 *                             step as they are.
 *
 * Returns:
 *        DWORD                The result of the stock step.
 */
DWORD GdSeekStep(DWORD r0, DWORD r1, DWORD r2, DWORD r3) {
    DWORD result = SeekStep(r0, r1, r2, r3);
    GdSeekLimitCheck();
    return result;
}

/*
 * Function Name:  GdSeekRecover
 *
 * Retry a failed seek step twice before the stock recovery may run.
 *
 * The stock recovery moves the sled far, again and again, and the read then
 * takes about 9 seconds. A new try of the step starts from where the laser is.
 *
 * Preconditions:   GdSeekLimitOn has cleared gd.limitWaits.
 *
 * Parameters:
 *        DWORD status         The status of the failed step.
 *        DWORD r1, r2, r3     The other registers of the call. They go to the
 *                             stock recovery as they are.
 *
 * Returns:
 *        DWORD                0 for a retry, else the result of the recovery.
 *
 * Notes: the wait also ends after 67,108,864 tests, so that it ends even if the
 * clock register stops.
 */
DWORD GdSeekRecover(DWORD status, DWORD r1, DWORD r2, DWORD r3) {
    DWORD caller = (DWORD)__builtin_return_address(0);
    // The first two read errors (3 or 0x18) of the seek: we return 0, and the stock seek runs its step again.
    if (caller == (DWORD)seekRecoverReturn && (status == 3 || status == 0x18) && gd.limitWaits < 2) {
        // Before the first retry, with focus and the FG below 20, we wait up to 0.5 seconds for the spindle. Right
        // after a spin-up the data PLL cannot lock yet.
        if (++gd.limitWaits == 1 && FOCUS_OK() && SPINDLE_FG() < 20) {
            DWORD start = RTC_COUNT, tests = 1u << 26;
            while (RTC_COUNT - start < 500 * TICKS_MS && SPINDLE_FG() < 20 && --tests)
                ;
        }
        return 0;
    }
    return SeekRecover(status, r1, r2, r3);
}

/*
 * Function Name:  GdCacheHit
 *
 * Make the tests of the stock cache hit test, with no writes.
 *
 * The stock test also writes, and on a hit it moves the transfer. We need the
 * answer before we decide on a seek, when nothing may move yet.
 *
 * Preconditions:   forceUnitAccess is 0 (GdReadStart sets it).
 *
 * Parameters:
 *        DWORD *dist          OUT: the request minus the first unsent LBA, in
 *                             blocks. It can be negative.
 *
 * Returns:
 *        DWORD  1             All of the request lies in the unsent blocks.
 *        DWORD  2             A part of it does.
 *        DWORD  0             A miss.
 */
static DWORD GdCacheHit(DWORD *dist) {
    DWORD target = DATA_PATH_LBA & 0x0FFFFFFF, start = startAddress, unsent = BUFFER_UNSENT, len = transferLength;
    SDWORD distance = (SDWORD)(start - target);
    DWORD c2Form = (DECODER_MODE >> 1) & 1, unsentCount;
    *dist = (DWORD)distance;
    if (cdb[10] == 4 ? !c2Form : c2Form)                // the C2 form of the decoder must be the one of the request
        return 0;
    if ((cdb[11] == 3) != (forceUnitAccess != 0))       // as the stock test (the meaning of cdb[11] = 3 is not known)
        return 0;
    if (((unsent >> 8) & 0xFF) == 0xFF)                 // the count of unsent blocks is negative
        return 0;
    unsentCount = unsent & 0xFFFF;
    if (unsentCount == 0 && !(DECODER_STATUS & 0x02))   // none unsent; bit 1 as the stock test (its use is not known)
        return 0;
    if (hitTestMode == 1 && distance < 0)               // the stock mode 1: below the first unsent block
        return 0;
    if (hitTestStart > start || distance < -32 ||       // below the stock bound, more than 32 back,
        distance > (SDWORD)unsentCount)                 //   or past the end
        return 0;
    if ((unsentCount & 0x8000) || distance < 0 || (WORD)len == 0 || (DWORD)distance + (WORD)len > unsentCount)
        return 2;
    return 1;
}

/*
 * Function Name:  GdCacheTest
 *
 * Tell if the buffer holds the request, by looking at the block itself.
 *
 * The counters of GdCacheHit can say yes when the block holds another sector.
 *
 * Preconditions:   startAddress and transferLength hold the request.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD  1             The next blocks hold the request.
 *        DWORD  2             A partial hit, or the block holds another sector.
 *        DWORD  0             A miss.
 */
static DWORD GdCacheTest(void) {
    DWORD distance, result = GdCacheHit(&distance), block, total, fad;
    if (result != 1)
        return result;
    block = (BUFFER_READ_BLOCK & 0x1FFF) + distance;    // the block that the host reads next, + the distance
    total = bufferBlocks;
    if ((SDWORD)block >= (SDWORD)total)                 // around the ring
        block -= total;
    else if ((SDWORD)block < 0)
        block += total;
    if (block >= total)
        return 2;
    fad = GdBlockFad(block);
    return (fad == 0 || fad == startAddress) ? 1 : 2;   // a block with no readable address counts as a hit
}

/*
 * Function Name:  GdReadStart
 *
 * Decide if the request needs a seek, and find the reference if it does.
 *
 * A seek takes about a second. When the host is slower than the disc, the next
 * request is already in the buffer, and the read takes it from there.
 *
 * Preconditions:   startAddress and transferLength hold the request.
 * Postconditions:  gd.noSeek says if the request needs no seek. gd.ref.failed
 *                  says if the read has no reference.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
GD_OUTLINE void GdReadStart(void) {
    gd.noSeek = 0;
    if (!(cdb[1] & 0x08)) {                             // no FUA bit: look in the buffer first
        DWORD result;
        forceUnitAccess = 0;
        result = (powerState & 0x02) ? 2 : GdCacheTest();   // a sleeping drive reads from the disc
        if (result == 1) {                              // the next blocks hold the request: no seek
            gd.noSeek = 1;
            gd.ref.failed = 0;
            refillDone = 1;
            return;
        }
        if (result == 2)                                // there in a form that we cannot serve: read the disc
            forceUnitAccess = 0xFF;
    }
    GdReference();
}

/*
 * Function Name:  GdSeekReference
 *
 * Make one stock seek to LBA 9,000.
 *
 * We write an internal seek into cdb[] and call the stock seek handler. Our
 * hooks stay out, because cdb[] no longer holds a C0 03. LBA 9,000 lies in the
 * written area of a GD-R.
 *
 * Preconditions:   The caller has kept cdb[], senseCode and hdMinuteMap.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD  0             A good seek.
 *        DWORD  1             The player is not ready.
 *        DWORD  4             The seek left a sense.
 *
 * Notes: after a disc load the handler asks for speed 10 (the stock wake), so
 * the first measure leaves the drive above the speed cap.
 */
static DWORD GdSeekReference(void) {
    int i;
    cdb[0] = 0xEB;                                      // an internal seek: the handler skips its steps with the host
    for (i = 1; i < 12; i++)
        cdb[i] = 0;
    cdb[4] = 9000 >> 8;
    cdb[5] = 9000 & 0xFF;
    senseCode = 0;
    hdMinuteMap &= ~0x03;                               // LBA 9,000 lies in the single-density area
    Seek10Command();
    if (senseCode)
        return 4;
    return playerStatus ? 1 : 0;
}

/*
 * Function Name:  GdReferenceMeasure
 *
 * Read the sled position at LBA 9,000, after it has settled.
 *
 * After a long seek the sled lags the lens, and it catches up in under a
 * second. So we read the position up to 4 times, until two readings in a row
 * agree within GdReferenceTolerance counts (5 in the BU40N firmware).
 *
 * Preconditions:   As GdSeekReference.
 *
 * Parameters:
 *        SDWORD *ref          OUT: the position, when the result is 0.
 *
 * Returns:
 *        DWORD  0             *ref holds the position.
 *        DWORD  2             The positions did not agree in 4 readings.
 *        DWORD  other         The result of GdSeekReference.
 *
 * Notes: each wait also ends after 16,777,216 tests, in case the clock
 * register stops.
 */
static DWORD GdReferenceMeasure(SDWORD *ref) {
    SDWORD last = 0x7FFF, position;
    DWORD result, round, start, tests;
    if ((result = GdSeekReference()) != 0)
        return result;
    for (round = 1; ; round++) {
        start = RTC_COUNT;
        for (tests = 1u << 24; RTC_COUNT - start < 250 * TICKS_MS && --tests; )   // 250 ms, then the seek again
            ;
        if ((result = GdSeekReference()) != 0)
            return result;
        position = GdSledPosition();
        if (position - last <= GdReferenceTolerance && last - position <= GdReferenceTolerance) {
            *ref = position;
            return 0;
        }
        last = position;
        if (round >= 4)
            return 2;
    }
}

/*
 * Function Name:  GdReferenceRead
 *
 * Measure the reference, with the protections of a stock seek on.
 *
 * Preconditions:   gd.hdRead is 0, so our hooks stay out of the stock seek.
 * Postconditions:  The sled limit is off, and mediaType shows a CD-ROM again.
 *
 * Parameters:
 *        SDWORD *ref          OUT: the reference, when the result is 0.
 *
 * Returns:
 *        DWORD  0             *ref holds the reference.
 *        DWORD  1             No measure ran, or the player was not ready.
 *        DWORD  2             The positions never agreed.
 *        DWORD  4             The stock seek left a sense.
 */
static DWORD GdReferenceRead(SDWORD *ref) {
    DWORD result;
    GdSeekLimitOn();
    mediaType |= gd.recordable;                         // the stock seek fails on a CD-R that shows as a CD-ROM
    // A program read with no focus on an active drive gets no measure: such a stock seek takes about 9 seconds, and
    // a few of them take longer than the host waits.
    if (gd.targetFad < 600000 && !FOCUS_OK() && !(powerState & 0x0F))
        result = 1;
    else
        result = GdReferenceMeasure(ref);
    mediaType &= ~gd.recordable;
    GdSeekLimitOff();
    if (result == 0)
        gd.ref.pll = dataPllPreset;                     // the PLL word that the stock seek left
    return result;
}

/*
 * Function Name:  GdFitReset
 *
 * Start a new fit of the reference and the density (GdFitAdd). The density of
 * the old fit stays in use until the new fit has a landing.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdFitReset(void) {
    gd.fit.count = 0;
    gd.fit.rejects = 0;
    gd.fit.sumEffect = gd.fit.sumMeasured = gd.fit.sumProduct = 0;
    gd.fit.sumEffectSquared = 0;
    gd.check.density = (SBYTE)((gd.fit.slope + 2) >> 2);
}

/*
 * Function Name:  GdReferenceChoose
 *
 * Decide if this read can use the fit, or needs a new measure.
 *
 * A measure takes at least 500 ms, so a program read uses the fit when it can.
 * A lead-in read always gets a measure, and so does the read after a start of
 * the disc.
 *
 * Preconditions:   gd.readGap holds the time with no read.
 * Postconditions:  gd.ref.time and gd.ref.checks describe this reference.
 *
 * Parameters:
 *        SDWORD *out          OUT: the reference, when the result is 0.
 *
 * Returns:
 *        DWORD  0             *out holds the reference.
 *        DWORD  other         The reason of GdReferenceRead: no reference.
 */
static DWORD GdReferenceChoose(SDWORD *out) {
    DWORD now = RTC_COUNT, age = now - gd.ref.time;
    // A recent fit: one position check since the last reference, the reference under 30 seconds old (below the 32
    // seconds of the standby), and the last read under 400 ms ago. A start of the disc takes longer than 400 ms,
    // and it leaves a state that only a stock seek repairs.
    int recent = (BYTE)(gd.check.count - gd.ref.checks) == 1 && age < GD_REF_AGE && gd.readGap < GD_IDLE_TICKS;
    gd.ref.time = now;
    gd.ref.checks = gd.check.count;
    if (gd.targetFad >= 600000)                         // a lead-in read: GdReference has cleared gd.leadIn
        return GdReferenceRead(out);
    if (gd.fit.count && gd.ref.clockFixes != gd.fit.starts)
        GdFitReset();                                   // the disc was started: a new fit
    else if (gd.fit.count && recent && !(powerState & 0x0F)) {   // a fit with a landing, and an active drive
        if (gd.ref.pll)
            dataPllPreset = gd.ref.pll;                 // the data clock preset of the check starts from it
        *out = gd.fit.ref;
        return 0;
    }
    return GdReferenceRead(out);
}

/*
 * Function Name:  GdReference
 *
 * Get the reference position of this read.
 *
 * The reference is the sled position at LBA 9,000. A seek target for any
 * address is the position from the model plus this value. A reference outside
 * GdReferenceLow..GdReferenceHigh (20..160 in the BU40N firmware) is a failed
 * measure, and the read ends with 03/02/00.
 *
 * Preconditions:   The read flags are set. gd.readGap holds the idle time.
 * Postconditions:  gd.ref.value and gd.ref.failed are set. All that we kept is
 *                  back in place.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdReference(void) {
    // A measure runs a stock seek inside our command, and the stock code writes over cdb[], the request, the sense
    // and the minute map. We keep them, and put them back at the end.
    BYTE savedCdb[12], map = hdMinuteMap, leadIn = gd.leadIn;
    DWORD savedStart = startAddress, savedLength = transferLength, sense = senseCode;
    SDWORD ref = 0;
    int i;
    for (i = 0; i < 12; i++)
        savedCdb[i] = cdb[i];
    gd.hdRead = 0;                                      // our hooks stay out: the stock seek runs as stock
    gd.leadIn = 0;
    gd.ref.failed = 1;
    if (GdReferenceChoose(&ref) == 0) {
        gd.ref.value = (SWORD)ref;
        if (ref >= GdReferenceLow && ref <= GdReferenceHigh)
            gd.ref.failed = 0;
    }
    for (i = 0; i < 12; i++)
        cdb[i] = savedCdb[i];
    startAddress = savedStart;
    transferLength = savedLength;
    senseCode = sense;
    hdMinuteMap = map;
    gd.hdRead = 1;
    gd.leadIn = leadIn;
}

/*
 * Function Name:  GdIsqrt
 *
 * Get the integer square root of a number, rounded down.
 *
 * Parameters:
 *        DWORD value          The number.
 *
 * Returns:
 *        DWORD                The root.
 */
static DWORD GdIsqrt(DWORD value) {
    DWORD root = 0, bit = 1u << 30;
    while (bit > value)
        bit >>= 2;
    while (bit) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else
            root >>= 1;
        bit >>= 2;
    }
    return root;
}

/*
 * Function Name:  GdSignedDivide
 *
 * Divide two signed numbers with the stock unsigned divide, rounding to 0.
 *
 * Parameters:
 *        SDWORD number        The number.
 *        SDWORD divisor       The divisor.
 *
 * Returns:
 *        SDWORD               The result.
 */
static SDWORD GdSignedDivide(SDWORD number, SDWORD divisor) {
    DWORD quotient = UnsignedDivide(GdAbs(number), GdAbs(divisor));
    return ((number ^ divisor) < 0) ? -(SDWORD)quotient : (SDWORD)quotient;
}

/*
 * Function Name:  GdRoundDivide
 *
 * Divide a signed number by a positive one, rounding to the nearest.
 *
 * Parameters:
 *        SDWORD number        The number.
 *        DWORD divisor        The divisor.
 *
 * Returns:
 *        SDWORD               The result.
 */
SDWORD GdRoundDivide(SDWORD number, DWORD divisor) {
    DWORD quotient = UnsignedDivide(GdAbs(number) + divisor / 2, divisor);
    return number < 0 ? -(SDWORD)quotient : (SDWORD)quotient;
}

/*
 * Function Name:  GdModelPosition
 *
 * Work out the sled position of an address from the layout of the disc.
 *
 * The track is one long spiral, and each frame takes the same area, so:
 *
 *   radius in um = square root of
 *                  (fad - 38,475) x (4,682 + 4 x density) + 961,000,000
 *
 * The firmware gives the size of one position and the radius of position 0
 * (GdModelScale, GdModelOrigin: 7.8125 um and 26,561 um in the
 * BU40N firmware). The caller adds the reference, which it measures on the
 * drive.
 *
 * Parameters:
 *        DWORD fad            The address, limited to 45,150..556,000.
 *        SDWORD density       The correction of this disc (0: the model).
 *        DWORD *radiusOut     OUT: the radius in um. NULL when not wanted.
 *
 * Returns:
 *        DWORD                The sled position, before the reference.
 */
static DWORD GdModelPosition(DWORD fad, SDWORD density, DWORD *radiusOut) {
    DWORD radius;
    if (fad < 45150)
        fad = 45150;
    if (fad > 556000)
        fad = 556000;
    radius = GdIsqrt((fad - 38475) * (DWORD)(4682 + 4 * density) + 961000000u);
    if (radiusOut)
        *radiusOut = radius;
    return ((radius - GdModelOrigin) * GdModelScale) >> 16;
}

/*
 * Function Name:  GdFitAdd
 *
 * Learn the reference and the density of this disc from one landing.
 *
 * A landing is where a coarse seek put the laser. It gives the sled position
 * and the address that the laser read there. We fit a straight line through
 * the last 32 landings or so. Its offset is the reference, and its slope is the
 * density: how far the spiral of this disc differs from the model. With the
 * fit, most reads need no stock seek to measure the reference.
 *
 * The first landing of a new fit can lie far out and give a reference outside
 * its range. Then the density of this disc differs much from the model. That
 * read measured its reference, so the landing gives the density, and the fit
 * keeps the measured reference.
 *
 * Preconditions:   gd.check.fad holds the address of the landing.
 * Postconditions:  gd.check.density holds the density of the fit.
 *
 * Parameters:
 *        SDWORD position      The sled position at the landing.
 *        SDWORD lens          The lens shift at the landing.
 *
 * Returns:     Nothing.
 */
static void GdFitAdd(SDWORD position, SDWORD lens) {
    struct GdFit *fit = &gd.fit;
    SDWORD measured, predicted, effect, window, learned;
    DWORD fad = gd.check.fad, radius, frames;
    if (lens > 1024 || lens < -1024)                    // the lens far from its centre: no use
        goto Done;
    measured = position - (SDWORD)GdModelPosition(fad, 0, &radius);   // the sled position minus the model at density 0
    frames = (fad < 45150 ? 45150 : fad > 556000 ? 556000 : fad) - 38475;
    // how far a change of the density moves this address
    effect = (SDWORD)UnsignedDivide(frames << 8, (radius * GdSledStepX16) >> 5);
    if (fit->count == 0) {                              // the first landing: a window of 96 counts
        predicted = ((gd.check.density * 4 * effect) >> 10) + gd.ref.value;
        window = 96;
    } else {                                            // then 24 counts
        predicted = ((fit->slope * effect) >> 10) + fit->ref;
        window = 24;
    }
    if (measured - predicted > window || predicted - measured > window) {   // outside the window: no use
        if (fit->count && ++fit->rejects >= 2)          // two in a row start a new fit
            GdFitReset();
        goto Done;
    }
    fit->rejects = 0;
    if (fit->count == 0) {
        fit->prior = (SWORD)(gd.check.density * 4);
        fit->starts = gd.ref.clockFixes;
    }
    if (fit->count >= 32) {                             // the older landings count half
        fit->count >>= 1;
        fit->sumEffect >>= 1;
        fit->sumMeasured >>= 1;
        fit->sumEffectSquared >>= 1;
        fit->sumProduct >>= 1;
    }
    fit->count++;
    fit->sumEffect += effect;
    fit->sumMeasured += measured;
    fit->sumEffectSquared += (DWORD)(effect * effect);
    fit->sumProduct += effect * measured;
    {
        DWORD squares = fit->sumEffectSquared - UnsignedDivide((DWORD)(fit->sumEffect * fit->sumEffect), fit->count);
        SDWORD products = fit->sumProduct - GdSignedDivide(fit->sumEffect * fit->sumMeasured, fit->count), slope, ref;
        SDWORD divisor = (SDWORD)(squares + GD_FIT_LAMBDA) >> 4;   // the weight holds the density near its last value
        slope = GdSignedDivide((products << 6) + ((fit->prior * GD_FIT_LAMBDA) >> 4), divisor);
        if (slope > 128)
            slope = 128;
        if (slope < -256)
            slope = -256;
        ref = GdSignedDivide(fit->sumMeasured - GdSignedDivide(slope * fit->sumEffect, 1024), fit->count);
        // The first landing of a new fit, far out: the density that puts it on the measured reference of this read
        if ((ref < GdReferenceLow || ref > GdReferenceHigh) && fit->count == 1 && frames >= GD_FIT_FAR_FRAMES) {
            learned = GdSignedDivide((measured - gd.ref.value) * 1024, effect);
            if (learned <= 128 && learned >= -256) {
                fit->prior = (SWORD)learned;
                slope = learned;
                ref = gd.ref.value;
            }
        }
        if (ref < GdReferenceLow || ref > GdReferenceHigh) {   // a reference outside its range starts a new fit
            GdFitReset();
            goto Done;
        }
        fit->ref = (SWORD)ref;
        fit->slope = (SWORD)slope;
    }
Done:
    if (fit->count)
        gd.check.density = (SBYTE)((fit->slope + 2) >> 2);
}

/*
 * Function Name:  GdLeadInLimit
 *
 * Keep a lead-in seek target inside its range.
 *
 * The range is reference + 469 - the raise .. reference + GdLeadInAimHigh, and
 * GdLeadInAimMax at most (640 and 760 in the BU40N firmware). The raise lowers
 * the inner bound by up to 4 when the first Q lands above the target.
 *
 * Preconditions:   gd.ref.value lies in 20..160.
 *
 * Parameters:
 *        SDWORD target        The wanted sled position.
 *
 * Returns:
 *        SDWORD               The sled position to use.
 */
SDWORD GdLeadInLimit(SDWORD target) {
    SDWORD lowest = gd.ref.value + 469 - gd.attempt.raise;   // a higher bound made the first lead-in reads slow
    if (target < lowest)
        target = lowest;
    if (target > gd.ref.value + GdLeadInAimHigh)
        target = gd.ref.value + GdLeadInAimHigh;
    if (target > GdLeadInAimMax)
        target = GdLeadInAimMax;
    return target;
}

/*
 * Function Name:  GdSeekTarget
 *
 * Work out the sled position of the coarse seek.
 *
 * A coarse seek moves the sled to the area of the target. Track jumps then
 * bring the laser to the target (GdPositionCheck).
 *
 * Preconditions:   gd.ref and gd.check.density are set for this read.
 * Postconditions:  gd.check.radius holds the radius of a program target.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD                The sled position.
 */
DWORD GdSeekTarget(void) {
    DWORD radius, target;
    if (GdInProgramRead())
        GdFocusOn();                                    // a sled move with the focus off is often slow
    if (gd.ref.failed)
        return GdServoRead(0x300);                      // no reference: the position now, so the sled does not move
    if (gd.leadIn) {
        // A rule measured on the drive: reference + GdLeadInOffset is FAD GdLeadInLandingFad, and one position is
        // GdLeadInScale frames (567, 675,000 and 102 in the BU40N firmware).
        gd.attempt.count = 0;
        gd.attempt.lastFirstQ = 0;
        gd.attempt.target = (WORD)GdLeadInLimit(gd.ref.value + GdLeadInOffset +
                                                GdRoundDivide((SDWORD)gd.targetFad - GdLeadInMargin -
                                                              GdLeadInLandingFad, GdLeadInScale));
        return gd.attempt.target;
    }
    // The model's position of the target + 500 frames, plus the reference. The 500 frames put the landing past the
    // target, because the track jump only goes back.
    target = GdModelPosition(gd.targetFad + 500, gd.check.density, &radius) + (WORD)gd.ref.value;
    gd.check.radius = (WORD)radius;
    return target > GdProgramSeekMax ? GdProgramSeekMax : target;
}

/*
 * Function Name:  GdSeekTracking
 *
 * Pull in the tracking after a coarse seek.
 *
 * In a program read, a pull-in that reports success can still slide. Then we
 * make one more pull-in.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdSeekTracking(void) {
    SDWORD lens;
    DWORD result;
    TimerWait(30);
    if (!FOCUS_OK())
        return;
    result = GdTrackingOn();
    if (!GdInProgramRead() || result != 0)
        return;
    lens = GdLensShift();
    if (GdAbs(lens) > 2000 || !SEEK_OK())               // a slide: the lens shift over 2,000, or seek-OK off
        GdPullInAgain();
}

/*
 * Function Name:  GdSeekClose
 *
 * Bring the servos back after the sled move of a seek.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdSeekClose(void) {
    DWORD band;
    GdSeekDoneWait();
    GdFocusOn();
    band = GdServoRead(0x300) >> 4;
    // The pull-in only with the sled in the high-density band (position / 16 from 30 to GdTrackingBandHigh, 260 in
    // the BU40N firmware): a sled that did not reach the target must not pull in on a track elsewhere.
    if (band >= 30 && band <= GdTrackingBandHigh)
        GdSeekTracking();
}

/*
 * Function Name:  GdQAfterSeek
 *
 * Ignore a Q that is left over from before the seek or the jump.
 *
 * The Q register keeps its last value until a new Q comes, so a landing could
 * take the Q of the read before as its own.
 *
 * Preconditions:   gd.check.seekQ holds the Q at the seek, or 0.
 *
 * Parameters:
 *        DWORD fad            The address in the Q.
 *
 * Returns:
 *        DWORD                fad, or 0 when it is a leftover.
 */
static DWORD GdQAfterSeek(DWORD fad) {
    if (fad && gd.check.seekQ) {
        if (fad - gd.check.seekQ <= 40)                 // 0 to 40 frames above the Q at the seek: not moved yet
            return 0;
        gd.check.seekQ = 0;
    }
    return fad;
}

/*
 * Function Name:  GdQPair
 *
 * Test one new Q: does it make a pair with the Q before it?
 *
 * A pair is two Qs 1 to 40 frames apart. One Q alone can be wrong or old.
 *
 * Parameters:
 *        DWORD fad            The address in the new Q, or 0.
 *        DWORD *last          IN, OUT: the last Q. At a pair, the position.
 *
 * Returns:
 *        int                  1 at a pair.
 */
static int GdQPair(DWORD fad, DWORD *last) {
    if (!fad || fad == *last)
        return 0;
    if (fad - *last <= 40)
        return 1;
    *last = fad;
    return 0;
}

struct GdQWait {
    DWORD start, lastQ;                                 // the start of the wait; the last program Q
};

/*
 * Function Name:  GdQWaitRestart
 *
 * Start the wait for a pair from now.
 *
 * Parameters:
 *        struct GdQWait *wait The state of the wait.
 *
 * Returns:     Nothing.
 */
static void GdQWaitRestart(struct GdQWait *wait) {
    wait->start = RTC_COUNT;
    wait->lastQ = 0;
    gd.check.leadInQTime = 0;
}

/*
 * Function Name:  GdQWaitNext
 *
 * Get the next program Q of the wait, or end the wait if the laser slides.
 *
 * Parameters:
 *        struct GdQWait *wait The state of the wait.
 *
 * Returns:
 *        DWORD                The FAD of a new Q, or 0.
 */
static DWORD GdQWaitNext(struct GdQWait *wait) {
    DWORD servo;
    // a slide: the lens shift over 4,000 with the tracking drive over its limit
    if (GdAbs(GdLensShift()) > 4000 && ((servo = SERVO_STATUS) & SERVO_TRACK_DRIVE_HIGH)) {
        if (servo & SERVO_TRACKING_ON)
            GdTrackingOpen();
        wait->start = RTC_COUNT - (1u << 30);           // the wait ends
        gd.check.leadInQTime = 0;
        return 0;
    }
    return GdQAfterSeek(GdQProgramFad());
}

/*
 * Function Name:  GdQWaitTime
 *
 * Get the time of the wait so far, and refocus once if the focus has gone.
 *
 * For a target in the pregap of track 3, the time in the lead-in does not
 * count, because the laser can land there and play through it for up to
 * GdLeadInPlayTicks (about 7.9 seconds in the BU40N firmware).
 *
 * Parameters:
 *        struct GdQWait *wait The state of the wait.
 *
 * Returns:
 *        DWORD                The time in clock ticks. 0 after a refocus.
 */
static DWORD GdQWaitTime(struct GdQWait *wait) {
    DWORD now = RTC_COUNT, elapsed;
    if (gd.targetFad < 46150) {
        DWORD qChannel0 = Q_CHANNEL0;
        if (qChannel0 != gd.check.leadInQ) {
            gd.check.leadInQ = qChannel0;
            if ((qChannel0 >> 24) == 0xF1 && Q_CHANNEL0_LOW == 0x41 && now - wait->start < GdLeadInPlayTicks)
                gd.check.leadInQTime = now - wait->start;   // a data Q of the lead-in area with MIN 0xF1
        }
        elapsed = now - wait->start - gd.check.leadInQTime;
    } else
        elapsed = now - wait->start;
    if (gd.check.refocus != gd.check.count) {           // once in a check
        if (DATA_STATUS & (1u << 12))
            gd.check.refocus = gd.check.count;          // the data PLL locked: no refocus in this check
        else if (!FOCUS_OK() && coilCounter == 0) {     // no focus, and the coil protection does not count
            gd.check.refocus = gd.check.count;
            GdRefocus();
            GdQWaitRestart(wait);
            return 0;
        }
    }
    return elapsed;
}

/*
 * Function Name:  GdDataClockPreset
 *
 * Set the data PLL to the rate for the radius of the target.
 *
 * The rate comes from the radius and the stock preset word. Without this, a
 * GD-R read with the drive speed above the speed cap can have the focus and
 * the tracking but no PLL lock, and fail with 03/02/00. Why that state needs
 * the preset is not known.
 *
 * Preconditions:   gd.check.radius holds the radius of the target.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 *
 * Notes: do not remove this without a test on a GD-R.
 */
static void GdDataClockPreset(void) {
    DWORD rate, pll = dataPllPreset;
    if (!pll)
        return;
    rate = UnsignedDivide(22118400u << ((ANALOG_PLL >> 4) & 3), (pll >> 4) + 256);
    rate = (rate * ((gd.check.radius * 3411u) >> 18)) >> 16;
    if (rate)
        GdDataPllSet(rate);
}

/*
 * Function Name:  GdPositionCheck
 *
 * Find out where the coarse seek landed, then get just before the target.
 *
 * After a coarse seek the laser is on some track near the target, but we do not
 * know which one. We read the Q channel until two Qs arrive 1 to 40 frames
 * apart (a pair), for at most 300 ms. GdJumpToTarget then jumps back to just
 * before the target, and GdFitAdd learns from this landing.
 *
 * Preconditions:   The coarse seek has ended. gd.targetFad holds the target.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int  1               The laser is just before the target.
 *        int  0               The drive has answered 03/02/00.
 *
 * Notes: the caller starts the read loop after this function has returned, so
 * this stack frame does not sit below the read loop.
 */
GD_SYMBOL int GdPositionCheck(void) {
    struct GdQWait wait;
    DWORD fad;
    int found = 0;
    gd.check.count++;
    gd.check.seekQ = GdQProgramFad();
    GdDataClockPreset();
    GdQWaitRestart(&wait);
    for (;;) {
        fad = GdQWaitNext(&wait);
        if (GdQPair(fad, &wait.lastQ))
            break;
        if (GdQWaitTime(&wait) >= GD_CHECK_TICKS) {
            fad = 0;
            break;
        }
    }
    if (fad) {
        SDWORD position = (SDWORD)GdServoRead(0x300);   // the unsigned halfword, as the servo DSP gives it
        SDWORD lens = GdLensShift();
        gd.check.fad = wait.lastQ;
        found = GdJumpToTarget();
        GdFitAdd(position, lens);
    }
    gd.check.leadInQ = Q_CHANNEL0;
    // In the first 3 seconds of a command, a landing more than 1,000 frames below the target is refused: a new seek
    // gets there sooner than the spiral would.
    if (!found ||
        ((SDWORD)(gd.targetFad - gd.jump.fad) > GD_FAR_FRAMES && RTC_COUNT - gd.commandTime < GD_FAR_TICKS)) {
        ReturnSense(0x03, 0x02, 0x00);
        return 0;
    }
    return 1;
}

// The servo values that the stock track jump writes. A value of 0x1000 or more
// is a register at SERVO_BASE + the value. A smaller value is a servo DSP
// address.
const WORD GdServoSaveTable[18] = {
    0xB110, 0xB120, 0xB16C, 0xB1A0, 0xB1A4, 0xB1C4, 0xB1C8, 0xB380, 0xB3D4,
    0x3000, 0x4000, 0x022F, 0x032A, 0x0211, 0x0223, 0x0224, 0xB150, 0xB160 };

/*
 * Function Name:  GdServoSave
 *
 * Keep the servo values that the stock track jump changes, or put them back.
 *
 * The stock jump leaves its own values in the servo. On the ASUS firmware,
 * with no put-back, the next reference seek was slow, and the drive left the
 * bus.
 *
 * Preconditions:   A call with restore 1 follows a call with restore 0.
 *
 * Parameters:
 *        int restore          0: keep the values. 1: put them back.
 *
 * Returns:     Nothing.
 *
 * Notes: keep this function and its table global and not inlined (gd.h).
 */
GD_SYMBOL void GdServoSave(int restore) {
    const WORD *entry = GdServoSaveTable;               // (pointers: four values across the calls, a small frame)
    DWORD *saved = gd.jump.servo, value;
    for (; entry < GdServoSaveTable + 18; entry++, saved++) {
        value = (*entry >> 12) ? REG32(SERVO_BASE + *entry) : GdServoRead(*entry);
        if (!restore)
            *saved = value;
        else if (value != *saved) {                     // only a value that changed is written back
            if (*entry >> 12)
                REG32(SERVO_BASE + *entry) = *saved;
            else
                GdServoWrite(*entry, *saved);
        }
    }
}

/*
 * Function Name:  GdTrackJump
 *
 * Move the laser inward by a number of tracks, with the stock track jump.
 *
 * Parameters:
 *        DWORD tracks         The tracks, 2 to 150.
 *
 * Returns:     Nothing.
 */
static void GdTrackJump(DWORD tracks) {
    WORD savedTracks = jumpTracks;                      // other stock code reads these two: they come back after
    BYTE savedFlags = jumpFlags;
    GdServoSave(0);
    jumpTracks = (WORD)tracks;
    jumpFlags = (BYTE)(savedFlags | 1);                 // bit 0: inward
    TrackJump();
    jumpFlags = savedFlags;
    jumpTracks = savedTracks;
    GdServoSave(1);
    GdFocusOn();                                        // the jump can lose the focus
    TrackingOn();
}

/*
 * Function Name:  GdJumpTarget
 *
 * Get the address that a jump aims for: the jump target.
 *
 * It lies GdJumpMargin frames before the target (66 in the BU40N firmware,
 * about 70 ms at 12x), so that the read loop can start in time.
 *
 * Preconditions:   gd.targetFad holds the target.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD                The FAD of the jump target.
 */
static DWORD GdJumpTarget(void) {
    DWORD fad = gd.targetFad - GdJumpMargin;
    return fad < 45040 ? 45040 : fad;                   // a target in the lead-in made the first program reads fail
}

/*
 * Function Name:  GdTracksOf
 *
 * Change a length of the spiral from frames to tracks, at the target.
 *
 * One track at the radius r (in um) holds r / 2,129 frames. The result is
 * rounded to the nearest track.
 *
 * Preconditions:   gd.check.radius holds the radius, 31,445..60,390.
 *
 * Parameters:
 *        DWORD frames         The length in frames.
 *
 * Returns:
 *        DWORD                The length in tracks.
 */
static DWORD GdTracksOf(DWORD frames) {
    return UnsignedDivide(frames * 2129 + gd.check.radius / 2, gd.check.radius);
}

/*
 * Function Name:  GdNextJump
 *
 * After a jump and a pair: decide if the laser needs one more jump.
 *
 * Preconditions:   gd.jump.fad holds the first Q of the new pair.
 *
 * Parameters:
 *        DWORD jumpTarget     The FAD of the jump target (GdJumpTarget).
 *        DWORD steps          The jumps so far.
 *        DWORD lastTracks     The tracks of the last jump.
 *
 * Returns:
 *        DWORD                The tracks of the next jump, or 0 for no jump.
 */
static DWORD GdNextJump(DWORD jumpTarget, DWORD steps, DWORD lastTracks) {
    DWORD tracks;
    if (steps >= 12 || (SDWORD)(gd.jump.fad - jumpTarget) < 0)   // at most 12 jumps: a bound on the time
        return 0;
    tracks = GdTracksOf(gd.jump.fad - jumpTarget);
    if (tracks < 2)                                     // less than 2 tracks past the jump target
        return 0;
    if (tracks < lastTracks)                            // shorter than the last jump
        return tracks;
    if (gd.jump.retries < 2 && tracks <= GD_JUMP_MAX_TRACKS) {   // not shorter: a retry, at most 2 in all
        gd.jump.retries++;
        return tracks;
    }
    return 0;
}

/*
 * Function Name:  GdJumpToTarget
 *
 * Get from the position that the check found to just before the target.
 *
 * A position below the jump target, or under 2 tracks past it, needs no jump:
 * the disc brings the target to the laser. A position over 150 tracks from it
 * is refused. Any other position jumps back. After each jump we wait up to
 * 400 ms for a pair, to learn where the laser is. When no pair comes, the focus
 * and the tracking go on again, GdJumpRecoveries times at most (2 in the
 * BU40N firmware, 0 in the ASUS firmware), and each time we wait again.
 *
 * Preconditions:   gd.check.fad holds the position. gd.check.radius is set.
 * Postconditions:  gd.jump.fad holds the last position found.
 *
 * Parameters:  None.
 *
 * Returns:
 *        int                  1: the read loop can start. 0: it cannot.
 */
static int GdJumpToTarget(void) {
    DWORD jumpTarget = GdJumpTarget(), tracks, lastTracks = 0, steps = 0, start, fad, last;
    gd.jump.fad = gd.check.fad;
    gd.jump.retries = 0;
    if (gd.jump.fad < jumpTarget)                       // below the jump target: no jump
        return GdTracksOf(jumpTarget - gd.jump.fad) <= GD_JUMP_MAX_TRACKS;
    tracks = GdTracksOf(gd.jump.fad - jumpTarget);
    if (tracks < 2)                                     // under 2 tracks past it: no jump
        return 1;
    if (tracks > GD_JUMP_MAX_TRACKS)                    // over 150 tracks: refused
        return 0;
    for (;;) {
        if (tracks) {
            lastTracks = tracks;
            steps++;
            GdTrackJump(tracks);
            gd.check.seekQ = gd.jump.fad;
        }
        start = RTC_COUNT;
        last = 0;
        for (;;) {
            // No Q in the first 2.4 ms after the jump: an earlier Q made reads fail more often at LBAs with a bad
            // Q. Why is not known.
            fad = ((RTC_COUNT - start) >> 8) < 5 ? 0 : GdQAfterSeek(GdQProgramFad());
            if (GdQPair(fad, &last))
                break;
            if (RTC_COUNT - start >= GD_JUMP_Q_TICKS) {
                fad = 0;
                break;
            }
        }
        if (!fad) {                                     // no pair: focus-on and tracking-on again
            if (gd.jump.retries >= GdJumpRecoveries)
                return 0;
            gd.jump.retries++;
            GdFocusOn();
            TrackingOn();
            gd.check.seekQ = 0;
            tracks = 0;
            continue;
        }
        gd.jump.fad = last;
        tracks = GdNextJump(jumpTarget, steps, lastTracks);
        if (!tracks)
            return 1;
    }
}
