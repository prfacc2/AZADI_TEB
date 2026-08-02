#!/usr/bin/env python3
"""Structural regression checks for the 30 native builtin print templates."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "print_designer_templates.inc"
text = SOURCE.read_text(encoding="utf-8")

failures = []

def check(condition, message):
    if not condition:
        failures.append(message)

build_match = re.search(
    r"static PrintDesign buildTemplate\(int idx\)\s*\{(.*?)\n\s*return d;\n\}",
    text,
    re.S,
)
check(build_match is not None, "buildTemplate body was not found")
build = build_match.group(1) if build_match else ""

cases = list(re.finditer(r"\n\s*case\s+(\d+)\s*:\s*\{", build))
indexes = [int(match.group(1)) for match in cases]
check(indexes == list(range(30)), f"expected cases 0..29 exactly once, got {indexes}")

for pos, match in enumerate(cases):
    idx = int(match.group(1))
    end = cases[pos + 1].start() if pos + 1 < len(cases) else len(build)
    block = build[match.end():end]
    service_builders = len(re.findall(r"\badd(?:A4Body|ServicesBlock)\s*\(", block))
    check(service_builders == 1,
          f"template {idx + 1:02d} must contain exactly one dynamic services block; got {service_builders}")
    check("addFooter(" in block,
          f"template {idx + 1:02d} has no reserved footer and barcode band")
    check("setPaper(L\"A4\"" in block or "a4p();" in block,
          f"template {idx + 1:02d} is not authored against A4 geometry")

service_fn = re.search(
    r"static PrintItem mkServices\(.*?\)\s*\{(.*?)\n\}", text, re.S
)
check(service_fn is not None, "mkServices was not found")
service_body = service_fn.group(1) if service_fn else ""
for escaped_label, label in [
    (r"\u0631\u062f\u06cc\u0641", "row"),
    (r"\u0646\u0627\u0645 \u062e\u062f\u0645\u062a", "service name"),
    (r"\u062a\u0639\u062f\u0627\u062f", "quantity"),
    (r"\u0634\u0631\u062d \u062e\u062f\u0645\u062a", "service description"),
]:
    check(escaped_label in service_body, f"mkServices is missing the {label} label")
check('L"{\\"cols\\":4' in service_body, "services model must declare four columns")
check("it.type=PIT_SERVICES" in service_body, "services builder is not PIT_SERVICES")

check('getSetting(L"tpl_migration_1_59"' in text,
      "v1.59 migration guard is missing")
check(text.count('setSetting(L"tpl_migration_1_59", L"1")') == 2,
      "v1.59 migration must be marked for both fresh installs and upgrades")

# The canonical footer begins at paperH-30 and its separator at paperH-33.
# All current service boxes are at most 78 mm high and begin no lower than
# 72 mm, leaving the totals/custom blocks well above 264 mm on portrait A4;
# landscape T02 explicitly uses a 52 mm table. These guards catch accidental
# growth that would reintroduce A4/A5 overlap after proportional scaling.
heights = [float(value) for value in re.findall(
    r"add(?:A4Body|ServicesBlock)\([^;\n]*?,\s*(\d+(?:\.\d+)?)\s*\)\s*;", build
)]
check(all(height <= 78.0 for height in heights),
      f"a services box exceeds the overlap-safe 78 mm bound: {heights}")
check("double y = paperH - 30;" in text and "mkHLine(m,y-3" in text,
      "footer no longer reserves the final 33 mm of the page")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    sys.exit(1)

print("PASS: 30 builtin templates are unique indexed A4 designs")
print("PASS: every template has one dynamic real-service table and one footer")
print("PASS: service labels and v1.59 migration guard are valid")
print("PASS: service/footer geometry remains within the A4/A5 scaling bounds")
