#!/usr/bin/env python3
"""Actual persistent trace/file controls and exported JSONL, usable after install.
RUNTIME_BIN and SDK_INCLUDE select installed artifacts; TRACE_FILE_REPORT records
schema-validated responses and parsed segments for durable verification.
"""
import contextlib
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from test_runtime_control import BINARY, ROOT, SCHEMA, Client, wire
sys.path.insert(0, str(ROOT / 'interfaces/tools'))
import target_wire
FILE_SCHEMA = json.loads((ROOT / 'interfaces/0.3.0/schemas/trace-file.schema.json').read_text())
responses = []
segments = []

class ValidatedClient(Client):
    def response(self):
        value = super().response()
        target_wire.validate(value, SCHEMA)
        responses.append(value)
        return value

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def config():
    selector = dict(matchMask=0, kind=0, space=0, accessMask=0, pcFirst=0, pcLast=0, addressFirst=0, addressLast=0)
    return dict(traces=[dict(traceId=i, destinationId=1, enabled=True, interruptPolicy=0, tag='trace'+str(i), comment='Unicode æ') for i in (1, 2)],
                destinations=[dict(destinationId=1, kind=1, capacity=256, stopOnFull=False)],
                points=[dict(pointId=1, enabled=True, selector=selector, traceIds=[1,2])], gates=[], watches=[])
def wait_sink(c, operation):
    until = time.monotonic() + 5
    while time.monotonic() < until:
        s = c.call('sinkStatus', {**c.identity, 'destinationId': 1})
        if int(s['completedOperationId']) >= int(operation): return s
        time.sleep(.001)
    raise AssertionError('sink operation timeout')
def sink_open(c, token, max_records='100000'):
    s = c.call('sinkOpen', c.args(destinationId=1, destinationToken=token, maxSegmentBytes='65536', maxSegments=4, maxTotalRecords=max_records))
    assert s['state'] == 1 and int(s['completedOperationId']) + 1 == int(s['operationId']), s
    return wait_sink(c, s['operationId'])
def sink_op(c, name):
    s = c.call(name, c.args(destinationId=1))
    return wait_sink(c, s['operationId'])
def terminate(c):
    until = time.monotonic() + 5
    while time.monotonic() < until:
        c.i += 1; c.socket.sendall(wire(c.i, 'terminate', c.args())); r = c.response()
        if r['success']: return r
        assert r['error']['code'] == 'BUSY', r
        time.sleep(.001)
    raise AssertionError('terminate remained busy')
def parse_files(work, stem, expected_config, hello, snapshot):
    files = sorted(work.glob(stem + '.*.jsonl')); assert files
    rows = []
    for index, path in enumerate(files):
        assert path.name == f'{stem}.{index:04d}.jsonl'
        raw = path.read_bytes(); assert raw.endswith(b'\n') and len(raw) <= 65536
        lines = [target_wire.validate_frame(line+b'\n', FILE_SCHEMA) for line in raw.splitlines()]
        h, footer = lines[0], lines[-1]
        assert h['type'] == 'header' and footer['type'] == 'footer'
        assert h['segment'] == footer['segment'] == index
        assert h['runtimeCommit'] == hello['runtimeCommit']
        assert h['contractManifestSha256'] == hello['contractManifestSha256']
        for key in ('sessionId', 'imageSha256', 'moduleSha256', 'configSha256'): assert h[key] == snapshot[key]
        assert h['configuration'] == expected_config
        canonical = json.dumps(expected_config, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode()
        assert h['traceConfigSha256'] == hashlib.sha256(canonical).hexdigest()
        records = [line['record'] for line in lines[1:-1]]
        assert int(footer['records']) == len(records)
        assert footer['complete'] and footer['status'] in ('OK', 'CAPACITY')
        assert path.stat().st_mode & 0o777 == 0o600
        if records: assert footer['lastRecordSequence'] == records[-1]['recordSequence']
        rows.extend(records); segments.append(dict(name=path.name, bytes=len(raw), lines=lines))
    seq = [int(r['recordSequence']) for r in rows]
    assert seq == sorted(set(seq))
    return rows

def main():
    with tempfile.TemporaryDirectory(prefix='tracefile3-', dir='/tmp') as directory:
        work = Path(directory)
        module, image, board_config, endpoint = [work/x for x in ('board.so', 'code.bin', 'config.json', 'control')]
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-shared', '-fPIC', '-I'+os.environ.get('SDK_INCLUDE', str(ROOT/'lib/include')), str(ROOT/'tests/runtime/synthetic_board.c'), '-o', str(module)], check=True)
        program = bytes.fromhex('12000880fb0000000422')
        image.write_bytes(program + bytes(65536-len(program))); board_config.write_bytes(b'{}')
        args = [str(BINARY), '--variant', 'sab80535', '--clock-hz', '11059200', '--seed', '0', '--entry', '0', '--image', str(image), '--image-sha256', digest(image), '--board', str(module), '--board-sha256', digest(module), '--board-config', str(board_config), '--control-unix', str(endpoint), '--execution-mode', 'externally-driven', '--trace-file', 'primary='+str(work/'trace'), '--trace-file', 'second='+str(work/'second'), '--trace-file', 'third='+str(work/'third')]
        proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        c = None
        try:
            until = time.monotonic()+5
            while not endpoint.exists() and proc.poll() is None and time.monotonic()<until: time.sleep(.01)
            assert endpoint.exists() and proc.poll() is None, proc.communicate()
            c=ValidatedClient(endpoint); hello=c.hello(); assert {'traceConfig','tracePage','captureA','traceFile'} <= set(hello['capabilities'])
            initial=c.attach(); cfg=config()
            revision=c.call('replaceTraceConfig', c.args(expectedRevision='0', config=cfg))['revision']
            c.call('sinkOpen', c.args(destinationId=1, destinationToken='missing', maxSegmentBytes='65536', maxSegments=4, maxTotalRecords='100'), 'DENIED')
            opened=sink_open(c, 'primary'); assert opened['state']==0 and opened['error']=='OK'
            c.call('replaceTraceConfig', c.args(expectedRevision=revision,config=cfg), 'BUSY')
            cap=c.call('captureStart',c.args(traceId=1,mode=0,contextId='0',entryPc=0,maxRecords='10000',maxCycles='10000',rearmOnReset=False,stopCpu=False)); assert cap['state']==1
            c.call('resume', c.args()); c.call('advance', c.args(cycleBudget=80)); c.call('pause',c.args())
            before=c.status(); flushed=sink_op(c,'sinkFlush'); assert flushed['state']==0 and flushed['records']!='0' and flushed['lost']=='0', flushed
            assert c.status()['cycle']==before['cycle']
            scope=c.call('captureStatus',{**c.identity,'captureId':cap['captureId']}); assert scope['state']==1 and int(scope['records'])>0
            reset=c.call('reset', c.args(seed=0,entry=0)); c.generation=reset['generation']
            old_session=c.identity['sessionId']; c.close(); c=ValidatedClient(endpoint); c.hello(); attached=c.attach(); assert attached['sessionId']==old_session
            assert c.call('sinkStatus',{**c.identity,'destinationId':1})['state']==0
            c.call('resume',c.args()); c.call('advance',c.args(cycleBudget=20)); c.call('pause',c.args())
            stopped=c.call('captureStop',c.args(captureId=cap['captureId'])); assert stopped['state']==4
            c.call('resume',c.args()); c.call('advance',c.args(cycleBudget=20)); c.call('pause',c.args())
            assert c.call('captureStatus',{**c.identity,'captureId':cap['captureId']})['records']==stopped['records']
            closed=sink_op(c,'sinkClose'); assert closed['state']==3 and closed['error']=='OK' and closed['lost']=='0'
            rows=parse_files(work,'trace',cfg,hello,initial); events=[r for r in rows if r['kind']==0]
            assert {1,2,6,7} <= {r['event']['kind'] for r in events}
            assert {'1','2'} <= {r['generation'] for r in events}
            assert any(r['traceIds']==[1,2] for r in events) and any(r['traceIds']==[2] for r in events)
            assert all(r['traceIds']==sorted(set(r['traceIds'])) for r in events)
            assert int(closed['records'])==len(rows) and int(closed['bytes'])==sum(x['bytes'] for x in segments)
            assert len(list(work.glob('trace.*.jsonl')))>=2
            # Reopen keeps operation and file storage IDs, resetting per-open counts.
            reopened=sink_open(c,'second','1'); assert int(reopened['operationId'])>int(closed['operationId']) and reopened['records']=='0'
            c.call('resume',c.args()); c.call('advance',c.args(cycleBudget=10)); c.call('pause',c.args())
            until=time.monotonic()+5
            while time.monotonic()<until:
                limited=c.call('sinkStatus',{**c.identity,'destinationId':1})
                if limited['state']==3:break
                time.sleep(.001)
            assert limited['state']==3 and limited['error']=='CAPACITY' and limited['records']=='1' and int(limited['lost'])>0
            rows2=parse_files(work,'second',cfg,hello,initial); assert int(rows2[0]['recordSequence'])>int(rows[-1]['recordSequence'])
            # Terminate actively drains/closes an open sink through asynchronous BUSY.
            sink_open(c,'third'); c.call('resume',c.args()); c.call('advance',c.args(cycleBudget=10)); c.call('pause',c.args())
            terminate(c); c.close(); c=None; proc.wait(timeout=5)
            parse_files(work,'third',cfg,hello,initial)
        finally:
            if c is not None:c.close()
            if proc.poll() is None:proc.terminate()
            out,err=proc.communicate(timeout=5)
            assert proc.returncode==0 and not out and not err,(proc.returncode,out,err)
        # Existing output failure disables this destination while execution and
        # configuration controls remain available; the host reports partial output.
        preserved=(work/'trace.0000.jsonl').read_bytes()
        proc=subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        c=None
        try:
            until=time.monotonic()+5
            while not endpoint.exists() and proc.poll() is None and time.monotonic()<until:time.sleep(.01)
            assert endpoint.exists() and proc.poll() is None
            c=ValidatedClient(endpoint);c.hello();c.attach()
            revision=c.call('replaceTraceConfig',c.args(expectedRevision='0',config=cfg))['revision']
            failed=sink_open(c,'primary')
            assert failed['state']==4 and failed['error']=='IO' and failed['partialOutput']
            assert failed['bytes']=='0' and (work/'trace.0000.jsonl').read_bytes()==preserved
            before=c.status()
            c.call('resume',c.args());c.call('advance',c.args(cycleBudget=20));c.call('pause',c.args())
            after=c.status();assert int(after['cycle'])>int(before['cycle'])
            c.call('replaceTraceConfig',c.args(expectedRevision=revision,config=cfg))
            terminate(c);c.close();c=None;proc.wait(timeout=5)
        finally:
            if c is not None:c.close()
            if proc.poll() is None:proc.terminate()
            out,err=proc.communicate(timeout=5)
            assert proc.returncode==3 and not out and b'partial output or failed footer' in err,(proc.returncode,out,err)
        if os.environ.get('TRACE_FILE_REPORT'):
            Path(os.environ['TRACE_FILE_REPORT']).write_text(json.dumps(dict(runtime=str(BINARY),responses=responses,segments=segments),ensure_ascii=False,indent=2)+'\n')
        print(f'Unix persistent trace/file: {len(responses)} responses and {sum(len(x["lines"]) for x in segments)} JSONL lines validated; reset/reconnect/coalescing/rotation/reopen/termination passed')
if __name__=='__main__':main()
