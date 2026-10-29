#!/usr/bin/env python3
"""Deterministic authoring of target 3.0 schemas/examples; no runtime code."""
import copy
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'interfaces/0.3.0'
def obj(**props): return {'type':'object','properties':props,'required':list(props),'additionalProperties':False}
def num(lo=0,hi=4294967295): return {'type':'integer','minimum':lo,'maximum':hi}
def arr(item,maximum,minimum=0): return {'type':'array','items':item,'minItems':minimum,'maxItems':maximum}
def enum(*values): return {'enum':list(values)}
def const(value): return {'const':value}
def ref(name): return {'$ref':'#/$defs/'+name}
U64={'type':'string','pattern':'^(0|[1-9][0-9]*)$','maxLength':20,'format':'uint64'}
ID64={**U64,'pattern':'^[1-9][0-9]*$'}
BOOL={'type':'boolean'}
TOKEN={'type':'string','pattern':'^[A-Za-z0-9_.-]{1,64}$'}
SESSION={'type':'string','pattern':'^[0-9a-f]{32}$'}
HASH={'type':'string','pattern':'^[0-9a-f]{64}$'}
TEXT=lambda n:{'type':'string','maxLength':n}
STATUS=enum('OK','INVALID','VERSION','STATE','RANGE','CAPACITY','UNSUPPORTED','BUSY','STALE','DENIED','FAULT','EXHAUSTED','IO','HASH','NOT_FOUND')
ROUTES={**arr(num(1),16),'uniqueItems':True}
value=obj(value=U64,width=enum(8,1,16,32,64),known=BOOL,isSigned=BOOL)
event=obj(**{k:U64 for k in ['generation','sequence','cycle','instructionId','legacyInstructions','retiredInstructions','accessId','parentStimulusId','cpuInputOrder','frameId','relatedFrameId','contextId','parentContextId','scheduledCycle','startCycle','sourceEventSequence','beginCycle']},kind=num(1,28),inputStage=enum(0,1),phase=num(0,4),reason=num(0,10),known=num(0,63),status=STATUS,partialEffects=BOOL,space=num(0,3),access=enum(0,1,2,4,8),frameKind=num(0,3),irqSource=num(0,11),irqDepth=num(0,64),watchId=num(),firedActions=num(0,255),pc=num(0,65535),target=num(0,65535),address=num(0,65535),returnPc=num(0,65535),nextPc=num(0,65535),spBefore=num(0,255),spAfter=num(0,255),bytes=arr(num(0,255),3),uartMode=num(0,3),uartWidth=enum(0,8,9),ninth=num(0,1),scon=num(0,255),tcon=num(0,255),oldValue=value,newValue=value,text=TEXT(96))
event['properties']['generation']=ID64; event['properties']['sequence']=ID64
selector=obj(matchMask=num(0,31),kind=num(0,28),space=num(0,3),accessMask=num(0,15),pcFirst=num(0,65535),pcLast=num(0,65535),addressFirst=num(0,65535),addressLast=num(0,65535))
condition=obj(present=BOOL,operand=enum(0,1),comparison=num(0,5),width=enum(8,1,16,32,64),isSigned=BOOL,constant=U64)
action=obj(condition=condition,kind=num(0,3),traceIds=ROUTES)
watch=obj(watchId=num(1),enabled=BOOL,space=num(0,3),accessMask=num(1,15),addressFirst=num(0,65535),addressLast=num(0,65535),actions=arr(action,8,1))
point=obj(pointId=num(1),enabled=BOOL,selector=selector,traceIds=ROUTES)
gate=obj(gateId=num(1),enabled=BOOL,selector=selector,action=enum(0,1),timing=enum(0,1),traceIds=ROUTES)
trace=obj(traceId=num(1),destinationId=num(1),enabled=BOOL,interruptPolicy=num(0,2),tag=TEXT(64),comment=TEXT(256))
dest=obj(destinationId=num(1),kind=enum(0,1),capacity=num(1,4096),stopOnFull=BOOL)
config=obj(traces=arr(trace,16),destinations=arr(dest,8),points=arr(point,32),gates=arr(gate,32),watches=arr(watch,64))
recordbase=dict(epoch=ID64,recordSequence=ID64,generation=ID64,destinationId=num(),traceIds=ROUTES)
record={'oneOf':[obj(**recordbase,kind=const(0),event=ref('observation')),obj(**recordbase,kind=const(1),traceId=num(1),interruptPolicy=num(0,2),firstSourceSequence=ID64,lastSourceSequence=ID64,count=ID64,entryDepth=num(0,64),maximumDepth=num(0,64)),obj(**recordbase,kind=const(2),firstSourceSequence=U64,lastSourceSequence=U64,count=ID64),obj(**recordbase,kind=const(3),status=STATUS)]}
cursor=obj(sessionId=SESSION,destinationId=num(),epoch=ID64,afterRecordSequence=U64,maxRecords=num(0,16))
page=obj(sessionId=SESSION,destinationId=num(),epoch=ID64,cursorState=enum('current','expired','ahead','wrong-session','wrong-epoch'),requestedAfter=U64,oldest=U64,newest=U64,first=U64,last=U64,nextAfter=U64,overwritten=U64,dropped=U64,lostBeforePage=U64,remaining=num(0,4096),more=BOOL,records=arr(ref('record'),16))
capture=obj(captureId=ID64,successorCaptureId=U64,rearmStatus=STATUS,invocationId=U64,contextId=U64,generation=ID64,startSequence=U64,endSequence=U64,records=U64,cycles=U64,traceId=num(1),mode=num(0,2),state=num(0,4),reason=num(0,8),rearmOnReset=BOOL)
sink=obj(operationId=ID64,completedOperationId=U64,bytes=U64,records=U64,lost=U64,destinationId=num(1),state=num(0,4),segment=num(0,15),queued=num(0,256),partialOutput=BOOL,error=STATUS)
defs={'observation':event,'record':record,'config':config,'page':page,'capture':capture,'sink':sink}
base=json.loads((ROOT/'interfaces/0.2.0/schemas/runtime-2.1.schema.json').read_text())
# Inline baseline objects are immutable in the old bundle; copy into new major.
branches=[copy.deepcopy(x) for x in base['oneOf'][:-1] if x['properties']['command']['const']!='tracePage']
commands={x['properties']['command']['const'] for x in branches}
mutations={'pause','resume','step','reset','replaceCodeBreakpoints','boardInput','terminate'}
CAPS=['attachSession','status','control','codeBreakpoints','decodeCode','memoryRead','boardInput','boardInspect','boardInspectFault','advance','observations','uartOutcomes','traceConfig','tracePage','captureA','captureB','traceFile']
for b in branches:
    p=b['properties']; name=p['command']['const']; body=p.get('arguments',p.get('body'))
    if name=='hello':
        body['properties']['protocol']=obj(major=const(3),minor=const(0)); body['properties']['contractVersion']=const('0.3.0')
        capkey='requiredCapabilities' if 'arguments' in p else 'capabilities'
        body['properties'][capkey]={**arr(TOKEN if capkey=='requiredCapabilities' else enum(*CAPS),32 if capkey=='requiredCapabilities' else len(CAPS)),'uniqueItems':True}
        if capkey=='capabilities':
            body['properties']['executionMode']=enum('externally-driven','host-driven'); body['required'].append('executionMode')
            body['properties']['observationCoverage']=arr(enum('instruction','access','control','irq','lifecycle','input','uart'),7); body['required'].append('observationCoverage')
            limits=body['properties']['limits']; additions={'maxPageRecords':16,'maxTraces':16,'maxDestinations':8,'maxPoints':32,'maxGates':32,'maxWatches':64,'maxActions':8,'maxRoutes':16,'maxContextDepth':64,'maxRingRecords':4096,'outcomeRecords':1024,'maxRecordBytes':3072,'fileQueueRecords':256,'maxFileSegments':16,'frameTimeoutMs':5000,'maxJsonDepth':16,'maxJsonValues':8192}
            limits['properties'].update({k:const(v) for k,v in additions.items()}); limits['required']=list(limits['properties'])
    if 'arguments' in p and name in mutations:
        body['properties']['leaseId']=ID64; body['required'].append('leaseId')
    if 'body' in p and name in ('attach','status'):
        body['properties']['leaseId']=U64; body['required'].append('leaseId')
    if 'body' in p and name=='boardInput':
        body['properties']['parentStimulusId']=ID64; body['required'].append('parentStimulusId')
    # Zero generation was accepted in 2.1 schema but is invalid for live sessions.
    def generations(x):
        if isinstance(x,dict):
            if 'generation' in x.get('properties',{}): x['properties']['generation']=ID64
            for v in x.values():generations(v)
        elif isinstance(x,list):
            for v in x:generations(v)
    generations(body)
SID=dict(sessionId=SESSION)
MUT=dict(sessionId=SESSION,generation=ID64,leaseId=ID64)
def command(name,args,result):
    commands.add(name)
    branches.append(obj(type=const('request'),id=num(1),command=const(name),arguments=args))
    branches.append(obj(type=const('response'),id=num(1),command=const(name),success=const(True),body=result))
progress=obj(generation=ID64,cycles=U64,instructions=U64,retiredInstructions=U64,cycle=U64,stopRevision=U64,state=enum('paused','running','faulted','terminated'),safeBoundary=BOOL,reason=enum('budget','pause','breakpoint','watchpoint','fault','power-down-unsupported','terminated'),partial=BOOL)
command('advance',obj(**MUT,cycleBudget=num(1,100000)),progress)
command('stopDetail',obj(**SID),obj(stopRevision=U64,generation=ID64,reason=enum('entry','step','budget','pause','breakpoint','watchpoint','fault','power-down-unsupported','terminated'),sourceSequence=U64,instructionId=U64,matches=arr(obj(watchId=num(1),sequence=ID64,sourceSequence=ID64),64)))
command('releaseControl',obj(**MUT),obj(leaseId=const('0')))
command('acquireControl',obj(**SID,generation=ID64),obj(leaseId=ID64))
command('replaceTraceConfig',obj(**MUT,expectedRevision=U64,config=ref('config')),obj(revision=ID64))
command('traceConfig',obj(**SID),obj(revision=U64,config=ref('config')))
command('tracePage',cursor,ref('page'))
command('outcomePage',obj(**SID,epoch=ID64,afterRecordSequence=U64,maxRecords=num(0,16)),ref('page'))
command('clearTrace',obj(**MUT,destinationId=num(1)),obj(epoch=ID64))
command('captureStart',obj(**MUT,traceId=num(1),mode=num(0,2),contextId=U64,entryPc=num(0,65535),maxRecords=ID64,maxCycles=ID64,rearmOnReset=BOOL,stopCpu=BOOL),ref('capture'))
command('captureStatus',obj(**SID,captureId=ID64),ref('capture'))
command('captureStop',obj(**MUT,captureId=ID64),ref('capture'))
command('sinkOpen',obj(**MUT,destinationId=num(1),destinationToken=TOKEN,maxSegmentBytes={**ID64,'format':'segment-bytes'},maxSegments=num(1,16),maxTotalRecords=ID64),ref('sink'))
for name in ('sinkFlush','sinkClose'):command(name,obj(**MUT,destinationId=num(1)),ref('sink'))
command('sinkStatus',obj(**SID,destinationId=num(1)),ref('sink'))
branches.append(obj(type=const('response'),id=num(1),command=enum(*sorted(commands)),success=const(False),error=obj(code={**STATUS,'enum':STATUS['enum'][1:]},message=TEXT(256),retryable=BOOL)))
schema={'$schema':'https://json-schema.org/draft/2020-12/schema','$id':'urn:emusa80535:runtime:3.0','title':'Target control 3.0 (not implemented)','$defs':defs,'oneOf':branches}
def write(path,data): (OUT/path).write_text(json.dumps(data,indent=2)+'\n')
write('schemas/runtime-3.0.schema.json',schema)
write('schemas/observation.schema.json',{'$schema':schema['$schema'],'$id':'urn:emusa80535:observation:1',**event})
filedefs=copy.deepcopy(defs)
header=obj(type=const('header'),format=const('emusa80535-trace-jsonl-1'),contractVersion=const('0.3.0'),protocol=obj(major=const(3),minor=const(0)),runtimeCommit={'type':'string','pattern':'^[0-9a-f]{40}$'},contractManifestSha256=HASH,sessionId=SESSION,imageSha256=HASH,moduleSha256=HASH,configSha256=HASH,traceConfigSha256=HASH,generation=ID64,segment=num(0,15),destinationId=num(1),configuration=ref('config'))
footer=obj(type=const('footer'),destinationId=num(1),segment=num(0,15),lastRecordSequence=U64,records=U64,lost=U64,complete=BOOL,status=STATUS)
fileschema={'$schema':schema['$schema'],'$id':'urn:emusa80535:trace-file:1','$defs':filedefs,'oneOf':[header,obj(type=const('record'),record=ref('record')),footer]}
write('schemas/trace-file.schema.json',fileschema)
def example(s):
    if '$ref' in s:return example(defs[s['$ref'].split('/')[-1]])
    if 'const' in s:return s['const']
    if 'enum' in s:return s['enum'][0]
    if 'oneOf' in s:return example(s['oneOf'][0])
    t=s.get('type')
    if t=='object':return {k:example(v) for k,v in s.get('properties',{}).items() if k in s.get('required',[])}
    if t=='array':return [example(s['items']) for _ in range(s.get('minItems',0))]
    if t=='integer':return s.get('minimum',0)
    if t=='boolean':return False
    if t=='null':return None
    if s.get('format')=='segment-bytes':return '65536'
    if s.get('format')=='uint64':return '1' if s.get('pattern')=='^[1-9][0-9]*$' else '0'
    p=s.get('pattern','')
    if '[0-9a-f]{' in p:return 'a'*int(p.split('{')[1].split('}')[0])
    if p:return 'example'
    return ''
examples=[]
for i,b in enumerate(branches):
    e=example(b);e['id']=i//2+1
    if e['command']=='hello' and e['type']=='response':e['body']['capabilities']=CAPS;e['body']['observationCoverage']=['instruction','access','control','irq','lifecycle','input','uart']
    if e['command']=='attach' and e['type']=='response':e['body']['leaseId']='0'
    examples.append(e)
(OUT/'examples/runtime-3.0.ndjson').write_text(''.join(json.dumps(e,separators=(',',':'))+'\n' for e in examples))
events=[]
for kind in range(1,29):
    e=example(event);e.update(kind=kind,generation='1',sequence=str(kind),cycle='10',instructionId='1',phase=1)
    if kind in (1,2,3): e.update(phase={1:0,2:2,3:4}[kind],bytes=[0],known=1,pc=0,text='NOP')
    if kind in (4,5):e.update(accessId='1',access=2)
    if kind in (6,7,9,10): e.update(contextId='1',frameKind=1 if kind in (6,7) else 2,known=7)
    if 14<=kind<=21:e.update(parentStimulusId='1',cpuInputOrder='1',inputStage=1,instructionId='0',phase=4 if kind in (14,17) else 3)
    if 18<=kind<=27:e.update(frameId='1',uartWidth=8,known=8,phase=3)
    if kind==23:e.update(relatedFrameId='2',reason=5)
    if kind in (22,23):e.update(phase=1,accessId='1')
    if kind in (11,12,13,28):e['phase']=4
    if kind==8:e['phase']=3
    if kind==9:e['phase']=4
    if kind==13:e.update(status='FAULT',reason=8)
    if kind==28:e.update(watchId=1,sourceEventSequence='1',firedActions=1)
    if kind==3:e.update(status='FAULT',reason=7)
    if kind==16:e.update(status='FAULT',reason=8)
    if kind==17:e.update(reason=6)
    if kind==20:e.update(reason=3)
    events.append(e)
(OUT/'examples/observations.ndjson').write_text(''.join(json.dumps(e,separators=(',',':'))+'\n' for e in events))
fileexamples=[example(header)]+[{'type':'record','record':dict(example(v),recordSequence=str(i+1))} for i,v in enumerate(record['oneOf'])]+[example(footer)]
fileexamples[1]['record']['event']=events[0]
(OUT/'examples/trace-file.ndjson').write_text(''.join(json.dumps(e,separators=(',',':'))+'\n' for e in fileexamples))
if __name__=='__main__':print('generated target schemas and illustrative fixtures')
