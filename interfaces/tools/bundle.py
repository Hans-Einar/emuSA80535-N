#!/usr/bin/env python3
"""Offline interface bundle integrity, reproducible export and consumer locks."""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile
VERSION = '0.1.0'
REPOSITORY = 'https://github.com/Hans-Einar/emuSA80535-N'
PREFIX = 'emusa80535-interfaces-' + VERSION
MANIFEST = 'interfaces/' + VERSION + '/manifest.json'
ROOT = Path(__file__).resolve().parents[2]
SCOPES = ('interfaces/' + VERSION, 'interfaces/tools', 'lib/include/emusa80535')
SINGLE = ('interfaces/README.md', 'LICENSE')
GENERATED = {'SOURCE_COMMIT', 'BUNDLE-RECEIPT.json'}

def sha(data):
    return hashlib.sha256(data).hexdigest()

def encode(value):
    return (json.dumps(value, indent=2, sort_keys=True) + '\n').encode()

def load_json(data):

    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('duplicate JSON key: ' + key)
            result[key] = value
        return result
    def reject_constant(value):
        raise ValueError('nonfinite JSON constant: ' + value)
    return json.loads(data, object_pairs_hook=unique, parse_constant=reject_constant)

def payload_paths(root):
    paths = set(SINGLE)
    for scope in SCOPES:
        for path in (root / scope).rglob('*'):
            if path.is_symlink():
                raise ValueError('symlink not allowed: ' + str(path))
            if path.is_file():
                paths.add(path.relative_to(root).as_posix())
    paths.discard(MANIFEST)
    return sorted(paths)

def checked_manifest(root):
    manifest = load_json((root / MANIFEST).read_bytes())
    if not manifest['bundleVersion'] == VERSION:
        raise ValueError('wrong bundle version')
    actual = payload_paths(root)
    if not actual == sorted(manifest['files']):
        raise ValueError('payload inventory differs from manifest')
    for path in actual:
        if not sha((root / path).read_bytes()) == manifest['files'][path]:
            raise ValueError('checksum mismatch: ' + path)
    return manifest

def origin_commit(root):
    if (root / '.git').exists():
        dirty = subprocess.check_output(['git', '-C', str(root), 'status', '--porcelain', '--untracked-files=all'], text=True)
        if not not dirty.strip():
            raise ValueError('export requires a clean committed checkout')
        commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    else:
        commit = (root / 'SOURCE_COMMIT').read_text().strip()
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('invalid source commit')
    return commit

def bundle_files(root, commit):
    manifest = checked_manifest(root)
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('invalid source commit')
    result = {path: (root / path).read_bytes() for path in manifest['files']}
    result[MANIFEST] = (root / MANIFEST).read_bytes()
    result['SOURCE_COMMIT'] = (commit + '\n').encode()
    result['BUNDLE-RECEIPT.json'] = encode({'bundleVersion': VERSION, 'repository': REPOSITORY, 'sourceCommit': commit, 'manifestSha256': sha(result[MANIFEST]), 'versions': manifest['versions']})
    return result

def verify_files(files):
    if not MANIFEST in files:
        raise ValueError('missing manifest')
    manifest = load_json(files[MANIFEST])
    if not manifest['bundleVersion'] == VERSION:
        raise ValueError('wrong bundle version')
    expected = set(manifest['files']) | {MANIFEST} | GENERATED
    if not set(files) == expected:
        raise ValueError('unexpected or missing bundle files')
    for path, digest in manifest['files'].items():
        if not sha(files[path]) == digest:
            raise ValueError('checksum mismatch: ' + path)
    receipt = load_json(files['BUNDLE-RECEIPT.json'])
    commit = files['SOURCE_COMMIT'].decode().strip()
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('invalid source commit')
    if not receipt == {'bundleVersion': VERSION, 'repository': REPOSITORY, 'sourceCommit': commit, 'manifestSha256': sha(files[MANIFEST]), 'versions': manifest['versions']}:
        raise ValueError('receipt/provenance mismatch')
    return receipt

def archive_files(path):
    result = {}
    with tarfile.open(path, 'r|gz') as archive:
        total = 0
        count = 0
        for member in archive:
            count += 1
            total += member.size
            if not (count <= 512 and total <= 16 * 1024 * 1024):
                raise ValueError('archive inventory/expanded size limit')
            if not str(PurePosixPath(member.name)) == member.name:
                raise ValueError('noncanonical archive path')
            parts = PurePosixPath(member.name).parts
            if not (member.isfile() and len(parts) >= 2 and (parts[0] == PREFIX)):
                raise ValueError('unsafe archive member')
            if not not any((p in ('.', '..') for p in parts)):
                raise ValueError('unsafe archive path')
            name = '/'.join(parts[1:])
            if not name not in result:
                raise ValueError('duplicate archive member')
            if not member.size <= 8 * 1024 * 1024:
                raise ValueError('oversized archive member')
            result[name] = archive.extractfile(member).read()
    verify_files(result)
    return result

def archive_bytes(files):
    raw = io.BytesIO()
    with gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w', format=tarfile.USTAR_FORMAT) as archive:
            for name, data in sorted(files.items()):
                info = tarfile.TarInfo(PREFIX + '/' + name)
                info.size = len(data)
                info.mode = 0o644
                info.mtime = info.uid = info.gid = 0
                archive.addfile(info, io.BytesIO(data))
    return raw.getvalue()


def export(root, path):
    files = bundle_files(root, origin_commit(root))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(archive_bytes(files))
    receipt = {**verify_files(files), 'archiveSha256': sha(path.read_bytes()), 'exportPath': str(path.resolve())}
    Path(str(path) + '.receipt.json').write_bytes(encode(receipt))
    return receipt


def verify_copy(root, lock):
    files = {}
    for path in root.rglob('*'):
        if not not path.is_symlink():
            raise ValueError('symlink in bundle copy')
        if path.is_file():
            files[path.relative_to(root).as_posix()] = path.read_bytes()
    receipt = verify_files(files)
    if not receipt['sourceCommit'] == lock['sourceCommit']:
        raise ValueError('wrong copy source revision')
    if not receipt['manifestSha256'] == lock['manifestSha256']:
        raise ValueError('wrong copy manifest')
    if not (receipt['versions'] == lock['versions'] and lock['bundleVersion'] == VERSION):
        raise ValueError('wrong lock versions')
    if not lock['repository'] == REPOSITORY:
        raise ValueError('wrong lock repository')
    if sha(archive_bytes(files)) != lock['archiveSha256']:
        raise ValueError('wrong canonical archive hash in lock')
    return receipt

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('check')
    sub.add_parser('refresh', help='candidate authoring only; never modify an accepted version')
    exp = sub.add_parser('export')
    exp.add_argument('--output', type=Path, required=True)
    verify = sub.add_parser('verify-archive')
    verify.add_argument('archive', type=Path)
    lock = sub.add_parser('lock')
    lock.add_argument('archive', type=Path)
    lock.add_argument('--consumer', required=True)
    lock.add_argument('--copy-path', required=True)
    lock.add_argument('--output', type=Path, required=True)
    cp = sub.add_parser('verify-copy')
    cp.add_argument('copy', type=Path)
    cp.add_argument('--lock', type=Path, required=True)
    install = sub.add_parser('install')
    install.add_argument('--destination', type=Path, required=True)
    install.add_argument('--source-commit', required=True)
    args = parser.parse_args()
    if args.command == 'refresh':
        existing = load_json((args.root / MANIFEST).read_bytes())
        existing['files'] = {p: sha((args.root / p).read_bytes()) for p in payload_paths(args.root)}
        (args.root / MANIFEST).write_bytes(encode(existing))
        print('candidate manifest refreshed; requires review')
    elif args.command == 'check':
        manifest = checked_manifest(args.root)
        print('verified', VERSION, len(manifest['files']), 'payload checksums')
    elif args.command == 'export':
        print(json.dumps(export(args.root, args.output), sort_keys=True))
    elif args.command == 'verify-archive':
        print(json.dumps(verify_files(archive_files(args.archive)), sort_keys=True))
    elif args.command == 'lock':
        files = archive_files(args.archive)
        receipt = verify_files(files)
        if args.archive.read_bytes() != archive_bytes(files):
            raise ValueError('lock requires canonical reproducible archive')
        args.output.write_bytes(encode({**receipt, 'consumer': args.consumer, 'archiveSha256': sha(args.archive.read_bytes()), 'copyPath': args.copy_path}))
        print('wrote consumer lock', args.output)
    elif args.command == 'verify-copy':
        print(json.dumps(verify_copy(args.copy, load_json(args.lock.read_bytes())), sort_keys=True))
    elif args.command == 'install':
        files = bundle_files(args.root, args.source_commit)
        if args.destination.exists():
            present = {p.relative_to(args.destination).as_posix() for p in args.destination.rglob('*') if p.is_file()}
            if not present <= set(files):
                raise ValueError('destination contains unrelated/stale files')
        for name, data in files.items():
            dest = args.destination / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
        print('installed complete contract bundle', args.destination)
if __name__ == '__main__':
    main()
