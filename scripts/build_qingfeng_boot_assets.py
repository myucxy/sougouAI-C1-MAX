#!/usr/bin/env python3
"""Turn the selected 25 PNGs into cropped immutable native RGB565 image sources."""
from pathlib import Path
from PIL import Image
import hashlib,json,struct
ROOT=Path(__file__).resolve().parent.parent
ASSETS=ROOT/'assets/qingfeng-boot'
HEADER=ROOT/'patches/xiaozhi-main/boot_frames.h'
REGIONS=((0,0,240,140),(0,140,240,182),(0,182,240,208),(0,208,240,240))

def color565(rgb):return ((rgb[0]>>3)<<11)|((rgb[1]>>2)<<5)|(rgb[2]>>3)
def build():
    paths=sorted(ASSETS.glob('[0-9][0-9].png'));assert len(paths)==25
    images=[Image.open(p).convert('RGB') for p in paths];assert all(p.size==(240,240) for p in images)
    bg=color565((3,10,13));sources=[(240,240,struct.pack('<H',bg)*(240*240))];lookup={sources[0]:0};frames=[];frame_hashes=[]
    for image in images:
        pixels=list(image.get_flattened_data());quantized=[color565(c) for c in pixels];frame=[]
        for x0,y0,x1,y1 in REGIONS:
            changed=[(n%240,n//240) for n,p in enumerate(quantized) if x0<=n%240<x1 and y0<=n//240<y1 and p!=bg]
            if not changed:frame.append((0,0,None));continue
            left=min(x for x,y in changed);top=min(y for x,y in changed);right=max(x for x,y in changed)+1;bottom=max(y for x,y in changed)+1
            raw=b''.join(struct.pack('<H',quantized[y*240+x]) for y in range(top,bottom) for x in range(left,right))
            key=(right-left,bottom-top,raw)
            if key not in lookup:lookup[key]=len(sources);sources.append(key)
            frame.append((left,top,lookup[key]))
        # Exact reconstruction in native 16-bit color, including background clearing.
        decoded=[bg]*(240*240)
        for x,y,index in frame:
            if index is None:continue
            w,h,raw=sources[index];values=struct.unpack('<'+'H'*(w*h),raw)
            for row in range(h):decoded[(y+row)*240+x:(y+row)*240+x+w]=values[row*w:(row+1)*w]
        assert decoded==quantized
        frame_hashes.append(hashlib.sha256(struct.pack('<'+'H'*len(quantized),*quantized)).hexdigest())
        frames.append(frame)
    lines=['/* Generated from the user-selected wind animation; RGB565, no RAM pixels. */','#ifndef C208_BOOT_FRAMES_H','#define C208_BOOT_FRAMES_H','#include <stdint.h>','#define QF_BOOT_FRAMES 25u','#define QF_BOOT_PARTS 4u','#define QF_BOOT_PERIOD_MS 80u','struct qf_image {uint32_t header,bytes;const uint16_t *pixels;};','struct qf_part {uint16_t x,y;const struct qf_image *image;};']
    for i,(w,h,raw) in enumerate(sources):
        values=struct.unpack('<'+'H'*(w*h),raw);lines.append(f'static const uint16_t qf_pixels_{i}[{w*h}]={{')
        for at in range(0,len(values),16):lines.append(','.join(f'0x{v:04x}' for v in values[at:at+16])+',')
        lines.extend(['};',f'static const struct qf_image qf_image_{i}={{0x{4|(w<<10)|(h<<21):08x}u,{len(raw)}u,qf_pixels_{i}'+'};'])
    lines.append('static const struct qf_part qf_frames[QF_BOOT_FRAMES][QF_BOOT_PARTS]={')
    for frame in frames:lines.append('{'+','.join('{'+f'{x},{y},'+('0' if i is None else f'&qf_image_{i}')+'}' for x,y,i in frame)+'},')
    lines.extend(['};','#endif','']);HEADER.write_text('\n'.join(lines))
    metadata={'name':'清风拂尘','author':'清风惹尘丶','caption':'风过，尘起。','version':130,'screen':[240,240],'frames':25,'period_ms':80,'original_boot_timer_ms':2000,'background_rgb565':bg,'pixel_heap_bytes':0,'unique_images':len(sources),'readonly_pixel_bytes':sum(len(raw) for w,h,raw in sources),'native_image_format':4,'sources':[{'filename':p.name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths],'images':[{'width':w,'height':h,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()} for w,h,raw in sources],'frame_parts':frames,'frame_RGB565_sha256':frame_hashes,'RGB565_reconstruction_exact_all_frames':True,'header_sha256':hashlib.sha256(HEADER.read_bytes()).hexdigest()}
    (ASSETS/'manifest.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({k:metadata[k] for k in ('frames','period_ms','unique_images','readonly_pixel_bytes','pixel_heap_bytes','RGB565_reconstruction_exact_all_frames')},ensure_ascii=False,indent=2))
if __name__=='__main__':build()
