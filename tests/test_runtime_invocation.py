#!/usr/bin/env python3
"""Real CPU invocation capture through Unix and shared ring/file destinations.
RUNTIME_BIN/SDK_INCLUDE select installed artifacts; INVOCATION_REPORT preserves
actual schema/semantic responses and JSONL for an independent release check.
"""
import contextlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from test_runtime_control import BINARY, ROOT
from test_runtime_trace import TraceClient, RESPONSES, configuration, digest
from test_runtime_trace_file import parse_files, segments, sink_open, sink_op


def program(body=bytes.fromhex('1200200022')):
    code = bytearray(64)
    code[:5] = bytes.fromhex('12001080fe')
    code[16:16+len(body)] = body
    code[32:34] = bytes.fromhex('0022')
    return code


@contextlib.contextmanager
def host(code=None, board_config=b'{}', file=False):
    with tempfile.TemporaryDirectory(prefix='invocation3-', dir='/tmp') as directory:
        work = Path(directory)
        module, image, config, endpoint = [work/n for n in ('board.so', 'code.bin', 'config.json', 'control')]
        subprocess.run([os.environ.get('CC', 'cc'), '-shared', '-fPIC', '-std=c99',
                        '-I'+os.environ.get('SDK_INCLUDE', str(ROOT/'lib/include')),
                        str(ROOT/'tests/runtime/synthetic_board.c'), '-o', str(module)], check=True)
        code = program() if code is None else code
        image.write_bytes(code+bytes(65536-len(code))); config.write_bytes(board_config)
        args = [str(BINARY), '--variant', 'sab80535', '--clock-hz', '11059200', '--seed', '0',
                '--entry', '0', '--image', str(image), '--image-sha256', digest(image), '--board',
                str(module), '--board-sha256', digest(module), '--board-config', str(config),
                '--control-unix', str(endpoint), '--execution-mode', 'externally-driven']
        if file: args += ['--trace-file', 'capture='+str(work/'trace')]
        proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        client = None
        try:
            deadline = time.monotonic()+5
            while not endpoint.exists() and proc.poll() is None and time.monotonic()<deadline:
                time.sleep(.01)
            assert endpoint.exists() and proc.poll() is None
            client = TraceClient(endpoint); client.endpoint = endpoint
            hello = client.hello(); assert 'captureB' in hello['capabilities']
            initial = client.attach()
            yield client, work, hello, initial
        finally:
            if client: client.close()
            if proc.poll() is None: proc.terminate()
            out, err = proc.communicate(timeout=5)
            assert proc.returncode == 0 and not out and not err, (proc.returncode, out, err)


def start(c, mode=1, context='0', entry=16, records=10000, cycles=10000, rearm=False, stop=False, error=None):
    return c.call('captureStart', c.args(traceId=2, mode=mode, contextId=context, entryPc=entry,
                  maxRecords=str(records), maxCycles=str(cycles), rearmOnReset=rearm, stopCpu=stop), error)


def events(rows): return [r for r in rows if r['kind'] == 0]


def completed(c, cap, rows):
    status = c.capture(cap); all_events = events(rows)
    selected = [r['event'] for r in all_events if 2 in r['traceIds']]
    assert status['state'] == 2 and status['reason'] == 0, status
    assert len(selected) == int(status['records'])
    source_ids = [int(r['event']['sequence']) for r in all_events]
    assert source_ids == sorted(set(source_ids))
    assert all(r['traceIds'] == sorted(set(r['traceIds'])) for r in all_events)
    root_return = next(e for e in selected if e['kind'] == 7 and e['contextId'] == status['contextId'])
    assert selected[-1]['kind'] == 2 and selected[-1]['instructionId'] == root_return['instructionId']
    assert selected[-1]['sequence'] == status['endSequence']
    assert any(int(r['event']['sequence']) > int(status['endSequence']) and r['traceIds'] == [1]
               for r in all_events)
    return status, selected


def ring_and_mode2():
    with host() as (c, _, _, _):
        c.config(configuration(2)); a = c.start(); b = start(c)
        assert b['state'] == 0 and b['startSequence'] == b['invocationId'] == '0'
        c.run(40); c.pause(); rows = c.records()
        status, selected = completed(c, b, rows)
        assert selected[0]['kind'] == 6 and selected[1]['kind'] == 2
        assert selected[1]['sequence'] == status['startSequence']
        assert selected[0]['contextId'] == status['contextId']
        assert len([e for e in selected if e['kind'] == 6]) == 2  # root and nested call
        assert int(c.capture(a)['records']) == len(events(rows))
        assert c.capture(a)['state'] == 1
    with host() as (c, _, _, _):
        c.config(configuration(2)); c.call('replaceCodeBreakpoints', c.args(addresses=[16]))
        start(c, mode=2, entry=0, context='1', error='STATE')
        c.run(30)
        assert c.status()['state'] == 'paused'
        call = next(r['event'] for r in c.records() if r['kind'] == 0 and r['event']['kind'] == 6)
        b = start(c, mode=2, entry=0, context=call['contextId'])
        assert b['state'] == 1 and b['contextId'] == call['contextId']
        c.call('replaceCodeBreakpoints', c.args(addresses=[])); c.run(40); c.pause()
        status = c.capture(b); assert status['state'] == 2 and status['reason'] == 0


def limits_and_reset():
    with host() as (c, _, _, _):
        c.config(configuration(2)); b = start(c, records=1); c.run(30); c.pause()
        s = c.capture(b); rows = events(c.records())
        assert s['state'] == 3 and s['reason'] == 2 and s['records'] == '1'
        assert s['invocationId'] != '0' and s['contextId'] != '0'
        selected = [r['event'] for r in rows if 2 in r['traceIds']]
        assert len(selected) == 1 and selected[0]['kind'] == 6
        assert any(r['event']['sequence'] == s['startSequence'] and r['traceIds'] == [1] for r in rows)
    with host() as (c, _, _, _):
        c.config(configuration(2)); b = start(c, cycles=1); c.run(30); c.pause()
        s = c.capture(b); assert s['state'] == 3 and s['reason'] == 2 and s['cycles'] == '1'
        rows = events(c.records()); binding = next(r['event'] for r in rows if r['event']['sequence'] == s['startSequence'])
        assert all(int(r['event']['cycle']) < int(binding['cycle'])+1 for r in rows if 2 in r['traceIds'])
    with host() as (c, _, _, _):
        cfg = configuration(2); c.config(cfg); b = start(c, rearm=True)
        c.call('replaceCodeBreakpoints', c.args(addresses=[16])); c.run(20)
        bound = c.capture(b); assert bound['state'] == 1
        c.reset(); old = c.capture(b)
        assert old['state'] == 3 and old['reason'] == 3 and old['generation'] == '1'
        successor = c.capture({'captureId': old['successorCaptureId']})
        assert successor['generation'] == '2' and successor['state'] == 0
        assert c.call('traceConfig', c.identity)['config'] == cfg
        c.call('replaceCodeBreakpoints', c.args(addresses=[])); c.run(30); c.pause()
        assert c.capture(successor)['state'] == 2


def filtered_and_fault():
    with host() as (c, _, _, _):
        cfg = configuration(2); cfg['traces'][1]['enabled'] = False
        c.config(cfg); b = start(c); c.run(40); c.pause()
        s = c.capture(b); assert s['state'] == 2 and s['records'] == '0'
        assert s['startSequence'] != '0' and s['endSequence'] != '0'
    with host(program(bytes.fromhex('32'))) as (c, _, _, _):
        c.config(configuration(2)); b = start(c); c.run(20); c.pause()
        s = c.capture(b); assert s['state'] == 3 and s['reason'] == 5
    with host(program(bytes.fromhex('901234f022')), b'{"fault":true}') as (c, _, _, _):
        c.config(configuration(2)); b = start(c); c.run(30)
        s = c.capture(b); assert s['state'] == 3 and s['reason'] == 7
        rows = events(c.records()); abort = next(r['event'] for r in rows if r['event']['kind'] == 3)
        assert not any(r['event']['kind'] == 2 and r['event']['instructionId'] == abort['instructionId'] for r in rows)


def stop_policies():
    with host() as (c, _, _, _):
        c.config(configuration(2)); b = start(c, stop=True)
        p = c.run(100); assert p['state'] == 'paused' and p['reason'] == 'budget', p
        s = c.capture(b); assert s['state'] == 2 and s['reason'] == 0
        assert c.status()['pc'] == 3
    with host(program(bytes.fromhex('901234f022')), b'{"fault":true}') as (c, _, _, _):
        c.config(configuration(2)); b = start(c, stop=True)
        p = c.run(100); assert p['state'] == 'faulted' and p['reason'] == 'fault', p
        s = c.capture(b); assert s['state'] == 3 and s['reason'] == 7
    with host() as (c, _, _, _):
        c.config(configuration(2)); b = start(c, stop=True)
        c.call('replaceCodeBreakpoints', c.args(addresses=[16])); c.run(100)
        c.reset(); assert c.status()['reason'] == 'entry'
        s = c.capture(b); assert s['state'] == 3 and s['reason'] == 3
        c.call('replaceCodeBreakpoints', c.args(addresses=[])); p = c.run(20)
        assert p['state'] == 'running', p  # no stale pending stop from reset
        c.pause()


def shared_file():
    with host(file=True) as (c, work, hello, initial):
        cfg = configuration(2); cfg['destinations'][0].update(kind=1, capacity=256)
        c.config(cfg); assert sink_open(c, 'capture')['state'] == 0
        a = c.start(); b = start(c); c.run(40); c.pause()
        closed = sink_op(c, 'sinkClose'); assert closed['lost'] == '0' and closed['error'] == 'OK'
        rows = parse_files(work, 'trace', cfg, hello, initial)
        completed(c, b, rows)
        assert int(c.capture(a)['records']) == len(events(rows))
        assert any(r['traceIds'] == [1, 2] for r in events(rows))


def main():
    ring_and_mode2(); limits_and_reset(); filtered_and_fault(); stop_policies(); shared_file()
    if os.environ.get('INVOCATION_REPORT'):
        Path(os.environ['INVOCATION_REPORT']).write_text(json.dumps(
            dict(runtime=str(BINARY), responses=RESPONSES, segments=segments), indent=2)+'\n')
    print(f'Unix invocation B: {len(RESPONSES)} validated responses, {sum(len(s["lines"]) for s in segments)} JSONL lines; nested scope, entry mode2, limits, reset, filtering, faults and shared A+B file passed')

if __name__ == '__main__': main()
