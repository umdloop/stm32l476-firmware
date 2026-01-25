#!/usr/bin/env python3
import sys
from pathlib import Path

def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace("\"", "\\\"")

def main():
    if len(sys.argv) != 3:
        print("Usage: dbc_to_c.py <input.dbc> <output_can_dbc_text.c>")
        return 2

    inp = Path(sys.argv[1])
    out = Path(sys.argv[2])

    text = inp.read_text(encoding="utf-8").splitlines()

    lines = []
    lines.append('#include <stddef.h>\n\n')
    lines.append('/*\n')
    lines.append(' * AUTO-GENERATED FILE — DO NOT EDIT BY HAND.\n')
    lines.append(f' *\n * Source: {inp.as_posix()}\n')
    lines.append(' * Generator: tools/dbc_to_c.py\n')
    lines.append(' */\n\n')

    lines.append('const char* g_can_dbc_text =\n')
    for l in text:
        lines.append(f"\"{c_escape(l)}\\n\"\n")
    lines.append(';\n\n')
    lines.append('const size_t g_can_dbc_text_len = 0;\n')

    out.write_text("".join(lines), encoding="utf-8")
    print(f"Wrote {out}")

    return 0

if __name__ == "__main__":
    raise SystemExit(main())
