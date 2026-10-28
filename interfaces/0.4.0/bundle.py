#!/usr/bin/env python3
"""Verify, export or install the standalone wire-only0.4.0 contract sidecar.

The trusted expected manifest hash comes from an external release/consumer lock;
no payload embeds the hash of a manifest which hashes that same payload.
"""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import tarfile

VERSION = '0.4.0'
PREFIX = 'emusa80535-control-' + VERSION
ROOT = Path(__file__).resolve().parent
BASE = {
    'bundleVersion': '0.3.0',
    'sourceCommit': '2133e7220284bb8dc1600ba98b2d842106a7dba4',
    'manifestSha256': 'ed3917b0d3a7300aca94171f1259eacf9861b0ee87376afa534690618da27ff0',
    'archiveSha256': '405f22f7fc7e0a49d921715102102a4d667ba1ff84b1238494d76353feeb0f53',
}
META = {'manifest.json', 'SOURCE_COMMIT', 'BUNDLE-RECEIPT.json'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load_json(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'duplicate key: ' + key)
            result[key] = value
        return result
    def bad(value):
        raise ValueError('nonfinite constant: ' + value)
    return json.loads(data, object_pairs_hook=unique, parse_constant=bad)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def path_key(name):
    require(isinstance(name, str) and name and '\\' not in name and '\x00' not in name,
            'invalid payload path')
    p = PurePosixPath(name)
    require(not p.is_absolute() and all(x not in ('', '.', '..') for x in name.split('/'))
            and p.as_posix() == name and name not in META, 'unsafe payload path: ' + name)
    return p


def inventory(root):
    result = []
    for p in root.rglob('*'):
        require(not p.is_symlink(), 'symlink forbidden: ' + str(p))
        if p.is_dir():
            continue
        require(p.is_file(), 'nonregular payload: ' + str(p))
        relative = p.relative_to(root)
        if '__pycache__' in relative.parts and p.suffix == '.pyc':
            continue
        name = relative.as_posix()
        if name not in META:
            result.append(name)
    return sorted(result)


def check(root=ROOT, expected_manifest=None):
    root = Path(root)
    require(root.is_dir() and not root.is_symlink(), 'contract root must be a directory')
    p = root / 'manifest.json'
    require(p.is_file() and not p.is_symlink(), 'missing/nonregular manifest')
    data = p.read_bytes()
    digest = sha(data)
    if expected_manifest is not None:
        require(re.fullmatch('[0-9a-f]{64}', expected_manifest) is not None,
                'expected manifest must be SHA256')
        require(digest == expected_manifest, 'manifest hash mismatch')
    manifest = load_json(data)
    require(set(manifest) == {'bundleVersion', 'kind', 'protocol', 'base',
                              'nativeInterfaces', 'traceFileContract', 'files'},
            'manifest fields')
    require(manifest['bundleVersion'] == VERSION and
            manifest['kind'] == 'control-wire-extension' and
            manifest['protocol'] == {'major': 3, 'minor': 1} and
            manifest['base'] == BASE, 'manifest profile/base mismatch')
    require(manifest['nativeInterfaces'] == {'cpuApi': 1, 'boardAbi': '1.0',
            'observationApi': 1, 'traceApi': 1, 'controlQuery': 'emusa80535_control_query_v3'},
            'native interface mismatch')
    require(manifest['traceFileContract'] == {'bundleVersion': '0.3.0',
            'protocol': {'major': 3, 'minor': 0}, 'manifestSha256': BASE['manifestSha256']},
            'trace file contract mismatch')
    files = manifest['files']
    require(isinstance(files, dict) and files, 'empty payload inventory')
    for name, expected in files.items():
        path_key(name)
        require(isinstance(expected, str) and re.fullmatch('[0-9a-f]{64}', expected),
                'invalid payload hash')
    require(inventory(root) == sorted(files), 'payload inventory mismatch')
    for name, expected in files.items():
        require(sha((root/name).read_bytes()) == expected, 'payload hash mismatch: ' + name)
    present = [(root/x).exists() for x in ('SOURCE_COMMIT', 'BUNDLE-RECEIPT.json')]
    require(present[0] == present[1], 'incomplete provenance receipt')
    if present[0]:
        source = (root/'SOURCE_COMMIT').read_text()
        require(re.fullmatch('[0-9a-f]{40}\n', source), 'source commit encoding')
        require(load_json((root/'BUNDLE-RECEIPT.json').read_bytes()) == receipt(source.strip(), digest),
                'provenance receipt mismatch')
    return manifest, digest


def receipt(source, digest):
    return {'bundleVersion': VERSION, 'kind': 'control-wire-extension',
            'sourceCommit': source, 'manifestSha256': digest, 'base': BASE}


def json_bytes(value):
    return (json.dumps(value, sort_keys=True, indent=2)+'\n').encode()


def payload(root, source, expected_manifest=None):
    require(re.fullmatch('[0-9a-f]{40}', source or '') is not None,
            '--source-commit must be an exact40-hex revision')
    manifest, digest = check(root, expected_manifest)
    if (root/'SOURCE_COMMIT').exists():
        require((root/'SOURCE_COMMIT').read_text() == source+'\n',
                'cannot rewrite immutable copied contract provenance')
    result = {name: (root/name).read_bytes() for name in manifest['files']}
    result['manifest.json'] = (root/'manifest.json').read_bytes()
    result['SOURCE_COMMIT'] = (source+'\n').encode()
    result['BUNDLE-RECEIPT.json'] = json_bytes(receipt(source, digest))
    return result, digest


def outside(root, target):
    root, target = root.resolve(), target.resolve()
    require(target != root and root not in target.parents, 'destination must be outside contract root')


def archive_bytes(files):
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode='w', format=tarfile.USTAR_FORMAT) as archive:
        for name, data in sorted(files.items()):
            info = tarfile.TarInfo(PREFIX+'/'+name)
            info.size = len(data); info.mode = 0o644; info.mtime = 0
            archive.addfile(info, io.BytesIO(data))
    compressed = io.BytesIO()
    with gzip.GzipFile(filename='', fileobj=compressed, mode='wb', mtime=0) as stream:
        stream.write(raw.getvalue())
    return compressed.getvalue()


def export(root, output, source, expected_manifest=None, expected_archive=None):
    root, output = Path(root), Path(output)
    outside(root, output)
    require(not output.exists() and not output.is_symlink(), 'output already exists')
    files, digest = payload(root, source, expected_manifest)
    data = archive_bytes(files)
    require(expected_archive is None or sha(data) == expected_archive, 'archive hash mismatch')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as stream:
        stream.write(data)
    return {'archive': str(output), 'archiveSha256': sha(data),
            'manifestSha256': digest, 'sourceCommit': source}


def install(root, destination, source, expected_manifest=None, expected_archive=None):
    root, destination = Path(root), Path(destination)
    outside(root, destination)
    files, digest = payload(root, source, expected_manifest)
    require(expected_archive is None or sha(archive_bytes(files)) == expected_archive,
            'archive hash mismatch')
    require(not destination.is_symlink(), 'destination symlink')
    if destination.exists():
        check(destination, digest)
        require((destination/'SOURCE_COMMIT').read_text() == source+'\n',
                'existing destination has different provenance')
        return {'destination': str(destination), 'manifestSha256': digest, 'sourceCommit': source}
    destination.mkdir(parents=True)
    for name, data in files.items():
        target = destination/name
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as stream:
            stream.write(data)
    check(destination, digest)
    return {'destination': str(destination), 'manifestSha256': digest, 'sourceCommit': source}


def locked_identity(path, source=None, expected_manifest=None):
    """External accepted lock fixes contract provenance independently of runtime."""
    path = Path(path)
    require(path.is_file() and not path.is_symlink(), 'lock must be a regular file')
    lock = load_json(path.read_bytes())
    require(set(lock) == {'schema', 'bundleVersion', 'sourceCommit',
                          'manifestSha256', 'archiveSha256'}, 'lock fields')
    require(lock['schema'] == 'emusa80535-control-lock-v1' and
            lock['bundleVersion'] == VERSION, 'lock profile')
    require(isinstance(lock['sourceCommit'], str) and
            re.fullmatch('[0-9a-f]{40}', lock['sourceCommit']), 'lock source')
    for key in ('manifestSha256', 'archiveSha256'):
        require(isinstance(lock[key], str) and re.fullmatch('[0-9a-f]{64}', lock[key]),
                'lock hash')
    require(source is None or source == lock['sourceCommit'], 'source conflicts with accepted lock')
    require(expected_manifest is None or expected_manifest == lock['manifestSha256'],
            'manifest pin conflicts with accepted lock')
    return lock['sourceCommit'], lock['manifestSha256'], lock['archiveSha256']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['check', 'export', 'install'])
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--expected-manifest')
    parser.add_argument('--source-commit')
    parser.add_argument('--lock', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--destination', type=Path)
    args = parser.parse_args()
    try:
        expected_archive = None
        if args.lock is not None:
            args.source_commit, args.expected_manifest, expected_archive = locked_identity(
                args.lock, args.source_commit, args.expected_manifest)
        if args.command == 'check':
            manifest, digest = check(args.root, args.expected_manifest)
            if expected_archive is not None:
                files, _ = payload(args.root, args.source_commit, args.expected_manifest)
                require(sha(archive_bytes(files)) == expected_archive, 'archive hash mismatch')
            result = {'bundleVersion': VERSION, 'manifestSha256': digest,
                      'payloads': len(manifest['files'])}
        elif args.command == 'export':
            require(args.output is not None, 'export requires --output')
            result = export(args.root, args.output, args.source_commit, args.expected_manifest, expected_archive)
        else:
            require(args.destination is not None, 'install requires --destination')
            result = install(args.root, args.destination, args.source_commit, args.expected_manifest, expected_archive)
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(2, str(error)+'\n')
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
