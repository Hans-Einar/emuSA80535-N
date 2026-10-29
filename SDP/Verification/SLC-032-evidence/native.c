/* Read-only SLC-032 throughput probe; synthetic SJMP loop. MIT. */
#define _POSIX_C_SOURCE 200809L
#include <emusa80535/contracts/cpu.h>
#include <emusa80535/contracts/observation.h>
#include "emu_debug_observation_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define INIT(x) do { memset(&(x),0,sizeof(x)); (x).struct_size=sizeof(x); (x).api_version=1; } while(0)
static uint64_t events;
static emuif_status bus(void *ctx,const struct emuif_bus_access *a,struct emuif_bus_result *r)
{ (void)ctx; r->value=a->value; return EMUIF_OK; }
static emuif_status port(void *ctx,const struct emuif_port_output *p)
{ (void)ctx; (void)p; return EMUIF_OK; }
static emuif_status signal_out(void *ctx,const struct emuif_signal_output *p)
{ (void)ctx; (void)p; return EMUIF_OK; }
static void observe(void *ctx,const struct emuif_observation *o)
{ events++; if(ctx) assert(!emu_debug_runtime_observation(ctx,o,NULL)); }
static double seconds(clockid_t clock)
{ struct timespec t; assert(!clock_gettime(clock,&t)); return t.tv_sec+t.tv_nsec/1e9; }
int main(void)
{
    struct emuif_cpu_api api;
    struct emuif_observation_api obs;
    INIT(api); INIT(obs);
    assert(!emusa80535_cpu_query_v1(&api));
    assert(!emusa80535_cpu_observation_query_v1(&obs));
    for(unsigned rep=0;rep<3;rep++) for(unsigned mode=0;mode<3;mode++) {
        struct emuif_cpu *cpu;
        struct emuif_cpu_config cfg;
        struct emuif_cpu_hooks hooks;
        struct emuif_reset rst;
        struct emuif_observation_binding binding;
        struct emuif_cycle_result r;
        struct emuif_snapshot s;
        struct em8051_debug_runtime *debug=NULL;
        unsigned char rom[65536]={0x80,0xfe};
        INIT(cfg); INIT(hooks); INIT(rst); INIT(binding); INIT(binding.observer); INIT(r); INIT(s);
        hooks.bus_access=bus; hooks.port_output=port; hooks.signal_output=signal_out;
        cfg.variant=2; cfg.oscillator_hz=11059200;
        assert(!api.create(&cfg,&hooks,&cpu)); assert(!api.reset(cpu,&rst));
        assert(!api.load_code(cpu,rom,sizeof rom));
        if(mode) {
            if(mode==2) assert(!em8051_debug_runtime_create(&debug));
            binding.generation=1; binding.observer.observe=observe; binding.observer.context=debug;
            assert(!obs.bind(cpu,&binding));
        }
        events=0;
        double wall=seconds(CLOCK_MONOTONIC), process=seconds(CLOCK_PROCESS_CPUTIME_ID);
        unsigned cycles=2000000;
        for(unsigned i=0;i<cycles;i++) { INIT(r); assert(!api.cycle(cpu,&r)); }
        process=seconds(CLOCK_PROCESS_CPUTIME_ID)-process; wall=seconds(CLOCK_MONOTONIC)-wall;
        assert(!api.snapshot(cpu,&s));
        assert(s.cycle==cycles && s.instructions==cycles/2 && s.pc==0 && s.safe_boundary);
        assert(events==(mode ? cycles : 0));
        printf("{\"rep\":%u,\"mode\":%u,\"cycles\":%llu,\"instructions\":%llu,"
               "\"events\":%llu,\"wallSeconds\":%.9f,\"processSeconds\":%.9f,\"cyclesPerSecond\":%.0f}\n",
               rep,mode,(unsigned long long)s.cycle,(unsigned long long)s.instructions,
               (unsigned long long)events,wall,process,cycles/wall);
        api.destroy(cpu); if(debug) em8051_debug_runtime_destroy(debug);
    }
}
