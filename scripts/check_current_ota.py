#!/usr/bin/env python3
"""Inspect current C208 OTA files and boot markers without changing device files.
Reads only OTA/log filenames at the selected root; credentials/recordings are not
opened. User-storage files do not establish the currently executing SPI image.
"""
import argparse
from datetime import datetime,timezone
import hashlib,json,os,re,struct
from pathlib import Path
from inspect_device_ota import snapshot,package_info,HOST_BYTES

ROOT=Path(__file__).resolve().parent.parent
REFERENCE=ROOT/'_local/firmware/V0141/SOGOUOTA.BIN'
EXPECTED='4a91383c0ae06ba16aa0772c42619ced8e4800b1d1d48dc9c75a106ee060d6f7'
NAMES={'sogouota.bin','mfota.bin','fota.bin','bfota.bin','btstart.txt','xzdbg.txt','xztime.txt','xzaudio.txt','xzmem.txt','btdbg.txt','wifidbg.txt'}


def files_at(root):
    files=[]
    with os.scandir(root) as entries:
        for index,p in enumerate(entries):
            if index>=4096:raise ValueError('根目录条目超过检查上限')
            name=p.name.lower()
            if name in NAMES or name.startswith('sogouota'):files.append(Path(p.path))
    return sorted(files,key=lambda p:p.name)


def candidates(volumes=Path('/Volumes')):
    if not volumes.exists():return []
    found=[]
    for p in sorted(volumes.iterdir()):
        if p.is_symlink() or not p.is_dir():continue
        try:
            if any(f.name.lower() in NAMES for f in files_at(p)):found.append(p)
        except (OSError,ValueError):continue
    return found


def catalog():
    rows=[]
    for p in sorted((ROOT/'_local/firmware').glob('*/manifest.json')):
        try:m=json.loads(p.read_text())
        except (ValueError,OSError):continue
        if not isinstance(m.get('version'),int) or not re.fullmatch('[0-9a-f]{64}',str(m.get('package_sha256',''))):continue
        rows.append({'version':m['version'],'sha256':m['package_sha256'],'path':p.parent/'SOGOUOTA.BIN'})
    return rows


def version_getter(data):
    if len(data)!=HOST_BYTES:return None
    if data[0x416c5:0x416c8]!=bytes.fromhex('207047'):return None
    value=data[0x416c4]
    return value if struct.unpack_from('<I',data,0x416cc)[0]==0x56000000|value else None


def inspect(root,*,copied_tree=False,reference=REFERENCE):
    root=root.resolve(strict=True)
    if not root.is_dir():raise ValueError('输入必须为设备根目录或该根目录的副本')
    reference_bytes=snapshot(reference,HOST_BYTES+16);digest=hashlib.sha256(reference_bytes).hexdigest();known=catalog()
    if digest!=EXPECTED and not any(r['sha256']==digest for r in known):raise ValueError('参考包未匹配本地交付清单')
    info=package_info(reference_bytes,digest)
    if info.get('error'):raise ValueError('参考包校验失败：'+info['error'])
    expected_version=info['offered_version'];host_digest=hashlib.sha256(reference_bytes[8:-8]).hexdigest()
    result={'status':'inspected','input_root':str(root),'input_kind':'copied-tree' if copied_tree else 'user-selected-directory',
        'device_files_modified':False,'expected_version':expected_version,'expected_package_sha256':digest,
        'expected_host_sha256':host_digest,'files':[],'findings':[],
        'currently_running_spi_version':'unknown','spi_boot_metadata_observed':False}
    for p in files_at(root):
        row={'name':p.name}
        try:
            if p.is_symlink():raise ValueError('跳过符号链接')
            row['modified_utc']=datetime.fromtimestamp(p.stat().st_mtime,timezone.utc).isoformat()
            kind=p.name.lower()
            if kind in ('btdbg.txt','btstart.txt','xzdbg.txt','xztime.txt','xzaudio.txt','xzmem.txt','wifidbg.txt'):
                data=snapshot(p,256*1024);text=data.decode('utf-8','replace')
                row.update(bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),boot_markers=re.findall(r'\bBOOT_V\d{4}\b',text)[-20:])
                if kind in ('btdbg.txt','btstart.txt'):
                    row['status_records']=[line for line in text.splitlines() if re.fullmatch(r'(?:BOOT_V\d{4}|B\d{2}|EARLY_FAIL=B\d{2}|(?:BT_START|BT_STOP|APP_[A-Z_]+|STACK_[A-Z_]+|MGMT_[A-Z_]+|BTM_STATE|UART_ACTIVE|NATIVE_ACTIVE|HEAP_[A-Z_]+|START_[A-Z_]+|LATE_ENABLE_RECOVERED|L2CAP_[A-Z_]+|STATE|ERROR|SCAN_[A-Z_]+|SEARCH_CLICK)(?:=[0-9a-fA-F]{8})?)',line)][-60:]
                if kind in ('btdbg.txt','btstart.txt'):
                    row['note']='保留旧蓝牙日志的状态记录；V0141不声明支持蓝牙耳机。'
                if kind=='xzdbg.txt':
                    row['official_records']=[line for line in text.splitlines() if re.fullmatch(r'(?:BOOT_V\d{4}|(?:OFFICIAL_STATE|ERROR|TLS_ERROR|HTTP_STATUS)=[0-9a-fA-F]{8})',line)][-20:]
                    row['note']='官方连接历史诊断；不单独证明当前状态、账号绑定或语音可用。'
                if kind=='xztime.txt':
                    row['clock_records']=[line for line in text.splitlines() if re.fullmatch(r'(?:BOOT_V\d{4}|(?:TIME_STATE|ERROR|UTC_HIGH|UTC_LOW|TICKS)=[0-9a-fA-F]{8})',line)][-20:]
                    row['note']='历史自动校时结果；TIME_STATE=2 为RTC写入并读回成功。'
                if kind=='xzaudio.txt':
                    row['audio_records']=[line for line in text.splitlines() if re.fullmatch(r'(?:BOOT_V\d{4}|(?:AUDIO_ERROR|CAPTURE_STEP|ELAPSED_MS|READ_CALLS|PCM_FRAMES|EMPTY_READS|DRIVER_STATE|HW_PTR|APP_PTR|BUFFER_FRAMES|BOUNDARY)=[0-9a-fA-F]{8})',line)][-20:]
                    row['note']='历史采音故障计数；不含PCM、录音内容或网络身份。'
                if kind=='xzmem.txt':
                    row['memory_records']=[line for line in text.splitlines() if re.fullmatch(r'(?:BOOT_V\d{4}|(?:ALLOC_STAGE|REQUEST_BYTES|HEAP_TOTAL|HEAP_USED|HEAP_FREE|HEAP_PEAK)=[0-9a-fA-F]{8})',line)][-20:]
                    row['note']='历史运行堆计数；HEAP_FREE不等于最大连续空闲块。'
                row['historical_only']=True
            elif kind in ('sogouota.bin','mfota.bin'):
                data=snapshot(p,HOST_BYTES+16);row.update(package_info(data,digest))
                row['matches_expected_package']=row.pop('matches_local_v0097')
                row['exact_local_build_matches']=[{'version':k['version'],'package':str(k['path'].relative_to(ROOT))} for k in known if k['sha256']==row['sha256']]
                row['loaded_by_local_host_ota']=kind=='sogouota.bin'
                if kind=='sogouota.bin':
                    if row.get('error'):result['findings'].append('SOGOUOTA.BIN 的布局、版本字段或哈希校验异常。')
                    elif row['matches_expected_package']:result['findings'].append(f'设备目录中的 SOGOUOTA.BIN 与选定 V{expected_version:04d} 包逐字节一致；仍不证明已写入运行。')
                    else:result['findings'].append(f"SOGOUOTA.BIN 实际声明 V{row['offered_version']:04d}，与选定 V{expected_version:04d} 包不一致。")
            elif kind in ('fota.bin','bfota.bin'):
                data=snapshot(p,HOST_BYTES);value=hashlib.sha256(data).hexdigest()
                row.update(bytes=len(data),sha256=value,matches_expected_host=len(data)==HOST_BYTES and value==host_digest,app_version=version_getter(data))
                if row['app_version'] is not None:
                    row['resc']=[]
                    from firmware_format import verify_resc
                    try:row['resc']=[verify_resc(data,0x20000,0x700000),verify_resc(data,0x720000,0x80000)]
                    except ValueError as e:row['integrity_error']=str(e)
                    matches=[]
                    for k in known:
                        if k['version']!=row['app_version']:continue
                        local=snapshot(k['path'],HOST_BYTES+16)
                        if hashlib.sha256(local).hexdigest()==k['sha256'] and hashlib.sha256(local[8:-8]).hexdigest()==value:
                            matches.append(str(k['path'].relative_to(ROOT)))
                    row['exact_local_host_matches']=matches
                if kind=='fota.bin':result['findings'].append('FOTA.BIN 为拆包后的候选主镜像；其版本和完整性已列出，不能据此认定已写入设备。')
            else:
                row['loaded_by_local_host_ota']=False
                row['note']='原本地更新按 SOGOUOTA.BIN 精确文件名读取；此重名/带后缀文件不按该名称加载。'
        except (OSError,ValueError) as e:row['error']=str(e)
        result['files'].append(row)
    for row in result['files']:
        diag=row.get('startup_diagnostics')
        if diag and diag['timeout_recorded'] and not diag['late_enable_recovery_recorded']:
            version=diag['boot_version_marker'];version_text=f'V{version:04d}' if version is not None else '未知版本'
            result['findings'].append(f'蓝牙日志（{version_text}）记录启动超时；根因仍未知，需结合分阶段字段。')
            result['findings'].extend(diag['observations'])
            result['findings'].extend(diag['next_diagnostic_branches'])
    if not any(r['name'].lower()=='sogouota.bin' for r in result['files']):result['findings'].append('当前未见 SOGOUOTA.BIN；原流程会清理文件，缺失不能证明升级成功或失败。')
    result['findings'].append('BTDBG/BTSTART/WIFIDBG 启动记录可能来自此前运行；旧 Wi-Fi 日志版本不能替代本机版本信息。')
    result['findings'].append('此检查不读取 SPI，不能确认当前固件、恢复标志或自动回滚；保留这些项目为未知。')
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('device_root',type=Path,nargs='?')
    p.add_argument('--summary',action='store_true',help='输出便于阅读的检查摘要');p.add_argument('--copied-tree',action='store_true');p.add_argument('--reference-package',type=Path,default=REFERENCE);p.add_argument('--output',type=Path);a=p.parse_args()
    if a.copied_tree and a.device_root is None:p.error('--copied-tree 需要指定目录')
    roots=[a.device_root] if a.device_root else candidates()
    try:
        if not roots:result={'status':'device_not_mounted','device_files_modified':False,'findings':['未发现带 OTA/调试日志的设备卷。请将录音笔接入 USB 文件传输模式；或把指定文件放进新目录并传入该路径。']}
        elif len(roots)>1:result={'status':'multiple_candidates','candidate_roots':list(map(str,roots)),'device_files_modified':False,'findings':['存在多个候选目录，请指定录音笔目录。']}
        else:result=inspect(roots[0],copied_tree=a.copied_tree,reference=a.reference_package)
        text=json.dumps(result,ensure_ascii=False,indent=2)+'\n'
        if a.output:
            destination=a.output.resolve()
            if any(r.resolve()==destination or r.resolve() in destination.parents for r in roots):raise ValueError('报告不能写入被检查目录')
            destination.parent.mkdir(parents=True,exist_ok=True);destination.write_text(text)
        if a.summary:
            for finding in result['findings']:print(finding)
            for row in result.get('files',[]):
                version=row.get('offered_version',row.get('app_version'))
                if version is not None:print(f"{row['name']}：文件版本 V{version:04d}")
                if row.get('error'):print(f"{row['name']}：{row['error']}")
            if a.output:print('检查报告：'+str(destination))
            print('设备文件未修改。当前 SPI 运行版本仍需结合设备信息确认。')
        else:print(text,end='')
    except (OSError,ValueError) as e:p.exit(2,str(e)+'\n')

if __name__=='__main__':main()
