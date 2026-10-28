#!/usr/bin/env python3
"""Actual opt-in wire3.1 enumeration against private synthetic Unix runtimes.
RUNTIME_BIN/SDK_INCLUDE select an installed release; CONTROL31_REPORT retains
requests/responses and base-contract file content for independent provenance.
"""
import contextlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from test_runtime_control import BINARY, ROOT, Client, HELLO, digest, wire

spec = importlib.util.spec_from_file_location('control31_wire', ROOT/'interfaces/0.4.0/wire.py')
validation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validation)
sys.path.insert(0, str(ROOT/'interfaces/tools'))
import target_wire
BASE_SCHEMA = json.loads((ROOT/'interfaces/0.3.0/schemas/runtime-3.0.schema.json').read_text())
FILE_SCHEMA = json.loads((ROOT/'interfaces/0.3.0/schemas/trace-file.schema.json').read_text())
HELLO31 = dict(protocol=dict(major=3, minor=1), contractVersion='0.4.0', requiredCapabilities=['readCodeBreakpoints'])
EXCHANGES=[]
FILES=[]

class ReadClient(Client):
    def __init__(self,path,minor=1):
        super().__init__(path);self.minor=minor;self.pending=None
    def response(self):
        line=self.file.readline();assert line and line.endswith(b'\n') and len(line)<=65536,line
        value=json.loads(line)
        if self.minor:
            validation.validate(value)
            if self.pending and value['success']:validation.validate_exchange(self.pending,value)
        else:target_wire.validate(value,BASE_SCHEMA)
        EXCHANGES.append(dict(minor=self.minor,request=self.pending,response=value))
        return value
    def call(self,command,args,error=None):
        self.pending=dict(type='request',id=self.i+1,command=command,arguments=args)
        return super().call(command,args,error)
    def hello(self):return self.call('hello',HELLO31 if self.minor else HELLO)
    def read(self):
        result=self.call('readCodeBreakpoints',self.identity)
        assert result['sessionId']==self.identity['sessionId']
        return result
    def replace(self,values):return self.call('replaceCodeBreakpoints',self.args(addresses=values))

@contextlib.contextmanager
def host(code=bytes.fromhex('80fe'),config_data=b'{}',file=False):
    with tempfile.TemporaryDirectory(prefix='ctl31-',dir='/tmp') as directory:
        work=Path(directory);module,image,config,endpoint=[work/n for n in ('board.so','code.bin','config.json','control')]
        subprocess.run([os.environ.get('CC','cc'),'-shared','-fPIC','-std=c99','-I'+os.environ.get('SDK_INCLUDE',str(ROOT/'lib/include')),str(ROOT/'tests/runtime/synthetic_board.c'),'-o',str(module)],check=True)
        image.write_bytes(code+bytes(65536-len(code)));config.write_bytes(config_data)
        args=[str(BINARY),'--variant','sab80535','--clock-hz','11059200','--seed','0','--entry','0','--image',str(image),'--image-sha256',digest(image),'--board',str(module),'--board-sha256',digest(module),'--board-config',str(config),'--control-unix',str(endpoint),'--execution-mode','externally-driven']
        if file:args+=['--trace-file','output='+str(work/'trace')]
        p=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE);clients=[]
        def connect(minor=1,attach=True,role='controller'):
            c=ReadClient(endpoint,minor);clients.append(c);h=c.hello()
            if minor:
                assert h['protocol']==dict(major=3,minor=1) and h['contractVersion']=='0.4.0'
                assert h['contractManifestSha256']==digest(ROOT/'interfaces/0.4.0/manifest.json')
                assert 'readCodeBreakpoints' in h['capabilities']
            else:
                assert h['protocol']==dict(major=3,minor=0) and h['contractVersion']=='0.3.0'
                assert h['contractManifestSha256']==digest(ROOT/'interfaces/0.3.0/manifest.json')
                assert 'readCodeBreakpoints' not in h['capabilities']
            if attach:c.attach(role)
            return c
        try:
            deadline=time.monotonic()+5
            while not endpoint.exists() and p.poll() is None and time.monotonic()<deadline:time.sleep(.01)
            assert endpoint.exists() and p.poll() is None
            yield connect,endpoint,work
        finally:
            for c in clients:c.close()
            if p.poll() is None:p.terminate()
            out,err=p.communicate(timeout=5);assert p.returncode==0 and not out and not err,(p.returncode,out,err)

def states_and_ownership():
    with host() as (connect,endpoint,_):
        c=connect();o=connect(role='observer');old=connect(0,role='observer')
        assert c.read()['addresses']==o.read()['addresses']==[]
        saved=[0,17,65535];c.replace(saved[::-1]);before=c.status();detail=c.call('stopDetail',c.identity)
        assert o.read()['addresses']==saved and c.read()['addresses']==saved
        assert c.status()==before and c.call('stopDetail',c.identity)==detail
        o.call('acquireControl',{**o.identity,'generation':o.generation},'BUSY')
        o.call('replaceCodeBreakpoints',{**o.args(addresses=[]),'leaseId':c.lease},'DENIED')
        assert o.read()['addresses']==saved
        c.replace([22]);assert o.read()['addresses']==[22];c.replace(saved);assert o.read()['addresses']==saved
        new=c.call('reset',c.args(seed=0,entry=0));c.generation=new['generation']
        assert o.generation=='1' and o.read()==dict(sessionId=c.identity['sessionId'],generation='2',addresses=saved)
        assert old.status()['generation']=='2'
        c.replace([]);assert o.read()['addresses']==[]
        all_addresses=list(range(1023))+[65535];c.replace(all_addresses[::-1]);assert o.read()['addresses']==all_addresses
        c.replace([65535]);c.call('resume',c.args());before=c.status();assert before['state']=='running'
        for _ in range(3):assert o.read()['addresses']==[65535]
        assert c.status()==before
        c.call('pause',c.args());c.replace([0]);c.call('resume',c.args());p=c.call('advance',c.args(cycleBudget=10))
        assert p['reason']=='breakpoint';before=c.status();assert o.read()['addresses']==[0];assert c.status()==before
        step=c.call('step',c.args(maxCycles=10));assert step['instructions']=='1'
        o.call('readCodeBreakpoints',dict(sessionId='f'*32),'STALE')
        unattached=connect(attach=False);unattached.call('readCodeBreakpoints',c.identity,'STATE')
        c.call('releaseControl',c.args());o.generation=c.generation;o.lease=o.call('acquireControl',{**o.identity,'generation':o.generation})['leaseId']
        assert int(o.lease)>int(c.lease)
        o.replace([5]);assert c.read()['addresses']==[5]
        c.call('replaceCodeBreakpoints',c.args(addresses=saved),'DENIED') # old lease cannot restore
        old.i+=1;old.socket.sendall(wire(old.i,'readCodeBreakpoints',old.identity));assert old.file.readline()==b''

def negative_profiles():
    with host() as (connect,endpoint,_):
        c=connect()
        for extra in [dict(generation='1'),dict(leaseId='1'),dict(addresses=[True])]:
            bad=connect(role='observer');bad.call('readCodeBreakpoints',{**c.identity,**extra},'INVALID');assert bad.file.readline()==b''
        for value in [True,1,None,'A'*32]:
            bad=connect(role='observer');bad.call('readCodeBreakpoints',dict(sessionId=value),'INVALID');assert bad.file.readline()==b''
        old=ReadClient(endpoint,0)
        try:
            old.call('hello',{**HELLO,'requiredCapabilities':['readCodeBreakpoints']},'UNSUPPORTED')
        finally:old.close()
        for protocol,version in [(dict(major=3,minor=2),'0.4.0'),(dict(major=3,minor=1),'0.3.0'),(dict(major=3,minor=0),'0.4.0')]:
            wrong=ReadClient(endpoint)
            try:
                wrong.call('hello',dict(protocol=protocol,contractVersion=version,requiredCapabilities=[]),'VERSION');assert wrong.file.readline()==b''
            finally:wrong.close()
        c.call('hello',HELLO,'STATE');assert c.read()['addresses']==[] # no renegotiation
        before_hello=ReadClient(endpoint)
        try:
            before_hello.socket.sendall(wire(1,'readCodeBreakpoints',c.identity));assert before_hello.file.readline()==b''
        finally:before_hello.close()
        failed=ReadClient(endpoint)
        try:
            failed.call('hello',{**HELLO31,'requiredCapabilities':['notAvailable']},'UNSUPPORTED')
            failed.minor=0;failed.hello();failed.attach('observer')
            failed.socket.sendall(wire(failed.i+1,'readCodeBreakpoints',failed.identity));assert failed.file.readline()==b''
        finally:failed.close() # failed 3.1 hello never selected the new profile

def faulted():
    with host(bytes.fromhex('901234f080fe'),b'{"fault":true}') as (connect,_,_):
        c=connect();o=connect(role='observer');c.replace([65535]);c.call('resume',c.args());c.call('advance',c.args(cycleBudget=30));before=c.status()
        assert before['state']=='faulted' and o.read()['addresses']==[65535]
        assert c.status()==before

def trace_file_base():
    from test_runtime_trace import configuration
    with host(file=True) as (connect,_,work):
        c=connect();cfg=configuration();cfg['destinations'][0].update(kind=1,capacity=256)
        c.call('replaceTraceConfig',c.args(expectedRevision='0',config=cfg))
        opened=c.call('sinkOpen',c.args(destinationId=1,destinationToken='output',maxSegmentBytes='65536',maxSegments=2,maxTotalRecords='1000'))
        def wait(operation):
            deadline=time.monotonic()+5
            while True:
                status=c.call('sinkStatus',{**c.identity,'destinationId':1})
                assert status['error']=='OK',status
                if status['completedOperationId']==operation:return status
                assert time.monotonic()<deadline;time.sleep(.001)
        wait(opened['operationId']);c.call('resume',c.args());c.call('advance',c.args(cycleBudget=8));c.call('pause',c.args())
        closed=c.call('sinkClose',c.args(destinationId=1));status=wait(closed['operationId']);assert status['lost']=='0'
        for p in sorted(work.glob('trace.*.jsonl')):
            rows=[json.loads(line) for line in p.read_text().splitlines()]
            for row in rows:target_wire.validate(row,FILE_SCHEMA)
            assert rows[0]['contractVersion']=='0.3.0'
            assert rows[0]['contractManifestSha256']==digest(ROOT/'interfaces/0.3.0/manifest.json')
            FILES.append(dict(name=p.name,lines=rows))
        assert FILES

def main():
    states_and_ownership();negative_profiles();faulted();trace_file_base()
    if os.environ.get('CONTROL31_REPORT'):
        Path(os.environ['CONTROL31_REPORT']).write_text(json.dumps(dict(runtime=str(BINARY),exchanges=EXCHANGES,segments=FILES),indent=2)+'\n')
    print(f'Unix control3.1: {len(EXCHANGES)} actual validated responses, {sum(len(s["lines"]) for s in FILES)} base0.3 JSONL lines; enumeration states/ownership/reset/isolation and no progress passed')
if __name__=='__main__':main()
