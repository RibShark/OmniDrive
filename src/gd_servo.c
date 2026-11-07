// gd_servo.c
//
// These functions control the servo during a read. They read and write the
// servo DSP with a limit on each wait, get the focus back, pull in the
// tracking, set up DPD tracking and its RF gains, and set the data PLL.

#include "gd.h"

/*
 * Function Name:  GdServoRead
 *
 * Read one value from the servo DSP, with a limit on the wait.
 *
 * Interrupts stay off during the access. The wait ends after 262,144 polls.
 *
 * The stock version waits with no limit. If the DSP never answered, the
 * drive would hang.
 *
 * Parameters:
 *        DWORD index          The DSP register.
 *
 * Returns:
 *        DWORD                The 16-bit value. Not valid after a timeout.
 */
DWORD GdServoRead(DWORD index) {
    DWORD cpsr = InterruptsOff(0xC0), polls = 0x40000, value;
    REG32(SERVO_DSP) = index + (1u << 20);              // the read of the register
    while ((REG8(SERVO_DSP + 4) & 1) && --polls)        // wait while the DSP is busy
        ;
    value = REG16(SERVO_DSP + 0x10);
    InterruptsRestore(cpsr);
    return value;
}

/*
 * Function Name:  GdServoWrite
 *
 * Write one value to the servo DSP, with a limit on the wait (as GdServoRead).
 *
 * Parameters:
 *        DWORD index          The DSP register.
 *        DWORD value          The value.
 *
 * Returns:     Nothing.
 *
 * Notes: keep this function global and out of line, so that a check of the
 * image can find every DSP write of ours by this symbol.
 */
GD_SYMBOL void GdServoWrite(DWORD index, DWORD value) {
    DWORD cpsr = InterruptsOff(0xC0), polls = 0x40000;
    REG32(SERVO_DSP + 0x10) = value;
    REG32(SERVO_DSP) = index + (9u << 20);              // the write of the register
    while ((REG8(SERVO_DSP + 4) & 1) && --polls)        // wait while the DSP is busy
        ;
    InterruptsRestore(cpsr);
}

/*
 * Function Name:  GdServoCommand
 *
 * Send one command word to the servo DSP, with a limit on the wait.
 *
 * Parameters:
 *        DWORD reg            The command register, such as 0xC600.
 *        DWORD value          The value, in the low byte of the word.
 *
 * Returns:     Nothing.
 *
 * Notes: keep this function global and out of line, as GdServoWrite.
 */
GD_SYMBOL void GdServoCommand(DWORD reg, DWORD value) {
    DWORD cpsr = InterruptsOff(0xC0), polls = 0x40000;
    REG32(SERVO_DSP + 0x10) = reg | value;
    REG32(SERVO_DSP) = 0x00910200;
    while ((REG8(SERVO_DSP + 4) & 3) != 2 && --polls)
        ;
    InterruptsRestore(cpsr);
}

/*
 * Function Name:  GdWaitFocus
 *
 * Wait until the focus-OK flag reaches the wanted state.
 *
 * The wait ends at the wanted state or when the time is over. A limit of
 * 16,777,216 tests ends it if the clock register ever stops.
 *
 * Parameters:
 *        int want             The state to wait for: 1 = focus, 0 = no focus.
 *        DWORD ticks          The longest wait, in clock ticks (529 = 1 ms).
 *
 * Returns:
 *        int                  The flag at the last test.
 */
int GdWaitFocus(int want, DWORD ticks) {
    DWORD start = RTC_COUNT, tests = 1u << 24;
    int focusOk;
    while ((focusOk = FOCUS_OK() != 0) != want && RTC_COUNT - start < ticks && --tests)
        ;
    return focusOk;
}

/*
 * Function Name:  GdFocusOn
 *
 * Get the laser back in focus if focus has been lost.
 *
 * If the focus servo is still running, it gets 30 ms to find the focus by
 * itself. If there is still no focus, the stock focus search runs.
 *
 * No later step of a read works without focus. The focus can be lost if the
 * sled has to move a long distance.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdFocusOn(void) {
    DWORD servoOn = SERVO_STATUS & SERVO_FOCUS_ON;
    if (!FOCUS_OK() && !(servoOn && GdWaitFocus(1, 0x3E01)))   // 0x3E01 ticks = 30 ms
        FocusSearch();
}

/*
 * Function Name:  GdFocusOff
 *
 * Open the tracking loop, turn the focus off, and wait up to 50 ms until the
 * focus is gone.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
static void GdFocusOff(void) {
    GdTrackingOpen();
    FocusOff();
    GdWaitFocus(0, 50 * TICKS_MS);
}

/*
 * Function Name:  GdSeekDoneWait
 *
 * Wait for the seek-OK flag, for at most 254 reads of the sled position.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing. The flag can still be off at the end.
 */
void GdSeekDoneWait(void) {
    DWORD reads = 0xFF;
    while (!SEEK_OK() && --reads)
        GdServoRead(0x300);
}

/*
 * Function Name:  GdServoClose
 *
 * Close every servo loop: the focus, the tracking and the sled.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdServoClose(void) {
    GdSeekDoneWait();
    GdFocusOn();
    GdTrackingClose();
    SledServoSet(0);                                    // the sled servo values of the tracking mode
}

/*
 * Function Name:  GdTrackingOn
 *
 * Pull in the tracking, in the stock slip mode specifically for recordable
 * discs.
 *
 * A pull-in starts the tracking servo on a track.
 *
 * In a program read of a recordable disc, we set the slip mode for this one
 * call. The slip mode is a mode of the stock pull-in, which the stock code
 * turns on for a CD-RW that slips often: the pull-in waits longer, and when
 * the laser slips off the track, the stock code pulls in again. It also
 * changes two RF zero-cross values, and we put them back afterwards.
 *
 * For GD-Rs a normal pull-in often fails. I don't know why.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD                0 when the tracking servo is on.
 */
DWORD GdTrackingOn(void) {
    DWORD result, flags, hys, hys2;
    if (!gd.recordable || !GdInProgramRead())
        return TrackingOn();                            // the normal pull-in
    flags = trackingFlags;
    hys = RF_ZC_HYSTERESIS;                             // the slip mode changes these two
    hys2 = RF_ZC_HYSTERESIS2;
    trackingFlags = flags | 1;                          // bit 0: the slip mode
    result = TrackingOn();
    trackingFlags = flags;
    RF_ZC_HYSTERESIS = hys;
    RF_ZC_HYSTERESIS2 = hys2;
    return result;
}

/*
 * Function Name:  GdPullIn
 *
 * Measure the focus offset, wait, then pull in the tracking.
 *
 * The stock pull-in makes the same two steps before it tries again: it
 * measures the offset of the focus error signal, then it waits a short time.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD                0 when the tracking servo is on.
 */
static DWORD GdPullIn(void) {
    FocusOffsetMeasure(20);
    TimerWait(20);
    return GdTrackingOn();
}

/*
 * Function Name:  GdPullInAgain
 *
 * Make one more pull-in after one that specifically failed.
 *
 * With no focus we do nothing. While the coil protection counts, we only open
 * the tracking loop. If we do another pull-in we might heat the coils further,
 * which I suspect causes issues later on.
 *
 * The coil protection is stock code that counts while the currents of the lens
 * coils are too high. At its limit it stops the servo, and the drive refuses
 * reads until the disc is loaded again.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdPullInAgain(void) {
    if (!FOCUS_OK())
        return;
    GdTrackingOpen();
    if (coilCounter == 0)                               // the coil protection does not count
        GdPullIn();
}

/*
 * Function Name:  GdRefocus
 *
 * Turn the focus off and on in place, then pull in the tracking.
 *
 * A recordable disc gets a second try when the pull-in loses the focus and the
 * coil protection does not count.
 *
 * A pull-in can lose focus when using GD-Rs. Refocusing seems to always bring
 * it back.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdRefocus(void) {
    int i;
    for (i = 0; i < 2; i++) {
        GdFocusOff();
        GdFocusOn();
        GdPullIn();
        // a second try only for a recordable disc with no focus, while the coil protection does not count
        if (FOCUS_OK() || !gd.recordable || coilCounter != 0)
            break;
    }
}

/*
 * Function Name:  GdDataPllSet
 *
 * Move the data PLL to a new rate, and let it lock from its preset word.
 *
 * The data PLL is the clock that locks to the data on the disc. The stock code
 * makes the same three calls in this order.
 *
 * Parameters:
 *        DWORD rate           The new rate, in the unit of the stock function.
 *
 * Returns:     Nothing.
 */
void GdDataPllSet(DWORD rate) {
    DataPllRate(rate);
    DataPllHold();                                      // the PLL waits at the preset word, dataPllPreset
    DataPllRelease();                                   //   and then locks from there
}

/*
 * Function Name:  GdClockAfterStart
 *
 * After a start of the disc, set the data clock as the stock pull-in would.
 *
 * The stock tracking pull-in sets the clock divider of the data channel only
 * when the disc is fast enough. Our read starts slower, so it can miss that
 * step. With the divider still at 4, the spindle in CAV, 10x or less, and a
 * stock clock of 1 to 31, we select the divider 3 and set the clock ourselves.
 *
 * Without this, the data PLL loses its lock at about FG 47.
 *
 * Preconditions:   The disc is a CD, and servoFlags bit 6 says that it started.
 * Postconditions:  servoFlags bit 6 is clear. gd.ref.clockFixes counts the fix.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdClockAfterStart(void) {
    DWORD clock = jumpClock;
    servoFlags &= ~0x40;
    if (((CLOCK_DIVIDER >> 4) & 7) != 4 || (spindleMode & 7) != 2 || driveSpeed > 10 || clock - 1 > 30)
        return;
    ClockDividerSelect(3);
    GdDataPllSet(clock << 3);
    gd.ref.clockFixes++;
}

/*
 * Function Name:  GdDpdOn
 *
 * Set up the tracking for the high-density read.
 *
 * With a seek ahead, we turn on DPD (differential phase detection, a way to
 * measure the tracking error), keep the stock RF gains, and set the RF gain
 * 0x4003 on both inputs. Then the tracking loop gain goes up to 0x4800 if it is
 * lower. GdSledReturn puts the stock method and the stock gains back.
 *
 * Preconditions:   gd.noSeek is set for this read.
 * Postconditions:  After a seek, gd.rfGain10 and gd.rfGain0 hold the stock
 *                  gains.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void GdDpdOn(void) {
    if (!gd.noSeek) {
        POWER_DOWN &= ~(1u << 30);                      // the DPD block on
        RF_POWER_DOWN &= ~0x40u;                        //   and its RF front end
        gd.rfGain10 = (WORD)RfGainGet(0x10);            // the stock gains, for GdSledReturn
        gd.rfGain0 = (WORD)RfGainGet(0);
        DpdTracking(1);
        DpdEqualizer(8);
        // DPD gives a larger signal than push-pull, the stock method, so it needs a lower gain than the stock one.
        RfGainSet(0x4003, 0x10);
        RfGainSet(0x4003, 0);
    }
    // 0x212 is the gain of the tracking loop. While the drive sleeps, the next C0 03 command raises it.
    if (!(powerState & 0x02) && GdServoRead(0x212) < 0x4800)
        GdServoWrite(0x212, 0x4800);
}
