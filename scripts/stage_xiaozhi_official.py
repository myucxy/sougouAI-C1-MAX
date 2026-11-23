#!/usr/bin/env python3
"""Stage the explicit official-activation TEST package and per-device identity.
Only touches SOGOUOTA.BIN and XZAUTH0/1.CFG, preserves any previous OTA in a backup.
Voice pipeline is integrated; physical microphone/speaker/realtime validation remains pending.
"""
import argparse,hashlib,hmac,json,os,struct,tempfile,time,uuid
from pathlib import Path
from prepare_xiaozhi_identity import record
from inspect_device_ota import snapshot,package_info
from check_current_ota import candidates
ROOT=Path(__file__).resolve().parent.parent
PACKAGE=ROOT/'_local/firmware/V0141/SOGOUOTA.BIN'
MAGIC=b'XZID\1\0\160\0'


def valid(data):
    return len(data)==112 and data[:8]==MAGIC and data[22]>>4==4 and data[24]&0xc0==0x80 and any(data[32:64]) and not any(data[72:80]) and hmac.compare_digest(hmac.digest(data[32:64],data[:80],'sha256'),data[80:])


def write_new(path,data):
    fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL|getattr(os,'O_NOFOLLOW',0),0o600)
    owned=os.fstat(fd)
    try:
        with os.fdopen(fd,'wb') as out:out.write(data);out.flush();os.fsync(out.fileno())
        if snapshot(path,len(data)+1)!=data:raise ValueError('写入读回校验失败')
    except Exception:
        if path.exists() and not path.is_symlink():
            current=path.stat()
            if (current.st_dev,current.st_ino)==(owned.st_dev,owned.st_ino):path.unlink()
        raise


def stage(device,package=PACKAGE,backup_parent=None):
    device=Path(device).resolve(strict=True);package=Path(package).resolve(strict=True)
    manifest=json.loads(package.with_name('manifest.json').read_text());data=snapshot(package,8388624)
    digest=hashlib.sha256(data).hexdigest()
    if manifest.get('version')!=141 or manifest.get('base_version')!=121 or not manifest.get('added_MP3') or digest!=manifest.get('package_sha256') or not manifest.get('native_official_activation_linked'):raise ValueError('不是已校验的 V0141 录音／小智／MP3 测试包')
    audit=package_info(data,digest)
    if audit.get('error'):raise ValueError('测试包布局或哈希校验失败')
    if not device.is_dir():raise ValueError('设备路径必须是目录')
    for p in (device/'SOGOUOTA.BIN',device/'XZAUTH0.CFG',device/'XZAUTH1.CFG'):
        if p.is_symlink():raise ValueError('拒绝写入设备符号链接')
    paths=[device/'XZAUTH0.CFG',device/'XZAUTH1.CFG'];exists=[p.exists() for p in paths]
    prepared=None
    if any(exists):
        if not all(exists):raise ValueError('小智身份文件不完整，拒绝重置已有随机数计数；需重新初始化身份后再测试')
        a,b=[snapshot(p,113) for p in paths]
        if not valid(a) or not valid(b) or a[16:64]!=b[16:64] or abs(struct.unpack_from('<Q',a,8)[0]-struct.unpack_from('<Q',b,8)[0])!=1:raise ValueError('小智身份文件损坏或计数不一致；拒绝用旧备份覆盖')
    else:
        client=uuid.uuid4();seed=os.urandom(32);now=int(time.time());prepared=[record(bank,client,seed,now) for bank in (0,1)]
    target=device/'SOGOUOTA.BIN';old=None
    if target.exists():old=snapshot(target,8388624)
    backup=None
    if old is not None and old!=data:
        parent=Path(backup_parent) if backup_parent else ROOT/'_local/backups/previous-ota'
        parent.mkdir(parents=True,exist_ok=True);backup=parent/('ota-before-'+time.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]+'.bin')
        write_new(backup,old)
    created=[];temporary=None
    try:
        if prepared:
            for p,blob in zip(paths,prepared):write_new(p,blob);created.append(p)
        if old!=data:
            temporary=device/('.c208-ota-'+uuid.uuid4().hex+'.tmp');write_new(temporary,data);os.replace(temporary,target);temporary=None
        if hashlib.sha256(snapshot(target,8388624)).hexdigest()!=digest:raise ValueError('最终升级包校验失败')
    except Exception:
        # A staging failure has not consumed counters; remove only identities created in this call.
        for p in created:
            if p.exists():p.unlink()
        if temporary is not None and temporary.exists():temporary.unlink()
        raise
    return {'staged':True,'version':141,'voice_ready':False,'voice_pipeline_integrated':True,'physical_voice_verified':False,'package_sha256':digest,'identity_created':prepared is not None,
        'existing_identity_counters_preserved':prepared is None,'previous_ota_backup':str(backup) if backup else None,
        'device_files_written':['SOGOUOTA.BIN']+(['XZAUTH0.CFG','XZAUTH1.CFG'] if prepared else []),
        'next':'安全弹出USB后按原本地升级流程安装；本版为 V0141 录音／小智／MP3；本包基于V0140加入时间行播放模式图标及四种模式，保留物理按键、设置不断歌、半屏音量及点文件过滤，RAM预留不变；先确认开机，再验证两页上滑／下滑和音量。'}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('device_root',type=Path,nargs='?');args=p.parse_args()
    roots=[args.device_root] if args.device_root else candidates()
    try:
        if len(roots)!=1:result={'staged':False,'reason':'未发现唯一录音笔存储盘，请接入USB文件传输模式后重试','candidates':list(map(str,roots))}
        else:result=stage(roots[0])
        print(json.dumps(result,ensure_ascii=False,indent=2))
    except (OSError,ValueError) as error:p.exit(2,str(error)+'\n')
