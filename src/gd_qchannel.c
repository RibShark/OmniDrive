// gd_qchannel.c
//
// These functions get the address of a sector as a FAD: from the Q channel
// that the decoder gives, and from a block in the buffer (its sector header or
// its subcode). They also change a FAD to the form of the decoder target.

#include "gd.h"

/*
 * Function Name:  GdBcdToBin
 *
 * Turn one byte of a GD address into its value: tens digit x 10 + units digit.
 *
 * Parameters:
 *        DWORD digits         The byte.
 *
 * Returns:
 *        DWORD                The value.
 */
static DWORD GdBcdToBin(DWORD digits) {
    return (digits >> 4) * 10 + (digits & 0xF);
}

/*
 * Function Name:  GdBcdIn
 *
 * Tell if a byte of a GD address has a good units digit and a value up to max.
 *
 * Parameters:
 *        DWORD digits         The byte.
 *        DWORD max            The highest value allowed.
 *
 * Returns:
 *        int                  Not 0 when the byte is good.
 */
static int GdBcdIn(DWORD digits, DWORD max) {
    return (digits & 0xF) <= 9 && GdBcdToBin(digits) <= max;
}

/*
 * Function Name:  GdBinToBcd
 *
 * Change a number to two digits, with a hex tens digit above 99.
 *
 * A GD disc writes minute 122 as 0xC2: the tens digit 12 is the hex digit C.
 *
 * Parameters:
 *        DWORD number         The number, 0 to 159.
 *
 * Returns:
 *        DWORD                The two digits.
 */
static DWORD GdBinToBcd(DWORD number) {
    DWORD tens = UnsignedDivide(number, 10);
    return (tens << 4) | (number - tens * 10);
}

/*
 * Function Name:  GdMsfToFad
 *
 * Turn the three bytes of a GD address into a FAD.
 *
 * A FAD is a frame address: the LBA + 150.
 *
 * Parameters:
 *        DWORD minute         The minute byte.
 *        DWORD second         The second byte.
 *        DWORD frame          The frame byte.
 *
 * Returns:
 *        DWORD                The FAD.
 */
static DWORD GdMsfToFad(DWORD minute, DWORD second, DWORD frame) {
    return GdBcdToBin(minute) * 4500 + GdBcdToBin(second) * 75 + GdBcdToBin(frame);
}

/*
 * Function Name:  GdFadToMsf
 *
 * Change a FAD to the minute, second and frame bytes of the decoder target.
 *
 * We do not use the stock converter, because it cannot write a minute above
 * 99.
 *
 * Parameters:
 *        DWORD fad            The address.
 *
 * Returns:
 *        DWORD                The minute in bits 23..16, the second, the frame.
 */
DWORD GdFadToMsf(DWORD fad) {
    DWORD msf = BlockToMsf(fad);
    return (GdBinToBcd(msf >> 16) << 16) | (GdBinToBcd((msf >> 8) & 0xFF) << 8) | GdBinToBcd(msf & 0xFF);
}

/*
 * Function Name:  GdQAbsFad
 *
 * Get the FAD from the absolute time of the decoded Q.
 *
 * The Q is the Q channel of the subcode. It holds the address of each sector.
 *
 * For a GD disc, the tens digit of the minute is a hex digit, and the highest
 * address of a GD disc lies in minute 151.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD                The FAD, or 0 when a field is out of range.
 */
static DWORD GdQAbsFad(void) {
    DWORD minute = GdBcdToBin(Q_CHANNEL1 >> 24), second, frame;
    if (minute > 151)
        return 0;
    second = GdBcdToBin(Q_CHANNEL2 & 0xFF);
    if (second > 59)
        return 0;
    frame = GdBcdToBin((Q_CHANNEL2 >> 8) & 0xFF);
    if (frame > 74)
        return 0;
    return minute * 4500 + second * 75 + frame;
}

/*
 * Function Name:  GdQFad
 *
 * Get the address in the decoded Q, in the form that this read needs.
 *
 * In a program read it is the absolute time. In a lead-in read it is the
 * running time of a Q of the high-density lead-in: ADR 1, TNO 0, and a minute
 * from 145 to 151.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD                The FAD, or 0 for any other Q.
 */
DWORD GdQFad(void) {
    DWORD qChannel0, qChannel1, minute;
    if (!gd.leadIn)
        return GdQAbsFad();
    qChannel0 = Q_CHANNEL0;
    minute = qChannel0 >> 24;
    if ((qChannel0 & 0xF) != 1 || (qChannel0 & 0xFF00) || !GdBcdIn(minute, 151) || GdBcdToBin(minute) < 145)
        return 0;
    qChannel1 = Q_CHANNEL1;
    if (!GdBcdIn(qChannel1 & 0xFF, 59) || !GdBcdIn((qChannel1 >> 8) & 0xFF, 74))
        return 0;
    return GdMsfToFad(minute, qChannel1 & 0xFF, (qChannel1 >> 8) & 0xFF);
}

/*
 * Function Name:  GdQProgramFad
 *
 * Get the address in the decoded Q, if it is a program position.
 *
 * It must have ADR 1 and a track number, and lie in FAD 45,000..555,999.
 *
 * Parameters:  None.
 *
 * Returns:
 *        DWORD                The FAD, or 0.
 */
DWORD GdQProgramFad(void) {
    DWORD qChannel0 = Q_CHANNEL0, fad;
    if ((qChannel0 & 0xF) != 1 || ((qChannel0 >> 8) & 0xFF) == 0)
        return 0;
    fad = GdQAbsFad();
    return (fad >= 45000 && fad < 556000) ? fad : 0;
}

/*
 * Function Name:  GdBlockHeaderFad
 *
 * Read the address from the sector header at the start of a buffer block.
 *
 * Parameters:
 *        DWORD block          The buffer block, from 0.
 *
 * Returns:
 *        DWORD                The FAD, or 0 when the block does not start with
 *                             a CD sync.
 */
DWORD GdBlockHeaderFad(DWORD block) {
    volatile DWORD *sync = (volatile DWORD *)(GD_BUFFER + block * 0xB00);
    volatile BYTE *sector = (volatile BYTE *)sync;
    if (sync[0] != 0xFFFFFF00u || sync[1] != 0xFFFFFFFFu || sync[2] != 0x00FFFFFFu)
        return 0;
    return GdMsfToFad(sector[0xC], sector[0xD], sector[0xE]);
}

/*
 * Function Name:  GdBlockQByte
 *
 * Get one byte of the Q channel from the subcode that a buffer block holds.
 *
 * The block holds 96 subcode bytes. Bit 6 of each one belongs to the Q channel,
 * so 8 subcode bytes make one Q byte, highest bit first (ECMA-130).
 *
 * Parameters:
 *        volatile BYTE *subcode The first subcode byte of the block.
 *        DWORD index          The Q byte that we want, from 0.
 *
 * Returns:
 *        DWORD                The Q byte.
 */
static DWORD GdBlockQByte(volatile BYTE *subcode, DWORD index) {
    DWORD value = 0, bit;
    subcode += index * 8;
    for (bit = 0; bit < 8; bit++)
        value = (value << 1) | ((subcode[bit] >> 6) & 1);
    return value;
}

/*
 * Function Name:  GdBlockFad
 *
 * Read the address of the sector in one buffer block.
 *
 * A block of the lead-in starts with a CD sync, so its header gives the
 * address. A program block has its sync some bytes in, so we use its Q channel.
 * A Q gives an address only with ADR 1 and a track number.
 *
 * Preconditions:   gd.hdRead is 1, so gd.leadIn says what kind of read it is.
 *
 * Parameters:
 *        DWORD block          The buffer block, from 0.
 *
 * Returns:
 *        DWORD                The FAD, or 0 when the block has no address.
 */
DWORD GdBlockFad(DWORD block) {
    volatile BYTE *subcode = (volatile BYTE *)(GD_BUFFER + block * 0xB00 + 2352);
    if (!GdInProgramRead())
        return GdBlockHeaderFad(block);
    if ((GdBlockQByte(subcode, 0) & 0xF) != 1 || GdBlockQByte(subcode, 1) == 0)
        return 0;
    return GdMsfToFad(GdBlockQByte(subcode, 7), GdBlockQByte(subcode, 8), GdBlockQByte(subcode, 9));
}
