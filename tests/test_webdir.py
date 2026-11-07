#!/usr/bin/env python3
"""Web servida desde una carpeta (flag -w de la RPi): mismo montaje que scripts/gen_web_pages.py, cambios sin recompilar,
respaldo a la copia incrustada y sin salir de la carpeta. Arnés en modo 'webdir <carpeta>' (puerto 18082)."""
import urllib.request, urllib.error, sys, pathlib, shutil, time
ok = True
def check(n, c, x=""):
    global ok; print(("PASS " if c else "FAIL ") + n + (f"  [{x}]" if x else "")); ok &= bool(c)
def get(path):
    try:
        r = urllib.request.urlopen("http://127.0.0.1:18082" + path, timeout=3); return r.status, r.read().decode()
    except urllib.error.HTTPError as e: return e.code, e.read().decode()
root = pathlib.Path(__file__).resolve().parent.parent / "rpi" / "web"
def build(f, d=root):   # el mismo montaje que gen_web_pages.py
    h = (d / f).read_text(encoding="utf-8")
    rd = lambda n: (d / n).read_text(encoding="utf-8").strip()
    return h.replace("/*@SHARED_CSS@*/", rd("shared.css")).replace("/*@ANALYSIS@*/", rd("analysis.js")).replace("/*@THEME_JS@*/", rd("theme.js")).rstrip()
for path, f in (("/", "panel.html"), ("/calibrate", "calibrate.html"), ("/ble", "ble.html")):
    c, b = get(path); check(f"-w: {path} sale idéntica al montaje del generador", c == 200 and b.strip() == build(f).strip(), (c, len(b)))
c, b = get("/calibrate"); check("-w: no quedan marcas sin sustituir", "@SHARED_CSS@" not in b and "@THEME_JS@" not in b and "@ANALYSIS@" not in b)
for bad in ("/../../etc/passwd", "/panel.html", "/%2e%2e/%2e%2e/etc/passwd"):
    c, b = get(bad); check(f"-w: {bad} no sirve ficheros de la carpeta -> 404", c == 404 and "root:" not in b, c)
print("RESULTADO WEBDIR:", "TODO OK" if ok else "HAY FALLOS"); sys.exit(0 if ok else 1)
