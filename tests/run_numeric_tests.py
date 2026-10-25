from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXECUTABLE = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "x64" / "Debug" / "Crol.exe"

CASES = [
    ("folded_add", "print 2+3;", "5"),
    ("runtime_add", "var x=2; print x+3;", "5"),
    ("fractional_add", "print 0.5+0.25;", "0.75"),
    ("folded_negation", "print -5;", "-5"),
    ("negated_expression", "print -(2+3);", "-5"),
    ("negated_fraction", "print -0.5;", "-0.5"),
    ("double_negation", "print -(-5);", "5"),
    ("mixed_precedence", "print 5+6*2;", "17"),
    ("folded_add_mul", "print (2+3)*2;", "10"),
    ("folded_add_eq", "print (2+3)==5;", "true"),
    ("folded_add_neq", "print (2+3)!=5;", "false"),
    ("ordinary_subtract", "print 5-2;", "3"),
    ("ordinary_divide", "print 10/2;", "5"),
    ("string_concat", 'print "a"+"b";', "ab"),
    ("string_length", 'print "hello".length;', "5"),
]


def main() -> int:
    if not EXECUTABLE.is_file():
        print("Missing executable:", EXECUTABLE, file=sys.stderr)
        return 2
    passed = 0
    for name, source, expected in CASES:
        try:
            result = subprocess.run(
                [str(EXECUTABLE)],
                input=(source + "\n\n").encode("utf-8"),
                capture_output=True,
                timeout=6,
                check=False,
            )
        except subprocess.TimeoutExpired:
            print("TIMEOUT", name)
            continue
        output = (result.stdout + result.stderr).decode(
            "utf-8", errors="replace"
        ).replace("\r", "")
        runtime = output.split("========================================================")[-1]
        values = [
            line.strip() for line in runtime.rsplit(">>>", 1)[0].splitlines()
            if line.strip()
        ]
        valid = result.returncode == 0 and values == [expected]
        passed += valid
        print(("PASS" if valid else "FAIL"), name)
        if not valid:
            print("  expected:", repr(expected))
            print("  actual:", values[-6:])
            print("  exit:", result.returncode)
    print(f"RESULTS: {passed}/{len(CASES)} numeric tests passed")
    return 0 if passed == len(CASES) else 1


if __name__ == "__main__":
    raise SystemExit(main())
