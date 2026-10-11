# SPDX-License-Identifier: GPL-2.0-or-later
import importlib.util
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('steam', Path(__file__).resolve().parents[1] / 'tools/linux/add-to-steam.py')
steam = importlib.util.module_from_spec(spec)
spec.loader.exec_module(steam)

class SteamTests(unittest.TestCase):
    def test_migration_and_local_artwork_preserve_identity(self):
        with tempfile.TemporaryDirectory() as tmp, patch.dict(os.environ, HOME=tmp), patch.object(steam, 'steam_closed'):
            config = Path(tmp) / '.steam/steam/userdata/1/config'
            config.mkdir(parents=True)
            exe = tmp + '/current/gtetrinet-ng.sh'
            appid = 0x89abcdef
            entry = steam.new_entry(0, appid, 'gtetrinet-ng', exe, tmp, '', '')
            path = config / 'shortcuts.vdf'
            original = b'\0shortcuts\0' + entry + b'\x08\x08'
            path.write_bytes(original)
            artwork = Path(tmp) / 'art'
            artwork.mkdir()
            sources = {}
            for suffix in steam.ARTWORK:
                sources[suffix] = artwork / ('steam' + suffix)
                sources[suffix].write_bytes(('original image ' + suffix).encode())
            with patch.object(steam, 'ARTWORK', sources):
                steam.main(['GTetrinet NG — TetriNET', exe, tmp, ''])
                first = path.read_bytes()
                match = steam.matching_entry(steam.shortcuts(first), exe)
                self.assertEqual(match[2], appid)
                self.assertEqual(match[1][b'appname'][5].decode(), 'GTetrinet NG — TetriNET')
                self.assertEqual(len(steam.shortcuts(first)[5]), 1)
                for suffix, source in sources.items():
                    self.assertEqual((config / 'grid' / (str(appid) + suffix)).read_bytes(), source.read_bytes())
                steam.main(['GTetrinet NG — TetriNET', exe, tmp, ''])
                self.assertEqual(first, path.read_bytes())
                self.assertEqual(Path(str(path) + '.gtetrinet-ng-backup').read_bytes(), original)
                with patch.object(steam, 'steam_closed', side_effect=RuntimeError('Steam running')):
                    with self.assertRaises(RuntimeError):
                        steam.main(['Changed', exe, tmp, ''])
                    self.assertEqual(first, path.read_bytes())
                    steam.main(['--artwork-only', 'Changed', exe])
                    self.assertEqual(first, path.read_bytes())
