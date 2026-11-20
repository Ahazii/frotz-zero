#!/usr/bin/env python3
"""Read Git's index or a committed tree; reject private and non-source inputs.

This examines Git blobs, not working files, so an ignored file added with -f is
still checked. Only original CC0 Inform source is allowed; no compiled stories.
"""
import argparse
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
ROOT_FILES = {'.gitignore', '.gitattributes', 'CMakeLists.txt', 'LICENSE',
              'README.md', 'CHANGELOG.md', 'CONTRIBUTING.md',
              'THIRD_PARTY_NOTICES.md', 'app-builder.json', 'dependencies.lock.json'}
TEXT_TYPES = {'src': {'.cpp', '.hpp', '.h'}, 'scripts': {'.py', '.ps1', '.sh'},
              'tests': {'.cpp', '.inf', '.txt'}, 'docs': {'.md'},
              'assets': {'.json', '.svg', '.txt'}, 'cmake': {'.cmake', '.json'},
              'packaging': {'.desktop', '.mit'}, 'store': {'.md'}}
PUBLIC_IMAGES = {'assets/frotz-zero.png', 'assets/brand-24.png',
                 'assets/brand-40.png', 'assets/brand-96.png'} | {
    'store/screenshots/' + name + '.png' for name in
    ('home', 'library', 'story', 'lantern', 'settings-light',
     'online-release', 'manual-link', 'about')}
PRIVATE_PARTS = {'stories', 'saves', 'local', 'dev-data', 'build', 'dist', 'out',
                 '.cache', '.git', '.ssh', '__pycache__'}
FORBIDDEN_SUFFIXES = {'.zblorb', '.zlb', '.blb', '.blorb', '.dat', '.qzl', '.sav',
                      '.fzsave', '.zip', '.7z', '.rar', '.tar', '.gz', '.tgz',
                      '.xz', '.bz2', '.aiff', '.aif', '.wav', '.ogg', '.mod',
                      '.snd', '.mg1', '.eg1', '.cg1', '.pem', '.key', '.pfx'}
SECRET_PATTERNS = {
    'private key': re.compile(rb'-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----'),
    'GitHub token': re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})\b'),
    'GitLab token': re.compile(rb'\bglpat-[A-Za-z0-9_-]{20,}\b'),
}


def path_problem(name):
    path = pathlib.PurePosixPath(name)
    parts = {part.lower() for part in path.parts}
    suffix = path.suffix.lower()
    if path.is_absolute() or '..' in parts or '\\' in name:
        return 'unexpected path'
    if parts & PRIVATE_PARTS or any(part.endswith('.fzsave') for part in parts):
        return 'private data or generated directory'
    if path.name.lower().startswith('.env') or path.name.lower() == 'deployment.local.json':
        return 'local configuration'
    if suffix in FORBIDDEN_SUFFIXES or re.fullmatch(r'\.z[1-8]', suffix):
        return 'story, media, save, archive or credential file'
    if name in ROOT_FILES or name in PUBLIC_IMAGES or name == 'tests/fixtures/LICENSE':
        return None
    if name in ('third_party/qrcodegen/qrcodegen.c', 'third_party/qrcodegen/qrcodegen.h'):
        return None
    if len(path.parts) > 1 and suffix in TEXT_TYPES.get(path.parts[0], set()):
        if suffix == '.inf' and path.parent != pathlib.PurePosixPath('tests/fixtures'):
            return 'Inform source outside the original fixture folder'
        return None
    return 'outside the public source allowlist'


def content_problem(name, body):
    if body[:1] in tuple(bytes([v]) for v in range(1, 9)):
        return 'possible renamed Z-machine binary'
    if body.startswith(b'FORM') and body[8:12] in (b'IFRS', b'IFZS', b'AIFF', b'AIFC'):
        return 'renamed Blorb, save or audio container'
    for label, pattern in SECRET_PATTERNS.items():
        if pattern.search(body):
            return label + ' detected (value suppressed)'
    if name in PUBLIC_IMAGES:
        return None if body.startswith(b'\x89PNG\r\n\x1a\n') else 'expected public PNG artwork'
    try:
        text = body.decode('utf-8-sig')
    except UnicodeDecodeError:
        return 'non-text source file'
    if '\0' in text:
        return 'binary content in a text file'
    if name.endswith('.inf') and 'SPDX-License-Identifier: CC0-1.0' not in text[:256]:
        return 'fixture source lacks its CC0 notice'
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=pathlib.Path, default=ROOT)
    parser.add_argument('--ref', help='Check this committed tree instead of the staged index')
    args = parser.parse_args()
    def git(*command, **kwargs):
        return subprocess.check_output(['git', '-C', str(args.repo), *command], **kwargs)
    if args.ref:
        tree = git('rev-parse', '--verify', '--end-of-options', args.ref + '^{tree}').decode().strip()
        records = git('ls-tree', '-r', '-z', tree).split(b'\0')
    else:
        records = git('ls-files', '--stage', '-z').split(b'\0')
    entries, problems = [], []
    for record in filter(None, records):
        metadata, raw_name = record.split(b'\t', 1)
        name = raw_name.decode('utf-8')
        if args.ref:
            mode, kind, oid = metadata.decode().split()
            valid = kind == 'blob'
        else:
            mode, oid, stage = metadata.decode().split()
            valid = stage == '0'
        issue = path_problem(name)
        if not valid or mode not in ('100644', '100755'):
            issue = 'unmerged entry, symlink or nested Git repository'
        if issue:
            problems.append((name, issue))
        else:
            entries.append((name, oid))
    if not records or not any(records):
        problems.append(('(tree)', 'no source files found'))
    if entries:
        batch = git('cat-file', '--batch', input=('\n'.join(oid for _, oid in entries) + '\n').encode())
        position, total = 0, 0
        for name, oid in entries:
            end = batch.index(b'\n', position)
            actual, kind, size = batch[position:end].decode().split()
            if actual != oid or kind != 'blob':
                raise RuntimeError('Unexpected Git object response')
            size = int(size); position = end + 1
            body = batch[position:position + size]; position += size + 1; total += size
            issue = 'unexpectedly large source file' if size > 2 * 1024 * 1024 else content_problem(name, body)
            if issue:
                problems.append((name, issue))
    else:
        total = 0
    if problems:
        for name, issue in problems:
            print(f'FAIL {name!r}: {issue}')
        return 1
    fixtures = sum(name.endswith('.inf') for name, _ in entries)
    print(f'PASS: {len(entries)} public files, {total} bytes; {fixtures} original CC0 fixture sources.')
    print('No playable stories, media packs, saves, private directories or detected credentials in this tree.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
