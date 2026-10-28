#!/usr/bin/env python3
"""Actual local Unix UART outcomes, also usable against an installed runtime.
RUNTIME_BIN selects executable, SDK_INCLUDE selects independent fixture headers,
OUTCOME_REPORT optionally retains all schema-validated wire responses.
"""
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

responses = []
class ValidatedClient(Client):
    def response(self):
        value = super().response()
        target_wire.validate(value, SCHEMA)
        responses.append(value)
        return value

def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    with tempfile.TemporaryDirectory(prefix='outcome3-', dir='/tmp') as directory:
        work = Path(directory)
        module, image, config, endpoint = [work / x for x in ('board.so','code.bin','config.json','control')]
        subprocess.run([os.environ.get('CC','cc'), '-std=c99','-shared','-fPIC','-I'+os.environ.get('SDK_INCLUDE',str(ROOT/'lib/include')),str(ROOT/'tests/runtime/synthetic_board.c'),'-o',str(module)],check=True)
        program=bytes.fromhex('758920758dff758bff7598d875878075884075995580fe')
        image.write_bytes(program+bytes(65536-len(program)));config.write_bytes(b'{}')
        args=[str(BINARY),'--variant','sab80535','--clock-hz','11059200','--seed','0','--entry','0','--image',str(image),'--image-sha256',digest(image),'--board',str(module),'--board-sha256',digest(module),'--board-config',str(config),'--control-unix',str(endpoint),'--execution-mode','externally-driven']
        proc=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        try:
            deadline=time.monotonic()+5
            while not endpoint.exists() and proc.poll() is None and time.monotonic()<deadline:time.sleep(.01)
            assert endpoint.exists() and proc.poll() is None
            client=ValidatedClient(endpoint);hello=client.hello()
            assert {'observations','uartOutcomes'} <= set(hello['capabilities'])
            assert not {'captureB','traceFile'} & set(hello['capabilities'])
            client.attach()
            admission=client.call('boardInput',client.args(cycle='20',schema='synthetic.uart.v1',payload={}))
            client.call('resume',client.args());progress=client.call('advance',client.args(cycleBudget=1000))
            assert progress['state']=='running'
            page=client.call('outcomePage',{**client.identity,'epoch':'1','afterRecordSequence':'0','maxRecords':16})
            events=[r['event'] for r in page['records'] if r['kind']==0]
            bykind={k:[e for e in events if e['kind']==k] for k in range(14,28)}
            assert len(bykind[14])==2 and len(bykind[15])==2
            assert all(len(bykind[k])==1 for k in (18,19,21,22,24,25,26,27)), events
            assert not bykind[16] and not bykind[20] and not bykind[23]
            rx=bykind[18][0];accepted=bykind[19][0];end=bykind[21][0]
            assert rx['frameId']==accepted['frameId']==end['frameId']
            assert rx['parentStimulusId']==admission['parentStimulusId'] and rx['cpuInputOrder']!='0'
            assert accepted['newValue']['value']=='341' and accepted['ninth']==1
            assert rx['cycle']=='21' and int(end['cycle'])>21
            assert bykind[24][0]['frameId']==bykind[27][0]['frameId']
            assert int(bykind[26][0]['cycle'])<int(bykind[27][0]['cycle'])
            # Independent cursor readers and reset retain the old operational history.
            same=client.call('outcomePage',{**client.identity,'epoch':'1','afterRecordSequence':'0','maxRecords':16});assert same==page
            client.call('pause',client.args())
            queued=client.call('boardInput',client.args(cycle='2000',schema='synthetic.uart.v1',payload={}))
            reset=client.call('reset',client.args(seed=0,entry=0));client.generation=reset['generation']
            page2=client.call('outcomePage',{**client.identity,'epoch':'1','afterRecordSequence':page['nextAfter'],'maxRecords':16})
            assert page2['epoch']=='1' and page2['oldest']==page['oldest']
            assert any(r['event']['kind']==17 and r['event']['parentStimulusId']==queued['parentStimulusId'] and r['generation']=='1' for r in page2['records'])
            client.close()
        finally:
            if proc.poll() is None:proc.terminate()
            out,err=proc.communicate(timeout=5)
            assert proc.returncode==0 and not out,(proc.returncode,out,err)
        if os.environ.get('OUTCOME_REPORT'):
            Path(os.environ['OUTCOME_REPORT']).write_text(json.dumps({'runtime':str(BINARY),'responses':responses},indent=2)+'\n')
        print(f'Unix UART outcomes: {len(responses)} actual responses passed frozen schema+semantics, RX/TX correlation and reset retention')
if __name__=='__main__':main()
