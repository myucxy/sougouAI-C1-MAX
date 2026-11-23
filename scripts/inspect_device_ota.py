"""Bounded regular-file snapshots and C208 OTA structure validation."""
import hashlib,os,stat,struct
from firmware_format import verify_resc
EXPECTED='4a91383c0ae06ba16aa0772c42619ced8e4800b1d1d48dc9c75a106ee060d6f7'
TRUSTED_REFERENCES={EXPECTED:'V0141'}
HOST_BYTES=0x800000

def snapshot(path,limit):
    fd=os.open(str(path),os.O_RDONLY|getattr(os,'O_NOFOLLOW',0))
    try:
        before=os.fstat(fd)
        if not stat.S_ISREG(before.st_mode):raise ValueError('不是普通文件')
        if before.st_size>limit:raise ValueError('文件超过本次检查大小上限')
        chunks=[];remaining=limit+1
        while remaining:
            chunk=os.read(fd,min(remaining,1024*1024))
            if not chunk:break
            chunks.append(chunk);remaining-=len(chunk)
        data=b''.join(chunks);after=os.fstat(fd)
        identity=lambda s:(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)
        if len(data)>limit or len(data)!=after.st_size or identity(before)!=identity(after):
            raise ValueError('文件在读取中变化，请等待复制完成后再检查')
        return data
    finally:os.close(fd)

def package_info(data,expected_digest=EXPECTED):
    info={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'matches_local_V0141':hashlib.sha256(data).hexdigest()==expected_digest,
          'recognized_variant':TRUSTED_REFERENCES.get(hashlib.sha256(data).hexdigest())}
    if len(data)<16:info['error']='容器少于16字节';return info
    host_n,slave_n=struct.unpack_from('<II',data)
    info.update(host_bytes=host_n,auxiliary_bytes=slave_n,declared_total_bytes=host_n+slave_n+16)
    if host_n!=HOST_BYTES or slave_n!=0 or len(data)!=host_n+16:
        info['error']='不符合已交付的完整8MiB主镜像容器布局';return info
    word,aux=struct.unpack_from('<II',data,len(data)-8);host=data[8:-8]
    info.update(trailer_version_word=hex(word),offered_version=word&0xffffff,
                app_version_getter=host[0x416c4:0x416c8].hex())
    wanted=word&0xffffff
    if word>>24!=ord('V') or aux or wanted>255 or host[0x416c4:0x416c8]!=bytes([wanted,0x20,0x70,0x47]) or struct.unpack_from('<I',host,0x416cc)[0]!=word:
        info['error']='包尾版本与系统版本字段不一致';return info
    try:info['resc']=[verify_resc(host,0x20000,0x700000),verify_resc(host,0x720000,0x80000)]
    except ValueError as e:info['error']=str(e)
    return info
