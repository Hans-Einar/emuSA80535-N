/* Opt-in read-only CODE enumeration on the real native session. MIT. */
#define main baseline_main
#include "test_runtime_session.c"
#undef main
#include <emusa80535/contracts/control3.h>
static struct emuif_control3_api control31;
static uint32_t ids31[16];
static uint64_t lease31;
static void send31(unsigned id, const char *cmd, const char *args)
{
    char q[16000];
    struct emuif_json_request req;
    struct emuif_json_response res;
    int n=snprintf(q,sizeof q,"{\"type\":\"request\",\"id\":%u,\"command\":\"%s\",\"arguments\":%s}",++ids31[id],cmd,args);
    assert(n>0 && (size_t)n<sizeof q); INIT(req); INIT(res);
    req.bytes=(uint8_t*)q; req.byte_count=(uint32_t)n;
    res.bytes=(uint8_t*)response_bytes; res.capacity=65536;
    assert(!control31.request(session,id,&req,&res));
    response_bytes[res.written]=0;
    if(res.written) {
        const char *path=getenv("CONTROL31_OUTPUT");
        assert(json_document_parse(&document,response_bytes,res.written,true));
        if(path) {FILE *f=fopen(path,"ab");assert(f);fwrite(response_bytes,1,res.written,f);fputc('\n',f);fclose(f);}
    }
}
static void cmd31(unsigned id,const char *cmd,const char *extra)
{
    char args[15000];snprintf(args,sizeof args,"{\"sessionId\":\"%s\"%s%s}",sid,extra?",":"",extra?extra:"");send31(id,cmd,args);
}
static void mut31(const char *cmd,const char *extra)
{
    char args[14000];snprintf(args,sizeof args,"\"generation\":\"%"PRIu64"\",\"leaseId\":\"%"PRIu64"\"%s%s",generation,lease31,extra?",":"",extra?extra:"");cmd31(1,cmd,args);
}
static void open31(unsigned id, unsigned minor, bool attach, bool owner)
{
    struct emuif_client c; INIT(c);c.connection_id=id;c.role=owner?1:0;c.may_terminate=1;
    assert(!control31.open(session,&c));
    send31(id,"hello",minor?"{\"protocol\":{\"major\":3,\"minor\":1},\"contractVersion\":\"0.4.0\",\"requiredCapabilities\":[\"readCodeBreakpoints\"]}":"{\"protocol\":{\"major\":3,\"minor\":0},\"contractVersion\":\"0.3.0\",\"requiredCapabilities\":[]}");
    assert(successful());
    assert((strstr(response_bytes,"readCodeBreakpoints")!=NULL)==(minor==1));
    if(attach) {
        send31(id,"attach",owner?"{\"sessionId\":null,\"role\":\"controller\"}":"{\"sessionId\":null,\"role\":\"observer\"}");assert(successful());
        assert(json_string(&document,json_member(&document,body(),"sessionId"),sid,sizeof sid));generation=field64("generation");
        if(owner)lease31=field64("leaseId");
    }
}
static void addresses31(unsigned id,const unsigned *expected,unsigned count)
{
    int array,t;unsigned at=0;char identity[33];
    cmd31(id,"readCodeBreakpoints",NULL);assert(successful());
    assert(field64("generation")==generation);
    assert(json_string(&document,json_member(&document,body(),"sessionId"),identity,sizeof identity));assert(!strcmp(identity,sid));
    array=json_member(&document,body(),"addresses");assert(document.tokens[array].size==(int)count);
    for(t=array+1;t<document.count&&document.tokens[t].start<document.tokens[array].end;t=json_next(&document,t)) {
        int64_t v;assert(json_integer(&document,t,0,65535,&v));assert(at<count && v==(int64_t)expected[at++]);
    }
    assert(at==count);
}
static void test31(void)
{
    struct emuif_session_config c=config();
    unsigned expected[]={0,17,65535}, all[1024];
    char list[10000],before[65536];unsigned i;size_t n;
    memset(code,0,sizeof code);code[0]=0x80;code[1]=0xfe;
    assert(!emusa80535_session_create_v1(&c,&session));
    memset(ids31,0,sizeof ids31);open31(1,1,true,true);open31(2,1,true,false);open31(3,0,true,false);
    addresses31(2,NULL,0);
    mut31("replaceCodeBreakpoints","\"addresses\":[65535,17,0]");assert(successful());
    cmd31(1,"status",NULL);strcpy(before,strstr(response_bytes,"\"body\":"));
    addresses31(2,expected,3);addresses31(1,expected,3);
    cmd31(1,"status",NULL);assert(!strcmp(before,strstr(response_bytes,"\"body\":")));
    mut31("replaceCodeBreakpoints","\"addresses\":[22]");assert(successful());
    {unsigned temp=22;addresses31(2,&temp,1);}
    mut31("replaceCodeBreakpoints","\"addresses\":[65535,17,0]");assert(successful());addresses31(2,expected,3);
    mut31("reset","\"seed\":0,\"entry\":0");assert(successful());generation=field64("generation");assert(generation==2);addresses31(2,expected,3);
    mut31("replaceCodeBreakpoints","\"addresses\":[]");assert(successful());addresses31(2,NULL,0);
    n=(size_t)snprintf(list,sizeof list,"\"addresses\":[");
    for(i=0;i<1024;i++) {all[i]=i==1023?65535:i*31;n+=(size_t)snprintf(list+n,sizeof list-n,"%s%u",i?",":"",all[i]);}
    snprintf(list+n,sizeof list-n,"]");mut31("replaceCodeBreakpoints",list);assert(successful());addresses31(2,all,1024);
    mut31("replaceCodeBreakpoints","\"addresses\":[65535]");assert(successful());
    mut31("resume",NULL);assert(successful());cmd31(1,"status",NULL);strcpy(before,strstr(response_bytes,"\"body\":"));
    {unsigned last=65535;addresses31(2,&last,1);}
    cmd31(1,"status",NULL);assert(!strcmp(before,strstr(response_bytes,"\"body\":")));
    send31(2,"readCodeBreakpoints","{\"sessionId\":\"ffffffffffffffffffffffffffffffff\"}");error_is("STALE");
    open31(4,1,false,false);cmd31(4,"readCodeBreakpoints",NULL);error_is("STATE");
    cmd31(3,"readCodeBreakpoints",NULL);assert(!response_bytes[0]);
    open31(5,1,true,false);cmd31(5,"readCodeBreakpoints","\"extra\":true");error_is("INVALID");
    assert(!control31.close(session,4));
    mut31("pause",NULL);mut31("replaceCodeBreakpoints","\"addresses\":[0]");
    mut31("resume",NULL);mut31("advance","\"cycleBudget\":10");assert(successful());
    cmd31(1,"status",NULL);assert(strstr(response_bytes,"\"reason\":\"breakpoint\""));
    {unsigned zero=0;addresses31(2,&zero,1);}
    mut31("step","\"maxCycles\":10");assert(successful());assert(field64("instructions")==1);
    emusa80535_session_destroy_v1(session);
}
static void fault31(void)
{
    struct emuif_session_config c=config();unsigned one=65535;
    memset(code,0,sizeof code);code[0]=0xf0; access_fault=true;
    assert(!emusa80535_session_create_v1(&c,&session));memset(ids31,0,sizeof ids31);
    open31(1,1,true,true);open31(2,1,true,false);
    mut31("replaceCodeBreakpoints","\"addresses\":[65535]");mut31("resume",NULL);mut31("advance","\"cycleBudget\":10");assert(successful());
    cmd31(1,"status",NULL);assert(strstr(response_bytes,"\"state\":\"faulted\""));addresses31(2,&one,1);
    emusa80535_session_destroy_v1(session);access_fault=false;
}
int main(void)
{
    setup();INIT(control31);assert(!emusa80535_control_query_v3(&control31));test31();fault31();
    puts("control3.1 native: sorted complete enumeration, observers, state/reset, no progress and old profile isolation PASS");return 0;
}
