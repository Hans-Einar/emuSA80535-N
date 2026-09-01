#!/usr/bin/env python3
"""Cross-platform child-process contract tests for emu-debug 1.0."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


CAPABILITIES = [
    "rawCode64k",
    "deterministicReset",
    "snapshotBasicRegisters",
    "decodeCode",
    "replaceCodeBreakpoints",
    "boundedRun",
    "stepInstruction",
]


class Session:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable), "--headless-debug"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

    def request(self, request: dict[str, object]) -> dict[str, object]:
        assert self.process.stdin is not None
        assert self.process.stdout is not None
        encoded = json.dumps(request, ensure_ascii=True, separators=(",", ":")).encode()
        assert len(encoded) <= 65_536
        self.process.stdin.write(encoded + b"\n")
        self.process.stdin.flush()
        response_bytes = self.process.stdout.readline()
        assert response_bytes.endswith(b"\n"), response_bytes
        assert len(response_bytes) - 1 <= 65_536
        response = json.loads(response_bytes)
        assert response["type"] == "response"
        assert response["id"] == request["id"]
        assert response["command"] == request["command"]
        return response

    def close_input(self) -> tuple[int, bytes, bytes]:
        assert self.process.stdin is not None
        self.process.stdin.close()
        stdout = self.process.stdout.read() if self.process.stdout else b""
        stderr = self.process.stderr.read() if self.process.stderr else b""
        return self.process.wait(timeout=5), stdout, stderr


def hello(identifier: int = 1) -> dict[str, object]:
    return {
        "type": "request",
        "id": identifier,
        "command": "hello",
        "arguments": {
            "protocol": {"major": 1, "minor": 0},
            "requiredCapabilities": CAPABILITIES,
        },
        "ignoredMajorOneField": {"safe": True},
    }


def assert_error(response: dict[str, object], code: str) -> None:
    assert response["success"] is False, response
    error = response["error"]
    assert isinstance(error, dict)
    assert error["code"] == code, error
    assert isinstance(error["message"], str) and error["message"]
    assert error["retryable"] is False
    assert isinstance(error["data"], dict)
    assert "body" not in response


def main_flow(executable: Path, directory: Path) -> None:
    image = bytearray(65_536)
    image[:5] = bytes((0x74, 0x01, 0x04, 0x80, 0xFD))
    image[0x10] = 0xA5  # architecturally reserved opcode
    image[0x20:0x23] = bytes((0x43, 0x87, 0x01))  # ORL PCON,#IDL
    image_path = directory / "synthetic-ø-loop.bin"
    image_path.write_bytes(image)
    short_path = directory / "short.bin"
    short_path.write_bytes(image[:-1])
    digest = hashlib.sha256(image).hexdigest()

    session = Session(executable)
    response = session.request(hello())
    assert response["success"] is True
    body = response["body"]
    assert isinstance(body, dict)
    assert body["protocol"] == {"major": 1, "minor": 0}
    assert body["product"] == "emuSA80535-N"
    assert body["variants"] == ["sab80535"]
    assert body["capabilities"] == CAPABILITIES
    limits = body["limits"]
    assert isinstance(limits, dict)
    assert limits["maxBreakpoints"] >= 1
    assert limits["maxRunChunkInstructions"] >= 1
    assert limits["maxDisassembleInstructions"] >= 1
    assert limits["maxRecordBytes"] == 65_536

    assert_error(session.request({"type": "request", "id": 2, "command": "getState"}), "INVALID_STATE")
    assert_error(
        session.request(
            {
                "type": "request",
                "id": 3,
                "command": "load",
                "arguments": {
                    "path": str(short_path.resolve()),
                    "format": "raw-code-64k",
                    "expectedSha256": digest,
                },
            }
        ),
        "IMAGE_SIZE",
    )
    assert_error(
        session.request(
            {
                "type": "request",
                "id": 4,
                "command": "load",
                "arguments": {
                    "path": str(image_path.resolve()),
                    "format": "raw-code-64k",
                    "expectedSha256": "0" * 64,
                },
            }
        ),
        "IMAGE_HASH",
    )
    response = session.request(
        {
            "type": "request",
            "id": 5,
            "command": "load",
            "arguments": {
                "path": str(image_path.resolve()),
                "format": "raw-code-64k",
                "expectedSha256": digest,
            },
        }
    )
    assert response["success"] is True
    assert response["body"] == {"sha256": digest}

    reset_request = {
        "type": "request",
        "id": 6,
        "command": "reset",
        "arguments": {"seed": 525109, "entryAddress": 0},
    }
    first = session.request(reset_request)["body"]
    assert isinstance(first, dict)
    assert first["state"] == "idle"
    assert first["resultKind"] == "architectural-stop"
    assert first["reason"] == "entry"
    assert first["pc"] == 0
    assert first["variant"] == "sab80535"
    assert first["instructionCount"] == 0
    assert first["machineCycleCount"] == 0
    registers = first["registers"]
    assert isinstance(registers, dict)
    assert len(registers["r"]) == 8
    repeated_request = dict(reset_request)
    repeated_request["id"] = 7
    assert session.request(repeated_request)["body"] == first

    unknown = session.request(
        {
            "type": "request",
            "id": 8,
            "command": "decodeCode",
            "arguments": {
                "reference": 2,
                "byteOffset": 0,
                "instructionOffset": -1,
                "instructionCount": 3,
            },
        }
    )["body"]
    assert isinstance(unknown, dict)
    records = unknown["instructions"]
    assert isinstance(records, list) and len(records) == 3
    assert records[0] == {
        "address": 1,
        "size": 1,
        "valid": False,
        "text": "<invalid>",
        "reason": "unknown-predecessor",
    }
    forward = session.request(
        {
            "type": "request",
            "id": 9,
            "command": "decodeCode",
            "arguments": {
                "reference": 0,
                "byteOffset": 0,
                "instructionOffset": 0,
                "instructionCount": 4,
            },
        }
    )["body"]
    assert isinstance(forward, dict)
    records = forward["instructions"]
    assert isinstance(records, list) and len(records) == 4
    assert [(item["address"], item["size"], item["valid"]) for item in records] == [
        (0, 2, True),
        (2, 1, True),
        (3, 2, True),
        (5, 1, True),
    ]
    known = session.request(
        {
            "type": "request",
            "id": 10,
            "command": "decodeCode",
            "arguments": {
                "reference": 2,
                "byteOffset": 0,
                "instructionOffset": -1,
                "instructionCount": 2,
            },
        }
    )["body"]
    assert isinstance(known, dict)
    assert [item["address"] for item in known["instructions"]] == [0, 2]
    assert_error(
        session.request(
            {
                "type": "request",
                "id": 11,
                "command": "decodeCode",
                "arguments": {
                    "reference": 65_535,
                    "byteOffset": 1,
                    "instructionOffset": 0,
                    "instructionCount": 1,
                },
            }
        ),
        "RANGE",
    )

    response = session.request(
        {
            "type": "request",
            "id": 12,
            "command": "replaceCodeBreakpoints",
            "arguments": {"addresses": [2]},
        }
    )
    assert response["body"] == {"accepted": [2], "rejected": [], "limit": 1024}
    stopped = session.request(
        {
            "type": "request",
            "id": 13,
            "command": "run",
            "arguments": {"maxInstructions": 1024},
        }
    )["body"]
    assert isinstance(stopped, dict)
    assert stopped["reason"] == "breakpoint" and stopped["pc"] == 2
    assert stopped["instructionCount"] == 1
    stepped = session.request(
        {"type": "request", "id": 14, "command": "stepInstruction"}
    )["body"]
    assert isinstance(stepped, dict)
    assert stepped["reason"] == "step" and stepped["pc"] == 3
    assert stepped["registers"]["a"] == 2
    assert session.request({"type": "request", "id": 15, "command": "getState"})["body"] == stepped
    assert_error(
        session.request(
            {
                "type": "request",
                "id": 16,
                "command": "replaceCodeBreakpoints",
                "arguments": {"addresses": [3, 3]},
            }
        ),
        "INVALID_REQUEST",
    )
    yielded = session.request(
        {
            "type": "request",
            "id": 17,
            "command": "run",
            "arguments": {"maxInstructions": 1},
        }
    )["body"]
    assert isinstance(yielded, dict)
    assert yielded["resultKind"] == "yield" and yielded["reason"] == "yield"
    assert yielded["pc"] == 2
    stopped_again = session.request(
        {
            "type": "request",
            "id": 18,
            "command": "run",
            "arguments": {"maxInstructions": 1},
        }
    )["body"]
    assert isinstance(stopped_again, dict) and stopped_again["reason"] == "breakpoint"
    cleared = session.request(
        {
            "type": "request",
            "id": 19,
            "command": "replaceCodeBreakpoints",
            "arguments": {"addresses": []},
        }
    )
    assert cleared["body"] == {"accepted": [], "rejected": [], "limit": 1024}
    assert_error(
        session.request(
            {
                "type": "request",
                "id": 20,
                "command": "replaceCodeBreakpoints",
                "arguments": {"addresses": list(range(1025))},
            }
        ),
        "BREAKPOINT_LIMIT",
    )
    assert_error(
        session.request({"type": "request", "id": 20, "command": "getState"}),
        "INVALID_REQUEST",
    )
    assert_error(
        session.request({"type": "request", "id": 21, "command": "notACommand"}),
        "UNSUPPORTED_COMMAND",
    )
    before_invalid_run = session.request(
        {"type": "request", "id": 22, "command": "getState"}
    )["body"]
    assert_error(
        session.request(
            {
                "type": "request",
                "id": 23,
                "command": "run",
                "arguments": {"maxInstructions": 0},
            }
        ),
        "INVALID_REQUEST",
    )
    assert session.request(
        {"type": "request", "id": 24, "command": "getState"}
    )["body"] == before_invalid_run

    exception_reset = session.request(
        {
            "type": "request",
            "id": 25,
            "command": "reset",
            "arguments": {"seed": 525109, "entryAddress": 0x10},
        }
    )["body"]
    assert isinstance(exception_reset, dict) and exception_reset["pc"] == 0x10
    exception = session.request(
        {
            "type": "request",
            "id": 26,
            "command": "run",
            "arguments": {"maxInstructions": 1},
        }
    )["body"]
    assert isinstance(exception, dict)
    assert exception["reason"] == "exception"
    assert exception["resultKind"] == "architectural-stop"
    assert exception["exception"] == {
        "code": "EMU_EXCEPTION_ILLEGAL_OPCODE",
        "message": "illegal opcode",
    }

    session.request(
        {
            "type": "request",
            "id": 27,
            "command": "reset",
            "arguments": {"seed": 525109, "entryAddress": 0x20},
        }
    )
    halt = session.request(
        {"type": "request", "id": 28, "command": "stepInstruction"}
    )["body"]
    assert isinstance(halt, dict)
    assert halt["reason"] == "halt" and halt["resultKind"] == "architectural-stop"
    assert halt["pc"] == 0x23 and halt["instructionCount"] == 1
    terminated = session.request(
        {"type": "request", "id": 29, "command": "terminate"}
    )
    assert terminated["body"] == {"terminated": True}
    code, stdout, stderr = session.close_input()
    assert code == 0
    assert stdout == b""
    assert b"physical" not in stderr.lower()


def raw_case(executable: Path, payload: bytes, expected_code: int) -> tuple[bytes, bytes]:
    process = subprocess.run(
        [str(executable), "--headless-debug"],
        input=payload,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=5,
        check=False,
    )
    assert process.returncode == expected_code, (process.returncode, process.stderr)
    return process.stdout, process.stderr


def lifecycle_cases(executable: Path) -> None:
    session = Session(executable)
    response = session.request({"type": "request", "id": 1, "command": "getState"})
    assert_error(response, "INVALID_STATE")
    response = session.request(hello(2))
    assert_error(response, "INVALID_STATE")
    code, stdout, _ = session.close_input()
    assert code == 0 and stdout == b""

    stdout, stderr = raw_case(executable, b"{not-json}\n", 65)
    assert stdout == b"" and stderr
    stdout, stderr = raw_case(
        executable,
        b'{"type":"request" "id":1,"command":"hello"}\n',
        65,
    )
    assert stdout == b"" and stderr
    stdout, stderr = raw_case(
        executable,
        b'{"type":"request","id":1,"command":"hello\\u0000ignored","arguments":{}}\n',
        65,
    )
    assert stdout == b"" and stderr
    stdout, stderr = raw_case(executable, b"\xff\n", 65)
    assert stdout == b"" and stderr
    stdout, stderr = raw_case(executable, b"{}", 65)
    assert stdout == b"" and stderr
    stdout, _ = raw_case(executable, b"", 0)
    assert stdout == b""
    stdout, stderr = raw_case(executable, b"x" * 65_537 + b"\n", 65)
    assert stdout == b"" and stderr

    record = hello()
    record["padding"] = ""
    encoded = json.dumps(record, separators=(",", ":")).encode()
    record["padding"] = "x" * (65_536 - len(encoded))
    encoded = json.dumps(record, separators=(",", ":")).encode()
    assert len(encoded) == 65_536
    stdout, stderr = raw_case(executable, encoded + b"\n", 0)
    lines = stdout.splitlines()
    assert len(lines) == 1
    assert json.loads(lines[0])["success"] is True
    assert stderr == b""

    compatible_minor = hello()
    compatible_minor["arguments"]["protocol"]["minor"] = 7
    stdout, stderr = raw_case(
        executable,
        json.dumps(compatible_minor, separators=(",", ":")).encode() + b"\n",
        0,
    )
    response = json.loads(stdout)
    assert response["success"] is True
    assert response["body"]["protocol"] == {"major": 1, "minor": 0}
    assert stderr == b""

    unsupported = hello()
    unsupported["arguments"]["requiredCapabilities"].append("notSupported")
    stdout, stderr = raw_case(
        executable,
        json.dumps(unsupported, separators=(",", ":")).encode() + b"\n",
        0,
    )
    assert_error(json.loads(stdout), "UNSUPPORTED_CAPABILITY")
    assert stderr == b""

    incompatible = hello()
    incompatible["arguments"]["protocol"]["major"] = 2
    stdout, stderr = raw_case(
        executable,
        json.dumps(incompatible, separators=(",", ":")).encode() + b"\n",
        65,
    )
    response = json.loads(stdout)
    assert_error(response, "UNSUPPORTED_PROTOCOL")
    assert stderr == b""


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_emu_debug_process.py EMU_DEBUG")
    executable = Path(sys.argv[1]).resolve()
    assert executable.is_file(), executable
    version = subprocess.run(
        [str(executable), "--version"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=5,
        check=True,
    )
    assert b"emu-debug 1.0.0" in version.stdout and version.stderr == b""
    with tempfile.TemporaryDirectory(prefix="emu-debug-contract-") as temporary:
        main_flow(executable, Path(temporary))
    lifecycle_cases(executable)
    print("emu-debug process tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
