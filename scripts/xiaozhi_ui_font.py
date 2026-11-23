"""Original shipped C208 bitmap font metrics for UI validation/preview (LVGL v6)."""
import struct
import hashlib
from pathlib import Path
ORIGINAL = (Path(__file__).resolve().parent.parent / '_local/inputs/sg-bak.bin').read_bytes()
assert hashlib.sha256(ORIGINAL).hexdigest() == '20d63efd687fdfbf76ae979bc04ca472162d07d5f6addc1c53fc9e21ed7cb204', 'Original font image mismatch'
def runtime_offset(address):
    if 0x18000000 <= address < 0x18800000:
        return address - 0x18000000
    if 0x2000af20 <= address < 0x2000e224:
        return address - 0x2000af20 + 0x585460
    raise ValueError(f'Unsupported font address 0x{address:x}')
FONT=runtime_offset(0x2000d2c4)
LINE_HEIGHT=ORIGINAL[FONT+8]
BASELINE=ORIGINAL[FONT+9]

def glyph(char):
    desc=runtime_offset(struct.unpack_from('<I',ORIGINAL,FONT+12)[0])
    bitmap,descriptors,cmaps,_,meta=struct.unpack_from('<5I',ORIGINAL,desc)
    assert meta>>26&15==1 and not meta>>31
    for i in range(meta>>16&1023):
        start,length,first,codes,offsets,num,kind,_=struct.unpack_from('<IHHIIHBB',ORIGINAL,runtime_offset(cmaps)+20*i)
        if not start<=ord(char)<start+length:continue
        relative=ord(char)-start
        if kind==0:index=first+relative
        elif kind==2:
            values=struct.unpack_from('<'+'H'*num,ORIGINAL,runtime_offset(codes))
            if relative not in values:continue
            index=first+values.index(relative)
        else:raise ValueError('unsupported cmap')
        at,advance,width,height,dx,dy=struct.unpack_from('<IIBBbb',ORIGINAL,runtime_offset(descriptors)+index*12)
        n=(width*height+7)//8;at=runtime_offset(bitmap)+at
        return {'advance':(advance+8)>>4,'width':width,'height':height,'dx':dx,'dy':dy,'bitmap':ORIGINAL[at:at+n]}
    raise ValueError('Original font missing '+repr(char))

def lines(text,width=224):
    result=[''];used=0
    for char in text:
        if char=='\n':result.append('');used=0;continue
        n=glyph(char)['advance']
        if used+n>width and result[-1]:result.append('');used=0
        result[-1]+=char;used+=n
    return result

def height(text,width=224):return len(lines(text,width))*LINE_HEIGHT+(len(lines(text,width))-1)*2
