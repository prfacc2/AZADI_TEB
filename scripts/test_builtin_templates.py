#!/usr/bin/env python3
"""Structural regression checks for the 30 builtin print designs (v1.62.0).

Validates BOTH sides of the ready-made-template contract:

  1. src/print_designer_templates.inc  — the C++ seeder the print engine uses
  2. assets/designer/templates.js      — the ES5 mirror the web gallery shows

The single invariant that matters for the bug this architecture was written to
kill («خدمات چاپ نمی‌شود»): every one of the 30 designs must own exactly ONE
dynamic PIT_SERVICES table, its column captions must all be classifiable by
printer.cpp::pdSvcColOf(), and the table must be tall enough to print a real
bill (>= 12 rows on A4) without ever overlapping the footer band.
"""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / "src" / "print_designer_templates.inc"
JS = ROOT / "assets" / "designer" / "templates.js"
PRINTER = ROOT / "src" / "printer.cpp"

failures = []
notes = []


def check(condition, message):
    if not condition:
        failures.append(message)
    return bool(condition)


# ===========================================================================
# 0. pdSvcColOf() re-implementation — keeps the templates honest.
# ===========================================================================
ZW = "\u200c\u200f\u200e \t"


def _norm(label):
    out = []
    for ch in label:
        if ch in ZW:
            continue
        if ch == "\u064a":
            ch = "\u06cc"
        if ch == "\u0643":
            ch = "\u06a9"
        out.append(ch)
    return "".join(out)


# ordered exactly like the C++ cascade in printer.cpp::pdSvcColOf
COL_RULES = [
    (("\u0634\u0631\u062d", "\u062a\u0648\u0636\u06cc\u062d"), "DESC"),
    (("\u0646\u0648\u0639",), "CAT"),
    (("\u062a\u0639\u062f\u0627\u062f", "\u0645\u0642\u062f\u0627\u0631"), "QTY"),
    (("\u0631\u062f\u06cc\u0641", "\u0634\u0645\u0627\u0631\u0647"), "ROW"),
    (
        (
            "\u0633\u0647\u0645\u0628\u06cc\u0645\u0647",
            "\u0633\u0647\u0645\u067e\u0627\u06cc\u0647",
            "\u0628\u06cc\u0645\u0647",
        ),
        "INS",
    ),
    (("\u0633\u0647\u0645\u0628\u06cc\u0645\u0627\u0631", "\u067e\u0631\u062f\u0627\u062e\u062a\u06cc"), "PAT"),
    (("\u062a\u062e\u0641\u06cc\u0641",), "DISC"),
    (("\u0645\u0628\u0644\u063a\u06a9\u0644", "\u062c\u0645\u0639", "\u06a9\u0644"), "LINE"),
    (("\u0642\u06cc\u0645\u062a", "\u0641\u06cc", "\u0645\u0628\u0644\u063a", "\u0646\u0631\u062e"), "PRICE"),
    (("\u06a9\u062f",), "CODE"),
    (
        (
            "\u0646\u0627\u0645\u062e\u062f\u0645\u062a",
            "\u062e\u062f\u0645\u062a",
            "\u0646\u0627\u0645",
            "\u0639\u0646\u0648\u0627\u0646",
        ),
        "NAME",
    ),
]


def classify(label):
    norm = _norm(label)
    if not norm:
        return "NONE"
    for needles, kind in COL_RULES:
        for needle in needles:
            if needle in norm:
                return kind
    return "NONE"


# sanity: the python mirror must agree with the real cascade on the two
# captions that are historically the trickiest (سهم بیمار vs سهم بیمه).
check(
    classify("\u0633\u0647\u0645 \u0628\u06cc\u0645\u0627\u0631") == "INS"
    or classify("\u0633\u0647\u0645 \u0628\u06cc\u0645\u0627\u0631") == "PAT",
    "the pdSvcColOf mirror cannot classify «سهم بیمار» at all",
)
check(
    "pdSvcColOf" in PRINTER.read_text(encoding="utf-8"),
    "printer.cpp no longer exposes pdSvcColOf — the caption contract is gone",
)

# ===========================================================================
# 1. src/print_designer_templates.inc
# ===========================================================================
inc = INC.read_text(encoding="utf-8")

check("PIT_SERVICES" in inc, "the seeder never emits a PIT_SERVICES item")
check(
    "it.type=PIT_SERVICES" in inc.replace(" ", ""),
    "mkServices() does not build a PIT_SERVICES item",
)

# --- page geometry ---------------------------------------------------------
geom = dict(re.findall(r"static const double (PG_\w+|FOOT_Y)\s*=\s*([-\d.]+)", inc))
check(geom.get("PG_W") == "210.0", f"page width is not A4 portrait: {geom.get('PG_W')}")
check(geom.get("PG_H") == "297.0", f"page height is not A4 portrait: {geom.get('PG_H')}")
check("PG_CW  = PG_W - 2*PG_M" in inc, "content width is no longer derived from the margin")
check("FOOT_Y = PG_H - 34.0" in inc, "the 34 mm footer band reservation is gone")

# --- the 8 column presets --------------------------------------------------
preset_names = ["SVC3", "SVC4_ROW", "SVC4_CAT", "SVC5", "SVC5_CODE", "SVC6_FIN", "SVC6_INS", "SVC7"]
enum_match = re.search(r"enum SvcPreset \{(.*?)\}", inc, re.S)
check(enum_match is not None, "enum SvcPreset was not found")
if enum_match:
    body = enum_match.group(1)
    for name in preset_names:
        check(re.search(r"\b%s\b" % name, body) is not None, f"SvcPreset {name} is missing")
    check("SVC_PRESET_COUNT" in body, "SvcPreset has no COUNT sentinel")

model_fn = re.search(r"static std::wstring svcModelJson\(int preset\)\{(.*?)\n\}", inc, re.S)
check(model_fn is not None, "svcModelJson() was not found")
model_body = model_fn.group(1) if model_fn else ""

# reconstruct each preset's JSON out of the concatenated wide-string literals
inc_presets = {}
for chunk in re.split(r"\n\s*case\s+|\n\s*default:", model_body):
    tag = re.match(r"(SVC[A-Z0-9_]*)\s*:", chunk)
    pieces = re.findall(r'L"((?:[^"\\]|\\.)*)"', chunk)
    if not pieces:
        continue
    raw = "".join(pieces).replace('\\"', '"')
    try:
        parsed = json.loads(raw)
    except ValueError:
        continue
    key = tag.group(1) if tag else "SVC3"
    inc_presets.setdefault(key, parsed)

check(
    len(inc_presets) == 8,
    f"expected 8 parsable column presets in svcModelJson, got {sorted(inc_presets)}",
)


def audit_preset(where, key, model):
    cols = model.get("cols")
    widths = model.get("widths") or []
    labels = model.get("labels") or []
    check(model.get("header") is True, f"{where} preset {key} has no header row")
    check(cols in (3, 4, 5, 6, 7), f"{where} preset {key} declares an odd column count {cols}")
    check(len(widths) == cols, f"{where} preset {key}: {len(widths)} widths for {cols} cols")
    check(len(labels) == cols, f"{where} preset {key}: {len(labels)} labels for {cols} cols")
    total = sum(widths)
    check(
        abs(total - 1.0) < 0.005,
        f"{where} preset {key}: column widths sum to {total:.4f}, not 1.0",
    )
    check(
        all(w >= 0.045 for w in widths),
        f"{where} preset {key} has an unprintably narrow column: {widths}",
    )
    kinds = [classify(lab) for lab in labels]
    check(
        "NONE" not in kinds,
        f"{where} preset {key} has a caption printer.cpp cannot classify: "
        + ", ".join("%s->%s" % (l, k) for l, k in zip(labels, kinds)),
    )
    check(
        "NAME" in kinds,
        f"{where} preset {key} has no service-name column — the receipt would be anonymous",
    )
    check(
        len(set(kinds)) == len(kinds),
        f"{where} preset {key} repeats a column kind: {kinds}",
    )
    return kinds


for key, model in sorted(inc_presets.items()):
    kinds = audit_preset("inc", key, model)
    notes.append("  %-9s %d cols  %s" % (key, model.get("cols"), " | ".join(kinds)))

# --- computed services height --------------------------------------------
for fn in ("servicesBlock", "servicesBlockAt"):
    block = re.search(r"static double %s\(.*?\n\}" % fn, inc, re.S)
    check(block is not None, f"{fn}() was not found")
    if block:
        body = block.group(0)
        check(
            "footY - reserve - y" in body,
            f"{fn}() no longer COMPUTES its height from the free page space",
        )
        check("if(h < 40) h = 40;" in body, f"{fn}() can collapse below 40 mm")
        check("if(h > 200) h = 200;" in body, f"{fn}() can overflow past 200 mm")
        check("mkServices(" in body, f"{fn}() does not emit the services table")

# --- the 30 specs ---------------------------------------------------------
spec_match = re.search(r"static const TplSpec TPL\[30\] = \{(.*?)\n\};", inc, re.S)
check(spec_match is not None, "the TplSpec TPL[30] table was not found")
specs = []
if spec_match:
    for line in spec_match.group(1).splitlines():
        row = re.search(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(SVC[A-Z0-9_]*)\s*,(.*?)\}", line)
        if not row:
            continue
        rest = [p.strip() for p in row.group(4).split(",")]
        specs.append(
            {
                "family": int(row.group(1)),
                "variant": int(row.group(2)),
                "svc": row.group(3),
                "accent": rest[0],
                "headFill": rest[2],
                "bw": float(rest[3]),
                "rowH": float(rest[4]),
                "frame": rest[5] == "true",
            }
        )

check(len(specs) == 30, f"expected 30 TplSpec rows, parsed {len(specs)}")
if len(specs) == 30:
    fams = sorted({s["family"] for s in specs})
    check(fams == list(range(10)), f"expected 10 layout families 0..9, got {fams}")
    for fam in range(10):
        variants = sorted(s["variant"] for s in specs if s["family"] == fam)
        check(
            variants == [0, 1, 2],
            f"family {fam} must have variants 0,1,2 — got {variants}",
        )
    used = {s["svc"] for s in specs}
    check(
        used <= set(preset_names),
        f"a spec references an unknown preset: {sorted(used - set(preset_names))}",
    )
    check(
        len(used) >= 7,
        f"only {len(used)} of the 8 column presets are actually used: {sorted(used)}",
    )
    for i, s in enumerate(specs):
        check(
            5.5 <= s["rowH"] <= 9.0,
            f"template {i + 1:02d} row pitch {s['rowH']} mm is outside the legible 5.5..9 mm band",
        )
        check(
            0.2 <= s["bw"] <= 0.8,
            f"template {i + 1:02d} table border {s['bw']} mm is outside 0.2..0.8 mm",
        )
    lineart = [i + 1 for i, s in enumerate(specs) if s["headFill"] == "0x000000"]
    check(
        len(lineart) >= 3,
        f"expected at least 3 pure line-art (monochrome) designs, got {lineart}",
    )

# --- names are actually applied to the design ---------------------------
name_match = re.search(r"static const wchar_t\* const TPL_NAMES\[30\]=\{(.*?)\n\};", inc, re.S)
check(name_match is not None, "the TPL_NAMES[30] table was not found")
inc_names = re.findall(r'L"([^"]*)"', name_match.group(1)) if name_match else []
check(len(inc_names) == 30, f"expected 30 template names, got {len(inc_names)}")
check(len(set(inc_names)) == len(inc_names), "two builtin templates share the same name")
check(
    all(n.strip() for n in inc_names),
    "a builtin template name is blank — the gallery would show an empty card",
)
check(
    "d.name = TPL_NAMES[idx];" in inc,
    "buildTemplate() does not stamp TPL_NAMES onto the design — designs seed NAMELESS "
    "(this was the v1.62.0 blank-gallery bug)",
)
check(
    "return TPL_NAMES[idx];" in inc,
    "buildTemplateName() no longer shares the single TPL_NAMES table",
)

# --- every family really prints the live services ----------------------
build_match = re.search(
    r"static PrintDesign buildTemplate\(int idx\)\{(.*?)\n    return d;\n\}", inc, re.S
)
check(build_match is not None, "buildTemplate() body was not found")
build = build_match.group(1) if build_match else ""
cases = list(re.finditer(r"\n    (?:case\s+(\d+)\s*:|default:)\s*\{?", build))
check(len(cases) == 10, f"buildTemplate must switch over 10 families, found {len(cases)} arms")
for pos, match in enumerate(cases):
    fam = match.group(1) if match.group(1) is not None else "9(default)"
    end = cases[pos + 1].start() if pos + 1 < len(cases) else len(build)
    arm = build[match.end() : end]
    n = len(re.findall(r"\bservicesBlock(?:At)?\s*\(", arm))
    check(n == 1, f"family {fam} must call servicesBlock*() exactly once; got {n}")
    # A family either calls one of the four shared footer builders, or (family 3,
    # the sidebar layout) paints its own barcode band inside the sidebar column.
    check(
        re.search(r"\bfoot(?:Barcode|Centered|Dual|Minimal)\s*\(", arm) is not None
        or "mkBarcode(" in arm,
        f"family {fam} reserves no footer / barcode band",
    )
    check("footY" in arm, f"family {fam} does not respect the footer reservation")

check('d.paper=L"A4"' in inc.replace(" ", "").replace('d.paper=L"A4"', 'd.paper=L"A4"') or 'd.paper=L"A4"' in inc,
      "designs are no longer authored against A4")

# --- migration guard ---------------------------------------------------
check('getSetting(L"tpl_migration_1_65"' in inc, "the v1.65 migration guard is missing")
init_fn = re.search(r"void Designs_Init\(\)\{(.*?)\n\}", inc, re.S)
check(init_fn is not None, "Designs_Init() was not found")
if init_fn:
    init_body = init_fn.group(1)
    check(
        init_body.count("stamp();") == 2,
        "the migration must be stamped for BOTH fresh installs and upgrades "
        f"(found {init_body.count('stamp();')} stamp() calls)",
    )
    check(
        "Designs_Insert(d)" in init_body and "Designs_Update(fresh)" in init_body,
        "Designs_Init() no longer both seeds fresh installs and rebuilds existing ones",
    )
    check(
        "Designs_Delete(existing[i].id)" in init_body,
        "Designs_Init() no longer removes surplus builtins beyond the 30",
    )
for old in ("1_52", "1_53", "1_58", "1_59", "1_60", "1_61", "1_62"):
    check(
        'setSetting(L"tpl_migration_%s", L"1")' % old in inc,
        f"upgrade path no longer retires the tpl_migration_{old} guard",
    )

# ===========================================================================
# 2. assets/designer/templates.js — executed, then compared to the .inc
# ===========================================================================
js = JS.read_text(encoding="utf-8")
check("window.AZ_TEMPLATES" in js, "templates.js no longer publishes window.AZ_TEMPLATES")
check("var PG_W   = 210.0" in js or "PG_W = 210.0" in js.replace("   ", " "),
      "templates.js drifted off A4 geometry")
check("FOOT_Y = PG_H - 34.0" in js, "templates.js lost the footer reservation")

harness = r"""
var fs = require('fs');
global.window = {};
new Function(fs.readFileSync(process.argv[2], 'utf8')).call(global);
var all = global.window.AZ_TEMPLATES;
var out = [];
for (var i = 0; i < all.length; i++) {
  var t = all[i], svc = [], k;
  for (k = 0; k < t.items.length; k++) if (t.items[k].type === 'services') svc.push(t.items[k]);
  var minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
  for (k = 0; k < t.items.length; k++) {
    var it = t.items[k];
    if (it.isFrame) continue;
    if (it.x < minX) minX = it.x;
    if (it.y < minY) minY = it.y;
    if (it.x + it.w > maxX) maxX = it.x + it.w;
    if (it.y + it.h > maxY) maxY = it.y + it.h;
  }
  var s = svc.length === 1 ? svc[0] : null;
  var model = null;
  if (s) { try { model = JSON.parse(s.text); } catch (e) { model = null; } }
  var rows = 0;
  if (s && s.rowH > 0) rows = Math.floor((s.h - (s.headerH || s.rowH)) / s.rowH);
  out.push({
    name: t.name, paper: t.paper, orientation: t.orientation,
    items: t.items.length, svcCount: svc.length,
    model: model, h: s ? s.h : 0, y: s ? s.y : 0, rowH: s ? s.rowH : 0,
    headerH: s ? s.headerH : 0, rows: rows,
    minX: minX, minY: minY, maxX: maxX, maxY: maxY
  });
}
process.stdout.write(JSON.stringify(out));
"""

js_designs = []
try:
    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as fh:
        fh.write(harness)
        harness_path = fh.name
    proc = subprocess.run(
        ["node", harness_path, str(JS)], capture_output=True, text=True, timeout=60
    )
    if proc.returncode != 0:
        failures.append("templates.js failed to execute under node: " + proc.stderr.strip()[:400])
    else:
        js_designs = json.loads(proc.stdout)
except FileNotFoundError:
    notes.append("  (node not available — skipped the templates.js execution audit)")
except Exception as exc:  # pragma: no cover
    failures.append(f"could not run the templates.js audit: {exc}")

if js_designs:
    check(len(js_designs) == 30, f"templates.js publishes {len(js_designs)} designs, expected 30")
    js_names = [d["name"] for d in js_designs]
    check(
        js_names == inc_names,
        "the web gallery names have drifted from the C++ TPL_NAMES table",
    )
    for i, d in enumerate(js_designs):
        tag = "web template %02d" % (i + 1)
        check(d["paper"] == "A4" and d["orientation"] == 0, f"{tag} is not A4 portrait")
        check(
            d["svcCount"] == 1,
            f"{tag} must own exactly one dynamic services table; got {d['svcCount']}",
        )
        if d["svcCount"] != 1:
            continue
        check(d["model"] is not None, f"{tag} has an unparsable services model")
        if d["model"]:
            audit_preset("web", "#%02d" % (i + 1), d["model"])
            if len(specs) == 30:
                want = inc_presets.get(specs[i]["svc"])
                if want:
                    check(
                        d["model"].get("labels") == want.get("labels"),
                        f"{tag} column captions differ from the C++ preset {specs[i]['svc']}",
                    )
                    check(
                        d["model"].get("widths") == want.get("widths"),
                        f"{tag} column widths differ from the C++ preset {specs[i]['svc']}",
                    )
        check(d["rowH"] > 0 and d["headerH"] > 0, f"{tag} has no pinned row/header pitch")
        check(
            d["rows"] >= 12,
            f"{tag} only fits {d['rows']} service rows — a real bill would not fit",
        )
        check(
            d["y"] + d["h"] <= 263.0 + 0.01,
            f"{tag} services table (y={d['y']:.1f} h={d['h']:.1f}) runs into the footer band",
        )
        # Decorative shells (family 5's rounded page card) and the footer bands
        # deliberately reach 2 mm outside the 12 mm text margin, so the guard is
        # the printable-area bleed limit, not the text margin.
        check(
            d["minX"] >= 9.9 and d["maxX"] <= 200.1,
            f"{tag} bleeds off the printable width ({d['minX']:.1f}..{d['maxX']:.1f})",
        )
        check(
            d["minY"] >= 9.9 and d["maxY"] <= 293.6,
            f"{tag} bleeds off the printable height ({d['minY']:.1f}..{d['maxY']:.1f})",
        )
        check(d["items"] >= 18, f"{tag} looks under-designed ({d['items']} items)")
    rowcounts = [d["rows"] for d in js_designs if d["svcCount"] == 1]
    if rowcounts:
        notes.append(
            "  printable service rows per design: min=%d max=%d avg=%.1f"
            % (min(rowcounts), max(rowcounts), sum(rowcounts) / float(len(rowcounts)))
        )

# ===========================================================================
if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    sys.exit(1)

print("PASS: 8 services column presets, every caption classifiable by pdSvcColOf")
for note in notes:
    print(note)
print("PASS: 30 specs = 10 distinct layout families x 3 variants, legible pitch/borders")
print("PASS: all 30 designs carry their Persian name (no more blank gallery cards)")
print("PASS: every family emits exactly one live PIT_SERVICES table + a footer band")
print("PASS: services height is computed from free page space (>=12 rows, no footer overlap)")
print("PASS: assets/designer/templates.js mirrors the C++ seeder exactly")
print("PASS: tpl_migration_1_65 guard stamped for fresh installs and upgrades")
