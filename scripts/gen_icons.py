#!/usr/bin/env python3
"""Genera los iconos PNG de la PWA (rpi/web/icons/) a partir de rpi/web/icon.svg con Chromium (Playwright).
Los PNG están en el repo: solo hace falta volver a ejecutar esto si cambias icon.svg. Después: python3 scripts/gen_web_pages.py
Icono según la HIG: cuadrado a sangre, sin esquinas redondeadas ni texto (el sistema aplica la máscara); contenido centrado dentro de la zona segura."""
import pathlib
from playwright.sync_api import sync_playwright
root = pathlib.Path(__file__).resolve().parent.parent
web = root / "rpi" / "web"
svg = (web / "icon.svg").read_text(encoding="utf-8")
sizes = {"icon-180.png": 180, "icon-192.png": 192, "icon-512.png": 512, "icon-maskable-512.png": 512}
with sync_playwright() as p:
    b = p.chromium.launch()
    for name, s in sizes.items():
        pg = b.new_page(viewport={"width": s, "height": s}, device_scale_factor=1)
        pg.set_content(f'<body style="margin:0;background:#000">{svg.replace("<svg ", f"<svg width=\'{s}\' height=\'{s}\' ", 1)}</body>')
        pg.screenshot(path=str(web / "icons" / name), omit_background=False)
        pg.close()
    b.close()
print("iconos generados en", web / "icons")
