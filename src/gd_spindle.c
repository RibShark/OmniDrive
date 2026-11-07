// gd_spindle.c
//
// These functions control the spindle speed of a read: 2,000 rpm for a lead-in
// read, and ZCAV at up to 12x for a program read. They keep the stock spindle
// values at the start of a read and put them back at the end.

#include "gd.h"

// the rpm factor of each ZCAV zone of 16,384 sectors from FAD 45,150 (the rpm = the factor x the speed x 2)
static const BYTE GdZcavFactor[31] = {
    138, 133, 129, 125, 121, 118, 115, 112, 110, 107, 105, 103, 101, 99, 97, 95,
    93, 92, 90, 89, 88, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77 };

/*
 * Function Name:  GdSpindleSave
 *
 * Keep the stock spindle values, and lower the stock speed cap for the read.
 *
 * Five values wait here for GdSpindleRestore: the FG divider, the FG speed-up,
 * a gain and a clamp value that follow the rpm, and the spindle mode. They are
 * kept only when none are kept yet, so a second call before GdSpindleRestore
 * does not keep our own values in their place.
 *
 * The cap goes to 8, or to the stock speed if that is lower. capSaved keeps
 * the stock cap, because GdSpindleRestore leaves a lower cap behind.
 *
 * All of this because I'm paranoid about spinning the disc too fast under
 * certain undetermined situations.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 *
 * Notes: gd.speedUp gets the stock value here, so a stock spindle write before
 * GdSpindleSet stays at the stock speed.
 */
void GdSpindleSave(void) {
    struct GdSpindle *spindle = &gd.spindle;
    BYTE cap;
    if (!spindle->saved) {
        spindle->reg30 = REG32(SPINDLE_BASE + 0x30);
        spindle->reg38 = (WORD)SPINDLE_SPEEDUP;
        spindle->reg40 = (BYTE)(REG32(SPINDLE_BASE + 0x40) >> 8);
        spindle->reg28 = (BYTE)SPINDLE_CLAMP;
        spindle->reg24 = (BYTE)REG32(SPINDLE_BASE + 0x24);
        spindle->saved = 1;
    }
    spindle->stockSpeedUp = 0;
    if (!(spindle->reg38 & 0x100)) {                    // the stock speed-up fits in one byte
        gd.speedUp = (BYTE)spindle->reg38;
        spindle->stockSpeedUp = gd.speedUp;
    }
    if (spindle->capSet == 0 || driveSpeedCap != spindle->capSet)   // the cap now is the stock cap
        spindle->capSaved = driveSpeedCap;
    cap = spindle->capSaved > 8 ? 8 : spindle->capSaved;
    if (driveSpeed && driveSpeed < cap)                 // a stock speed below the cap (0: no limit)
        cap = driveSpeed;
    driveSpeedCap = cap;
    spindle->capSet = cap;
}

/*
 * Function Name:  GdSpindleSet
 *
 * Set the spindle to the rpm of this read: the ZCAV rpm, or 2,000 rpm for a
 * lead-in read.
 *
 * The hardware reaches rpm = 90,144 x divider / (speed-up + 1). The stock CAV
 * setter runs first, because it also sets the spindle mode and the values that
 * follow the rpm. Then our divider and speed-up replace its own, and
 * SPINDLE_LOAD applies them.
 *
 * We do this because the stock setter picks a larger divider and a speed-up
 * that does not fit in one byte. The speed hooks hold one byte (gd.speedUp).
 *
 * Preconditions:   gd.zcav holds the rpm when gd.zcav.on is 1.
 * Postconditions:  gd.speedUp holds the speed-up for the speed hooks.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD                The FG that this rpm gives: about 1 FG for each
 *                             61 rpm.
 */
static DWORD GdSpindleSet(void) {
    DWORD rpm = gd.zcav.on ? gd.zcav.rpm : GD_RPM, fgDiv = (93 * rpm) >> 15;
    DWORD speedUp = UnsignedDivide(fgDiv * 90144 + rpm / 2, rpm) - 1;
    gd.speedUp = (BYTE)speedUp;
    SpindleCavSet(rpm);
    REG32(SPINDLE_BASE + 0x30) = (REG32(SPINDLE_BASE + 0x30) & 0xC0FFFFFF) | ((fgDiv - 1) << 24);
    SPINDLE_SPEEDUP = (SPINDLE_SPEEDUP & ~0x1FFu) | speedUp;
    SPINDLE_LOAD = 4;
    return (67 * rpm) >> 12;
}

/*
 * Function Name:  GdZcavZone
 *
 * Find the ZCAV zone of an address.
 *
 * In ZCAV the disc turns at one rpm in each zone, and the rpm falls from zone
 * to zone toward the edge, so that the data rate stays the same. A zone holds
 * 16,384 sectors, and the zones count from FAD 45,150.
 *
 * Parameters:
 *        DWORD fad            The address.
 *
 * Returns:
 *        DWORD                The zone, 0..30.
 */
static DWORD GdZcavZone(DWORD fad) {
    DWORD zone = fad > 45150 ? (fad - 45150) >> 14 : 0;
    return zone > 30 ? 30 : zone;
}

/*
 * Function Name:  GdZcavRpm
 *
 * Work out the rpm that gives a speed at an address.
 *
 * The rpm is the factor of the zone x the speed x 2, from 1,000 to 3,792. The
 * factors fall toward the edge, because each turn there passes more data. The
 * table gives at most 3,312 rpm at 12x, and 3,792 is our safety limit.
 *
 * Parameters:
 *        DWORD fad            The address.
 *        DWORD speed          The speed, in x.
 *
 * Returns:
 *        DWORD                The FAD where this zone ends. The rpm goes to
 *                             gd.zcav.rpm.
 */
static DWORD GdZcavRpm(DWORD fad, DWORD speed) {
    DWORD zone = GdZcavZone(fad), rpm = GdZcavFactor[zone] * speed * 2;
    gd.zcav.rpm = (WORD)(rpm < 1000 ? 1000 : rpm > 3792 ? 3792 : rpm);
    return ((zone + 1) << 14) + 45150;
}

/*
 * Function Name:  GdZcavNext
 *
 * Find the address of the next speed step.
 *
 * During the ramp it lies 512 sectors on. After the ramp, or when the zone ends
 * first, it is the end of the zone.
 *
 * Preconditions:   gd.zcav.speed, goal and zoneEnd are set.
 *
 * Parameters:
 *        DWORD fad            The address now.
 *
 * Returns:     Nothing. The result goes to gd.zcav.nextStep.
 */
static void GdZcavNext(DWORD fad) {
    struct GdZcav *zcav = &gd.zcav;
    zcav->nextStep = zcav->speed < zcav->goal && fad + 512 <= zcav->zoneEnd ? fad + 512 : zcav->zoneEnd;
}

/*
 * Function Name:  GdZcavStart
 *
 * Choose the spindle speed at the start of a read.
 *
 * ZCAV runs in each program read, of a pressed disc and of a recordable disc.
 * The goal is 12x, or 8x with the FUA bit. The read starts at the speed that
 * the disc has now, and at least at 4x. That speed comes from the FG and the
 * factor of this zone.
 *
 * I want the rpm to stay under 3,792. In this area 12x gives about 890 sectors
 * a second. A lead-in read stays at 2,000 rpm.
 *
 * Preconditions:   gd.targetFad and gd.leadIn are set.
 * Postconditions:  gd.zcav says if ZCAV runs, with its speed and next step.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdZcavStart(void) {
    struct GdZcav *zcav = &gd.zcav;
    DWORD speed = (cdb[1] & 0x08) ? 8 : GD_ZCAV_SPEED, factor = 67 * GdZcavFactor[GdZcavZone(gd.targetFad)], now;
    zcav->on = 0;
    if (gd.leadIn)
        return;
    zcav->goal = (BYTE)speed;
    now = SPINDLE_FG();
    if (now > 0x20)                                     // FG 32 is 2,000 rpm: never start far above it
        now = 0x20;
    now = UnsignedDivide((now << 11) + factor / 2, factor);   // the speed that the disc has now
    if (now < 4)
        now = 4;
    if (now < speed)
        speed = now;
    zcav->speed = (BYTE)speed;
    zcav->zoneEnd = GdZcavRpm(gd.targetFad, speed);
    GdZcavNext(gd.targetFad);
    zcav->on = 1;
}

/*
 * Function Name:  GdZcavStep
 *
 * Change the spindle speed during a stream.
 *
 * At the end of a zone the disc needs a new rpm for the same data rate. During
 * the ramp the speed also goes up by 2x every 512 sectors while the host keeps
 * up, that is, while 20 or fewer blocks wait in the buffer.
 *
 * The disc must not speed up too fast. A host that is slower than the disc
 * also keeps the disc at a lower speed.
 *
 * Preconditions:   gd.zcav.on is 1.
 *
 * Parameters:
 *        DWORD fad            The address of the cache hit at the step.
 *
 * Returns:     Nothing.
 */
void GdZcavStep(DWORD fad) {
    struct GdZcav *zcav = &gd.zcav;
    int change = fad >= zcav->zoneEnd;                  // the end of the zone
    if (zcav->speed < zcav->goal && BUFFER_UNSENT16 <= 0x14) {   // the ramp, while the host keeps up
        zcav->speed = zcav->speed + 2 > zcav->goal ? zcav->goal : zcav->speed + 2;
        change = 1;
    }
    if (change) {
        zcav->zoneEnd = GdZcavRpm(fad, zcav->speed);
        GdSpindleSet();
    }
    GdZcavNext(fad);
}

/*
 * Function Name:  GdSpindleWait
 *
 * Bring the spindle to the speed of this read, and wait until it is steady.
 *
 * A stopped spindle first gets the stock wake. Then we wait until the FG stays
 * within 3 of its target for 304 ms, and for 3 seconds at most. In a program
 * read with the speed equal to the cap, the first good sample ends the wait.
 *
 * The wait is necessary because a coarse seek on a disc that is still speeding
 * up loses the track. In some tests, sometimes a read immediately following a
 * jump is lost.
 *
 * The reason for the shorter wait when speed = the cap is that the laser slides
 * more after a jump when the speed is above the cap and there is no wait. A
 * lead-in read always waits in full, because a short wait for a single lead-in
 * read with the stock speed step took longer to complete.
 *
 * Preconditions:   gd.noSeek and gd.leadIn are set for this read.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 *
 * Notes: the 3 second wait also ends after 16,777,216 tests, and the 4 second
 * wait of the wake after 268,435,456 tests, so that both end even if the clock
 * register stops.
 */
void GdSpindleWait(void) {
    DWORD fgTarget, start, since = 0, tests = 1u << 24;
    int first;
    if (gd.noSeek)                                      // the request is in the buffer: no seek, no wait
        return;
    if (SPINDLE_DRIVE == 0) {                           // a stopped spindle: the stock wake
        DWORD wakeStart, wakeTests = 1u << 28;
        PowerModeSet(2);
        PowerModeSet(0);
        GdSpindleSet();
        wakeStart = RTC_COUNT;
        while (SPINDLE_FG() < 12 && RTC_COUNT - wakeStart < (1u << 21) && --wakeTests)   // 4 seconds
            ;
        GdServoClose();
    }
    fgTarget = GdSpindleSet();
    start = RTC_COUNT;
    GdCalibrationOff();
    first = driveSpeed == driveSpeedCap && !gd.leadIn;
    while (--tests) {
        DWORD now = RTC_COUNT;
        SDWORD difference;
        if (now - start >= 47 * 33862)                  // 3 seconds
            break;
        difference = (SDWORD)SPINDLE_FG() - (SDWORD)fgTarget;
        if (difference < -3 || difference > 3) {
            since = 0;
            first = 0;
        } else if (first)
            break;
        else if (!since)
            since = now;
        else if (now - since >= 19 * 8466)              // 304 ms
            break;
    }
}

/*
 * Function Name:  GdSpindleRestore
 *
 * Give the spindle back to the stock code at the end of a read.
 *
 * The five kept values go back, and SPINDLE_LOAD applies them. The stock seek
 * target goes back too. The speed cap goes to 4 or lower.
 *
 * We have to do this to ensure that normal drive reading operations aren't
 * affected after doing a GD read.
 *
 * Postconditions:  gd.speedUp holds the stock speed-up again, when it has one.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 *
 * Notes: do not take off the kick clamp or the FG limit that GdHdRead sets.
 * They stay on as protections.
 */
void GdSpindleRestore(void) {
    struct GdSpindle *spindle = &gd.spindle;
    int i;
    if (spindle->saved) {
        // With our values left in place, the stock speed loop aims far above any speed that the motor can reach.
        SPINDLE_SPEEDUP = (SPINDLE_SPEEDUP & ~0x7FFu) | (spindle->reg38 & 0x7FF);
        REG32(SPINDLE_BASE + 0x30) = spindle->reg30;
        REG32(SPINDLE_BASE + 0x40) = (REG32(SPINDLE_BASE + 0x40) & ~0xF00u) | ((DWORD)(spindle->reg40 & 0xF) << 8);
        SPINDLE_CLAMP = (SPINDLE_CLAMP & ~0xFFu) | spindle->reg28;
        REG32(SPINDLE_BASE + 0x24) = (REG32(SPINDLE_BASE + 0x24) & ~1u) | (spindle->reg24 & 1);
        SPINDLE_LOAD = 4;
        spindle->saved = 0;
        if (spindle->stockSpeedUp)
            gd.speedUp = spindle->stockSpeedUp;
    }
    if (gd.seekTargetSaved) {
        for (i = 0; i < 3; i++)
            seekTargetMsf[i] = gd.seekTarget[i];
        gd.seekTargetSaved = 0;
    }
    if (spindle->capSet) {
        BYTE cap = driveSpeedCap;
        if (cap > spindle->capSet)                      // the stock code raised the cap during the read
            spindle->capSaved = cap;
        if (cap > 4) {                                  // 4 is the stock cap of a GD-ROM
            cap = 4;
            driveSpeedCap = 4;
        }
        spindle->capSet = cap;
    }
}
