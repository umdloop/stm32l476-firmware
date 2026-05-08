#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    project_dir = Path(__file__).resolve().parents[1]
    converter = project_dir / "tools" / "dbc_to_c.py"
    input_dbc = project_dir / "App" / "dbc" / "4.13.2026.dbc"
    output_c = project_dir / "App" / "dbc" / "can_dbc_text.c"

    return subprocess.call(
        [
            sys.executable,
            str(converter),
            str(input_dbc),
            str(output_c),
            "--no-install-dbc",
        ]
    )


if __name__ == "__main__":
    raise SystemExit(main())
