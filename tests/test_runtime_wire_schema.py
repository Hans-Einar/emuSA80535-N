#!/usr/bin/env python3
"""Validate actual native service output against immutable wire2.1 schema."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import jsonschema
root=Path(__file__).resolve().parents[1]
schema=json.loads((root/'interfaces/0.2.0/schemas/runtime-2.1.schema.json').read_text())
with tempfile.TemporaryDirectory() as directory:
    output=Path(directory)/'responses.ndjson'
    subprocess.run([str(root/'tests/runtime_session_tests')],check=True,env={**os.environ,'WIRE_OUTPUT':str(output)})
    validator=jsonschema.Draft202012Validator(schema)
    count=0
    for line in output.read_text().splitlines():
        validator.validate(json.loads(line));count+=1
    print(f'wire2.1 schema: {count} real responses passed')
