#!/usr/bin/env python3
"""Create a reproducible RPM source archive from clean committed HEAD."""
import argparse
import gzip
import io
from pathlib import Path
import subprocess
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="output directory outside the checkout")
    args = parser.parse_args()
    root = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"],
                                        text=True).strip()).resolve()
    output = args.output.resolve()
    if output == root or root in output.parents:
        parser.error("output directory must be outside the checkout")
    status = subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=all"],
                                     cwd=root, text=True)
    if status:
        parser.error("commit or remove pending changes before creating release sources")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    timestamp = int(subprocess.check_output(["git", "show", "-s", "--format=%ct", "HEAD"],
                                            cwd=root, text=True).strip())
    version = "1.0.0"
    prefix = "emusa80535-" + version
    data = subprocess.check_output(["git", "archive", "--format=tar", "--prefix=" + prefix + "/", commit],
                                   cwd=root)
    stream = io.BytesIO(data)
    with tarfile.open(fileobj=stream, mode="a") as archive:
        provenance = (commit + "\n").encode("ascii")
        info = tarfile.TarInfo(prefix + "/SOURCE_COMMIT")
        info.size = len(provenance)
        info.mode = 0o644
        info.mtime = timestamp
        archive.addfile(info, io.BytesIO(provenance))
    output.mkdir(parents=True, exist_ok=True)
    target = output / (prefix + ".tar.gz")
    with target.open("wb") as destination:
        with gzip.GzipFile(filename="", mode="wb", fileobj=destination, mtime=0) as compressed:
            compressed.write(stream.getvalue())
    print(str(target))
    print("source commit: " + commit)


if __name__ == "__main__":
    main()
