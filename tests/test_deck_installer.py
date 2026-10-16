# SPDX-License-Identifier: GPL-2.0-or-later
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DeckInstallerTests(unittest.TestCase):
    def test_graphical_install_and_repeat_with_spaces_in_home(self):
        dialogs = self.install({'LANG': 'en_US.UTF-8'})
        self.assertIn('Installed with Steam artwork', dialogs)
        self.assertIn('--ok-label=Yes', dialogs)

    def test_german_locale_gives_german_dialogs(self):
        dialogs = self.install({'LANG': 'de_DE.UTF-8'})
        self.assertIn('Mit Steam-Grafiken installiert', dialogs)
        self.assertIn('auf diesem Gerät installieren?', dialogs)
        self.assertIn('--ok-label=Ja', dialogs)
        self.assertNotIn('Installed with', dialogs)

    def test_saved_game_language_beats_locale(self):
        self.assertIn('Mit Steam-Grafiken installiert', self.install({'LANG': 'en_US.UTF-8'}, 'de'))
        self.assertIn('Installed with Steam artwork', self.install({'LANG': 'de_DE.UTF-8'}, 'en'))
        self.assertIn('Installed with Steam artwork', self.install({'LANGUAGE': 'en_GB:de', 'LANG': 'de_DE.UTF-8'}, 'auto'))

    def test_failed_bootstrap_reports_in_german(self):
        dialogs = self.install({'LANG': 'de_DE.UTF-8'}, fail_bootstrap=True)
        self.assertIn('--error', dialogs)
        self.assertIn('Die Installation ist fehlgeschlagen (bootstrap)', dialogs)

    def install(self, locale, saved=None, fail_bootstrap=False):
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
                'curl': 'case "$4" in */install.sh) cp "$GTN_INSTALLER_FIXTURE" "$6";; *) printf "%s\\n" "$GTN_BOOTSTRAP" > "$6";; esac',
                'pgrep': 'exit 1',
                'zenity': 'printf "%s\\n" "$*" >> "$HOME/dialogs"; exit 0',
            }.items():
                path = stubs / name
                path.write_text('#!/bin/sh\n' + body + '\n')
                path.chmod(0o755)
            if saved:
                prefs = home / '.local/share/gtetrinet-ng/gtetrinet-ng'
                prefs.mkdir(parents=True)
                (prefs / 'settings.ini').write_text('host=x\nlang=%s\nblocks=modern\n' % saved)
            env = {k: v for k, v in os.environ.items() if k not in ('LANGUAGE', 'LC_ALL', 'LC_MESSAGES', 'LANG', 'XDG_DATA_HOME')}
            env.update(locale, GTN_BOOTSTRAP='raise SystemExit(1)' if fail_bootstrap else 'pass')
            env = dict(env, HOME=tmp, PATH=str(stubs) + ':' + os.environ['PATH'], GTN_NO_REPORT='1', GTN_INSTALLER_FIXTURE=str(ROOT / 'tools/linux/install.sh'))
            raw = next(line[5:] for line in (ROOT / 'tools/linux/Install-GTetrinet-NG.desktop').read_text().splitlines() if line.startswith('Exec='))
            command = shlex.split(raw.replace('\\\\', '\\'))
            self.assertEqual(command[:2], ['sh', '-c'])
            self.assertEqual(len(command), 3)
            # Desktop Exec unquotes dollars/backticks; Python shlex preserves these.
            command[2] = command[2].replace('\\$', '$').replace('\\`', '`')
            if fail_bootstrap:
                self.assertNotEqual(subprocess.run(command, env=env, capture_output=True).returncode, 0)
                return (home / 'dialogs').read_text()
            for _ in range(2):
                subprocess.run(command, env=env, check=True, capture_output=True)
            desktop = (home / '.local/share/applications/gtetrinet-ng.desktop').read_text()
            self.assertIn('Name=GTetrinet NG — TetriNET', desktop)
            self.assertIn('Icon=' + str(art / 'steam_icon.png'), desktop)
            self.assertEqual(len(list((config / 'grid').glob('*.png'))), 5)
            return (home / 'dialogs').read_text()
