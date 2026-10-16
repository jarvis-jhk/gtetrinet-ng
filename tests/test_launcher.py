# SPDX-License-Identifier: GPL-2.0-or-later
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('launcher', Path(__file__).resolve().parents[1] / 'tools/linux/launcher.py')
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class InstallerTests(unittest.TestCase):
    def archive(self, unsafe=False):
        data = io.BytesIO()
        with tarfile.open(fileobj=data, mode='w:gz') as tar:
            for name, content in [('VERSION', b'v0.1.0\n'), ('bin/gtetrinet-ng', b'game')]:
                info = tarfile.TarInfo(('..' if unsafe else 'gtetrinet-ng') + '/' + name)
                info.size = len(content)
                tar.addfile(info, io.BytesIO(content))
        return data.getvalue()

    def apply(self, archive, checksum=None):
        base = 'https://github.com/' + launcher.REPO + '/releases/download/v0.1.0/'
        name = 'gtetrinet-ng-linux-x86_64.tar.gz'
        release = {'tag_name': 'v0.1.0', 'assets': [{'name': n, 'browser_download_url': base + n} for n in (name, 'SHA256SUMS')]}
        replies = [json.dumps(release).encode(), ((checksum or hashlib.sha256(archive).hexdigest()) + '  ' + name).encode(), archive]
        with patch.object(launcher, 'fetch', side_effect=replies), patch.object(launcher.platform, 'machine', return_value='x86_64'):
            launcher.update(True)

    def test_valid_atomic_install(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(launcher, 'ROOT', Path(tmp)):
            self.apply(self.archive())
            self.assertEqual((Path(tmp) / 'current/VERSION').read_text(), 'v0.1.0\n')

    def test_bad_checksum_keeps_existing(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(launcher, 'ROOT', Path(tmp)):
            old = Path(tmp) / 'v0.0.1'
            old.mkdir()
            (old / 'VERSION').write_text('v0.0.1')
            (Path(tmp) / 'current').symlink_to('v0.0.1')
            with self.assertRaises(ValueError):
                self.apply(self.archive(), '0' * 64)
            self.assertEqual((Path(tmp) / 'current').resolve(), old)

    def test_path_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(launcher, 'ROOT', Path(tmp)):
            with self.assertRaises(ValueError):
                self.apply(self.archive(True))
            self.assertFalse((Path(tmp) / 'current').exists())

    def test_warpzone_managed_install_never_updates(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(launcher, 'ROOT', Path(tmp)):
            tree = Path(tmp) / 'v0.0.1'
            (tree / 'tools/linux').mkdir(parents=True)
            (tree / 'VERSION').write_text('v0.0.1')
            self.assertFalse(launcher.warpzone_managed(tree))
            (Path(tmp) / '.warpzone-managed').write_text('')
            self.assertTrue(launcher.warpzone_managed(tree))
            script = str(tree / 'tools/linux/launcher.py')
            with patch.object(launcher, '__file__', script), patch.object(launcher.sys, 'argv', [script]), \
                    patch.object(launcher, 'update', side_effect=AssertionError('updated')) as update, \
                    patch.object(launcher.subprocess, 'call', return_value=0) as call:
                self.assertEqual(launcher.main(), 0)
            update.assert_not_called()
            call.assert_called_once()


if __name__ == '__main__':
    unittest.main()


class LanguageTests(unittest.TestCase):
    def german(self, env, saved=None):
        with tempfile.TemporaryDirectory() as tmp:
            if saved:
                prefs = Path(tmp) / 'gtetrinet-ng/gtetrinet-ng'
                prefs.mkdir(parents=True)
                (prefs / 'settings.ini').write_text('host=x\nlang=%s\n' % saved)
            with patch.dict('os.environ', dict(env, XDG_DATA_HOME=tmp), clear=True):
                return launcher.german(), launcher.t('Installed ', 'Installiert: ')

    def test_locale_and_saved_choice(self):
        self.assertEqual(self.german({'LANG': 'de_DE.UTF-8'}), (True, 'Installiert: '))
        self.assertEqual(self.german({'LANG': 'en_US.UTF-8'}), (False, 'Installed '))
        self.assertEqual(self.german({'LANG': 'C'}), (False, 'Installed '))
        self.assertEqual(self.german({'LANGUAGE': 'de:en', 'LANG': 'en_US.UTF-8'})[0], True)
        self.assertEqual(self.german({'LANG': 'en_US.UTF-8'}, 'de')[0], True)
        self.assertEqual(self.german({'LANG': 'de_DE.UTF-8'}, 'en')[0], False)
        self.assertEqual(self.german({'LANG': 'de_AT.UTF-8'}, 'auto')[0], True)
