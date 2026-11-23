#!/usr/bin/env python3
"""Ensure public verification rejects corrupt artifacts and private tracked files."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parent.parent/'scripts'))
from verify_release import verify
from check_public_tree import check


class ReleaseTools(unittest.TestCase):
    def test_reject_wrong_size(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'SOGOUOTA.BIN'
            p.write_bytes(b'RKFP')
            with self.assertRaisesRegex(ValueError, '8388624'):
                verify(p)

    def test_reject_changed_ota(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'SOGOUOTA.BIN'
            p.write_bytes(bytes(8388624))
            with self.assertRaisesRegex(ValueError, 'SHA-256'):
                verify(p)

    def test_reject_private_artifact_even_if_git_tracks_it(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            p = root/'_local/device.CFG'
            p.parent.mkdir()
            p.write_bytes(b'fixture')
            with patch('check_public_tree.ROOT', root), patch('check_public_tree.public_files', return_value=[p]):
                with self.assertRaisesRegex(ValueError, 'Private directory tracked'):
                    check(tracked=True)

    def test_reject_external_symlink(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            p = root/'external.txt'
            p.symlink_to('/outside/not-required')
            with patch('check_public_tree.ROOT', root), patch('check_public_tree.public_files', return_value=[p]):
                with self.assertRaisesRegex(ValueError, 'symlink'):
                    check()


if __name__ == '__main__':
    unittest.main()
