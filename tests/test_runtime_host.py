#!/usr/bin/env python3
"""Process-level script1 and independently compiled SDK0.1 ABI1 checks."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import sys
import jsonschema

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get('RUNTIME_BIN', ROOT / 'emu-runtime')).resolve()
OUTPUT_SCHEMA = json.loads((ROOT / 'runtime/output1.schema.json').read_text())
FORMAT = jsonschema.FormatChecker()
@FORMAT.checks('uint64')
def uint64(value):
    return not isinstance(value, str) or (value.isascii() and value.isdigit() and str(int(value)) == value and int(value) <= 2**64 - 1)
VALIDATOR = jsonschema.Draft202012Validator(OUTPUT_SCHEMA, format_checker=FORMAT)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    for arg in ['--help', '--version']:
        assert subprocess.run([BINARY, arg], capture_output=True).returncode == 0
    with tempfile.TemporaryDirectory(prefix='runtime-host-') as temp:
        work = Path(temp)
        include = work / 'old-sdk'
        include.mkdir()
        # Genuine immutable 0.1 bundle headers, never regenerated from current API.
        for header in ['board.h', 'types.h', 'version.h']:
            sdk01 = os.environ.get('EMUIF_SDK01_INCLUDE')
            data = (Path(sdk01) / 'emusa80535/contracts' / header).read_bytes() if sdk01 else subprocess.check_output(['git', 'show', f'7d924cc:lib/include/emusa80535/contracts/{header}'], cwd=ROOT)
            target = include / 'emusa80535/contracts' / header
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        modules = []
        for mode in range(15):
            module = work / f'board-{mode}.so'
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-shared', '-fPIC', '-O1', '-g', f'-DFIXTURE_MODE={mode}', '-I' + str(include), str(ROOT / 'tests/runtime/synthetic_board.c'), '-o', str(module)], check=True)
            modules.append(module)
        image = work / 'image.bin'
        # MOV DPTR,#1234; MOV A,#42; MOVX @DPTR,A; MOVX A,@DPTR;
        # MOV P1,#33; SJMP self. Exactly two accesses, one port write.
        image.write_bytes(bytes.fromhex('9012347442f0e075903380fe') + bytes(65536 - 12))
        config = work / 'config.json'
        config.write_bytes(b'{}')
        script = work / 'commands.ndjson'
        header = {'format': 'emu-runtime-script', 'version': 1}
        def invoke(ops=None, *, raw=None, module=None, promise=True, changes=None, expected=0):
            if raw is None:
                raw = ''.join(json.dumps(x, separators=(',', ':')) + '\n' for x in [header, *(ops or [])]).encode()
            script.write_bytes(raw)
            module = module or modules[0]
            args = {'--variant': 'sab80535', '--clock-hz': '11059200', '--seed': '0', '--entry': '0', '--image': str(image), '--image-sha256': digest(image), '--board': str(module), '--board-sha256': digest(module), '--board-config': str(config), '--script': str(script)}
            args.update(changes or {})
            cmd = [str(BINARY)]
            for k, v in args.items(): cmd += [k, v]
            if promise: cmd += ['--board-inspect-fault']
            result = subprocess.run(cmd, capture_output=True)
            assert result.returncode == expected, (result.returncode, expected, result.stderr.decode(), result.stdout[:2000])
            if expected == 2:
                assert not result.stdout
                return []
            assert result.stdout.endswith(b'\n')
            assert all(len(line) + 1 <= 65536 for line in result.stdout.splitlines())
            records = [json.loads(line) for line in result.stdout.splitlines()]
            for record in records:
                errors = list(VALIDATOR.iter_errors(record))
                assert not errors, (record, [e.message for e in errors])
            assert records[0]['imageSha256'] == digest(image)
            assert records[0]['module']['sha256'] == digest(module)
            assert records[0]['configSha256'] == digest(config)
            assert records[0]['module']['inspectFaultPromise'] == promise
            return records
        inspect = {'op': 'boardInspect', 'schema': 'synthetic.status.v1', 'request': {}}
        state = {'op': 'state'}
        run = {'op': 'run', 'cycles': 100}
        reset = {'op': 'reset', 'seed': 0, 'entry': 0}
        ops = [state, run, inspect, {'op': 'pause'}, {'op': 'step', 'maxCycles': 10}, reset, run, inspect]
        records = invoke(ops)
        assert len(records) == len(ops) + 1
        assert records[3]['body']['payload']['accesses'] == '2'
        assert records[3]['body']['payload']['ports'] == '1'
        assert records[3]['body']['payload']['value'] == 66
        assert records[3]['body']['payload'] == records[8]['body']['payload']
        assert records[6]['snapshot']['generation'] == '2'
        # Separate processes replay architecture and board evidence identically.
        again = invoke(ops)
        def normalize(records):
            return json.loads(json.dumps(records).replace(records[0]['snapshot']['sessionId'], '<session>'))
        assert normalize(records) == normalize(again)
        stimulus = lambda cycle: {'op': 'boardInput', 'schema': 'synthetic.inputs.v1', 'cycle': cycle, 'payload': {}}
        mixed = invoke([stimulus('20'), stimulus('3'), stimulus(None), run, inspect])
        assert [x['body']['order'] for x in mixed[1:4]] == ['1', '2', '3']
        assert mixed[-1]['body']['payload']['applied'] == '3'
        assert mixed[-1]['body']['payload']['lastOrder'] == '1'
        assert mixed[-1]['body']['payload']['lastCycle'] == '20'
        paused = invoke([{'op': 'replaceCodeBreakpoints', 'addresses': [5]}, run, state, {'op': 'step', 'maxCycles': 10}, inspect])
        assert paused[2]['snapshot']['reason'] == 'breakpoint'
        assert paused[2]['snapshot']['pc'] == 5
        assert paused[-1]['body']['payload']['accesses'] == '1'
        # Exact faults preserve diagnostics, reject execution, and permit explicit reset.
        config.write_bytes(b'{"fault":true}')
        fault = invoke([run, state, inspect, {'op': 'step', 'maxCycles': 10}, reset, state], expected=3)
        assert fault[1]['snapshot']['state'] == 'faulted'
        assert fault[1]['error']['code'] == 'UNSUPPORTED'
        assert fault[1]['progress'] == {'cycles': '3', 'instructions': '2'}
        later = invoke([run, run, inspect], expected=3)
        assert later[2]['error']['code'] == 'STATE'
        assert later[2]['progress'] == {'cycles': '0', 'instructions': '0'}
        assert later[1]['snapshot'] == later[2]['snapshot']
        assert fault[1]['snapshot']['safeBoundary'] is False
        assert fault[3]['body']['payload']['accesses'] == '1'
        assert fault[4]['success'] is False
        assert fault[5]['snapshot']['state'] == 'paused'
        no_promise = invoke([run, inspect], promise=False, expected=3)
        assert no_promise[2]['success'] is False
        config.write_bytes(b'{\n  \"valid\": true\n}\n')
        invoke([state])
        config.write_bytes(b'{\"duplicate\":1,\"duplicate\":2}')
        invoke([state], expected=2)
        config.write_bytes(b'{}')
        # Illegal late command prevents even the valid initial run or module startup.
        bad_commands = [
            {'op': 'unknown'}, {'op': 'run', 'cycles': 0}, {'op': 'run', 'cycles': 100001},
            {'op': 'run', 'cycles': True}, {'op': 'state', 'extra': 1},
            {'op': 'reset', 'seed': 2**32, 'entry': 0},
            {'op': 'replaceCodeBreakpoints', 'addresses': [1, 1]},
            {'op': 'replaceCodeBreakpoints', 'addresses': [65536]},
            {'op': 'boardInput', 'schema': 'x', 'payload': {}},
            stimulus('null'), stimulus('01'), stimulus(1), stimulus('18446744073709551616'),
            {'op': 'boardInspect', 'schema': 'bad/schema', 'request': {}},
            {'op': 'boardInspect', 'schema': 'x', 'request': {'large': 'x' * 4096}},
        ]
        nested = {}
        for _ in range(14): nested = {'nested': nested}
        bad_commands.append({'op': 'boardInput', 'schema': 'synthetic.inputs.v1', 'cycle': None, 'payload': nested})
        for bad in bad_commands: invoke([run, bad], expected=2)
        for raw in [b'', b'{}\n', b'{"format":"emu-runtime-script","version":2}\n', b'\xef\xbb\xbf' + json.dumps(header).encode() + b'\n', json.dumps(header).encode() + b'\r\n', json.dumps(header).encode() + b'\n\n', json.dumps(header).encode(), json.dumps(header).encode() + b'\n{"op":"state","op":"state"}\n', json.dumps(header).encode() + b'\n{"op":"state"}\x00\n', json.dumps(header).encode() + b'\n{"op":"\xff"}\n']:
            invoke(raw=raw, expected=2)
        invoke([state] * 257, expected=2)
        invoke([stimulus('18446744073709551615'), state])
        invoke([stimulus('0'), state], expected=3)
        for mode in range(1, 15):
            invoke([run], module=modules[mode], changes={'--variant': '8051'} if mode == 14 else None, expected=2)
        invoke([state], changes={'--image-sha256': '0' * 64}, expected=2)
        invoke([state], changes={'--board-sha256': '0' * 64}, expected=2)
        invoke([state], changes={'--script': 'relative'}, expected=2)
        image.write_bytes(bytes(65535))
        invoke([state], expected=2)
    print('runtime host: script1 schemas, ABI0.1 modules, replay, controls, faults and startup rejection passed')

if __name__ == '__main__':
    main()
