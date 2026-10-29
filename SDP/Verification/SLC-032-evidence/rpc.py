#!/usr/bin/env python3
"""Owned synthetic Unix benchmark. Never connects to an existing runtime.
All child processes are reaped. Timings are observations, never pass thresholds.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import statistics
import subprocess
import sys
import time

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
a = parser.parse_args()
a.source = a.source.resolve(); a.binary = a.binary.resolve(); a.output = a.output.resolve()
a.output.mkdir(mode=0o700)
private = a.output / 'private'; private.mkdir(mode=0o700)
sys.path.insert(0, str(a.source / 'tests'))
from test_runtime_control import VALIDATOR

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

module = a.output / 'synthetic.so'
compile_command = ['cc', '-O2', '-shared', '-fPIC', '-std=c99', '-I' + str(a.source/'lib/include'),
                   str(a.source/'tests/runtime/synthetic_board.c'), '-o', str(module)]
subprocess.run(compile_command, check=True)
image = a.output/'code.bin'; image.write_bytes(bytes.fromhex('80fe') + bytes(65534))
config = a.output/'board.json'; config.write_text('{}')
endpoint = private/'control'
command = [str(a.binary), '--variant', 'sab80535', '--clock-hz', '11059200', '--seed', '0', '--entry', '0',
           '--image', str(image), '--image-sha256', sha(image), '--board', str(module),
           '--board-sha256', sha(module), '--board-config', str(config), '--control-unix', str(endpoint),
           '--execution-mode', 'externally-driven']
meta = {'argv': command, 'compile': compile_command, 'binarySha256': sha(a.binary),
        'sourceRoot': str(a.source), 'moduleSha256': sha(module), 'imageSha256': sha(image),
        'host': list(os.uname()), 'loadBefore': os.getloadavg(), 'python': sys.version,
        'clkTck': os.sysconf('SC_CLK_TCK'),
        'sourceCommit': subprocess.check_output(['git','rev-parse','HEAD'], cwd=a.source,text=True).strip(),
        'schemaSha256': sha(a.source/'interfaces/0.3.0/schemas/runtime-3.0.schema.json')}
p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
client = f = None
request_id = 0
samples = []; rows = []; advance_sample = None

def child_cpu():
    fields = Path('/proc/{}/stat'.format(p.pid)).read_text().rsplit(')', 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / meta['clkTck']

def call(name, args, validate=False):
    global request_id, advance_sample
    request_id += 1
    request = {'type':'request','id':request_id,'command':name,'arguments':args}
    client.sendall((json.dumps(request,separators=(',',':'))+'\n').encode())
    line = f.readline()
    assert line.endswith(b'\n') and len(line) <= 65536
    reply = json.loads(line)
    assert reply['id']==request_id and reply['command']==name and reply['success'],reply
    if validate: VALIDATOR.validate(reply)
    if len(samples)<16: samples.append({'request':request, 'response':reply})
    if name == 'advance' and advance_sample is None: advance_sample=reply
    return reply['body']

try:
    deadline=time.monotonic()+5
    while not endpoint.exists():
        assert p.poll() is None and time.monotonic()<deadline
        time.sleep(.005)
    client=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); client.settimeout(5)
    client.connect(str(endpoint)); f=client.makefile('rb')
    hello=call('hello',{'protocol':{'major':3,'minor':0},'contractVersion':'0.3.0','requiredCapabilities':[]})
    meta['hello']=hello
    attached=call('attach',{'sessionId':None,'role':'controller'})
    ids={k:attached[k] for k in ('sessionId','generation','leaseId')}
    identity={'sessionId':ids['sessionId']}; total=0
    for rep in range(3):
        for workload in ('advance','resume-advance-pause-status','advance-schema'):
            for quantum in (1024,4096,8000):
                if workload != 'resume-advance-pause-status': call('resume',ids)
                times=[]; cycles=0; start_id=request_id
                process=time.process_time(); child=child_cpu(); wall=time.monotonic()
                for n in range(100):
                    before=time.monotonic()
                    if workload=='resume-advance-pause-status': call('resume',ids)
                    out=call('advance',{**ids,'cycleBudget':quantum},workload=='advance-schema')
                    assert out['state']=='running' and out['generation']==ids['generation']
                    assert int(out['cycles'])==quantum and int(out['instructions'])==quantum//2
                    total+=quantum; cycles+=quantum
                    assert int(out['cycle'])==total and out['safeBoundary']
                    if workload=='resume-advance-pause-status':
                        paused=call('pause',ids); status=call('status',identity)
                        assert int(paused['cycle'])==total and int(status['cycle'])==total
                        assert status['state']=='paused'
                    times.append(time.monotonic()-before)
                wall=time.monotonic()-wall; child=child_cpu()-child; process=time.process_time()-process
                rows.append({'rep':rep,'workload':workload,'quantum':quantum,'advances':100,
                             'requests':request_id-start_id,'cycles':cycles,'instructions':cycles//2,
                             'cycle':total,'wallSeconds':wall,'clientProcessSeconds':process,
                             'runtimeProcessSecondsQuantized':child,'cyclesPerSecond':cycles/wall,
                             'medianIterationMs':statistics.median(times)*1000,'maxIterationMs':max(times)*1000})
                if workload!='resume-advance-pause-status': call('pause',ids)
    status=call('status',identity)
    assert int(status['cycle'])==total and int(status['instructions'])==total//2 and status['pc']==0
    meta['finalStatus']=status; meta['totalRequests']=request_id; meta['loadAfter']=os.getloadavg()
    call('terminate',ids); p.wait(timeout=5)
finally:
    if f:f.close()
    if client:client.close()
    if p.poll() is None:p.terminate()
    out,err=p.communicate(timeout=5)
    (a.output/'runtime.stdout').write_bytes(out); (a.output/'runtime.stderr').write_bytes(err)
    assert p.returncode==0 and not out and not err,(p.returncode,out,err)
    assert not endpoint.exists()
start=time.monotonic(); process=time.process_time()
for _ in range(1000): VALIDATOR.validate(advance_sample)
meta['offlineSchemaValidation1000']={'wallSeconds':time.monotonic()-start,'processSeconds':time.process_time()-process}
(a.output/'metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
(a.output/'samples.json').write_text(json.dumps(samples,indent=2)+'\n')
(a.output/'measurements.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps({'output':str(a.output),'runtimeCommit':hello['runtimeCommit'],'binarySha256':meta['binarySha256'],
                  'rows':len(rows),'cycles':total,'requests':meta['totalRequests'],'closed':True}))
