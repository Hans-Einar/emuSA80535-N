#!/usr/bin/env python3
"""Offline wire3.1 validation, with exact frozen0.3 inherited semantics.

Use validate_exchange for request/response session correlation. Snapshot freshness
and no CPU progression require actual execution evidence, not a schema assertion.
"""
import builtins
import hashlib
import json
from pathlib import Path
import types

ROOT = Path(__file__).resolve().parent
BASE_MANIFEST = 'ed3917b0d3a7300aca94171f1259eacf9861b0ee87376afa534690618da27ff0'
BASE_HASHES = {
    'target_wire.py': 'cb44b6a04b3319b98348dc07cb428b3d5358242dee2cd38d65a7bad4e5f8668c',
    'bundle.py': 'bb4739bbf98fc5a2387895dc6258b8b62365e48dc5f098978082da5e678914d1',
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def _baseline():
    sources = {}
    for name, expected in BASE_HASHES.items():
        path = ROOT / 'baseline-tools' / name
        require(not path.is_symlink(), 'baseline validator symlink')
        sources[name] = path.read_bytes()
        require(hashlib.sha256(sources[name]).hexdigest() == expected,
                'baseline validator checksum: ' + name)
    # Bind only the frozen validator's dependency to its frozen sibling. Never
    # search sys.path for an unrelated installed module named "bundle" and never
    # replace a caller's sys.modules entry. All other imports retain Python rules.
    dependency = types.ModuleType('_emusa_wire31_frozen_bundle')
    dependency.__file__ = str(ROOT / 'baseline-tools' / 'bundle.py')
    exec(compile(sources['bundle.py'], dependency.__file__, 'exec'), dependency.__dict__)
    def frozen_import(name, globals=None, locals=None, fromlist=(), level=0):
        if name == 'bundle' and level == 0:
            return dependency
        return builtins.__import__(name, globals, locals, fromlist, level)
    inherited = types.ModuleType('_emusa_wire31_frozen_validator')
    inherited.__file__ = str(ROOT / 'baseline-tools' / 'target_wire.py')
    inherited.__dict__['__builtins__'] = dict(vars(builtins), __import__=frozen_import)
    exec(compile(sources['target_wire.py'], inherited.__file__, 'exec'), inherited.__dict__)
    return inherited


BASE = _baseline()
SCHEMA = json.loads((ROOT / 'schemas' / 'runtime-3.1.schema.json').read_text())
FILE_SCHEMA = json.loads((ROOT / 'schemas' / 'trace-file-0.3.schema.json').read_text())


def validate(value, schema=None):
    """Check complete schema and inherited plus enumeration semantics."""
    BASE.validate(value, SCHEMA if schema is None else schema)
    body = value.get('body', {})
    if value.get('command') == 'hello' and value.get('success') is True:
        caps = body['capabilities']
        if 'readCodeBreakpoints' in caps:
            require('codeBreakpoints' in caps, 'breakpoint read capability dependency')
    if value.get('command') == 'readCodeBreakpoints' and value.get('success') is True:
        addresses = body['addresses']
        require(all(type(a) is int and 0 <= a <= 65535 for a in addresses),
                'CODE addresses must be JSON integers, not bool or floating values')
        require(len(addresses) <= 1024 and addresses == sorted(set(addresses)),
                'CODE addresses must be strictly ascending and unique')
    return value


def validate_frame(data, schema=None):
    """Strict LF framing/duplicate-key checks plus wire3.1 semantics."""
    require(isinstance(data, bytes), 'frame must be bytes')
    require(data.endswith(b'\n') and len(data) <= 65536, 'frame boundary')
    require(b'\r' not in data and b'\n' not in data[:-1] and
            not data.startswith(b'\xef\xbb\xbf'), 'framing')
    value = BASE.bundle.load_json(data[:-1].decode('utf-8'))
    return validate(value, schema)


def validate_exchange(request, response):
    """Correlate reply ID/command/session, without inventing generation freshness."""
    validate(request)
    validate(response)
    require(request.get('type') == 'request' and response.get('type') == 'response',
            'exchange direction')
    require(request['id'] == response['id'] and request['command'] == response['command'],
            'response correlation')
    if response['success'] and request['command'] == 'hello':
        require(set(request['arguments']['requiredCapabilities']) <=
                set(response['body']['capabilities']), 'required capabilities missing')
    if response['success'] and request['command'] == 'readCodeBreakpoints':
        require(request['arguments']['sessionId'] == response['body']['sessionId'],
                'enumeration session mismatch')
    return response


def validate_file_line(value):
    """File format remains frozen0.3 even when opened by a wire3.1 connection."""
    BASE.validate(value, FILE_SCHEMA)
    if value.get('type') == 'header':
        require(value['contractManifestSha256'] == BASE_MANIFEST, 'file base manifest')
    return value
