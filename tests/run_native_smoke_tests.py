from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "x64" / "Debug" / "Crol.exe"

CASES = [
    ("printStack null return", "print inspect.printStack();", ["0 | null", "null"]),
    ("viewStack null return", "print inspect.viewStack();", ["0 | VAL_NULL", "null"]),
    ("printCallFrames null return", "print inspect.printCallFrames();", ["(nativefn)", "null"]),
    ("setPrototypeOf null return", "print inspect.setPrototypeOf({}, {});", ["null"]),
    ("getPrototypeOf object return", "print inspect.getPrototypeOf({});", ["<object_prototype object>"]),
]


def run() -> int:
    if not BINARY.is_file():
        print("Missing executable:", BINARY, file=sys.stderr)
        return 2
    passed = 0
    for name, source, expected_parts in CASES:
        try:
            completed = subprocess.run(
                [str(BINARY)], input=(source + "\n\n").encode("utf-8"),
                capture_output=True, timeout=7, check=False,
            )
        except subprocess.TimeoutExpired:
            print("TIMEOUT", name)
            continue
        output = (completed.stdout + completed.stderr).decode(
            "utf-8", errors="replace"
        ).replace("\r", "")
        # Restrict to runtime output after optional Debug bytecode listing.
        section = output.split("========================================================")[-1]
        section = section.rsplit(">>>", 1)[0]
        nonblank = [line.strip() for line in section.splitlines() if line.strip()]
        ok = (
            completed.returncode == 0
            and all(fragment in section for fragment in expected_parts)
            and (nonblank[-1] == expected_parts[-1] if nonblank else False)
            and "CompileError:" not in output
            and "TypeErrorException" not in output
        )
        passed += ok
        print(("PASS" if ok else "FAIL"), name)
        if not ok:
            print("  exit:", completed.returncode, "output:", repr(section[-500:]))
    print(f"RESULTS: {passed}/{len(CASES)} native smoke tests passed")
    return 0 if passed == len(CASES) else 1


if __name__ == "__main__":
    raise SystemExit(run())
