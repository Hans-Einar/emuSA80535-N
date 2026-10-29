#!/usr/bin/env python3
"""Deterministic private host file tests and actual JSONL contract validation."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'interfaces/tools'))
import target_wire
SCHEMA = json.loads((ROOT / 'interfaces/0.3.0/schemas/trace-file.schema.json').read_text())


def main():
    with tempfile.TemporaryDirectory(prefix='emusa-file-validation-') as tmp:
        root = Path(tmp)
        binary = root / 'file-test'
        evidence = root / 'evidence'
        evidence.mkdir(mode=0o700)
        command = shlex.split(os.environ.get('CC', 'cc')) + shlex.split(os.environ.get('CFLAGS', '-O2'))
        command += ['-std=c99', '-Wall', '-Wextra', '-Werror', '-DRUNTIME_FILE_TESTING', '-I'+str(ROOT), '-I'+str(ROOT/'lib/include'), '-pthread']
        if os.environ.get('FILE_SINK_SANITIZE'):
            command += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command += ['-o', str(binary)] + [str(ROOT/p) for p in ['tests/test_file_sink.c', 'runtime/file_sink.c', 'emu_outcome_internal.c', 'emu_json_internal.c', 'emu_sha_internal.c']]
        subprocess.run(command, check=True)
        subprocess.run([str(binary), str(evidence)], check=True, timeout=30)
        subprocess.run([str(binary), '--blocked-shutdown', str(evidence)], check=True, timeout=5)
        segments = []
        for path in sorted(evidence.glob('*/*.jsonl')):
            # Deliberate partial output, retained existing data and FIFO are not
            # complete JSONL artifacts; never attempt to open the FIFO.
            if path.parent.name in {'existing', 'partial', 'fsync', 'fifo', 'exhaustion', 'summary-exhaustion'}:
                continue
            raw = path.read_bytes()
            assert raw.endswith(b'\n') and len(raw) <= 65536
            rows = [target_wire.validate_frame(line+b'\n', SCHEMA) for line in raw.splitlines()]
            header, footer = rows[0], rows[-1]
            assert header['type'] == 'header' and footer['type'] == 'footer'
            canonical = json.dumps(header['configuration'], sort_keys=True, separators=(',', ':'), ensure_ascii=False).encode()
            assert hashlib.sha256(canonical).hexdigest() == header['traceConfigSha256']
            assert int(footer['records']) == len(rows)-2 and footer['complete']
            assert path.stat().st_mode & 0o777 == 0o600
            segments.append(dict(path=str(path.relative_to(evidence)), bytes=len(raw), lines=rows))
        basic = [row['record'] for seg in segments if seg['path'].startswith('basic/') for row in seg['lines'] if row['type']=='record']
        ids = [int(row['recordSequence']) for row in basic]
        assert ids == sorted(set(ids))
        losses = [row for row in basic if row['kind']==2]
        assert [int(row['count']) for row in losses] == [1, 45, 1]
        exhausted = (evidence/'exhaustion/capture0.0000.jsonl').read_bytes().splitlines()
        exhausted_rows = [target_wire.validate_frame(line+b'\n', SCHEMA) for line in exhausted]
        assert exhausted_rows[1]['record']['recordSequence'] == str(2**64-1)
        if os.environ.get('FILE_SINK_REPORT'):
            Path(os.environ['FILE_SINK_REPORT']).write_text(json.dumps(dict(segments=segments, exhaustion=exhausted_rows), indent=2)+'\n')
        print(f'file sink: {sum(len(x["lines"]) for x in segments)} JSONL lines validated; bounded shutdown, barriers, limits, paths, failures and identities passed')


if __name__ == '__main__':
    main()
