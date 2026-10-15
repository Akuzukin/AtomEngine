"""Decode the engine's own R10G10B10A2 framebuffer captures to inspectable PNGs."""
from pathlib import Path
import struct,sys,zlib,array
for name in sys.argv[1:]:
    path=Path(name)
    with path.open('rb') as stream:
        fmt,pitch,width,height=struct.unpack('<4I',stream.read(16));data=stream.read()
    if fmt!=24:raise ValueError(f'Expected R10G10B10A2 format24, got {fmt}')
    packed=array.array('I');packed.frombytes(data)
    rgb=bytearray()
    for y in range(height):
        rgb.append(0)
        for pixel in packed[y*(pitch//4):y*(pitch//4)+width]:
            rgb.extend(((pixel&1023)*255//1023,((pixel>>10)&1023)*255//1023,((pixel>>20)&1023)*255//1023))
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rgb))+chunk(b'IEND',b'')
    path.with_suffix('.png').write_bytes(png)
    print(path.with_suffix('.png'))
