#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace("\"", "\\\"")


def usage() -> int:
    print("Usage: dbc_to_c.py <input.dbc> <output_can_dbc_text.c>")
    print("Example: python tools/dbc_to_c.py app/dbc/file.dbc app/dbc/can_dbc_text.c")
    return 2


def main() -> int:
    if len(sys.argv) != 3:
        return usage()

    inp = Path(sys.argv[1]).expanduser()
    out = Path(sys.argv[2]).expanduser()

    if not inp.exists():
        print(f"Error: input file does not exist: {inp}")
        return 2
    if inp.is_dir():
        print(f"Error: input path is a directory, not a file: {inp}")
        return 2

    try:
        text_lines = inp.read_text(encoding="utf-8", errors="strict").splitlines()
    except UnicodeDecodeError as e:
        print(f"Error: failed to read as UTF-8: {inp}")
        print(f"  {e}")
        return 2

    lines: list[str] = []
    lines.append("#include <stddef.h>\n\n")
    lines.append("/*\n")
    lines.append(" * AUTO-GENERATED FILE — DO NOT EDIT BY HAND.\n")
    lines.append(f" *\n * Source: {inp.as_posix()}\n")
    lines.append(" * Generator: tools/dbc_to_c.py\n")
    lines.append(" */\n\n")

    lines.append("const char* g_can_dbc_text =\n")
    for l in text_lines:
        lines.append(f"\"{c_escape(l)}\\n\"\n")
    lines.append(";\n\n")

    # Leave 0 if unused; or compute on-target with strlen(g_can_dbc_text).
    lines.append("const size_t g_can_dbc_text_len = 0;\n")

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("".join(lines), encoding="utf-8", newline="\n")

    print(f"Wrote: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
