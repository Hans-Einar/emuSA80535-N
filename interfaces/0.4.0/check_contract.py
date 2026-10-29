#!/usr/bin/env python3
"""Standalone contract/tool conformance; actual runtime proofs are separate."""
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parent
sys.dont_write_bytecode = True

def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, ROOT/filename)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

bundle = module('_emusa_control31_bundle', 'bundle.py')
wire = module('_emusa_control31_wire', 'wire.py')
CHECKS = 0


def check(condition, message):
    global CHECKS
    if not condition:
        raise AssertionError(message)
    CHECKS += 1


def rejected(action, label):
    global CHECKS
    try:
        action()
    except (ValueError, OSError, KeyError, TypeError, AssertionError):
        CHECKS += 1
        return
    # jsonschema.ValidationError is intentionally independent of the helper.
    except Exception as error:
        if error.__class__.__module__.startswith('jsonschema'):
            CHECKS += 1
            return
        raise
    raise AssertionError('accepted invalid case: '+label)


def schema_and_semantics():
    wire.BASE.jsonschema.Draft202012Validator.check_schema(wire.SCHEMA)
    examples = [json.loads(line) for line in (ROOT/'examples/runtime-3.1.ndjson').read_text().splitlines()]
    for example in examples:
        wire.validate(example)
    check(len(examples) > 50, 'inherited command fixtures missing')
    for line in (ROOT/'examples/trace-file-0.3.ndjson').read_text().splitlines():
        wire.validate_file_line(json.loads(line))
    # Reverse exactly the new delta and reproduce the frozen schema's bytes.
    old = copy.deepcopy(wire.SCHEMA)
    old['$id'] = 'urn:emusa80535:runtime:3.0'
    old['title'] = 'Target control 3.0 (not implemented)'
    old['oneOf'] = [b for b in old['oneOf'] if b['properties']['command'].get('const') != 'readCodeBreakpoints']
    old['oneOf'][-1]['properties']['command']['enum'].remove('readCodeBreakpoints')
    for branch in old['oneOf']:
        p = branch['properties']
        if p['command'].get('const') == 'hello':
            body = p.get('arguments', p.get('body'))['properties']
            body['protocol']['properties']['minor']['const'] = 0
            body['contractVersion']['const'] = '0.3.0'
            if 'capabilities' in body:
                body['capabilities']['items']['enum'].remove('readCodeBreakpoints')
                body['capabilities']['maxItems'] -= 1
    old_bytes = (json.dumps(old, indent=2)+'\n').encode()
    check(bundle.sha(old_bytes) == '459f6a4c2d8d419df5b750f60f9fe3b9d9f4766586eb8f86e31119eb91c7fc0e',
          'unintended inherited schema change')
    check(bundle.sha((ROOT/'schemas/trace-file-0.3.schema.json').read_bytes()) ==
          'e504082cfe5a4ef9f17a6c028a5c04f96521bcef5a73b1421123538d03fa00cd', 'file schema changed')
    request = {'type': 'request', 'id': 1, 'command': 'readCodeBreakpoints',
               'arguments': {'sessionId': 'a'*32}}
    response = {'type': 'response', 'id': 1, 'command': 'readCodeBreakpoints', 'success': True,
                'body': {'sessionId': 'a'*32, 'generation': '1', 'addresses': []}}
    for addresses in [[], [0], [65535], [0,65535], list(range(64512,65536))]:
        sample = copy.deepcopy(response); sample['body']['addresses'] = addresses
        wire.validate_exchange(request, sample)
        check(len(json.dumps(sample).encode())+1 < 65536, 'max list frame overflow')
    for addresses in [[True], [False], [1.0], [-1], [65536], [1,0], [1,1], list(range(1025))]:
        sample = copy.deepcopy(response); sample['body']['addresses'] = addresses
        rejected(lambda: wire.validate(sample), 'addresses '+str(addresses[:3]))
    for generation in ['0', '01', '-1', '18446744073709551616', 1, True]:
        sample = copy.deepcopy(response); sample['body']['generation'] = generation
        rejected(lambda: wire.validate(sample), 'generation')
    sample = copy.deepcopy(response); sample['body']['generation'] = '18446744073709551615'
    wire.validate_exchange(request, sample) # not compared to a stale attach generation
    for where, key, value in [('arguments','generation','1'), ('arguments','leaseId','1')]:
        sample = copy.deepcopy(request); sample[where][key] = value
        rejected(lambda: wire.validate(sample), 'extra request field')
    sample = copy.deepcopy(response); sample['body']['stopRevision'] = '1'
    rejected(lambda: wire.validate(sample), 'extra response field')
    sample = copy.deepcopy(response); sample['body']['sessionId'] = 'b'*32
    rejected(lambda: wire.validate_exchange(request, sample), 'foreign session')
    sample = copy.deepcopy(response); sample['id'] = 2
    rejected(lambda: wire.validate_exchange(request, sample), 'reply ID')
    failure = {'type':'response','id':1,'command':'readCodeBreakpoints','success':False,
               'error':{'code':'STALE','message':'foreign session','retryable':False}}
    wire.validate_exchange(request, failure)
    failure['error']['retryable'] = True
    rejected(lambda: wire.validate(failure), 'retryability')
    encoded = (json.dumps(request,separators=(',',':'))+'\n').encode()
    wire.validate_frame(encoded)
    for raw in [encoded[:-1], b'\xef\xbb\xbf'+encoded, encoded.replace(b'\n',b'\r\n'),
                encoded.replace(b'"id":1',b'"id":1,"id":1'), encoded+b'\n']:
        rejected(lambda: wire.validate_frame(raw), 'framing')
    old_hello = next(copy.deepcopy(x) for x in examples if x['command']=='hello' and x['type']=='request')
    old_hello['arguments']['protocol']['minor'] = 0
    old_hello['arguments']['contractVersion'] = '0.3.0'
    rejected(lambda: wire.validate(old_hello), 'old profile in new schema')
    header = json.loads((ROOT/'examples/trace-file-0.3.ndjson').read_text().splitlines()[0])
    header['contractManifestSha256'] = 'a'*64
    rejected(lambda: wire.validate_file_line(header), 'wire manifest used as file manifest')


def tool_integrity():
    manifest, digest = bundle.check(ROOT)
    bundle.check(ROOT, digest)
    rejected(lambda: bundle.check(ROOT, '0'*64), 'trusted pin mismatch')
    source = (ROOT/'SOURCE_COMMIT').read_text().strip() if (ROOT/'SOURCE_COMMIT').exists() else 'a'*40
    with tempfile.TemporaryDirectory(prefix='control31-contract-') as directory:
        directory = Path(directory)
        one, two = directory/'one.tgz', directory/'two.tgz'
        first = bundle.export(ROOT, one, source, digest)
        second = bundle.export(ROOT, two, source, digest)
        check(one.read_bytes() == two.read_bytes(), 'archive not reproducible')
        check(first['archiveSha256'] == second['archiveSha256'], 'archive hash mismatch')
        rejected(lambda: bundle.export(ROOT, one, source, digest), 'existing archive overwrite')
        rejected(lambda: bundle.export(ROOT, ROOT/'bad.tgz', source, digest), 'export inside root')
        lock = {'schema':'emusa80535-control-lock-v1','bundleVersion':'0.4.0',
                'sourceCommit':source,'manifestSha256':digest,'archiveSha256':first['archiveSha256']}
        lock_path=directory/'accepted.lock.json';lock_path.write_bytes(bundle.json_bytes(lock))
        pinned_source,pinned_manifest,pinned_archive=bundle.locked_identity(lock_path)
        locked_output=directory/'locked.tgz'
        bundle.export(ROOT,locked_output,pinned_source,pinned_manifest,pinned_archive)
        check(locked_output.read_bytes()==one.read_bytes(),'accepted lock changes archive')
        rejected(lambda: bundle.locked_identity(lock_path,'b'*40 if source!='b'*40 else 'c'*40),
                 'runtime source overrides accepted contract source')
        rejected(lambda: bundle.locked_identity(lock_path,expected_manifest='0'*64), 'conflicting lock pin')
        destination=directory/'must-not-exist'
        rejected(lambda: bundle.install(ROOT,destination,source,digest,'0'*64), 'wrong archive pin')
        check(not destination.exists(),'mismatch created destination')
        output=directory/'must-not-exist.tgz'
        rejected(lambda: bundle.export(ROOT,output,source,digest,'0'*64), 'wrong export pin')
        check(not output.exists(),'mismatch created archive')
        subprocess.run([sys.executable,str(ROOT/'bundle.py'),'check','--lock',str(lock_path)],
                       check=True,stdout=subprocess.PIPE)
        subprocess.run([sys.executable,str(ROOT/'bundle.py'),'install','--destination',str(directory/'locked-install'),
                        '--lock',str(lock_path)],check=True,stdout=subprocess.PIPE)
        rejected(lambda: bundle.locked_identity(directory/'absent-lock'), 'missing lock must not fallback')
        extract = directory/'extracted'; extract.mkdir()
        with tarfile.open(one) as archive:
            # Only inspect/extract this helper's just-generated archive.
            for member in archive:
                check(member.isfile() and member.name.startswith(bundle.PREFIX+'/') and
                      '..' not in Path(member.name).parts and member.mode == 0o644 and
                      member.uid == member.gid == member.mtime == 0, 'archive metadata/path')
            for member in archive.getmembers():
                target=extract/member.name
                target.parent.mkdir(parents=True,exist_ok=True)
                with archive.extractfile(member) as incoming:
                    target.write_bytes(incoming.read())
        copied = extract/bundle.PREFIX
        bundle.check(copied, digest)
        installed = directory/'installed'
        bundle.install(copied, installed, source, digest)
        bundle.install(copied, installed, source, digest)
        rejected(lambda: bundle.install(copied, installed, 'b'*40 if source!='b'*40 else 'c'*40, digest),
                 'provenance rewrite')
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
        subprocess.run([sys.executable,str(copied/'bundle.py'),'check','--expected-manifest',digest],
                       cwd=directory,env=env,check=True,stdout=subprocess.PIPE)
        subprocess.run([sys.executable,'-c',
                        'import importlib.util,json; s=importlib.util.spec_from_file_location("copied_wire",'+repr(str(copied/'wire.py'))+'); m=importlib.util.module_from_spec(s); s.loader.exec_module(m); [m.validate_frame(x) for x in open('+repr(str(copied/'examples/runtime-3.1.ndjson'))+',"rb")]'],
                       cwd=directory,env=env,check=True)
        check(True, 'standalone copied validator')
        p=installed/'README.md'; original=p.read_bytes(); p.write_bytes(original+b'tamper')
        rejected(lambda: bundle.check(installed,digest), 'payload tamper');p.write_bytes(original)
        extra=installed/'unexpected.txt';extra.write_text('x')
        rejected(lambda: bundle.check(installed,digest), 'unknown payload');extra.unlink()
        p.unlink();p.symlink_to(copied/'README.md')
        rejected(lambda: bundle.check(installed,digest), 'symlink payload');p.unlink();p.write_bytes(original)
        fifo=installed/'fifo';os.mkfifo(fifo)
        rejected(lambda: bundle.check(installed,digest), 'nonregular payload');fifo.unlink()
        mp=installed/'manifest.json';original=mp.read_bytes(); altered=json.loads(original)
        altered['files']['../outside']='0'*64;mp.write_bytes(bundle.json_bytes(altered))
        rejected(lambda: bundle.check(installed), 'unsafe manifest path');mp.write_bytes(original)
        receipt=installed/'BUNDLE-RECEIPT.json';original=receipt.read_bytes();receipt.write_bytes(b'{}\n')
        rejected(lambda: bundle.check(installed,digest), 'receipt tamper');receipt.write_bytes(original)
        bundle.check(installed,digest)
    return len(manifest['files'])


def main():
    schema_and_semantics()
    payloads=tool_integrity()
    print('PASS: wire3.1 sidecar, %d payloads, %d focused checks; inherited schema exact, strict enumeration, copied validator and deterministic export' % (payloads,CHECKS))


if __name__ == '__main__':
    main()
