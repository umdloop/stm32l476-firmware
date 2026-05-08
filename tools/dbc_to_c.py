#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

BO_RE = re.compile(r"(?m)^BO_\s+(\d+)\s+[^\n]*")
SG_RE = re.compile(r"\sSG_\s+[^\n]*?(?=(?:\sSG_\s+)|\n|$)")
SG_NAME_RE = re.compile(r"^\s*SG_\s+([A-Za-z0-9_]+)\s+")
SG_PLAIN_MUX_RE = re.compile(r"^(\s*SG_\s+)([A-Za-z0-9_]+)(\s+)m(\d+)(\s*:.*)$")
SG_MUL_VAL_RE = re.compile(r"^SG_MUL_VAL_\s+(\d+)\s+([A-Za-z0-9_]+)\s+([A-Za-z0-9_]+)\s+(\d+)-(\d+)\s*;")


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def collect_sg_mul_values(text: str) -> dict[tuple[str, str], str]:
    mux_values: dict[tuple[str, str], str] = {}
    for raw in text.splitlines():
        match = SG_MUL_VAL_RE.match(raw.strip())
        if match is None:
            continue
        message_id = match.group(1)
        signal_name = match.group(2)
        mux_start = match.group(4)
        mux_end = match.group(5)
        if mux_start == mux_end:
            mux_values[(message_id, signal_name)] = mux_start
    return mux_values


def fix_sg_mux_token(line: str, message_id: str, mux_values: dict[tuple[str, str], str]) -> str:
    match = SG_PLAIN_MUX_RE.match(line)
    if match is None:
        return line
    signal_name = match.group(2)
    mux_value_from_sg = match.group(4)
    mux_value_from_table = mux_values.get((message_id, signal_name))
    if mux_value_from_table is None:
        return line
    if mux_value_from_table != mux_value_from_sg:
        print(
            "Warning: mux mismatch for "
            f"BO_ {message_id} signal {signal_name}: "
            f"SG_ says m{mux_value_from_sg}, SG_MUL_VAL_ says {mux_value_from_table}",
        )
        return line
    return match.group(1) + match.group(2) + match.group(3) + "m" + match.group(4) + "M" + match.group(5)


def extract_bo_sg_lines(text: str) -> tuple[list[str], int]:
    """Extract BO_/SG_ records. Multiple SG_ records on one physical line are split."""
    mux_values = collect_sg_mul_values(text)
    filtered: list[str] = []
    motorola_warnings = 0
    bo_matches = list(BO_RE.finditer(text))
    for index, bo_match in enumerate(bo_matches):
        message_id = bo_match.group(1)
        filtered.append(bo_match.group(0).strip())
        section_start = bo_match.end()
        section_end = bo_matches[index + 1].start() if index + 1 < len(bo_matches) else len(text)
        section = text[section_start:section_end]
        for sg_match in SG_RE.finditer(section):
            sg_line = sg_match.group(0).strip()
            if not SG_NAME_RE.match(sg_line):
                continue
            if "@0" in sg_line:
                motorola_warnings += 1
            sg_line = fix_sg_mux_token(sg_line, message_id, mux_values)
            filtered.append("\t" + sg_line)
    return filtered, motorola_warnings


def emit_c_file(source_name: Path, out: Path, filtered: list[str]) -> None:
    lines: list[str] = []
    lines.append("#include <stddef.h>\n\n")
    lines.append("/*\n")
    lines.append(" * AUTO-GENERATED FILE -- DO NOT EDIT BY HAND.\n")
    lines.append(" *\n")
    lines.append(f" * Source: {source_name.as_posix()}\n")
    lines.append(" * Generator: tools/dbc_to_c.py\n")
    lines.append(" *\n")
    lines.append(" * This file intentionally contains only BO_ and SG_ records because\n")
    lines.append(" * the firmware DBC parser ignores the rest of the DBC format.\n")
    lines.append(" *\n")
    lines.append(" * SG_MUL_VAL_ records are consumed by this generator and represented\n")
    lines.append(" * as simple m##M SG_ mux syntax for the firmware parser.\n")
    lines.append(" */\n\n")
    lines.append("const char* g_can_dbc_text =\n")
    for line in filtered:
        lines.append(f'"{c_escape(line)}\\n"\n')
    lines.append(";\n\n")
    lines.append(f"const size_t g_can_dbc_text_len = {len(filtered)}U;\n")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("".join(lines), encoding="utf-8", newline="\n")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert a DBC into firmware-embedded BO_/SG_ C text.")
    parser.add_argument("input_dbc", help="Path to the source .dbc file")
    parser.add_argument("output_c", nargs="?", default="App/dbc/can_dbc_text.c", help="Destination C file")
    parser.add_argument("--install-dbc", default="App/dbc/file.dbc", help="Optional destination for copying the raw DBC")
    parser.add_argument("--no-install-dbc", action="store_true", help="Do not copy the raw DBC")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    inp = Path(args.input_dbc).expanduser().resolve()
    out = Path(args.output_c).expanduser()
    install_dbc = Path(args.install_dbc).expanduser()
    if not inp.is_file():
        print(f"Error: input DBC does not exist: {inp}")
        return 2
    text = inp.read_text(encoding="utf-8", errors="strict")
    if not args.no_install_dbc:
        install_dbc.parent.mkdir(parents=True, exist_ok=True)
        if inp.resolve() != install_dbc.resolve():
            shutil.copyfile(inp, install_dbc)
            print(f"Installed raw DBC to: {install_dbc}")
        source_name = install_dbc
    else:
        source_name = inp
    filtered, motorola_warnings = extract_bo_sg_lines(text)
    emit_c_file(source_name, out, filtered)
    if motorola_warnings:
        print(f"Warning: found {motorola_warnings} Motorola/big-endian @0 SG_ records.")
        print("         Current firmware bit helpers are little-endian @1 only.")
    print(f"Wrote: {out}")
    print(f"Kept {len(filtered)} BO_/SG_ records.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
