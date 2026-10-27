#!/usr/bin/env python3
"""Link isolated C/C++ API1 consumers, including the immutable 0.1 headers.

The historical gate uses the preserved upstream Git object, not a mutable sibling
checkout. Run from a Git checkout after `make cpu-sdk`.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASELINE = '7d924cc27e48c599d5775904543070d92e413c26'
SOURCE = r'''
#include <emusa80535/contracts/cpu.h>
#include <emusa80535/contracts/board.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    struct emuif_cpu_api api;
    memset(&api, 0, sizeof(api));
    api.struct_size = sizeof(api); api.api_version = 1;
    if (emusa80535_cpu_query_v1(&api) || !api.create || !api.destroy ||
        !api.reset || !api.load_code || !api.apply_input || !api.cycle ||
        !api.snapshot || !api.peek || !api.decode) return 1;
    printf("%zu %zu %zu %zu %zu %zu %zu %zu\n", sizeof(api),
        sizeof(struct emuif_board_api), sizeof(struct emuif_board_host),
        sizeof(struct emuif_board_create), sizeof(struct emuif_bus_access),
        sizeof(struct emuif_input), offsetof(struct emuif_cpu_api, decode),
        offsetof(struct emuif_board_api, inspect));
    return 0;
}
'''

# A deliberately minimal ABI fixture, not a runtime board implementation.
BOARD = r'''
#include <emusa80535/contracts/board.h>
#include <string.h>
static emuif_status access_fixture(void *ctx, const struct emuif_bus_access *access,
                                  struct emuif_bus_result *result) {
    (void)ctx;
    if (access->sequence != 42 || access->port_latches[5] != 0xa5) return EMUIF_FAULT;
    result->value = 0x5a;
    return EMUIF_OK;
}
emuif_status emusa80535_board_query_v1(struct emuif_board_api *out) {
    if (out->struct_size != sizeof(*out) || out->api_version != 1) return EMUIF_VERSION;
    out->abi_major=1; out->abi_minor=0; out->platform=1;
    strcpy(out->module_id,"old-header-abi-fixture");
    out->access=access_fixture;
    return EMUIF_OK;
}
'''
HOST = r'''
#include <emusa80535/contracts/board.h>
#include <dlfcn.h>
#include <string.h>
int main(int argc, char **argv) {
    void *handle;
    void *symbol;
    emuif_board_query_fn query;
    struct emuif_board_api api;
    struct emuif_bus_access access;
    struct emuif_bus_result result;
    if(argc!=2) return 1;
    handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL); if(!handle)return 2;
    symbol=dlsym(handle,EMUIF_BOARD_ENTRY_SYMBOL);
    if(!symbol || sizeof(symbol)!=sizeof(query))return 3;
    memcpy(&query,&symbol,sizeof(query));
    memset(&api,0,sizeof(api));api.struct_size=sizeof(api);api.api_version=1;
    if(query(&api)||api.abi_major!=1||api.abi_minor||api.platform!=1||
       strcmp(api.module_id,"old-header-abi-fixture")||!api.access)return 4;
    memset(&access,0,sizeof(access));access.struct_size=sizeof(access);access.api_version=1;
    access.sequence=42;access.port_latches[5]=0xa5;
    memset(&result,0,sizeof(result));result.struct_size=sizeof(result);result.api_version=1;
    if(api.access(0,&access,&result)||result.value!=0x5a)return 5;
    return dlclose(handle)!=0;
}
'''

def run():
    outputs = []
    with tempfile.TemporaryDirectory(prefix='emusa-cpu-link-') as temp:
        work = Path(temp)
        old = work / 'old/include/emusa80535/contracts'
        old.mkdir(parents=True)
        for header in (ROOT / 'lib/include/emusa80535/contracts').glob('*.h'):
            content = subprocess.check_output(['git', '-C', str(ROOT), 'show',
                BASELINE + ':lib/include/emusa80535/contracts/' + header.name])
            (old / header.name).write_bytes(content)
        for profile, include in [('current', ROOT / 'lib/include'), ('0.1', old.parents[1])]:
            for language, compiler, standard in [('c', os.environ.get('CC', 'cc'), 'c99'),
                                                ('cpp', os.environ.get('CXX', 'c++'), 'c++17')]:
                source = work / (profile + '.' + language)
                source.write_text(SOURCE)
                binary = work / (profile + '-' + language)
                subprocess.run(shlex.split(compiler) + ['-std=' + standard, '-Wall', '-Wextra',
                    '-Werror', '-I' + str(include), str(source), str(ROOT / 'libemusa80535_cpu.a'),
                    '-o', str(binary)], check=True, cwd=work)
                outputs.append(subprocess.check_output([str(binary)]))
        assert len(set(outputs)) == 1, 'API/board ABI layout changed'
        board_source, host_source = work / 'old-board.c', work / 'new-host.c'
        board_source.write_text(BOARD); host_source.write_text(HOST)
        board, host = work / 'old-board.so', work / 'new-host'
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        subprocess.run(compiler + ['-std=c99', '-Wall', '-Wextra', '-Werror', '-shared',
            '-fPIC', '-I' + str(old.parents[1]), str(board_source), '-o', str(board)], check=True)
        subprocess.run(compiler + ['-std=c99', '-Wall', '-Wextra', '-Werror',
            '-I' + str(ROOT / 'lib/include'), str(host_source), '-ldl', '-o', str(host)], check=True)
        subprocess.run([str(host), str(board)], check=True)
    print('PASS: isolated C99/C++17 current and immutable 0.1 CPU/board headers link; layouts match; old-header .so query/access works')

if __name__ == '__main__':
    run()
