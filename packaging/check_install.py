#!/usr/bin/env python3
"""Check staged/installed artifacts without source-tree headers or libraries."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="DESTDIR, or / for installed packages")
    parser.add_argument("--prefix", default="/usr")
    parser.add_argument("--libdir", default="/usr/lib64")
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    prefix = root / args.prefix.lstrip("/")
    library = root / args.libdir.lstrip("/")
    docs = prefix / "share/doc/emusa80535"
    executable = prefix / "bin/emu-debug"
    assert (docs / "source-commit").read_text().strip() == args.commit
    assert (prefix / "share/licenses/emusa80535/LICENSE").is_file()
    env = os.environ.copy()
    env["PKG_CONFIG_LIBDIR"] = str(library / "pkgconfig")
    env["PKG_CONFIG_PATH"] = ""
    env["PKG_CONFIG_SYSROOT_DIR"] = str(root)
    flags = subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "emusa80535"], env=env, text=True)
    with tempfile.TemporaryDirectory(prefix="emusa80535-consumer-") as directory:
        source = Path(directory) / "consumer.c"
        source.write_bytes((docs / "sdk_smoke.c").read_bytes())
        binary = Path(directory) / "consumer"
        subprocess.run(shlex.split(env.get("CC", "cc")) +
                       ["-std=c99", "-Wall", "-Wextra", "-Werror", str(source),
                        "-o", str(binary)] + shlex.split(flags),
                       cwd=directory, check=True, env=env)
        subprocess.run([str(binary)], cwd=directory, check=True, timeout=10)
    subprocess.run([str(executable), "--version"], check=True, timeout=10)
    requests = [
        {"type": "request", "id": 1, "command": "hello",
         "arguments": {"protocol": {"major": 1, "minor": 0},
                       "requiredCapabilities": ["rawCode64k", "boundedRun"]}},
        {"type": "request", "id": 2, "command": "terminate"},
    ]
    result = subprocess.run([str(executable), "--headless-debug"],
                            input="".join(json.dumps(r) + "\n" for r in requests),
                            capture_output=True, text=True, check=True, timeout=10)
    responses = [json.loads(line) for line in result.stdout.splitlines()]
    assert len(responses) == 2 and all(r["success"] for r in responses), responses
    assert responses[0]["body"]["commit"] == args.commit, responses[0]
    assert responses[0]["body"]["protocol"] == {"major": 1, "minor": 0}
    assert not result.stderr, result.stderr
    print("installed executable: protocol 1.0 and source identity passed")


if __name__ == "__main__":
    main()
