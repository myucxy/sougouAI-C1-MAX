#!/usr/bin/env python3
"""Build pinned upstream fixed-point libopus for the C208 Cortex-M4."""
import hashlib,json,re,subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from toolchain import tool
ROOT=Path(__file__).resolve().parent.parent
UP=ROOT/'third_party/opus-1.5.2'
OUT=ROOT/'_local/build/opus';OUT.mkdir(parents=True,exist_ok=True)
def sources():
 result=[]
 for file,names in [('opus_sources.mk',['OPUS_SOURCES']),('celt_sources.mk',['CELT_SOURCES']),('silk_sources.mk',['SILK_SOURCES','SILK_SOURCES_FIXED'])]:
  text=(UP/file).read_text()
  for name in names:
   match=re.search(r'^'+name+r'\s*=\s*((?:.*\\\n)*[^\n]*)',text,re.M);assert match,name
   result+=re.findall(r'[\w/]+\.c',match.group(1))
 return list(dict.fromkeys(result))
FLAGS=['--target=armv7m-none-eabi','-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-ffunction-sections','-fdata-sections','-DFIXED_POINT=1','-DDISABLE_FLOAT_API=1','-DVAR_ARRAYS=1','-DOPUS_BUILD=1','-DHAVE_STDINT_H=1','-DDISABLE_DEBUG_FLOAT=1','-DOPUS_ARM_ASM=0','-I'+str(ROOT/'patches/opus-compat'),'-I'+str(ROOT/'patches/xiaozhi-official/compat'),*['-I'+str(UP/p) for p in ['include','celt','silk','silk/fixed','.']]]
def main():
 def compile_one(name):
  p=OUT/(name.replace('/','_')+'.o');subprocess.run([tool('clang'),*FLAGS,'-c',str(UP/name),'-o',str(p)],check=True,capture_output=True);return p
 try:
  with ThreadPoolExecutor(max_workers=6) as pool:objects=list(pool.map(compile_one,sources()))
 except subprocess.CalledProcessError as e:print(e.stderr.decode());raise
 archive=OUT/'libopus-fixed.a';subprocess.run([tool('llvm-ar'),'rcs',str(archive),*map(str,objects)],check=True)
 report={'version':'1.5.2','flags':FLAGS,'source_count':len(objects),'library_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'bytes':archive.stat().st_size}
 (OUT/'build.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
