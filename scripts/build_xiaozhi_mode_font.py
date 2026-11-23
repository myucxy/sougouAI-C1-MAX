#!/usr/bin/env python3
"""Build a small 1bpp LVGL v6 subset from the shipped C208 font, no external font."""
from pathlib import Path
import hashlib,json
from xiaozhi_ui_font import glyph,LINE_HEIGHT,BASELINE
ROOT=Path(__file__).resolve().parent.parent
TEXT=['当前：按住说话','当前：单击开停','当前：连续对话','按住录音键说话','点击开始或发送','单击开始或结束']


def build():
    chars=sorted(set(''.join(TEXT)));first=ord(chars[0]);bitmap=bytearray();descriptors=[(0,0,0,0,0,0)]
    for c in chars:
        g=glyph(c);w=max(1,round(g['advance']*.6));h=18;small=[]
        for y in range(h):
            for x in range(w):
                # Area sampling keeps thin strokes visible when scaling 30px to 18px.
                values=[]
                for sy in range(y*LINE_HEIGHT//h,(y+1)*LINE_HEIGHT//h):
                    for sx in range(x*g['advance']//w,(x+1)*g['advance']//w):
                        px=sx-g['dx'];py=sy-(LINE_HEIGHT-BASELINE-g['height']-g['dy'])
                        n=py*g['width']+px
                        values.append(int(0<=px<g['width'] and 0<=py<g['height'] and bool(g['bitmap'][n//8]&(128>>(n%8)))))
                small.append(int(sum(values)*3>=len(values)))
        descriptors.append((len(bitmap),w*16,w,h,0,-3))
        for start in range(0,len(small),8):
            value=0
            for i,bit in enumerate(small[start:start+8]):value|=bit<<(7-i)
            bitmap.append(value)
    out='''/* Generated from original C208 30px glyphs at 18px; scripts/build_xiaozhi_mode_font.py. */
#ifndef C208_MODE_FONT_H
#define C208_MODE_FONT_H
#include <stdint.h>
static const uint8_t ai_mode_font_bitmap[]={'''+','.join(str(n) for n in bitmap)+'''};
static const struct {uint32_t at,advance;uint8_t width,height;int8_t dx,dy;} ai_mode_font_glyphs[]={'''+','.join('{'+','.join(str(n) for n in row)+'}' for row in descriptors)+'''};
static const uint16_t ai_mode_font_codes[]={'''+','.join(str(ord(c)-first) for c in chars)+'''};
static const struct {uint32_t start;uint16_t length,first;const void *codes,*offsets;uint16_t count;uint8_t type,reserved;} ai_mode_font_cmap={'''+str(first)+','+str(ord(chars[-1])-first+1)+',1,ai_mode_font_codes,0,'+str(len(chars))+''',2,0};
/* The original callbacks update the two cache words: the descriptor must be RAM. */
static struct {const void *bitmap,*glyphs,*cmaps,*kern;uint32_t flags,cache1,cache2;} ai_mode_font_desc;
static void ai_mode_font_init(void){if(!ai_mode_font_desc.bitmap){ai_mode_font_desc.bitmap=ai_mode_font_bitmap;ai_mode_font_desc.glyphs=ai_mode_font_glyphs;ai_mode_font_desc.cmaps=&ai_mode_font_cmap;ai_mode_font_desc.flags=0x04010000;}}
static const struct {uintptr_t metrics,bitmap;uint8_t height,baseline;uint16_t flags;const void *desc,*user;} ai_mode_font={0x180935d9,0x180933e1,18,3,0,&ai_mode_font_desc,0};
#endif
'''
    return out,{'original_font':'0x2000d2c4','original_font_height':30,'subset_height':18,'characters':''.join(chars),'strings':TEXT,'bitmap_bytes':len(bitmap),'source':'original shipped bitmap font; 3/5 area downsample, 1bpp'}

if __name__=='__main__':
    text,report=build();path=ROOT/'_local/inputs/mode_font.h';path.write_text(text)
    report['header_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
    (path.with_suffix('.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(report,ensure_ascii=False,indent=2))
