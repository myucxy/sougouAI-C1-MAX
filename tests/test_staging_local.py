#!/usr/bin/env python3
"""Verify identity counters cannot be reset by restaging and prior OTA/data survive."""
from pathlib import Path
from unittest.mock import patch
import hashlib,json,os,tempfile
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'scripts'))
from firmware_format import ROOT
from stage_xiaozhi_official import stage


def main():
    package=(ROOT/'_local/firmware/V0141/SOGOUOTA.BIN').read_bytes()
    manifest=json.loads((ROOT/'_local/firmware/V0141/manifest.json').read_text())
    with tempfile.TemporaryDirectory(prefix='c208-stage-') as tmp:
        tmp=Path(tmp);local=tmp/'package';local.mkdir();binary=local/'SOGOUOTA.BIN';binary.write_bytes(package);(local/'manifest.json').write_text(json.dumps(manifest))
        device=tmp/'device';device.mkdir();(device/'recording.wav').write_bytes(b'private-audio');(device/'C208WIF0.CFG').write_bytes(b'private-password')
        old=b'previous-OTA-bytes';(device/'SOGOUOTA.BIN').write_bytes(old)
        result=stage(device,binary,tmp/'backups');assert result['identity_created'] and not result['voice_ready']
        assert Path(result['previous_ota_backup']).read_bytes()==old
        original={name:(device/name).read_bytes() for name in ('XZAUTH0.CFG','XZAUTH1.CFG')}
        result=stage(device,binary,tmp/'backups');assert not result['identity_created'] and result['existing_identity_counters_preserved']
        assert original=={name:(device/name).read_bytes() for name in original}
        assert (device/'recording.wav').read_bytes()==b'private-audio' and (device/'C208WIF0.CFG').read_bytes()==b'private-password'
        (device/'XZAUTH0.CFG').unlink()
        try:stage(device,binary,tmp/'backups')
        except ValueError:pass
        else:raise AssertionError('partial identity was silently reset')
        (device/'XZAUTH0.CFG').symlink_to(local/'SOGOUOTA.BIN')
        try:stage(device,binary,tmp/'backups')
        except ValueError:pass
        else:raise AssertionError('symbolic link accepted')
        broken=tmp/'write-failure';broken.mkdir();(broken/'SOGOUOTA.BIN').write_bytes(old)
        original_sync=os.fsync;calls={'n':0}
        def fail_second_identity(fd):
            calls['n']+=1
            # Prior OTA backup is synced first, first identity second, second identity third.
            if calls['n']==3:raise OSError('fixture interrupted write')
            return original_sync(fd)
        try:
            with patch('stage_xiaozhi_official.os.fsync',fail_second_identity):stage(broken,binary,tmp/'backups')
        except OSError:pass
        else:raise AssertionError('write failure hidden')
        assert not list(broken.glob('XZAUTH*.CFG')) and (broken/'SOGOUOTA.BIN').read_bytes()==old
    out=ROOT/'_local/reports';out.mkdir(parents=True,exist_ok=True);(out/'staging-validation.json').write_text(json.dumps({'package_sha256':manifest['package_sha256'],
        'existing_counters_never_reset':True,'old_OTA_backed_up':True,'recordings_and_wifi_credentials_untouched':True,
        'partial_identity_and_symlinks_rejected':True,'interrupted_provisioning_rolls_back_own_new_files':True,'real_device_written':False},indent=2)+'\n')
    print('PASS: safe staging, existing identity counters preserved, prior OTA backup, unrelated data untouched and interrupted new identity cleanup.')

if __name__=='__main__':main()
