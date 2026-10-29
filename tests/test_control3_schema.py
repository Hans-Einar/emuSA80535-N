#!/usr/bin/env python3
"""Validate actual control3 responses with the frozen strict contract validator."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'interfaces/tools'))
import target_wire
schema=json.loads((root/'interfaces/0.3.0/schemas/runtime-3.0.schema.json').read_text())
with tempfile.TemporaryDirectory() as directory:
    output=Path(directory)/'responses.ndjson'
    subprocess.run([str(root/'tests/control3_tests')],check=True,env={**os.environ,'CONTROL3_OUTPUT':str(output)})
    lines=output.read_bytes().splitlines(keepends=True)
    for line in lines:target_wire.validate_frame(line,schema)
    print(f'wire3.0 schema: {len(lines)} real responses passed')
