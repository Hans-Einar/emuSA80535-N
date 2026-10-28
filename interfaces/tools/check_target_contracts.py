#!/usr/bin/env python3
"""Contract-only target SDK gates; no target behavior implementation claimed."""
import copy
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
sys.dont_write_bytecode=True
import bundle
import jsonschema
import target_wire as wire
ROOT=bundle.ROOT
VERSION_ROOT=ROOT/'interfaces/0.3.0'


def reject(action,label):
    try:action()
    except (ValueError,jsonschema.ValidationError,UnicodeError):return
    raise AssertionError('negative unexpectedly accepted: '+label)


def main():
    bundle.select_version('0.3.0')
    manifest=bundle.checked_manifest(ROOT)
    schemas={k:json.loads((VERSION_ROOT/'schemas'/v).read_text()) for k,v in [('wire','runtime-3.0.schema.json'),('event','observation.schema.json'),('file','trace-file.schema.json')]}
    for s in schemas.values():jsonschema.Draft202012Validator.check_schema(s)
    fixtures={}
    for key,name in [('wire','runtime-3.0.ndjson'),('event','observations.ndjson'),('file','trace-file.ndjson')]:
        fixtures[key]=[bundle.load_json(x) for x in (VERSION_ROOT/'examples'/name).read_bytes().splitlines()]
        for e in fixtures[key]:wire.validate_frame((json.dumps(e,ensure_ascii=False,separators=(',',':'))+'\n').encode(),schemas[key])
    expected={x['properties']['command']['const'] for x in schemas['wire']['oneOf'] if x['properties']['type']['const']=='request'}
    for typ in ('request','response'):
        assert {x['command'] for x in fixtures['wire'] if x['type']==typ and x.get('success',True)}==expected
    assert {x['kind'] for x in fixtures['event']}==set(range(1,29))
    assert {x['record']['kind'] for x in fixtures['file'] if x['type']=='record'}==set(range(4))
    def req(command):return copy.deepcopy(next(x for x in fixtures['wire'] if x['command']==command and x['type']=='request'))
    def rsp(command):return copy.deepcopy(next(x for x in fixtures['wire'] if x['command']==command and x['type']=='response' and x.get('success')))
    # Every recognized command has a bounded correlated error and closed shape.
    for cmd in expected:
        err={'type':'response','id':1,'command':cmd,'success':False,'error':{'code':'STATE','message':'invalid fixture state','retryable':False}}
        wire.validate(err,schemas['wire'])
        bad=req(cmd);bad['unexpected']=0;reject(lambda:wire.validate(bad,schemas['wire']),cmd+' unknown property')
    hello=req('hello');hello['arguments']['protocol']['major']=2;reject(lambda:wire.validate(hello,schemas['wire']),'version')
    hello=req('hello');hello['arguments']['requiredCapabilities']=['futureBoundedName'];wire.validate(hello,schemas['wire'])
    for value in [1,'01','-1','18446744073709551616','1.0']:
        bad=req('advance');bad['arguments']['generation']=value;reject(lambda:wire.validate(bad,schemas['wire']),'uint64')
    bad=req('advance');bad['arguments']['generation']='18446744073709551615';wire.validate(bad,schemas['wire'])
    bad=req('advance');bad['arguments']['cycleBudget']=100001;reject(lambda:wire.validate(bad,schemas['wire']),'cycle bound')
    bad=req('tracePage');bad['arguments']['maxRecords']=17;reject(lambda:wire.validate(bad,schemas['wire']),'page bound')
    bad=req('sinkOpen');bad['arguments']['maxSegmentBytes']='65535';reject(lambda:wire.validate(bad,schemas['wire']),'segment bound')
    bad=req('captureStart');bad['arguments']['mode']=2;reject(lambda:wire.validate(bad,schemas['wire']),'unknown active context')
    for raw in [b'{"id":1,"id":2}\n',b'{"x":NaN}\n',b'\xef\xbb\xbf{}\n',b'{}\r\n',b'{}',b'\xff\n',b'{}\n{}\n']:
        reject(lambda:wire.validate_frame(raw,schemas['wire']),'malformed frame')
    bad=rsp('hello');bad['body']['capabilities'].remove('attachSession');reject(lambda:wire.validate(bad,schemas['wire']),'mandatory capability')
    bad=rsp('hello');bad['body']['capabilities'].remove('observations');reject(lambda:wire.validate(bad,schemas['wire']),'capability dependency')
    for field,val in [('instructionId','0'),('phase',3)]:
        bad=copy.deepcopy(fixtures['event'][1]);bad[field]=val;reject(lambda:wire.validate(bad,schemas['event']),'retire correlation')
    bad=copy.deepcopy(fixtures['event'][1]);bad['status']='FAULT';reject(lambda:wire.validate(bad,schemas['event']),'failed fake retirement')
    bad=copy.deepcopy(fixtures['event'][1]);bad['known']=0;bad['pc']=123;reject(lambda:wire.validate(bad,schemas['event']),'unknown PC')
    failed=copy.deepcopy(fixtures['event'][4]);failed.update(status='FAULT',partialEffects=True,reason=8,space=3,access=2);wire.validate(failed,schemas['event'])
    payload=req('boardInput');payload['arguments']['payload']={'key\0':'value\0'};wire.validate(payload,schemas['wire'])
    bad=copy.deepcopy(fixtures['event'][22]);bad['relatedFrameId']=bad['frameId'];reject(lambda:wire.validate(bad,schemas['event']),'TX replacement correlation')
    bad=copy.deepcopy(fixtures['event'][13]);bad['inputStage']=0;reject(lambda:wire.validate(bad,schemas['event']),'board vs CPU order')
    good=copy.deepcopy(fixtures['event'][13]);good['parentStimulusId']='0';wire.validate(good,schemas['event'])
    good=copy.deepcopy(fixtures['event'][13]);good.update(inputStage=0,cpuInputOrder='0');wire.validate(good,schemas['event'])
    good=copy.deepcopy(fixtures['event'][8]);good['irqSource']=11;wire.validate(good,schemas['event'])
    good['irqSource']=12;reject(lambda:wire.validate(good,schemas['event']),'IRQ source')
    bad=copy.deepcopy(fixtures['event'][0]);bad['oldValue']['value']='1';reject(lambda:wire.validate(bad,schemas['event']),'unknown value')
    file=copy.deepcopy(fixtures['file'][1]);file['record']['event']['generation']='2';reject(lambda:wire.validate(file,schemas['file']),'file event generation')
    file=copy.deepcopy(fixtures['file'][1]);file['record']['event']['instructionId']='0';reject(lambda:wire.validate(file,schemas['file']),'nested event correlation')
    config=req('replaceTraceConfig');config['arguments']['config']['destinations']=[dict(destinationId=1,kind=0,capacity=1,stopOnFull=False)];reject(lambda:wire.validate(config,schemas['wire']),'minimum ring')
    bad=rsp('tracePage');bad['body'].update(oldest='1',newest='1',requestedAfter='100',nextAfter='100');reject(lambda:wire.validate(bad,schemas['wire']),'ahead mislabeled current')
    page=rsp('tracePage');body=page['body'];record=copy.deepcopy(fixtures['file'][1]['record']);body.update(records=[record],first='1',last='1',nextAfter='1',oldest='1',newest='1');wire.validate(page,schemas['wire'])
    body['requestedAfter']='1';reject(lambda:wire.validate(page,schemas['wire']),'exclusive cursor')
    body['requestedAfter']='0';body['records'][0]['epoch']='2';reject(lambda:wire.validate(page,schemas['wire']),'epoch correlation')
    advance=req('advance');response=rsp('advance');response['id']=advance['id']
    context=dict(lastId=0,sessionId=advance['arguments']['sessionId'],generation='1',leaseId='1',role='controller',state='running',mode='externally-driven')
    wire.validate_exchange(advance,response,context)
    for field,value in [('generation','2'),('leaseId','2'),('role','observer'),('state','paused'),('mode','host-driven'),('lastId',advance['id'])]:
        bad=dict(context);bad[field]=value;reject(lambda:wire.validate_exchange(advance,response,bad),'state '+field)
    response['id']+=1;reject(lambda:wire.validate_exchange(advance,response,context),'reply correlation')
    for path in VERSION_ROOT.rglob('*.md'):
        for target in re.findall(r'\]\(([^)]+)\)',path.read_text()):
            if '://' not in target and not target.startswith('#'):assert (path.parent/target.split('#')[0]).exists(),(path,target)
    with tempfile.TemporaryDirectory(prefix='emu-target-contract-') as temp:
        work=Path(temp)
        for compiler,standard in [(os.environ.get('CC','cc'),'c99'),(os.environ.get('CXX','c++'),'c++17')]:
            for header in sorted((ROOT/'lib/include/emusa80535/contracts').glob('*.h')):
                unit=work/('header.c' if standard=='c99' else 'header.cpp');unit.write_text('#include <emusa80535/contracts/'+header.name+'>\nint main(void){return 0;}\n')
                subprocess.run(shlex.split(compiler)+['-std='+standard,'-Wall','-Wextra','-Werror','-I'+str(ROOT/'lib/include'),'-fsyntax-only',str(unit)],check=True)
            subprocess.run(shlex.split(compiler)+['-std='+standard,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-I'+str(ROOT/'lib/include'),'-x','c' if standard=='c99' else 'c++',str(VERSION_ROOT/'examples/consumer.c'),'-o',str(work/'consumer')],check=True)
            subprocess.run([str(work/'consumer')],check=True)
        fixture=work/'source'
        for name in manifest['files']:
            dest=fixture/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes((ROOT/name).read_bytes())
        (fixture/bundle.MANIFEST).write_bytes((ROOT/bundle.MANIFEST).read_bytes());(fixture/'SOURCE_COMMIT').write_text('a'*40+'\n')
        a,b=work/'a.tar.gz',work/'b.tar.gz';bundle.export(fixture,a);bundle.export(fixture,b);assert a.read_bytes()==b.read_bytes()
        files=bundle.archive_files(a);lock={**bundle.verify_files(files),'archiveSha256':bundle.sha(a.read_bytes()),'copyPath':'vendor/example','consumer':'fixture'}
        cp=work/'copy'
        for name,data in files.items():dest=cp/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
        bundle.verify_copy(cp,lock)
        for field,value in [('sourceCommit','b'*40),('manifestSha256','0'*64),('archiveSha256','0'*64),('bundleVersion','0.2.0'),('repository','wrong')]:
            reject(lambda:bundle.verify_copy(cp,{**lock,field:value}),'lock '+field)
        header=cp/'lib/include/emusa80535/contracts/observation.h';original=header.read_bytes();header.write_bytes(original+b'\n');reject(lambda:bundle.verify_copy(cp,lock),'changed header');header.write_bytes(original)
        extra=cp/'extra';extra.write_text('x');reject(lambda:bundle.verify_copy(cp,lock),'extra file');extra.unlink()
        header.unlink();reject(lambda:bundle.verify_copy(cp,lock),'missing file');header.write_bytes(original)
        bundle.select_version('0.2.0');legacy=bundle.archive_bytes(bundle.bundle_files(fixture,'a'*40));assert bundle.sha(legacy)=='5abd71f52925c8fcf299b8b8ed417279a1bc20f2a5f004425bc083f87e9cf91d','legacy archive drift'
        # Both versions work from new standalone SDK without Git or sibling data.
        for version in ('0.2.0','0.3.0'):
            subprocess.run([sys.executable,str(fixture/'interfaces/tools/bundle.py'),'--root',str(fixture),'--version',version,'check'],check=True)
        bundle.select_version('0.3.0');target=fixture/'lib/include/emusa80535/contracts/observation.h';target.write_bytes(target.read_bytes()+b'x');reject(lambda:bundle.checked_manifest(fixture),'stale manifest')
        optimized=subprocess.run([sys.executable,'-O',str(ROOT/'interfaces/tools/bundle.py'),'--root',str(fixture),'--version','0.3.0','check'],capture_output=True);assert optimized.returncode!=0
    print('PASS: target headers/layout, all commands/results/events, semantic negatives, standalone reproducible export/locks, exact immutable baseline reconstruction; target runtime NOT implemented')

if __name__=='__main__':main()
