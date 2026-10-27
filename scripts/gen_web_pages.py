#!/usr/bin/env python3
"""Genera rpi/include/web_pages.h a partir de rpi/web/*.html (+ shared.css y analysis.js).
Uso: python3 scripts/gen_web_pages.py [--check]   (--check falla si web_pages.h no coincide con las fuentes)"""
import sys, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
web = root / "rpi" / "web"
out = root / "rpi" / "include" / "web_pages.h"
css = (web / "shared.css").read_text(encoding="utf-8").strip()
ana = (web / "analysis.js").read_text(encoding="utf-8").strip()
pages = [("PAGE_PANEL", "panel.html"), ("PAGE_CAL", "calibrate.html"), ("PAGE_BLE", "ble.html")]
parts = ["""// web_pages.h — GENERADO por scripts/gen_web_pages.py a partir de rpi/web/*.html. NO editar a mano.
//   PAGE_PANEL : panel principal (/)
//   PAGE_CAL   : calibración acústica con el micrófono del móvil (/calibrate, requiere HTTPS)
//   PAGE_BLE   : configuración de los ESP32 por Web Bluetooth    (/ble, requiere HTTPS)
// El bloque ANALYSIS_BEGIN/END de PAGE_CAL son funciones puras: tests/test_analysis.js lo extrae y ejecuta en Node.
#pragma once
"""]
for name, f in pages:
    h = (web / f).read_text(encoding="utf-8")
    h = h.replace("/*@SHARED_CSS@*/", css).replace("/*@ANALYSIS@*/", ana)
    assert ')HTML"' not in h and "@SHARED_CSS@" not in h and "@ANALYSIS@" not in h, f
    parts.append(f'\nstatic const char {name}[] = R"HTML({h.rstrip()})HTML";\n')
text = "".join(parts)
if "--check" in sys.argv:
    cur = out.read_text(encoding="utf-8") if out.exists() else ""
    if cur != text:
        print("FAIL web_pages.h no está regenerado: ejecuta python3 scripts/gen_web_pages.py"); sys.exit(1)
    print("PASS web_pages.h coincide con rpi/web/"); sys.exit(0)
out.write_text(text, encoding="utf-8")
print("generado", out, len(text), "bytes")
