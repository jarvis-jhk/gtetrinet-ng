# SPDX-License-Identifier: GPL-2.0-or-later
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DeckInstallerTests(unittest.TestCase):
    def test_graphical_install_and_repeat_with_spaces_in_home(self):
        with tempfile.TemporaryDirectory(prefix='deck home ') as tmp:
            home = Path(tmp)
            current = home / '.local/share/gtetrinet-ng/current'
            helper = current / 'tools/linux'
            helper.mkdir(parents=True)
            shutil.copy(ROOT / 'tools/linux/add-to-steam.py', helper)
            art = current / 'artwork'
            art.mkdir()
            for suffix in ('p', '', '_hero', '_logo', '_icon'):
                (art / ('steam' + suffix + '.png')).write_bytes(b'fixture image')
            config = home / '.steam/steam/userdata/1/config'
            config.mkdir(parents=True)
            stubs = home / 'bin'
            stubs.mkdir()
            for name, body in {
                'curl': 'printf "pass\\n" > "$6"',
                'pgrep': 'exit 1',
                'zenity': 'printf "%s\\n" "$*" >> "$HOME/dialogs"; exit 0',
            }.items():
                path = stubs / name
                path.write_text('#!/bin/sh\n' + body + '\n')
                path.chmod(0o755)
            env = dict(os.environ, HOME=tmp, PATH=str(stubs) + ':' + os.environ['PATH'], GTN_NO_REPORT='1')
            for _ in range(2):
                subprocess.run(['bash', str(ROOT / 'tools/linux/install.sh')], env=env, check=True, capture_output=True)
            desktop = (home / '.local/share/applications/gtetrinet-ng.desktop').read_text()
            self.assertIn('Name=GTetrinet NG — TetriNET', desktop)
            self.assertIn('Icon=' + str(art / 'steam_icon.png'), desktop)
            self.assertEqual(len(list((config / 'grid').glob('*.png'))), 5)
            self.assertIn('Installed with Steam artwork', (home / 'dialogs').read_text())
