#!/usr/bin/env python3
"""Prepare per-device C208 TLS identity journals in a NEW private directory.
Does not contact the cloud or modify the recorder. Copy both files once, never restore consumed counters.
"""
import argparse,hashlib,hmac,json,os,struct,time,uuid
from pathlib import Path
FORMAT='<4sHHQ16s32sQ8s'


def record(sequence,client,seed,now):
    payload=struct.pack(FORMAT,b'XZID',1,112,sequence,client.bytes,seed,now,bytes(8))
    assert len(payload)==80
    return payload+hmac.digest(seed,payload,'sha256')


def prepare(destination):
    destination=Path(destination)
    if destination.exists():raise ValueError('输出目录必须不存在；不能覆盖已经使用的计数文件')
    destination.mkdir(parents=False,mode=0o700)
    seed=os.urandom(32);client=uuid.uuid4();now=int(time.time())
    for bank in (0,1):
        p=destination/f'XZAUTH{bank}.CFG';fd=os.open(p,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
        with os.fdopen(fd,'wb') as stream:stream.write(record(bank,client,seed,now));stream.flush();os.fsync(stream.fileno())
    text='''此目录是单台录音笔的私有身份文件，不要分享或复制给另一台设备。
首次启用官方接入前，将 XZAUTH0.CFG 和 XZAUTH1.CFG 一起复制到设备根目录。
设备使用后会更新文件计数；不要用本目录旧文件覆盖设备上的新文件，否则存在TLS随机数重复风险。
文件损坏时固件会拒绝联网。重新初始化必须生成全新目录、全新种子和UUID，并重新完成绑定。
此文件不包含云端令牌或账号密码；仍必须保密，里面有TLS随机数主密钥。
'''
    (destination/'说明.txt').write_text(text)
    return {'created':True,'client_id':str(client),'device_files_modified':False,'cloud_contacted':False,'seed_printed':False}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output-directory',type=Path,required=True);args=p.parse_args()
    print(json.dumps(prepare(args.output_directory),ensure_ascii=False,indent=2))
