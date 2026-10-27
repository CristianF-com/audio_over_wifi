#!/usr/bin/env python3
"""Autenticación HTTP Basic de la web (harness en modo 'auth', puerto 18081)."""
import urllib.request, urllib.error, base64, sys
ok = True
def check(n, c, x=""):
    global ok; print(("PASS " if c else "FAIL ") + n + (f"  [{x}]" if x else "")); ok &= bool(c)
def get(path, user=None, pw=None):
    req = urllib.request.Request("http://127.0.0.1:18081" + path)
    if user is not None: req.add_header("Authorization", "Basic " + base64.b64encode(f"{user}:{pw}".encode()).decode())
    try:
        r = urllib.request.urlopen(req, timeout=3); return r.status, dict(r.headers)
    except urllib.error.HTTPError as e: return e.code, dict(e.headers)
c, h = get("/api/status");                           check("sin credenciales -> 401", c == 401, c)
check("401 incluye WWW-Authenticate Basic", "Basic" in h.get("WWW-Authenticate", ""), h.get("WWW-Authenticate"))
c, _ = get("/");                                     check("la SPA también exige contraseña", c == 401, c)
c, _ = get("/api/status", "x", "mal");               check("contraseña incorrecta -> 401", c == 401, c)
c, _ = get("/api/status", "sonos", "s3cret");        check("prefijo de la contraseña -> 401", c == 401, c)
c, _ = get("/api/status", "sonos", "s3cret:clave");  check("contraseña correcta (con ':' dentro) -> 200", c == 200, c)
c, _ = get("/api/status", "otro", "s3cret:clave");   check("el nombre de usuario es libre -> 200", c == 200, c)
c, _ = get("/calibrate", "sonos", "s3cret:clave");   check("/calibrate con credenciales -> 200", c == 200, c)
print("RESULTADO AUTH:", "TODO OK" if ok else "HAY FALLOS"); sys.exit(0 if ok else 1)
