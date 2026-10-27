#!/bin/bash
# Comprueba la sintaxis del JavaScript de las páginas embebidas (PAGE_PANEL, PAGE_CAL y PAGE_BLE)
cd "$(dirname "$0")"
python3 ../scripts/gen_web_pages.py --check || exit 1
python3 - << 'PY'
import re, subprocess, sys
ok = True
for f, names in (("../rpi/include/web_pages.h", ["PAGE_PANEL", "PAGE_CAL", "PAGE_BLE"]),):
    s = open(f, encoding="utf-8").read()
    for blk in re.findall(r'R"HTML\((.*?)\)HTML"', s, re.S):
        m = re.search(r"<script>(.*?)</script>", blk, re.S)
        if not m: continue
        open("/tmp/_page.js", "w").write(m.group(1))
        r = subprocess.run(["node", "--check", "/tmp/_page.js"], capture_output=True, text=True)
        title = re.search(r"<title>(.*?)</title>", blk)
        print(("PASS " if r.returncode == 0 else "FAIL ") + "JS " + (title.group(1) if title else "?") + ("" if r.returncode == 0 else "\n" + r.stderr[:400]))
        ok &= r.returncode == 0
sys.exit(0 if ok else 1)
PY
