// web_pages.h — GENERADO por scripts/gen_web_pages.py a partir de rpi/web/*.html. NO editar a mano.
//   PAGE_PANEL : panel principal (/)
//   PAGE_CAL   : calibración acústica con el micrófono del móvil (/calibrate, requiere HTTPS)
//   PAGE_BLE   : configuración de los ESP32 por Web Bluetooth    (/ble, requiere HTTPS)
// El bloque ANALYSIS_BEGIN/END de PAGE_CAL son funciones puras: tests/test_analysis.js lo extrae y ejecuta en Node.
#pragma once

static const char PAGE_PANEL[] = R"HTML(<!DOCTYPE html>
<html lang="es"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#000000"><meta name="color-scheme" content="dark light">
<meta name="mobile-web-app-capable" content="yes"><meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="SonosWiFi"><meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<link rel="manifest" href="/manifest.webmanifest">
<link rel="icon" href="/icon.svg" type="image/svg+xml"><link rel="icon" href="/icons/icon-192.png" sizes="192x192" type="image/png">
<link rel="apple-touch-icon" href="/icons/icon-180.png">
<script>if('serviceWorker' in navigator&&window.isSecureContext)addEventListener('load',function(){navigator.serviceWorker.register('/sw.js').catch(function(){});});</script>
<script>/* Tema de la interfaz: «dark» (por defecto), «graphite», «light» o «auto» (sigue al sistema; resuelve a dark o light). Se guarda en localStorage (por navegador).
   Se ejecuta en el <head> (antes de pintar) para que no haya destello; los botones [data-theme-btn] rotan Oscuro → Grafito → Claro → Automático. */
(function () {
  var KEY = 'sonos_theme', ORDER = ['dark', 'graphite', 'light', 'auto'], NAME = { dark: 'Oscuro', graphite: 'Grafito', light: 'Claro', auto: 'Automático' };
  var META = { dark: '#000000', graphite: '#1c1c1e', light: '#d5d9e0' };
  var ICON = {
    dark: '<path d="M20 14.5A8 8 0 0 1 9.5 4 8 8 0 1 0 20 14.5z"/>',
    graphite: '<rect x="3.5" y="5" width="17" height="14" rx="3"/><path d="M3.5 14h17" /><path d="M3.5 14v2a3 3 0 0 0 3 3h11a3 3 0 0 0 3-3v-2z" fill="currentColor"/>',
    light: '<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/>',
    auto: '<circle cx="12" cy="12" r="8.5"/><path d="M12 3.5v17a8.5 8.5 0 0 0 0-17z" fill="currentColor"/>'
  };
  var mq = window.matchMedia ? matchMedia('(prefers-color-scheme: light)') : null;
  function pref() { var p = 'dark'; try { p = localStorage.getItem(KEY) || 'dark'; } catch (e) {} return ORDER.indexOf(p) < 0 ? 'dark' : p; }
  function apply() {
    var p = pref(), t = p === 'auto' ? (mq && mq.matches ? 'light' : 'dark') : p;
    document.documentElement.setAttribute('data-theme', t);
    var m = document.querySelector('meta[name=theme-color]'); if (m) m.setAttribute('content', META[t]);
    var bs = document.querySelectorAll('[data-theme-btn]');
    for (var i = 0; i < bs.length; i++) {
      var b = bs[i];
      b.innerHTML = '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + ICON[p] + '</svg><span class="tl">' + NAME[p] + '</span>';
      b.setAttribute('aria-label', 'Tema: ' + NAME[p] + '. Pulsa para cambiar');
      b.title = 'Tema: ' + NAME[p];
    }
  }
  function set(p) { try { localStorage.setItem(KEY, p); } catch (e) {} apply(); }
  function next() { set(ORDER[(ORDER.indexOf(pref()) + 1) % ORDER.length]); }
  window.SonosTheme = { get: pref, set: set, next: next, apply: apply };
  apply();
  if (mq && mq.addEventListener) mq.addEventListener('change', apply);
  addEventListener('storage', function (e) { if (e.key === KEY) apply(); });
  document.addEventListener('DOMContentLoaded', function () {
    var bs = document.querySelectorAll('[data-theme-btn]');
    for (var i = 0; i < bs.length; i++) bs[i].addEventListener('click', next);
    apply();
  });
})();</script>
<title>SonosWiFi</title>
<style>
/* Sistema de diseño compartido por las tres páginas (panel, /calibrate, /ble). Negro puro + azul señal. */
:root{
  --r-card:24px; --r-sheet:28px;
  --bg:#000; --panel:#0a0d13; --raise:#121722; --line:#1f2735; --line2:#2b3548;
  --fg:#e9edf5; --mut:#8a93a6; --ac:#4aa3ff; --ac-bg:rgba(74,163,255,.13); --ac-ln:rgba(74,163,255,.5);
  --ctl:#6b7a96;                                   /* borde de controles (campos, interruptor apagado): ≥3:1 sobre las superficies */
  --ok:#3ecf8e; --warn:#f5b84a; --bad:#ff6b6b;
  --on-ac:#04121f; --ac-hi:#9ad0ff; --ac-lo:#2f6fb3; --bad-fg:#ffb0b0; --bad-ln:rgba(255,107,107,.45); --warn-ln:rgba(245,184,74,.4);
  --top-bg:rgba(0,0,0,.88); --top-solid:#000; --tabs-bg:rgba(10,13,19,.94); --scrim:rgba(0,0,0,.55); --code-bg:#05070b; --bar-bg:#000; --cl-off:#3a1512;
  --thumb:#fff; --thumb-ring:rgba(74,163,255,.38); --cap-bg:rgba(0,0,0,.65); --glow:rgba(74,163,255,.09); --orb-glow:rgba(74,163,255,.35);
  --led-g:#30d158; --led-y:#ffd60a; --led-r:#ff453a;
  color-scheme:dark;
}
/* Temas alternativos: data-theme="graphite" (gris grafito, de bajo brillo) y "light" (claro suave, sin blanco puro).
   El script del <head> (theme.js) resuelve «Automático» según el sistema. */
html[data-theme=graphite]{
  --bg:#1c1c1e; --panel:#2b2b2d; --raise:#363639; --line:#3d3d41; --line2:#4c4c51;
  --fg:#e9e9ec; --mut:#a3a3aa; --ac:#5aaeff; --ac-bg:rgba(90,174,255,.15); --ac-ln:rgba(90,174,255,.55);
  --ctl:#8a8a91; --ok:#46d39a; --warn:#f0b64a; --bad:#ff7b72;
  --on-ac:#06121f; --ac-hi:#a5d3ff; --ac-lo:#3478b8; --bad-fg:#ffb4ae; --bad-ln:rgba(255,123,114,.5); --warn-ln:rgba(240,182,74,.45);
  --top-bg:rgba(28,28,30,.9); --top-solid:#1c1c1e; --tabs-bg:rgba(43,43,45,.95); --scrim:rgba(0,0,0,.5); --code-bg:#1f1f21; --bar-bg:#18181a; --cl-off:#4a2a28;
  --thumb:#f2f2f4; --thumb-ring:rgba(90,174,255,.4); --cap-bg:rgba(28,28,30,.78); --glow:rgba(90,174,255,.07); --orb-glow:rgba(90,174,255,.35);
  --led-g:#34c759; --led-y:#ffd60a; --led-r:#ff5a50;
  color-scheme:dark;
}
html[data-theme=light]{
  --bg:#d5d9e0; --panel:#e4e7ec; --raise:#d9dde4; --line:#c3c8d1; --line2:#aab1bd;
  --fg:#1a1f27; --mut:#4b5465; --ac:#0a58ac; --ac-bg:rgba(10,88,172,.11); --ac-ln:rgba(10,88,172,.45);
  --ctl:#667184; --ok:#0f6240; --warn:#7a4a00; --bad:#a82620;
  --on-ac:#ffffff; --ac-hi:#4aa3ff; --ac-lo:#7fa9d6; --bad-fg:#8f1a16; --bad-ln:rgba(179,42,37,.5); --warn-ln:rgba(138,83,0,.45);
  --top-bg:rgba(213,217,224,.92); --top-solid:#d5d9e0; --tabs-bg:rgba(228,231,236,.95); --scrim:rgba(26,31,39,.35); --code-bg:#cfd4dc; --bar-bg:#0b0e14; --cl-off:#e0c3c1;
  --thumb:#f4f5f8; --thumb-ring:rgba(10,88,172,.35); --cap-bg:rgba(228,231,236,.88); --glow:rgba(10,88,172,.06); --orb-glow:rgba(10,88,172,.3);
  --led-g:#1fa84a; --led-y:#d9a400; --led-r:#e0342b;
  color-scheme:light;
}
*{box-sizing:border-box;margin:0}
html{font-size:106.25%}          /* 17 px: tamaño de texto por defecto de iOS; todo lo demás va en rem y sigue el ajuste del navegador */
body{background:var(--bg);color:var(--fg);font:1rem/1.45 var(--font);-webkit-font-smoothing:antialiased;
  font-variant-numeric:tabular-nums;min-height:100vh;-webkit-text-size-adjust:100%;
  padding-left:env(safe-area-inset-left);padding-right:env(safe-area-inset-right)}
a{color:var(--ac);text-decoration:none}a:hover{text-decoration:underline}
button,input,select{font:inherit;color:inherit}
button{cursor:pointer}
[hidden]{display:none!important}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px;border-radius:6px}
h1{font-size:1.55rem;font-weight:650;letter-spacing:-.01em}
h2{font-size:1.02rem;font-weight:650}
.mut{color:var(--mut)}
.small{font-size:.86rem}

.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;text-decoration:none;line-height:1.2;font-family:inherit;appearance:none;border:0;background:var(--ac);color:var(--on-ac);font-weight:650;padding:10px 22px;border-radius:999px;min-height:44px;box-shadow:0 8px 22px -10px var(--orb-glow);
  transition:filter .12s,transform .06s,background .12s}
.btn:hover{filter:brightness(1.1);text-decoration:none}.btn:active{transform:translateY(1px)}
.btn.ghost{background:var(--raise);color:var(--fg);border:1px solid var(--line2);font-weight:600;box-shadow:none}
.btn.ghost:hover{background:var(--line);filter:none}
.btn.danger{background:transparent;color:var(--bad);border:1px solid var(--bad-ln);font-weight:600;box-shadow:none}
.btn.sm{padding:6px 16px;min-height:36px;font-size:.88rem}
.btn:disabled{opacity:.4;cursor:not-allowed;filter:none}

.seg{display:inline-flex;background:var(--code-bg);border:1px solid var(--line);border-radius:999px;padding:4px;gap:2px;max-width:100%}
.seg button{appearance:none;border:0;background:transparent;color:var(--mut);padding:7px 16px;border-radius:999px;min-height:36px;font-weight:600;white-space:nowrap;display:inline-flex;align-items:center;justify-content:center;gap:8px;transition:background .12s,color .12s}
.seg button:hover{color:var(--fg)}
.seg button[aria-pressed=true]{background:var(--ac-bg);color:var(--ac);box-shadow:inset 0 0 0 1px var(--ac-ln)}
.seg button[aria-pressed=true]::before{content:"";width:7px;height:7px;border-radius:50%;background:currentColor;flex:none}
.seg.block{display:flex}.seg.block button{flex:1;min-width:0;white-space:normal;line-height:1.2;padding-left:8px;padding-right:8px}   /* con texto grande o pantalla estrecha, las etiquetas bajan de línea en vez de salirse */

.switch{appearance:none;position:relative;width:52px;height:32px;border-radius:16px;border:1px solid var(--ctl);background:var(--raise);flex:none;
  transition:background .15s,border-color .15s}
.switch::after{content:"";position:absolute;top:3px;left:3px;width:24px;height:24px;border-radius:50%;background:var(--mut);transition:transform .15s,background .15s}
.switch[aria-checked=true]{background:var(--ac);border-color:var(--ac)}
.switch[aria-checked=true]::after{transform:translateX(20px);background:var(--on-ac)}

input[type=range].rng{-webkit-appearance:none;appearance:none;width:100%;height:28px;background:transparent;border-radius:0;cursor:pointer;
  --f:.5;--rh:28px;height:var(--rh);
  background:linear-gradient(to right,var(--ac) calc(var(--f)*(100% - 38px) + 19px),var(--line2) 0) no-repeat center/100% 10px}
input[type=range].rng.mid{--lo:.5;--hi:.5;
  background:linear-gradient(to right,var(--line2) calc(var(--lo)*(100% - 38px) + 19px),var(--ac) 0,var(--ac) calc(var(--hi)*(100% - 38px) + 19px),var(--line2) 0) no-repeat center/100% 10px}
.rng::-webkit-slider-runnable-track{height:var(--rh);background:transparent}
.rng::-moz-range-track{height:var(--rh);background:transparent}
.rng::-webkit-slider-thumb{-webkit-appearance:none;width:38px;height:26px;border-radius:13px;background:var(--thumb);border:0;margin-top:calc((var(--rh) - 26px)/2);box-shadow:0 2px 8px rgba(0,0,0,.35),0 0 0 3px var(--thumb-ring)}
.rng::-moz-range-thumb{width:38px;height:26px;border-radius:13px;background:var(--thumb);border:0;box-shadow:0 2px 8px rgba(0,0,0,.35),0 0 0 3px var(--thumb-ring)}
.rng:focus-visible{outline-offset:0}

.num,.txt,.sel{background:var(--raise);border:1px solid var(--ctl);border-radius:14px;padding:9px 14px;min-height:44px;width:100%;color:var(--fg)}
.num{width:110px}
.num:focus,.txt:focus,.sel:focus{outline:2px solid var(--ac);outline-offset:1px;border-color:transparent}
.sel{appearance:none;background-image:linear-gradient(45deg,transparent 50%,var(--mut) 50%),linear-gradient(135deg,var(--mut) 50%,transparent 50%);
  background-position:calc(100% - 21px) 19px,calc(100% - 16px) 19px;background-size:5px 5px,5px 5px;background-repeat:no-repeat;padding-right:32px}
label,.lbl{display:block;color:var(--mut);font-size:.74rem;font-weight:600;letter-spacing:.07em;text-transform:uppercase;margin-bottom:7px}

.chip{display:inline-flex;align-items:center;gap:7px;padding:5px 13px;border-radius:999px;background:var(--raise);border:1px solid var(--line);font-size:.84rem;color:var(--mut)}
.chip.ac{color:var(--ac);border-color:var(--ac-ln);background:var(--ac-bg)}
.chip.warn{color:var(--warn);border-color:var(--warn-ln)}
.chip.bad{color:var(--bad);border-color:var(--bad-ln)}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--mut);flex:none}
.dot.ok{background:var(--ok)}.dot.bad{background:var(--bad)}.dot.warn{background:var(--warn)}

.top{position:sticky;top:0;z-index:30;background:var(--top-bg);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);border-bottom:1px solid var(--line);
  padding-top:env(safe-area-inset-top)}               /* en la app instalada (barra de estado translúcida) el contenido no queda bajo la muesca */
.top .in{max-width:1180px;margin:auto;display:flex;align-items:center;gap:18px;padding:10px 18px;flex-wrap:wrap}
.brand{display:flex;align-items:center;gap:10px;font-weight:700;font-size:1.05rem;color:var(--fg)}
.brand:hover{text-decoration:none}
.logo{width:26px;height:26px;flex:none;color:var(--ac)}
.panelbox{background:var(--panel);border:1px solid var(--line);border-radius:var(--r-card);padding:20px}
.note{border-radius:16px;padding:12px 14px;background:var(--raise);border:1px solid var(--line2);font-size:.92rem}
.note.bad{border-color:var(--bad-ln);color:var(--bad-fg)}.note.warn{border-color:var(--warn-ln)}

.toasts{position:fixed;left:50%;bottom:calc(18px + env(safe-area-inset-bottom));transform:translateX(-50%);display:flex;flex-direction:column;gap:6px;z-index:60;pointer-events:none;width:max-content;max-width:92vw}
.toast{pointer-events:auto;display:flex;align-items:center;background:var(--raise);border:1px solid var(--line2);padding:10px 16px;border-radius:999px;font-size:.9rem;
  animation:tin .18s ease-out}
.toast::before{content:"";flex:none;width:9px;height:9px;border-radius:50%;background:var(--ok);margin-right:10px}
.toast.bad::before{background:var(--bad)}
@keyframes tin{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
.procbar{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin:0 0 14px;padding:10px 18px;border-radius:999px;background:var(--ac-bg);border:1px solid var(--ac-ln);color:var(--ac);font-size:.92rem}
.procbar[hidden]{display:none}

/* Objetivos táctiles: 44 × 44 (mínimo cómodo de iOS) cuando el puntero es el dedo */
@media (pointer:coarse){
  .btn,.btn.sm,.seg button,.tab,.num,.txt,.sel{min-height:44px}
  input[type=range].rng{--rh:44px}
  .switch::before{content:"";position:absolute;inset:-9px -6px}   /* zona de toque ampliada; el aspecto no cambia */
}
.themebtn{display:inline-flex;align-items:center;gap:6px}
.themebtn svg{flex:none}
@media (max-width:760px){.themebtn .tl{display:none}.themebtn{padding-left:10px;padding-right:10px}}
/* Ajustes de accesibilidad del sistema */
@media (prefers-contrast:more){
  :root{--mut:#b4bccc;--ctl:#8d9ab3;--line2:#6b7a96;--ac:#7ec0ff;--ac-ln:rgba(126,192,255,.8)}
  html[data-theme=light]{--mut:#323b4a;--ctl:#4a5468;--line2:#7d8798;--ac:#0a4f99;--ac-ln:rgba(10,79,153,.8)}
  html[data-theme=graphite]{--mut:#c6c6cc;--ctl:#a8a8af;--line2:#7d7d84;--ac:#8cc6ff;--ac-ln:rgba(140,198,255,.8)}
}
@media (prefers-reduced-transparency:reduce){
  .top{background:var(--top-solid);backdrop-filter:none;-webkit-backdrop-filter:none}
}
main{max-width:1180px;margin:0 auto;padding:18px 18px 90px}
.top .in{gap:14px}
.mv{display:flex;align-items:center;gap:10px;margin-left:auto;min-width:210px;flex:0 1 300px}
.mv svg{width:20px;height:20px;flex:none;color:var(--mut)}
.mv .rng{flex:1}.mv b{min-width:42px;text-align:right;font-weight:600}
.links{display:flex;gap:6px}
.tabs{display:inline-flex;gap:4px;overflow-x:auto;margin:6px 0 20px;padding:4px;background:var(--code-bg);border:1px solid var(--line);border-radius:999px;scrollbar-width:none;max-width:100%}
.tabs::-webkit-scrollbar{display:none}
.tab{appearance:none;background:none;border:0;color:var(--mut);padding:9px 20px;font-weight:600;border-radius:999px;white-space:nowrap;transition:background .12s,color .12s}
.tab svg{display:none;width:24px;height:24px;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}
.tab:hover{color:var(--fg)}
.tab[aria-selected=true]{color:var(--ac);background:var(--ac-bg);box-shadow:inset 0 0 0 1px var(--ac-ln)}
.pane[hidden]{display:none}
.head{display:flex;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:14px}
.head h1{margin-right:auto}

/* Plano de sala: cada canal es un altavoz dibujado; se coloca donde está en la sala */
.roombar{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:10px}
.roombar .hint{margin-right:auto;min-width:200px;flex:1}
.room{--hw:64px;--ht:80px;--hb:72px;user-select:none;-webkit-user-select:none;position:relative;width:100%;aspect-ratio:16/9;min-height:360px;border:1px solid var(--line);border-radius:var(--r-sheet);overflow:hidden;
  background:radial-gradient(circle at 50% 78%,var(--glow),transparent 62%),var(--panel)}
.room>*{z-index:1}
.room{touch-action:pan-y}                       /* la página sigue deslizándose; el pellizco lo gestiona el plano */
.room.zoomed{touch-action:none;cursor:grab}
.room.lod .spk .cap,.room.lod .spk .cal{display:none}   /* alejado: solo el altavoz, su canal y su vúmetro */
.room.zoomed.panning{cursor:grabbing}
.room>.radar{position:absolute;left:0;top:0;width:100%;height:100%;z-index:0;pointer-events:none}
.radar .ring{fill:none;stroke:var(--line);stroke-width:1}
.radar .ring.m{stroke:var(--line2)}
.radar .ax{stroke:var(--line2);stroke-width:1;stroke-dasharray:2 5}
.radar .walls{fill:none;stroke:var(--ctl);stroke-width:1.5;stroke-dasharray:6 5}
.radar text{font-size:11px;font-weight:600;fill:var(--mut);paint-order:stroke;stroke:var(--panel);stroke-width:3px;stroke-linejoin:round}
.dragging,.dragging *{user-select:none!important;-webkit-user-select:none!important}
.room.edit{border-color:var(--ac-ln);box-shadow:0 0 0 3px var(--ac-bg)}
.rl{position:absolute;font-size:.72rem;color:var(--mut);pointer-events:none}
.rl.back{left:50%;bottom:6px;transform:translateX(-50%)}
.me{--s:1;position:absolute;left:50%;top:78%;transform:translate(-50%,-50%) scale(var(--s));width:64px;height:64px;box-shadow:0 0 0 6px var(--ac-bg);border-radius:50%;border:1px solid var(--line2);background:var(--raise);
  display:grid;place-items:center;color:var(--mut)}
.me svg{width:32px;height:32px;color:var(--ac)}
.me small{position:absolute;top:calc(100% + 6px);left:50%;transform:translateX(-50%);font-size:.8rem;font-weight:600;color:var(--mut);white-space:nowrap}
.screen{position:absolute;left:30%;right:30%;top:1.5%;height:18px;border-radius:5px;background:var(--ctl);display:grid;place-items:center;font-size:.68rem;font-weight:600;color:var(--bg);letter-spacing:.03em}
.spk{--s:1;position:absolute;transform:translate(-50%,-50%) scale(var(--s));display:flex;flex-direction:column;align-items:center;gap:2px;background:none;border:0;padding:4px 6px;color:var(--fg);z-index:2;border-radius:12px}
.cab{position:relative;display:block;width:46px;height:62px;transition:transform .12s}
.spk.sub .cab{width:56px;height:56px}
.cab svg{width:100%;height:100%;display:block;overflow:visible}
.cab .body{fill:var(--raise);stroke:var(--ac);stroke-width:1.6}
.cab .tw{fill:var(--panel);stroke:var(--ctl);stroke-width:1.4}
.cab .wf{fill:var(--panel);stroke:var(--ctl);stroke-width:1.6}
.cab .cone{fill:var(--ctl)}
.spk:hover .cab{transform:scale(1.05)}
.spk[aria-expanded=true]{outline:2px solid var(--ac);outline-offset:0;background:var(--ac-bg)}
.spk.off .cab{opacity:.5}
.spk.off .cab .body{stroke:var(--ctl);stroke-dasharray:4 3}
.spk .st{position:absolute;right:-5px;top:-5px;width:14px;height:14px;border-radius:50%;border:2px solid var(--panel);background:var(--bad)}
.spk .st.on{background:var(--ok)}
.spk .ch{font-size:1.02rem;font-weight:700;letter-spacing:.01em;line-height:1.1}
.spk .cap{display:flex;align-items:center;gap:5px;font-size:.72rem;color:var(--mut);background:var(--cap-bg);padding:1px 6px;border-radius:6px;white-space:nowrap}
.spk .cap .dot{width:7px;height:7px}
.spk .cal{display:block;text-align:center;font:500 .72rem/1.2 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-variant-numeric:tabular-nums;color:var(--fg);background:var(--cap-bg);padding:1px 7px;border-radius:6px;white-space:nowrap}
.spk .cal i{font-style:normal;color:var(--mut)}
.room.edit .spk{cursor:grab;touch-action:none;outline:1.5px dashed var(--ac-ln);outline-offset:0}
.room.edit .spk.drag{cursor:grabbing;z-index:5;outline:2px solid var(--ac);background:var(--ac-bg)}
.room.edit .spk.drag .cab{transform:scale(1.1)}
.room>.zoomctl{position:absolute;right:10px;top:10px;z-index:6;display:flex;align-items:center;background:var(--raise);border:1px solid var(--line2);border-radius:999px;padding:2px;box-shadow:0 6px 18px -8px rgba(0,0,0,.5);touch-action:manipulation}
.zoomctl button{appearance:none;border:0;background:none;color:var(--fg);min-width:44px;min-height:44px;border-radius:999px;font-size:1.4rem;font-weight:600;line-height:1;display:grid;place-items:center}
.zoomctl button:hover:not(:disabled){background:var(--ac-bg)}
.zoomctl button:disabled{opacity:.35;cursor:default}
.zoomctl .zv{font:500 .8rem ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-variant-numeric:tabular-nums;min-width:60px;color:var(--mut)}
.legend{display:flex;gap:16px;flex-wrap:wrap;margin-top:12px;color:var(--mut);font-size:.85rem}
.chips{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px}
.chips button{appearance:none;background:var(--raise);border:1px solid var(--line2);border-radius:999px;padding:7px 16px 7px 13px;display:flex;align-items:center;gap:8px;min-height:38px}
.chips button:hover{border-color:var(--line2)}
.chips button[aria-expanded=true]{border-color:var(--ac-ln);background:var(--ac-bg)}
.empty{padding:26px;text-align:center;color:var(--mut)}

/* Vúmetros de entrada */
.meters{background:var(--panel);border:1px solid var(--line);border-radius:var(--r-card);padding:12px 14px 10px;margin-bottom:14px}
.mh{display:flex;align-items:baseline;gap:10px;margin-bottom:8px}.mh b{font-weight:650}.mh .hint{margin-left:auto}
.mb{display:grid;grid-template-columns:repeat(10,1fr);gap:8px}
.mc{display:flex;flex-direction:column;align-items:center;gap:6px;min-width:0}
.mc .lab{font-size:.72rem;font-weight:600;color:var(--mut);line-height:1}
/* Indicador de señal: cuadrado rojo (parpadea) si entra señal en ese canal, apagado si no */
.lamp{display:block;width:22px;height:22px;border-radius:5px;background:var(--bar-bg);border:1.5px solid var(--ctl)}
.lamp.on{background:var(--led-r);border-color:var(--led-r);box-shadow:0 0 10px 1px var(--led-r);animation:lampblink .7s ease-in-out infinite alternate}
@keyframes lampblink{from{opacity:1}to{opacity:.4}}
@media (prefers-reduced-motion:reduce){.lamp.on{animation:none}}
.mc .lvn{font:300 .78rem/1 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-variant-numeric:tabular-nums;color:var(--fg);min-height:1em}
.ms{color:var(--mut);font-size:.74rem;margin-top:10px}
.meters.off .lamp.on{animation:none;opacity:.4}
/* Vúmetro vertical junto a cada altavoz: misma altura que el dibujo */
.vu{position:absolute;left:calc(100% + 8px);top:0;bottom:0;width:9px;display:flex;flex-direction:column-reverse;gap:1px;padding:1px;background:var(--bar-bg);border:1px solid var(--ctl);border-radius:3px;pointer-events:none}
.led{display:block;flex:1 1 0;min-height:0;border-radius:1px;background:var(--c);opacity:.16}   /* LED apagado: el mismo color, muy tenue */
.led.on{opacity:1}
.led.g{--c:var(--led-g)}.led.y{--c:var(--led-y)}.led.r{--c:var(--led-r)}
/* Cajón del altavoz */
.scrim{position:fixed;inset:0;background:var(--scrim);z-index:40;opacity:0;pointer-events:none;transition:opacity .18s}
.scrim.on{opacity:1;pointer-events:auto}
.drawer{position:fixed;top:12px;right:12px;bottom:12px;width:min(420px,calc(100% - 24px));border-radius:var(--r-sheet);overflow:hidden;box-shadow:0 24px 60px -20px rgba(0,0,0,.6);background:var(--panel);border:1px solid var(--line2);z-index:50;
  transform:translateX(calc(100% + 40px));transition:transform .2s ease-out;display:flex;flex-direction:column}
.drawer.on{transform:none}
.grab{display:none;flex:none;width:100%;padding:8px 0 2px;touch-action:none}
.grab::before{content:"";display:block;width:38px;height:5px;border-radius:3px;background:var(--ctl);margin:0 auto}
.drawer header{touch-action:none;display:flex;align-items:center;gap:10px;padding:16px 18px;border-bottom:1px solid var(--line)}
.drawer header h2{font-size:1.15rem;margin-right:auto}
.x{appearance:none;border:0;background:var(--raise);width:36px;height:36px;border-radius:50%;font-size:1.1rem;color:var(--mut)}
.x:hover{color:var(--fg)}
.dbody{overflow-y:auto;padding:16px 18px 30px;display:flex;flex-direction:column;gap:18px}
.fld .top2{display:flex;justify-content:space-between;align-items:baseline;gap:8px}
.fld .top2 label{margin:0}.fld .top2 b{font-weight:600}
.two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.sw{display:flex;align-items:center;justify-content:space-between;gap:12px}
.sw b{display:block;font-weight:600}
.eqrow{display:grid;grid-template-columns:64px 1fr 46px;align-items:center;gap:8px}
.eqrow span{color:var(--mut);font-size:.84rem}.eqrow output{text-align:right;font-size:.88rem}
.sep{border:0;border-top:1px solid var(--line)}
.savestate{font-size:.82rem;color:var(--mut);min-height:1.2em}

/* Sonido */
.flow{display:flex;align-items:stretch;gap:0;margin:0 0 20px;overflow-x:auto;padding-bottom:4px}
.fs{flex:1 1 0;min-width:112px;border:1px solid var(--ac-ln);background:var(--ac-bg);border-radius:14px;padding:12px 12px;text-align:center}
.fs b{display:block;font-weight:650}.fs span{font-size:.8rem;color:var(--mut)}
.fs.bp{border:1px dashed var(--line2);background:transparent}.fs.bp b{color:var(--mut)}
.fs.io{border-color:var(--line2);background:var(--panel)}
.arr{align-self:center;color:var(--line2);padding:0 6px;font-size:1.2rem;flex:none}
.tiles{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:18px}
.tile{appearance:none;text-align:left;border:1.5px solid var(--line);background:var(--panel);border-radius:var(--r-card);padding:22px;display:flex;flex-direction:column;gap:6px;transition:border-color .12s,background .12s}
.tile:hover{border-color:var(--line2)}
.tile[aria-pressed=true]{border-color:var(--ac);background:var(--ac-bg)}
.tile h2{font-size:1.2rem}.tile p{color:var(--mut)}
.tile .tag{align-self:flex-start;margin-top:6px}
.cards{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:var(--r-card);padding:20px;display:flex;flex-direction:column;gap:14px}
.card.dim>:not(.sw){opacity:.65}                     /* se atenúa el contenido, no el interruptor que lo activa */
.card p{color:var(--mut);font-size:.92rem}
.hint{font-size:.88rem;color:var(--mut)}

/* Latencia */
.cmd{display:block;background:var(--code-bg);border:1px solid var(--line);border-radius:10px;padding:10px 12px;font-family:ui-monospace,Consolas,monospace;font-size:.85rem;word-break:break-all;color:var(--fg)}
.big{font-family:ui-monospace,"SF Mono",SFMono-Regular,Menlo,Consolas,monospace;font-size:2.5rem;font-weight:300;letter-spacing:-.03em;line-height:1.1}
.big small{font-size:1rem;color:var(--mut);font-weight:500;margin-left:4px}
table{width:100%;border-collapse:collapse;font-size:.9rem}
th{font-weight:550;color:var(--mut);text-align:right;padding:8px 10px;border-bottom:1px solid var(--line);white-space:nowrap}
td{padding:9px 10px;text-align:right;border-bottom:1px solid var(--line)}
th:first-child,td:first-child{text-align:left}
td.bad{color:var(--bad)}td.warn{color:var(--warn)}
.tw{overflow-x:auto;border:1px solid var(--line);border-radius:14px;background:var(--panel)}
.tw table{min-width:640px}.tw tr:last-child td{border-bottom:0}
.log{background:var(--code-bg);border:1px solid var(--line);border-radius:12px;padding:12px;font:.84rem/1.6 ui-monospace,Consolas,monospace;height:340px;overflow-y:auto;color:var(--mut)}
.log b{color:var(--ac);font-weight:500}

.found{border:1px solid var(--line);background:var(--bg);border-radius:20px;padding:12px 14px;display:flex;flex-direction:column;gap:10px}
.found .hd{display:flex;align-items:center;gap:8px;flex-wrap:wrap}.found .hd b{font-weight:650;margin-right:auto}
.found .ac{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.found .ac .sel{width:auto;min-width:130px;flex:1}
.found.done{border-color:var(--ok)}
.found .res{font-size:.88rem}.found .res.ok{color:var(--ok)}.found .res.bad{color:var(--bad)}
.spin{display:inline-block;width:14px;height:14px;border:2px solid var(--line2);border-top-color:var(--ac);border-radius:50%;animation:sp .8s linear infinite;vertical-align:-2px;margin-right:6px}
@keyframes sp{to{transform:rotate(360deg)}}
@media (max-width:760px){
  main{padding:12px 12px calc(104px + env(safe-area-inset-bottom))}
  /* Barra de pestañas inferior (al alcance del pulgar), con icono y una sola palabra */
  /* flotante, como una píldora: no toca los bordes de la pantalla */
  .tabs{position:fixed;left:calc(14px + env(safe-area-inset-left));right:calc(14px + env(safe-area-inset-right));bottom:calc(10px + env(safe-area-inset-bottom));z-index:35;margin:0;gap:2px;overflow:visible;justify-content:space-around;
    border:1px solid var(--line2);border-radius:32px;background:var(--tabs-bg);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);
    padding:6px;box-shadow:0 14px 34px -12px rgba(0,0,0,.55)}
  .tab{flex:1;display:flex;flex-direction:column;align-items:center;gap:2px;padding:6px 2px;font-size:.72rem;min-height:52px;border-radius:26px}
  .tab svg{display:block}
  .toasts{bottom:calc(88px + env(safe-area-inset-bottom))}
  .grab{display:block}
  .drawer{padding-bottom:env(safe-area-inset-bottom)}
  .top{position:static}                                /* la navegación ya va en la barra inferior: la cabecera no ocupa pantalla fija */
  .top .in{padding:8px 12px;gap:8px}
  .mv{order:5;flex:1 1 100%;margin-left:0}
  .links{margin-left:auto}
  .room{aspect-ratio:9/16;min-height:640px;--hw:60px;--ht:78px;--hb:70px}
  .cab{width:40px;height:54px}.spk.sub .cab{width:48px;height:48px}.spk .ch{font-size:.92rem}.spk .cap{font-size:.66rem;padding:1px 4px}.spk .cal{font-size:.7rem;padding:1px 5px}
  .tiles,.cards{grid-template-columns:1fr}
  .drawer{top:auto;left:0;right:0;bottom:0;width:100%;max-height:88vh;border-bottom:0;box-shadow:0 -20px 50px -24px rgba(0,0,0,.6);border-top:1px solid var(--line2);border-radius:var(--r-sheet) var(--r-sheet) 0 0;transform:translateY(calc(100% + 40px))}
  .drawer.on{transform:none}
  .flow{flex-wrap:nowrap}
}
@media (pointer:coarse){.x{width:44px;height:44px}.chips button{min-height:44px}}   /* aquí, después de las reglas base de estos componentes */
@media (prefers-reduced-transparency:reduce){.tabs{background:var(--panel);backdrop-filter:none;-webkit-backdrop-filter:none}}
</style>
</head><body>
<div class="top"><div class="in">
  <a class="brand" href="/"><svg class="logo" viewBox="0 0 26 26" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M3 13h2M8 8v10M13 4v18M18 8v10M23 13h-2"/></svg>SonosWiFi</a>
  <span class="chip" id="st"><i class="dot" id="dot"></i><span id="hs">Conectando…</span></span>
  <div class="mv" title="Volumen maestro">
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M11 5 6 9H3v6h3l5 4z"/><path d="M15.5 8.5a5 5 0 0 1 0 7M18.5 5.5a9 9 0 0 1 0 13"/></svg>
    <input class="rng" id="mvs" type="range" min="0" max="100" value="100" aria-label="Volumen maestro"><b id="mvd">100%</b>
  </div>
  <div class="links"><button class="btn ghost sm themebtn" data-theme-btn type="button"></button><a class="btn ghost sm" id="lcal" href="#">Calibrar</a><button class="btn ghost sm" id="lble">Añadir altavoces</button></div>
</div></div>

<main>
<div class="tabs" role="tablist" aria-label="Secciones">
  <button class="tab" role="tab" id="tab_spk" aria-controls="p_spk" data-tab="spk" aria-selected="true"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 9.5v5h3.5L12 18.5v-13L7.5 9.5z"/><path d="M15.5 9a4 4 0 0 1 0 6M18 6.5a7.5 7.5 0 0 1 0 11"/></svg><span>Altavoces</span></button>
  <button class="tab" role="tab" id="tab_snd" aria-controls="p_snd" data-tab="snd" aria-selected="false" tabindex="-1"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M6 4v5M6 13v7M12 4v9M12 17v3M18 4v3M18 11v9"/><circle cx="6" cy="11" r="2"/><circle cx="12" cy="15" r="2"/><circle cx="18" cy="9" r="2"/></svg><span>Sonido</span></button>
  <button class="tab" role="tab" id="tab_lat" aria-controls="p_lat" data-tab="lat" aria-selected="false" tabindex="-1"><svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="8.5"/><path d="M12 7.5V12l3 2"/></svg><span>Latencia</span></button>
  <button class="tab" role="tab" id="tab_log" aria-controls="p_log" data-tab="log" aria-selected="false" tabindex="-1"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 7h14M5 12h14M5 17h9"/></svg><span>Registro</span></button>
</div>

<section class="pane" id="p_spk" role="tabpanel" aria-labelledby="tab_spk">
  <div class="head"><h1>Tu sala</h1><span class="chip" id="onl">—</span>
</div>
  <div class="procbar" id="procbar" hidden><span id="proctxt">Procesado activo</span>
    <button class="btn ghost sm" id="procoff">Volver a pass-through</button></div>
  <div class="meters" id="meters" role="group" aria-label="Niveles de entrada por canal">
    <div class="mh"><b>Entrada</b><span class="hint" id="mtxt">Esperando datos…</span></div>
    <div class="mb" id="mb"></div>
    <div class="ms">El cuadrado rojo parpadea cuando entra señal en ese canal (por encima de −70 dBFS) y el número es su nivel en dBFS. El vúmetro de cada altavoz del plano marca de −60 a 0 dBFS: verde hasta −20, amarillo hasta −10 y rojo por encima; el segmento suelto es el pico reciente.</div>
  </div>
  <div class="roombar"><span class="hint" id="mhint">Toca un altavoz para ajustarlo. Los cambios se guardan solos.</span>
    <button class="btn ghost sm" id="mreset" hidden>Restablecer posiciones</button>
    <button class="btn ghost sm" id="medit" aria-pressed="false" aria-describedby="mhint">Mover altavoces</button></div>
  <div class="room" id="room" aria-label="Plano de la sala con los altavoces">
    <svg class="radar" id="radar" aria-hidden="true"></svg>
    <div class="screen" title="Pantalla">Pantalla</div><span class="rl back">Atrás</span>
    <div class="me" title="Posición de escucha"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><circle cx="12" cy="8" r="4"/><path d="M4 21c1-4 4-6 8-6s7 2 8 6"/></svg><small>Escucha</small></div>
    <div id="spks"></div>
    <div class="zoomctl" role="group" aria-label="Zoom del plano">
      <button type="button" id="zout" aria-label="Alejar el plano">−</button>
      <button type="button" class="zv" id="zval" aria-label="Zoom actual. Pulsa para volver al 100 %">100%</button>
      <button type="button" id="zin" aria-label="Acercar el plano">+</button>
    </div>
  </div>
  <div class="legend"><span>Cada altavoz muestra su canal, el ESP que lo maneja, su estado y, debajo, su volumen y retardo de calibración</span><span>Los graves (subwoofer) se dibujan como caja cuadrada</span><span>Zoom: pellizca o usa + y −; con zoom, arrastra el fondo para moverte. Al acercar, los altavoces no crecen; al alejar, se achican para despejar el plano</span></div>
  <div class="chips" id="chips"></div>
</section>

<section class="pane" id="p_snd" role="tabpanel" aria-labelledby="tab_snd" hidden>
  <div class="head"><h1>Sonido</h1></div>
  <div class="flow" id="flow"></div>
  <div class="tiles">
    <button class="tile" id="t_pass" aria-pressed="false">
      <h2>Pass-through</h2><p>Los canales llegan ya separados. El audio sale tal cual, solo con volumen y retardo por altavoz.</p>
      <span class="chip tag" id="tg_pass">Activo</span></button>
    <button class="tile" id="t_proc" aria-pressed="false">
      <h2>Procesado</h2><p>Llega una señal estéreo o mezclada. Se filtran los graves hacia el subwoofer y se aplica la EQ de cada altavoz.</p>
      <span class="chip tag" id="tg_proc">Activo</span></button>
  </div>
  <div class="cards">
    <div class="card" id="c_xo">
      <div class="sw"><div><b>Crossover de graves</b><span class="hint">Pasa-altos en satélites, pasa-bajos en el sub</span></div>
        <button class="switch" id="sw_xo" role="switch" aria-checked="false" aria-label="Crossover de graves"></button></div>
      <div class="fld"><div class="top2"><label for="xos">Frecuencia de corte</label><b id="xod">80 Hz</b></div>
        <input class="rng" id="xos" type="range" min="40" max="200" step="5" value="80"></div>
      <div class="fld"><label for="subsel">Subwoofer</label>
        <select class="sel" id="subsel"></select></div>
      <p id="xohint">Sin subwoofer, todos los altavoces reciben el canal completo.</p>
    </div>
    <div class="card" id="c_eq">
      <div class="sw"><div><b>Ecualizador por altavoz</b><span class="hint">6 bandas, de 354 Hz a 9,8 kHz</span></div>
        <button class="switch" id="sw_eq" role="switch" aria-checked="false" aria-label="Ecualizador"></button></div>
      <p id="eqhint">Ajusta cada altavoz desde el plano, o deja que <a id="lcal2" href="#">Calibrar</a> proponga la EQ con el micrófono del móvil.</p>
      <div id="eqsum" class="hint"></div>
    </div>
  </div>
</section>

<section class="pane" id="p_lat" role="tabpanel" aria-labelledby="tab_lat" hidden>
  <div class="head"><h1>Latencia</h1></div>
  <div class="cards">
    <div class="card">
      <div><div class="lbl">Retardo total del sistema</div><div class="big" id="bigtot">—<small>ms</small></div></div>
      <div class="fld"><div class="top2"><label for="pls">Colchón de red</label><b id="pld">80 ms</b></div>
        <input class="rng" id="pls" type="range" min="40" max="200" step="5" value="80"></div>
      <div class="seg block" id="plseg"><button data-pl="40" aria-pressed="false">40 baja</button><button data-pl="60" aria-pressed="false">60</button><button data-pl="80" aria-pressed="false">80 normal</button><button data-pl="120" aria-pressed="false">120 segura</button></div>
      <p>Menos colchón significa menos retardo y más riesgo de cortes. Si ves «tarde» o «perd» subir en la tabla, súbelo.</p>
    </div>
    <div class="card">
      <div><div class="lbl">Sincronizar con la imagen</div><div class="big" id="bigad">—<small>ms</small></div></div>
      <p>Retardo de audio que debe aplicar el reproductor de vídeo.</p>
      <code class="cmd" id="mpvcmd">—</code>
      <div class="fld"><label for="lex">Ajuste extra (ms), p. ej. el retardo de la entrada USB</label><input class="num" id="lex" type="number" min="-50" max="200" step="1" value="0"></div>
      <p>Para que mpv lo haga solo, usa <code>sonos_lipsync.lua</code>.</p>
    </div>
  </div>
  <h2 style="margin:22px 0 10px">Estado de la red por altavoz</h2>
  <div class="tw"><table><thead><tr><th>Altavoz</th><th>RTT</th><th>Jitter</th><th>Perdidos</th><th>Recuperados</th><th>Tarde</th><th>Vacíos</th><th>Sync</th><th>Búfer</th></tr></thead><tbody id="health"></tbody></table></div>
</section>

<section class="pane" id="p_log" role="tabpanel" aria-labelledby="tab_log" hidden>
  <div class="head"><h1>Registro</h1><button class="btn ghost sm" id="logclr">Limpiar</button></div>
  <div class="log" id="lb" role="log"></div>
</section>
</main>

<div class="scrim" id="scrim"></div>
<aside class="drawer" id="drawer" role="dialog" aria-modal="true" aria-labelledby="dtitle" aria-hidden="true" inert>
  <div class="grab" aria-hidden="true"></div>
  <header><h2 id="dtitle">Altavoz</h2><span class="chip" id="dst">—</span><button class="x" id="dclose" aria-label="Cerrar">✕</button></header>
  <div class="dbody" id="dbody"></div>
</aside>
<aside class="drawer" id="bdrawer" role="dialog" aria-modal="true" aria-labelledby="btitle" aria-hidden="true" inert>
  <div class="grab" aria-hidden="true"></div>
  <header><h2 id="btitle">Añadir altavoces</h2><button class="x" id="bclose" aria-label="Cerrar">✕</button></header>
  <div class="dbody" id="bbody"></div>
</aside>
<div class="toasts" id="tb" role="status" aria-live="polite"></div>

<script>
const CH=['FL','FR','C','LFE','SL','SR','SBL','SBR','AUX-L','AUX-R'];
const CHN=['Frontal izq.','Frontal der.','Central','Graves (LFE)','Lateral izq.','Lateral der.','Trasero izq.','Trasero der.','Extra izq.','Extra der.'];
const POS=[[15,20],[85,20],[50,17],[50,37],[10,60],[90,60],[18,88],[82,88],[20,42],[80,42]];
const EQF=['354 Hz','707 Hz','1,4 kHz','2,8 kHz','5,7 kHz','9,8 kHz'];
const ROLES=['Rango completo','Satélite','Subwoofer'];
let nodes=[],st={},openId=null,sig='',editRoom=false,drag=null;const pend=new Set();
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const pct=v=>Math.round(v/2.55);
const J=(u,b)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)}).then(r=>r.json()).catch(()=>({error:'red'}));

function toast(m,ok=true){const d=document.createElement('div');d.className='toast'+(ok?'':' bad');d.textContent=m;
  if(!ok)d.setAttribute('role','alert');d.onclick=()=>d.remove();$('tb').appendChild(d);setTimeout(()=>d.remove(),ok?2600:7000);}
function log(m){const b=$('lb'),t=new Date().toLocaleTimeString();const d=document.createElement('div');d.innerHTML='<b>'+t+'</b> '+esc(m);b.appendChild(d);b.scrollTop=b.scrollHeight;}
function fillRng(el){const mn=+el.min,mx=+el.max,v=+el.value;
  if(el.classList.contains('mid')){const p=(v-mn)/(mx-mn);el.style.setProperty('--lo',v>=0?.5:p);el.style.setProperty('--hi',v>=0?p:.5);}
  else el.style.setProperty('--f',(v-mn)/(mx-mn));}
function setSeg(seg,attr,val){seg.querySelectorAll('button').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset[attr])===String(val)));}
function setSw(el,on){el.setAttribute('aria-checked',on?'true':'false');}

/* Pestañas */
function showTab(t){document.querySelectorAll('.tab').forEach(b=>{const on=b.dataset.tab===t;b.setAttribute('aria-selected',on);b.tabIndex=on?0:-1;});
  ['spk','snd','lat','log'].forEach(k=>$('p_'+k).hidden=(k!==t));
  try{history.replaceState(null,'','#'+t);}catch(e){}if(t==='lat')updLat();}
document.querySelectorAll('.tab').forEach(b=>b.onclick=()=>showTab(b.dataset.tab));
document.querySelector('.tabs').addEventListener('keydown',e=>{               // flechas, Inicio y Fin como en cualquier barra de pestañas
  if(!['ArrowLeft','ArrowRight','Home','End'].includes(e.key))return;
  const tabs=[...document.querySelectorAll('.tab')];let i=tabs.indexOf(document.activeElement);if(i<0)return;
  i=e.key==='Home'?0:e.key==='End'?tabs.length-1:(i+(e.key==='ArrowRight'?1:-1)+tabs.length)%tabs.length;
  e.preventDefault();tabs[i].focus();showTab(tabs[i].dataset.tab);});

/* Hojas (cajón del altavoz y Bluetooth): modales de verdad. El resto de la página queda inerte (teclado y lector de pantalla),
   el foco entra en la hoja y vuelve a donde estaba al cerrarla; en el móvil se cierran deslizando hacia abajo. */
let lastFocus=null;
const lockBehind=on=>[document.querySelector('.top'),document.querySelector('main')].forEach(e=>{if(e)e.inert=on;});
function sheetOpen(el){if(!lastFocus)lastFocus=document.activeElement;
  el.inert=false;lockBehind(true);requestAnimationFrame(()=>{const f=el.querySelector('.x');if(f)f.focus();});}
function sheetClose(el){el.inert=true;if(document.querySelector('.drawer.on'))return;lockBehind(false);
  const f=lastFocus;lastFocus=null;if(!f)return;
  const t=f.isConnected?f:(f.dataset&&f.dataset.id!==undefined?document.querySelector('.spk[data-id="'+f.dataset.id+'"]'):null);
  if(t&&t.focus)t.focus();}
function sheetDrag(el,close){const hs=[el.querySelector('.grab'),el.querySelector('header')];let y0=null,dy=0,t0=0;
  const start=e=>{if(!matchMedia('(max-width:760px)').matches||e.target.closest('button'))return;y0=e.clientY;dy=0;t0=e.timeStamp;el.style.transition='none';e.currentTarget.setPointerCapture(e.pointerId);};
  const move=e=>{if(y0===null)return;dy=Math.max(0,e.clientY-y0);el.style.transform='translateY('+dy+'px)';};
  const end=e=>{if(y0===null)return;const v=dy/Math.max(1,e.timeStamp-t0),go=dy>90||(dy>30&&v>.6);y0=null;
    if(go){el.style.transition='transform .2s ease-out';el.style.transform='translateY(102%)';close();setTimeout(()=>{el.style.transform='';el.style.transition='';},260);}
    else{el.style.transition='';el.style.transform='';}};
  hs.forEach(h=>{h.addEventListener('pointerdown',start);h.addEventListener('pointermove',move);h.addEventListener('pointerup',end);h.addEventListener('pointercancel',end);});}

/* Datos */
async function loadStatus(){
  try{const d=await (await fetch('/api/status')).json();st=d;
    if(!lvOK){$('hs').textContent=(d.running?'Capturando':'Detenido')+' · '+d.sample_rate/1000+' kHz';$('dot').className='dot '+(d.running?'ok':'bad');}
    if(document.activeElement!==$('xos')){$('xos').value=d.xover_hz||80;$('xod').textContent=Math.round(d.xover_hz||80)+' Hz';fillRng($('xos'));}
    if(document.activeElement!==$('lex')&&d.lipsync_extra_ms!==undefined)$('lex').value=d.lipsync_extra_ms;
    if(document.activeElement!==$('pls')&&d.playout_ms){$('pls').value=d.playout_ms;$('pld').textContent=Math.round(d.playout_ms)+' ms';fillRng($('pls'));setSeg($('plseg'),'pl',Math.round(d.playout_ms));}
    if(document.activeElement!==$('mvs')&&d.master_volume!==undefined){const v=Math.round(Math.min(1,d.master_volume)*100);$('mvs').value=v;$('mvd').textContent=v+'%';$('mvs').setAttribute('aria-valuetext',v+' %');fillRng($('mvs'));}
    drawSound();
  }catch(e){lvOK=false;lvTxt='';$('hs').textContent='Sin conexión';$('dot').className='dot bad';}
}
async function loadNodes(){
  if(drag||pend.size)return;                                  // no pisar una posición recién movida que aún no se guardó
  try{nodes=await (await fetch('/api/nodes')).json();}catch(e){return;}
  const vis=nodes.filter(isKnown);
  const s=JSON.stringify(vis.map(n=>[n.id,n.enabled,n.online,n.ch_left,n.ch_right,n.mono,n.role,n.ip,n.pos_x,n.pos_y,n.pos2_x,n.pos2_y,n.known,n.volume_l,n.volume_r,n.latency_offset_us]))+openId+editRoom;
  if(s!==sig){sig=s;drawRoom();}
  else vis.forEach(n=>{const l=document.querySelector('#spks [data-id="'+n.id+'"] .lv i');if(l)l.style.width=Math.max(0,Math.min(100,n.fill_pct||0))+'%';});
  const on=nodes.filter(n=>n.enabled&&n.online).length,tot=nodes.filter(n=>n.enabled&&isKnown(n)).length;
  $('onl').innerHTML=tot?'<i class="dot '+(on===tot?'ok':on?'warn':'bad')+'"></i> '+on+' de '+tot+' en línea':'<i class="dot bad"></i> Sin altavoces';
  drawHealth();drawSound();if(openId!==null)updDrawerStats();
}
const find=id=>nodes.find(n=>n.id===id);
const isKnown=n=>n.known||n.online;   // solo los ESP que de verdad existen (se conectaron alguna vez); «Quitar del panel» olvida uno apagado
/* Un nodo estéreo son dos altavoces (canal izquierdo y derecho); un nodo mono, uno. Cada uno tiene su propia posición. */
const hasPos=(n,side)=>side?(n.pos2_x>=0&&n.pos2_y>=0):(n.pos_x>=0&&n.pos_y>=0);
function units(n){
  const u=[{n,side:0,ch:n.ch_left}];
  if(!(n.mono||n.ch_left===n.ch_right))u.push({n,side:1,ch:n.ch_right});
  return u;
}
function unitPos(u){
  const n=u.n;
  if(hasPos(n,u.side))return u.side?[n.pos2_x,n.pos2_y]:[n.pos_x,n.pos_y];
  return POS[u.ch]||[50,50];
}
const fmtMs=us=>{const v=Math.round((us||0)/100)/10;return(v<0?'−':'')+Math.abs(v).toFixed(1).replace('.',',')+' ms';};
function nodeLabel(n){return n.mono||n.ch_left===n.ch_right?CH[n.ch_left]:CH[n.ch_left]+' + '+CH[n.ch_right];}
/* Zoom del plano: se escala la distancia entre cosas (y los anillos de metros), nunca el tamaño de los altavoces ni de «Escucha».
   El punto fijo del zoom es quien escucha; las posiciones guardadas siguen en % de la sala a 100 %. */
const ZMIN=0.5,ZMAX=3;
const unitScale=()=>zoom<1?Math.max(0.55,zoom):1;   // al acercar los altavoces no crecen; al alejar se achican (y pierden las etiquetas) para despejar el plano
let zoom=1,panX=0,panY=0;
try{const z=parseFloat(localStorage.getItem('sonos_zoom'));if(z>=ZMIN&&z<=ZMAX)zoom=z;}catch(e){}
const saveZoom=()=>{try{localStorage.setItem('sonos_zoom',String(Math.round(zoom*1000)/1000));}catch(e){}};
const vw=()=>{const el=$('room');return {W:el.clientWidth,H:el.clientHeight};};
const LXY=(W,H)=>[W/2,H*0.78];
function toPx(X,Y){const {W,H}=vw(),[lx,ly]=LXY(W,H);return [lx+(X/100*W-lx)*zoom+panX,ly+(Y/100*H-ly)*zoom+panY];}
function toWorld(px,py){const {W,H}=vw(),[lx,ly]=LXY(W,H);return [((px-panX-lx)/zoom+lx)/W*100,((py-panY-ly)/zoom+ly)/H*100];}
function clampPos(x,y){   // el altavoz entero (dibujo + etiquetas) debe quedar dentro de la sala; con zoom caben más cerca de las paredes
  const {W,H}=vw(),cs=getComputedStyle($('room')),g=k=>parseFloat(cs.getPropertyValue(k))||60;
  const sc=unitScale(),mx=g('--hw')*sc/(W*zoom)*100,mt=g('--ht')*sc/(H*zoom)*100,mb=g('--hb')*sc/(H*zoom)*100;
  return [clampN(x,mx,100-mx),clampN(y,mt,100-mb)];
}
function clampPan(){
  const {W,H}=vw(),[lx,ly]=LXY(W,H);
  if(zoom<=1){panX=0;panY=0;return;}
  panX=clampN(panX,(W-lx)*(1-zoom),lx*(zoom-1));panY=clampN(panY,(H-ly)*(1-zoom),ly*(zoom-1));
}
function placeUnit(b,x,y){const [px,py]=toPx(x,y);b.style.left=px.toFixed(1)+'px';b.style.top=py.toFixed(1)+'px';}
function placeAll(){
  const {W,H}=vw();if(!W||!H)return;
  clampPan();
  const us=unitScale();$('room').classList.toggle('lod',zoom<0.85);
  document.querySelectorAll('#spks .spk').forEach(b=>{b.style.setProperty('--s',us);const [x,y]=clampPos(+b.dataset.x,+b.dataset.y);placeUnit(b,x,y);});
  const me=document.querySelector('#room .me'),[mx,my]=toPx(50,78);me.style.setProperty('--s',us);me.style.left=mx.toFixed(1)+'px';me.style.top=my.toFixed(1)+'px';
  const sc=document.querySelector('#room .screen'),[sl,st]=toPx(30,1.5),[sr]=toPx(70,0);sc.style.left=sl.toFixed(1)+'px';sc.style.width=Math.max(0,sr-sl).toFixed(1)+'px';sc.style.right='auto';sc.style.top=st.toFixed(1)+'px';
  const rl=document.querySelector('#room .rl.back'),[bx,by]=toPx(50,100);rl.style.left=bx.toFixed(1)+'px';rl.style.top=(by-6).toFixed(1)+'px';rl.style.bottom='auto';rl.style.transform='translate(-50%,-100%)';
  $('zval').textContent=Math.round(zoom*100)+'%';$('zin').disabled=zoom>=ZMAX-0.001;$('zout').disabled=zoom<=ZMIN+0.001;
  $('room').classList.toggle('zoomed',Math.abs(zoom-1)>0.001);
  drawRadar();
}
function setZoom(z2,cx,cy){   // (cx,cy): punto de la vista que se queda quieto (por defecto, quien escucha)
  z2=clampN(z2,ZMIN,ZMAX);const {W,H}=vw(),[lx,ly]=LXY(W,H);if(cx===undefined){cx=lx+panX;cy=ly+panY;}
  const f=z2/zoom;panX=(cx-lx)-(cx-lx-panX)*f;panY=(cy-ly)-(cy-ly-panY)*f;zoom=z2;saveZoom();placeAll();
}
function resetZoom(){zoom=1;panX=0;panY=0;saveZoom();placeAll();}
$('zin').onclick=()=>setZoom(zoom*1.25);$('zout').onclick=()=>setZoom(zoom/1.25);$('zval').onclick=resetZoom;
const roomPt=e=>{const el=$('room'),r=el.getBoundingClientRect();return [e.clientX-r.left-el.clientLeft,e.clientY-r.top-el.clientTop];};
$('room').addEventListener('wheel',e=>{if(!(e.ctrlKey||e.metaKey))return;e.preventDefault();const [x,y]=roomPt(e);setZoom(zoom*Math.exp(-e.deltaY*0.01),x,y);},{passive:false});
const pts=new Map();
$('room').addEventListener('pointerdown',e=>{
  if(e.target.closest('.spk,.zoomctl'))return;
  pts.set(e.pointerId,{x:e.clientX,y:e.clientY});try{$('room').setPointerCapture(e.pointerId);}catch(x){}
  if(zoom!==1)$('room').classList.add('panning');
});
$('room').addEventListener('pointermove',e=>{
  const prev=pts.get(e.pointerId);if(!prev)return;
  const cur={x:e.clientX,y:e.clientY};
  if(pts.size===2){
    const o=[...pts.entries()].find(([k])=>k!==e.pointerId)[1],d0=Math.hypot(prev.x-o.x,prev.y-o.y),d1=Math.hypot(cur.x-o.x,cur.y-o.y);pts.set(e.pointerId,cur);
    if(d0>0){const r=$('room').getBoundingClientRect(),el=$('room');setZoom(zoom*d1/d0,(cur.x+o.x)/2-r.left-el.clientLeft,(cur.y+o.y)/2-r.top-el.clientTop);}
  }else{
    pts.set(e.pointerId,cur);
    if(zoom>1){panX+=cur.x-prev.x;panY+=cur.y-prev.y;placeAll();}
  }
});
const endPan=e=>{pts.delete(e.pointerId);if(!pts.size)$('room').classList.remove('panning');};
$('room').addEventListener('pointerup',endPan);$('room').addEventListener('pointercancel',endPan);
const spkSvg=sub=>sub
  ?'<svg viewBox="0 0 56 56" aria-hidden="true"><rect class="body" x="2" y="2" width="52" height="52" rx="8"/><circle class="wf" cx="28" cy="28" r="19"/><circle class="cone" cx="28" cy="28" r="7"/></svg>'
  :'<svg viewBox="0 0 46 62" aria-hidden="true"><rect class="body" x="2" y="2" width="42" height="58" rx="8"/><circle class="tw" cx="23" cy="17" r="6"/><circle class="wf" cx="23" cy="42" r="13"/><circle class="cone" cx="23" cy="42" r="4.5"/></svg>';

/* Fondo de radar: anillos cada 0,5 m centrados en quien escucha (el ancho del plano son 5 m; es solo una referencia visual) */
const ROOM_M=5,RING_M=0.5;
function drawRadar(){
  const el=$('room'),svg=$('radar');if(!el||!svg)return;
  const w=el.clientWidth,h=el.clientHeight;if(!w||!h)return;
  const ppm=w/ROOM_M*zoom,[lx,ly]=LXY(w,h),cx=lx+panX,cy=ly+panY,
    maxR=Math.max(Math.hypot(cx,cy),Math.hypot(w-cx,cy),Math.hypot(cx,h-cy),Math.hypot(w-cx,h-cy)),n=Math.floor(maxR/(ppm*RING_M)),
    ly2=clampN(cy-5,14,h-8),dy=ly2-cy;
  const [x0,y0]=toPx(0,0),[x1,y1]=toPx(100,100);
  let o='<rect class="walls" x="'+x0.toFixed(1)+'" y="'+y0.toFixed(1)+'" width="'+(x1-x0).toFixed(1)+'" height="'+(y1-y0).toFixed(1)+'" rx="2"/>'+
    '<line class="ax" x1="0" y1="'+cy+'" x2="'+w+'" y2="'+cy+'"/><line class="ax" x1="'+cx+'" y1="0" x2="'+cx+'" y2="'+h+'"/>';
  for(let k=1;k<=n;k++){const r=k*ppm*RING_M,m=k%2===0;
    o+='<circle class="ring'+(m?' m':'')+'" cx="'+cx.toFixed(1)+'" cy="'+cy.toFixed(1)+'" r="'+r.toFixed(1)+'"/>';
    if(m&&Math.abs(dy)<r){const x=cx+Math.sqrt(r*r-dy*dy)+4;if(x>4&&x<w-30)o+='<text x="'+x.toFixed(1)+'" y="'+ly2.toFixed(1)+'">'+(k/2)+' m</text>';}}
  svg.setAttribute('viewBox','0 0 '+w+' '+h);svg.innerHTML=o;
}
if(window.ResizeObserver)new ResizeObserver(placeAll).observe($('room'));else addEventListener('resize',placeAll);

function drawRoom(){
  const ae=document.activeElement,fk=ae&&ae.closest&&ae.closest('#spks .spk')?[ae.dataset.id,ae.dataset.side]:null;   // conserva el foco del teclado al redibujar
  const vis=nodes.filter(isKnown);
  const used={};let h='';
  vis.forEach(n=>{
    units(n).forEach(u=>{
      let [x,y]=unitPos(u);
      if(!hasPos(n,u.side)){const k=Math.round(x)+','+Math.round(y);used[k]=(used[k]||0)+1;x+=(used[k]-1)*7;y+=(used[k]-1)*9;}
      const nm=CH[u.ch],sub=n.role===2,live=n.enabled&&n.online,state=!n.enabled?'Desactivado':n.online?'En línea':'Sin conexión';
      h+='<button class="spk'+(sub?' sub':'')+(live?'':' off')+'" data-id="'+n.id+'" data-side="'+u.side+'" aria-expanded="'+(openId===n.id)+'" data-x="'+x+'" data-y="'+y+'" '+
        'aria-label="Altavoz '+nm+' ('+CHN[u.ch]+'), ESP '+n.id+', '+state.toLowerCase()+', volumen '+pct(u.side?n.volume_r:n.volume_l)+' %, retardo '+fmtMs(n.latency_offset_us)+(editRoom?'. Mover con las flechas del teclado':'. Pulsa para ajustarlo')+'">'+
        '<span class="cab">'+spkSvg(sub)+'<i class="st'+(live?' on':'')+'"></i>'+vuHtml(u.ch)+'</span>'+
        '<b class="ch">'+nm+'</b><span class="cap"><i class="dot '+(live?'ok':'bad')+'"></i>ESP '+n.id+' · '+state+'</span>'+
        '<span class="cal" title="Volumen · retardo de este altavoz (los de la calibración)">'+pct(u.side?n.volume_r:n.volume_l)+'% <i>·</i> '+fmtMs(n.latency_offset_us)+'</span></button>';
    });
  });
  $('spks').innerHTML=h||'<div class="empty" style="position:absolute;inset:0;display:grid;place-items:center">Aún no hay altavoces. Enciende un ESP32 o añádelo con «Añadir altavoz».</div>';
  $('chips').innerHTML=vis.map(n=>'<button data-id="'+n.id+'" aria-expanded="'+(openId===n.id)+'"><i class="dot '+(n.online?'ok':'bad')+'"></i>ESP '+n.id+' · '+nodeLabel(n)+(n.enabled?'':' · apagado')+'</button>').join('');
  placeAll();collectVus();
  if(fk){const f=unitEl(+fk[0],+fk[1]);if(f)f.focus({preventScroll:true});}
  document.querySelectorAll('#spks .spk,#chips button').forEach(b=>b.onclick=()=>{if(editRoom&&b.classList.contains('spk'))return;openDrawer(+b.dataset.id);});
}

/* Colocar los altavoces en el plano: modo «Mover altavoces». Cada altavoz (canal) se guarda por separado en la Raspberry. */
const clampN=(v,a,b)=>Math.max(a,Math.min(b,v));
const r1=v=>Math.round(v*10)/10;
const posBody=(n,side,x,y)=>side?{id:n.id,pos2_x:r1(x),pos2_y:r1(y)}:{id:n.id,pos_x:r1(x),pos_y:r1(y)};
const setLocal=(n,side,x,y)=>{if(side){n.pos2_x=x;n.pos2_y=y;}else{n.pos_x=x;n.pos_y=y;}};
async function savePos(n,side){
  const x=side?n.pos2_x:n.pos_x,y=side?n.pos2_y:n.pos_y;
  const r=await J('/api/node',posBody(n,side,x,y));
  if(!r.ok){toast('No se pudo guardar la posición. Comprueba la conexión con la Raspberry.',false);sig='';loadNodes();}
}
const unitEl=(id,side)=>document.querySelector('#spks .spk[data-id="'+id+'"][data-side="'+side+'"]');
function setEdit(on){
  editRoom=on;$('room').classList.toggle('edit',on);$('medit').setAttribute('aria-pressed',on);$('medit').textContent=on?'Listo':'Mover altavoces';
  $('mreset').hidden=!on;
  $('mhint').textContent=on?'Arrastra cada altavoz a su lugar en la sala, o usa las flechas del teclado. Se guarda solo.':'Toca un altavoz para ajustarlo. Los cambios se guardan solos.';
  if(on&&$('drawer').classList.contains('on'))closeDrawer();
  drawRoom();
}
$('medit').onclick=()=>setEdit(!editRoom);
$('mreset').onclick=async()=>{
  const rs=await Promise.all(nodes.filter(n=>hasPos(n,0)||hasPos(n,1)).map(n=>J('/api/node',{id:n.id,pos_auto:true})));
  toast(rs.every(r=>r.ok)?'Posiciones restablecidas: cada altavoz vuelve al lugar de su canal':'No se pudieron restablecer todas las posiciones',rs.every(r=>r.ok));
  sig='';await loadNodes();
};
$('spks').addEventListener('pointerdown',e=>{
  if(!editRoom)return;const b=e.target.closest('.spk');if(!b)return;
  e.preventDefault();b.setPointerCapture(e.pointerId);b.classList.add('drag');document.body.classList.add('dragging');if(getSelection)getSelection().removeAllRanges();
  const bb=b.getBoundingClientRect();
  drag={id:+b.dataset.id,side:+b.dataset.side,b,pid:e.pointerId,moved:false,dx:(bb.left+bb.width/2-e.clientX),dy:(bb.top+bb.height/2-e.clientY)};
});
$('spks').addEventListener('pointermove',e=>{
  if(!drag||e.pointerId!==drag.pid)return;
  const n=find(drag.id),[px,py]=roomPt({clientX:e.clientX+drag.dx,clientY:e.clientY+drag.dy});
  const [x,y]=clampPos(...toWorld(px,py));
  setLocal(n,drag.side,x,y);drag.moved=true;drag.b.dataset.x=x;drag.b.dataset.y=y;placeUnit(drag.b,x,y);
});
const endDrag=async e=>{
  if(!drag||e.pointerId!==drag.pid)return;
  const d=drag;d.b.classList.remove('drag');document.body.classList.remove('dragging');
  if(d.moved){await savePos(find(d.id),d.side);}
  drag=null;sig='';drawRoom();
};
$('spks').addEventListener('pointerup',endDrag);$('spks').addEventListener('pointercancel',endDrag);
const posT={};
$('spks').addEventListener('keydown',e=>{
  if(!editRoom)return;const b=e.target.closest('.spk');if(!b)return;
  const k={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]}[e.key];if(!k)return;
  e.preventDefault();const n=find(+b.dataset.id),side=+b.dataset.side,u=units(n)[side]||units(n)[0],p=unitPos(u),s=e.shiftKey?5:1;
  setLocal(n,side,...clampPos(p[0]+k[0]*s,p[1]+k[1]*s));
  drawRoom();const f=unitEl(n.id,side);if(f)f.focus();
  const key=n.id+'_'+side;pend.add(key);clearTimeout(posT[key]);posT[key]=setTimeout(async()=>{await savePos(n,side);pend.delete(key);},400);
});

/* Cajón */
function openDrawer(id){closeBle();openId=id;sig='';buildDrawer();$('drawer').classList.add('on');$('scrim').classList.add('on');$('drawer').setAttribute('aria-hidden','false');sheetOpen($('drawer'));drawRoom();sig='x';loadNodes();}
function closeDrawer(){const was=openId!==null;openId=null;$('drawer').classList.remove('on');if(!$('bdrawer').classList.contains('on'))$('scrim').classList.remove('on');$('drawer').setAttribute('aria-hidden','true');sig='';drawRoom();sheetClose($('drawer'));}
$('scrim').onclick=()=>{closeDrawer();closeBle();};$('dclose').onclick=closeDrawer;sheetDrag($('drawer'),closeDrawer);
document.addEventListener('keydown',e=>{if(e.key==='Escape'){if(openId!==null)closeDrawer();closeBle();}});
function chOpts(sel){return CH.map((c,i)=>'<option value="'+i+'"'+(i===sel?' selected':'')+'>'+c+' · '+CHN[i]+'</option>').join('');}
function buildDrawer(){
  const n=find(openId);if(!n)return;
  $('dtitle').textContent='Altavoz #'+n.id;
  const eq=n.eq||[];
  $('dbody').innerHTML=
   '<div class="sw"><div><b>Habilitado</b><span class="hint">Si lo apagas, no recibe audio.</span></div><button class="switch" id="d_en" role="switch" aria-checked="'+!!n.enabled+'" aria-label="Habilitado"></button></div>'+
   '<div class="fld"><label>Salida</label><div class="seg block" id="d_md"><button data-v="0" aria-pressed="'+!n.mono+'">Estéreo</button><button data-v="1" aria-pressed="'+!!n.mono+'">Mono</button></div></div>'+
   '<div class="two"><div class="fld"><label for="d_cl" id="d_cll">'+(n.mono?'Canal':'Canal izquierdo')+'</label><select class="sel" id="d_cl">'+chOpts(n.ch_left)+'</select></div>'+
   '<div class="fld" id="d_crw"><label for="d_cr">Canal derecho</label><select class="sel" id="d_cr">'+chOpts(n.ch_right)+'</select></div></div>'+
   '<div class="fld"><div class="top2"><label for="d_vl" id="d_vll">'+(n.mono?'Volumen':'Volumen izquierdo')+'</label><b id="d_vld">'+pct(n.volume_l)+'%</b></div><input class="rng" id="d_vl" type="range" min="0" max="255" value="'+n.volume_l+'"></div>'+
   '<div class="fld" id="d_vrw"><div class="top2"><label for="d_vr">Volumen derecho</label><b id="d_vrd">'+pct(n.volume_r)+'%</b></div><input class="rng" id="d_vr" type="range" min="0" max="255" value="'+n.volume_r+'"></div>'+
   '<div class="fld"><label>Función en los graves</label><div class="seg block" id="d_rl">'+ROLES.map((r,i)=>'<button data-v="'+i+'" aria-pressed="'+(n.role===i)+'">'+r+'</button>').join('')+'</div>'+
   '<p class="hint" style="margin-top:6px" id="d_rlh"></p></div>'+
   '<div class="fld"><label for="d_lat">Retardo extra (ms; positivo lo retrasa)</label><input class="num" id="d_lat" type="number" min="-500" max="500" step="0.01" value="'+((n.latency_offset_us||0)/1000)+'"></div>'+
   '<hr class="sep"><div class="fld"><div class="top2"><b>Ecualizador</b><button class="btn ghost sm" id="d_eqz">Plano</button></div><p class="hint" id="d_eqh" style="margin:4px 0 8px"></p>'+
   EQF.map((f,i)=>'<div class="eqrow"><span>'+f+'</span><input class="rng mid" type="range" min="-12" max="12" step="0.5" id="d_eq'+i+'" value="'+(eq[i]||0)+'" aria-label="EQ '+f+'"><output id="d_eo'+i+'"></output></div>').join('')+'</div>'+
   '<hr class="sep"><div class="hint" id="d_stats"></div><div class="savestate" id="d_ss"></div>'+
   '<div class="fld" id="d_fgw"><button class="btn danger sm" id="d_fg" type="button">Quitar este ESP del panel</button><p class="hint" style="margin-top:6px">Solo si está apagado. Si vuelve a conectarse, aparece de nuevo con su configuración.</p></div>';
  wireDrawer();modeUI();rolHint();eqHint();document.querySelectorAll('#dbody .rng').forEach(fillRng);for(let i=0;i<6;i++)eqOut(i);updDrawerStats();
}
function eqOut(i){const v=+$('d_eq'+i).value;$('d_eo'+i).textContent=(v>0?'+':'')+v.toFixed(1).replace('.',',');}
function modeUI(){const mono=$('d_md').querySelector('[aria-pressed=true]').dataset.v==='1';
  $('d_crw').style.display=mono?'none':'';$('d_vrw').style.display=mono?'none':'';
  $('d_cll').textContent=mono?'Canal':'Canal izquierdo';$('d_vll').textContent=mono?'Volumen':'Volumen izquierdo';}
function rolHint(){const r=+$('d_rl').querySelector('[aria-pressed=true]').dataset.v;
  $('d_rlh').textContent=r===2?'Recibe su canal y los graves de los demás. Solo se usa con el crossover activo.':r===1?'Solo agudos por encima del corte, cuando hay subwoofer en línea.':'Recibe el canal completo.';}
function eqHint(){const el=$('d_eqh');if(!el)return;el.innerHTML=st.dsp_eq?'Se aplica a este altavoz. <a href="#" id="d_eqoff">Desactivar EQ</a>':'La EQ está desactivada (pass-through). <a href="#" id="d_eqon">Activar</a>';
  const a=$('d_eqon');if(a)a.onclick=e=>{e.preventDefault();setDsp({eq:true});};
  const b=$('d_eqoff');if(b)b.onclick=e=>{e.preventDefault();setDsp({eq:false});};}
let saveT=null;
function queueSave(){clearTimeout(saveT);$('d_ss').textContent='Guardando…';saveT=setTimeout(doSave,450);}
async function doSave(){
  const id=openId;if(id===null||!$('d_cl'))return;
  const mono=$('d_md').querySelector('[aria-pressed=true]').dataset.v==='1';
  const b={id,ch_left:+$('d_cl').value,ch_right:+$('d_cr').value,volume_l:+$('d_vl').value,volume_r:+$('d_vr').value,
    enabled:$('d_en').getAttribute('aria-checked')==='true',mono,role:+$('d_rl').querySelector('[aria-pressed=true]').dataset.v};
  if(mono){b.ch_right=b.ch_left;b.volume_r=b.volume_l;}
  const eb={id};for(let i=0;i<6;i++)eb['eq'+i]=Math.max(-12,Math.min(12,+$('d_eq'+i).value||0));
  const lat=Math.round((+$('d_lat').value||0)*1000);
  const r1=await J('/api/node',b),r2=await J('/api/eq',eb),r3=await J('/api/calibrate',{esp_id:id,latency_us:lat});
  if(r1.ok&&r2.ok&&r3.ok){const n=find(id);if(n){Object.assign(n,b);n.eq=[0,1,2,3,4,5].map(i=>eb['eq'+i]);n.latency_offset_us=lat;}
    if($('d_ss'))$('d_ss').textContent='Guardado';log('Altavoz #'+id+' actualizado');sig='';loadStatus();loadNodes();}
  else{if($('d_ss'))$('d_ss').textContent='No se pudo guardar';toast('No se pudo guardar. Comprueba la conexión con la Raspberry.',false);}
}
function wireDrawer(){
  const q=queueSave;
  $('d_en').onclick=()=>{setSw($('d_en'),$('d_en').getAttribute('aria-checked')!=='true');q();};
  $('d_md').querySelectorAll('button').forEach(b=>b.onclick=()=>{setSeg($('d_md'),'v',b.dataset.v);modeUI();q();});
  $('d_rl').querySelectorAll('button').forEach(b=>b.onclick=()=>{setSeg($('d_rl'),'v',b.dataset.v);rolHint();q();});
  ['d_cl','d_cr'].forEach(i=>$(i).onchange=q);
  $('d_lat').onchange=q;
  const fg=$('d_fg'),nn=find(openId);if(nn&&nn.online)fg.disabled=true;
  fg.onclick=async()=>{
    if(!fg.dataset.arm){fg.dataset.arm='1';fg.textContent='Pulsa otra vez para quitarlo';setTimeout(()=>{delete fg.dataset.arm;fg.textContent='Quitar este ESP del panel';},4000);return;}
    const r=await J('/api/node',{id:openId,forget:true});
    if(r.ok){toast('ESP '+openId+' quitado del panel');log('ESP '+openId+' quitado del panel');closeDrawer();sig='';loadNodes();}else toast(r.error||'No se pudo quitar el ESP',false);
  };
  $('d_vl').oninput=function(){$('d_vld').textContent=pct(+this.value)+'%';fillRng(this);q();};
  $('d_vr').oninput=function(){$('d_vrd').textContent=pct(+this.value)+'%';fillRng(this);q();};
  for(let i=0;i<6;i++)$('d_eq'+i).oninput=function(){fillRng(this);eqOut(i);q();};
  $('d_eqz').onclick=()=>{for(let i=0;i<6;i++){$('d_eq'+i).value=0;fillRng($('d_eq'+i));eqOut(i);}q();};
}
function updDrawerStats(){const n=find(openId),s=$('d_stats');if(!n||!s)return;
  $('dst').className='chip '+(n.online?'ac':'bad');$('dst').textContent=n.online?(n.ip||'en línea'):'sin conexión';
  s.textContent=n.online?'RTT '+((n.rtt_us||0)/1000).toFixed(1)+' ms · jitter '+(n.jitter_us||0)+' µs · perdidos '+(n.lost_packets||0)+' · recuperados '+(n.recovered_packets||0)+' · vacíos '+(n.underruns||0):'Este altavoz no responde ahora mismo.';}


/* Sonido */
async function setDsp(o){const r=await J('/api/dsp',o);
  if(r.ok){if(o.xover!==undefined)st.dsp_xover=o.xover;if(o.eq!==undefined)st.dsp_eq=o.eq;
    log('Procesado: crossover '+(st.dsp_xover?'sí':'no')+', EQ '+(st.dsp_eq?'sí':'no'));drawSound();if($('d_eqh'))eqHint();toast(!st.dsp_xover&&!st.dsp_eq?'Pass-through activo':'Procesado actualizado');}
  else{toast('No se pudo cambiar el procesado. Vuelve a intentarlo.',false);}loadStatus();}
function subNode(){return nodes.find(n=>n.role===2&&n.enabled);}
function drawSound(){
  const xo=!!st.dsp_xover,eq=!!st.dsp_eq,pass=!xo&&!eq,sub=subNode();
  const nc=st.channels||10,on=nodes.filter(n=>n.enabled).length;
  const S=(t,s,bp,io)=>'<div class="fs'+(bp?' bp':'')+(io?' io':'')+'"><b>'+t+'</b><span>'+s+'</span></div>';
  $('flow').innerHTML=S('Entrada USB',nc+' canales',false,true)+'<span class="arr">›</span>'+
    S('Crossover',xo?(sub?'corte '+Math.round(st.xover_hz||80)+' Hz':'sin sub: inactivo'):'omitido',!xo||!sub)+'<span class="arr">›</span>'+
    S('Ecualizador',eq?'por altavoz':'omitido',!eq)+'<span class="arr">›</span>'+
    S('Volumen y retardo','por altavoz',false)+'<span class="arr">›</span>'+S('Altavoces',on+' activos',false,true);
  $('procbar').hidden=pass;
  $('proctxt').textContent='Procesado activo: '+(xo&&eq?'crossover y EQ':xo?'crossover':'EQ');
  $('t_pass').setAttribute('aria-pressed',pass);$('t_proc').setAttribute('aria-pressed',!pass);
  $('tg_pass').style.display=pass?'':'none';$('tg_proc').style.display=pass?'none':'';
  $('tg_pass').className='chip tag ac';$('tg_proc').className='chip tag ac';
  setSw($('sw_xo'),xo);setSw($('sw_eq'),eq);
  $('c_xo').classList.toggle('dim',!xo);$('c_eq').classList.toggle('dim',!eq);
  $('xohint').textContent=!sub?'Elige un subwoofer para que el crossover tenga efecto. Sin él, todos reciben el canal completo.':'El sub recibe su canal y los graves de los demás, que pasan a satélite (24 dB/oct). Si el sub se desconecta, los satélites vuelven al canal completo.';
  const ss=$('subsel');if(document.activeElement!==ss){const cur=sub?String(sub.id):'-';
    ss.innerHTML='<option value="-">Ninguno</option>'+nodes.map(n=>'<option value="'+n.id+'"'+(String(n.id)===cur?' selected':'')+'>Altavoz #'+n.id+' ('+nodeLabel(n)+')</option>').join('');}
  const adj=nodes.filter(n=>(n.eq||[]).some(x=>Math.abs(x)>=0.05)).length;
  $('eqsum').textContent=adj?adj+(adj===1?' altavoz tiene':' altavoces tienen')+' la EQ ajustada.':'Ningún altavoz tiene la EQ ajustada todavía.';
}
$('t_pass').onclick=()=>setDsp({xover:false,eq:false});
$('procoff').onclick=()=>setDsp({xover:false,eq:false});
$('t_proc').onclick=()=>{if(st.dsp_xover||st.dsp_eq)return;setDsp({xover:true,eq:true});};
$('sw_xo').onclick=()=>setDsp({xover:$('sw_xo').getAttribute('aria-checked')!=='true'});
$('sw_eq').onclick=()=>setDsp({eq:$('sw_eq').getAttribute('aria-checked')!=='true'});
$('xos').oninput=function(){$('xod').textContent=this.value+' Hz';fillRng(this);};
$('xos').onchange=async function(){const r=await J('/api/bass',{xover_hz:+this.value});if(r.ok){toast('Corte a '+this.value+' Hz');log('Corte de graves: '+this.value+' Hz');loadStatus();}else toast('No se pudo guardar. Comprueba la conexión con la Raspberry.',false);};
$('subsel').onchange=async function(){const sel=this.value;
  for(const n of nodes){const role=sel==='-'?0:(String(n.id)===sel?2:(n.enabled?1:0));if(role!==n.role)await J('/api/node',{id:n.id,role});}
  toast(sel==='-'?'Todos a rango completo':'Subwoofer: altavoz #'+sel);log(sel==='-'?'Todos a rango completo':'Subwoofer = #'+sel);sig='';loadNodes();};

/* Latencia */
async function updLat(){try{const j=await (await fetch('/api/latency')).json();
  $('bigtot').innerHTML=Math.round(j.total_ms)+'<small>ms</small>';$('bigad').innerHTML=Math.round(j.audio_delay_ms)+'<small>ms</small>';
  $('mpvcmd').textContent='mpv --audio-delay='+j.mpv_audio_delay_s.toFixed(3)+' vídeo.mkv';}catch(e){}}
async function setPL(v){const r=await J('/api/playout',{ms:+v});if(r.ok){toast('Colchón '+v+' ms');log('Colchón de red: '+v+' ms');loadStatus();updLat();}else toast('No se pudo cambiar el colchón de red. Vuelve a intentarlo.',false);}
$('pls').oninput=function(){$('pld').textContent=this.value+' ms';fillRng(this);};
$('pls').onchange=function(){setPL(this.value);};
$('plseg').querySelectorAll('button').forEach(b=>b.onclick=()=>setPL(b.dataset.pl));
$('lex').onchange=async function(){const r=await J('/api/latency',{extra_ms:+this.value});if(r.ok){toast('Ajuste extra '+this.value+' ms');updLat();}else toast('Valor fuera de rango (−50 a 200)',false);};
function cls(v,w,b){return v>=b?' class="bad"':v>=w?' class="warn"':'';}
function drawHealth(){const vis=nodes.filter(isKnown);
  $('health').innerHTML=vis.map(n=>'<tr><td><i class="dot '+(n.online?'ok':'bad')+'"></i> #'+n.id+' · '+nodeLabel(n)+'</td><td>'+(n.online?((n.rtt_us||0)/1000).toFixed(1)+' ms':'—')+'</td><td>'+(n.jitter_us||0)+' µs</td><td'+cls(n.lost_packets||0,1,50)+'>'+(n.lost_packets||0)+'</td><td>'+(n.recovered_packets||0)+'</td><td'+cls(n.late_packets||0,5,50)+'>'+(n.late_packets||0)+'</td><td'+cls(n.underruns||0,1,10)+'>'+(n.underruns||0)+'</td><td>'+((n.sync_err_us||0))+' µs</td><td>'+Math.round(n.fill_pct||0)+'%</td></tr>').join('')||'<tr><td colspan="9" class="mut">Sin altavoces</td></tr>';}

/* Añadir altavoces por Bluetooth desde la Raspberry */
let bs={available:false,ssid:'',has_pass:false,rpi_ip:''},bdevs=[],bscan=false,bres={};
const rssiTxt=r=>r==null?'':r>-60?'Señal fuerte':r>-75?'Señal media':'Señal débil';
function openBle(){closeDrawer();$('bdrawer').classList.add('on');$('scrim').classList.add('on');$('bdrawer').setAttribute('aria-hidden','false');sheetOpen($('bdrawer'));
  fetch('/api/ble/status').then(r=>r.json()).then(j=>{bs=j;bleBuild();}).catch(()=>{bleBuild();});bleBuild();}
function closeBle(){const d=$('bdrawer');if(!d.classList.contains('on'))return;d.classList.remove('on');d.setAttribute('aria-hidden','true');if(openId===null)$('scrim').classList.remove('on');sheetClose(d);}
$('lble').onclick=openBle;$('bclose').onclick=closeBle;sheetDrag($('bdrawer'),closeBle);
const onlineIds=()=>new Set(nodes.filter(n=>n.online).map(n=>n.id));
function bleBuild(){
  const credsOk=!!(bs.ssid&&bs.has_pass),keep=document.activeElement&&document.activeElement.id;
  let h='';
  if(!bs.available)h+='<div class="note warn"><b>Esta Raspberry no tiene el ayudante Bluetooth.</b><br>Instala <code>python3-bleak</code> (lo hace <code>scripts/build.sh</code>) o configura el ESP32 desde el móvil con <a href="'+(window._bleHref||'#')+'">la página Bluetooth</a>.</div>';
  h+='<div class="fld"><b>WiFi de los altavoces</b><p class="hint" style="margin:3px 0 8px">Es la red 5 GHz a la que se unirá cada ESP32. Se guarda en la Raspberry.</p>'+
     '<label for="b_ssid">Nombre de la red</label><input class="txt" id="b_ssid" maxlength="32" autocomplete="off" autocapitalize="off" value="'+esc(bs.ssid||'')+'">'+
     '<label for="b_pass" style="margin-top:8px">Contraseña</label><input class="txt" id="b_pass" type="password" maxlength="63" autocomplete="new-password" placeholder="'+(bs.has_pass?'Guardada (déjala vacía para conservarla)':'8 a 63 caracteres')+'">'+
     '<details style="margin-top:8px"><summary class="hint" style="cursor:pointer">Opciones avanzadas</summary><label for="b_rip" style="margin-top:8px">IP de la Raspberry (solo si el router bloquea el broadcast)</label><input class="txt" id="b_rip" maxlength="15" inputmode="decimal" placeholder="192.168.1.10" value="'+esc(bs.rpi_ip||'')+'"></details>'+
     '<div style="display:flex;gap:8px;align-items:center;margin-top:10px"><button class="btn sm" id="b_save">Guardar WiFi</button><span class="hint" id="b_ss">'+(credsOk?'Guardada: «'+esc(bs.ssid)+'»':'Falta guardarla')+'</span></div></div>'+
     '<hr class="sep"><div><button class="btn" id="b_scan"'+(!bs.available||bscan?' disabled':'')+'>'+(bscan?'<span class="spin"></span>Buscando…':'Buscar altavoces por Bluetooth')+'</button>'+
     '<p class="hint" style="margin-top:6px" id="b_hint">'+(bscan?'Puede tardar hasta 40 s. Los ESP32 deben estar encendidos y cerca.':'Enciende los ESP32 nuevos cerca de la Raspberry y busca.')+'</p></div>';
  const used=onlineIds();
  h+=bdevs.map((d,i)=>{
    const r=bres[d.address]||{},st=d.status||{},isNew=d.new,own=!isNew&&d.id!=null;
    const chip=d.error&&!d.status?'<span class="chip bad">No se pudo leer</span>':isNew?'<span class="chip ac">Nuevo, sin WiFi</span>':'<span class="chip">En la red · #'+d.id+'</span>';
    const opts=[0,1,2,3,4,5,6,7,8,9].map(k=>'<option value="'+k+'"'+(k===d.suggested_id?' selected':'')+'>'+k+(used.has(k)&&!(own&&k===d.id)?' (en uso)':'')+'</option>').join('');
    return '<div class="found'+(r.ok?' done':'')+'" data-i="'+i+'"><div class="hd"><b>'+esc(d.name)+'</b>'+chip+'<span class="hint">'+rssiTxt(d.rssi)+(d.rssi!=null?' ('+d.rssi+' dBm)':'')+'</span></div>'+
      (d.error&&!d.status?'<div class="res bad">'+esc(d.error)+'</div>':'')+
      '<div class="ac"><label style="margin:0">Número</label><select class="sel" data-sel="'+i+'" aria-label="Número del altavoz">'+opts+'</select>'+
      '<button class="btn ghost sm" data-id="'+i+'">Sonar</button><button class="btn sm" data-pv="'+i+'"'+(credsOk?'':' disabled')+'>'+(isNew?'Configurar':'Reconfigurar')+'</button></div>'+
      '<div class="res'+(r.ok?' ok':r.err?' bad':'')+'" id="b_r'+i+'">'+(r.ok?'Enviado. Aparecerá en el panel en unos segundos.':r.err?esc(r.err):'')+'</div></div>';}).join('');
  const nn=bdevs.filter(d=>d.new&&d.status&&d.suggested_id!=null&&!(bres[d.address]||{}).ok);
  if(nn.length>1)h+='<button class="btn" id="b_all"'+(credsOk?'':' disabled')+'>Configurar los '+nn.length+' nuevos</button>';
  if(bs.available&&!bscan&&bdevs.length===0&&bleScanned)h+='<div class="empty">No se encontró ningún altavoz. Comprueba que está encendido y cerca, o que su Bluetooth sigue activo (se cierra a los 5 minutos de arrancar si ya tiene WiFi: reinícialo).</div>';
  h+='<p class="hint">¿Prefieres usar el móvil? <a href="'+(window._bleHref||'#')+'">Configurar desde el móvil</a>.</p>';
  $('bbody').innerHTML=h;
  if($('b_save'))$('b_save').onclick=bleSaveWifi;if($('b_scan'))$('b_scan').onclick=bleDoScan;if($('b_all'))$('b_all').onclick=bleProvAll;
  $('bbody').querySelectorAll('[data-id]').forEach(b=>b.onclick=()=>bleIdent(+b.dataset.id));
  $('bbody').querySelectorAll('[data-pv]').forEach(b=>b.onclick=()=>bleProv(+b.dataset.pv));
  if(keep&&$(keep)&&keep.startsWith('b_')&&keep!=='b_scan')$(keep).focus();
}
let bleScanned=false;
async function bleSaveWifi(){const r=await J('/api/ble/wifi',{ssid:$('b_ssid').value,pass:$('b_pass').value,rpi_ip:$('b_rip').value.trim()});
  if(r.ok){toast('WiFi guardada');log('WiFi de los altavoces guardada: «'+$('b_ssid').value+'»');try{bs=await (await fetch('/api/ble/status')).json();}catch(e){}bleBuild();}
  else{$('b_ss').textContent=r.error||'No se pudo guardar';$('b_ss').style.color='var(--bad)';}}
async function bleDoScan(){bscan=true;bleScanned=true;bleBuild();const r=await J('/api/ble/scan',{seconds:6});bscan=false;
  if(r.devices){bdevs=r.devices;bres={};log('Bluetooth: '+bdevs.length+' altavoz(ces) encontrado(s)');}
  else{bdevs=[];toast(r.error||'No se pudo buscar. Comprueba el Bluetooth de la Raspberry.',false);log('Bluetooth: '+(r.error||'error'));}
  bleBuild();if(r.error&&$('b_hint')){$('b_hint').textContent=r.error;$('b_hint').style.color='var(--bad)';}}
async function bleIdent(i){const d=bdevs[i];const r=await J('/api/ble/identify',{address:d.address});if(r.ok)toast('Sonando en '+d.name);else toast(r.error||'No se pudo hacer sonar el altavoz.',false);}
async function bleProvOne(i){const d=bdevs[i],sel=document.querySelector('[data-sel="'+i+'"]'),id=sel?+sel.value:d.suggested_id;
  const own=!d.new&&d.id===id;
  const el=$('b_r'+i);if(el){el.className='res';el.innerHTML='<span class="spin"></span>Configurando…';}
  const r=await J('/api/ble/provision',{address:d.address,id,force:own});
  bres[d.address]=r.ok?{ok:true}:{err:r.error||'No se pudo configurar'};
  if(r.ok){log('Bluetooth: '+d.name+' configurado como #'+id);setTimeout(()=>{sig='';loadNodes();},6000);}else log('Bluetooth: '+d.name+': '+bres[d.address].err);
  return !!r.ok;}
async function bleProv(i){const okk=await bleProvOne(i);if(okk)toast('Enviado a '+bdevs[i].name);else toast('No se pudo configurar el altavoz. El motivo está en la lista.',false);bleBuild();}
async function bleProvAll(){const todo=[];bdevs.forEach((d,i)=>{if(d.new&&d.status&&d.suggested_id!=null&&!(bres[d.address]||{}).ok)todo.push(i);});
  if(!todo.length)return;
  const msg='Se enviará la WiFi «'+bs.ssid+'» a:\n'+todo.map(i=>'  '+bdevs[i].name+' → número '+(document.querySelector('[data-sel="'+i+'"]')||{value:bdevs[i].suggested_id}).value).join('\n')+'\n\n¿Continuar?';
  if(!confirm(msg))return;
  let n=0;for(const i of todo){if(await bleProvOne(i))n++;}toast(n+' de '+todo.length+' enviados',n===todo.length);bleBuild();}

/* Estado de la transmisión y vúmetros (consulta ligera /api/levels) */
const CHS=['FL','FR','C','LFE','SL','SR','SBL','SBR','AXL','AXR'];
const MDB=60;                                   // la escala va de −60 a 0 dBFS
let lvOK=false,lastDraw=0,lvTxt='',holdV=new Array(10).fill(-90),holdT=new Array(10).fill(0);
const NSEG=12,SEGDB=MDB/NSEG;                  // 12 LED por altavoz, 5 dB cada uno
const segCls=i=>{const top=-MDB+SEGDB*(i+1);return top<=-20?'g':top<=-10?'y':'r';};   // verde hasta −20 dB, amarillo hasta −10 dB, rojo por encima
const VUSEGS=Array.from({length:NSEG},(_,k)=>'<i class="led '+segCls(k)+'"></i>').join('');
const vuHtml=ch=>'<span class="vu" aria-hidden="true" data-ch="'+ch+'">'+VUSEGS+'</span>';
$('mb').innerHTML=CHS.map((c,i)=>'<div class="mc" title="'+CH[i]+' · '+CHN[i]+'"><span class="lab">'+c+'</span><i class="lamp" id="lp'+i+'"></i><span class="lvn" id="nm'+i+'">—</span></div>').join('');
const segIdx=db=>db<=-MDB?-1:Math.min(NSEG-1,Math.ceil((db+MDB)/SEGDB)-1);
const LV=new Array(10).fill(-1),PK=new Array(10).fill(-1);
let VUS=[];
function collectVus(){VUS=[...document.querySelectorAll('#spks .vu')].map(v=>({ch:+v.dataset.ch,segs:[...v.children]}));applyVu();}
function applyVu(){   // el vúmetro de cada altavoz sigue el nivel de su canal; el segmento de pico queda 1,2 s
  VUS.forEach(u=>{const lv=LV[u.ch],pk=PK[u.ch];
    for(let k=0;k<NSEG;k++){const on2=k<=lv||k===pk;if(u.segs[k].classList.contains('on')!==on2)u.segs[k].classList.toggle('on',on2);}});
}
const dbTxt=db=>{const v=Math.round(db);return v>=0?'0':'−'+Math.abs(v);};
function setTx(dot,txt,title){const t=txt;if(t!==lvTxt){lvTxt=t;$('hs').textContent=t;}$('dot').className='dot '+dot;$('st').title=title||'';}
function drawMeters(d){
  const now=performance.now(),on=d.capturing,dt=Math.min(0.5,(now-lastDraw)/1000);
  $('meters').classList.toggle('off',!on);
  for(let i=0;i<10;i++){
    const db=on?d.db[i]:-90,sig=on&&db>-70;
    if(db>=holdV[i]){holdV[i]=db;holdT[i]=now;}else if(now-holdT[i]>1200)holdV[i]=Math.max(db,holdV[i]-20*dt);   // el pico se mantiene 1,2 s y baja a 20 dB/s
    LV[i]=segIdx(db);PK[i]=segIdx(holdV[i]);
    const lp=$('lp'+i);if(lp.classList.contains('on')!==sig)lp.classList.toggle('on',sig);
    const t=sig?dbTxt(db):'—',nm=$('nm'+i);if(nm.textContent!==t)nm.textContent=t;
  }
  applyVu();lastDraw=now;
  const mx=on?Math.max.apply(null,d.db):-90;
  $('mtxt').textContent=!on?'Sin captura de la Traktor':mx<-70?'Silencio':'Pico '+mx.toFixed(1).replace('.',',')+' dBFS';
}
async function pollLevels(){
  if(document.hidden)return;
  try{const d=await (await fetch('/api/levels')).json();lvOK=true;drawMeters(d);
    if(!d.capturing)setTx('bad','Sin captura de la Traktor','No llegan bloques de audio de la tarjeta (¿Traktor desconectada o ocupada?).');
    else if(!d.signal)setTx('warn','Traktor conectada, sin señal','La tarjeta captura, pero todos los canales están en silencio (< −70 dBFS).');
    else if(!d.sending)setTx('warn',d.nodes_online?'Hay señal, sin enviar':'Hay señal, ningún altavoz en línea',d.nodes_online?'No se están enviando paquetes.':'Enciende o conecta algún altavoz.');
    else setTx('ok','Transmitiendo · '+d.nodes_online+(d.nodes_online===1?' altavoz':' altavoces'),d.pps+' paquetes de audio por segundo.');
  }catch(e){lvOK=false;setTx('bad','Sin conexión con la Raspberry','');}
}
function lvLoop(){pollLevels().finally(()=>setTimeout(lvLoop,$('p_spk').hidden?1000:150));}

/* Volumen maestro y enlaces */
$('mvs').oninput=function(){$('mvd').textContent=this.value+'%';this.setAttribute('aria-valuetext',this.value+' %');fillRng(this);clearTimeout(window._mv);window._mv=setTimeout(()=>J('/api/volume',{volume:+this.value/100}),120);};
$('logclr').onclick=()=>{$('lb').innerHTML='';};
(function(){const b=location.protocol==='https:'?'':'https://'+location.hostname+':8443';
  $('lcal').href=b+'/calibrate';$('lcal2').href=b+'/calibrate';window._bleHref=b+'/ble';})();
document.querySelectorAll('.rng').forEach(fillRng);
const h0=(location.hash||'').slice(1);if(['spk','snd','lat','log'].includes(h0))showTab(h0);
loadStatus().then(loadNodes);updLat();lvLoop();
setInterval(loadStatus,5000);setInterval(loadNodes,2500);
</script>
</body></html>)HTML";

static const char PAGE_CAL[] = R"HTML(<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#000000"><meta name="color-scheme" content="dark light">
<meta name="mobile-web-app-capable" content="yes"><meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="SonosWiFi"><meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<link rel="manifest" href="/manifest.webmanifest">
<link rel="icon" href="/icon.svg" type="image/svg+xml"><link rel="icon" href="/icons/icon-192.png" sizes="192x192" type="image/png">
<link rel="apple-touch-icon" href="/icons/icon-180.png">
<script>if('serviceWorker' in navigator&&window.isSecureContext)addEventListener('load',function(){navigator.serviceWorker.register('/sw.js').catch(function(){});});</script>
<script>/* Tema de la interfaz: «dark» (por defecto), «graphite», «light» o «auto» (sigue al sistema; resuelve a dark o light). Se guarda en localStorage (por navegador).
   Se ejecuta en el <head> (antes de pintar) para que no haya destello; los botones [data-theme-btn] rotan Oscuro → Grafito → Claro → Automático. */
(function () {
  var KEY = 'sonos_theme', ORDER = ['dark', 'graphite', 'light', 'auto'], NAME = { dark: 'Oscuro', graphite: 'Grafito', light: 'Claro', auto: 'Automático' };
  var META = { dark: '#000000', graphite: '#1c1c1e', light: '#d5d9e0' };
  var ICON = {
    dark: '<path d="M20 14.5A8 8 0 0 1 9.5 4 8 8 0 1 0 20 14.5z"/>',
    graphite: '<rect x="3.5" y="5" width="17" height="14" rx="3"/><path d="M3.5 14h17" /><path d="M3.5 14v2a3 3 0 0 0 3 3h11a3 3 0 0 0 3-3v-2z" fill="currentColor"/>',
    light: '<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/>',
    auto: '<circle cx="12" cy="12" r="8.5"/><path d="M12 3.5v17a8.5 8.5 0 0 0 0-17z" fill="currentColor"/>'
  };
  var mq = window.matchMedia ? matchMedia('(prefers-color-scheme: light)') : null;
  function pref() { var p = 'dark'; try { p = localStorage.getItem(KEY) || 'dark'; } catch (e) {} return ORDER.indexOf(p) < 0 ? 'dark' : p; }
  function apply() {
    var p = pref(), t = p === 'auto' ? (mq && mq.matches ? 'light' : 'dark') : p;
    document.documentElement.setAttribute('data-theme', t);
    var m = document.querySelector('meta[name=theme-color]'); if (m) m.setAttribute('content', META[t]);
    var bs = document.querySelectorAll('[data-theme-btn]');
    for (var i = 0; i < bs.length; i++) {
      var b = bs[i];
      b.innerHTML = '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + ICON[p] + '</svg><span class="tl">' + NAME[p] + '</span>';
      b.setAttribute('aria-label', 'Tema: ' + NAME[p] + '. Pulsa para cambiar');
      b.title = 'Tema: ' + NAME[p];
    }
  }
  function set(p) { try { localStorage.setItem(KEY, p); } catch (e) {} apply(); }
  function next() { set(ORDER[(ORDER.indexOf(pref()) + 1) % ORDER.length]); }
  window.SonosTheme = { get: pref, set: set, next: next, apply: apply };
  apply();
  if (mq && mq.addEventListener) mq.addEventListener('change', apply);
  addEventListener('storage', function (e) { if (e.key === KEY) apply(); });
  document.addEventListener('DOMContentLoaded', function () {
    var bs = document.querySelectorAll('[data-theme-btn]');
    for (var i = 0; i < bs.length; i++) bs[i].addEventListener('click', next);
    apply();
  });
})();</script>
<title>SonosWiFi — Calibración</title>
<style>
/* Sistema de diseño compartido por las tres páginas (panel, /calibrate, /ble). Negro puro + azul señal. */
:root{
  --r-card:24px; --r-sheet:28px;
  --bg:#000; --panel:#0a0d13; --raise:#121722; --line:#1f2735; --line2:#2b3548;
  --fg:#e9edf5; --mut:#8a93a6; --ac:#4aa3ff; --ac-bg:rgba(74,163,255,.13); --ac-ln:rgba(74,163,255,.5);
  --ctl:#6b7a96;                                   /* borde de controles (campos, interruptor apagado): ≥3:1 sobre las superficies */
  --ok:#3ecf8e; --warn:#f5b84a; --bad:#ff6b6b;
  --on-ac:#04121f; --ac-hi:#9ad0ff; --ac-lo:#2f6fb3; --bad-fg:#ffb0b0; --bad-ln:rgba(255,107,107,.45); --warn-ln:rgba(245,184,74,.4);
  --top-bg:rgba(0,0,0,.88); --top-solid:#000; --tabs-bg:rgba(10,13,19,.94); --scrim:rgba(0,0,0,.55); --code-bg:#05070b; --bar-bg:#000; --cl-off:#3a1512;
  --thumb:#fff; --thumb-ring:rgba(74,163,255,.38); --cap-bg:rgba(0,0,0,.65); --glow:rgba(74,163,255,.09); --orb-glow:rgba(74,163,255,.35);
  --led-g:#30d158; --led-y:#ffd60a; --led-r:#ff453a;
  color-scheme:dark;
}
/* Temas alternativos: data-theme="graphite" (gris grafito, de bajo brillo) y "light" (claro suave, sin blanco puro).
   El script del <head> (theme.js) resuelve «Automático» según el sistema. */
html[data-theme=graphite]{
  --bg:#1c1c1e; --panel:#2b2b2d; --raise:#363639; --line:#3d3d41; --line2:#4c4c51;
  --fg:#e9e9ec; --mut:#a3a3aa; --ac:#5aaeff; --ac-bg:rgba(90,174,255,.15); --ac-ln:rgba(90,174,255,.55);
  --ctl:#8a8a91; --ok:#46d39a; --warn:#f0b64a; --bad:#ff7b72;
  --on-ac:#06121f; --ac-hi:#a5d3ff; --ac-lo:#3478b8; --bad-fg:#ffb4ae; --bad-ln:rgba(255,123,114,.5); --warn-ln:rgba(240,182,74,.45);
  --top-bg:rgba(28,28,30,.9); --top-solid:#1c1c1e; --tabs-bg:rgba(43,43,45,.95); --scrim:rgba(0,0,0,.5); --code-bg:#1f1f21; --bar-bg:#18181a; --cl-off:#4a2a28;
  --thumb:#f2f2f4; --thumb-ring:rgba(90,174,255,.4); --cap-bg:rgba(28,28,30,.78); --glow:rgba(90,174,255,.07); --orb-glow:rgba(90,174,255,.35);
  --led-g:#34c759; --led-y:#ffd60a; --led-r:#ff5a50;
  color-scheme:dark;
}
html[data-theme=light]{
  --bg:#d5d9e0; --panel:#e4e7ec; --raise:#d9dde4; --line:#c3c8d1; --line2:#aab1bd;
  --fg:#1a1f27; --mut:#4b5465; --ac:#0a58ac; --ac-bg:rgba(10,88,172,.11); --ac-ln:rgba(10,88,172,.45);
  --ctl:#667184; --ok:#0f6240; --warn:#7a4a00; --bad:#a82620;
  --on-ac:#ffffff; --ac-hi:#4aa3ff; --ac-lo:#7fa9d6; --bad-fg:#8f1a16; --bad-ln:rgba(179,42,37,.5); --warn-ln:rgba(138,83,0,.45);
  --top-bg:rgba(213,217,224,.92); --top-solid:#d5d9e0; --tabs-bg:rgba(228,231,236,.95); --scrim:rgba(26,31,39,.35); --code-bg:#cfd4dc; --bar-bg:#0b0e14; --cl-off:#e0c3c1;
  --thumb:#f4f5f8; --thumb-ring:rgba(10,88,172,.35); --cap-bg:rgba(228,231,236,.88); --glow:rgba(10,88,172,.06); --orb-glow:rgba(10,88,172,.3);
  --led-g:#1fa84a; --led-y:#d9a400; --led-r:#e0342b;
  color-scheme:light;
}
*{box-sizing:border-box;margin:0}
html{font-size:106.25%}          /* 17 px: tamaño de texto por defecto de iOS; todo lo demás va en rem y sigue el ajuste del navegador */
body{background:var(--bg);color:var(--fg);font:1rem/1.45 var(--font);-webkit-font-smoothing:antialiased;
  font-variant-numeric:tabular-nums;min-height:100vh;-webkit-text-size-adjust:100%;
  padding-left:env(safe-area-inset-left);padding-right:env(safe-area-inset-right)}
a{color:var(--ac);text-decoration:none}a:hover{text-decoration:underline}
button,input,select{font:inherit;color:inherit}
button{cursor:pointer}
[hidden]{display:none!important}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px;border-radius:6px}
h1{font-size:1.55rem;font-weight:650;letter-spacing:-.01em}
h2{font-size:1.02rem;font-weight:650}
.mut{color:var(--mut)}
.small{font-size:.86rem}

.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;text-decoration:none;line-height:1.2;font-family:inherit;appearance:none;border:0;background:var(--ac);color:var(--on-ac);font-weight:650;padding:10px 22px;border-radius:999px;min-height:44px;box-shadow:0 8px 22px -10px var(--orb-glow);
  transition:filter .12s,transform .06s,background .12s}
.btn:hover{filter:brightness(1.1);text-decoration:none}.btn:active{transform:translateY(1px)}
.btn.ghost{background:var(--raise);color:var(--fg);border:1px solid var(--line2);font-weight:600;box-shadow:none}
.btn.ghost:hover{background:var(--line);filter:none}
.btn.danger{background:transparent;color:var(--bad);border:1px solid var(--bad-ln);font-weight:600;box-shadow:none}
.btn.sm{padding:6px 16px;min-height:36px;font-size:.88rem}
.btn:disabled{opacity:.4;cursor:not-allowed;filter:none}

.seg{display:inline-flex;background:var(--code-bg);border:1px solid var(--line);border-radius:999px;padding:4px;gap:2px;max-width:100%}
.seg button{appearance:none;border:0;background:transparent;color:var(--mut);padding:7px 16px;border-radius:999px;min-height:36px;font-weight:600;white-space:nowrap;display:inline-flex;align-items:center;justify-content:center;gap:8px;transition:background .12s,color .12s}
.seg button:hover{color:var(--fg)}
.seg button[aria-pressed=true]{background:var(--ac-bg);color:var(--ac);box-shadow:inset 0 0 0 1px var(--ac-ln)}
.seg button[aria-pressed=true]::before{content:"";width:7px;height:7px;border-radius:50%;background:currentColor;flex:none}
.seg.block{display:flex}.seg.block button{flex:1;min-width:0;white-space:normal;line-height:1.2;padding-left:8px;padding-right:8px}   /* con texto grande o pantalla estrecha, las etiquetas bajan de línea en vez de salirse */

.switch{appearance:none;position:relative;width:52px;height:32px;border-radius:16px;border:1px solid var(--ctl);background:var(--raise);flex:none;
  transition:background .15s,border-color .15s}
.switch::after{content:"";position:absolute;top:3px;left:3px;width:24px;height:24px;border-radius:50%;background:var(--mut);transition:transform .15s,background .15s}
.switch[aria-checked=true]{background:var(--ac);border-color:var(--ac)}
.switch[aria-checked=true]::after{transform:translateX(20px);background:var(--on-ac)}

input[type=range].rng{-webkit-appearance:none;appearance:none;width:100%;height:28px;background:transparent;border-radius:0;cursor:pointer;
  --f:.5;--rh:28px;height:var(--rh);
  background:linear-gradient(to right,var(--ac) calc(var(--f)*(100% - 38px) + 19px),var(--line2) 0) no-repeat center/100% 10px}
input[type=range].rng.mid{--lo:.5;--hi:.5;
  background:linear-gradient(to right,var(--line2) calc(var(--lo)*(100% - 38px) + 19px),var(--ac) 0,var(--ac) calc(var(--hi)*(100% - 38px) + 19px),var(--line2) 0) no-repeat center/100% 10px}
.rng::-webkit-slider-runnable-track{height:var(--rh);background:transparent}
.rng::-moz-range-track{height:var(--rh);background:transparent}
.rng::-webkit-slider-thumb{-webkit-appearance:none;width:38px;height:26px;border-radius:13px;background:var(--thumb);border:0;margin-top:calc((var(--rh) - 26px)/2);box-shadow:0 2px 8px rgba(0,0,0,.35),0 0 0 3px var(--thumb-ring)}
.rng::-moz-range-thumb{width:38px;height:26px;border-radius:13px;background:var(--thumb);border:0;box-shadow:0 2px 8px rgba(0,0,0,.35),0 0 0 3px var(--thumb-ring)}
.rng:focus-visible{outline-offset:0}

.num,.txt,.sel{background:var(--raise);border:1px solid var(--ctl);border-radius:14px;padding:9px 14px;min-height:44px;width:100%;color:var(--fg)}
.num{width:110px}
.num:focus,.txt:focus,.sel:focus{outline:2px solid var(--ac);outline-offset:1px;border-color:transparent}
.sel{appearance:none;background-image:linear-gradient(45deg,transparent 50%,var(--mut) 50%),linear-gradient(135deg,var(--mut) 50%,transparent 50%);
  background-position:calc(100% - 21px) 19px,calc(100% - 16px) 19px;background-size:5px 5px,5px 5px;background-repeat:no-repeat;padding-right:32px}
label,.lbl{display:block;color:var(--mut);font-size:.74rem;font-weight:600;letter-spacing:.07em;text-transform:uppercase;margin-bottom:7px}

.chip{display:inline-flex;align-items:center;gap:7px;padding:5px 13px;border-radius:999px;background:var(--raise);border:1px solid var(--line);font-size:.84rem;color:var(--mut)}
.chip.ac{color:var(--ac);border-color:var(--ac-ln);background:var(--ac-bg)}
.chip.warn{color:var(--warn);border-color:var(--warn-ln)}
.chip.bad{color:var(--bad);border-color:var(--bad-ln)}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--mut);flex:none}
.dot.ok{background:var(--ok)}.dot.bad{background:var(--bad)}.dot.warn{background:var(--warn)}

.top{position:sticky;top:0;z-index:30;background:var(--top-bg);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);border-bottom:1px solid var(--line);
  padding-top:env(safe-area-inset-top)}               /* en la app instalada (barra de estado translúcida) el contenido no queda bajo la muesca */
.top .in{max-width:1180px;margin:auto;display:flex;align-items:center;gap:18px;padding:10px 18px;flex-wrap:wrap}
.brand{display:flex;align-items:center;gap:10px;font-weight:700;font-size:1.05rem;color:var(--fg)}
.brand:hover{text-decoration:none}
.logo{width:26px;height:26px;flex:none;color:var(--ac)}
.panelbox{background:var(--panel);border:1px solid var(--line);border-radius:var(--r-card);padding:20px}
.note{border-radius:16px;padding:12px 14px;background:var(--raise);border:1px solid var(--line2);font-size:.92rem}
.note.bad{border-color:var(--bad-ln);color:var(--bad-fg)}.note.warn{border-color:var(--warn-ln)}

.toasts{position:fixed;left:50%;bottom:calc(18px + env(safe-area-inset-bottom));transform:translateX(-50%);display:flex;flex-direction:column;gap:6px;z-index:60;pointer-events:none;width:max-content;max-width:92vw}
.toast{pointer-events:auto;display:flex;align-items:center;background:var(--raise);border:1px solid var(--line2);padding:10px 16px;border-radius:999px;font-size:.9rem;
  animation:tin .18s ease-out}
.toast::before{content:"";flex:none;width:9px;height:9px;border-radius:50%;background:var(--ok);margin-right:10px}
.toast.bad::before{background:var(--bad)}
@keyframes tin{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
.procbar{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin:0 0 14px;padding:10px 18px;border-radius:999px;background:var(--ac-bg);border:1px solid var(--ac-ln);color:var(--ac);font-size:.92rem}
.procbar[hidden]{display:none}

/* Objetivos táctiles: 44 × 44 (mínimo cómodo de iOS) cuando el puntero es el dedo */
@media (pointer:coarse){
  .btn,.btn.sm,.seg button,.tab,.num,.txt,.sel{min-height:44px}
  input[type=range].rng{--rh:44px}
  .switch::before{content:"";position:absolute;inset:-9px -6px}   /* zona de toque ampliada; el aspecto no cambia */
}
.themebtn{display:inline-flex;align-items:center;gap:6px}
.themebtn svg{flex:none}
@media (max-width:760px){.themebtn .tl{display:none}.themebtn{padding-left:10px;padding-right:10px}}
/* Ajustes de accesibilidad del sistema */
@media (prefers-contrast:more){
  :root{--mut:#b4bccc;--ctl:#8d9ab3;--line2:#6b7a96;--ac:#7ec0ff;--ac-ln:rgba(126,192,255,.8)}
  html[data-theme=light]{--mut:#323b4a;--ctl:#4a5468;--line2:#7d8798;--ac:#0a4f99;--ac-ln:rgba(10,79,153,.8)}
  html[data-theme=graphite]{--mut:#c6c6cc;--ctl:#a8a8af;--line2:#7d7d84;--ac:#8cc6ff;--ac-ln:rgba(140,198,255,.8)}
}
@media (prefers-reduced-transparency:reduce){
  .top{background:var(--top-solid);backdrop-filter:none;-webkit-backdrop-filter:none}
}
main{max-width:760px;margin:0 auto;padding:20px 16px 80px;display:flex;flex-direction:column;gap:16px}
.top .in{max-width:760px}
.steps{display:flex;gap:8px;align-items:center;color:var(--mut);font-size:.9rem}
.steps i{flex:1;height:2px;background:var(--line);border-radius:2px}
.steps span{display:flex;align-items:center;gap:8px;white-space:nowrap}
.steps b{width:24px;height:24px;border-radius:50%;border:1.5px solid var(--line2);display:grid;place-items:center;font-size:.8rem}
.steps .on{color:var(--fg)}.steps .on b{border-color:var(--ac);color:var(--ac)}
.steps .done b{background:var(--ac);border-color:var(--ac);color:var(--on-ac)}
.opts{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.meter{height:10px;background:var(--raise);border-radius:5px;overflow:hidden;border:1px solid var(--line)}
.meter>div{height:100%;width:0;background:linear-gradient(90deg,var(--ac),var(--ac-hi));transition:width .08s}
.prog{height:4px;background:var(--line);border-radius:2px;overflow:hidden}.prog>div{height:100%;width:0;background:var(--ac);transition:width .25s linear}
.count{font-size:2.6rem;font-weight:700;letter-spacing:-.02em;line-height:1}
.node{border:1px solid var(--line);background:var(--panel);border-radius:var(--r-card);padding:14px 16px;display:flex;flex-direction:column;gap:12px}
.node.bad{border-color:var(--bad-ln)}
.node h3{font-size:1rem;display:flex;align-items:center;gap:10px}
.node h3 .chip{margin-left:auto}
.mets{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.mets div{background:var(--raise);border-radius:10px;padding:8px 10px}
.mets span{display:block;font-size:.76rem;color:var(--mut)}.mets b{font-weight:600}
.eqm{display:flex;align-items:stretch;gap:6px;height:64px}
.eqm div{flex:1;position:relative;background:var(--raise);border-radius:6px}
.eqm div::before{content:"";position:absolute;left:0;right:0;top:50%;height:1px;background:var(--line2)}
.eqm i{position:absolute;left:18%;right:18%;border-radius:3px;background:var(--ac)}
.eqm i.neg{background:var(--ac-lo)}
.eqm em{position:absolute;left:0;right:0;bottom:-18px;text-align:center;font-style:normal;font-size:.7rem;color:var(--mut)}
.eqw{padding-bottom:20px}
.tg{display:flex;gap:18px;flex-wrap:wrap}
.tg label.ck{display:flex;align-items:center;gap:8px;margin:0;color:var(--fg);font-size:.9rem}
input[type=checkbox]{appearance:none;width:20px;height:20px;border-radius:6px;border:1.5px solid var(--line2);background:var(--raise);display:inline-grid;place-items:center;flex:none;cursor:pointer}
input[type=checkbox]:checked{background:var(--ac);border-color:var(--ac)}
input[type=checkbox]:checked::after{content:"";width:5px;height:10px;border:solid var(--on-ac);border-width:0 2.5px 2.5px 0;transform:rotate(45deg) translate(-1px,-1px)}
.applybox{display:flex;flex-direction:column;gap:12px}
.log{font:.82rem/1.6 ui-monospace,Consolas,monospace;color:var(--mut);white-space:pre-wrap;max-height:150px;overflow:auto}
@media (max-width:560px){.opts{grid-template-columns:1fr}.mets{grid-template-columns:1fr 1fr}.steps span em{display:none}}
</style></head><body>
<div class="top"><div class="in">
  <a class="brand" href="/"><svg class="logo" viewBox="0 0 26 26" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M3 13h2M8 8v10M13 4v18M18 8v10M23 13h-2"/></svg>SonosWiFi</a>
  <span style="margin-left:auto;display:flex;gap:6px"><button class="btn ghost sm themebtn" data-theme-btn type="button"></button><a class="btn ghost sm" href="/">Volver al panel</a></span>
</div></div>
<main>
  <div><h1>Calibración acústica</h1><p class="mut">Mide cuándo y con qué nivel llega cada altavoz a tu asiento, y propone latencia, volumen y EQ.</p></div>
  <div class="steps" id="steps"><span class="on" id="sp1"><b>1</b><em>Preparar</em></span><i></i><span id="sp2"><b>2</b><em>Medir</em></span><i></i><span id="sp3"><b>3</b><em>Aplicar</em></span></div>

  <div class="panelbox" id="v1" style="display:flex;flex-direction:column;gap:14px">
    <p>Pon el móvil <b>donde te sientas a escuchar</b>, con el micrófono libre y la sala en silencio. Cada altavoz emite un barrido y se mide lo que llega. Dura unos 20 s.</p>
    <div class="opts">
      <div><label for="lvl">Volumen de la prueba (dBFS)</label><input class="num" style="width:100%" id="lvl" type="number" value="-12" min="-40" max="-3" step="1"></div>
      <div><label for="rounds">Rondas (más = más precisión)</label><input class="num" style="width:100%" id="rounds" type="number" value="2" min="1" max="3" step="1"></div>
    </div>
    <div class="note bad" id="err" hidden></div>
    <div><button class="btn" id="go">Empezar a medir</button></div>
  </div>

  <div class="panelbox" id="v2" style="display:none;flex-direction:column;gap:14px">
    <div class="count" id="count">—</div>
    <div class="prog"><div id="prog"></div></div>
    <p id="msg" class="mut">Silencio, por favor…</p>
    <div><div class="lbl">Micrófono</div><div class="meter"><div id="mic"></div></div></div>
    <div><button class="btn ghost" id="cancel">Cancelar</button></div>
  </div>

  <div id="v3" style="display:none;flex-direction:column;gap:14px">
    <div id="cards" style="display:flex;flex-direction:column;gap:12px"></div>
    <div class="panelbox applybox">
      <h2>Qué quieres aplicar</h2>
      <div class="tg">
        <label class="ck"><input type="checkbox" id="ck_lat" checked> Latencias</label>
        <label class="ck"><input type="checkbox" id="ck_vol" checked> Volúmenes</label>
        <label class="ck"><input type="checkbox" id="ck_eq" checked> EQ</label>
      </div>
      <div class="note" id="eqoff" hidden><label class="ck" style="display:flex;gap:8px;align-items:flex-start"><input type="checkbox" id="ck_enable" checked> <span>La EQ está en pass-through. Activarla al aplicar, para que se oiga el cambio.</span></label></div>
      <p class="mut small">Latencia: retrasa los altavoces cercanos para que todo llegue a la vez. Volumen: iguala al más bajo. EQ: acerca el timbre de cada altavoz al promedio, con ±6 dB como máximo. Los subwoofers vienen sin marcar en volumen y EQ.</p>
      <div class="row2" style="display:flex;gap:8px;flex-wrap:wrap">
        <button class="btn" id="apply">Aplicar selección</button>
        <button class="btn ghost" id="again">Medir otra vez</button>
        <button class="btn danger" id="reset">Restablecer todo</button>
      </div>
      <div class="small" id="applied" style="color:var(--ok)"></div>
    </div>
  </div>

  <div class="panelbox"><h2 style="margin-bottom:8px">Actividad</h2><div class="log" id="log" role="log"></div></div>
</main>
<div class="toasts" id="tb"></div>
<script>
/*ANALYSIS_BEGIN*/
function makeChirp(sr,f0,f1,chirpMs,fadeMs){
  const n=Math.floor(chirpMs/1000*sr),T=chirpMs/1000,r=Math.log(f1/f0),K=2*Math.PI*f0*T/r,out=new Float32Array(n);
  for(let i=0;i<n;i++){const t=i/sr,tu=t*1e6,fu=fadeMs*1000;let w=1;
    if(tu<fu)w=0.5-0.5*Math.cos(Math.PI*tu/fu);else if(tu>chirpMs*1000-fu)w=0.5-0.5*Math.cos(Math.PI*(chirpMs*1000-tu)/fu);
    out[i]=Math.sin(K*(Math.exp(t*r/T)-1))*w;}
  return out;
}
function fft(re,im,inv){
  const n=re.length;
  for(let i=1,j=0;i<n;i++){let bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;
    if(i<j){let t=re[i];re[i]=re[j];re[j]=t;t=im[i];im[i]=im[j];im[j]=t;}}
  for(let len=2;len<=n;len<<=1){const ang=2*Math.PI/len*(inv?1:-1),wr=Math.cos(ang),wi=Math.sin(ang),h=len>>1;
    for(let i=0;i<n;i+=len){let cr=1,ci=0;
      for(let j=0;j<h;j++){const a=i+j,b=a+h,vr=re[b]*cr-im[b]*ci,vi=re[b]*ci+im[b]*cr;
        re[b]=re[a]-vr;im[b]=im[a]-vi;re[a]+=vr;im[a]+=vi;const nc=cr*wr-ci*wi;ci=cr*wi+ci*wr;cr=nc;}}}
  if(inv)for(let i=0;i<n;i++){re[i]/=n;im[i]/=n;}
}
// Correlación cruzada rec ⋆ tmpl por bloques (overlap-save): c[n]=Σ rec[n+m]·tmpl[m]
function xcorr(rec,tmpl,N){
  const M=tmpl.length,step=N-M+1,out=new Float32Array(rec.length),tr=new Float64Array(N),ti=new Float64Array(N);
  tr.set(tmpl);fft(tr,ti,false);
  const sr=new Float64Array(N),si=new Float64Array(N);
  for(let s=0;s<rec.length;s+=step){
    sr.fill(0);si.fill(0);const e=Math.min(rec.length,s+N);
    for(let i=s;i<e;i++)sr[i-s]=rec[i];
    fft(sr,si,false);
    for(let i=0;i<N;i++){const a=sr[i],b=si[i];sr[i]=a*tr[i]+b*ti[i];si[i]=b*tr[i]-a*ti[i];}  // R·conj(T)
    fft(sr,si,true);
    const lim=Math.min(step,rec.length-s);
    for(let i=0;i<lim;i++)out[s+i]=sr[i];
  }
  return out;
}
function power(rec,a,b){a=Math.max(0,Math.floor(a));b=Math.min(rec.length,Math.floor(b));let s=0;
  for(let i=a;i<b;i++)s+=rec[i]*rec[i];return b>a?s/(b-a):0;}
// Potencia por banda (lineal) de rec[start .. start+lenS) mediante FFT. El chirp logarítmico reparte la misma energía en cada octava,
// así que las potencias de las bandas son comparables entre sí y entre altavoces.
function bandPow(rec,sr,start,lenS,edges){
  let N=1;while(N<lenS)N<<=1;const re=new Float64Array(N),im=new Float64Array(N),a=Math.floor(start);
  for(let i=0;i<lenS;i++){const j=a+i;if(j>=0&&j<rec.length)re[i]=rec[j];}
  fft(re,im,false);
  const out=new Float64Array(edges.length-1),df=sr/N;
  for(let b=0;b<out.length;b++){const k0=Math.ceil(edges[b]/df),k1=Math.min(N/2-1,Math.floor(edges[b+1]/df));let t=0;
    for(let k=k0;k<=k1;k++)t+=re[k]*re[k]+im[k]*im[k];out[b]=t;}
  return out;
}
// p = {slot_ms,chirp_ms,f0,f1,fade_ms,sequence:[ids], expect_s?, eq_edges?:[Hz…]}.  expect_s = instante (s, en la grabación) en que
// debería sonar el 1er chirp; evita que un nodo mudo desplace la secuencia un slot (ambigüedad de periodicidad).
function analyze(rec,sr,p){
  const tm=makeChirp(sr,p.f0,p.f1,p.chirp_ms,p.fade_ms),c=xcorr(rec,tm,65536);
  const B=Math.max(1,Math.round(sr/1000)),nb=Math.floor(c.length/B),bm=new Float32Array(nb);
  for(let j=0;j<nb;j++){let m=0;for(let i=j*B;i<(j+1)*B;i++){const a=Math.abs(c[i]);if(a>m)m=a;}bm[j]=m;}
  const med=Math.max(Float32Array.from(bm).sort()[nb>>1],1e-12),W=60,mm=new Float32Array(nb);
  for(let j=0;j<nb;j++){let m=0;for(let i=Math.max(0,j-W);i<=Math.min(nb-1,j+W);i++)if(bm[i]>m)m=bm[i];mm[j]=m;}
  const L=p.sequence.length,slot=p.slot_ms,slotS=slot/1000*sr;
  if(nb<(L-1)*slot+1)return{error:'Grabación demasiado corta'};
  let best=-1e300,bj=0,jlo=0,jhi=nb;
  if(p.expect_s!=null){jlo=Math.max(0,Math.floor((p.expect_s-0.65)*1000));jhi=Math.ceil((p.expect_s+0.65)*1000)+1;}
  for(let j=jlo;j<jhi&&j+(L-1)*slot<nb;j++){let sc=0;for(let k=0;k<L;k++)sc+=Math.log(Math.max(mm[j+k*slot]/med,1));if(sc>best){best=sc;bj=j;}}
  const peakIn=(lo,hi)=>{lo=Math.max(1,Math.floor(lo));hi=Math.min(c.length-2,Math.floor(hi));let bi=lo,bv=0;
    for(let i=lo;i<=hi;i++){const a=Math.abs(c[i]);if(a>bv){bv=a;bi=i;}}
    const y0=Math.abs(c[bi-1]),y1=Math.abs(c[bi]),y2=Math.abs(c[bi+1]),d=y0-2*y1+y2;
    return{pos:bi+(Math.abs(d)>1e-20?0.5*(y0-y2)/d:0),val:bv};};
  const p0=peakIn((bj-W-5)*B,(bj+W+5)*B).pos,slots=[];
  for(let k=0;k<L;k++){const ex=p0+k*slotS,pk=peakIn(ex-W*B,ex+W*B);
    slots.push({k,id:p.sequence[k],pos:pk.pos,snr:pk.val/med});}
  const nEnd=Math.min(1.0*sr,p0-0.3*sr),Pn=power(rec,0.1*sr,nEnd);
  const edges=p.eq_edges||null,segLen=Math.round((p.chirp_ms+34)/1000*sr);
  let Nb=null;                                   // ruido por banda (misma longitud de ventana que el chirp)
  if(edges&&0.1*sr+segLen<p0-0.1*sr)Nb=bandPow(rec,sr,0.1*sr,segLen,edges);
  const per={};
  for(const s of slots){
    const d=((s.pos-slots[0].pos)/sr-s.k*slot/1000)*1e6;
    const P=power(rec,s.pos,s.pos+p.chirp_ms/1000*sr);
    const o=per[s.id]||(per[s.id]={id:s.id,d:[],P:[],snr:1e300,B:[]});
    o.d.push(d);o.P.push(P);o.snr=Math.min(o.snr,s.snr);
    if(edges)o.B.push(bandPow(rec,sr,s.pos-0.004*sr,segLen,edges));}
  const nodes=Object.values(per).sort((a,b)=>a.id-b.id).map(o=>{
    const dm=o.d.reduce((a,b)=>a+b,0)/o.d.length,Pm=o.P.reduce((a,b)=>a+b,0)/o.P.length;
    const r={id:o.id,d_us:dm,spread_us:Math.max(...o.d)-Math.min(...o.d),
      level_db:10*Math.log10(Math.max(Pm-Pn,1e-14)),snr:o.snr,ok:o.snr>=8&&Pm>Pn*2,bands:null,band_ok:null};
    if(edges&&o.B.length){const nb=edges.length-1;r.bands=[];r.band_ok=[];
      for(let b=0;b<nb;b++){let t=0;for(const B of o.B)t+=B[b];t/=o.B.length;const nz=Nb?Nb[b]:0;
        r.band_ok.push(t>nz*4&&t>1e-12);r.bands.push(10*Math.log10(Math.max(t-nz,1e-14)));}}
    return r;});
  return{nodes,slots,noise_db:10*Math.log10(Math.max(Pn,1e-14))};
}
// EQ propuesto: iguala la respuesta por bandas de cada altavoz a la MEDIA de los altavoces medidos (el mismo micrófono mide
// a todos, así su propia respuesta se cancela). Se quita el nivel global (eso lo corrige el volumen) y se limita a ±6 dB.
function propose(nodes,volIncl,eqIncl){
  volIncl=volIncl||{};eqIncl=eqIncl||{};
  const good=nodes.filter(n=>n.ok);if(!good.length)return nodes;
  const dmax=Math.max(...good.map(n=>n.d_us));
  const lv=good.filter(n=>volIncl[n.id]!==false).map(n=>n.level_db),Lmin=lv.length?Math.min(...lv):0;
  const eqn=good.filter(n=>n.bands&&eqIncl[n.id]!==false),nb=eqn.length?eqn[0].bands.length:0,R=[];
  for(let b=0;b<nb;b++){const v=eqn.filter(n=>n.band_ok[b]).map(n=>n.bands[b]);R.push(v.length>=2?v.reduce((x,y)=>x+y,0)/v.length:null);}
  return nodes.map(n=>{const o=Object.assign({},n);
    if(n.ok){o.latency_us=Math.max(0,Math.min(500000,Math.round(dmax-n.d_us)));
      o.volume=(volIncl[n.id]!==false)?Math.max(1,Math.min(255,Math.round(255*Math.pow(10,(Lmin-n.level_db)/20)))):255;}
    o.eq=null;
    if(n.ok&&n.bands&&eqIncl[n.id]!==false&&eqn.length>=2){
      const d=n.bands.map((x,b)=>(R[b]!=null&&n.band_ok[b])?R[b]-x:null),dv=d.filter(x=>x!=null);
      if(dv.length){const m=dv.reduce((x,y)=>x+y,0)/dv.length;
        o.eq=d.map(x=>x==null?0:Math.round(Math.max(-6,Math.min(6,x-m))*10)/10);}}
    return o;});
}
/*ANALYSIS_END*/

const $=id=>document.getElementById(id);
const log=m=>{const l=$('log');l.textContent+=new Date().toLocaleTimeString()+'  '+m+'\n';l.scrollTop=l.scrollHeight;};
function toast(m,ok=true){const d=document.createElement('div');d.className='toast'+(ok?'':' bad');d.textContent=m;$('tb').appendChild(d);setTimeout(()=>d.remove(),2600);}
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function api(m,p,b){const r=await fetch(p,{method:m,headers:{'Content-Type':'application/json'},body:b?JSON.stringify(b):undefined});
  let j={};try{j=await r.json();}catch(e){}j._status=r.status;return j;}
let chunks=[],lastRes=null,volIncl={},eqIncl={},cancelled=false,eqOn=true;
const CH=['FL','FR','C','LFE','SL','SR','SBL','SBR','AUX-L','AUX-R'];
let info={};
function view(n){$('v1').style.display=n===1?'flex':'none';$('v2').style.display=n===2?'flex':'none';$('v3').style.display=n===3?'flex':'none';
  [1,2,3].forEach(i=>$('sp'+i).className=i===n?'on':i<n?'done':'');}
function showErr(h){$('err').hidden=!h;$('err').innerHTML=h||'';}

async function runCal(){
  cancelled=false;showErr('');
  if(!window.isSecureContext||!navigator.mediaDevices){
    showErr('<b>El micrófono exige HTTPS.</b> Abre <a href="https://'+location.hostname+':8443/calibrate">https://'+location.hostname+':8443/calibrate</a> y acepta el aviso del certificado.');return;}
  $('go').disabled=true;
  try{
    const stream=await navigator.mediaDevices.getUserMedia({audio:{echoCancellation:false,noiseSuppression:false,autoGainControl:false,channelCount:1}});
    view(2);$('count').textContent='…';$('prog').style.width='0';$('msg').textContent='Silencio, por favor…';
    const ctx=new (window.AudioContext||window.webkitAudioContext)(),sr=ctx.sampleRate;
    const src=ctx.createMediaStreamSource(stream),proc=ctx.createScriptProcessor(4096,1,1);
    chunks=[];
    proc.onaudioprocess=e=>{const d=e.inputBuffer.getChannelData(0);chunks.push(new Float32Array(d));
      let s=0;for(let i=0;i<d.length;i+=8)s+=d[i]*d[i];$('mic').style.width=Math.min(100,Math.sqrt(s/(d.length/8))*400)+'%';};
    src.connect(proc);proc.connect(ctx.destination);
    log('Micrófono a '+sr+' Hz. Midiendo ruido ambiente (1,3 s)…');
    await sleep(1300);
    let nPost=0;for(const c of chunks)nPost+=c.length;
    const cfg=await api('POST','/api/cal/start',{level_db:+$('lvl').value,rounds:+$('rounds').value});
    if(!cfg.ok){stream.getTracks().forEach(t=>t.stop());await ctx.close();
      view(1);showErr('<b>'+(cfg.error||'No se pudo empezar')+'</b>');$('go').disabled=false;return;}
    try{const ni=await (await fetch('/api/nodes')).json();for(const n of ni)if(n.role===2){if(volIncl[n.id]===undefined)volIncl[n.id]=false;if(eqIncl[n.id]===undefined)eqIncl[n.id]=false;}}catch(e){}
    try{eqOn=!!(await (await fetch('/api/status')).json()).dsp_eq;}catch(e){}
    log('Secuencia: altavoces '+cfg.sequence.join(', ')+' ('+(cfg.total_ms/1000).toFixed(1)+' s)');
    const t0=Date.now(),tot=cfg.total_ms+900;
    $('msg').textContent='Midiendo cada altavoz. No hagas ruido.';
    while(Date.now()-t0<tot&&!cancelled){
      const el=Date.now()-t0;$('count').textContent=Math.max(0,Math.round((tot-el)/1000))+' s';$('prog').style.width=Math.min(100,el*100/tot)+'%';await sleep(200);}
    stream.getTracks().forEach(t=>t.stop());proc.disconnect();src.disconnect();await ctx.close();$('mic').style.width='0';
    if(cancelled){view(1);$('go').disabled=false;return;}
    let n=0;for(const c of chunks)n+=c.length;const rec=new Float32Array(n);let o=0;for(const c of chunks){rec.set(c,o);o+=c.length;}
    $('count').textContent='…';$('msg').textContent='Analizando…';await sleep(30);
    const res=analyze(rec,sr,{slot_ms:cfg.slot_ms,chirp_ms:cfg.chirp_ms,f0:cfg.f0,f1:cfg.f1,fade_ms:cfg.fade_ms,sequence:cfg.sequence,expect_s:nPost/sr+cfg.lead_ms/1000,eq_edges:cfg.eq_edges});
    if(res.error){view(1);showErr('<b>'+res.error+'</b>');$('go').disabled=false;return;}
    lastRes=res;log('Ruido ambiente: '+res.noise_db.toFixed(1)+' dB');
    try{info={};for(const n of await (await fetch('/api/nodes')).json())info[n.id]=n;}catch(e){}
    render();view(3);$('applied').textContent='';
  }catch(e){view(1);showErr('<b>'+e.name+':</b> '+e.message);log('Error: '+e);}
  $('go').disabled=false;
}
async function stopCal(){cancelled=true;await api('POST','/api/cal/stop');log('Cancelado');}

function eqBars(eq){
  const M=6;return '<div class="eqw"><div class="eqm">'+['354','707','1,4k','2,8k','5,7k','9,8k'].map((f,i)=>{const g=eq[i]||0,h=Math.min(1,Math.abs(g)/M)*50;
    return '<div title="'+f+' Hz: '+(g>0?'+':'')+g.toFixed(1)+' dB"><i class="'+(g<0?'neg':'')+'" style="'+(g>=0?'bottom:50%':'top:50%')+';height:'+h+'%"></i><em>'+f+'</em></div>';}).join('')+'</div></div>';
}
function render(){
  const ps=propose(lastRes.nodes,volIncl,eqIncl);let h='';
  for(const n of ps){const nd=info[n.id]||{},lab=nd.ch_left!==undefined?(nd.mono||nd.ch_left===nd.ch_right?CH[nd.ch_left]:CH[nd.ch_left]+' + '+CH[nd.ch_right]):'';
    const sub=nd.role===2;
    h+='<div class="node'+(n.ok?'':' bad')+'"><h3>Altavoz #'+n.id+(lab?' · '+lab:'')+(sub?' · sub':'')+
      '<span class="chip '+(n.ok?'ac':'bad')+'">'+(n.ok?'Señal buena':'Señal débil')+'</span></h3>'+
      (n.ok?'<div class="mets"><div><span>Llegada</span><b>'+(n.d_us/1000).toFixed(2)+' ms</b></div><div><span>Dispersión</span><b'+(n.spread_us>200?' style="color:var(--warn)"':'')+'>±'+(n.spread_us/1000).toFixed(2)+' ms</b></div>'+
      '<div><span>Nivel</span><b>'+n.level_db.toFixed(1)+' dB</b></div><div><span>SNR</span><b>'+(n.snr>999?'999+':n.snr.toFixed(0))+' dB</b></div></div>'+
      '<div class="mets" style="grid-template-columns:1fr 1fr"><div><span>Latencia propuesta</span><b>'+(n.latency_us/1000).toFixed(3)+' ms</b></div><div><span>Volumen propuesto</span><b>'+Math.round(n.volume/2.55)+'%</b></div></div>'+
      (n.eq?'<div><div class="lbl">EQ propuesta (±6 dB)</div>'+eqBars(n.eq)+'</div>':'')+
      '<div class="tg"><label class="ck"><input type="checkbox" data-k="v" data-id="'+n.id+'"'+(volIncl[n.id]===false?'':' checked')+'> Incluir en volumen</label><label class="ck"><input type="checkbox" data-k="e" data-id="'+n.id+'"'+(eqIncl[n.id]===false?'':' checked')+'> Incluir en EQ</label></div>'
      :'<p class="mut small">No se oyó bien este altavoz. Sube el volumen de la prueba o acerca el móvil.</p>')+'</div>';}
  $('cards').innerHTML=h;
  $('cards').querySelectorAll('input[type=checkbox]').forEach(c=>c.onchange=()=>{(c.dataset.k==='v'?volIncl:eqIncl)[c.dataset.id]=c.checked;render();});
  $('eqoff').hidden=eqOn;
  const bad=ps.filter(n=>!n.ok).map(n=>'#'+n.id);
  if(bad.length)log('AVISO: señal débil en '+bad.join(', ')+' (sube el nivel de prueba o acerca el móvil)');
  if(ps.some(n=>n.spread_us>200))log('AVISO: ± > 0,2 ms entre rondas: repite con más rondas y menos ruido');
}
async function apply(lat,vol,eq){
  const ps=propose(lastRes.nodes,volIncl,eqIncl).filter(n=>n.ok);let k=0;
  for(const n of ps){
    if(lat){await api('POST','/api/calibrate',{esp_id:n.id,latency_us:n.latency_us});}
    if(vol){await api('POST','/api/node',{id:n.id,volume_l:n.volume,volume_r:n.volume});}
    if(eq&&n.eq){const b={id:n.id};n.eq.forEach((g,i)=>b['eq'+i]=g);await api('POST','/api/eq',b);}
    k++;}
  let extra='';
  if(eq&&!eqOn&&$('ck_enable').checked){const r=await api('POST','/api/dsp',{eq:true});if(r.ok){eqOn=true;extra=' EQ activada.';}}
  const what=[lat?'latencia':'',vol?'volumen':'',eq?'EQ':''].filter(Boolean).join(', ');
  log('Aplicado a '+k+' altavoz(ces): '+what+'.'+extra);return {k,what,extra};
}
async function resetAll(){for(let i=0;i<10;i++){await api('POST','/api/calibrate',{esp_id:i,latency_us:0});await api('POST','/api/node',{id:i,volume_l:255,volume_r:255});await api('POST','/api/eq',{id:i,reset:true});}
  log('Restablecido: latencia 0, volumen 100% y EQ plano en todos los altavoces');}

$('go').onclick=runCal;
$('cancel').onclick=()=>{stopCal();};
$('again').onclick=()=>{view(1);};
$('apply').onclick=async()=>{
  const lat=$('ck_lat').checked,vol=$('ck_vol').checked,eq=$('ck_eq').checked;
  if(!lat&&!vol&&!eq){$('applied').textContent='';toast('Marca al menos una opción',false);return;}
  $('apply').disabled=true;const r=await apply(lat,vol,eq);$('apply').disabled=false;
  $('applied').textContent='Aplicado a '+r.k+' altavoz(ces): '+r.what+'.'+r.extra;toast('Aplicado');render();
};
$('reset').onclick=async()=>{if(confirm('¿Poner latencia a 0, volumen al 100% y EQ plano en todos los altavoces?')){await resetAll();toast('Restablecido');$('applied').textContent='Restablecido.';}};
view(1);
</script></body></html>)HTML";

static const char PAGE_BLE[] = R"HTML(<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#000000"><meta name="color-scheme" content="dark light">
<meta name="mobile-web-app-capable" content="yes"><meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="SonosWiFi"><meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<link rel="manifest" href="/manifest.webmanifest">
<link rel="icon" href="/icon.svg" type="image/svg+xml"><link rel="icon" href="/icons/icon-192.png" sizes="192x192" type="image/png">
<link rel="apple-touch-icon" href="/icons/icon-180.png">
<script>if('serviceWorker' in navigator&&window.isSecureContext)addEventListener('load',function(){navigator.serviceWorker.register('/sw.js').catch(function(){});});</script>
<script>/* Tema de la interfaz: «dark» (por defecto), «graphite», «light» o «auto» (sigue al sistema; resuelve a dark o light). Se guarda en localStorage (por navegador).
   Se ejecuta en el <head> (antes de pintar) para que no haya destello; los botones [data-theme-btn] rotan Oscuro → Grafito → Claro → Automático. */
(function () {
  var KEY = 'sonos_theme', ORDER = ['dark', 'graphite', 'light', 'auto'], NAME = { dark: 'Oscuro', graphite: 'Grafito', light: 'Claro', auto: 'Automático' };
  var META = { dark: '#000000', graphite: '#1c1c1e', light: '#d5d9e0' };
  var ICON = {
    dark: '<path d="M20 14.5A8 8 0 0 1 9.5 4 8 8 0 1 0 20 14.5z"/>',
    graphite: '<rect x="3.5" y="5" width="17" height="14" rx="3"/><path d="M3.5 14h17" /><path d="M3.5 14v2a3 3 0 0 0 3 3h11a3 3 0 0 0 3-3v-2z" fill="currentColor"/>',
    light: '<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/>',
    auto: '<circle cx="12" cy="12" r="8.5"/><path d="M12 3.5v17a8.5 8.5 0 0 0 0-17z" fill="currentColor"/>'
  };
  var mq = window.matchMedia ? matchMedia('(prefers-color-scheme: light)') : null;
  function pref() { var p = 'dark'; try { p = localStorage.getItem(KEY) || 'dark'; } catch (e) {} return ORDER.indexOf(p) < 0 ? 'dark' : p; }
  function apply() {
    var p = pref(), t = p === 'auto' ? (mq && mq.matches ? 'light' : 'dark') : p;
    document.documentElement.setAttribute('data-theme', t);
    var m = document.querySelector('meta[name=theme-color]'); if (m) m.setAttribute('content', META[t]);
    var bs = document.querySelectorAll('[data-theme-btn]');
    for (var i = 0; i < bs.length; i++) {
      var b = bs[i];
      b.innerHTML = '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + ICON[p] + '</svg><span class="tl">' + NAME[p] + '</span>';
      b.setAttribute('aria-label', 'Tema: ' + NAME[p] + '. Pulsa para cambiar');
      b.title = 'Tema: ' + NAME[p];
    }
  }
  function set(p) { try { localStorage.setItem(KEY, p); } catch (e) {} apply(); }
  function next() { set(ORDER[(ORDER.indexOf(pref()) + 1) % ORDER.length]); }
  window.SonosTheme = { get: pref, set: set, next: next, apply: apply };
  apply();
  if (mq && mq.addEventListener) mq.addEventListener('change', apply);
  addEventListener('storage', function (e) { if (e.key === KEY) apply(); });
  document.addEventListener('DOMContentLoaded', function () {
    var bs = document.querySelectorAll('[data-theme-btn]');
    for (var i = 0; i < bs.length; i++) bs[i].addEventListener('click', next);
    apply();
  });
})();</script>
<title>SonosWiFi — Añadir altavoz</title>
<style>
/* Sistema de diseño compartido por las tres páginas (panel, /calibrate, /ble). Negro puro + azul señal. */
:root{
  --r-card:24px; --r-sheet:28px;
  --bg:#000; --panel:#0a0d13; --raise:#121722; --line:#1f2735; --line2:#2b3548;
  --fg:#e9edf5; --mut:#8a93a6; --ac:#4aa3ff; --ac-bg:rgba(74,163,255,.13); --ac-ln:rgba(74,163,255,.5);
  --ctl:#6b7a96;                                   /* borde de controles (campos, interruptor apagado): ≥3:1 sobre las superficies */
  --ok:#3ecf8e; --warn:#f5b84a; --bad:#ff6b6b;
  --on-ac:#04121f; --ac-hi:#9ad0ff; --ac-lo:#2f6fb3; --bad-fg:#ffb0b0; --bad-ln:rgba(255,107,107,.45); --warn-ln:rgba(245,184,74,.4);
  --top-bg:rgba(0,0,0,.88); --top-solid:#000; --tabs-bg:rgba(10,13,19,.94); --scrim:rgba(0,0,0,.55); --code-bg:#05070b; --bar-bg:#000; --cl-off:#3a1512;
  --thumb:#fff; --thumb-ring:rgba(74,163,255,.38); --cap-bg:rgba(0,0,0,.65); --glow:rgba(74,163,255,.09); --orb-glow:rgba(74,163,255,.35);
  --led-g:#30d158; --led-y:#ffd60a; --led-r:#ff453a;
  color-scheme:dark;
}
/* Temas alternativos: data-theme="graphite" (gris grafito, de bajo brillo) y "light" (claro suave, sin blanco puro).
   El script del <head> (theme.js) resuelve «Automático» según el sistema. */
html[data-theme=graphite]{
  --bg:#1c1c1e; --panel:#2b2b2d; --raise:#363639; --line:#3d3d41; --line2:#4c4c51;
  --fg:#e9e9ec; --mut:#a3a3aa; --ac:#5aaeff; --ac-bg:rgba(90,174,255,.15); --ac-ln:rgba(90,174,255,.55);
  --ctl:#8a8a91; --ok:#46d39a; --warn:#f0b64a; --bad:#ff7b72;
  --on-ac:#06121f; --ac-hi:#a5d3ff; --ac-lo:#3478b8; --bad-fg:#ffb4ae; --bad-ln:rgba(255,123,114,.5); --warn-ln:rgba(240,182,74,.45);
  --top-bg:rgba(28,28,30,.9); --top-solid:#1c1c1e; --tabs-bg:rgba(43,43,45,.95); --scrim:rgba(0,0,0,.5); --code-bg:#1f1f21; --bar-bg:#18181a; --cl-off:#4a2a28;
  --thumb:#f2f2f4; --thumb-ring:rgba(90,174,255,.4); --cap-bg:rgba(28,28,30,.78); --glow:rgba(90,174,255,.07); --orb-glow:rgba(90,174,255,.35);
  --led-g:#34c759; --led-y:#ffd60a; --led-r:#ff5a50;
  color-scheme:dark;
}
html[data-theme=light]{
  --bg:#d5d9e0; --panel:#e4e7ec; --raise:#d9dde4; --line:#c3c8d1; --line2:#aab1bd;
  --fg:#1a1f27; --mut:#4b5465; --ac:#0a58ac; --ac-bg:rgba(10,88,172,.11); --ac-ln:rgba(10,88,172,.45);
  --ctl:#667184; --ok:#0f6240; --warn:#7a4a00; --bad:#a82620;
  --on-ac:#ffffff; --ac-hi:#4aa3ff; --ac-lo:#7fa9d6; --bad-fg:#8f1a16; --bad-ln:rgba(179,42,37,.5); --warn-ln:rgba(138,83,0,.45);
  --top-bg:rgba(213,217,224,.92); --top-solid:#d5d9e0; --tabs-bg:rgba(228,231,236,.95); --scrim:rgba(26,31,39,.35); --code-bg:#cfd4dc; --bar-bg:#0b0e14; --cl-off:#e0c3c1;
  --thumb:#f4f5f8; --thumb-ring:rgba(10,88,172,.35); --cap-bg:rgba(228,231,236,.88); --glow:rgba(10,88,172,.06); --orb-glow:rgba(10,88,172,.3);
  --led-g:#1fa84a; --led-y:#d9a400; --led-r:#e0342b;
  color-scheme:light;
}
*{box-sizing:border-box;margin:0}
html{font-size:106.25%}          /* 17 px: tamaño de texto por defecto de iOS; todo lo demás va en rem y sigue el ajuste del navegador */
body{background:var(--bg);color:var(--fg);font:1rem/1.45 var(--font);-webkit-font-smoothing:antialiased;
  font-variant-numeric:tabular-nums;min-height:100vh;-webkit-text-size-adjust:100%;
  padding-left:env(safe-area-inset-left);padding-right:env(safe-area-inset-right)}
a{color:var(--ac);text-decoration:none}a:hover{text-decoration:underline}
button,input,select{font:inherit;color:inherit}
button{cursor:pointer}
[hidden]{display:none!important}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px;border-radius:6px}
h1{font-size:1.55rem;font-weight:650;letter-spacing:-.01em}
h2{font-size:1.02rem;font-weight:650}
.mut{color:var(--mut)}
.small{font-size:.86rem}

.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;text-decoration:none;line-height:1.2;font-family:inherit;appearance:none;border:0;background:var(--ac);color:var(--on-ac);font-weight:650;padding:10px 22px;border-radius:999px;min-height:44px;box-shadow:0 8px 22px -10px var(--orb-glow);
  transition:filter .12s,transform .06s,background .12s}
.btn:hover{filter:brightness(1.1);text-decoration:none}.btn:active{transform:translateY(1px)}
.btn.ghost{background:var(--raise);color:var(--fg);border:1px solid var(--line2);font-weight:600;box-shadow:none}
.btn.ghost:hover{background:var(--line);filter:none}
.btn.danger{background:transparent;color:var(--bad);border:1px solid var(--bad-ln);font-weight:600;box-shadow:none}
.btn.sm{padding:6px 16px;min-height:36px;font-size:.88rem}
.btn:disabled{opacity:.4;cursor:not-allowed;filter:none}

.seg{display:inline-flex;background:var(--code-bg);border:1px solid var(--line);border-radius:999px;padding:4px;gap:2px;max-width:100%}
.seg button{appearance:none;border:0;background:transparent;color:var(--mut);padding:7px 16px;border-radius:999px;min-height:36px;font-weight:600;white-space:nowrap;display:inline-flex;align-items:center;justify-content:center;gap:8px;transition:background .12s,color .12s}
.seg button:hover{color:var(--fg)}
.seg button[aria-pressed=true]{background:var(--ac-bg);color:var(--ac);box-shadow:inset 0 0 0 1px var(--ac-ln)}
.seg button[aria-pressed=true]::before{content:"";width:7px;height:7px;border-radius:50%;background:currentColor;flex:none}
.seg.block{display:flex}.seg.block button{flex:1;min-width:0;white-space:normal;line-height:1.2;padding-left:8px;padding-right:8px}   /* con texto grande o pantalla estrecha, las etiquetas bajan de línea en vez de salirse */

.switch{appearance:none;position:relative;width:52px;height:32px;border-radius:16px;border:1px solid var(--ctl);background:var(--raise);flex:none;
  transition:background .15s,border-color .15s}
.switch::after{content:"";position:absolute;top:3px;left:3px;width:24px;height:24px;border-radius:50%;background:var(--mut);transition:transform .15s,background .15s}
.switch[aria-checked=true]{background:var(--ac);border-color:var(--ac)}
.switch[aria-checked=true]::after{transform:translateX(20px);background:var(--on-ac)}

input[type=range].rng{-webkit-appearance:none;appearance:none;width:100%;height:28px;background:transparent;border-radius:0;cursor:pointer;
  --f:.5;--rh:28px;height:var(--rh);
  background:linear-gradient(to right,var(--ac) calc(var(--f)*(100% - 38px) + 19px),var(--line2) 0) no-repeat center/100% 10px}
input[type=range].rng.mid{--lo:.5;--hi:.5;
  background:linear-gradient(to right,var(--line2) calc(var(--lo)*(100% - 38px) + 19px),var(--ac) 0,var(--ac) calc(var(--hi)*(100% - 38px) + 19px),var(--line2) 0) no-repeat center/100% 10px}
.rng::-webkit-slider-runnable-track{height:var(--rh);background:transparent}
.rng::-moz-range-track{height:var(--rh);background:transparent}
.rng::-webkit-slider-thumb{-webkit-appearance:none;width:38px;height:26px;border-radius:13px;background:var(--thumb);border:0;margin-top:calc((var(--rh) - 26px)/2);box-shadow:0 2px 8px rgba(0,0,0,.35),0 0 0 3px var(--thumb-ring)}
.rng::-moz-range-thumb{width:38px;height:26px;border-radius:13px;background:var(--thumb);border:0;box-shadow:0 2px 8px rgba(0,0,0,.35),0 0 0 3px var(--thumb-ring)}
.rng:focus-visible{outline-offset:0}

.num,.txt,.sel{background:var(--raise);border:1px solid var(--ctl);border-radius:14px;padding:9px 14px;min-height:44px;width:100%;color:var(--fg)}
.num{width:110px}
.num:focus,.txt:focus,.sel:focus{outline:2px solid var(--ac);outline-offset:1px;border-color:transparent}
.sel{appearance:none;background-image:linear-gradient(45deg,transparent 50%,var(--mut) 50%),linear-gradient(135deg,var(--mut) 50%,transparent 50%);
  background-position:calc(100% - 21px) 19px,calc(100% - 16px) 19px;background-size:5px 5px,5px 5px;background-repeat:no-repeat;padding-right:32px}
label,.lbl{display:block;color:var(--mut);font-size:.74rem;font-weight:600;letter-spacing:.07em;text-transform:uppercase;margin-bottom:7px}

.chip{display:inline-flex;align-items:center;gap:7px;padding:5px 13px;border-radius:999px;background:var(--raise);border:1px solid var(--line);font-size:.84rem;color:var(--mut)}
.chip.ac{color:var(--ac);border-color:var(--ac-ln);background:var(--ac-bg)}
.chip.warn{color:var(--warn);border-color:var(--warn-ln)}
.chip.bad{color:var(--bad);border-color:var(--bad-ln)}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--mut);flex:none}
.dot.ok{background:var(--ok)}.dot.bad{background:var(--bad)}.dot.warn{background:var(--warn)}

.top{position:sticky;top:0;z-index:30;background:var(--top-bg);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);border-bottom:1px solid var(--line);
  padding-top:env(safe-area-inset-top)}               /* en la app instalada (barra de estado translúcida) el contenido no queda bajo la muesca */
.top .in{max-width:1180px;margin:auto;display:flex;align-items:center;gap:18px;padding:10px 18px;flex-wrap:wrap}
.brand{display:flex;align-items:center;gap:10px;font-weight:700;font-size:1.05rem;color:var(--fg)}
.brand:hover{text-decoration:none}
.logo{width:26px;height:26px;flex:none;color:var(--ac)}
.panelbox{background:var(--panel);border:1px solid var(--line);border-radius:var(--r-card);padding:20px}
.note{border-radius:16px;padding:12px 14px;background:var(--raise);border:1px solid var(--line2);font-size:.92rem}
.note.bad{border-color:var(--bad-ln);color:var(--bad-fg)}.note.warn{border-color:var(--warn-ln)}

.toasts{position:fixed;left:50%;bottom:calc(18px + env(safe-area-inset-bottom));transform:translateX(-50%);display:flex;flex-direction:column;gap:6px;z-index:60;pointer-events:none;width:max-content;max-width:92vw}
.toast{pointer-events:auto;display:flex;align-items:center;background:var(--raise);border:1px solid var(--line2);padding:10px 16px;border-radius:999px;font-size:.9rem;
  animation:tin .18s ease-out}
.toast::before{content:"";flex:none;width:9px;height:9px;border-radius:50%;background:var(--ok);margin-right:10px}
.toast.bad::before{background:var(--bad)}
@keyframes tin{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
.procbar{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin:0 0 14px;padding:10px 18px;border-radius:999px;background:var(--ac-bg);border:1px solid var(--ac-ln);color:var(--ac);font-size:.92rem}
.procbar[hidden]{display:none}

/* Objetivos táctiles: 44 × 44 (mínimo cómodo de iOS) cuando el puntero es el dedo */
@media (pointer:coarse){
  .btn,.btn.sm,.seg button,.tab,.num,.txt,.sel{min-height:44px}
  input[type=range].rng{--rh:44px}
  .switch::before{content:"";position:absolute;inset:-9px -6px}   /* zona de toque ampliada; el aspecto no cambia */
}
.themebtn{display:inline-flex;align-items:center;gap:6px}
.themebtn svg{flex:none}
@media (max-width:760px){.themebtn .tl{display:none}.themebtn{padding-left:10px;padding-right:10px}}
/* Ajustes de accesibilidad del sistema */
@media (prefers-contrast:more){
  :root{--mut:#b4bccc;--ctl:#8d9ab3;--line2:#6b7a96;--ac:#7ec0ff;--ac-ln:rgba(126,192,255,.8)}
  html[data-theme=light]{--mut:#323b4a;--ctl:#4a5468;--line2:#7d8798;--ac:#0a4f99;--ac-ln:rgba(10,79,153,.8)}
  html[data-theme=graphite]{--mut:#c6c6cc;--ctl:#a8a8af;--line2:#7d7d84;--ac:#8cc6ff;--ac-ln:rgba(140,198,255,.8)}
}
@media (prefers-reduced-transparency:reduce){
  .top{background:var(--top-solid);backdrop-filter:none;-webkit-backdrop-filter:none}
}
main{max-width:600px;margin:0 auto;padding:20px 16px 80px;display:flex;flex-direction:column;gap:16px}
.top .in{max-width:600px}
.step{display:flex;gap:14px}
.num2{flex:none;width:30px;height:30px;border-radius:50%;border:1.5px solid var(--line2);display:grid;place-items:center;font-weight:700;color:var(--mut)}
.step.cur .num2{border-color:var(--ac);color:var(--ac)}
.step.done .num2{background:var(--ac);border-color:var(--ac);color:var(--on-ac)}
.step .bd{flex:1;min-width:0;display:flex;flex-direction:column;gap:12px}
.step.lock .bd{opacity:.4;pointer-events:none}
.pills{display:grid;grid-template-columns:repeat(5,1fr);gap:8px}
.pills button{appearance:none;background:var(--raise);border:1px solid var(--line2);border-radius:999px;min-height:46px;font-weight:650;font-size:1.05rem}
.pills button:hover{border-color:var(--ac-ln)}
.pills button[aria-pressed=true]{background:var(--ac-bg);border-color:var(--ac);color:var(--ac)}
.kv{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.kv div{background:var(--raise);border:1px solid var(--line);border-radius:12px;padding:10px 12px}
.kv span{display:block;font-size:.8rem;color:var(--mut)}.kv b{font-weight:600}
.pw{position:relative}.pw button{position:absolute;right:6px;top:5px}
details.adv{border:1px solid var(--line);border-radius:var(--r-card);padding:4px 14px;background:var(--panel)}
details.adv summary{cursor:pointer;padding:10px 0;font-weight:600;color:var(--mut)}
details.adv[open] summary{color:var(--fg)}
details.adv .in2{padding:4px 0 14px;display:flex;flex-direction:column;gap:12px}
.row{display:flex;gap:8px;flex-wrap:wrap}
.log{font:.82rem/1.6 ui-monospace,Consolas,monospace;color:var(--mut);white-space:pre-wrap;max-height:150px;overflow:auto}
@media (max-width:420px){.kv{grid-template-columns:1fr}}
</style></head><body>
<div class="top"><div class="in">
  <a class="brand" href="/"><svg class="logo" viewBox="0 0 26 26" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M3 13h2M8 8v10M13 4v18M18 8v10M23 13h-2"/></svg>SonosWiFi</a>
  <span style="margin-left:auto;display:flex;gap:6px"><button class="btn ghost sm themebtn" data-theme-btn type="button"></button><a class="btn ghost sm" href="/">Volver al panel</a></span>
</div></div>
<main>
  <div><h1>Añadir un altavoz</h1><p class="mut">Conecta el ESP32 por Bluetooth y dile a qué WiFi unirse. Solo hace falta una vez.</p></div>

  <div class="note bad" id="nosup" hidden><b>Este navegador no admite Web Bluetooth.</b><br>Usa Chrome o Edge en Android o en un ordenador, o la app Bluefy en iPhone.</div>
  <div class="note warn" id="nohttps" hidden><b>Falta HTTPS.</b> Abre <a id="httpslink" href="#">esta página con HTTPS</a> y acepta el aviso del certificado.</div>

  <div class="panelbox step cur" id="s1">
    <div class="num2">1</div>
    <div class="bd">
      <h2>Conecta el ESP32</h2>
      <p class="mut small">Enciéndelo cerca del móvil. Aparece como «SonosWiFi-XXXX».</p>
      <div class="row"><button class="btn" id="bc">Buscar altavoz</button><button class="btn ghost" id="bd" disabled>Desconectar</button></div>
      <div id="kv" class="kv" hidden>
        <div><span>Estado</span><b id="k_wifi">—</b></div><div><span>Señal</span><b id="k_rssi">—</b></div>
        <div><span>Dirección IP</span><b id="k_ip">—</b></div><div><span>Con la Raspberry</span><b id="k_rpi">—</b></div>
      </div>
    </div>
  </div>

  <div class="panelbox step lock" id="s2">
    <div class="num2">2</div>
    <div class="bd">
      <h2>Elige su número</h2>
      <p class="mut small">Cada altavoz necesita un número distinto del 0 al 9. Anótalo en el ESP32.</p>
      <div class="pills" id="pills" role="group" aria-label="Número del altavoz"></div>
    </div>
  </div>

  <div class="panelbox step lock" id="s3">
    <div class="num2">3</div>
    <div class="bd">
      <h2>Indica la WiFi</h2>
      <div><label for="ssid">Nombre de la red (mejor de 5 GHz)</label><input class="txt" id="ssid" maxlength="32" autocapitalize="off" autocomplete="off"></div>
      <div><label for="pass">Contraseña (vacía = mantener la actual)</label>
        <div class="pw"><input class="txt" id="pass" type="password" maxlength="63" autocomplete="new-password"><button class="btn ghost sm" id="eye" type="button">Ver</button></div></div>
      <div class="row"><button class="btn" id="save">Guardar y reiniciar</button><button class="btn ghost" id="ident">Hacer sonar un pitido</button></div>
      <div class="savestate small mut" id="res"></div>
    </div>
  </div>

  <details class="adv" id="adv">
    <summary>Opciones avanzadas</summary>
    <div class="in2">
      <div><label for="rip">IP de la Raspberry (solo si el router bloquea el broadcast; vacío = automática)</label>
        <input class="txt" id="rip" maxlength="15" inputmode="decimal" autocomplete="off" placeholder="192.168.1.10"></div>
      <div class="row"><button class="btn ghost" id="reboot">Reiniciar</button><button class="btn danger" id="factory">Borrar configuración</button></div>
      <p class="mut small">Borrar configuración elimina la WiFi y el número guardados en este ESP32.</p>
    </div>
  </details>

  <div class="panelbox"><h2 style="margin-bottom:8px">Actividad</h2><div class="log" id="log" role="log"></div></div>
</main>
<div class="toasts" id="tb"></div>
<script>
const U=n=>'5c0a'+n.toString(16).padStart(4,'0')+'-8e3b-4d5e-9a41-534f4e4f5346';
const SVC=U(1),C_SSID=U(2),C_PASS=U(3),C_ID=U(4),C_ST=U(5),C_CMD=U(6),C_RPI=U(7);
const $=i=>document.getElementById(i),enc=new TextEncoder(),dec=new TextDecoder();
const log=m=>{const l=$('log');l.textContent+=new Date().toLocaleTimeString()+'  '+m+'\n';l.scrollTop=l.scrollHeight;};
function toast(m,ok=true){const d=document.createElement('div');d.className='toast'+(ok?'':' bad');d.textContent=m;$('tb').appendChild(d);setTimeout(()=>d.remove(),2600);}
let dev=null,ch={},curId=0,connected=false;
if(!navigator.bluetooth)$('nosup').hidden=false;
if(!window.isSecureContext){$('nohttps').hidden=false;$('httpslink').href='https://'+location.hostname+':8443/ble';}
$('pills').innerHTML=[0,1,2,3,4,5,6,7,8,9].map(i=>'<button data-i="'+i+'" aria-pressed="'+(i===0)+'">'+i+'</button>').join('');
function setId(i){curId=i;$('pills').querySelectorAll('button').forEach(b=>b.setAttribute('aria-pressed',+b.dataset.i===i));}
$('pills').onclick=e=>{const b=e.target.closest('button');if(b)setId(+b.dataset.i);};
function stage(on){connected=on;$('s1').className='panelbox step '+(on?'done':'cur');
  $('s2').className='panelbox step '+(on?'cur':'lock');$('s3').className='panelbox step '+(on?'cur':'lock');
  $('adv').style.opacity=on?1:.5;$('adv').style.pointerEvents=on?'auto':'none';
  $('bc').disabled=on;$('bd').disabled=!on;$('kv').hidden=!on;$('s1').querySelector('.num2').textContent=on?'✓':'1';}
stage(false);
function showStatus(t){let j=null;try{j=JSON.parse(t);}catch(e){}if(!j)return;
  $('k_wifi').textContent=j.wifi?'Conectado a la WiFi':'Sin WiFi';
  $('k_rssi').textContent=j.wifi?(j.rssi>-60?'Buena':j.rssi>-72?'Aceptable':'Débil')+' ('+j.rssi+' dBm)':'—';
  $('k_ip').textContent=j.wifi&&j.ip?j.ip:'—';
  $('k_rpi').textContent=j.rpi?(j.sync?'Sincronizado':'Conectado'):'Buscando…';}
async function readStatus(){try{showStatus(dec.decode(await ch.st.readValue()));}catch(e){}}
const wr=(c,b)=>c.writeValueWithResponse?c.writeValueWithResponse(b):c.writeValue(b);
$('bc').onclick=async()=>{
  try{
    if(!window.isSecureContext){log('Web Bluetooth exige HTTPS.');return;}
    dev=await navigator.bluetooth.requestDevice({filters:[{services:[SVC]}]});
    dev.addEventListener('gattserverdisconnected',()=>{log('Desconectado');stage(false);});
    log('Conectando a '+dev.name+'…');$('bc').disabled=true;
    const s=await dev.gatt.connect(),svc=await s.getPrimaryService(SVC);
    ch.ssid=await svc.getCharacteristic(C_SSID);ch.pass=await svc.getCharacteristic(C_PASS);
    ch.id=await svc.getCharacteristic(C_ID);ch.st=await svc.getCharacteristic(C_ST);ch.cmd=await svc.getCharacteristic(C_CMD);
    try{ch.rpi=await svc.getCharacteristic(C_RPI);$('rip').value=dec.decode(await ch.rpi.readValue());}catch(e){ch.rpi=null;}
    try{$('ssid').value=dec.decode(await ch.ssid.readValue());setId((await ch.id.readValue()).getUint8(0));}catch(e){}
    await ch.st.startNotifications();ch.st.addEventListener('characteristicvaluechanged',readStatus);await readStatus();
    stage(true);log('Conectado a '+dev.name);
  }catch(e){log('Error: '+e.name+': '+e.message);$('bc').disabled=false;if(e.name!=='NotFoundError')toast('No se pudo conectar',false);}
};
$('bd').onclick=()=>{if(dev&&dev.gatt.connected)dev.gatt.disconnect();};
async function cmd(c){try{await wr(ch.cmd,enc.encode(c));log('Comando: '+c);return true;}catch(e){log('Error: '+e.message);toast('El ESP32 no respondió',false);return false;}}
$('ident').onclick=()=>cmd('identify');
$('reboot').onclick=()=>cmd('reboot');
$('factory').onclick=async()=>{if(confirm('¿Borrar WiFi y número guardados en este ESP32?'))if(await cmd('factory'))toast('Configuración borrada');};
$('eye').onclick=()=>{const p=$('pass');const s=p.type==='password';p.type=s?'text':'password';$('eye').textContent=s?'Ocultar':'Ver';};
$('save').onclick=async()=>{
  const ssid=$('ssid').value,pass=$('pass').value;
  if(!ssid){$('res').textContent='Escribe el nombre de la WiFi.';return;}
  if(pass&&(pass.length<8||pass.length>63)){$('res').textContent='La contraseña debe tener entre 8 y 63 caracteres.';return;}
  try{
    $('save').disabled=true;$('res').textContent='Enviando…';
    await wr(ch.id,new Uint8Array([curId]));
    await wr(ch.ssid,enc.encode(ssid));
    if(ch.rpi)await wr(ch.rpi,enc.encode($('rip').value.trim()));
    if(pass)await wr(ch.pass,enc.encode(pass));
    log('Valores enviados. Aplicando…');
    if(await cmd('apply')){$('res').textContent='Listo. El altavoz #'+curId+' se reinicia y se une a «'+ssid+'». Aparecerá en el panel en unos segundos.';toast('Guardado');}
  }catch(e){log('Error: '+e.name+': '+e.message);$('res').textContent='No se pudo guardar. Si el móvil pide emparejar, acepta.';}
  $('save').disabled=false;
};
</script></body></html>)HTML";

static const char PWA_MANIFEST[] = R"HTML({
  "id": "/",
  "name": "SonosWiFi",
  "short_name": "SonosWiFi",
  "description": "Control del audio multicanal inalámbrico: altavoces, sonido y latencia.",
  "lang": "es",
  "dir": "ltr",
  "start_url": "/",
  "scope": "/",
  "display": "standalone",
  "orientation": "any",
  "background_color": "#000000",
  "theme_color": "#000000",
  "icons": [
    { "src": "/icons/icon-192.png", "sizes": "192x192", "type": "image/png", "purpose": "any" },
    { "src": "/icons/icon-512.png", "sizes": "512x512", "type": "image/png", "purpose": "any" },
    { "src": "/icons/icon-maskable-512.png", "sizes": "512x512", "type": "image/png", "purpose": "maskable" }
  ],
  "shortcuts": [
    { "name": "Sonido", "short_name": "Sonido", "url": "/#snd", "icons": [{ "src": "/icons/icon-192.png", "sizes": "192x192", "type": "image/png" }] },
    { "name": "Latencia", "short_name": "Latencia", "url": "/#lat", "icons": [{ "src": "/icons/icon-192.png", "sizes": "192x192", "type": "image/png" }] }
  ]
})HTML";

static const char PWA_SW[] = R"HTML(/* Service worker de SonosWiFi. Solo se registra en contexto seguro (HTTPS o localhost).
 * - /api/*: SIEMPRE a la red (el estado en vivo no se cachea).
 * - Páginas e iconos: red primero y, si no hay red, la última copia (la interfaz abre sin conexión y avisa de que no hay datos).
 * 4d5cbfe19f lo sustituye scripts/gen_web_pages.py por un hash de las páginas: cada cambio invalida la caché. */
const CACHE = 'sonoswifi-4d5cbfe19f';
const SHELL = ['/', '/calibrate', '/ble', '/manifest.webmanifest', '/icons/icon-192.png'];

self.addEventListener('install', e => {
  e.waitUntil(caches.open(CACHE)
    .then(c => Promise.all(SHELL.map(u => c.add(new Request(u, { credentials: 'same-origin' })).catch(() => 0))))
    .then(() => self.skipWaiting()));
});

self.addEventListener('activate', e => {
  e.waitUntil(caches.keys()
    .then(ks => Promise.all(ks.filter(k => k.startsWith('sonoswifi-') && k !== CACHE).map(k => caches.delete(k))))
    .then(() => self.clients.claim()));
});

self.addEventListener('fetch', e => {
  const r = e.request, u = new URL(r.url);
  if (r.method !== 'GET' || u.origin !== self.location.origin || u.pathname.startsWith('/api/')) return;   // no se intercepta
  e.respondWith(fetch(r).then(res => {
    if (res.ok) { const copy = res.clone(); caches.open(CACHE).then(c => c.put(r, copy)).catch(() => 0); }
    return res;
  }).catch(() => caches.match(r).then(hit => hit || (r.mode === 'navigate' ? caches.match('/') : undefined) || Response.error())));
});)HTML";

static const char PWA_ICON_SVG[] = R"HTML(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" role="img" aria-label="SonosWiFi">
  <rect width="512" height="512" fill="#000"/>
  <g fill="#4aa3ff">
    <rect x="78"  y="211" width="44" height="90"  rx="22"/>
    <rect x="156" y="161" width="44" height="190" rx="22"/>
    <rect x="234" y="96"  width="44" height="320" rx="22"/>
    <rect x="312" y="161" width="44" height="190" rx="22"/>
    <rect x="390" y="211" width="44" height="90"  rx="22"/>
  </g>
</svg>)HTML";

static const unsigned char PWA_ICON_180[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,180,0,0,0,180,8,2,0,0,0,178,175,145,101,0,0,8,25,73,68,65,84,120,156,236,221,107,108,83,101,28,199,241,167,187,117,99,45,219,216,152,110,221,6,97,19,38,93,17,140,81,32,68,197,104,140,23,212,24,149,121,33,162,243,130,168,111,80,81,81,95,152,160,137,70,141,38,222,230,37,98,52,110,106,98,148,121,35,49,72,50,197,73,184,108,172,195,41,227,178,107,89,25,108,93,187,110,93,233,234,95,140,72,24,255,246,57,208,179,246,217,249,125,66,120,209,254,147,157,243,236,219,203,78,219,211,20,1,192,72,17,0,12,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,1,44,196,241,63,107,81,5,253,239,237,114,10,56,14,113,136,146,43,30,156,113,229,35,214,226,138,19,151,120,59,157,237,63,189,217,177,185,90,24,155,73,24,88,218,212,233,142,170,15,207,185,112,217,105,175,237,221,89,215,84,189,226,152,223,35,140,202,192,113,152,76,75,214,55,78,45,153,23,97,100,176,163,233,151,103,23,136,112,88,24,82,178,48,170,89,203,158,178,45,190,51,242,140,57,235,220,80,112,164,255,175,95,132,33,25,52,14,107,177,99,193,234,26,83,82,244,221,159,54,231,210,222,157,27,71,7,123,133,241,36,9,67,42,186,172,42,41,37,77,102,146,198,10,23,223,33,12,201,160,127,173,208,227,133,78,195,147,9,226,136,241,240,100,98,208,56,194,161,160,252,176,204,83,147,73,9,7,193,128,133,56,128,133,56,128,133,56,128,133,56,128,133,56,128,133,56,128,133,56,128,133,56,128,133,56,128,133,56,128,133,56,128,133,56,128,165,100,28,153,133,229,86,155,125,74,126,233,240,145,14,95,119,139,183,179,89,36,30,107,177,195,98,179,103,228,150,248,221,251,188,221,45,67,61,173,66,53,138,197,145,93,182,104,254,67,159,78,201,159,117,242,133,193,161,1,231,134,85,174,134,207,69,98,40,88,184,188,98,229,187,169,153,217,39,95,232,119,239,111,124,231,174,129,182,223,132,58,148,121,155,160,41,57,117,246,173,47,44,122,174,254,148,50,8,253,26,22,60,92,59,127,117,77,178,57,83,196,85,202,148,44,218,12,218,152,83,202,32,180,217,180,241,179,111,89,79,59,34,20,161,76,28,197,151,85,149,221,176,46,194,251,110,10,23,85,150,221,244,156,136,171,243,110,126,158,54,131,187,150,54,190,236,198,103,104,71,132,34,212,136,131,110,118,229,183,191,18,117,108,214,181,79,228,156,183,88,196,9,61,228,205,188,234,209,168,99,180,35,227,239,252,18,147,26,113,20,47,125,32,37,61,250,67,134,41,41,105,214,117,107,69,156,148,94,255,36,109,64,212,49,218,17,218,29,161,2,53,158,144,102,151,46,148,157,44,147,157,140,49,147,41,103,246,18,201,89,249,221,137,47,5,238,57,146,82,205,217,165,151,72,14,155,179,206,201,152,62,83,76,184,204,130,57,105,214,92,201,225,236,210,139,147,82,211,69,194,83,32,14,122,44,79,78,211,176,148,185,229,151,139,9,151,163,229,206,32,57,45,99,234,140,249,34,225,41,240,176,18,240,28,210,52,31,244,15,136,9,167,245,135,198,101,35,181,194,225,115,96,33,14,96,33,14,96,33,14,96,33,14,96,33,14,96,33,14,96,33,14,96,233,27,7,189,72,157,231,184,218,90,50,207,106,179,167,100,100,249,122,246,248,186,90,14,239,254,97,212,219,39,64,139,52,107,222,63,43,105,179,91,108,115,67,163,126,111,151,115,176,189,177,175,121,83,120,44,36,116,163,99,28,244,194,244,5,171,62,57,249,53,244,127,207,248,25,240,184,155,63,188,207,189,171,78,128,156,252,5,203,28,85,31,152,179,242,79,185,188,127,239,214,166,119,87,248,221,251,133,62,244,122,109,165,248,242,251,150,188,208,116,218,119,87,208,78,94,180,102,99,197,189,239,41,241,226,83,124,37,167,91,28,85,239,211,114,141,47,131,208,242,210,34,23,44,172,20,250,208,37,142,212,204,156,242,202,151,83,210,45,17,102,74,150,222,159,59,119,169,128,136,242,236,87,210,205,44,194,0,45,242,249,119,188,154,106,153,38,116,160,75,28,246,149,239,80,31,81,199,232,174,82,167,189,154,28,104,13,237,43,223,142,58,150,158,83,104,191,59,250,216,25,136,125,28,230,172,115,11,23,46,151,153,164,189,42,184,248,54,1,140,130,75,150,167,103,23,200,76,210,130,235,113,202,195,216,199,161,233,157,10,74,188,173,33,94,226,190,146,177,143,195,170,105,151,74,16,7,75,211,226,88,117,136,35,246,127,202,166,231,216,228,135,205,211,52,12,27,141,166,197,209,180,236,146,112,132,20,88,136,3,88,136,3,88,136,3,88,136,3,88,136,3,88,136,3,88,136,3,88,136,3,88,136,3,88,136,3,88,136,3,88,136,3,88,136,3,88,136,3,88,177,127,179,207,72,127,183,252,112,224,168,134,97,163,209,180,56,154,150,93,82,236,227,240,182,55,202,15,15,118,104,24,54,26,77,139,163,105,217,37,197,254,97,101,80,83,28,237,136,131,21,247,149,140,253,61,71,192,115,168,71,238,52,228,35,3,46,215,239,137,114,194,242,4,228,218,246,197,72,127,143,204,36,45,184,214,51,167,201,208,229,115,43,45,31,175,30,245,29,141,58,182,251,189,123,130,67,253,2,24,65,223,209,150,143,31,142,58,70,183,177,150,13,15,9,29,232,18,135,204,94,117,108,174,238,107,222,36,32,162,222,29,95,71,189,27,214,239,54,166,215,103,101,93,13,181,91,30,43,237,223,187,117,252,85,1,143,123,251,107,55,56,63,90,37,64,66,227,91,149,180,92,180,104,227,175,162,229,165,69,214,239,54,166,227,113,14,191,123,127,195,250,75,243,28,87,79,157,49,223,90,84,145,108,182,224,20,12,103,198,189,171,174,254,105,251,244,121,215,88,138,236,150,194,185,161,128,79,249,83,48,16,218,244,195,77,223,211,63,1,103,135,110,78,221,191,126,34,38,22,142,144,2,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,75,129,239,120,27,238,107,15,143,141,201,207,251,92,113,248,214,120,77,63,148,118,103,248,240,65,145,240,20,136,99,108,116,120,168,119,175,228,112,136,134,93,127,138,9,71,63,52,20,240,75,14,123,187,154,199,130,35,34,225,169,241,165,195,242,111,115,242,118,236,22,225,176,152,120,225,176,183,179,89,114,118,160,173,65,168,64,141,56,14,252,248,218,216,177,81,153,201,182,186,23,69,156,236,251,238,101,153,177,177,99,193,206,45,31,8,21,168,17,135,103,223,182,214,218,39,163,142,29,216,244,134,123,231,70,17,39,189,219,191,234,216,92,29,117,172,181,118,173,231,192,118,161,2,53,226,32,7,55,189,222,231,252,41,194,128,183,123,79,107,205,227,34,174,254,248,108,141,183,211,25,97,160,119,199,55,180,35,66,17,202,196,65,182,189,116,213,214,231,23,143,255,6,137,224,208,192,174,183,42,235,159,178,135,67,199,68,92,209,115,210,250,117,14,218,24,218,164,83,174,162,205,166,141,223,241,250,77,66,29,138,29,231,24,104,251,109,203,99,165,153,133,229,86,155,125,74,126,233,240,145,14,95,119,139,252,51,193,137,225,106,248,156,254,89,139,29,22,155,61,35,183,196,239,222,231,237,110,25,234,137,195,31,216,103,73,201,131,96,180,208,137,191,214,148,108,162,85,171,21,142,144,2,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,11,113,0,75,165,55,251,196,144,231,224,14,157,134,39,19,131,222,115,224,100,169,50,12,122,207,225,110,252,54,48,120,88,102,114,212,219,71,195,194,144,12,26,71,104,196,247,103,237,90,153,201,214,154,39,104,88,24,146,65,227,32,93,245,27,6,218,126,143,60,115,100,207,207,52,38,140,42,89,24,88,239,174,111,50,11,202,45,5,115,78,127,237,206,186,166,234,21,242,31,114,156,124,12,29,7,253,226,93,13,53,1,207,161,140,188,153,230,172,252,19,151,123,59,157,127,124,182,230,175,47,215,25,185,12,98,18,240,31,107,81,133,248,231,83,206,78,1,199,33,14,96,225,8,41,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,16,7,176,254,6,0,0,255,255,38,60,246,51,0,0,0,6,73,68,65,84,3,0,118,209,99,217,197,162,153,61,0,0,0,0,73,69,78,68,174,66,96,130};

static const unsigned char PWA_ICON_192[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,192,0,0,0,192,8,2,0,0,0,221,190,251,80,0,0,7,206,73,68,65,84,120,156,236,221,107,108,84,101,26,192,241,119,232,149,14,208,153,161,165,165,96,101,41,180,83,2,195,174,100,89,22,109,65,19,113,49,187,11,187,49,49,236,234,154,93,212,238,77,118,99,98,52,36,198,47,70,33,104,162,38,138,38,168,212,251,7,141,38,26,209,106,189,80,162,120,1,44,165,76,169,92,164,194,64,161,237,180,208,171,197,214,151,17,16,105,211,158,183,15,135,115,14,252,127,33,4,152,103,146,115,222,254,123,202,156,57,39,147,172,0,129,100,5,8,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,52,136,80,241,194,73,243,111,242,231,133,199,76,12,235,191,182,31,170,235,136,69,15,126,242,98,75,244,35,133,159,243,41,156,197,55,42,169,224,143,43,167,255,233,62,253,135,115,30,234,239,251,190,254,181,251,246,190,181,74,255,65,225,52,2,250,73,234,184,9,87,172,120,45,84,116,213,16,51,205,209,143,183,61,126,227,119,109,141,10,9,73,10,167,77,91,122,239,164,249,127,25,122,38,35,123,138,62,2,53,215,190,175,144,48,74,33,33,48,237,183,83,175,191,203,202,164,30,27,123,89,68,33,129,128,78,153,114,221,255,124,163,44,173,134,30,251,197,226,59,21,18,120,21,118,74,112,218,60,235,195,227,139,23,42,36,112,4,58,41,45,51,119,116,214,229,214,231,245,176,126,138,2,1,253,40,96,114,248,25,241,83,46,74,252,8,59,169,183,179,85,25,26,193,83,46,74,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,120,62,32,127,110,161,63,47,172,127,239,237,104,233,136,213,181,199,162,189,29,113,229,86,41,254,224,152,188,98,189,193,41,254,80,199,225,122,189,181,157,135,191,86,94,230,225,128,178,102,94,27,41,43,79,15,76,60,231,223,15,84,149,239,124,225,255,39,92,118,189,78,114,70,230,140,155,31,155,124,213,223,206,249,247,238,120,172,102,221,242,163,219,223,81,222,228,201,43,18,83,199,102,233,116,230,222,93,49,176,30,109,114,201,45,165,171,106,179,35,191,83,174,145,115,197,146,210,213,209,129,245,104,233,193,188,95,223,181,33,114,251,250,228,140,128,242,32,79,6,84,120,195,253,131,126,49,206,208,95,149,95,221,241,106,198,132,169,202,5,244,102,204,254,215,139,131,182,126,134,142,62,124,227,42,229,65,222,11,40,107,214,162,252,107,202,134,29,75,78,247,71,202,158,83,62,167,111,189,245,249,102,255,243,121,189,49,195,14,234,157,114,213,81,211,34,239,5,84,240,135,149,22,39,67,133,87,134,194,11,148,163,244,6,4,167,207,183,56,60,245,247,247,40,175,241,90,64,62,95,160,224,55,214,199,29,191,119,194,104,3,2,5,115,157,63,100,26,242,88,64,227,242,103,39,165,166,91,159,15,22,56,28,144,209,6,36,165,142,214,59,168,60,197,99,1,133,194,11,77,198,85,176,240,74,39,191,167,125,190,96,81,137,209,51,76,119,208,113,30,59,15,148,226,55,123,173,171,95,240,171,254,126,229,148,254,254,212,49,33,163,103,152,238,160,227,120,43,3,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,23,40,160,241,51,174,25,123,89,100,76,94,56,125,124,126,119,115,67,123,44,122,252,219,237,205,59,63,84,56,31,28,92,94,219,3,74,11,76,140,220,190,62,123,214,162,129,15,29,169,222,80,243,244,173,61,241,152,194,72,57,190,188,118,190,153,234,243,77,94,176,188,116,117,116,208,221,211,38,204,94,92,250,96,237,228,210,191,43,140,128,59,150,215,198,128,210,50,115,139,151,173,73,201,200,28,98,70,191,119,24,94,246,80,90,48,79,193,144,75,150,215,198,128,34,183,61,155,226,15,14,59,166,223,175,158,121,203,19,10,134,172,47,239,172,229,235,148,109,236,10,200,159,23,206,142,92,103,113,56,103,206,146,209,217,83,20,44,51,90,94,253,179,204,190,229,181,43,160,113,249,191,52,154,15,56,125,233,160,183,184,103,121,109,11,232,114,179,61,52,157,191,196,185,103,121,237,122,25,111,154,60,71,32,35,238,89,94,251,206,3,153,94,72,234,220,133,167,158,228,150,229,229,173,12,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,177,235,146,214,142,67,187,108,157,191,196,185,103,121,237,58,2,29,219,255,149,173,243,151,56,247,44,175,109,1,53,24,238,97,3,1,25,112,207,242,218,245,35,172,117,207,231,241,221,155,173,14,239,254,76,207,43,88,102,180,188,77,181,149,246,45,175,109,119,166,246,247,85,175,253,171,149,207,126,211,51,219,158,88,166,231,21,172,51,88,222,214,234,181,55,217,183,188,54,222,218,220,121,100,239,142,245,255,30,118,76,207,116,29,221,167,96,200,226,242,110,95,247,143,158,182,195,202,54,246,126,212,193,161,205,175,84,174,152,116,164,122,195,160,143,30,173,169,168,188,35,79,207,40,140,136,149,229,109,252,242,117,101,39,219,207,3,245,196,99,95,62,116,125,214,204,107,199,23,95,29,44,42,25,155,31,57,222,176,61,94,191,169,57,250,97,83,77,133,130,140,227,203,123,129,78,36,54,237,120,79,255,82,176,135,131,203,203,153,104,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,60,246,185,241,173,150,47,130,57,61,255,153,114,148,233,6,152,238,160,227,60,22,144,241,165,156,78,95,232,104,124,233,160,215,46,237,245,88,64,61,109,135,123,218,26,173,207,59,254,245,48,218,0,189,107,182,94,187,99,7,143,5,164,237,121,115,149,197,201,174,166,134,216,167,47,41,71,197,54,191,220,213,180,223,226,176,245,93,115,15,239,5,244,205,187,143,52,110,125,115,216,177,239,123,58,183,60,178,244,68,215,49,229,168,19,157,109,91,30,253,115,223,137,222,97,39,15,126,242,146,222,53,229,53,222,11,72,171,126,234,230,246,131,209,161,103,106,203,255,115,108,255,54,229,2,199,190,217,186,243,249,21,67,207,180,199,234,106,158,190,85,121,144,39,3,210,223,214,27,239,153,241,197,154,197,221,131,125,28,223,129,77,207,85,148,5,14,84,173,87,174,209,240,193,147,21,101,193,3,85,229,3,31,234,110,61,244,249,234,69,27,239,46,238,251,174,75,121,144,79,121,92,102,193,220,80,97,73,112,250,252,238,248,193,120,253,166,150,186,141,35,248,127,104,168,120,225,188,149,102,159,48,186,249,129,171,91,162,31,41,67,105,153,185,193,162,146,80,81,73,122,112,82,124,247,167,241,93,85,173,123,28,62,209,32,228,249,128,206,139,11,22,208,197,135,51,209,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,78,234,136,213,41,67,35,120,202,69,201,147,151,180,158,119,166,119,11,121,241,254,27,155,16,208,41,70,55,112,241,201,30,103,16,208,41,251,54,60,108,125,120,239,219,107,20,18,146,20,18,58,143,236,77,246,7,131,211,230,13,59,185,239,221,71,27,42,215,42,36,16,208,79,154,107,43,115,230,44,77,203,204,25,98,230,248,183,59,182,62,118,3,159,236,113,6,1,157,165,191,79,31,90,186,154,27,66,225,5,73,169,233,231,60,216,219,217,86,91,254,223,29,207,220,70,61,103,227,182,158,65,164,101,230,230,204,89,114,242,6,174,194,18,253,215,150,250,77,241,93,85,141,91,222,224,149,215,64,4,4,17,78,36,66,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,33,32,136,16,16,68,8,8,34,4,4,17,2,130,8,1,65,132,128,32,66,64,16,249,1,0,0,255,255,57,103,197,235,0,0,0,6,73,68,65,84,3,0,217,133,88,36,68,161,119,155,0,0,0,0,73,69,78,68,174,66,96,130};

static const unsigned char PWA_ICON_512[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,2,0,0,0,2,0,8,2,0,0,0,123,26,67,173,0,0,16,0,73,68,65,84,120,156,236,221,123,108,94,229,125,192,241,231,117,98,18,163,144,187,73,19,82,135,109,144,208,132,75,46,180,92,210,81,86,149,66,171,65,27,104,72,47,80,164,209,85,145,202,74,181,85,83,186,242,199,90,154,117,72,19,29,45,237,168,70,105,85,180,138,4,180,32,96,20,42,149,113,81,75,130,32,9,151,52,77,128,145,184,198,78,98,59,78,236,130,29,146,198,59,101,82,43,117,162,36,241,123,206,241,123,126,159,143,222,191,28,251,61,143,242,156,223,251,125,207,177,101,143,77,0,132,52,54,1,16,146,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,129,180,76,159,51,99,201,178,9,179,78,27,63,173,173,101,90,219,9,179,23,28,26,28,24,236,109,207,30,67,189,237,191,238,252,229,238,103,214,13,246,236,76,16,67,45,65,213,77,156,179,112,230,57,43,78,92,116,105,246,138,255,182,159,60,208,177,101,207,198,251,186,158,90,219,191,115,115,130,74,19,0,170,108,194,73,243,231,94,241,213,25,103,95,94,171,29,221,169,62,60,60,188,235,169,123,182,223,115,195,107,187,182,39,168,40,1,160,154,178,155,60,243,150,175,158,117,222,39,107,77,77,233,88,29,254,205,161,142,199,239,120,113,221,87,14,244,117,38,168,28,1,160,130,166,159,113,241,162,235,214,52,31,63,41,213,195,193,215,247,111,186,117,69,207,243,15,39,168,150,49,9,170,101,206,69,215,45,92,121,231,152,227,90,82,157,140,105,30,63,235,188,79,12,246,236,24,104,127,54,65,133,8,0,21,82,171,205,255,244,183,230,94,254,143,181,218,177,223,246,121,139,39,110,122,199,217,203,106,99,198,246,110,125,52,65,85,8,0,213,209,246,254,149,217,183,124,83,110,166,158,118,193,80,95,103,255,142,103,18,84,130,0,80,17,83,230,46,93,244,185,53,181,166,124,79,233,214,51,46,233,221,250,223,67,189,237,9,26,95,157,175,148,161,20,227,167,189,115,241,245,235,154,198,54,167,156,101,135,88,124,253,127,102,135,75,208,248,4,128,42,56,227,175,254,125,220,196,214,84,136,236,64,217,225,18,52,62,1,160,225,77,125,215,133,173,103,94,156,10,148,29,46,59,104,130,6,39,0,52,188,121,203,87,167,194,205,189,226,198,4,13,78,0,104,108,51,207,253,248,148,83,207,79,133,155,58,239,189,51,150,124,52,65,35,19,0,26,91,137,175,194,2,64,163,19,0,26,88,211,216,227,90,207,250,80,42,201,244,211,47,74,53,191,76,133,6,38,0,52,176,201,167,158,223,220,50,49,149,100,252,148,89,147,255,236,156,4,13,75,0,104,96,173,103,94,146,74,229,46,16,13,205,95,4,163,129,77,108,91,152,74,85,250,2,96,36,4,128,6,54,110,242,59,82,169,154,143,159,156,160,97,9,0,13,108,220,164,146,3,80,122,129,96,36,4,128,6,54,182,236,55,224,199,149,93,32,24,9,1,160,129,141,105,30,151,74,85,250,2,96,36,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,72,45,211,231,204,88,178,108,194,172,211,198,79,107,107,121,243,145,125,112,176,183,61,123,12,245,182,255,186,243,151,187,159,89,55,216,179,51,209,56,236,41,71,162,150,136,106,226,156,133,51,207,89,113,226,162,75,79,152,189,224,109,63,121,160,99,203,158,141,247,117,61,181,182,127,231,230,52,106,124,248,206,225,84,182,7,175,30,69,67,84,129,61,165,72,2,16,209,132,147,230,207,189,226,171,51,206,190,188,86,59,186,19,96,120,120,120,215,83,247,108,191,231,134,215,118,109,79,163,128,0,252,78,101,246,148,34,9,64,44,217,13,129,121,203,87,207,58,239,147,181,166,166,116,172,14,255,230,80,199,227,119,188,184,238,43,7,250,58,83,169,4,32,85,110,79,41,146,0,4,50,253,140,15,46,186,110,109,243,241,147,82,61,28,124,125,255,166,91,175,236,121,254,39,169,60,2,80,189,61,165,72,199,254,150,129,198,50,231,3,159,123,247,23,31,172,215,43,69,38,123,170,236,9,79,90,122,117,162,36,246,148,17,18,128,16,230,127,250,91,11,174,185,181,214,52,38,213,85,246,132,103,173,252,225,220,143,221,152,106,46,37,139,102,79,25,57,1,168,190,182,247,175,60,249,162,235,82,110,78,249,200,13,239,124,223,103,18,5,178,167,212,133,0,84,220,148,83,207,159,127,245,55,83,206,22,92,243,237,41,115,151,38,10,97,79,169,23,1,168,178,241,83,103,47,254,194,189,77,99,155,83,206,178,67,44,190,126,93,118,184,68,206,236,41,117,36,0,85,118,198,181,183,143,155,216,154,10,145,29,40,59,92,34,103,246,148,58,18,128,202,154,250,174,11,91,207,188,56,21,40,59,92,118,208,68,110,236,41,245,37,0,149,53,111,249,234,84,184,185,87,220,152,200,141,61,165,190,4,160,154,102,158,251,241,236,91,133,169,112,83,231,189,119,198,146,143,38,114,96,79,169,59,1,168,166,18,39,214,139,69,78,236,41,117,231,215,65,87,80,109,76,115,235,89,31,74,37,153,126,250,69,137,122,179,167,228,193,21,64,5,77,153,187,180,185,101,98,42,201,248,41,179,38,159,114,110,162,174,236,41,121,16,128,10,106,61,243,146,84,42,119,12,234,206,158,146,7,183,128,42,104,98,219,194,84,170,210,23,80,61,246,148,60,8,64,5,141,155,252,142,84,170,230,227,39,39,234,202,158,146,7,1,168,160,113,147,74,126,177,40,253,213,170,122,236,41,121,16,128,10,26,91,246,155,181,227,202,126,181,170,30,123,74,30,4,160,130,198,52,143,75,165,42,125,1,213,99,79,201,131,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,240,150,90,166,207,153,177,100,217,132,89,167,141,159,214,214,242,230,35,251,224,96,111,123,246,24,234,109,255,117,231,47,119,63,179,110,176,103,103,2,202,99,78,71,66,0,254,208,196,57,11,103,158,179,226,196,69,151,158,48,123,193,255,255,215,236,131,191,251,248,252,171,190,49,208,177,101,207,198,251,186,158,90,219,191,115,115,2,138,98,78,235,66,0,126,111,194,73,243,231,94,241,213,25,103,95,94,171,213,142,240,75,254,239,60,251,211,75,87,237,122,234,158,237,247,220,240,218,174,237,9,200,147,57,173,35,1,248,173,236,226,113,222,242,213,179,206,251,100,173,169,41,29,189,236,68,156,121,206,242,25,103,47,235,120,252,142,23,215,125,229,64,95,103,2,234,205,156,214,221,177,252,63,86,204,244,51,62,248,231,255,244,220,73,75,175,58,182,179,234,119,154,198,140,109,251,139,207,94,240,207,191,200,158,48,1,117,101,78,243,16,61,0,115,62,240,185,119,127,241,193,230,227,39,165,58,201,158,42,123,194,147,150,94,157,128,58,49,167,57,9,29,128,249,159,254,214,130,107,110,173,53,141,73,117,149,61,225,89,43,127,56,247,99,55,166,35,190,71,9,188,21,115,154,159,184,1,104,123,255,202,147,47,186,46,229,230,148,143,220,240,206,247,125,38,1,35,96,78,115,21,52,0,83,78,61,127,254,213,223,76,57,91,112,205,183,167,204,93,154,128,99,98,78,243,22,49,0,227,167,206,94,252,133,123,155,198,54,167,156,101,135,88,124,253,186,236,112,9,56,74,230,180,0,17,3,112,198,181,183,143,155,216,154,10,145,29,40,59,92,2,142,146,57,45,64,184,0,76,125,215,133,173,103,94,156,10,148,29,46,59,104,2,142,152,57,45,70,184,0,204,91,190,58,21,110,238,21,55,38,224,136,153,211,98,196,10,192,204,115,63,158,125,91,41,21,110,234,188,247,206,88,242,209,4,28,1,115,90,152,88,1,40,113,119,5,0,142,144,57,45,76,160,223,5,84,27,211,220,122,214,135,82,73,166,159,126,81,2,222,142,57,45,82,160,43,128,41,115,151,54,183,76,76,37,25,63,101,214,228,83,206,77,192,31,101,78,139,20,40,0,173,103,94,146,74,229,46,16,188,45,115,90,164,64,183,128,38,182,45,76,165,42,125,1,48,250,153,211,34,5,10,192,184,201,239,72,165,106,62,126,114,2,254,40,115,90,164,72,1,152,84,242,137,85,250,153,13,163,159,57,45,82,160,0,140,45,59,236,199,149,125,102,195,232,103,78,139,20,40,0,99,154,199,165,82,149,190,0,24,253,204,105,145,252,77,96,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,154,82,24,7,95,223,159,74,85,250,2,96,244,51,167,69,10,20,128,3,251,119,165,82,149,190,0,24,253,204,105,145,34,5,96,95,217,39,86,217,11,128,209,207,156,22,41,208,247,0,188,179,128,209,207,156,22,41,208,21,64,127,251,230,84,170,210,23,0,163,159,57,45,82,160,0,244,60,247,80,42,85,233,11,128,209,207,156,22,41,210,21,192,206,205,131,123,59,82,73,134,250,58,179,5,36,224,143,50,167,69,10,20,128,76,239,150,159,166,146,236,217,116,127,2,142,128,57,45,76,172,0,116,151,119,113,215,237,254,15,28,25,115,90,152,88,1,232,90,127,87,127,251,179,169,112,125,47,254,124,247,51,247,38,224,8,152,211,194,196,10,64,102,219,154,85,169,112,219,238,254,114,2,142,152,57,45,70,184,0,100,151,120,189,91,31,75,5,234,126,238,225,189,91,31,77,192,17,51,167,197,8,23,128,204,179,183,93,117,160,191,59,21,226,141,129,158,231,111,191,54,1,71,201,156,22,32,98,0,134,246,118,108,188,101,217,225,67,7,83,206,178,67,60,125,243,165,67,125,175,38,224,40,153,211,2,68,12,64,166,111,251,207,126,113,231,231,83,206,94,184,227,179,251,94,90,159,128,99,98,78,243,22,52,0,153,246,71,110,107,127,228,187,41,55,47,63,112,83,199,19,63,72,192,8,152,211,92,197,13,64,230,133,239,175,220,252,157,79,29,62,244,70,170,171,236,9,55,221,186,162,148,31,99,128,234,49,167,249,9,29,128,76,231,147,63,90,191,250,194,161,190,206,84,39,67,251,186,214,127,237,130,174,13,107,19,80,39,230,52,39,209,3,144,217,247,210,147,79,124,233,244,87,126,124,243,8,223,98,100,95,254,202,67,223,120,98,213,130,125,47,111,72,64,93,153,211,60,8,192,111,29,124,173,111,235,143,254,238,177,191,159,215,249,228,93,195,195,195,233,40,101,95,210,185,126,77,246,229,91,255,227,111,179,167,74,64,14,204,105,221,249,163,240,191,55,216,189,99,243,119,62,241,63,255,117,211,204,247,92,121,226,226,203,78,152,189,224,109,191,100,160,99,203,158,77,247,119,109,88,227,55,125,66,49,204,105,29,9,192,31,202,78,145,236,177,237,238,127,104,153,62,103,198,146,101,19,102,157,54,126,90,91,203,155,143,236,95,7,123,219,179,199,80,111,251,192,171,91,246,108,188,111,176,103,103,2,10,103,78,235,66,0,222,82,118,210,236,120,248,95,19,48,138,153,211,145,16,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,170,41,81,57,7,95,223,159,74,85,250,2,170,199,158,146,7,1,168,160,3,251,119,165,82,149,190,128,234,177,167,228,65,0,42,232,192,190,178,95,44,202,94,64,245,216,83,242,224,123,0,21,228,221,98,245,216,83,242,224,10,160,130,250,219,55,167,82,149,190,128,234,177,167,228,65,0,42,168,231,185,135,82,169,74,95,64,245,216,83,242,32,0,21,212,191,115,243,224,222,142,84,146,161,190,206,108,1,137,186,178,167,228,65,0,170,169,119,203,79,83,73,246,108,186,63,145,3,123,74,221,9,64,53,117,151,119,193,222,237,94,65,62,236,41,117,39,0,213,212,181,254,174,254,246,103,83,225,250,94,252,249,238,103,238,77,228,192,158,82,119,2,80,89,219,214,172,74,133,219,118,247,151,19,185,177,167,212,151,0,84,86,118,217,222,187,245,177,84,160,238,231,30,222,187,245,209,68,110,236,41,245,37,0,85,246,236,109,87,29,232,239,78,133,120,99,160,231,249,219,175,77,228,204,158,82,71,2,80,101,67,123,59,54,222,178,236,240,161,131,41,103,217,33,158,190,249,210,161,190,87,19,57,179,167,212,145,0,84,92,223,246,159,253,226,206,207,167,156,189,112,199,103,247,189,180,62,81,8,123,74,189,8,64,245,181,63,114,91,251,35,223,77,185,121,249,129,155,58,158,248,65,162,64,246,148,186,16,128,16,94,248,254,202,205,223,249,212,225,67,111,164,186,202,158,112,211,173,43,74,249,209,20,236,41,35,39,0,81,116,62,249,163,245,171,47,28,234,235,76,117,50,180,175,107,253,215,46,232,218,176,54,81,18,123,202,8,9,64,32,251,94,122,242,137,47,157,254,202,143,111,30,225,219,198,236,203,95,121,232,27,79,172,90,176,239,229,13,137,82,217,83,70,162,150,136,167,165,245,228,121,203,191,62,243,220,21,181,218,209,157,0,195,195,195,217,219,195,109,107,87,13,118,239,72,163,192,135,239,28,78,101,123,240,234,81,49,68,149,217,83,138,36,0,113,77,156,179,112,230,123,174,60,113,241,101,39,204,94,240,182,159,60,208,177,101,207,166,251,187,54,172,25,85,191,21,82,0,254,64,5,246,148,34,9,0,169,101,250,156,25,75,150,77,152,117,218,248,105,109,45,111,62,178,15,14,246,182,103,143,161,222,246,129,87,183,236,217,120,223,96,207,206,52,250,8,192,91,105,220,61,165,72,2,64,3,19,0,24,9,127,19,24,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,170,41,65,195,58,248,250,254,84,170,210,23,0,35,33,0,52,176,3,251,119,165,82,149,190,0,24,9,1,160,129,29,216,255,197,159,149,0,0,3,69,73,68,65,84,87,118,0,202,94,0,140,132,239,1,208,192,92,1,192,72,184,2,160,129,245,183,111,78,165,42,125,1,48,18,2,64,3,235,121,238,161,84,170,210,23,0,35,33,0,52,176,254,246,103,7,247,118,164,146,12,245,117,102,11,72,208,176,4,128,70,54,60,220,187,229,167,169,36,123,54,221,159,45,32,65,195,18,0,26,91,119,121,55,97,186,221,255,161,193,9,0,141,173,107,253,93,165,220,135,233,123,241,231,187,159,185,55,65,35,19,0,26,222,182,53,171,82,225,182,221,253,229,4,13,78,0,104,120,217,173,152,222,173,143,165,2,117,63,247,240,222,173,143,38,104,112,2,64,21,60,251,221,171,15,244,119,167,66,188,49,208,243,252,247,62,147,160,241,9,0,85,48,212,251,171,141,183,92,126,248,208,193,148,179,236,16,79,223,124,217,80,121,63,123,10,117,52,38,65,37,12,245,182,191,49,208,125,226,162,191,76,121,122,254,123,127,253,219,159,254,132,74,16,0,170,99,255,43,79,143,155,60,115,210,159,156,157,242,241,242,3,55,189,242,224,191,36,168,10,1,160,82,246,108,126,224,181,221,47,101,215,1,181,166,122,158,219,135,15,189,177,249,223,62,181,227,225,91,18,84,72,45,65,229,76,57,245,252,69,127,115,247,248,41,179,82,61,12,237,235,202,190,193,176,239,165,245,9,170,197,21,0,21,52,180,247,87,29,143,127,191,105,76,243,164,63,89,50,146,75,129,236,141,255,142,159,124,115,243,173,43,94,223,253,82,130,202,113,5,64,149,181,180,158,60,111,249,215,103,158,187,162,86,59,186,83,125,120,120,184,107,195,218,109,107,87,13,118,239,72,80,81,2,64,245,77,156,179,112,230,123,174,60,113,241,101,39,204,94,240,182,159,60,208,177,101,207,166,251,187,54,172,233,223,233,119,253,83,113,2,64,32,45,211,231,204,88,178,108,194,172,211,198,79,107,107,153,214,150,245,224,208,224,192,96,111,123,246,24,234,109,31,120,117,203,158,141,247,13,246,236,76,16,131,0,0,4,229,111,2,3,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,255,11,0,0,255,255,156,84,83,158,0,0,0,6,73,68,65,84,3,0,100,20,74,143,110,152,57,168,0,0,0,0,73,69,78,68,174,66,96,130};

static const unsigned char PWA_ICON_MASKABLE_512[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,2,0,0,0,2,0,8,2,0,0,0,123,26,67,173,0,0,16,0,73,68,65,84,120,156,236,221,123,108,94,229,125,192,241,231,117,98,18,163,144,187,73,19,82,135,109,144,208,132,75,46,180,92,210,81,86,149,66,171,65,27,104,72,47,80,164,209,85,145,202,74,181,85,83,186,242,199,90,154,117,72,19,29,45,237,168,70,105,85,180,138,4,180,32,96,20,42,149,113,81,75,130,32,9,151,52,77,128,145,184,198,78,98,59,78,236,130,29,146,198,59,101,82,43,117,162,36,241,123,206,241,123,126,159,143,222,191,28,251,61,143,242,156,223,251,125,207,177,101,143,77,0,132,52,54,1,16,146,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,129,180,76,159,51,99,201,178,9,179,78,27,63,173,173,101,90,219,9,179,23,28,26,28,24,236,109,207,30,67,189,237,191,238,252,229,238,103,214,13,246,236,76,16,67,45,65,213,77,156,179,112,230,57,43,78,92,116,105,246,138,255,182,159,60,208,177,101,207,198,251,186,158,90,219,191,115,115,130,74,19,0,170,108,194,73,243,231,94,241,213,25,103,95,94,171,29,221,169,62,60,60,188,235,169,123,182,223,115,195,107,187,182,39,168,40,1,160,154,178,155,60,243,150,175,158,117,222,39,107,77,77,233,88,29,254,205,161,142,199,239,120,113,221,87,14,244,117,38,168,28,1,160,130,166,159,113,241,162,235,214,52,31,63,41,213,195,193,215,247,111,186,117,69,207,243,15,39,168,150,49,9,170,101,206,69,215,45,92,121,231,152,227,90,82,157,140,105,30,63,235,188,79,12,246,236,24,104,127,54,65,133,8,0,21,82,171,205,255,244,183,230,94,254,143,181,218,177,223,246,121,139,39,110,122,199,217,203,106,99,198,246,110,125,52,65,85,8,0,213,209,246,254,149,217,183,124,83,110,166,158,118,193,80,95,103,255,142,103,18,84,130,0,80,17,83,230,46,93,244,185,53,181,166,124,79,233,214,51,46,233,221,250,223,67,189,237,9,26,95,157,175,148,161,20,227,167,189,115,241,245,235,154,198,54,167,156,101,135,88,124,253,127,102,135,75,208,248,4,128,42,56,227,175,254,125,220,196,214,84,136,236,64,217,225,18,52,62,1,160,225,77,125,215,133,173,103,94,156,10,148,29,46,59,104,130,6,39,0,52,188,121,203,87,167,194,205,189,226,198,4,13,78,0,104,108,51,207,253,248,148,83,207,79,133,155,58,239,189,51,150,124,52,65,35,19,0,26,91,137,175,194,2,64,163,19,0,26,88,211,216,227,90,207,250,80,42,201,244,211,47,74,53,191,76,133,6,38,0,52,176,201,167,158,223,220,50,49,149,100,252,148,89,147,255,236,156,4,13,75,0,104,96,173,103,94,146,74,229,46,16,13,205,95,4,163,129,77,108,91,152,74,85,250,2,96,36,4,128,6,54,110,242,59,82,169,154,143,159,156,160,97,9,0,13,108,220,164,146,3,80,122,129,96,36,4,128,6,54,182,236,55,224,199,149,93,32,24,9,1,160,129,141,105,30,151,74,85,250,2,96,36,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,72,45,211,231,204,88,178,108,194,172,211,198,79,107,107,121,243,145,125,112,176,183,61,123,12,245,182,255,186,243,151,187,159,89,55,216,179,51,209,56,236,41,71,162,150,136,106,226,156,133,51,207,89,113,226,162,75,79,152,189,224,109,63,121,160,99,203,158,141,247,117,61,181,182,127,231,230,52,106,124,248,206,225,84,182,7,175,30,69,67,84,129,61,165,72,2,16,209,132,147,230,207,189,226,171,51,206,190,188,86,59,186,19,96,120,120,120,215,83,247,108,191,231,134,215,118,109,79,163,128,0,252,78,101,246,148,34,9,64,44,217,13,129,121,203,87,207,58,239,147,181,166,166,116,172,14,255,230,80,199,227,119,188,184,238,43,7,250,58,83,169,4,32,85,110,79,41,146,0,4,50,253,140,15,46,186,110,109,243,241,147,82,61,28,124,125,255,166,91,175,236,121,254,39,169,60,2,80,189,61,165,72,199,254,150,129,198,50,231,3,159,123,247,23,31,172,215,43,69,38,123,170,236,9,79,90,122,117,162,36,246,148,17,18,128,16,230,127,250,91,11,174,185,181,214,52,38,213,85,246,132,103,173,252,225,220,143,221,152,106,46,37,139,102,79,25,57,1,168,190,182,247,175,60,249,162,235,82,110,78,249,200,13,239,124,223,103,18,5,178,167,212,133,0,84,220,148,83,207,159,127,245,55,83,206,22,92,243,237,41,115,151,38,10,97,79,169,23,1,168,178,241,83,103,47,254,194,189,77,99,155,83,206,178,67,44,190,126,93,118,184,68,206,236,41,117,36,0,85,118,198,181,183,143,155,216,154,10,145,29,40,59,92,34,103,246,148,58,18,128,202,154,250,174,11,91,207,188,56,21,40,59,92,118,208,68,110,236,41,245,37,0,149,53,111,249,234,84,184,185,87,220,152,200,141,61,165,190,4,160,154,102,158,251,241,236,91,133,169,112,83,231,189,119,198,146,143,38,114,96,79,169,59,1,168,166,18,39,214,139,69,78,236,41,117,231,215,65,87,80,109,76,115,235,89,31,74,37,153,126,250,69,137,122,179,167,228,193,21,64,5,77,153,187,180,185,101,98,42,201,248,41,179,38,159,114,110,162,174,236,41,121,16,128,10,106,61,243,146,84,42,119,12,234,206,158,146,7,183,128,42,104,98,219,194,84,170,210,23,80,61,246,148,60,8,64,5,141,155,252,142,84,170,230,227,39,39,234,202,158,146,7,1,168,160,113,147,74,126,177,40,253,213,170,122,236,41,121,16,128,10,26,91,246,155,181,227,202,126,181,170,30,123,74,30,4,160,130,198,52,143,75,165,42,125,1,213,99,79,201,131,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,240,150,90,166,207,153,177,100,217,132,89,167,141,159,214,214,242,230,35,251,224,96,111,123,246,24,234,109,255,117,231,47,119,63,179,110,176,103,103,2,202,99,78,71,66,0,254,208,196,57,11,103,158,179,226,196,69,151,158,48,123,193,255,255,215,236,131,191,251,248,252,171,190,49,208,177,101,207,198,251,186,158,90,219,191,115,115,2,138,98,78,235,66,0,126,111,194,73,243,231,94,241,213,25,103,95,94,171,213,142,240,75,254,239,60,251,211,75,87,237,122,234,158,237,247,220,240,218,174,237,9,200,147,57,173,35,1,248,173,236,226,113,222,242,213,179,206,251,100,173,169,41,29,189,236,68,156,121,206,242,25,103,47,235,120,252,142,23,215,125,229,64,95,103,2,234,205,156,214,221,177,252,63,86,204,244,51,62,248,231,255,244,220,73,75,175,58,182,179,234,119,154,198,140,109,251,139,207,94,240,207,191,200,158,48,1,117,101,78,243,16,61,0,115,62,240,185,119,127,241,193,230,227,39,165,58,201,158,42,123,194,147,150,94,157,128,58,49,167,57,9,29,128,249,159,254,214,130,107,110,173,53,141,73,117,149,61,225,89,43,127,56,247,99,55,166,35,190,71,9,188,21,115,154,159,184,1,104,123,255,202,147,47,186,46,229,230,148,143,220,240,206,247,125,38,1,35,96,78,115,21,52,0,83,78,61,127,254,213,223,76,57,91,112,205,183,167,204,93,154,128,99,98,78,243,22,49,0,227,167,206,94,252,133,123,155,198,54,167,156,101,135,88,124,253,186,236,112,9,56,74,230,180,0,17,3,112,198,181,183,143,155,216,154,10,145,29,40,59,92,2,142,146,57,45,64,184,0,76,125,215,133,173,103,94,156,10,148,29,46,59,104,2,142,152,57,45,70,184,0,204,91,190,58,21,110,238,21,55,38,224,136,153,211,98,196,10,192,204,115,63,158,125,91,41,21,110,234,188,247,206,88,242,209,4,28,1,115,90,152,88,1,40,113,119,5,0,142,144,57,45,76,160,223,5,84,27,211,220,122,214,135,82,73,166,159,126,81,2,222,142,57,45,82,160,43,128,41,115,151,54,183,76,76,37,25,63,101,214,228,83,206,77,192,31,101,78,139,20,40,0,173,103,94,146,74,229,46,16,188,45,115,90,164,64,183,128,38,182,45,76,165,42,125,1,48,250,153,211,34,5,10,192,184,201,239,72,165,106,62,126,114,2,254,40,115,90,164,72,1,152,84,242,137,85,250,153,13,163,159,57,45,82,160,0,140,45,59,236,199,149,125,102,195,232,103,78,139,20,40,0,99,154,199,165,82,149,190,0,24,253,204,105,145,252,77,96,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,154,82,24,7,95,223,159,74,85,250,2,96,244,51,167,69,10,20,128,3,251,119,165,82,149,190,0,24,253,204,105,145,34,5,96,95,217,39,86,217,11,128,209,207,156,22,41,208,247,0,188,179,128,209,207,156,22,41,208,21,64,127,251,230,84,170,210,23,0,163,159,57,45,82,160,0,244,60,247,80,42,85,233,11,128,209,207,156,22,41,210,21,192,206,205,131,123,59,82,73,134,250,58,179,5,36,224,143,50,167,69,10,20,128,76,239,150,159,166,146,236,217,116,127,2,142,128,57,45,76,172,0,116,151,119,113,215,237,254,15,28,25,115,90,152,88,1,232,90,127,87,127,251,179,169,112,125,47,254,124,247,51,247,38,224,8,152,211,194,196,10,64,102,219,154,85,169,112,219,238,254,114,2,142,152,57,45,70,184,0,100,151,120,189,91,31,75,5,234,126,238,225,189,91,31,77,192,17,51,167,197,8,23,128,204,179,183,93,117,160,191,59,21,226,141,129,158,231,111,191,54,1,71,201,156,22,32,98,0,134,246,118,108,188,101,217,225,67,7,83,206,178,67,60,125,243,165,67,125,175,38,224,40,153,211,2,68,12,64,166,111,251,207,126,113,231,231,83,206,94,184,227,179,251,94,90,159,128,99,98,78,243,22,52,0,153,246,71,110,107,127,228,187,41,55,47,63,112,83,199,19,63,72,192,8,152,211,92,197,13,64,230,133,239,175,220,252,157,79,29,62,244,70,170,171,236,9,55,221,186,162,148,31,99,128,234,49,167,249,9,29,128,76,231,147,63,90,191,250,194,161,190,206,84,39,67,251,186,214,127,237,130,174,13,107,19,80,39,230,52,39,209,3,144,217,247,210,147,79,124,233,244,87,126,124,243,8,223,98,100,95,254,202,67,223,120,98,213,130,125,47,111,72,64,93,153,211,60,8,192,111,29,124,173,111,235,143,254,238,177,191,159,215,249,228,93,195,195,195,233,40,101,95,210,185,126,77,246,229,91,255,227,111,179,167,74,64,14,204,105,221,249,163,240,191,55,216,189,99,243,119,62,241,63,255,117,211,204,247,92,121,226,226,203,78,152,189,224,109,191,100,160,99,203,158,77,247,119,109,88,227,55,125,66,49,204,105,29,9,192,31,202,78,145,236,177,237,238,127,104,153,62,103,198,146,101,19,102,157,54,126,90,91,203,155,143,236,95,7,123,219,179,199,80,111,251,192,171,91,246,108,188,111,176,103,103,2,10,103,78,235,66,0,222,82,118,210,236,120,248,95,19,48,138,153,211,145,16,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,170,41,81,57,7,95,223,159,74,85,250,2,170,199,158,146,7,1,168,160,3,251,119,165,82,149,190,128,234,177,167,228,65,0,42,232,192,190,178,95,44,202,94,64,245,216,83,242,224,123,0,21,228,221,98,245,216,83,242,224,10,160,130,250,219,55,167,82,149,190,128,234,177,167,228,65,0,42,168,231,185,135,82,169,74,95,64,245,216,83,242,32,0,21,212,191,115,243,224,222,142,84,146,161,190,206,108,1,137,186,178,167,228,65,0,170,169,119,203,79,83,73,246,108,186,63,145,3,123,74,221,9,64,53,117,151,119,193,222,237,94,65,62,236,41,117,39,0,213,212,181,254,174,254,246,103,83,225,250,94,252,249,238,103,238,77,228,192,158,82,119,2,80,89,219,214,172,74,133,219,118,247,151,19,185,177,167,212,151,0,84,86,118,217,222,187,245,177,84,160,238,231,30,222,187,245,209,68,110,236,41,245,37,0,85,246,236,109,87,29,232,239,78,133,120,99,160,231,249,219,175,77,228,204,158,82,71,2,80,101,67,123,59,54,222,178,236,240,161,131,41,103,217,33,158,190,249,210,161,190,87,19,57,179,167,212,145,0,84,92,223,246,159,253,226,206,207,167,156,189,112,199,103,247,189,180,62,81,8,123,74,189,8,64,245,181,63,114,91,251,35,223,77,185,121,249,129,155,58,158,248,65,162,64,246,148,186,16,128,16,94,248,254,202,205,223,249,212,225,67,111,164,186,202,158,112,211,173,43,74,249,209,20,236,41,35,39,0,81,116,62,249,163,245,171,47,28,234,235,76,117,50,180,175,107,253,215,46,232,218,176,54,81,18,123,202,8,9,64,32,251,94,122,242,137,47,157,254,202,143,111,30,225,219,198,236,203,95,121,232,27,79,172,90,176,239,229,13,137,82,217,83,70,162,150,136,167,165,245,228,121,203,191,62,243,220,21,181,218,209,157,0,195,195,195,217,219,195,109,107,87,13,118,239,72,163,192,135,239,28,78,101,123,240,234,81,49,68,149,217,83,138,36,0,113,77,156,179,112,230,123,174,60,113,241,101,39,204,94,240,182,159,60,208,177,101,207,166,251,187,54,172,25,85,191,21,82,0,254,64,5,246,148,34,9,0,169,101,250,156,25,75,150,77,152,117,218,248,105,109,45,111,62,178,15,14,246,182,103,143,161,222,246,129,87,183,236,217,120,223,96,207,206,52,250,8,192,91,105,220,61,165,72,2,64,3,19,0,24,9,127,19,24,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,74,0,0,130,18,0,128,160,4,0,32,40,1,0,8,170,41,65,195,58,248,250,254,84,170,210,23,0,35,33,0,52,176,3,251,119,165,82,149,190,0,24,9,1,160,129,29,216,255,197,159,149,0,0,3,69,73,68,65,84,87,118,0,202,94,0,140,132,239,1,208,192,92,1,192,72,184,2,160,129,245,183,111,78,165,42,125,1,48,18,2,64,3,235,121,238,161,84,170,210,23,0,35,33,0,52,176,254,246,103,7,247,118,164,146,12,245,117,102,11,72,208,176,4,128,70,54,60,220,187,229,167,169,36,123,54,221,159,45,32,65,195,18,0,26,91,119,121,55,97,186,221,255,161,193,9,0,141,173,107,253,93,165,220,135,233,123,241,231,187,159,185,55,65,35,19,0,26,222,182,53,171,82,225,182,221,253,229,4,13,78,0,104,120,217,173,152,222,173,143,165,2,117,63,247,240,222,173,143,38,104,112,2,64,21,60,251,221,171,15,244,119,167,66,188,49,208,243,252,247,62,147,160,241,9,0,85,48,212,251,171,141,183,92,126,248,208,193,148,179,236,16,79,223,124,217,80,121,63,123,10,117,52,38,65,37,12,245,182,191,49,208,125,226,162,191,76,121,122,254,123,127,253,219,159,254,132,74,16,0,170,99,255,43,79,143,155,60,115,210,159,156,157,242,241,242,3,55,189,242,224,191,36,168,10,1,160,82,246,108,126,224,181,221,47,101,215,1,181,166,122,158,219,135,15,189,177,249,223,62,181,227,225,91,18,84,72,45,65,229,76,57,245,252,69,127,115,247,248,41,179,82,61,12,237,235,202,190,193,176,239,165,245,9,170,197,21,0,21,52,180,247,87,29,143,127,191,105,76,243,164,63,89,50,146,75,129,236,141,255,142,159,124,115,243,173,43,94,223,253,82,130,202,113,5,64,149,181,180,158,60,111,249,215,103,158,187,162,86,59,186,83,125,120,120,184,107,195,218,109,107,87,13,118,239,72,80,81,2,64,245,77,156,179,112,230,123,174,60,113,241,101,39,204,94,240,182,159,60,208,177,101,207,166,251,187,54,172,233,223,233,119,253,83,113,2,64,32,45,211,231,204,88,178,108,194,172,211,198,79,107,107,153,214,150,245,224,208,224,192,96,111,123,246,24,234,109,31,120,117,203,158,141,247,13,246,236,76,16,131,0,0,4,229,111,2,3,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,2,0,16,148,0,0,4,37,0,0,65,9,0,64,80,255,11,0,0,255,255,156,84,83,158,0,0,0,6,73,68,65,84,3,0,100,20,74,143,110,152,57,168,0,0,0,0,73,69,78,68,174,66,96,130};
