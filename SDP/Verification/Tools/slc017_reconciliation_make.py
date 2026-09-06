"""Run the actual merged Makefile in an isolated exact-commit snapshot."""
import argparse
import io
import json
from pathlib import Path
import subprocess
import tarfile

p = argparse.ArgumentParser()
p.add_argument('--repo', required=True)
p.add_argument('--head', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
root = Path(a.output).resolve()
root.mkdir(parents=True, exist_ok=True)
snapshot = root / 'source'
snapshot.mkdir(exist_ok=True)
archive = subprocess.check_output(['git', '-c', 'core.autocrlf=false', '-c', 'core.eol=lf',
                                   '-C', a.repo, 'archive', '--format=tar', a.head])
with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
    tar.extractall(snapshot, filter='data')
command = ['make', '-C', str(snapshot / 'tests'), '-B', 'test', 'CC=gcc']
result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=240)
(root / 'make-test.log').write_bytes(result.stdout)
(root / 'result.json').write_text(json.dumps({'head': a.head, 'command': command, 'exit_code': result.returncode}, indent=2))
print(result.stdout.decode(errors='replace')[-4500:])
raise SystemExit(result.returncode)
