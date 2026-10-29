#!/usr/bin/env python3
"""Contract-fixture validation, not a runtime transport implementation."""
import json
import re
import jsonschema


def validate(message, schema):
    checker = jsonschema.FormatChecker()
    @checker.checks("uint64")
    def uint64(value):
        return isinstance(value, str) and re.fullmatch(r"0|[1-9][0-9]*", value) is not None and int(value) <= 18446744073709551615
    validator = jsonschema.Draft202012Validator(schema, format_checker=checker)
    wire = json.dumps(message, separators=(",", ":"), ensure_ascii=False, allow_nan=False).encode("utf-8")
    if len(wire) + 1 > 65536:
        raise ValueError("frame byte limit")
    count = 0
    def walk(value, depth=1, field=""):
        nonlocal count
        count += 1
        if depth > 16 or count > 8192:
            raise ValueError("JSON structure limit")
        if isinstance(value, dict):
            if len(value) > 256:
                raise ValueError("object member limit")
            for key, item in value.items():
                walk(item, depth + 1, key)
        elif isinstance(value, list):
            if len(value) > 1024:
                raise ValueError("array item limit")
            for item in value:
                walk(item, depth + 1)
        elif isinstance(value, str):
            limit = 96 if field == "text" and message.get("command") == "decodeCode" else 256 if field in ("text", "message") else None
            if limit is not None and len(value.encode("utf-8")) > limit:
                raise ValueError("UTF-8 text byte limit")
        if field == "payload" and len(json.dumps(value, separators=(",", ":"), ensure_ascii=False, allow_nan=False).encode("utf-8")) > 4096:
            raise ValueError("module payload byte limit")
    walk(message)
    validator.validate(message)
    for body in (message.get("arguments", {}), message.get("body", {})):
        caps = body.get("requiredCapabilities", body.get("capabilities", []))
        if "boardInspectFault" in caps and "boardInspect" not in caps:
            raise ValueError("boardInspectFault requires boardInspect")
