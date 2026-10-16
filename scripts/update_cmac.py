import argparse
import struct
from pathlib import Path

MT1959_CMAC_KEY = bytes.fromhex('BD209408E35E526A36235234434FE8AB')
cmac_range_fmt = '<3L16s'
cmac_range_size = struct.calcsize(cmac_range_fmt)

parser = argparse.ArgumentParser()
parser.add_argument("firmware_file")
args = parser.parse_args()

# MT1939 uses native LE32 word sums instead of the MT1959 CMAC. Match the
# image identity rather than its filename, so other profiles keep the CMAC path.
image = bytearray(Path(args.firmware_file).read_bytes())
identity = b'ASUS    BW-12B1LT       1.032011/04/18 21:15    '
if len(image) == 0x200000 and image[0x1c6f40:0x1c6f40 + len(identity)] == identity:
    for start, end, offset in ((0x10000, 0x1cfdf8, 0x1cfff8),
                               (0x1d0000, 0x1dfff8, 0x1dfff8)):
        checksum = sum(word[0] for word in struct.iter_unpack('<I', image[start:end])) & 0xffffffff
        struct.pack_into('<I', image, offset, checksum)
    Path(args.firmware_file).write_bytes(image)
    print('MT1939 image: native word sums set; MT1959 CMAC skipped')
    raise SystemExit(0)
del image

from Crypto.Hash import CMAC
from Crypto.Cipher import AES

def calc_cmac(cmac_struct):
    enabled, start, end, original_cmac = struct.unpack(cmac_range_fmt, cmac_struct)

    if enabled == 0xFFFFFFFF:
        return None

    firmware_file.seek(start)
    cmac_range = bytearray(firmware_file.read(end - start + 1))
    
    # Reverse
    for i in range(0, len(cmac_range), 16):
        cmac_block = cmac_range[i:i+16]
        cmac_range[i:i+16] = cmac_block[::-1]

    cmac = CMAC.new(MT1959_CMAC_KEY, cmac_range, ciphermod=AES)
    print("CMAC for block starting at 0x{:X} ending at 0x{:X}: {}".format(start, end, cmac.hexdigest()))
    return cmac.digest()


with open(args.firmware_file, 'r+b') as firmware_file:
    firmware_file.seek(0x10400)
    cmac_ranges = firmware_file.read(cmac_range_size * 16)

    for i in range(0, 16):
        cmac_struct = cmac_ranges[i*cmac_range_size:i*cmac_range_size+cmac_range_size]
        cmac = calc_cmac(cmac_struct)
        if cmac is not None:
            cmac = bytearray(cmac)
            cmac.reverse()
            firmware_file.seek(0x10400 + i*cmac_range_size + 12)
            firmware_file.write(cmac)
    

        
    firmware_file.close()
