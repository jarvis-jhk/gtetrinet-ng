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


if __name__ == '__main__':
    unittest.main()
