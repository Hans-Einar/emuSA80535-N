#!/usr/bin/env python3
"""Real persistent Unix processes, synthetic ABI1 board, frozen wire3 schemas."""
import contextlib
import hashlib
import json
import os
from pathlib import Path
import socket
import stat
import subprocess
import tempfile
import time
import jsonschema

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get('RUNTIME_BIN', ROOT / 'emu-runtime')).resolve()
SCHEMA = json.loads((ROOT / 'interfaces/0.3.0/schemas/runtime-3.0.schema.json').read_text())
FORMATS = jsonschema.FormatChecker()
@FORMATS.checks('uint64')
def uint64(value):
    return not isinstance(value, str) or (value.isascii() and value.isdigit() and str(int(value)) == value and int(value) <= 2**64 - 1)
VALIDATOR = jsonschema.Draft202012Validator(SCHEMA, format_checker=FORMATS)
HELLO = {'protocol': {'major': 3, 'minor': 0}, 'contractVersion': '0.3.0', 'requiredCapabilities': []}

def wire(i, command, arguments):
    return (json.dumps({'type': 'request', 'id': i, 'command': command, 'arguments': arguments}, separators=(',', ':')) + '\n').encode()

def connect(path):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(3)
    s.connect(str(path))
    return s

class Client:
    def __init__(self, path):
        self.socket = connect(path)
        self.file = self.socket.makefile('rb')
        self.i = 0
    def response(self):
        line = self.file.readline()
        assert line and line.endswith(b'\n') and len(line) <= 65536, line
        value = json.loads(line)
        errors = list(VALIDATOR.iter_errors(value))
        assert not errors, (value, [e.message for e in errors])
        return value
    def call(self, command, args, error=None):
        self.i += 1
        self.socket.sendall(wire(self.i, command, args))
        response = self.response()
        assert response['id'] == self.i and response['command'] == command, response
        if error:
            assert not response['success'] and response['error']['code'] == error, response
            assert response['error']['retryable'] == (error == 'BUSY')
            return response
        assert response['success'], response
        return response['body']
    def hello(self): return self.call('hello', HELLO)
    def attach(self, role='controller'):
        self.snapshot = self.call('attach', {'sessionId': None, 'role': role})
        self.identity = {'sessionId': self.snapshot['sessionId']}
        self.generation = self.snapshot['generation']
        self.lease = self.snapshot['leaseId']
        return self.snapshot
    def args(self, **extra):
        return {**self.identity, 'generation': self.generation, 'leaseId': self.lease, **extra}
    def status(self): return self.call('status', self.identity)
    def close(self): self.file.close(); self.socket.close()

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    with tempfile.TemporaryDirectory(prefix='ctl3-', dir='/tmp') as temp:
        work = Path(temp)
        module = work / 'board.so'
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-shared', '-fPIC', '-O1', '-g', '-I' + str(ROOT / 'lib/include'), str(ROOT / 'tests/runtime/synthetic_board.c'), '-o', str(module)], check=True)
        image = work / 'image.bin'
        image.write_bytes(bytes.fromhex('9012347442f0e075903380fe') + bytes(65536 - 12))
        config = work / 'config.json'; config.write_bytes(b'{}')
        endpoint = work / 'control'
        def command(path=endpoint, mode='externally-driven'):
            return [str(BINARY), '--variant', 'sab80535', '--clock-hz', '11059200', '--seed', '0', '--entry', '0', '--image', str(image), '--image-sha256', digest(image), '--board', str(module), '--board-sha256', digest(module), '--board-config', str(config), '--control-unix', str(path), '--execution-mode', mode, '--board-inspect-fault']
        @contextlib.contextmanager
        def host(mode='externally-driven'):
            p = subprocess.Popen(command(mode=mode), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                deadline = time.monotonic() + 5
                while not endpoint.exists() and p.poll() is None and time.monotonic() < deadline: time.sleep(.01)
                assert p.poll() is None and endpoint.exists(), p.communicate()
                assert stat.S_IMODE(endpoint.stat().st_mode) == 0o600
                yield p
            finally:
                if p.poll() is None: p.terminate()
                out, err = p.communicate(timeout=5)
                assert p.returncode == 0 and not out, (p.returncode, out, err)
                assert not endpoint.exists()
        with host() as p:
            c = Client(endpoint); hello = c.hello()
            assert hello['executionMode'] == 'externally-driven'
            assert hello['protocol'] == {'major': 3, 'minor': 0}
            assert hello['contractManifestSha256'] == digest(ROOT / 'interfaces/0.3.0/manifest.json')
            assert hello['observationCoverage'] == [] and 'advance' in hello['capabilities']
            assert not set(hello['capabilities']) & {'observations', 'tracePage', 'captureA', 'captureB', 'uartOutcomes', 'traceFile'}
            initial = c.attach(); assert initial['cycle'] == '0'
            sid = initial['sessionId']; first_lease = c.lease
            observer = Client(endpoint); observer.hello(); observer.attach('observer')
            observer.call('resume', {**observer.args(), 'leaseId': first_lease}, 'DENIED')
            observer.call('acquireControl', {**observer.identity, 'generation': observer.generation}, 'BUSY')
            c.call('resume', c.args()); time.sleep(.05)
            assert c.status()['cycle'] == '0'
            progress = c.call('advance', c.args(cycleBudget=100))
            assert 100 <= int(progress['cycles']) <= 116
            assert progress['state'] == 'running'
            c.call('pause', c.args()); before = c.status()
            time.sleep(.05); assert c.status() == before
            inspection = c.call('boardInspect', {**c.identity, 'generation': c.generation, 'schema': 'synthetic.status.v1', 'payload': {}})
            assert inspection['payload']['accesses'] == '2'
            c.call('advance', c.args(cycleBudget=1), 'STATE')
            stepped = c.call('step', c.args(maxCycles=10)); assert int(stepped['cycle']) > int(before['cycle'])
            c.call('releaseControl', c.args()); observer.lease = observer.call('acquireControl', {**observer.identity, 'generation': observer.generation})['leaseId']
            assert int(observer.lease) > int(first_lease)
            c.call('resume', c.args(), 'DENIED')
            observer.call('resume', observer.args()); before = observer.status()
            observer.close(); time.sleep(.05)
            re = Client(endpoint); re.hello(); snap = re.attach()
            assert snap['sessionId'] == sid and snap['cycle'] == before['cycle'] and snap['state'] == 'running'
            assert int(re.lease) > int(observer.lease)
            re.call('pause', re.args())
            old = re.generation
            reset = re.call('reset', re.args(seed=0, entry=0)); re.generation = reset['generation']
            assert int(re.generation) == int(old) + 1
            re.call('resume', {**re.args(), 'generation': old}, 'STALE')
            re.call('boardInput', re.args(cycle=None, schema='synthetic.inputs.v1', payload={}))
            re.call('replaceCodeBreakpoints', re.args(addresses=[5]))
            re.call('resume', re.args()); progress = re.call('advance', re.args(cycleBudget=100))
            assert progress['reason'] == 'breakpoint' and progress['partial']
            detail = re.call('stopDetail', re.identity); assert detail['reason'] == 'breakpoint'
            re.call('traceConfig', re.identity, 'UNSUPPORTED')
            re.call('detach', re.identity); re.close(); c.close()
            ending = Client(endpoint); ending.hello(); ending.attach(); ending.call('terminate', ending.args()); ending.close()
            p.wait(timeout=5)
        # Host pumping continues while running and remains responsive to pause/status.
        with host('host-driven'):
            c = Client(endpoint); h = c.hello(); assert h['executionMode'] == 'host-driven' and 'advance' not in h['capabilities']
            c.attach(); c.call('resume', c.args()); time.sleep(.03)
            assert int(c.status()['cycle']) > 0
            c.call('advance', c.args(cycleBudget=1), 'UNSUPPORTED')
            c.call('pause', c.args()); before = c.status(); time.sleep(.03); assert c.status() == before
            c.close()
        # Faulted control stays responsive and actual partial progress survives.
        config.write_bytes(b'{"fault":true}')
        with host():
            c = Client(endpoint); c.hello(); c.attach(); c.call('resume', c.args())
            progress = c.call('advance', c.args(cycleBudget=100))
            assert progress['state'] == 'faulted' and progress['partial'] and progress['cycles'] == '3'
            snap = c.status(); assert not snap['safeBoundary']
            c.call('resume', c.args(), 'STATE')
            reset = c.call('reset', c.args(seed=0, entry=0)); assert reset['state'] == 'paused'
            c.close()
        config.write_bytes(b'{}')
        with host():
            # Fragmented exact maximum-size frame; no response before LF.
            s = connect(endpoint); raw = wire(1, 'hello', HELLO)
            raw = raw[:-1] + b' ' * (65536 - len(raw)) + b'\n'
            s.sendall(raw[:10]); s.settimeout(.05)
            try: s.recv(1); assert False, 'premature response'
            except socket.timeout: pass
            s.settimeout(3); s.sendall(raw[10:]); f = s.makefile('rb'); VALIDATOR.validate(json.loads(f.readline())); f.close(); s.close()
            # Malformed trustworthy request receives INVALID then closes.
            for raw in [wire(1, 'hello', {**HELLO, 'unknown': 1}), wire(1, 'hello', HELLO).replace(b'\n', b'\r\n')]:
                s = connect(endpoint); s.sendall(raw); f = s.makefile('rb'); r = json.loads(f.readline()); VALIDATOR.validate(r)
                assert r['error']['code'] == 'INVALID'; assert not f.readline(); f.close(); s.close()
            s = connect(endpoint); s.sendall(wire(1, 'hello', {**HELLO, 'protocol': {'major': 2, 'minor': 1}})); f = s.makefile('rb')
            r = json.loads(f.readline()); VALIDATOR.validate(r); assert r['error']['code'] == 'VERSION'; assert not f.readline(); f.close(); s.close()
            for raw in [b'{}\n', b'\xef\xbb\xbf' + wire(1, 'hello', HELLO), b'{"x":"\xff"}\n', b'x' * 65536]:
                s = connect(endpoint); s.sendall(raw); assert not s.recv(65536); s.close()
            peers = [Client(endpoint) for _ in range(8)]
            for x in peers: x.hello()
            extra = connect(endpoint); assert not extra.recv(1); extra.close()
            for x in peers: x.close()
            time.sleep(.05)
            # Incomplete hello, incomplete subsequent frame and unread output all
            # expire independently; another client remains responsive.
            idle = connect(endpoint)
            negotiating = Client(endpoint)
            negotiating.call('hello', {**HELLO, 'requiredCapabilities': ['notImplemented']}, 'UNSUPPORTED')
            partial = Client(endpoint); partial.hello(); partial.socket.sendall(b'{')
            slow = Client(endpoint); slow.hello(); slow.attach('observer')
            slow.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
            slow.socket.sendall(b''.join(wire(i, 'status', slow.identity) for i in range(3, 1003)))
            good = Client(endpoint); good.hello(); good.attach('observer'); assert good.status()['cycle'] == '0'
            time.sleep(2.7)
            partial.socket.sendall(b'"stillIncomplete"')
            negotiating.call('hello', {**HELLO, 'requiredCapabilities': ['notImplemented']}, 'UNSUPPORTED')
            time.sleep(2.7)
            assert not idle.recv(1); idle.close()
            assert not partial.file.readline(); partial.close()
            assert not negotiating.file.readline(); negotiating.close()
            # Drain queued responses after deadline until EOF; blocked output
            # must have expired before the 1000 queued requests could run.
            lines = 0
            try:
                while slow.file.readline(): lines += 1
            except ConnectionResetError:
                pass  # Closing a socket with unread pipelined input may reset it.
            assert 0 < lines < 1000, lines
            slow.close(); assert good.status()['cycle'] == '0'; good.close()
        # Static CLI/path/hash rejection never creates or removes another inode.
        def reject(args):
            r = subprocess.run(args, capture_output=True, timeout=5)
            assert r.returncode == 2 and not r.stdout, (r.returncode, r.stdout, r.stderr)
        endpoint.write_text('owned elsewhere'); reject(command()); assert endpoint.read_text() == 'owned elsewhere'; endpoint.unlink()
        endpoint.symlink_to(config); reject(command()); assert endpoint.is_symlink(); endpoint.unlink()
        alias = work / 'alias'; alias.symlink_to(work, target_is_directory=True); reject(command(alias / 'control')); alias.unlink()
        insecure = work / 'insecure'; insecure.mkdir(mode=0o755); reject(command(insecure / 'control'))
        reject(command(Path('relative'))); reject(command(work / ('x' * 108)))
        reject(command(mode='unknown'))
        reject(command() + ['--script', str(config)])
        args = command(); args[args.index('--execution-mode'):args.index('--execution-mode') + 2] = []; reject(args)
        # Shutdown only removes the exact socket inode the host created.
        p = subprocess.Popen(command(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 5
            while not endpoint.exists() and time.monotonic() < deadline: time.sleep(.01)
            assert endpoint.exists(); endpoint.unlink(); endpoint.write_text('replacement')
            p.terminate(); out, err = p.communicate(timeout=5)
            assert p.returncode == 0 and not out and endpoint.read_text() == 'replacement', (out, err)
        finally:
            if p.poll() is None: p.kill(); p.wait()
            endpoint.unlink(missing_ok=True)
    print('runtime control3: real Unix/schema/control/lease/reconnect/mode/fault/framing/bounds/timeout/path checks passed')

if __name__ == '__main__': main()
