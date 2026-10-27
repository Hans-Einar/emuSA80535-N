#!/usr/bin/env python3
"""Compile isolated public consumers and verify schema/export integrity gates."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
import bundle
import jsonschema
import wire

ROOT = bundle.ROOT
VERSION_ROOT = ROOT / "interfaces" / bundle.VERSION


def reject(action, label):
    try:
        action()
    except (AssertionError, ValueError, jsonschema.ValidationError):
        return
    raise AssertionError("negative check unexpectedly accepted: " + label)


def main():
    manifest = bundle.checked_manifest(ROOT)
    for ref in manifest["references"]:
        assert bundle.sha((ROOT / ref["path"]).read_bytes()) == ref["sha256"]
        assert re.fullmatch("[0-9a-f]{40}", ref["sourceCommit"])
    for path in VERSION_ROOT.rglob("*.md"):
        for target in re.findall(r"\]\(([^)]+)\)", path.read_text()):
            if "://" in target or target.startswith("#"):
                continue
            assert (path.parent / target.split("#")[0]).exists(), (path, target)
    checker = jsonschema.FormatChecker()
    @checker.checks("uint64")
    def uint64(value):
        return isinstance(value, str) and re.fullmatch(r"0|[1-9][0-9]*", value) is not None and int(value) <= 18446744073709551615
    schema = json.loads((VERSION_ROOT / "schemas/runtime-2.0.schema.json").read_text())
    jsonschema.Draft202012Validator.check_schema(schema)
    validator = jsonschema.Draft202012Validator(schema, format_checker=checker)
    examples = [bundle.load_json(line) for line in (VERSION_ROOT / "examples/runtime-2.0.ndjson").read_bytes().splitlines()]
    for example in examples:
        wire.validate(example, schema)
        assert len(json.dumps(example, separators=(",", ":")).encode()) + 1 <= 65536
    bad = copy.deepcopy(examples[0]); bad["arguments"]["protocol"]["major"] = 1
    reject(lambda: validator.validate(bad), "wire major")
    bad = copy.deepcopy(examples[3]); bad["body"]["generation"] = 1
    reject(lambda: validator.validate(bad), "numeric uint64")
    bad["body"]["generation"] = "18446744073709551616"
    reject(lambda: validator.validate(bad), "uint64 overflow")
    bad = copy.deepcopy(examples[0]); bad["command"] = "unknownCommand"
    reject(lambda: validator.validate(bad), "unknown command requires close without response")
    unknown_error = {"type": "response", "id": 1, "command": "unknownCommand",
                     "success": False, "error": {"code": "INVALID", "message": "unknown", "retryable": False}}
    reject(lambda: validator.validate(unknown_error), "unknown command has no error envelope")
    bad = copy.deepcopy(examples[0]); bad["extra"] = True
    reject(lambda: validator.validate(bad), "unknown field")
    bad = copy.deepcopy(examples[4]); bad["arguments"]["addresses"] = list(range(1025))
    reject(lambda: validator.validate(bad), "breakpoint bound")
    reject(lambda: bundle.load_json(b'{"id":1,"id":2}'), "duplicate JSON key")
    reject(lambda: bundle.load_json(b'{"payload":{"x":NaN}}'), "nonfinite parsed JSON")
    bad = copy.deepcopy(examples[6]); bad["arguments"]["payload"]["x"] = float("inf")
    reject(lambda: wire.validate(bad, schema), "nonfinite payload")
    bad = copy.deepcopy(examples[17]); bad["body"]["records"][0]["text"] = "ø" * 96
    reject(lambda: wire.validate(bad, schema), "UTF-8 text byte limit")
    with tempfile.TemporaryDirectory(prefix="emu-interface-check-") as temp:
        work = Path(temp)
        include = work / "include"
        shutil.copytree(ROOT / "lib/include", include)
        source = VERSION_ROOT / "examples/consumer.c"
        for compiler, standard, suffix in [(os.environ.get("CC", "cc"), "c99", ".c"), (os.environ.get("CXX", "c++"), "c++17", ".cpp")]:
            command = shlex.split(compiler) + ["-std=" + standard, "-Wall", "-Wextra", "-Werror", "-Wno-missing-field-initializers", "-I" + str(include), "-fsyntax-only"]
            for header in sorted(include.rglob("*.h")):
                unit = work / ("header" + suffix)
                unit.write_text("#include <" + header.relative_to(include).as_posix() + ">\nint main(void) { return 0; }\n")
                subprocess.run(command + [str(unit)], cwd=work, check=True)
            unit = work / ("consumer" + suffix)
            unit.write_bytes(source.read_bytes())
            subprocess.run(command + [str(unit)], cwd=work, check=True)
        # Source archive profile has no Git; use a labeled synthetic provenance.
        fixture = work / "source"
        for name in manifest["files"]:
            path = fixture / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes((ROOT / name).read_bytes())
        (fixture / bundle.MANIFEST).write_bytes((ROOT / bundle.MANIFEST).read_bytes())
        (fixture / "SOURCE_COMMIT").write_text("a" * 40 + "\n")
        a, b = work / "a.tar.gz", work / "b.tar.gz"
        bundle.export(fixture, a); bundle.export(fixture, b)
        assert a.read_bytes() == b.read_bytes(), "nondeterministic export"
        files = bundle.archive_files(a)
        lock = {**bundle.verify_files(files), "archiveSha256": bundle.sha(a.read_bytes()), "copyPath": "vendor/example", "consumer": "fixture"}
        destination = work / "copy"
        for name, data in files.items():
            path = destination / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        bundle.verify_copy(destination, lock)
        header = destination / "lib/include/emusa80535/contracts/board.h"
        original = header.read_bytes()
        header.write_bytes(original + b"\n/* modified */\n")
        reject(lambda: bundle.verify_copy(destination, lock), "modified copied header")
        header.write_bytes(original)
        header.unlink()
        reject(lambda: bundle.verify_copy(destination, lock), "missing copied header")
        header.write_bytes(original)
        (destination / "extra.txt").write_text("unexpected")
        reject(lambda: bundle.verify_copy(destination, lock), "extra copied payload")
        (destination / "extra.txt").unlink()
        bad_lock = {**lock, "sourceCommit": "b" * 40}
        reject(lambda: bundle.verify_copy(destination, bad_lock), "wrong source lock")
        bad_lock = {**lock, "archiveSha256": "0" * 64}
        reject(lambda: bundle.verify_copy(destination, bad_lock), "wrong archive hash lock")
        changed = dict(files); changed["extra.txt"] = b"x"
        reject(lambda: bundle.verify_files(changed), "extra archive payload")
        (fixture / "lib/include/emusa80535/contracts/board.h").write_bytes(original + b"x")
        reject(lambda: bundle.checked_manifest(fixture), "stale manifest")
        optimized = subprocess.run([sys.executable, "-O", str(ROOT / "interfaces/tools/bundle.py"), "--root", str(fixture), "check"], capture_output=True)
        assert optimized.returncode != 0, "optimized mode bypassed integrity checks"
    print("PASS: isolated C99/C++17 headers, strict wire examples/negative cases, references/links, deterministic source-archive export and copy integrity")


if __name__ == "__main__":
    main()
