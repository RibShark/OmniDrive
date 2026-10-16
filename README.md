# OmniDrive
Copyright © 2026 Rib

## Description
OmniDrive is a firmware modification for MediaTek MT1959-based optical disc drives manufactured by Hitachi-LG Data Storage that enables features that allow for better, more complete reading of discs, including proprietary game discs.

## Features
* Lead-in/lead-out reading for CD/DVD/BD and derivatives.
* Raw sector reading for CD/DVD/BD and derivatives (2352 bytes/sector for CD, 2064 bytes/sector for DVD, 2072 bytes/sector for BD).
* Custom READ DISC RAW command to read discs, with flags for FUA (bypassing cache), raw addressing (MSF for CD, PSN for DVD/BD), and descrambling (for DVD/BD).
* The ability to read various proprietary game discs (see table):

| Disc Type | Support | Notes |
| ---: | :---: | --- |
| CD (ROM/R/RW) | ☑️ |
| DVD (ROM/RAM/±R/±RW) | ☑️ |
| BD (ROM/R/RE) | ☑️ |
| BD-XL (R/RE) | ☑️ |
| UHD-BD | ☑️ |
| PlayStation 3 BD-ROM | ☑️ | Disc contents are encrypted, disc key is not retrievable
| PlayStation 4 BD-ROM | ☑️ | Game files are encrypted, encryption key is not retrievable
| PlayStation 5 BD-ROM | ☑️ | Game files are encrypted, encryption key is not retrievable
| Xbox (XGD1) | ✅ |
| Xbox 360 (XGD2/XGD3) | ✅ |
| Xbox One/Series (XGD4) | ✅ | Game files are encrypted, encryption key is not retrievable
| GameCube (NROM)/Wii (RVL-ROM) | ✅ |
| GameCube (NR)/Wii (RVT-R) | ✅ | May take a few minutes for the drive to recognise the disc. Support still needs to be added in dumping software.
| Wii U (WUP-ROM?) | ⚠️ | Can be read/descrambled but not decrypted, disc key is not retrievable
| Wii U (CAT-R) | ☑️ |
| Dreamcast (GD-ROM/GD-R) | ❎ | Only low-density area can be read

Key:
* ☑️ - Native Support (without patched firmware)
* ✅ - Supported with OmniDrive
* ⚠️ - Partial support (see notes)
* ❓ - Unknown support (needs testing)
* ❎ - Not currently supported

## Building
### Dependencies
* CMake
* ninja
* arm-none-eabi-gcc toolchain
* [armips](https://github.com/Kingcom/armips)
* Python 3
* pycryptodome
### Instructions
1. Put the stock LG BU40N 1.00 and ASUS BW-16D1HT 3.02 firmware files in the `firmware` folder. The files should be named the following and have the following checksums:

| Filename | CRC32 | MD5 | SHA-1 | SHA-256 |
| --- | --- | --- | --- | --- |
| HL-DT-ST_BD-RE_BU40N_1.00.bin | `E3C1A315` | `EDB28FCD7A239281ACE26A468D382A9C` | `9C48677B155154D24A3B95A32B4A29CA02FF40B3` | `79E8DF06C776ED40523A5B530D1559E561FBEFB71A9E981D301A92B7A6B5C964`
| ASUS_BW-16D1HT_3.02.bin | `84000B21` | `97F0EEABD0B675B4363B2C4749226016` | `F8DF5B579F25DA8D4E5AA5EF79F3005DAC5EB8C7` | `784E017FBACD2250FC37EE442869DB82CCC8D637D78126B570BA33C0256D1715`
2. Create a `build` folder and run `cmake -G Ninja ..` from the build folder.
3. Run `ninja`. The patched firmware files should be output in the `patched_firmware` folder.

## Flashing
Currently the recommended method to flash the drives is to use `sdftool` which comes as part of [MakeMKV](https://makemkv.com/). The BU40N firmware should work with all slim drives and the BW-16D1HT firmware should work with all desktop drives.

[Redumper](https://github.com/superg/redumper) has initial support for flashing, but cannot cross-flash drives yet.
