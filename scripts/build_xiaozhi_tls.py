#!/usr/bin/env python3
"""Cross-compile reviewed portable TLS/protocol objects; no firmware flashing.
Archive is development-only until C208 entropy/time/socket/root bindings exist.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from toolchain import tool
import argparse,hashlib,json,shutil,struct,subprocess,tempfile
from firmware_format import ROOT,elf_sections
UPSTREAM=ROOT/'third_party/bearssl-0.6'
OUT=ROOT/'_local/build/tls'


def build(out=OUT):
    out.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='c208-tls-arm-') as tmp:
        tmp=Path(tmp);headers=tmp/'include';headers.mkdir()
        (headers/'string.h').write_text('''#include <stddef.h>
void *memcpy(void *,const void *,size_t);void *memmove(void *,const void *,size_t);void *memset(void *,int,size_t);
int memcmp(const void *,const void *,size_t);int strcmp(const char *,const char *);int strncmp(const char *,const char *,size_t);
size_t strlen(const char *);char *strchr(const char *,int);
''')
        flags=['--target=armv7m-none-eabi','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft','-Os','-ffreestanding','-fno-builtin',
            '-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-ffunction-sections','-fdata-sections',
            '-DBR_USE_URANDOM=0','-DBR_USE_UNIX_TIME=0','-DBR_USE_WIN32_TIME=0','-DBR_USE_WIN32_RAND=0','-DBR_RDRAND=0','-DBR_LOMUL=1',
            '-I'+str(headers),'-I'+str(UPSTREAM/'inc'),'-I'+str(UPSTREAM/'src')]
        sources=sorted((UPSTREAM/'src').rglob('*.c'))+[ROOT/'patches/xiaozhi-official/tls.c',ROOT/'patches/xiaozhi-official/official.c',ROOT/'patches/xiaozhi-official/http.c']
        def compile_one(item):
            i,p=item;obj=tmp/(str(i)+'.o');result=subprocess.run([tool('clang'),*flags,'-c',str(p),'-o',str(obj)],capture_output=True,text=True)
            if result.returncode:raise RuntimeError(str(p)+'\n'+result.stderr)
            return obj
        with ThreadPoolExecutor(max_workers=6) as pool:objects=list(pool.map(compile_one,enumerate(sources)))
        ar=tool('llvm-ar')
        subprocess.run([ar,'rcs',str(out/'libxiaozhi-official.a'),*map(str,objects)],check=True,capture_output=True)
        # Cortex-M helpers are compiled as Thumb; Android ARM-state libraries are incompatible.
        obj=tmp/'libc.o';subprocess.run([tool('clang'),*flags,'-c',str(ROOT/'patches/xiaozhi-official/arm_runtime.c'),'-o',str(obj)],check=True,capture_output=True)
        abi=tmp/'abi.o';subprocess.run([tool('clang'),*flags,'-c',str(ROOT/'patches/xiaozhi-official/arm_runtime.S'),'-o',str(abi)],check=True,capture_output=True)
        measure=tmp/'size.c';measure.write_text('''#include "tls.h"
const unsigned xo_tls_size_constant=sizeof(struct xo_tls);
''')
        sizeobj=tmp/'size.o';subprocess.run([tool('clang'),*flags,'-I'+str(ROOT/'patches/xiaozhi-official'),'-c',str(measure),'-o',str(sizeobj)],check=True,capture_output=True)
        linker=tool('ld.lld')
        script=tmp/'link.ld';script.write_text('SECTIONS { . = 0x18600000; .text : { *(.text*) *(.rodata*) } . = 0x20030000; .data : { *(.data*) } .bss : { *(.bss*) } /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) } }')
        elf=out/'size-only.elf'
        roots=['xo_tls_init','xo_tls_size_constant','xo_parse_bootstrap','xo_bootstrap_body','xo_headers','xo_hello','xo_listen','xo_abort','xo_audio_pack','xo_audio_unpack','xo_http_init','xo_http_feed','xo_http_eof','__aeabi_uldivmod']
        command=[linker,'--gc-sections','--entry=xo_tls_init',*['--undefined='+s for s in roots],'-T',str(script),str(obj),str(abi),str(sizeobj),str(out/'libxiaozhi-official.a'),'-o',str(elf)]
        result=subprocess.run(command,capture_output=True,text=True)
        if result.returncode:raise RuntimeError(result.stderr)
        blob=elf.read_bytes();named,sections=elf_sections(blob);counts={name:s[5] for name,s in named if name in ('.text','.data','.bss')}
        context=None
        for name,s in named:
            if s[1]!=2:continue
            st=sections[s[6]];strings=blob[st[4]:st[4]+st[5]]
            for at in range(s[4],s[4]+s[5],s[9]):
                no,value,size,info,other,owner=struct.unpack_from('<IIIBBH',blob,at)
                if strings[no:].split(b'\0')[0]==b'xo_tls_size_constant':
                    sec=sections[owner];context=struct.unpack_from('<I',blob,sec[4]+value-sec[3])[0]
        report={'target':'Cortex-M4 Thumb ARMv7M soft float','library_sources':len(sources),'size_only_elf_sections':counts,
                'TLS_context_bytes_including_record_buffers':context,
                'adapter_source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'patches/xiaozhi-official/tls.c',ROOT/'patches/xiaozhi-official/tls.h')},'bearssl_source_version':'0.6',
                'library_sha256':hashlib.sha256((out/'libxiaozhi-official.a').read_bytes()).hexdigest(),'linked_into_release_firmware':False,
                'only_Thumb_functions_expected':True,'hardware_entropy_time_and_socket_bindings':'not established','certificate_verification_disabled':False,'TLS_hardware_execution':False}
        evidence=ROOT/'_local/build/tls';evidence.mkdir(parents=True,exist_ok=True);(evidence/'arm-tls-size.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))

if __name__=='__main__':build()
