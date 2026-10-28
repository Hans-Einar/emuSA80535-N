/* Canonical producer regressions. Copyright 2026 Hans-Einar. MIT. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emusa80535/contracts/observation.h>
#include "../emu_cpu_lifecycle_internal.h"
#define INIT(x) do { memset(&(x), 0, sizeof(x)); (x).struct_size=sizeof(x); (x).api_version=1; } while (0)
#define MAX_EVENTS 30000
static struct emuif_cpu_api api;
static struct emuif_observation_api obs;
struct fixture {
    struct emuif_cpu *cpu;
    struct emuif_observation *events;
    size_t count;
    uint64_t sequence;
    emuif_status bus_error, signal_error;
    unsigned signals;
};
static emuif_status bus(void *c, const struct emuif_bus_access *a, struct emuif_bus_result *r) {
    struct fixture *f=c; r->value=a->direction ? a->value : 0x5a; return f->bus_error;
}
static emuif_status port(void *c, const struct emuif_port_output *o) { (void)c; (void)o; return 0; }
static emuif_status signal_out(void *c, const struct emuif_signal_output *o) {
    struct fixture *f=c; (void)o; ++f->signals; return f->signal_error;
}
static void observed(void *c, const struct emuif_observation *e) {
    struct fixture *f=c;
    struct emuif_snapshot s;
    INIT(s);
    assert(api.snapshot(f->cpu, &s)==EMUIF_BUSY);
    assert(e->struct_size==sizeof(*e) && e->api_version==1);
    assert(e->sequence > f->sequence && e->generation);
    assert(e->old_value.struct_size==sizeof(e->old_value));
    assert(e->new_value.struct_size==sizeof(e->new_value));
    if (!e->old_value.known) assert(!e->old_value.value);
    if (!e->new_value.known) assert(!e->new_value.value);
    if (e->kind>=18 && e->kind<=21) assert(e->cpu_input_order && e->input_stage==1);
    if (e->kind==6 || e->kind==7 || e->kind==9 || e->kind==10) {
        assert(e->context_id);
        assert(e->frame_kind==(e->kind==6 || e->kind==7 ? 1u : 2u));
    }
    assert(f->count<MAX_EVENTS); f->events[f->count++]=*e; f->sequence=e->sequence;
}
static void start(struct fixture *f, const uint8_t *code, size_t count, unsigned variant, int bind) {
    struct emuif_cpu_config cfg;
    struct emuif_cpu_hooks hooks;
    struct emuif_reset reset;
    struct emuif_observation_binding binding;
    uint8_t *rom=calloc(65536,1);
    memset(f,0,sizeof(*f)); f->events=calloc(MAX_EVENTS,sizeof(*f->events)); assert(rom && f->events);
    INIT(cfg); INIT(hooks); INIT(reset); INIT(binding); INIT(binding.observer);
    cfg.variant=variant; cfg.oscillator_hz=12000000;
    hooks.context=f; hooks.bus_access=bus; hooks.port_output=port; hooks.signal_output=signal_out;
    assert(api.create(&cfg,&hooks,&f->cpu)==0); reset.seed=123;
    assert(api.reset(f->cpu,&reset)==0); memcpy(rom,code,count);
    assert(api.load_code(f->cpu,rom,65536)==0); free(rom);
    binding.generation=UINT64_C(0x100000001); binding.observer.context=f; binding.observer.observe=observed;
    if (bind) assert(obs.bind(f->cpu,&binding)==0);
}
static void finish(struct fixture *f) { api.destroy(f->cpu); free(f->events); }
static struct emuif_snapshot snapshot(struct fixture *f) {
    struct emuif_snapshot s; INIT(s); assert(api.snapshot(f->cpu,&s)==0); return s;
}
static emuif_status cycle(struct fixture *f) {
    struct emuif_cycle_result r; INIT(r); return api.cycle(f->cpu,&r);
}
static emuif_status step(struct fixture *f) {
    emuif_status status;
    do { status=cycle(f); if (status) return status; } while (!snapshot(f).safe_boundary);
    return 0;
}
static void cycles(struct fixture *f, unsigned n) { while (n--) assert(cycle(f)==0); }
static size_t count_kind(struct fixture *f, unsigned kind) {
    size_t n=0,i; for(i=0;i<f->count;++i) n+=f->events[i].kind==kind; return n;
}
static const struct emuif_observation *last_kind(struct fixture *f,unsigned kind) {
    size_t i=f->count; while(i--) if(f->events[i].kind==kind) return &f->events[i]; return NULL;
}
static emuif_status rx(struct fixture *f, uint64_t parent,uint64_t order,unsigned value,int direct) {
    struct emuif_observed_input input; INIT(input); INIT(input.input);
    input.parent_stimulus_id=parent; input.input.cycle=snapshot(f).cycle;
    input.input.order=order; input.input.kind=EMUIF_INPUT_UART_RX; input.input.value=value; input.input.mask=511;
    return direct ? api.apply_input(f->cpu,&input.input) : obs.deliver(f->cpu,&input);
}
static void test_prefix(void) {
    struct extended { struct emuif_observation_api api; uint8_t tail[32]; } out;
    struct binding_extended { struct emuif_observation_binding binding; uint8_t tail[32]; } b;
    struct fixture f;
    const uint8_t code[]={0};
    memset(&out,0,sizeof(out)); INIT(out.api); out.api.struct_size=sizeof(out); memset(out.tail,0xa5,sizeof(out.tail));
    assert(emusa80535_cpu_observation_query_v1(&out.api)==0);
    for(size_t i=0;i<sizeof(out.tail);++i) assert(out.tail[i]==0xa5);
    out.api.flags=1; assert(emusa80535_cpu_observation_query_v1(&out.api)==EMUIF_INVALID);
    start(&f,code,sizeof(code),2,0); INIT(b.binding); INIT(b.binding.observer);
    b.binding.struct_size=sizeof(b); memset(b.tail,0xa5,sizeof(b.tail));
    b.binding.generation=1; b.binding.observer.observe=observed; b.binding.observer.context=&f;
    assert(obs.bind(f.cpu,&b.binding)==0); assert(obs.bind(f.cpu,&b.binding)==EMUIF_BUSY);
    assert(obs.unbind(f.cpu)==0); assert(obs.unbind(f.cpu)==0); finish(&f);
    start(&f,code,sizeof(code),0,0); assert(obs.bind(f.cpu,&b.binding)==EMUIF_UNSUPPORTED); finish(&f);
}
static void test_instruction(void) {
    struct fixture f;
    const uint8_t code[]={0x75,0x20,0xa5,0x90,0x12,0x34,0xe0};
    start(&f,code,sizeof(code),2,1);
    assert(step(&f)==0); assert(step(&f)==0);
    const struct emuif_observation *e=last_kind(&f,EMUIF_OBS_INSTRUCTION_RETIRE);
    assert(e && e->pc==3 && e->byte_count==3 && e->bytes[1]==0x12 && e->bytes[2]==0x34);
    assert(e->begin_cycle==2 && e->cycle==4 && e->next_pc==6 && e->retired_instructions==2 && e->text[0]);
    f.bus_error=EMUIF_UNSUPPORTED;
    assert(step(&f)==EMUIF_UNSUPPORTED);
    e=last_kind(&f,EMUIF_OBS_ACCESS_RESULT); assert(e && e->status==EMUIF_UNSUPPORTED && e->reason==EMUIF_OBS_REASON_UNSUPPORTED && !e->new_value.known);
    assert(count_kind(&f,2)==2 && count_kind(&f,3)==1);
    e=last_kind(&f,3); assert(e->pc==6 && e->cycle==4 && e->retired_instructions==2);
    assert(!(e->known&EMUIF_OBS_KNOWN_NEXT_PC)); finish(&f);
    /* Every dispatched opcode has one attempt and exactly one terminal fact;
     * observational instrumentation must preserve status and architectural state. */
    for(unsigned opcode=0;opcode<256;++opcode) {
        struct fixture plain;
        uint8_t program[64]={0}; program[0]=(uint8_t)opcode; program[1]=0x20; program[2]=0x30;
        start(&f,program,sizeof(program),2,1); start(&plain,program,sizeof(program),2,0);
        emuif_status a=step(&f),bstatus=step(&plain); assert(a==bstatus);
        struct emuif_snapshot x=snapshot(&f),y=snapshot(&plain);
        assert(x.pc==y.pc && x.a==y.a && x.psw==y.psw && x.sp==y.sp && x.dptr==y.dptr);
        assert(x.cycle==y.cycle && x.instructions==y.instructions);
        assert(count_kind(&f,1)==1 && count_kind(&f,2)+count_kind(&f,3)==1);
        finish(&f); finish(&plain);
    }
}
static void test_context(void) {
    struct fixture f;
    uint8_t code[64]={0x12,0x00,0x10}; code[16]=0x12; code[18]=0x20; code[19]=0x22; code[32]=0x22;
    start(&f,code,sizeof(code),2,1); assert(step(&f)==0);
    uint64_t outer=last_kind(&f,6)->context_id;
    assert(step(&f)==0); uint64_t inner=last_kind(&f,6)->context_id;
    assert(last_kind(&f,6)->parent_context_id==outer && inner!=outer);
    assert(step(&f)==0); assert(last_kind(&f,7)->context_id==inner);
    assert(step(&f)==0); assert(last_kind(&f,7)->context_id==outer && snapshot(&f).pc==3); finish(&f);
    code[0]=0x22; start(&f,code,sizeof(code),2,1); assert(step(&f)==0);
    const struct emuif_observation *e=last_kind(&f,7); assert(e && e->context_id && e->frame_kind==1);
    assert(e->reason==EMUIF_OBS_REASON_CONTEXT && !(e->known&EMUIF_OBS_KNOWN_CONTEXT)); finish(&f);
}
static const uint8_t uart_code[]={
    0x75,0x98,0xc0, /* mode3 REN clear */
    0x75,0x99,0x11, 0x75,0x99,0x22, /* held then replaced */
    0x75,0x98,0xd0, /* REN */
    0x75,0x89,0x20, 0x75,0x8d,0xff, 0x75,0x8b,0xff,
    0x75,0x87,0x80, 0x75,0x88,0x40, 0x80,0xfe
};
static void test_uart(void) {
    struct fixture f; start(&f,uart_code,sizeof(uart_code),2,1);
    assert(step(&f)==0); assert(rx(&f,91,1,0x133,0)==EMUIF_STATE);
    assert(count_kind(&f,18)==0 && count_kind(&f,20)==1 && count_kind(&f,21)==1);
    assert(last_kind(&f,20)->reason==EMUIF_OBS_REASON_REN);
    assert(step(&f)==0); uint64_t displaced=last_kind(&f,22)->frame_id;
    assert(last_kind(&f,22)->access_id && last_kind(&f,22)->instruction_id);
    assert(step(&f)==0); assert(last_kind(&f,23)->related_frame_id==displaced);
    assert(step(&f)==0); assert(rx(&f,92,2,0x133,0)==0);
    uint64_t frame=last_kind(&f,18)->frame_id;
    assert(rx(&f,93,3,0x144,0)==EMUIF_BUSY);
    assert(last_kind(&f,20)->reason==EMUIF_OBS_REASON_BUSY && last_kind(&f,20)->frame_id!=frame);
    cycles(&f,220);
    assert(count_kind(&f,19)==1 && last_kind(&f,19)->frame_id==frame && last_kind(&f,19)->parent_stimulus_id==92);
    assert(last_kind(&f,19)->new_value.value==0x133);
    assert(count_kind(&f,24)==1 && count_kind(&f,25)==1 && count_kind(&f,26)==1 && count_kind(&f,27)==1);
    assert(last_kind(&f,27)->cycle-last_kind(&f,24)->cycle==176);
    assert(last_kind(&f,25)->cycle==last_kind(&f,26)->cycle);
    assert(rx(&f,0,4,0x155,1)==0); assert(last_kind(&f,18)->cpu_input_order==4);
    cycles(&f,180); assert(last_kind(&f,20)->reason==EMUIF_OBS_REASON_RI); finish(&f);
    /* Tail failure retains the successful frame-end fact but aborts this opcode
     * at the attempted peripheral phase, without incrementing completed cycles. */
    start(&f,uart_code,sizeof(uart_code),2,1); f.signal_error=EMUIF_FAULT;
    for(unsigned n=0;n<300;++n) { if(cycle(&f)==EMUIF_FAULT) break; }
    assert(f.signals==1); const struct emuif_observation *end=last_kind(&f,27),*aborted=last_kind(&f,3);
    assert(end && aborted && aborted->cycle==end->cycle);
    assert(aborted->cycle==snapshot(&f).cycle+1 && aborted->sequence>end->sequence);
    assert(last_kind(&f,13)->cycle==aborted->cycle); finish(&f);
    /* TI is an edge, not an unconditional event for every transmitted frame. */
    uint8_t two_frames[40]={0}; memcpy(two_frames,uart_code,sizeof(uart_code));
    const uint8_t delay_then_send[]={0x7f,100,0x00,0xdf,0xfd,0x75,0x99,0x44,0x80,0xfe};
    memcpy(two_frames+27,delay_then_send,sizeof(delay_then_send));
    start(&f,two_frames,sizeof(two_frames),2,1); cycles(&f,600);
    assert(count_kind(&f,27)==2 && count_kind(&f,26)==1); finish(&f);
}
static void test_reset(void) {
    struct fixture f; start(&f,uart_code,sizeof(uart_code),2,1);
    for(unsigned n=0;n<4;++n) assert(step(&f)==0);
    assert(rx(&f,99,7,0x155,0)==0);
    uint64_t last=last_kind(&f,18)->frame_id;
    struct emuif_reset r; INIT(r); r.seed=123; assert(api.reset(f.cpu,&r)==0);
    assert(last_kind(&f,21)->reason==EMUIF_OBS_REASON_RESET);
    assert(last_kind(&f,27)->reason==EMUIF_OBS_REASON_RESET);
    assert(obs.unbind(f.cpu)==0);
    struct emuif_observation_binding b; INIT(b); INIT(b.observer); b.generation=UINT64_C(0x100000002);
    b.observer.context=&f; b.observer.observe=observed; assert(obs.bind(f.cpu,&b)==0);
    assert(emu_cpu_observation_lifecycle(f.cpu,EMUIF_OBS_RESET,0)==0);
    assert(last_kind(&f,11)->generation==b.generation);
    uint8_t rom[65536]={0}; memcpy(rom,uart_code,sizeof(uart_code)); assert(api.load_code(f.cpu,rom,sizeof(rom))==0);
    assert(step(&f)==0); assert(rx(&f,100,8,1,0)==EMUIF_STATE);
    assert(last_kind(&f,20)->frame_id>last); finish(&f);
    start(&f,uart_code,sizeof(uart_code),2,1); cycles(&f,60);
    assert(count_kind(&f,24)==1 && count_kind(&f,27)==0);
    last=last_kind(&f,24)->frame_id; INIT(r); assert(api.reset(f.cpu,&r)==0);
    assert(last_kind(&f,27)->frame_id==last && last_kind(&f,27)->reason==EMUIF_OBS_REASON_RESET);
    finish(&f);
    /* API1 reset can abandon a pending tail; close the actual attempt once. */
    const uint8_t pending[]={0x90,0x12,0x34}; start(&f,pending,sizeof(pending),2,1);
    assert(cycle(&f)==0 && !snapshot(&f).safe_boundary); INIT(r); assert(api.reset(f.cpu,&r)==0);
    assert(count_kind(&f,2)==0 && count_kind(&f,3)==1);
    assert(last_kind(&f,3)->reason==EMUIF_OBS_REASON_RESET); finish(&f);
}
static void test_irq_and_gap(void) {
    struct fixture f;
    uint8_t code[512]={0x75,0x88,0xaa, 0x75,0x98,0x03, 0x75,0xc0,0xff};
    start(&f,code,sizeof(code),2,1);
    assert(step(&f)==0); assert(step(&f)==0); assert(step(&f)==0);
    unsigned sources=0;
    for(size_t i=0;i<f.count;++i) if(f.events[i].kind==EMUIF_OBS_IRQ_REQUEST) sources|=1u<<f.events[i].irq_source;
    assert(sources==0xfff); finish(&f);
    memset(code,0,sizeof(code)); code[0]=2; code[1]=1; code[3]=0x32;
    const uint8_t entry[]={0x75,0x81,0x40,0x75,0xa8,0x81,0x75,0x88,0x03,0x80,0xfe};
    memcpy(code+256,entry,sizeof(entry)); start(&f,code,sizeof(code),2,1);
    for(unsigned i=0;i<5;++i) assert(step(&f)==0);
    assert(step(&f)==0); /* interrupt entry consumes two cycles, no opcode */
    const struct emuif_observation *e=last_kind(&f,EMUIF_OBS_IRQ_ENTER);
    assert(e && e->irq_depth==0 && e->irq_source==0 && e->target==3);
    uint64_t context=e->context_id;
    assert(step(&f)==0); e=last_kind(&f,EMUIF_OBS_RETI);
    assert(e && e->context_id==context && e->irq_depth==1 && e->reason==0);
    assert(last_kind(&f,EMUIF_OBS_INSTRUCTION_RETIRE)->irq_depth==0); finish(&f);
    /* Same RET opcode in an IRQ context is a mismatch, never a known CALL. */
    code[3]=0x22; start(&f,code,sizeof(code),2,1);
    for(unsigned i=0;i<6;++i) assert(step(&f)==0);
    context=last_kind(&f,9)->context_id; assert(step(&f)==0); e=last_kind(&f,7);
    assert(e && e->context_id!=context && e->reason==EMUIF_OBS_REASON_CONTEXT);
    assert(!(e->known&EMUIF_OBS_KNOWN_CONTEXT) && !e->parent_context_id && !e->sp_before && !e->sp_after);
    finish(&f);
    /* An unobserved return invalidates prior context continuity on rebind. */
    memset(code,0,sizeof(code)); code[0]=0x12; code[2]=0x10; code[3]=0x12; code[5]=0x10; code[16]=0x22;
    start(&f,code,sizeof(code),2,1); assert(step(&f)==0); assert(obs.unbind(f.cpu)==0); assert(step(&f)==0);
    struct emuif_observation_binding b; INIT(b); INIT(b.observer); b.generation=2;
    b.observer.context=&f; b.observer.observe=observed; assert(obs.bind(f.cpu,&b)==0); assert(step(&f)==0);
    /* The next actual CALL establishes only its own fully observed extent;
     * the old stack was invalidated, so it has no known parent. */
    e=last_kind(&f,6); context=e->context_id;
    assert(e->reason==0 && (e->known&EMUIF_OBS_KNOWN_CONTEXT) && !e->parent_context_id);
    assert(step(&f)==0); e=last_kind(&f,7);
    assert(e->context_id==context && (e->known&EMUIF_OBS_KNOWN_CONTEXT));
    finish(&f);
    /* Recursion beyond the bounded observer stack produces a fresh unknown
     * token with the opcode's frame type, never an invented known entry. */
    memset(code,0,sizeof(code)); code[0]=0x12;
    start(&f,code,sizeof(code),2,1);
    for(unsigned i=0;i<65;++i) assert(step(&f)==0);
    e=last_kind(&f,6); assert(e->reason==EMUIF_OBS_REASON_EXHAUSTED && e->context_id && !(e->known&EMUIF_OBS_KNOWN_CONTEXT));
    finish(&f);
}
static void rebind(struct fixture *f, uint64_t generation) {
    struct emuif_observation_binding b; INIT(b); INIT(b.observer);
    b.generation=generation; b.observer.context=f; b.observer.observe=observed;
    assert(obs.bind(f->cpu,&b)==0);
}
static void test_context_recovery(void) {
    struct fixture f;
    const struct emuif_observation *e;
    uint64_t child, grandchild;
    uint8_t code[512]={0x12,0,16};
    /* An unobserved outer CALL cannot become known merely because a nested
     * child and grandchild are fully observed and return successfully. */
    code[16]=0x12; code[18]=32; code[19]=0x22;
    code[32]=0x12; code[34]=48; code[35]=0x22; code[48]=0x22;
    start(&f,code,sizeof(code),2,0); assert(step(&f)==0); rebind(&f,2);
    assert(step(&f)==0); e=last_kind(&f,6); child=e->context_id;
    assert((e->known&EMUIF_OBS_KNOWN_CONTEXT) && !e->parent_context_id && !e->reason);
    assert(step(&f)==0); e=last_kind(&f,6); grandchild=e->context_id;
    assert((e->known&EMUIF_OBS_KNOWN_CONTEXT) && e->parent_context_id==child);
    assert(step(&f)==0); e=last_kind(&f,7);
    assert(e->context_id==grandchild && (e->known&EMUIF_OBS_KNOWN_CONTEXT));
    assert(step(&f)==0); e=last_kind(&f,7);
    assert(e->context_id==child && (e->known&EMUIF_OBS_KNOWN_CONTEXT));
    assert(step(&f)==0); e=last_kind(&f,7);
    assert(!(e->known&EMUIF_OBS_KNOWN_CONTEXT) && e->reason==EMUIF_OBS_REASON_CONTEXT);
    assert(!e->parent_context_id && !e->sp_before && !e->sp_after);
    finish(&f);

    /* A typed mismatch loses the old CALL but permits a new observed child.
     * Returning that child still cannot repair the mismatched ancestry. */
    memset(code,0,sizeof(code)); code[0]=0x12; code[2]=16;
    code[3]=0x12; code[5]=32; code[6]=0x22; code[16]=0x32; code[32]=0x22;
    start(&f,code,sizeof(code),2,1); assert(step(&f)==0);
    uint64_t old=last_kind(&f,6)->context_id;
    assert(step(&f)==0); e=last_kind(&f,10);
    assert(!(e->known&EMUIF_OBS_KNOWN_CONTEXT) && e->context_id!=old);
    assert(e->reason==EMUIF_OBS_REASON_CONTEXT);
    assert(step(&f)==0); e=last_kind(&f,6); child=e->context_id;
    assert((e->known&EMUIF_OBS_KNOWN_CONTEXT) && !e->parent_context_id);
    assert(step(&f)==0); assert(last_kind(&f,7)->context_id==child);
    assert(step(&f)==0); e=last_kind(&f,7);
    assert(!(e->known&EMUIF_OBS_KNOWN_CONTEXT) && e->reason==EMUIF_OBS_REASON_CONTEXT);
    finish(&f);

    /* Known return PC and SP are both required. Corrupting either invalidates
     * the complete ancestor chain, not just the frame reporting the mismatch. */
    for(unsigned corrupt_sp=0;corrupt_sp<2;++corrupt_sp) {
        memset(code,0,sizeof(code)); code[0]=0x12; code[2]=16;
        code[6]=0x12; code[8]=32; code[9]=0x22; code[32]=0x22;
        const uint8_t corrupt_pc[]={0x75,8,6,0x22};
        const uint8_t corrupt_stack[]={0x75,10,6,0x75,11,0,0x75,0x81,11,0x22};
        memcpy(code+16,corrupt_sp ? corrupt_stack : corrupt_pc,
               corrupt_sp ? sizeof(corrupt_stack) : sizeof(corrupt_pc));
        start(&f,code,sizeof(code),2,1); assert(step(&f)==0); old=last_kind(&f,6)->context_id;
        for(unsigned i=0;i<(corrupt_sp ? 4u : 2u);++i) assert(step(&f)==0);
        e=last_kind(&f,7);
        assert(snapshot(&f).pc==6 && e->context_id!=old && !(e->known&EMUIF_OBS_KNOWN_CONTEXT));
        assert(e->reason==EMUIF_OBS_REASON_CONTEXT);
        assert(step(&f)==0); e=last_kind(&f,6); child=e->context_id;
        assert((e->known&EMUIF_OBS_KNOWN_CONTEXT) && !e->parent_context_id);
        assert(step(&f)==0); e=last_kind(&f,7);
        assert(e->context_id==child && (e->known&EMUIF_OBS_KNOWN_CONTEXT));
        assert(step(&f)==0); e=last_kind(&f,7);
        assert(!(e->known&EMUIF_OBS_KNOWN_CONTEXT) && e->reason==EMUIF_OBS_REASON_CONTEXT);
        finish(&f);
    }

    /* Overflow closes all older observation continuity. Entry66 can be known
     * above the unknown65th frame; its return must not expose frame64 as top. */
    memset(code,0,sizeof(code));
    const uint8_t recurse[]={0x75,0x81,0x30,0x7f,66,0x12,0,16,0x80,0xfe};
    memcpy(code,recurse,sizeof(recurse));
    const uint8_t function[]={0xdf,1,0x22,0x12,0,16,0x22};
    memcpy(code+16,function,sizeof(function));
    start(&f,code,sizeof(code),2,1);
    while(count_kind(&f,6)<65) assert(step(&f)==0);
    e=last_kind(&f,6); old=e->context_id;
    assert(e->reason==EMUIF_OBS_REASON_EXHAUSTED && !(e->known&EMUIF_OBS_KNOWN_CONTEXT));
    while(count_kind(&f,6)<66) assert(step(&f)==0);
    e=last_kind(&f,6); child=e->context_id;
    assert(child>old && (e->known&EMUIF_OBS_KNOWN_CONTEXT) && !e->parent_context_id);
    assert(step(&f)==0); assert(step(&f)==0); e=last_kind(&f,7);
    assert(e->context_id==child && (e->known&EMUIF_OBS_KNOWN_CONTEXT));
    assert(step(&f)==0); e=last_kind(&f,7);
    assert(!(e->known&EMUIF_OBS_KNOWN_CONTEXT) && e->reason==EMUIF_OBS_REASON_CONTEXT);
    finish(&f);

    /* A newly observed interrupt is a known IRQ above an unknown base, with
     * observed CALL children; RETI validates that IRQ without knowing ancestry. */
    memset(code,0,sizeof(code)); code[0]=2; code[1]=1;
    code[3]=0x12; code[5]=32; code[6]=0x32; code[32]=0x22;
    const uint8_t enable[]={0x75,0x81,0x40,0x75,0xa8,0x81,0x75,0x88,0x03,0x80,0xfe};
    memcpy(code+256,enable,sizeof(enable));
    start(&f,code,sizeof(code),2,0);
    for(unsigned i=0;i<4;++i) assert(step(&f)==0);
    rebind(&f,2);
    for(unsigned i=0;i<3 && !count_kind(&f,9);++i) assert(step(&f)==0);
    e=last_kind(&f,9); assert(e && (e->known&EMUIF_OBS_KNOWN_CONTEXT) && !e->parent_context_id);
    uint64_t irq=e->context_id;
    assert(step(&f)==0); e=last_kind(&f,6); child=e->context_id;
    assert((e->known&EMUIF_OBS_KNOWN_CONTEXT) && e->parent_context_id==irq);
    assert(step(&f)==0); assert(last_kind(&f,7)->context_id==child);
    assert(step(&f)==0); e=last_kind(&f,10);
    assert((e->known&EMUIF_OBS_KNOWN_CONTEXT) && e->context_id==irq && !e->reason);
    finish(&f);
}
static void test_context_tail_irq(void) {
    struct fixture f;
    uint8_t code[512]={2,1,0}; code[3]=0x32;
    const uint8_t main_code[]={0x75,0x81,0x40,0x75,0xa8,0x81,0x75,0x88,1,0,0x12,1,0x20,0x80,0xfe};
    memcpy(code+256,main_code,sizeof(main_code)); code[0x120]=0x22;
    for(unsigned returning=0;returning<2;++returning) {
        start(&f,code,sizeof(code),2,1);
        for(unsigned i=0;i<5;++i) assert(step(&f)==0);
        if(returning) assert(step(&f)==0);
        assert(cycle(&f)==0 && !snapshot(&f).safe_boundary);
        const struct emuif_observation *flow=last_kind(&f,returning ? 7 : 6);
        assert(flow && (flow->known&EMUIF_OBS_KNOWN_CONTEXT));
        uint64_t context=flow->context_id, attempt=flow->instruction_id;
        struct emuif_observed_input input; INIT(input); INIT(input.input);
        input.parent_stimulus_id=1; input.input.order=1;
        input.input.cycle=snapshot(&f).cycle; input.input.kind=EMUIF_INPUT_INTERRUPT;
        input.input.index=0; input.input.value=0; input.input.mask=1;
        assert(obs.deliver(f.cpu,&input)==0);
        assert(cycle(&f)==0 && snapshot(&f).safe_boundary);
        const struct emuif_observation *request=last_kind(&f,8),*retire=last_kind(&f,2);
        assert(request && request->instruction_id==attempt && retire->instruction_id==attempt);
        assert(request->sequence>flow->sequence && request->sequence<retire->sequence);
        assert(!count_kind(&f,9));
        uint64_t retired_sequence=retire->sequence;
        assert(step(&f)==0); const struct emuif_observation *entry=last_kind(&f,9);
        assert(entry && entry->sequence>retired_sequence);
        assert(entry->parent_context_id==(returning ? 0 : context));
        uint64_t irq=entry->context_id;
        assert(step(&f)==0); const struct emuif_observation *exit=last_kind(&f,10);
        assert(exit && exit->context_id==irq && (exit->known&EMUIF_OBS_KNOWN_CONTEXT));
        if(!returning) {
            assert(step(&f)==0); exit=last_kind(&f,7);
            assert(exit->context_id==context && (exit->known&EMUIF_OBS_KNOWN_CONTEXT));
        }
        finish(&f);
    }
}
static void test_sm2_and_rmw(void) {
    struct fixture f;
    uint8_t code[sizeof(uart_code)]; memcpy(code,uart_code,sizeof(code)); code[11]=0xf0;
    start(&f,code,sizeof(code),2,1); for(unsigned i=0;i<4;++i) assert(step(&f)==0);
    assert(rx(&f,9,1,0x55,0)==0); cycles(&f,220);
    assert(count_kind(&f,19)==0 && last_kind(&f,20)->reason==EMUIF_OBS_REASON_SM2); finish(&f);
    const uint8_t rmw[]={0x75,0x98,0xc0,0x05,0x99};
    start(&f,rmw,sizeof(rmw),2,1); assert(step(&f)==0); assert(step(&f)==0);
    const struct emuif_observation *a=last_kind(&f,4),*r=last_kind(&f,5),*h=last_kind(&f,22);
    assert(a && r && h && a->access==EMUIF_OBS_ACCESS_RMW && r->access==EMUIF_OBS_ACCESS_RMW);
    assert(a->access_id==r->access_id && r->access_id==h->access_id);
    assert(r->old_value.known && r->new_value.known && ((r->old_value.value+1)&255)==r->new_value.value);
    finish(&f);
}
#ifdef EMU_CPU_FACADE_TESTING
extern void emu_cpu_test_observation_limits(struct emuif_cpu *, uint64_t, uint64_t);
extern void emu_cpu_test_context_limit(struct emuif_cpu *, uint64_t);
static void test_exhaustion(void) {
    struct fixture f;
    const uint8_t code[]={0x75,0x98,0xd0,0x75,0x99,0x55};
    start(&f,code,sizeof(code),2,1); assert(step(&f)==0);
    emu_cpu_test_observation_limits(f.cpu,UINT64_MAX,0);
    assert(rx(&f,1,1,0x55,0)==EMUIF_EXHAUSTED);
    assert(count_kind(&f,18)==0 && count_kind(&f,20)==0 && snapshot(&f).state==EMUIF_STATE_PAUSED);
    assert(step(&f)==EMUIF_EXHAUSTED);
    assert(count_kind(&f,22)==0 && count_kind(&f,3)==1);
    assert(last_kind(&f,3)->reason==EMUIF_OBS_REASON_EXHAUSTED); finish(&f);
    const uint8_t movx[]={0xe0}; start(&f,movx,sizeof(movx),2,1);
    emu_cpu_test_observation_limits(f.cpu,0,UINT64_MAX);
    assert(step(&f)==EMUIF_EXHAUSTED); assert(count_kind(&f,4)==0 && count_kind(&f,5)==0);
    assert(snapshot(&f).a==0); finish(&f);
    for(unsigned op=0;op<3;++op) {
        uint8_t call[]={op==0 ? 0x12 : op==1 ? 0x11 : 0x22,0,0};
        start(&f,call,sizeof(call),2,1);
        uint8_t before=snapshot(&f).sp; emu_cpu_test_context_limit(f.cpu,UINT64_MAX);
        assert(step(&f)==EMUIF_EXHAUSTED);
        assert(count_kind(&f,6)==0 && count_kind(&f,7)==0 && count_kind(&f,2)==0 && count_kind(&f,3)==1);
        if(op<2) assert(snapshot(&f).sp==before); /* calls stop before stack writes */
        finish(&f);
    }
    /* IRQ allocation exhaustion also stops before pushes and emits no enter. */
    uint8_t irq[512]={2,1,0};
    const uint8_t enable[]={0x75,0x81,0x40,0x75,0xa8,0x81,0x75,0x88,0x03,0x80,0xfe};
    memcpy(irq+256,enable,sizeof(enable)); start(&f,irq,sizeof(irq),2,1);
    for(unsigned i=0;i<4;++i) assert(step(&f)==0);
    uint8_t before=snapshot(&f).sp; emu_cpu_test_context_limit(f.cpu,UINT64_MAX);
    assert(step(&f)==EMUIF_EXHAUSTED); assert(count_kind(&f,9)==0 && snapshot(&f).sp==before);
    finish(&f);
    /* A mismatched return cannot recycle the known CALL token when allocating
     * an unknown context fails. RETI without actual hardware IRQ is legal here. */
    uint8_t mismatch[32]={0x12,0,16}; mismatch[16]=0x32;
    start(&f,mismatch,sizeof(mismatch),2,1); assert(step(&f)==0);
    emu_cpu_test_context_limit(f.cpu,UINT64_MAX); assert(step(&f)==EMUIF_EXHAUSTED);
    assert(count_kind(&f,10)==0 && count_kind(&f,3)==1); finish(&f);
}
#endif
int main(void) {
    INIT(api); INIT(obs); assert(emusa80535_cpu_query_v1(&api)==0);
    assert(emusa80535_cpu_observation_query_v1(&obs)==0);
    assert(obs.coverage==125 && obs.bind && obs.unbind && obs.deliver);
    test_prefix(); test_instruction(); test_context(); test_uart(); test_reset(); test_irq_and_gap(); test_context_recovery(); test_context_tail_irq(); test_sm2_and_rmw();
#ifdef EMU_CPU_FACADE_TESTING
    test_exhaustion();
#endif
    puts("CPU observations: prefixes, all opcodes/equivalence, retirement/faults, contexts, UART/correlation/reset PASS");
    return 0;
}
