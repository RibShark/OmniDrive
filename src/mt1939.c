/* MT1939 platform layer for the shared OmniDrive payload (BW-12B1LT 1.03).
 *
 * src/main.c is linked unchanged from libOmniDrive.a and its hooks are used
 * directly wherever MT1939 has the same native mechanism as MT1959. This file
 * only provides what differs: shared-ABI functions without a native MT1939
 * equivalent, and hooks for native code that MT1959 does not have. All fixed
 * addresses are labels in asm/ASUS_BW-12B1LT_1.03.asm. No data or RAM is
 * allocated; like main.c, a raw read is marked in the CDB itself.
 */
#include "defs.h"
#include "mmio.h"

/* Shared main.c entry points (main.c has no header). */
void WriteDiscStructMemByte(DWORD offset, BYTE value);
void EnableXGD(void);
void CmdOmniDriveReadDiscRaw(void);

/* Native state. */
extern volatile BYTE MtSense[3];
extern volatile DWORD MtCopyStatus;
extern volatile BYTE MtPacketReady;
extern BYTE MtDVDLayers;
extern BYTE MtDVDPTP;

/* Native routines. */
extern DWORD MtNativeReadDiscData(DWORD area, BYTE count);
extern void MtNativeReadDVDData(DWORD fua);
extern void MtNativeReadDVDRAMData(DWORD fua);
extern void MtNativeReadBDData(DWORD fua);
extern DWORD MtNativeDVDCheckLayer(DWORD type);
extern DWORD MtNativeDVDPSN(DWORD address);
extern DWORD MtNativeReady(DWORD mode);
extern void MtNativeReadPrepare(void);
extern void MtNativeReadAccount(void);
extern DWORD MtNativeDVDCharacteristics(DWORD pfi);

static DWORD be32(const BYTE *p) {
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) |
           ((DWORD)p[2] << 8) | p[3];
}

/* MT1939 has no sense function; the native REQUEST SENSE reads these bytes. */
void ReturnSense(int key, int code, int qualifier) {
    MtSense[0] = key;
    MtSense[1] = code;
    MtSense[2] = qualifier;
}

/* main.c's spoofed READ(12) with the raw bit (DVD/BD raw reads). */
static BYTE IsRawDataRead(void) {
    return cdb[0] == 0xa8 && (cdb[1] & 0x80);
}

/* DVD PSN to the native address space; ROM dual layer is mapped here. */
static DWORD dvd_psn(DWORD address) {
    if ((mediaType & 0x20) || DiscIsDVDRAM())
        return MtNativeDVDPSN(address);
    if (MtDVDLayers == 1 && !MtDVDPTP && (address & 0x800000))
        address = (address + 2 * (layer0End + 1)) & 0xffffff;
    return address;
}

/* Native packet restart parsers expect LBA even when C0 supplied a PSN. */
static void encode_restart_address(void) {
    if (cdb[1] & 4) {
        DWORD bias = DiscIsBD() ? 0x100000 : (DiscIsDVDRAM() ? 0x31000 : 0x30000);
        DWORD address = startAddress - bias;
        cdb[2] = address >> 24;
        cdb[3] = address >> 16;
        cdb[4] = address >> 8;
        cdb[5] = address;
    }
}

/* Shared readers called by main.c's ReadBDRaw/ReadDVDRaw. */
void ReadBDData(DWORD fua) {
    encode_restart_address();
    MtNativeReadBDData(fua);
}

void ReadDVDData(DWORD fua) {
    encode_restart_address();
    MtNativeReadDVDData(fua);
}

void ReadDVDRAMData(DWORD fua) {
    encode_restart_address();
    MtNativeReadDVDRAMData(fua);
}

/* main.c's raw-address mapping assumes MT1959 layer state; use the native one. */
DWORD DVDCheckLayer(DWORD discType) {
    if (cdb[1] & 4)
        startAddress = dvd_psn(be32(cdb + 2));
    return MtNativeDVDCheckLayer(discType);
}

/* EnableXGD retries keep their requested PSN across native reader work. */
DWORD ReadDiscData(DWORD discArea, BYTE sectorCount) {
    DWORD address = startAddress;
    DWORD result = MtNativeReadDiscData(discArea, sectorCount);
    startAddress = address;
    return result;
}

/* READ DISC RAW (C0h) from the dispatcher's default arm; other unknown
 * opcodes keep the native ILLEGAL REQUEST response. */
void Mt1939DispatchHook(void) {
    if (cdb[0] != 0xc0) {
        ReturnSense(5, 0x20, 0);
        return;
    }
    /* The host DMA count register is 24 bits on this controller. */
    if (cdb[6]) {
        ReturnSense(5, 0x24, 0);
        return;
    }
    if (!MtNativeReady(1)) return;
    /* main.c falls through after MEDIUM NOT PRESENT and resets the idle
     * counter even on rejected requests. Validate before entering it. */
    if (!mediaType) {
        ReturnSense(2, 0x3a, 0);
        return;
    }
    BYTE kind = cdb[1] & 3;
    if (kind == 3) {
        ReturnSense(5, 0x24, 0);
        return;
    }
    if ((kind == 0 && !(DiscIsCD())) || (kind == 1 && !(DiscIsDVD())) ||
        (kind == 2 && !(DiscIsBD()))) {
        ReturnSense(5, 0x30, 2);
        return;
    }
    if ((kind == 0 && (cdb[1] & 0x10)) || (kind != 0 && cdb[10])) {
        ReturnSense(5, 0x24, 0);
        return;
    }
    /* A valid zero-sector request is a no-op. Native DMA setup can retain
     * the previous count when passed zero, so do not enter any reader. */
    if (!(cdb[7] | cdb[8] | cdb[9])) return;
    /* NativeReady prepares transfers only for its known READ opcodes. C0
     * reaches the default dispatch arm, so perform the same setup for every
     * request: an earlier ordinary READ or host priming must not be required. */
    MtNativeReadPrepare();
    /* Reception accounts only for READ(10/12). DVD/BD raw reads become A8
     * below, so the native no-progress watchdog will use that timestamp too.
     * Seed it for this request, otherwise an old timestamp can immediately
     * produce 03/11/05. Enter the native accounting body past its opcode test;
     * do not change the CDB before shared raw parsing. CD does not spoof A8. */
    if (kind != 0) MtNativeReadAccount();
    /* Keep raw sector selection local to this request. Native readers must
     * return before consuming another packet (Mt1939ContinuationPendingHook),
     * so the dispatcher sees both that packet and the original controls. */
    DWORD format = DISC_SECTOR_REGISTER;
    DWORD scramble1 = SCRAMBLE_REGISTER_UNK1 & 8;
    DWORD scramble2 = SCRAMBLE_REGISTER_UNK2 & 4;
    CmdOmniDriveReadDiscRaw();
    DISC_SECTOR_REGISTER = format;
    SCRAMBLE_REGISTER_UNK1 = (SCRAMBLE_REGISTER_UNK1 & ~8u) | scramble1;
    SCRAMBLE_REGISTER_UNK2 = (SCRAMBLE_REGISTER_UNK2 & ~4u) | scramble2;
}

/* The low format byte marks raw reads without adapter RAM. Only a queued
 * READ(10/12) must bypass native continuation and use ordinary dispatch/setup.
 * Other opcodes (especially the next C0) keep native immediate rejection;
 * reporting them absent would introduce an idle timeout on every raw read. */
DWORD Mt1939ContinuationPendingHook(void) {
    BYTE pending = MtPacketReady;
    if (pending) {
        BYTE format = (BYTE)DISC_SECTOR_REGISTER;
        if ((format == 0x98 || format == 0xf8) &&
            (cdb[0] == 0x28 || cdb[0] == 0xa8))
            return 0;
    }
    return pending;
}

/* main.c spoofs C0 as A8 before this check. Recognize that marker as well,
 * and discard the sense produced by a native check we deliberately bypass. */
DWORD Mt1939DVDMinusReadCheckHook(void) {
    BYTE raw = cdb[0] == 0xc0 || IsRawDataRead();
    BYTE sense0 = MtSense[0], sense1 = MtSense[1], sense2 = MtSense[2];
    DWORD result = DVDMinusReadCheck();
    if (!result && raw) {
        MtSense[0] = sense0;
        MtSense[1] = sense1;
        MtSense[2] = sense2;
        result = 1;
    }
    return result;
}

/* MT1959's DVDCharacteristicsPatch sends unknown PFI book types (Nintendo
 * 0xF) to the DVD-ROM case. Branching there on MT1939 skips the layer decode,
 * so rewrite the book type to DVD-ROM before the native switch instead. */
DWORD Mt1939DVDCharacteristicsHook(DWORD pfi) {
    DWORD book = pfi >> 28;
    if (book > 3 && book != 9 && book != 10 && book != 14)
        pfi &= 0x0fffffff;
    return MtNativeDVDCharacteristics(pfi);
}

DWORD Mt1939ReadDVDTOCHook(DWORD unk) {
    DWORD result = ReadDVDTOC(unk);
    if (result) {
        DWORD savedStart = startAddress, savedLength = transferLength;
        BYTE savedLayer = seekLayer;
        BYTE sense0 = MtSense[0], sense1 = MtSense[1], sense2 = MtSense[2];
        if (MtDVDLayers && !MtDVDPTP) EnableXGD();
        // If Nintendo Disc, enable BCA
        if ((ReadDiscStructMemDWORD(0x5000) >> 24) == 0xff)
            WriteDiscStructMemByte(0x5010, 0x80);
        startAddress = savedStart;
        transferLength = savedLength;
        seekLayer = savedLayer;
        MtSense[0] = sense0;
        MtSense[1] = sense1;
        MtSense[2] = sense2;
    }
    return result;
}

/* Empty Wii U DI: the native parser treats the end PSN as inclusive. */
DWORD Mt1939SetBDCharacteristicsHook(void) {
    BYTE *di = (BYTE *)(memoryStart + BDDIOffset);
    if (!*(DWORD *)di) {
        di[0x15] = 0xca; di[0x16] = 0x73; di[0x17] = 0xff;
        di[0x19] = 0x10;
        di[0x1d] = 0xca; di[0x1e] = 0x73; di[0x1f] = 0xfe;
    }
    return SetBDCharacteristics();
}

/* Unlike MT1959, MT1939 re-enables descrambling (bit 3) in DVD recovery and
 * copy-engine setup; keep it off for raw reads without the descramble flag. */
static DWORD filter_transfer_mode(DWORD mode) {
    return IsRawDataRead() && !(cdb[1] & 0x10) ? mode & ~8u : mode;
}

/* DVD reader: replaces the native (mode & ~0x2b) | 9 store. r0 is live at the
 * hook site and is returned unchanged. */
DWORD Mt1939DVDTransferModeHook(DWORD live) {
    SCRAMBLE_REGISTER_UNK1 = filter_transfer_mode((SCRAMBLE_REGISTER_UNK1 & ~0x2bu) | 9);
    return live;
}

/* Generic copy: replaces the native busy wait and mode store. r0 holds the
 * saved interrupt state and is returned unchanged. */
DWORD Mt1939CopyTransferModeHook(DWORD live, DWORD mode) {
    while (MtCopyStatus & 0x80)
        ;
    SCRAMBLE_REGISTER_UNK1 = filter_transfer_mode(mode);
    return live;
}
