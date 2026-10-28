#!/usr/bin/env python3
"""Actual Unix trace/capture/watch execution with frozen schema+semantic validation.
RUNTIME_BIN and SDK_INCLUDE support independent installed-artifact verification.
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
from test_runtime_control import BINARY, ROOT, SCHEMA, Client
sys.path.insert(0, str(ROOT / 'interfaces/tools'))
import target_wire
RESPONSES=[]
class TraceClient(Client):
    def response(self):
        r=super().response();target_wire.validate(r,SCHEMA);RESPONSES.append(r);return r
    def config(self, config, error=None, revision=None):
        if revision is None: revision=self.call('traceConfig',self.identity)['revision']
        return self.call('replaceTraceConfig',self.args(expectedRevision=revision,config=config),error)
    def start(self, trace=1, records=10000, cycles=10000, stop=False):
        return self.call('captureStart',self.args(traceId=trace,mode=0,contextId='0',entryPc=0,maxRecords=str(records),maxCycles=str(cycles),rearmOnReset=False,stopCpu=stop))
    def capture(self, cap): return self.call('captureStatus',{**self.identity,'captureId':cap['captureId']})
    def page(self, after='0', epoch='1', **extra):
        return self.call('tracePage',{**self.identity,'destinationId':1,'epoch':epoch,'afterRecordSequence':after,'maxRecords':16,**extra})
    def records(self,epoch='1'):
        out=[];after='0'
        while True:
            p=self.page(after,epoch);out+=p['records'];after=p['nextAfter']
            if not p['more']:return out
    def advance(self,n):return self.call('advance',self.args(cycleBudget=n))
    def run(self,n):
        if self.status()['state']=='paused':self.call('resume',self.args())
        return self.advance(n)
    def pause(self):
        if self.status()['state']=='running':self.call('pause',self.args())
    def reset(self):
        self.pause();r=self.call('reset',self.args(seed=0,entry=0));self.generation=r['generation'];return r

def selector(**extra):return dict(matchMask=0,kind=0,space=0,accessMask=0,pcFirst=0,pcLast=0,addressFirst=0,addressLast=0,**extra)
def configuration(traces=1,capacity=4096,stop=False):
    return {'traces':[dict(traceId=i,destinationId=1,enabled=True,interruptPolicy=0,tag='trace '+str(i),comment='persistent å') for i in range(1,traces+1)],'destinations':[dict(destinationId=1,kind=0,capacity=capacity,stopOnFull=stop)],'points':[dict(pointId=1,enabled=True,selector=selector(),traceIds=list(range(1,traces+1)))],'gates':[],'watches':[]}
def condition(**extra):
    v=dict(present=False,operand=0,comparison=0,width=8,isSigned=False,constant='0');v.update(extra);return v
def watch(i,condition_value=None):return dict(watchId=i,enabled=True,space=3,addressFirst=0x1234,addressLast=0x1234,accessMask=4,actions=[dict(kind=0,traceIds=[],condition=condition_value or condition()),dict(kind=3,traceIds=[1],condition=condition())])
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
@contextlib.contextmanager
def host(program=None,config_data=b'{}'):
    with tempfile.TemporaryDirectory(prefix='trace3-',dir='/tmp') as temp:
        work=Path(temp);module,image,config,endpoint=[work/n for n in ['board.so','code.bin','config.json','control']]
        subprocess.run([os.environ.get('CC','cc'),'-shared','-fPIC','-std=c99','-I'+os.environ.get('SDK_INCLUDE',str(ROOT/'lib/include')),str(ROOT/'tests/runtime/synthetic_board.c'),'-o',str(module)],check=True)
        if program is None:
            program=bytearray(34);program[:5]=bytes.fromhex('12001080fb');program[16:26]=bytes.fromhex('9012347442f012002022');program[32:34]=bytes.fromhex('0022')
        image.write_bytes(program+bytes(65536-len(program)));config.write_bytes(config_data)
        args=[str(BINARY),'--variant','sab80535','--clock-hz','11059200','--seed','0','--entry','0','--image',str(image),'--image-sha256',digest(image),'--board',str(module),'--board-sha256',digest(module),'--board-config',str(config),'--control-unix',str(endpoint),'--execution-mode','externally-driven','--board-inspect-fault']
        proc=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE);client=None
        try:
            deadline=time.monotonic()+5
            while not endpoint.exists() and proc.poll() is None and time.monotonic()<deadline:time.sleep(.01)
            assert proc.poll() is None and endpoint.exists()
            client=TraceClient(endpoint);client.endpoint=endpoint;hello=client.hello();assert {'traceConfig','tracePage','captureA'}<=set(hello['capabilities']);assert not {'captureB','traceFile'}&set(hello['capabilities']);client.attach();yield client
        finally:
            if client:client.close()
            if proc.poll() is None:proc.terminate()
            out,err=proc.communicate(timeout=5);assert proc.returncode==0 and not out,(proc.returncode,out,err)

def persistent():
    with host() as c:
        cfg=configuration(2);c.config(cfg);assert c.call('traceConfig',c.identity)['config']==cfg
        a=c.start(records=3);b=c.start(trace=2);c.run(50);c.pause()
        assert c.capture(a)['records']=='3' and c.capture(a)['state']==2
        records=c.records();events=[r for r in records if r['kind']==0]
        assert len(events)>3 and all(r['traceIds']==([1,2] if i<3 else [2]) for i,r in enumerate(events))
        assert {6,7} <= {r['event']['kind'] for r in events},events # CALL and RET remain in A after each call
        assert len({r['event']['sequence'] for r in events})==len(events)
        assert all(r['event']['text'] for r in events if r['event']['kind']==2)
        before=c.capture(b);c.reset();c.run(40);c.pause();after=c.capture(b)
        assert int(after['cycles'])>=int(before['cycles'])+40 and after['generation']=='2' and after['state']==1
        c.call('captureStop',c.args(captureId=b['captureId']));count=len(c.records());c.run(20);c.pause();assert len(c.records())==count
        c.call('captureStart',c.args(traceId=1,mode=1,contextId='0',entryPc=16,maxRecords='100',maxCycles='100',rearmOnReset=False,stopCpu=False),'UNSUPPORTED')
        c.config(cfg,revision='0',error='STALE')
        bad=json.loads(json.dumps(cfg));bad['points'][0]['selector']['matchMask']=8;bad['points'][0]['selector']['space']=1;c.config(bad,error='UNSUPPORTED')
        wrong=c.page(sessionId='f'*32);assert wrong['cursorState']=='wrong-session' and not wrong['records']
        assert c.call('clearTrace',c.args(destinationId=1))['epoch']=='2';assert c.page(epoch='1')['cursorState']=='wrong-epoch';assert c.records(epoch='2')==[]
        observer=TraceClient(c.endpoint);observer.hello();observer.attach('observer')
        observer.call('clearTrace',{**observer.args(),'leaseId':c.lease,'destinationId':1},'DENIED');observer.close()

def safe_stops():
    with host() as c:
        cfg=configuration();cfg['watches']=[watch(1,condition(present=True,operand=1,constant='66')),watch(2)];c.config(cfg)
        c.call('replaceCodeBreakpoints',c.args(addresses=[22]))
        progress=c.run(100);assert progress['reason']=='breakpoint' and progress['state']=='paused',progress
        detail=c.call('stopDetail',c.identity);assert detail['reason']=='breakpoint' and [m['watchId'] for m in detail['matches']]==[1,2],detail
        records=c.records();events=[r['event'] for r in records if r['kind']==0];source=[e for e in events if e['sequence']==detail['sourceSequence']];assert len(source)==1 and source[0]['kind']==5
        matches=[e for e in events if e['kind']==28];assert len(matches)==2 and [e['watchId'] for e in matches]==[1,2]
        assert all(m['sourceSequence']==detail['sourceSequence'] for m in detail['matches'])
    with host() as c:
        c.config(configuration(capacity=2,stop=True));r=c.run(100);assert r['state']=='paused' and r['reason']=='pause'
        p=c.page();assert len(p['records'])==2 and int(p['dropped'])>0
    with host(bytes.fromhex('75870180fe')) as c:
        c.config(configuration());cap=c.start(cycles=7,stop=True);r=c.run(100)
        assert r['state']=='paused' and r['reason']=='budget' and r['cycle']=='7',r
        status=c.capture(cap);assert status['cycles']=='7' and status['state']==2
        detail=c.call('stopDetail',c.identity);assert detail['sourceSequence']==detail['instructionId']=='0'
    with host(bytes.fromhex('0080fd')) as c:
        c.config(configuration());cap=c.start(cycles=1,stop=True);r=c.run(50);assert r['state']=='paused'
        records=c.records();assert all(int(r['event']['cycle'])<1 for r in records if r['kind']==0)
        detail=c.call('stopDetail',c.identity);assert detail['sourceSequence']!='0'

def failure():
    with host(bytes.fromhex('9012347442f080fe'),b'{"fault":true}') as c:
        cfg=configuration();cfg['watches']=[watch(1)];c.config(cfg);r=c.run(100)
        assert r['state']=='faulted',r
        events=[r['event'] for r in c.records() if r['kind']==0]
        assert any(e['kind']==3 for e in events) and not any(e['kind']==28 for e in events)
        abort=next(e for e in events if e['kind']==3)
        assert not any(e['kind']==2 and e['instructionId']==abort['instructionId'] for e in events)

def main():
    persistent();safe_stops();failure()
    if os.environ.get('TRACE_REPORT'):Path(os.environ['TRACE_REPORT']).write_text(json.dumps({'runtime':str(BINARY),'responses':RESPONSES},indent=2)+'\n')
    print(f'Unix persistent trace: {len(RESPONSES)} schema+semantic responses; coalescing, nested calls, watches, limits, reset, ownership and stable pages passed')
if __name__=='__main__':main()
