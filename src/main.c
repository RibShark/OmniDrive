#include "defs.h"
#include "mmio.h"

void WriteDiscStructMemByte(DWORD offset, BYTE value) {
    DWORD offset_ = memoryStart + discStructOffset + offset;
    *(BYTE*)offset_ = value;
}

DWORD SetBDCharacteristicsHook() {
    if (*(DWORD*)(memoryStart + BDDIOffset) == 0) {
        // most likely Wii U disc, add DI PSN values
        // max PSN
      //*(BYTE*)(memoryStart + BDDIOffset + 0x14) = 0x00;
        *(BYTE*)(memoryStart + BDDIOffset + 0x15) = 0xCA;
        *(BYTE*)(memoryStart + BDDIOffset + 0x16) = 0x74;
      //*(BYTE*)(memoryStart + BDDIOffset + 0x17) = 0x00;
        // start PSN
      //*(BYTE*)(memoryStart + BDDIOffset + 0x18) = 0x00;
        *(BYTE*)(memoryStart + BDDIOffset + 0x19) = 0x10;
      //*(BYTE*)(memoryStart + BDDIOffset + 0x1a) = 0x00;
      //*(BYTE*)(memoryStart + BDDIOffset + 0x1b) = 0000;
        // end PSN
      //*(BYTE*)(memoryStart + BDDIOffset + 0x1c) = 0x00;
        *(BYTE*)(memoryStart + BDDIOffset + 0x1d) = 0xCA;
        *(BYTE*)(memoryStart + BDDIOffset + 0x1e) = 0x74;
      //*(BYTE*)(memoryStart + BDDIOffset + 0x1f) = 0x00;
    }

    return SetBDCharacteristics();
}

void SetErrorModeHook(DWORD discType, DWORD mask, DWORD mode) {
    if (cdb[0] == 0xC0) {
        if (discType == 2)
            // DVD
            mode &= ~0x01;
        else if (discType == 3) {
            // BD
            mode &= ~0x06;
        }
    }
    SetErrorMode(discType, mask, mode);
}

void ChangeDiscRWModeHook(DWORD mode) {
    ChangeDiscRWMode(mode);
    
    if (cdb[0] == 0xC0 && !(cdb[1] & 0x10)) {
        // disable descrambling
        SCRAMBLE_REGISTER_UNK1 &= ~0x08;
        SCRAMBLE_REGISTER_UNK2 &= ~0x04;
    }
    else {
        SCRAMBLE_REGISTER_UNK1 |= 0x08;
        SCRAMBLE_REGISTER_UNK2 |= 0x04;
    }
}

void EnableXGD() {
    // check DMI to see if first byte is 1 or 2
    BYTE dmiIndicator = ReadDiscStructMemDWORD(0x5800) >> 24;
    if (dmiIndicator != 1 && dmiIndicator != 2)
        return;

    seekLayer = 1;
    startAddress = 0xFD0210;

    DWORD retryCount = 0;
    while (!ReadDiscData(8, 0x10)) {
        retryCount++;
        startAddress += 0x40;
        if (retryCount > 4)
            return;
    }

    CopySectorToDiscStructMem(0x6800, 0xE);
    BYTE discIdentifier = ReadDiscStructMemDWORD(0x6800) >> 24;

    // check disc identifier to make sure sector is the security sector
    if (discIdentifier != 0xD1 && discIdentifier != 0xE1)
        return;

    layer0End = ReadDiscStructMemDWORD(0x680C);
    lastSector = ReadDiscStructMemDWORD(0x5008) | 0xFF000000;
    lastSector += 2 * (layer0End + 1);

    DWORD discSize = (lastSector + 1) - 0x30000;

    WriteDiscStructMemByte((TocOffset + 0xF), (discSize & 0xFF0000) >> 16);
    WriteDiscStructMemByte((TocOffset + 0x10), (discSize & 0xFF00) >> 8);
    WriteDiscStructMemByte((TocOffset + 0x11), discSize & 0xFF);
}

DWORD ReadDVDTOCHook(DWORD unk) {
    // might as well patch that here
    *DVDCharacteristicsPatchPtr = 0x19;

    DWORD ret = ReadDVDTOC(unk);

    if ((DiscIsMultiLayer()) && !isDiscPTP)
        EnableXGD();

    // If Nintendo Disc, enable BCA
    if ((ReadDiscStructMemDWORD(0x5000) >> 24) == 0xFF)
        WriteDiscStructMemByte(0x5010, 0x80);

    return ret;
}

void BDReadCmdHook() {
    if (*(DWORD*)(memoryStart + BDDIOffset) == 0) {
        ReturnSense(0x05, 0x30, 0x02); // CANNOT READ MEDIUM - INCOMPATIBLE FORMAT
        return;
    }
    BDReadCmd();
}

void DVDReadCmdHook() {
    if ((ReadDiscStructMemDWORD(0x5000) >> 24) == 0xFF) {
        ReturnSense(0x05, 0x30, 0x02); // CANNOT READ MEDIUM - INCOMPATIBLE FORMAT
        return;
    }
    DVDReadCmd();
}

void ReadBDRaw() {
    // CD specific
    if (cdb[10]) {
        ReturnSense(0x05, 0x24, 0x00); // INVALID FIELD IN CDB
        return;
    }
    
    startAddress = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | (cdb[5]);
    transferLength = (cdb[6] << 24) | (cdb[7] << 16) | (cdb[8] << 8) | (cdb[9]);

    if (!(cdb[1] & 0x04)) {
        // if not using raw addressing
        startAddress += 0x100000;
    }
    else {
        // drive expects LBA+0x100000 rather than PSN, so correct for this
        if (DiscIsTripleLayer()) {
            if (startAddress >= (0xBFFFFFF - layer2End))
                startAddress = startAddress + (2 * layer2End) - 0xBFFFFFE;
            else if (startAddress >= (0x7FFFFFF - layer1End))
                startAddress = startAddress + (2 * layer1End) - 0x7FFFFFE;
            else if (startAddress >= (0x3FFFFFF - layer0End))
                startAddress = startAddress + (2 * layer0End) - 0x3FFFFFE;
        }
        else if (DiscIsMultiLayer()) {
            if (startAddress >= (0x1FFFFFF - layer0End))
                startAddress = startAddress + (2 * layer0End) - 0x1FFFFFE;
        }
    }

    DISC_SECTOR_REGISTER = 0xF8; // return full sector

    ReadBDData(cdb[1] & 0x08);
}

void ReadDVDRaw() {
    // CD specific
    if (cdb[10]) {
        ReturnSense(0x05, 0x24, 0x00); // INVALID FIELD IN CDB
        return;
    }

    startAddress = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | (cdb[5]);
    transferLength = (cdb[6] << 24) | (cdb[7] << 16) | (cdb[8] << 8) | (cdb[9]);

    if(!(cdb[1] & 0x04)) {
        // if not using raw addressing
        if (DiscIsDVDRAM())
            startAddress += 0x31000;
        else
            startAddress += 0x30000;
    }

    if((cdb[1] & 0x04)) {
        // if using raw addressing
        if (!isDiscPTP && (DiscIsMultiLayer()) && (startAddress & 0x800000))
            startAddress = startAddress - ~layer0End + layer0End + 1;
    }

    DVDCheckLayer(2);

    DISC_SECTOR_REGISTER = 0xF8; // return full sector

    if (DiscIsDVDRAM())
        ReadDVDRAMData(cdb[1] & 0x08);
    else
        ReadDVDData(cdb[1] & 0x08);
}

void ReadCDRaw() {
    // CD does not support descrambling
    if (cdb[1] & 0x10) {
        ReturnSense(0x05, 0x24, 0x00); // INVALID FIELD IN CDB
        return;
    }

    startAddress = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | (cdb[5]);
    transferLength = (cdb[6] << 24) | (cdb[7] << 16) | (cdb[8] << 8) | (cdb[9]);
    
    if (!(cdb[1] & 0x04)) {
        // if not using raw addressing
        startAddress += 150;
        startAddress = LBAtoMSF(startAddress);
    }

    if (cdb[1] & 0x08)
        forceUnitAccess = 0xFF;

    DISC_SECTOR_REGISTER = 0x10; // user data, required for reading as raw

    BYTE subChannels = cdb[10] & 0b11;
    if (subChannels == 0x02)
        SetCDType(0, 1); // Audio with SubQ only
    else
        SetCDType(0, 0); // Audio
    if (subChannels) {
        DISC_SECTOR_REGISTER |= 0x01; // enable subs
    }
    
    if (cdb[10] & 0x04) {
        DISC_SECTOR_REGISTER |= 0x02; // enable C2
    };
    
    ReadCDDA();
} 

void CmdOmniDriveReadDiscRaw() {
    if (!mediaType)
        ReturnSense(0x02, 0x3A, 0x00); // MEDIUM NOT PRESENT

    BYTE DiscType = cdb[1] & 0x03;
    if (DiscType == 0) {
        if (DiscIsCD())
            ReadCDRaw();
        else
            ReturnSense(0x05, 0x30, 0x02); // CANNOT READ MEDIUM - INCOMPATIBLE FORMAT
    }
        
    else if (DiscType == 1) {
        if (DiscIsDVD())
            ReadDVDRaw();
        else
            ReturnSense(0x05, 0x30, 0x02); // CANNOT READ MEDIUM - INCOMPATIBLE FORMAT
    }

    else if (DiscType == 2) {
        if (DiscIsBD())
            ReadBDRaw();
        else
            ReturnSense(0x05, 0x30, 0x02); // CANNOT READ MEDIUM - INCOMPATIBLE FORMAT
    }
    else {
        ReturnSense(0x05, 0x24, 0x00); // INVALID FIELD IN CDB 
    }
}