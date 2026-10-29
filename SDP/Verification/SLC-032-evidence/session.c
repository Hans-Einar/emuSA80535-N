/* Reuse the existing generic test board; no product source is modified. MIT. */
#define _POSIX_C_SOURCE 200809L
#define main original_fixture_main
#include "tests/test_runtime_session.c"
#undef main
#include <time.h>
#include <emusa80535/contracts/control3.h>
static struct emuif_control3_api control;
static uint64_t lease;
static unsigned reqid;
static double seconds(clockid_t clock)
{ struct timespec t; assert(!clock_gettime(clock,&t)); return t.tv_sec+t.tv_nsec/1e9; }
static void send3(const char *cmd,const char *args)
{
    char buffer[4096]; struct emuif_json_request req; struct emuif_json_response res;
    int n=snprintf(buffer,sizeof buffer,"{\"type\":\"request\",\"id\":%u,\"command\":\"%s\",\"arguments\":%s}",++reqid,cmd,args);
    INIT(req); INIT(res); req.bytes=(uint8_t*)buffer; req.byte_count=(uint32_t)n;
    res.bytes=(uint8_t*)response_bytes; res.capacity=65536;
    assert(!control.request(session,1,&req,&res));
    assert(json_document_parse(&document,response_bytes,res.written,true)); assert(successful());
}
int main(void)
{
    setup(); INIT(control); assert(!emusa80535_control_query_v3(&control));
    for(unsigned rep=0;rep<3;rep++) for(unsigned mode=0;mode<2;mode++)
    for(unsigned qi=0;qi<3;qi++) {
        const unsigned quanta[]={1024,4096,8000}; unsigned quantum=quanta[qi],calls=200;
        struct emuif_session_config cfg=config(); struct emuif_client client;
        struct emuif_control_advance advance; struct emuif_control_progress progress;
        char args[1024]; memset(code,0,sizeof code); code[0]=0x80; code[1]=0xfe;
        assert(!emusa80535_session_create_v1(&cfg,&session));
        assert(!emu_session_set_execution_mode(session,EMUIF_EXECUTION_EXTERNALLY_DRIVEN));
        INIT(client); client.connection_id=1; client.role=EMUIF_ROLE_CONTROLLER;
        assert(!control.open(session,&client)); reqid=0;
        send3("hello","{\"protocol\":{\"major\":3,\"minor\":0},\"contractVersion\":\"0.3.0\",\"requiredCapabilities\":[]}");
        send3("attach","{\"sessionId\":null,\"role\":\"controller\"}");
        assert(json_string(&document,json_member(&document,body(),"sessionId"),sid,sizeof sid));
        generation=field64("generation"); lease=field64("leaseId");
        snprintf(args,sizeof args,"{\"sessionId\":\"%s\",\"generation\":\"%llu\",\"leaseId\":\"%llu\"}",sid,(unsigned long long)generation,(unsigned long long)lease);
        send3("resume",args);
        INIT(advance); advance.connection_id=1; advance.generation=generation; advance.lease_id=lease; advance.cycle_budget=quantum;
        snprintf(args,sizeof args,"{\"sessionId\":\"%s\",\"generation\":\"%llu\",\"leaseId\":\"%llu\",\"cycleBudget\":%u}",sid,(unsigned long long)generation,(unsigned long long)lease,quantum);
        double wall=seconds(CLOCK_MONOTONIC),process=seconds(CLOCK_PROCESS_CPUTIME_ID);
        for(unsigned i=0;i<calls;i++) {
            if(!mode) { INIT(progress); assert(!control.advance(session,&advance,&progress));
                assert(progress.cycles==quantum && progress.cycle==(uint64_t)(i+1)*quantum && progress.instructions==quantum/2 && progress.state==EMUIF_STATE_RUNNING && progress.safe_boundary); }
            else { send3("advance",args); assert(field64("cycles")==quantum && field64("cycle")==((uint64_t)i+1)*quantum && field64("instructions")==quantum/2); }
        }
        process=seconds(CLOCK_PROCESS_CPUTIME_ID)-process; wall=seconds(CLOCK_MONOTONIC)-wall;
        assert(board->cycle==(uint64_t)calls*quantum && board->boundaries==board->cycle+1);
        printf("{\"rep\":%u,\"mode\":%u,\"quantum\":%u,\"calls\":%u,\"cycles\":%u,\"instructions\":%u,\"boundaries\":%u,\"wallSeconds\":%.9f,\"processSeconds\":%.9f,\"cyclesPerSecond\":%.0f}\n",rep,mode,quantum,calls,calls*quantum,calls*quantum/2,board->boundaries,wall,process,calls*quantum/wall);
        assert(!control.close(session,1)); emusa80535_session_destroy_v1(session);
    }
}
