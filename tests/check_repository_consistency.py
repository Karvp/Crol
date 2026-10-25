from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PROJECTS = ("Crol.vcxproj", "Crol.vcxproj.filters")
REQUIRED = ("Crol.sln", "README.md", "tests/run_control_flow_tests.py")
errors = []

for name in PROJECTS:
    file = ROOT / name
    tree = ET.parse(file)
    for item in tree.iter():
        reference = item.attrib.get("Include", "")
        if not reference.lower().endswith((".cpp", ".h", ".md")):
            continue
        destination = ROOT / reference
        if not destination.is_file():
            errors.append(f"{name}: missing {reference}")
            continue
        # Windows resolves casing automatically, so inspect actual directory entries.
        parent = destination.parent
        existing_names = {entry.name for entry in parent.iterdir()}
        if destination.name not in existing_names:
            matches = [x for x in existing_names if x.casefold() == destination.name.casefold()]
            errors.append(
                f"{name}: case mismatch {reference} != {matches[0] if matches else 'unknown'}"
            )

for name in REQUIRED:
    if not (ROOT / name).is_file():
        errors.append(f"Required file missing: {name}")

for relative in ("README.md", "docs/ARCHITECTURE.md"):
    doc = ROOT / relative
    text = doc.read_text(encoding="utf-8-sig")
    for target in re.findall(r"(?<!!)\[[^\]]*\]\(([^)]+)\)", text):
        target = target.split("#")[0].split("?")[0]
        if not target or "://" in target or target.startswith(("mailto:", "#")):
            continue
        if not (doc.parent / target).exists():
            errors.append(f"{relative}: broken relative Markdown link: {target}")

if errors:
    print("Repository consistency check FAILED:")
    for issue in errors:
        print(" -", issue)
    sys.exit(1)

print("Repository consistency check passed")
