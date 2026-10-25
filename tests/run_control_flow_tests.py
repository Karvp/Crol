from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
CASES = Path(__file__).with_name("control_flow_cases.json")
DEFAULT_BINARY = PROJECT / "x64" / "Debug" / "Crol.exe"


def main() -> int:
    binary = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT_BINARY
    if not binary.is_file():
        print(f"Missing executable: {binary}", file=sys.stderr)
        return 2

    cases = json.loads(CASES.read_text(encoding="utf-8"))
    passed = 0

    for case in cases:
        try:
            result = subprocess.run(
                [str(binary)],
                input=(case["source"] + "\n\n").encode("utf-8"),
                capture_output=True,
                timeout=5,
                check=False,
            )
            output = (result.stdout + result.stderr).decode(
                "utf-8", errors="replace"
            ).replace("\r", "")
            # Debug builds optionally print a disassembly separated by this line.
            section = output.split("========================================================")[-1]
            section = section.split(">>>")[0]
            found = [line.strip() for line in section.splitlines() if line.strip()]
            valid = (
                result.returncode == 0
                and found == case["expected"]
                and "CompileError:" not in output
                and "Traceback" not in output
            )
            passed += valid
            print(f'{"PASS" if valid else "FAIL"} {case["name"]}')
            if not valid:
                print("  expected:", case["expected"])
                print("  actual:", found[:15])
                print("  exit:", result.returncode)
        except subprocess.TimeoutExpired:
            print(f'TIMEOUT {case["name"]}')

    print(f"RESULTS: {passed}/{len(cases)} passed")
    return 0 if passed == len(cases) else 1


if __name__ == "__main__":
    raise SystemExit(main())
