#!/usr/bin/env python3
"""Offline contract3 fixture validation, never an emulator/server implementation."""
import json
import re
import jsonschema
import bundle


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate(value, schema):
    checker = jsonschema.FormatChecker()
    @checker.checks('uint64')
    def uint64(v):
        return isinstance(v,str) and re.fullmatch(r'0|[1-9][0-9]*',v) is not None and int(v)<=2**64-1
    @checker.checks('segment-bytes')
    def segment(v):return uint64(v) and 65536<=int(v)<=1073741824
    encoded=json.dumps(value,separators=(',',':'),ensure_ascii=False,allow_nan=False).encode()
    require(len(encoded)+1<=65536,'frame limit')
    count=0
    def walk(v,depth=1,key='',opaque=False):
        nonlocal count
        count+=1
        require(depth<=16 and count<=8192,'structure limit')
        if isinstance(v,dict):
            require(len(v)<=256,'member limit')
            for k,x in v.items():walk(x,depth+1,k,opaque or k=='payload')
        elif isinstance(v,list):
            require(len(v)<=1024,'array limit')
            for x in v:walk(x,depth+1,opaque=opaque)
        elif isinstance(v,str):
            if not opaque and key in ('text','tag','comment','message','destinationToken','sessionId','moduleId','moduleVersion'):
                require('\0' not in v,'NUL C-string')
            limit={'text':96,'tag':64,'comment':256,'message':256,'destinationToken':64}.get(key)
            if limit and not opaque:require(len(v.encode())<=limit,'UTF-8 byte limit')
        if key=='payload':require(len(json.dumps(v,separators=(',',':'),ensure_ascii=False).encode())<=4096,'board payload bytes')
    walk(value)
    jsonschema.Draft202012Validator(schema,format_checker=checker).validate(value)
    if value.get('type')=='response' and value.get('success') is False:
        require(value['error']['retryable']==(value['error']['code']=='BUSY'),'retryability')
    body=value.get('body',value.get('arguments',{}))
    caps=body.get('capabilities',[])
    if value.get('command')=='hello' and value.get('success') is True:
        require(all(x in caps for x in ('attachSession','status','control','codeBreakpoints','decodeCode')),'mandatory capabilities')
    dependencies={'boardInspectFault':['boardInspect'],'traceConfig':['observations'],'tracePage':['observations'],'captureA':['observations'],'captureB':['captureA'],'uartOutcomes':['observations'],'traceFile':['traceConfig','tracePage']}
    for cap,needed in dependencies.items():
        if cap in caps: require(all(x in caps for x in needed),'capability dependency')
    coverage=body.get('observationCoverage',[])
    if 'captureB' in caps:require(all(x in coverage for x in ('instruction','control','irq')),'capture coverage')
    if 'uartOutcomes' in caps:require(all(x in coverage for x in ('input','uart','lifecycle')),'outcome coverage')
    if 'advance' in caps:require(body.get('executionMode')=='externally-driven','advance mode')
    if 'config' in body:validate_config(body['config'])
    if value.get('command')=='captureStart' and value.get('type')=='request':
        mode=body['mode'];require(not body['rearmOnReset'] or mode==1,'rearm scope')
        require((mode==2)==(int(body['contextId'])>0),'capture context')
        if mode!=1:require(body['entryPc']==0,'capture entry')
    if value.get('command') in ('tracePage','outcomePage') and value.get('success') is True:validate_page(body)
    if 'kind' in value and 'instructionId' in value:validate_event(value)
    if value.get('type')=='record':validate_record(value['record'])
    if value.get('type')=='header':validate_config(value['configuration'])


def validate_frame(data,schema):
    require(data.endswith(b'\n') and len(data)<=65536,'frame boundary')
    require(b'\r' not in data and b'\n' not in data[:-1] and not data.startswith(b'\xef\xbb\xbf'),'framing')
    value=bundle.load_json(data[:-1].decode('utf-8'))
    validate(value,schema)
    return value


def validate_event(e):
    k=e['kind'];n=lambda f:int(e[f])
    if k in (1,2,6,7,8,9,10,11,12,14,15,18,19,20,22,23,24,25,26):require(e['status']=='OK','successful event status')
    if not e['known']&1:require(e['pc']==0,'unknown pc')
    if not e['known']&2:require(e['target']==0 and e['returnPc']==0,'unknown target')
    if not e['known']&4:require(e['spBefore']==0 and e['spAfter']==0,'unknown stack')
    if not e['known']&8:require(all(e[x]==0 for x in ('uartMode','uartWidth','ninth','scon','tcon')),'unknown UART state')
    if not e['known']&16:require(e['scheduledCycle']=='0' and e['startCycle']=='0','unknown timing')
    if not e['known']&32:require(e['nextPc']==0,'unknown nextPC')
    for field in ('oldValue','newValue'):
        v=e[field];require(int(v['value']) < (1<<v['width']),'value width')
        if not v['known']:require(v['value']=='0' and v['width']==8 and not v['isSigned'],'unknown value')
    if k in (1,2,3,4,5,6,7,10):require(n('instructionId')>0,'instruction identity')
    if k in (4,5):require(n('accessId')>0 and e['access'] in (1,2,4,8),'access correlation')
    if 14<=k<=17:
        require(e['inputStage']==1 or n('parentStimulusId')>0,'board parent stimulus')
        require((n('cpuInputOrder')>0)==(e['inputStage']==1),'input stage/order')
    if 18<=k<=21:require(n('cpuInputOrder')>0 and e['inputStage']==1,'RX input correlation')
    if 18<=k<=27:require(n('frameId')>0,'UART frame identity')
    if k==23:require(n('relatedFrameId')>0 and n('relatedFrameId')!=n('frameId') and e['reason']==5,'replacement identity')
    if k in (6,7,9,10):require(n('contextId')>0 and e['frameKind']==(1 if k in (6,7) else 2),'typed context')
    if k in (3,13,16):require(e['status']!='OK','failure status')
    if k==20:require(e['reason'] in (1,2,3,4),'RX drop reason')
    phases={1:0,2:2,3:4,4:1,5:1,6:1,7:1,8:3,9:4,10:1,11:4,12:4,13:4,14:4,15:3,16:3,17:4,18:3,19:3,20:3,21:3,22:1,23:1,24:3,25:3,26:3,27:3,28:4}
    require(e['phase']==phases[k],'event phase')
    if k==28:require(e['watchId']>0 and n('sourceEventSequence')>0 and e['firedActions']>0,'watch correlation')
    require(n('beginCycle')<=n('cycle'),'instruction cycle range')


def validate_config(c):
    require(len(json.dumps(c,separators=(',',':'),ensure_ascii=False).encode())<=16384,'configuration bytes')
    ids={}
    for category,key in [('traces','traceId'),('destinations','destinationId'),('points','pointId'),('gates','gateId'),('watches','watchId')]:
        values=[x[key] for x in c[category]];require(values==sorted(set(values)),'ID order/duplicate');ids[category]=set(values)
    for t in c['traces']:require(t['destinationId'] in ids['destinations'],'destination reference')
    for d in c['destinations']:
        require(d['capacity']>=2,'ring minimum')
        if d['kind']==1:require(d['capacity']==256 and not d['stopOnFull'],'file queue policy')
    def routes(r):require(r==sorted(set(r)) and set(r)<=ids['traces'],'route references/order')
    for cat in ('points','gates'):
        for p in c[cat]:
            routes(p['traceIds']);s=p['selector'];mask=s['matchMask']
            for bit,keys in [(1,['kind']),(2,['pcFirst','pcLast']),(4,['addressFirst','addressLast']),(8,['space']),(16,['accessMask'])]:
                if not mask&bit:require(all(s[x]==0 for x in keys),'unused selector')
            if mask&1:require(s['kind']>0,'selected kind')
            require(s['pcFirst']<=s['pcLast'] and s['addressFirst']<=s['addressLast'],'selector range')
    for w in c['watches']:
        require(w['addressFirst']<=w['addressLast'],'watch range')
        for a in w['actions']:
            routes(a['traceIds'])
            if a['kind']!=3:require(not a['traceIds'],'non-route action')
            cond=a['condition'];require(int(cond['constant'])<(1<<cond['width']),'condition width')
            if not cond['present']:require(cond==dict(present=False,operand=0,comparison=0,width=8,isSigned=False,constant='0'),'absent condition')


def validate_page(p):
    r=p['records'];seq=[int(x['recordSequence']) for x in r]
    oldest,newest,after=int(p['oldest']),int(p['newest']),int(p['requestedAfter'])
    require(0<=oldest<=newest and ((oldest==0)==(newest==0)),'retained range')
    if p['cursorState']=='current':require(after<=newest and p['lostBeforePage']=='0','current cursor classification')
    if p['cursorState']=='ahead':require(after>newest,'ahead cursor classification')
    if p['cursorState']=='expired':require(oldest>0 and after<oldest-1,'expired cursor classification')
    require(seq==sorted(set(seq)),'page sequence order')
    require(all(x['epoch']==p['epoch'] and x['destinationId']==p['destinationId'] for x in r),'page identity')
    require(int(p['first'])==(seq[0] if seq else 0) and int(p['last'])==(seq[-1] if seq else 0),'page endpoints')
    require(p['nextAfter']==(str(seq[-1]) if seq else p['requestedAfter']),'page continuation')
    require(p['more']==(p['remaining']>0),'page remaining')
    if p['cursorState'] in ('ahead','wrong-session','wrong-epoch'):require(not r and p['remaining']==0 and p['lostBeforePage']=='0','invalid cursor data')
    if p['cursorState']=='expired':require(int(p['lostBeforePage'])==int(p['oldest'])-int(p['requestedAfter'])-1,'expired cursor loss')
    require(all(int(p['requestedAfter'])<s<=int(p['newest']) and s>=int(p['oldest']) for s in seq),'cursor ordering')
    if p['cursorState']=='current':require(not int(p['oldest']) or int(p['requestedAfter'])>=int(p['oldest'])-1,'expired cursor mislabeled')
    for x in r:validate_record(x)

def validate_record(x):
    require(len(json.dumps(x,separators=(',',':'),ensure_ascii=False).encode())<=3072,'record bytes')
    require(x['traceIds']==sorted(set(x['traceIds'])),'record route order')
    if x['kind']==0:
        validate_event(x['event'])
        require(x['generation']==x['event']['generation'],'record generation')
    if x['kind'] in (1,2):
        require(int(x['firstSourceSequence'])<=int(x['lastSourceSequence']),'source range')
        if x['kind']==1:require(x['entryDepth']<=x['maximumDepth'],'suppression depth')


def validate_exchange(request,response,context):
    """Check fixture correlation/authorization; context is evidence, not live state."""
    require(request['id']==response['id'] and request['command']==response['command'],'request correlation')
    require(request['id']>context['lastId'],'request replay')
    if not response['success']:return
    args=request['arguments'];cmd=request['command']
    if 'sessionId' in args and args['sessionId'] is not None:require(args['sessionId']==context['sessionId'],'session state')
    if 'generation' in args:require(args['generation']==context['generation'],'generation state')
    if 'leaseId' in args:require(context['role']=='controller' and args['leaseId']==context['leaseId'],'lease state')
    if cmd=='advance':require(context['mode']=='externally-driven' and context['state']=='running','advance state')
    if cmd=='step':require(context['state']=='paused','step state')
