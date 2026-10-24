"""Re-run existing reconciliation gates; does not modify repository sources."""
import argparse
import datetime
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import time

p = argparse.ArgumentParser()
p.add_argument('--repo', required=True)
p.add_argument('--output', required=True)
p.add_argument('--head', required=True)
p.add_argument('--compilers', nargs='+', default=['gcc', 'clang'], choices=['gcc', 'clang'])
p.add_argument('--prepare-only', action='store_true', help='Validate and extract exact sources without compiling')
a = p.parse_args()
object_repo, output = Path(a.repo).resolve(), Path(a.output).resolve()
output.mkdir(parents=True, exist_ok=True)
resolved_head = subprocess.check_output(
    ['git', '-C', str(object_repo), 'rev-parse', '--verify', a.head + '^{commit}'], text=True).strip()
if resolved_head != a.head:
    raise SystemExit('--head must be an exact full commit SHA')
archive = subprocess.check_output(
    ['git', '-c', 'core.autocrlf=false', '-c', 'core.eol=lf', '-C', str(object_repo),
     'archive', '--format=tar', resolved_head])
repo = output / 'source'
if repo.exists():
    raise SystemExit('Use a fresh --output directory: source snapshot already exists')
repo.mkdir()
with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
    tar.extractall(repo, filter='data')
snapshot_identity = {'head': resolved_head, 'git_repository': str(object_repo),
                     'archive_sha256': hashlib.sha256(archive).hexdigest(),
                     'source_snapshot': str(repo)}
(output / 'source-identity.json').write_text(json.dumps(snapshot_identity, indent=2))
if a.prepare_only:
    print(json.dumps(snapshot_identity), flush=True)
    raise SystemExit(0)
windows = os.name == 'nt'
results = []
env = os.environ.copy()
if windows:
    resource_dir = subprocess.check_output(['clang', '--print-resource-dir'], text=True).strip()
    env['PATH'] = str(Path(resource_dir) / 'lib/windows') + os.pathsep + env['PATH']
env['ASAN_OPTIONS'] = 'halt_on_error=1' + ('' if windows else ':detect_leaks=1')
env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
metadata = {'head': a.head, 'repository': str(repo), 'platform': platform.platform(),
            'python': sys.version, 'python_executable': sys.executable,
            'started': datetime.datetime.now(datetime.timezone.utc).isoformat()}
metadata['source_identity'] = snapshot_identity
metadata['sanitizer_options'] = {key: env[key] for key in ['ASAN_OPTIONS', 'UBSAN_OPTIONS']}
if windows:
    metadata['asan_runtime_directory'] = str(Path(resource_dir) / 'lib/windows')
(output / 'environment.json').write_text(json.dumps(metadata, indent=2))

def run(name, command, cwd=output):
    start = time.monotonic()
    try:
        result = subprocess.run([str(x) for x in command], cwd=str(cwd), env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=240)
        code, content = result.returncode, result.stdout
    except subprocess.TimeoutExpired as exc:
        code, content = 124, (exc.stdout or b'') + b'\nTIMEOUT\n'
    (output / (name + '.log')).write_bytes(content)
    results.append({'name': name, 'command': [str(x) for x in command], 'cwd': str(cwd),
                    'exit_code': code, 'seconds': round(time.monotonic() - start, 3),
                    'log': name + '.log'})
    (output / 'results.json').write_text(json.dumps(results, indent=2))
    print(('PASS' if code == 0 else 'FAIL') + ' ' + name, flush=True)
    if code:
        print(content.decode('utf-8', errors='replace')[-6000:], flush=True)
    return code == 0

core = ['core.c', 'opcodes.c', 'disasm.c', 'binary_loader.c']
focused = {
    'debug_event': ['emu_debug_event.c'],
    'debug_trace': ['emu_debug_trace.c'],
    'debug_runtime': ['emu_debug_runtime.c', 'emu_debug_trace.c', 'emu_debug_event.c'],
}
regressions = ['stage0', 'stage1_irq', 'stage1_timers', 'stage1_uart',
               'stage2_ports', 'stage2_edges', 'stage3_adc', 'stage4_timer2']
strict = ['-std=c99', '-Wall', '-Wextra', '-Werror', '-pedantic', '-Wshadow',
          '-Wconversion', '-Wsign-conversion']
ordinary = ['-std=c99', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wshadow', '-Werror', '-pedantic']
if windows:
    strict += ['-D_CRT_SECURE_NO_WARNINGS']
    ordinary += ['-D_CRT_SECURE_NO_WARNINGS']
suffix = '.exe' if windows else ''
for compiler in a.compilers:
    run(compiler + '-version', [compiler, '--version'])
    for name, sources in list(focused.items()) + [(n, core) for n in regressions] + [('debug_facade', ['emu_debug.c'] + core)]:
        executable = output / (compiler + '-' + name + suffix)
        flags = strict if name in focused else ordinary
        extra = ['-Wl,/STACK:8388608'] if windows and compiler == 'clang' else []
        command = [compiler] + flags + ['-O2', '-g'] + [repo / ('tests/test_' + name + '.c')] + [repo / s for s in sources] + ['-o', executable] + extra
        if run(compiler + '-' + name + '-build', command):
            run(compiler + '-' + name + '-run', [executable])
    server = output / (compiler + '-emu-debug' + suffix)
    command = [compiler] + ordinary + ['-O2', '-g', '-DEMU_DEBUG_BUILD_COMMIT="' + a.head + '"'] + [repo / s for s in ['emu_debug_server.c', 'emu_debug.c'] + core] + ['-o', server]
    if run(compiler + '-server-build', command):
        run(compiler + '-ndjson-process', [sys.executable, repo / 'tests/test_emu_debug_process.py', server])

for name, sources in list(focused.items()) + [(n, core) for n in regressions] + [('debug_facade', ['emu_debug.c'] + core)]:
    executable = output / ('san-' + name + suffix)
    flags = strict if name in focused else ordinary
    command = ['clang'] + flags + ['-O0', '-g', '-fno-inline', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] + [repo / ('tests/test_' + name + '.c')] + [repo / s for s in sources] + ['-o', executable]
    if windows:
        command += ['-Wl,/STACK:8388608']
    if run('san-' + name + '-build', command):
        run('san-' + name + '-run', [executable])

for source in ['emu_debug_event.c', 'emu_debug_trace.c', 'emu_debug_runtime.c']:
    run('analyze-' + source, ['clang', '--analyze'] + strict + [repo / source, '-o', output / (source + '.plist')])

valgrind = shutil.which('valgrind')
if valgrind:
    for name in focused:
        run('valgrind-' + name, [valgrind, '--error-exitcode=99', '--leak-check=full', output / ('gcc-' + name + suffix)])
else:
    (output / 'valgrind.txt').write_text('NOT_AVAILABLE: valgrind not present on PATH; where-available gate.\n')
failures = [r['name'] for r in results if r['exit_code']]
print(json.dumps({'total_steps': len(results), 'failures': failures, 'head': a.head}), flush=True)
sys.exit(bool(failures))
