#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Versioned Linux installer, update check and anonymous exit-code reporting."""
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

REPO = 'jarvis-jhk/gtetrinet-ng'
TOPIC = 'gtetrinet-ng-faults-5e8b41e03cf04b57'
ROOT = Path.home() / '.local/share/gtetrinet-ng'


def german():
    """Same choice as install.sh: the game's saved language, then the locale as SDL reads it."""
    saved = Path(os.environ.get('XDG_DATA_HOME') or Path.home() / '.local/share') / 'gtetrinet-ng/gtetrinet-ng/settings.ini'
    try:
        choice = [l[5:].strip() for l in saved.read_text().splitlines() if l.startswith('lang=')][-1:]
    except OSError:
        choice = []
    if choice in (['de'], ['en']):
        return choice == ['de']
    for value in (os.environ.get('LANGUAGE', '').split(':')[0], os.environ.get('LC_ALL', ''),
                  os.environ.get('LC_MESSAGES', ''), os.environ.get('LANG', '')):
        if value and value != 'POSIX' and value.split('.')[0] != 'C':
            return value == 'de' or value[:3] in ('de_', 'de.', 'de@')
    return False


def t(english, deutsch):
    return deutsch if german() else english


def report(kind, version):
    if os.environ.get('GTN_NO_REPORT') == '1':
        return
    # No stderr, paths, hostname, nicknames or command-line parameters leave the device.
    body = ('gtetrinet-ng %s Linux: %s' % (version, kind)).encode()
    try:
        subprocess.Popen(['curl', '-fsS', '--max-time', '8', '-H',
                          'Title: gtetrinet-ng launcher fault', '--data-binary', body.decode(),
                          'https://ntfy.sh/' + TOPIC], stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, start_new_session=True)
    except OSError:
        pass


def fetch(url, limit=80 * 1024 * 1024):
    req = urllib.request.Request(url, headers={'User-Agent': 'gtetrinet-ng'})
    with urllib.request.urlopen(req, timeout=4) as response:
        data = response.read(limit + 1)
    if len(data) > limit:
        raise ValueError('download exceeds limit')
    return data


def update(required=False):
    if platform.machine() not in ('x86_64', 'AMD64'):
        if required:
            raise RuntimeError('only Linux x86_64 is supported')
        return
    release = json.loads(fetch('https://api.github.com/repos/' + REPO + '/releases/latest', 200000))
    tag = release['tag_name']
    if not tag.startswith('v') or any(c not in 'v0123456789.' for c in tag):
        raise ValueError('invalid release tag')
    current = ROOT / 'current'
    if current.exists() and (current / 'VERSION').read_text().strip() == tag:
        return
    assets = {a['name']: a['browser_download_url'] for a in release['assets']}
    name = 'gtetrinet-ng-linux-x86_64.tar.gz'
    for key in (name, 'SHA256SUMS'):
        if not assets[key].startswith('https://github.com/' + REPO + '/releases/download/'):
            raise ValueError('unexpected download origin')
    sums = fetch(assets['SHA256SUMS'], 20000).decode()
    expected = next(line.split()[0] for line in sums.splitlines() if line.split()[-1] == name)
    archive = fetch(assets[name])
    if hashlib.sha256(archive).hexdigest() != expected:
        raise ValueError('checksum mismatch')
    ROOT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT) as temporary:
        with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as tar:
            members = tar.getmembers()
            if sum(m.size for m in members) > 200 * 1024 * 1024:
                raise ValueError('unpacked archive exceeds limit')
            for m in members:
                path = Path(m.name)
                if path.is_absolute() or '..' in path.parts or not (m.isfile() or m.isdir()):
                    raise ValueError('unsafe archive entry')
            tar.extractall(temporary, members=members)
        tree = Path(temporary) / 'gtetrinet-ng'
        if not (tree / 'bin/gtetrinet-ng').is_file() or (tree / 'VERSION').read_text().strip() != tag:
            raise ValueError('incomplete archive')
        destination = ROOT / tag
        if destination.exists():
            shutil.rmtree(destination)
        os.replace(tree, destination)
    link = ROOT / 'current.new'
    link.unlink(missing_ok=True)
    link.symlink_to(tag)
    os.replace(link, current)
    print(t('Installed ', 'Installiert: ') + tag, flush=True)


def warpzone_managed(here):
    """The Warpzone launcher updates installs it marks; ours then stays out of the way."""
    return any((d / '.warpzone-managed').exists() for d in (here, here.parent))


def main():
    install = '--install' in sys.argv
    here = Path(__file__).resolve().parents[2]
    managed = here.parent == ROOT
    checking = managed and os.environ.get('GTN_AUTO_UPDATE') != '0' and not warpzone_managed(here)
    if install or checking:
        try:
            update(install)
        except Exception as error:
            # Ordinary offline checks are not faults. Mandatory install failure is.
            if install:
                report('installation failed', 'installer')
                raise RuntimeError(t('Installation failed: ', 'Installation fehlgeschlagen: ') + str(error)) from error
            print(t('Update unavailable; keeping installed version.',
                    'Kein Update erreichbar; die installierte Version bleibt.'), file=sys.stderr)
    if install:
        return 0
    if managed:
        here = (ROOT / 'current').resolve()
    version = (here / 'VERSION').read_text().strip() if (here / 'VERSION').exists() else 'development'
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = str(here / 'lib') + ':' + env.get('LD_LIBRARY_PATH', '')
    try:
        code = subprocess.call([str(here / 'bin/gtetrinet-ng')] + sys.argv[1:], env=env)
    except OSError:
        report('launch failed', version)
        return 1
    if code and code != 2:  # 2 is command-line usage, not an application fault.
        report('exit code %d' % code, version)
    return code if code >= 0 else 128 - code


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
