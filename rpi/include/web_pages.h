// web_pages.h — GENERADO por scripts/gen_web_pages.py a partir de rpi/web/*.html. NO editar a mano.
//   PAGE_PANEL : panel principal (/)
//   PAGE_CAL   : calibración acústica con el micrófono del móvil (/calibrate, requiere HTTPS)
//   PAGE_BLE   : configuración de los ESP32 por Web Bluetooth    (/ble, requiere HTTPS)
// El bloque ANALYSIS_BEGIN/END de PAGE_CAL son funciones puras: tests/test_analysis.js lo extrae y ejecuta en Node.
#pragma once

static const char PAGE_PANEL[] = R"HTML(<!DOCTYPE html>
<html lang="es"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#000000">
<title>SonosWiFi</title>
<style>
/* Sistema de diseño compartido por las tres páginas (panel, /calibrate, /ble). Negro puro + azul señal. */
:root{
  --bg:#000; --panel:#0a0d13; --raise:#121722; --line:#1f2735; --line2:#2b3548;
  --fg:#e9edf5; --mut:#8a93a6; --ac:#4aa3ff; --ac-bg:rgba(74,163,255,.13); --ac-ln:rgba(74,163,255,.5);
  --ok:#3ecf8e; --warn:#f5b84a; --bad:#ff6b6b;
  --font:"Segoe UI Variable Text","Segoe UI",system-ui,-apple-system,"Helvetica Neue",Roboto,sans-serif;
}
*{box-sizing:border-box;margin:0}
html{color-scheme:dark}
body{background:var(--bg);color:var(--fg);font:15px/1.45 var(--font);-webkit-font-smoothing:antialiased;
  font-variant-numeric:tabular-nums;min-height:100vh;-webkit-text-size-adjust:100%}
a{color:var(--ac);text-decoration:none}a:hover{text-decoration:underline}
button,input,select{font:inherit;color:inherit}
button{cursor:pointer}
[hidden]{display:none!important}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px;border-radius:6px}
h1{font-size:1.55rem;font-weight:650;letter-spacing:-.01em}
h2{font-size:1.02rem;font-weight:650}
.mut{color:var(--mut)}
.small{font-size:.86rem}

.btn{appearance:none;border:0;background:var(--ac);color:#04121f;font-weight:650;padding:10px 18px;border-radius:10px;min-height:42px;
  transition:filter .12s,transform .06s}
.btn:hover{filter:brightness(1.1)}.btn:active{transform:translateY(1px)}
.btn.ghost{background:var(--raise);color:var(--fg);border:1px solid var(--line2);font-weight:550}
.btn.danger{background:transparent;color:var(--bad);border:1px solid rgba(255,107,107,.45);font-weight:550}
.btn.sm{padding:6px 12px;min-height:34px;font-size:.88rem;border-radius:8px}
.btn:disabled{opacity:.4;cursor:not-allowed;filter:none}

.seg{display:inline-flex;background:var(--raise);border:1px solid var(--line);border-radius:11px;padding:3px;gap:2px;max-width:100%}
.seg button{appearance:none;border:0;background:transparent;color:var(--mut);padding:7px 14px;border-radius:8px;min-height:34px;font-weight:550;white-space:nowrap}
.seg button:hover{color:var(--fg)}
.seg button[aria-pressed=true]{background:var(--ac-bg);color:var(--ac);box-shadow:inset 0 0 0 1px var(--ac-ln)}
.seg.block{display:flex}.seg.block button{flex:1}

.switch{appearance:none;position:relative;width:46px;height:27px;border-radius:14px;border:1px solid var(--line2);background:var(--raise);flex:none;
  transition:background .15s,border-color .15s}
.switch::after{content:"";position:absolute;top:2px;left:2px;width:21px;height:21px;border-radius:50%;background:var(--mut);transition:transform .15s,background .15s}
.switch[aria-checked=true]{background:var(--ac-bg);border-color:var(--ac-ln)}
.switch[aria-checked=true]::after{transform:translateX(19px);background:var(--ac)}

input[type=range].rng{-webkit-appearance:none;appearance:none;width:100%;height:28px;background:transparent;border-radius:0;cursor:pointer;
  --f:.5;
  background:linear-gradient(to right,var(--ac) calc(var(--f)*(100% - 20px) + 10px),var(--line2) 0) no-repeat center/100% 6px}
input[type=range].rng.mid{--lo:.5;--hi:.5;
  background:linear-gradient(to right,var(--line2) calc(var(--lo)*(100% - 20px) + 10px),var(--ac) 0,var(--ac) calc(var(--hi)*(100% - 20px) + 10px),var(--line2) 0) no-repeat center/100% 6px}
.rng::-webkit-slider-runnable-track{height:28px;background:transparent}
.rng::-moz-range-track{height:28px;background:transparent}
.rng::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#fff;border:0;margin-top:4px;box-shadow:0 0 0 3px rgba(74,163,255,.38)}
.rng::-moz-range-thumb{width:20px;height:20px;border-radius:50%;background:#fff;border:0;box-shadow:0 0 0 3px rgba(74,163,255,.38)}
.rng:focus-visible{outline-offset:0}

.num,.txt,.sel{background:var(--raise);border:1px solid var(--line2);border-radius:9px;padding:9px 11px;min-height:40px;width:100%;color:var(--fg)}
.num{width:110px}
.num:focus,.txt:focus,.sel:focus{outline:2px solid var(--ac);outline-offset:1px;border-color:transparent}
.sel{appearance:none;background-image:linear-gradient(45deg,transparent 50%,var(--mut) 50%),linear-gradient(135deg,var(--mut) 50%,transparent 50%);
  background-position:calc(100% - 17px) 17px,calc(100% - 12px) 17px;background-size:5px 5px,5px 5px;background-repeat:no-repeat;padding-right:32px}
label,.lbl{display:block;color:var(--mut);font-size:.86rem;margin-bottom:5px}

.chip{display:inline-flex;align-items:center;gap:7px;padding:4px 11px;border-radius:999px;background:var(--raise);border:1px solid var(--line);font-size:.84rem;color:var(--mut)}
.chip.ac{color:var(--ac);border-color:var(--ac-ln);background:var(--ac-bg)}
.chip.warn{color:var(--warn);border-color:rgba(245,184,74,.4)}
.chip.bad{color:var(--bad);border-color:rgba(255,107,107,.4)}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--mut);flex:none}
.dot.ok{background:var(--ok)}.dot.bad{background:var(--bad)}.dot.warn{background:var(--warn)}

.top{position:sticky;top:0;z-index:30;background:rgba(0,0,0,.88);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);border-bottom:1px solid var(--line)}
.top .in{max-width:1180px;margin:auto;display:flex;align-items:center;gap:18px;padding:10px 18px;flex-wrap:wrap}
.brand{display:flex;align-items:center;gap:10px;font-weight:700;font-size:1.05rem;color:var(--fg)}
.brand:hover{text-decoration:none}
.logo{width:26px;height:26px;flex:none}
.panelbox{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:18px}
.note{border-radius:12px;padding:12px 14px;background:var(--raise);border:1px solid var(--line2);font-size:.92rem}
.note.bad{border-color:rgba(255,107,107,.5);color:#ffb0b0}.note.warn{border-color:rgba(245,184,74,.5)}

.toasts{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);display:flex;flex-direction:column;gap:6px;z-index:60;pointer-events:none;width:max-content;max-width:92vw}
.toast{background:var(--raise);border:1px solid var(--line2);border-left:3px solid var(--ok);padding:9px 14px;border-radius:10px;font-size:.9rem;
  animation:tin .18s ease-out}
.toast.bad{border-left-color:var(--bad)}
@keyframes tin{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
main{max-width:1180px;margin:0 auto;padding:18px 18px 90px}
.top .in{gap:14px}
.mv{display:flex;align-items:center;gap:10px;margin-left:auto;min-width:210px;flex:0 1 300px}
.mv svg{width:20px;height:20px;flex:none;color:var(--mut)}
.mv .rng{flex:1}.mv b{min-width:42px;text-align:right;font-weight:600}
.links{display:flex;gap:6px}
.tabs{display:flex;gap:4px;overflow-x:auto;margin:6px 0 18px;border-bottom:1px solid var(--line);scrollbar-width:none}
.tabs::-webkit-scrollbar{display:none}
.tab{appearance:none;background:none;border:0;color:var(--mut);padding:11px 16px;font-weight:600;border-bottom:2px solid transparent;white-space:nowrap;margin-bottom:-1px}
.tab:hover{color:var(--fg)}
.tab[aria-selected=true]{color:var(--ac);border-bottom-color:var(--ac)}
.pane[hidden]{display:none}
.head{display:flex;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:14px}
.head h1{margin-right:auto}

/* Plano de sala */
.room{position:relative;width:100%;aspect-ratio:16/9;min-height:340px;border:1px solid var(--line);border-radius:20px;overflow:hidden;
  background:radial-gradient(ellipse 70% 60% at 50% 62%,rgba(74,163,255,.09),transparent 70%),var(--panel)}
.room svg.lines{position:absolute;inset:0;width:100%;height:100%}
.room .ring{fill:none;stroke:var(--line);stroke-width:1;vector-effect:non-scaling-stroke}
.room .ln{stroke:var(--ac);stroke-opacity:.22;stroke-width:1;stroke-dasharray:3 5;vector-effect:non-scaling-stroke}
.room .ln.pair{stroke-opacity:.35;stroke-dasharray:none}
.gd{position:absolute;transform:translate(-50%,-50%);font-size:.7rem;color:var(--ac);border:1px solid var(--ac-ln);background:var(--ac-bg);border-radius:8px;padding:1px 6px;z-index:1}
.gd.off{color:var(--mut);border-color:var(--line2);background:transparent}
.room .ln.off{stroke:var(--line2);stroke-opacity:.5}
.me{position:absolute;left:50%;top:76%;transform:translate(-50%,-50%);width:46px;height:46px;border-radius:50%;border:1px solid var(--line2);background:var(--raise);
  display:grid;place-items:center;color:var(--mut)}
.me svg{width:22px;height:22px}
.screen{position:absolute;left:30%;right:30%;top:2.5%;height:3px;border-radius:3px;background:var(--line2)}
.spk{position:absolute;transform:translate(-50%,-50%);display:flex;flex-direction:column;align-items:center;gap:5px;background:none;border:0;padding:0;color:var(--fg);z-index:2}
.orb{position:relative;width:58px;height:58px;border-radius:50%;background:var(--raise);border:1.5px solid var(--ac-ln);display:grid;place-items:center;
  font-weight:700;font-size:1.15rem;transition:transform .12s,border-color .12s,box-shadow .12s}
.spk:hover .orb{transform:scale(1.06)}
.spk[aria-expanded=true] .orb{border-color:var(--ac);box-shadow:0 0 0 4px var(--ac-bg),0 0 26px rgba(74,163,255,.35)}
.spk.sub .orb{border-radius:16px}
.spk.off .orb{border-style:dashed;border-color:var(--line2);color:var(--mut);background:transparent}
.spk .st{position:absolute;right:-1px;top:-1px;width:13px;height:13px;border-radius:50%;border:2px solid var(--panel);background:var(--bad)}
.spk .st.on{background:var(--ok)}
.spk .cap{font-size:.78rem;color:var(--mut);background:rgba(0,0,0,.65);padding:1px 7px;border-radius:6px;white-space:nowrap}
.spk .lv{position:absolute;bottom:-1px;left:50%;transform:translateX(-50%);width:34px;height:3px;border-radius:2px;background:var(--line2);overflow:hidden}
.spk .lv i{display:block;height:100%;background:var(--ac);width:0}
.legend{display:flex;gap:16px;flex-wrap:wrap;margin-top:12px;color:var(--mut);font-size:.85rem}
.chips{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px}
.chips button{appearance:none;background:var(--panel);border:1px solid var(--line);border-radius:999px;padding:7px 14px 7px 11px;display:flex;align-items:center;gap:8px;min-height:38px}
.chips button:hover{border-color:var(--line2)}
.chips button[aria-expanded=true]{border-color:var(--ac-ln);background:var(--ac-bg)}
.empty{padding:26px;text-align:center;color:var(--mut)}

/* Cajón del altavoz */
.scrim{position:fixed;inset:0;background:rgba(0,0,0,.55);z-index:40;opacity:0;pointer-events:none;transition:opacity .18s}
.scrim.on{opacity:1;pointer-events:auto}
.drawer{position:fixed;top:0;right:0;bottom:0;width:min(420px,100%);background:var(--panel);border-left:1px solid var(--line2);z-index:50;
  transform:translateX(102%);transition:transform .2s ease-out;display:flex;flex-direction:column}
.drawer.on{transform:none}
.drawer header{display:flex;align-items:center;gap:10px;padding:16px 18px;border-bottom:1px solid var(--line)}
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
.tile{appearance:none;text-align:left;border:1.5px solid var(--line);background:var(--panel);border-radius:18px;padding:20px;display:flex;flex-direction:column;gap:6px;transition:border-color .12s,background .12s}
.tile:hover{border-color:var(--line2)}
.tile[aria-pressed=true]{border-color:var(--ac);background:var(--ac-bg)}
.tile h2{font-size:1.2rem}.tile p{color:var(--mut)}
.tile .tag{align-self:flex-start;margin-top:6px}
.cards{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:18px;display:flex;flex-direction:column;gap:14px}
.card.dim{opacity:.55}
.card p{color:var(--mut);font-size:.92rem}
.hint{font-size:.88rem;color:var(--mut)}

/* Latencia */
.cmd{display:block;background:#05070b;border:1px solid var(--line);border-radius:10px;padding:10px 12px;font-family:ui-monospace,Consolas,monospace;font-size:.85rem;word-break:break-all;color:var(--fg)}
.big{font-size:2.3rem;font-weight:700;letter-spacing:-.02em;line-height:1.1}
.big small{font-size:1rem;color:var(--mut);font-weight:500;margin-left:4px}
table{width:100%;border-collapse:collapse;font-size:.9rem}
th{font-weight:550;color:var(--mut);text-align:right;padding:8px 10px;border-bottom:1px solid var(--line);white-space:nowrap}
td{padding:9px 10px;text-align:right;border-bottom:1px solid var(--line)}
th:first-child,td:first-child{text-align:left}
td.bad{color:var(--bad)}td.warn{color:var(--warn)}
.tw{overflow-x:auto;border:1px solid var(--line);border-radius:14px;background:var(--panel)}
.tw table{min-width:640px}.tw tr:last-child td{border-bottom:0}
.log{background:#05070b;border:1px solid var(--line);border-radius:12px;padding:12px;font:.84rem/1.6 ui-monospace,Consolas,monospace;height:340px;overflow-y:auto;color:var(--mut)}
.log b{color:var(--ac);font-weight:500}

.found{border:1px solid var(--line);background:var(--bg);border-radius:14px;padding:12px 14px;display:flex;flex-direction:column;gap:10px}
.found .hd{display:flex;align-items:center;gap:8px;flex-wrap:wrap}.found .hd b{font-weight:650;margin-right:auto}
.found .ac{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.found .ac .sel{width:auto;min-width:130px;flex:1}
.found.done{border-color:rgba(62,207,142,.45)}
.found .res{font-size:.88rem}.found .res.ok{color:var(--ok)}.found .res.bad{color:var(--bad)}
.spin{display:inline-block;width:14px;height:14px;border:2px solid var(--line2);border-top-color:var(--ac);border-radius:50%;animation:sp .8s linear infinite;vertical-align:-2px;margin-right:6px}
@keyframes sp{to{transform:rotate(360deg)}}
@media (max-width:760px){
  main{padding:12px 12px 90px}
  .top .in{padding:8px 12px;gap:8px}
  .mv{order:5;flex:1 1 100%;margin-left:0}
  .links{margin-left:auto}
  .room{aspect-ratio:3/5;min-height:520px}
  .orb{width:50px;height:50px;font-size:1rem}
  .tiles,.cards{grid-template-columns:1fr}
  .drawer{top:auto;width:100%;max-height:88vh;border-left:0;border-top:1px solid var(--line2);border-radius:20px 20px 0 0;transform:translateY(102%)}
  .drawer.on{transform:none}
  .flow{flex-wrap:nowrap}
}
</style>
</head><body>
<div class="top"><div class="in">
  <a class="brand" href="/"><svg class="logo" viewBox="0 0 26 26" fill="none" stroke="#4aa3ff" stroke-width="2" stroke-linecap="round"><path d="M3 13h2M8 8v10M13 4v18M18 8v10M23 13h-2"/></svg>SonosWiFi</a>
  <span class="chip" id="st"><i class="dot" id="dot"></i><span id="hs">Conectando…</span></span>
  <div class="mv" title="Volumen maestro">
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M11 5 6 9H3v6h3l5 4z"/><path d="M15.5 8.5a5 5 0 0 1 0 7M18.5 5.5a9 9 0 0 1 0 13"/></svg>
    <input class="rng" id="mvs" type="range" min="0" max="100" value="100" aria-label="Volumen maestro"><b id="mvd">100%</b>
  </div>
  <div class="links"><a class="btn ghost sm" id="lcal" href="#">Calibrar</a><button class="btn ghost sm" id="lble">Añadir altavoces</button></div>
</div></div>

<main>
<div class="tabs" role="tablist">
  <button class="tab" role="tab" data-tab="spk" aria-selected="true">Altavoces</button>
  <button class="tab" role="tab" data-tab="snd" aria-selected="false">Sonido</button>
  <button class="tab" role="tab" data-tab="lat" aria-selected="false">Latencia</button>
  <button class="tab" role="tab" data-tab="log" aria-selected="false">Registro</button>
</div>

<section class="pane" id="p_spk" role="tabpanel">
  <div class="head"><h1>Tu sala</h1><span class="chip" id="onl">—</span>
    <div class="seg" id="lyseg" role="group" aria-label="Distribución">
      <button data-ly="stereo5" aria-pressed="false">5 estéreo</button><button data-ly="mono10" aria-pressed="false">10 mono</button>
    </div></div>
  <div class="room" id="room" aria-label="Plano de la sala con los altavoces">
    <svg class="lines" id="lines" viewBox="0 0 100 100" preserveAspectRatio="none" aria-hidden="true">
      <ellipse class="ring" cx="50" cy="76" rx="22" ry="17"/><ellipse class="ring" cx="50" cy="76" rx="42" ry="30"/>
    </svg>
    <div class="screen" title="Pantalla"></div>
    <div class="me" title="Posición de escucha"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><circle cx="12" cy="8" r="4"/><path d="M4 21c1-4 4-6 8-6s7 2 8 6"/></svg></div>
    <div id="spks"></div>
  </div>
  <div class="legend"><span>Cuadrado = subwoofer</span><span>Punto verde = en línea</span><span>Toca un altavoz para ajustarlo. Los cambios se guardan solos.</span></div>
  <div class="chips" id="chips"></div>
</section>

<section class="pane" id="p_snd" role="tabpanel" hidden>
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

<section class="pane" id="p_lat" role="tabpanel" hidden>
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

<section class="pane" id="p_log" role="tabpanel" hidden>
  <div class="head"><h1>Registro</h1><button class="btn ghost sm" id="logclr">Limpiar</button></div>
  <div class="log" id="lb" role="log"></div>
</section>
</main>

<div class="scrim" id="scrim"></div>
<aside class="drawer" id="drawer" aria-hidden="true" aria-label="Ajustes del altavoz">
  <header><h2 id="dtitle">Altavoz</h2><span class="chip" id="dst">—</span><button class="x" id="dclose" aria-label="Cerrar">✕</button></header>
  <div class="dbody" id="dbody"></div>
</aside>
<aside class="drawer" id="bdrawer" aria-hidden="true" aria-label="Añadir altavoces por Bluetooth">
  <header><h2>Añadir altavoces</h2><button class="x" id="bclose" aria-label="Cerrar">✕</button></header>
  <div class="dbody" id="bbody"></div>
</aside>
<div class="toasts" id="tb"></div>

<script>
const CH=['FL','FR','C','LFE','SL','SR','SBL','SBR','AUX-L','AUX-R'];
const CHN=['Frontal izq.','Frontal der.','Central','Graves (LFE)','Lateral izq.','Lateral der.','Trasero izq.','Trasero der.','Extra izq.','Extra der.'];
const POS=[[15,13],[85,13],[50,9],[6,34],[9,58],[91,58],[18,91],[82,91],[24,40],[76,40]];
const LY=76;
const EQF=['354 Hz','707 Hz','1,4 kHz','2,8 kHz','5,7 kHz','9,8 kHz'];
const ROLES=['Rango completo','Satélite','Subwoofer'];
const LYN=['5 ESP estéreo','10 ESP mono (1 por canal)','Personalizada'];
let nodes=[],layout=0,st={},openId=null,sig='';
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const pct=v=>Math.round(v/2.55);
const J=(u,b)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)}).then(r=>r.json()).catch(()=>({error:'red'}));

function toast(m,ok=true){const d=document.createElement('div');d.className='toast'+(ok?'':' bad');d.textContent=m;$('tb').appendChild(d);setTimeout(()=>d.remove(),2600);}
function log(m){const b=$('lb'),t=new Date().toLocaleTimeString();const d=document.createElement('div');d.innerHTML='<b>'+t+'</b> '+esc(m);b.appendChild(d);b.scrollTop=b.scrollHeight;}
function fillRng(el){const mn=+el.min,mx=+el.max,v=+el.value;
  if(el.classList.contains('mid')){const p=(v-mn)/(mx-mn);el.style.setProperty('--lo',v>=0?.5:p);el.style.setProperty('--hi',v>=0?p:.5);}
  else el.style.setProperty('--f',(v-mn)/(mx-mn));}
function setSeg(seg,attr,val){seg.querySelectorAll('button').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset[attr])===String(val)));}
function setSw(el,on){el.setAttribute('aria-checked',on?'true':'false');}

/* Pestañas */
function showTab(t){document.querySelectorAll('.tab').forEach(b=>b.setAttribute('aria-selected',b.dataset.tab===t));
  ['spk','snd','lat','log'].forEach(k=>$('p_'+k).hidden=(k!==t));
  try{history.replaceState(null,'','#'+t);}catch(e){}if(t==='lat')updLat();}
document.querySelectorAll('.tab').forEach(b=>b.onclick=()=>showTab(b.dataset.tab));

/* Datos */
async function loadStatus(){
  try{const d=await (await fetch('/api/status')).json();st=d;
    $('hs').textContent=(d.running?'Capturando':'Detenido')+' · '+d.sample_rate/1000+' kHz';
    $('dot').className='dot '+(d.running?'ok':'bad');
    if(d.layout!==undefined&&d.layout!==layout){layout=d.layout;}
    setSeg($('lyseg'),'ly',layout===0?'stereo5':layout===1?'mono10':'');
    if(document.activeElement!==$('xos')){$('xos').value=d.xover_hz||80;$('xod').textContent=Math.round(d.xover_hz||80)+' Hz';fillRng($('xos'));}
    if(document.activeElement!==$('lex')&&d.lipsync_extra_ms!==undefined)$('lex').value=d.lipsync_extra_ms;
    if(document.activeElement!==$('pls')&&d.playout_ms){$('pls').value=d.playout_ms;$('pld').textContent=Math.round(d.playout_ms)+' ms';fillRng($('pls'));setSeg($('plseg'),'pl',Math.round(d.playout_ms));}
    if(document.activeElement!==$('mvs')&&d.master_volume!==undefined){const v=Math.round(Math.min(1,d.master_volume)*100);$('mvs').value=v;$('mvd').textContent=v+'%';fillRng($('mvs'));}
    drawSound();
  }catch(e){$('hs').textContent='Sin conexión';$('dot').className='dot bad';}
}
async function loadNodes(){
  try{nodes=await (await fetch('/api/nodes')).json();}catch(e){return;}
  const vis=nodes.filter(n=>n.enabled||n.online||layout!==0);
  const s=JSON.stringify(vis.map(n=>[n.id,n.enabled,n.online,n.ch_left,n.ch_right,n.mono,n.role,n.ip]))+openId+layout;
  if(s!==sig){sig=s;drawRoom();}
  else vis.forEach(n=>{const l=document.querySelector('#spks [data-id="'+n.id+'"] .lv i');if(l)l.style.width=Math.max(0,Math.min(100,n.fill_pct||0))+'%';});
  const on=nodes.filter(n=>n.enabled&&n.online).length,tot=nodes.filter(n=>n.enabled).length;
  $('onl').innerHTML='<i class="dot '+(tot&&on===tot?'ok':on?'warn':'bad')+'"></i> '+on+' de '+tot+' en línea';
  drawHealth();drawSound();if(openId!==null)updDrawerStats();
}
const find=id=>nodes.find(n=>n.id===id);
function nodePos(n){const a=POS[n.ch_left]||[50,50],b=POS[n.ch_right]||a;return n.mono||n.ch_left===n.ch_right?a:[(a[0]+b[0])/2,(a[1]+b[1])/2];}
function nodeLabel(n){return n.mono||n.ch_left===n.ch_right?CH[n.ch_left]:CH[n.ch_left]+' + '+CH[n.ch_right];}

function drawRoom(){
  const vis=nodes.filter(n=>n.enabled||n.online||layout!==0);
  const used={};let h='',ln='';
  let gh='';
  vis.forEach(n=>{if(!n.enabled)return;let [x,y]=nodePos(n);const k=Math.round(x)+','+Math.round(y);used[k]=(used[k]||0)+1;x+=(used[k]-1)*5;y+=(used[k]-1)*7;
    ln+='<line class="ln'+(n.online?'':' off')+'" x1="50" y1="'+LY+'" x2="'+x+'" y2="'+y+'"/>';
    if(!(n.mono||n.ch_left===n.ch_right))[n.ch_left,n.ch_right].forEach(c=>{const g=POS[c]||[50,50];
      ln+='<line class="ln pair" x1="'+x+'" y1="'+y+'" x2="'+g[0]+'" y2="'+g[1]+'"/>';
      gh+='<span class="gd'+(n.online?'':' off')+'" style="left:'+g[0]+'%;top:'+g[1]+'%" title="'+CHN[c]+'">'+CH[c]+'</span>';});
    h+='<button class="spk'+(n.role===2?' sub':'')+(n.online?'':' off')+'" data-id="'+n.id+'" aria-expanded="'+(openId===n.id)+'" style="left:'+x+'%;top:'+y+'%" aria-label="Altavoz '+n.id+', '+nodeLabel(n)+'">'+
      '<span class="orb">'+n.id+'<i class="st'+(n.online?' on':'')+'"></i><span class="lv"><i style="width:'+Math.min(100,n.fill_pct||0)+'%"></i></span></span><span class="cap">'+nodeLabel(n)+(n.role===2?' · sub':n.role===1?' · sat':'')+'</span></button>';});
  $('spks').innerHTML=gh+h||'<div class="empty" style="position:absolute;inset:0;display:grid;place-items:center">Aún no hay altavoces. Enciende un ESP32 o añádelo con «Añadir altavoz».</div>';
  document.querySelectorAll('#lines .ln').forEach(e=>e.remove());$('lines').insertAdjacentHTML('beforeend',ln);
  $('chips').innerHTML=vis.map(n=>'<button data-id="'+n.id+'" aria-expanded="'+(openId===n.id)+'"><i class="dot '+(n.online?'ok':'bad')+'"></i>#'+n.id+' · '+nodeLabel(n)+(n.enabled?'':' · apagado')+'</button>').join('');
  document.querySelectorAll('#spks .spk,#chips button').forEach(b=>b.onclick=()=>openDrawer(+b.dataset.id));
}

/* Cajón */
function openDrawer(id){closeBle();openId=id;sig='';buildDrawer();$('drawer').classList.add('on');$('scrim').classList.add('on');$('drawer').setAttribute('aria-hidden','false');drawRoom();sig='x';loadNodes();}
function closeDrawer(){const was=openId!==null;openId=null;$('drawer').classList.remove('on');if(!$('bdrawer').classList.contains('on'))$('scrim').classList.remove('on');$('drawer').setAttribute('aria-hidden','true');sig='';drawRoom();}
$('scrim').onclick=()=>{closeDrawer();closeBle();};$('dclose').onclick=closeDrawer;
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
   '<hr class="sep"><div class="hint" id="d_stats"></div><div class="savestate" id="d_ss"></div>';
  wireDrawer();modeUI();rolHint();eqHint();document.querySelectorAll('#dbody .rng').forEach(fillRng);for(let i=0;i<6;i++)eqOut(i);updDrawerStats();
}
function eqOut(i){const v=+$('d_eq'+i).value;$('d_eo'+i).textContent=(v>0?'+':'')+v.toFixed(1).replace('.',',');}
function modeUI(){const mono=$('d_md').querySelector('[aria-pressed=true]').dataset.v==='1';
  $('d_crw').style.display=mono?'none':'';$('d_vrw').style.display=mono?'none':'';
  $('d_cll').textContent=mono?'Canal':'Canal izquierdo';$('d_vll').textContent=mono?'Volumen':'Volumen izquierdo';}
function rolHint(){const r=+$('d_rl').querySelector('[aria-pressed=true]').dataset.v;
  $('d_rlh').textContent=r===2?'Recibe su canal y los graves de los demás. Solo se usa con el crossover activo.':r===1?'Solo agudos por encima del corte, cuando hay subwoofer en línea.':'Recibe el canal completo.';}
function eqHint(){const el=$('d_eqh');if(!el)return;el.innerHTML=st.dsp_eq?'Se aplica a este altavoz.':'La EQ está desactivada (pass-through). <a href="#" id="d_eqon">Activar</a>';
  const a=$('d_eqon');if(a)a.onclick=e=>{e.preventDefault();setDsp({eq:true});};}
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
    layout=(r1.layout!==undefined)?r1.layout:layout;if($('d_ss'))$('d_ss').textContent='Guardado';log('Altavoz #'+id+' actualizado');sig='';loadStatus();loadNodes();}
  else{if($('d_ss'))$('d_ss').textContent='No se pudo guardar';toast('No se pudo guardar',false);}
}
function wireDrawer(){
  const q=queueSave;
  $('d_en').onclick=()=>{setSw($('d_en'),$('d_en').getAttribute('aria-checked')!=='true');q();};
  $('d_md').querySelectorAll('button').forEach(b=>b.onclick=()=>{setSeg($('d_md'),'v',b.dataset.v);modeUI();q();});
  $('d_rl').querySelectorAll('button').forEach(b=>b.onclick=()=>{setSeg($('d_rl'),'v',b.dataset.v);rolHint();q();});
  ['d_cl','d_cr'].forEach(i=>$(i).onchange=q);
  $('d_lat').onchange=q;
  $('d_vl').oninput=function(){$('d_vld').textContent=pct(+this.value)+'%';fillRng(this);q();};
  $('d_vr').oninput=function(){$('d_vrd').textContent=pct(+this.value)+'%';fillRng(this);q();};
  for(let i=0;i<6;i++)$('d_eq'+i).oninput=function(){fillRng(this);eqOut(i);q();};
  $('d_eqz').onclick=()=>{for(let i=0;i<6;i++){$('d_eq'+i).value=0;fillRng($('d_eq'+i));eqOut(i);}q();};
}
function updDrawerStats(){const n=find(openId),s=$('d_stats');if(!n||!s)return;
  $('dst').className='chip '+(n.online?'ac':'bad');$('dst').textContent=n.online?(n.ip||'en línea'):'sin conexión';
  s.textContent=n.online?'RTT '+((n.rtt_us||0)/1000).toFixed(1)+' ms · jitter '+(n.jitter_us||0)+' µs · perdidos '+(n.lost_packets||0)+' · recuperados '+(n.recovered_packets||0)+' · vacíos '+(n.underruns||0):'Este altavoz no responde ahora mismo.';}

/* Distribución */
$('lyseg').querySelectorAll('button').forEach(b=>b.onclick=async()=>{
  const r=await J('/api/layout',{layout:b.dataset.ly});
  if(r.ok){layout=b.dataset.ly==='mono10'?1:0;toast('Distribución aplicada');log('Distribución: '+LYN[layout]);closeDrawer();sig='';loadStatus();loadNodes();}else toast('No se pudo aplicar',false);});

/* Sonido */
async function setDsp(o){const r=await J('/api/dsp',o);
  if(r.ok){if(o.xover!==undefined)st.dsp_xover=o.xover;if(o.eq!==undefined)st.dsp_eq=o.eq;
    log('Procesado: crossover '+(st.dsp_xover?'sí':'no')+', EQ '+(st.dsp_eq?'sí':'no'));drawSound();if($('d_eqh'))eqHint();toast(!st.dsp_xover&&!st.dsp_eq?'Pass-through activo':'Procesado actualizado');}
  else{toast('No se pudo cambiar',false);}loadStatus();}
function subNode(){return nodes.find(n=>n.role===2&&n.enabled);}
function drawSound(){
  const xo=!!st.dsp_xover,eq=!!st.dsp_eq,pass=!xo&&!eq,sub=subNode();
  const nc=st.channels||10,on=nodes.filter(n=>n.enabled).length;
  const S=(t,s,bp,io)=>'<div class="fs'+(bp?' bp':'')+(io?' io':'')+'"><b>'+t+'</b><span>'+s+'</span></div>';
  $('flow').innerHTML=S('Entrada USB',nc+' canales',false,true)+'<span class="arr">›</span>'+
    S('Crossover',xo?(sub?'corte '+Math.round(st.xover_hz||80)+' Hz':'sin sub: inactivo'):'omitido',!xo||!sub)+'<span class="arr">›</span>'+
    S('Ecualizador',eq?'por altavoz':'omitido',!eq)+'<span class="arr">›</span>'+
    S('Volumen y retardo','por altavoz',false)+'<span class="arr">›</span>'+S('Altavoces',on+' activos',false,true);
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
$('t_proc').onclick=()=>{if(st.dsp_xover||st.dsp_eq)return;setDsp({xover:true,eq:true});};
$('sw_xo').onclick=()=>setDsp({xover:$('sw_xo').getAttribute('aria-checked')!=='true'});
$('sw_eq').onclick=()=>setDsp({eq:$('sw_eq').getAttribute('aria-checked')!=='true'});
$('xos').oninput=function(){$('xod').textContent=this.value+' Hz';fillRng(this);};
$('xos').onchange=async function(){const r=await J('/api/bass',{xover_hz:+this.value});if(r.ok){toast('Corte a '+this.value+' Hz');log('Corte de graves: '+this.value+' Hz');loadStatus();}else toast('No se pudo guardar',false);};
$('subsel').onchange=async function(){const sel=this.value;
  for(const n of nodes){const role=sel==='-'?0:(String(n.id)===sel?2:(n.enabled?1:0));if(role!==n.role)await J('/api/node',{id:n.id,role});}
  toast(sel==='-'?'Todos a rango completo':'Subwoofer: altavoz #'+sel);log(sel==='-'?'Todos a rango completo':'Subwoofer = #'+sel);sig='';loadNodes();};

/* Latencia */
async function updLat(){try{const j=await (await fetch('/api/latency')).json();
  $('bigtot').innerHTML=Math.round(j.total_ms)+'<small>ms</small>';$('bigad').innerHTML=Math.round(j.audio_delay_ms)+'<small>ms</small>';
  $('mpvcmd').textContent='mpv --audio-delay='+j.mpv_audio_delay_s.toFixed(3)+' vídeo.mkv';}catch(e){}}
async function setPL(v){const r=await J('/api/playout',{ms:+v});if(r.ok){toast('Colchón '+v+' ms');log('Colchón de red: '+v+' ms');loadStatus();updLat();}else toast('No se pudo cambiar',false);}
$('pls').oninput=function(){$('pld').textContent=this.value+' ms';fillRng(this);};
$('pls').onchange=function(){setPL(this.value);};
$('plseg').querySelectorAll('button').forEach(b=>b.onclick=()=>setPL(b.dataset.pl));
$('lex').onchange=async function(){const r=await J('/api/latency',{extra_ms:+this.value});if(r.ok){toast('Ajuste extra '+this.value+' ms');updLat();}else toast('Valor fuera de rango (−50 a 200)',false);};
function cls(v,w,b){return v>=b?' class="bad"':v>=w?' class="warn"':'';}
function drawHealth(){const vis=nodes.filter(n=>n.enabled||n.online);
  $('health').innerHTML=vis.map(n=>'<tr><td><i class="dot '+(n.online?'ok':'bad')+'"></i> #'+n.id+' · '+nodeLabel(n)+'</td><td>'+(n.online?((n.rtt_us||0)/1000).toFixed(1)+' ms':'—')+'</td><td>'+(n.jitter_us||0)+' µs</td><td'+cls(n.lost_packets||0,1,50)+'>'+(n.lost_packets||0)+'</td><td>'+(n.recovered_packets||0)+'</td><td'+cls(n.late_packets||0,5,50)+'>'+(n.late_packets||0)+'</td><td'+cls(n.underruns||0,1,10)+'>'+(n.underruns||0)+'</td><td>'+((n.sync_err_us||0))+' µs</td><td>'+Math.round(n.fill_pct||0)+'%</td></tr>').join('')||'<tr><td colspan="9" class="mut">Sin altavoces</td></tr>';}

/* Añadir altavoces por Bluetooth desde la Raspberry */
let bs={available:false,ssid:'',has_pass:false,rpi_ip:''},bdevs=[],bscan=false,bres={};
const rssiTxt=r=>r==null?'':r>-60?'Señal fuerte':r>-75?'Señal media':'Señal débil';
function openBle(){closeDrawer();$('bdrawer').classList.add('on');$('scrim').classList.add('on');$('bdrawer').setAttribute('aria-hidden','false');
  fetch('/api/ble/status').then(r=>r.json()).then(j=>{bs=j;bleBuild();}).catch(()=>{bleBuild();});bleBuild();}
function closeBle(){const d=$('bdrawer');if(!d.classList.contains('on'))return;d.classList.remove('on');d.setAttribute('aria-hidden','true');if(openId===null)$('scrim').classList.remove('on');}
$('lble').onclick=openBle;$('bclose').onclick=closeBle;
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
  else{bdevs=[];toast(r.error||'No se pudo buscar',false);log('Bluetooth: '+(r.error||'error'));}
  bleBuild();if(r.error&&$('b_hint')){$('b_hint').textContent=r.error;$('b_hint').style.color='var(--bad)';}}
async function bleIdent(i){const d=bdevs[i];const r=await J('/api/ble/identify',{address:d.address});if(r.ok)toast('Sonando en '+d.name);else toast(r.error||'No se pudo',false);}
async function bleProvOne(i){const d=bdevs[i],sel=document.querySelector('[data-sel="'+i+'"]'),id=sel?+sel.value:d.suggested_id;
  const own=!d.new&&d.id===id;
  const el=$('b_r'+i);if(el){el.className='res';el.innerHTML='<span class="spin"></span>Configurando…';}
  const r=await J('/api/ble/provision',{address:d.address,id,force:own});
  bres[d.address]=r.ok?{ok:true}:{err:r.error||'No se pudo configurar'};
  if(r.ok){log('Bluetooth: '+d.name+' configurado como #'+id);setTimeout(()=>{sig='';loadNodes();},6000);}else log('Bluetooth: '+d.name+': '+bres[d.address].err);
  return !!r.ok;}
async function bleProv(i){const okk=await bleProvOne(i);if(okk)toast('Enviado a '+bdevs[i].name);else toast('No se pudo configurar',false);bleBuild();}
async function bleProvAll(){const todo=[];bdevs.forEach((d,i)=>{if(d.new&&d.status&&d.suggested_id!=null&&!(bres[d.address]||{}).ok)todo.push(i);});
  if(!todo.length)return;
  const msg='Se enviará la WiFi «'+bs.ssid+'» a:\n'+todo.map(i=>'  '+bdevs[i].name+' → número '+(document.querySelector('[data-sel="'+i+'"]')||{value:bdevs[i].suggested_id}).value).join('\n')+'\n\n¿Continuar?';
  if(!confirm(msg))return;
  let n=0;for(const i of todo){if(await bleProvOne(i))n++;}toast(n+' de '+todo.length+' enviados',n===todo.length);bleBuild();}

/* Volumen maestro y enlaces */
$('mvs').oninput=function(){$('mvd').textContent=this.value+'%';fillRng(this);clearTimeout(window._mv);window._mv=setTimeout(()=>J('/api/volume',{volume:+this.value/100}),120);};
$('logclr').onclick=()=>{$('lb').innerHTML='';};
(function(){const b=location.protocol==='https:'?'':'https://'+location.hostname+':8443';
  $('lcal').href=b+'/calibrate';$('lcal2').href=b+'/calibrate';window._bleHref=b+'/ble';})();
document.querySelectorAll('.rng').forEach(fillRng);
const h0=(location.hash||'').slice(1);if(['spk','snd','lat','log'].includes(h0))showTab(h0);
loadStatus().then(loadNodes);updLat();
setInterval(loadStatus,5000);setInterval(loadNodes,2500);
</script>
</body></html>)HTML";

static const char PAGE_CAL[] = R"HTML(<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#000000">
<title>SonosWiFi — Calibración</title>
<style>
/* Sistema de diseño compartido por las tres páginas (panel, /calibrate, /ble). Negro puro + azul señal. */
:root{
  --bg:#000; --panel:#0a0d13; --raise:#121722; --line:#1f2735; --line2:#2b3548;
  --fg:#e9edf5; --mut:#8a93a6; --ac:#4aa3ff; --ac-bg:rgba(74,163,255,.13); --ac-ln:rgba(74,163,255,.5);
  --ok:#3ecf8e; --warn:#f5b84a; --bad:#ff6b6b;
  --font:"Segoe UI Variable Text","Segoe UI",system-ui,-apple-system,"Helvetica Neue",Roboto,sans-serif;
}
*{box-sizing:border-box;margin:0}
html{color-scheme:dark}
body{background:var(--bg);color:var(--fg);font:15px/1.45 var(--font);-webkit-font-smoothing:antialiased;
  font-variant-numeric:tabular-nums;min-height:100vh;-webkit-text-size-adjust:100%}
a{color:var(--ac);text-decoration:none}a:hover{text-decoration:underline}
button,input,select{font:inherit;color:inherit}
button{cursor:pointer}
[hidden]{display:none!important}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px;border-radius:6px}
h1{font-size:1.55rem;font-weight:650;letter-spacing:-.01em}
h2{font-size:1.02rem;font-weight:650}
.mut{color:var(--mut)}
.small{font-size:.86rem}

.btn{appearance:none;border:0;background:var(--ac);color:#04121f;font-weight:650;padding:10px 18px;border-radius:10px;min-height:42px;
  transition:filter .12s,transform .06s}
.btn:hover{filter:brightness(1.1)}.btn:active{transform:translateY(1px)}
.btn.ghost{background:var(--raise);color:var(--fg);border:1px solid var(--line2);font-weight:550}
.btn.danger{background:transparent;color:var(--bad);border:1px solid rgba(255,107,107,.45);font-weight:550}
.btn.sm{padding:6px 12px;min-height:34px;font-size:.88rem;border-radius:8px}
.btn:disabled{opacity:.4;cursor:not-allowed;filter:none}

.seg{display:inline-flex;background:var(--raise);border:1px solid var(--line);border-radius:11px;padding:3px;gap:2px;max-width:100%}
.seg button{appearance:none;border:0;background:transparent;color:var(--mut);padding:7px 14px;border-radius:8px;min-height:34px;font-weight:550;white-space:nowrap}
.seg button:hover{color:var(--fg)}
.seg button[aria-pressed=true]{background:var(--ac-bg);color:var(--ac);box-shadow:inset 0 0 0 1px var(--ac-ln)}
.seg.block{display:flex}.seg.block button{flex:1}

.switch{appearance:none;position:relative;width:46px;height:27px;border-radius:14px;border:1px solid var(--line2);background:var(--raise);flex:none;
  transition:background .15s,border-color .15s}
.switch::after{content:"";position:absolute;top:2px;left:2px;width:21px;height:21px;border-radius:50%;background:var(--mut);transition:transform .15s,background .15s}
.switch[aria-checked=true]{background:var(--ac-bg);border-color:var(--ac-ln)}
.switch[aria-checked=true]::after{transform:translateX(19px);background:var(--ac)}

input[type=range].rng{-webkit-appearance:none;appearance:none;width:100%;height:28px;background:transparent;border-radius:0;cursor:pointer;
  --f:.5;
  background:linear-gradient(to right,var(--ac) calc(var(--f)*(100% - 20px) + 10px),var(--line2) 0) no-repeat center/100% 6px}
input[type=range].rng.mid{--lo:.5;--hi:.5;
  background:linear-gradient(to right,var(--line2) calc(var(--lo)*(100% - 20px) + 10px),var(--ac) 0,var(--ac) calc(var(--hi)*(100% - 20px) + 10px),var(--line2) 0) no-repeat center/100% 6px}
.rng::-webkit-slider-runnable-track{height:28px;background:transparent}
.rng::-moz-range-track{height:28px;background:transparent}
.rng::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#fff;border:0;margin-top:4px;box-shadow:0 0 0 3px rgba(74,163,255,.38)}
.rng::-moz-range-thumb{width:20px;height:20px;border-radius:50%;background:#fff;border:0;box-shadow:0 0 0 3px rgba(74,163,255,.38)}
.rng:focus-visible{outline-offset:0}

.num,.txt,.sel{background:var(--raise);border:1px solid var(--line2);border-radius:9px;padding:9px 11px;min-height:40px;width:100%;color:var(--fg)}
.num{width:110px}
.num:focus,.txt:focus,.sel:focus{outline:2px solid var(--ac);outline-offset:1px;border-color:transparent}
.sel{appearance:none;background-image:linear-gradient(45deg,transparent 50%,var(--mut) 50%),linear-gradient(135deg,var(--mut) 50%,transparent 50%);
  background-position:calc(100% - 17px) 17px,calc(100% - 12px) 17px;background-size:5px 5px,5px 5px;background-repeat:no-repeat;padding-right:32px}
label,.lbl{display:block;color:var(--mut);font-size:.86rem;margin-bottom:5px}

.chip{display:inline-flex;align-items:center;gap:7px;padding:4px 11px;border-radius:999px;background:var(--raise);border:1px solid var(--line);font-size:.84rem;color:var(--mut)}
.chip.ac{color:var(--ac);border-color:var(--ac-ln);background:var(--ac-bg)}
.chip.warn{color:var(--warn);border-color:rgba(245,184,74,.4)}
.chip.bad{color:var(--bad);border-color:rgba(255,107,107,.4)}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--mut);flex:none}
.dot.ok{background:var(--ok)}.dot.bad{background:var(--bad)}.dot.warn{background:var(--warn)}

.top{position:sticky;top:0;z-index:30;background:rgba(0,0,0,.88);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);border-bottom:1px solid var(--line)}
.top .in{max-width:1180px;margin:auto;display:flex;align-items:center;gap:18px;padding:10px 18px;flex-wrap:wrap}
.brand{display:flex;align-items:center;gap:10px;font-weight:700;font-size:1.05rem;color:var(--fg)}
.brand:hover{text-decoration:none}
.logo{width:26px;height:26px;flex:none}
.panelbox{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:18px}
.note{border-radius:12px;padding:12px 14px;background:var(--raise);border:1px solid var(--line2);font-size:.92rem}
.note.bad{border-color:rgba(255,107,107,.5);color:#ffb0b0}.note.warn{border-color:rgba(245,184,74,.5)}

.toasts{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);display:flex;flex-direction:column;gap:6px;z-index:60;pointer-events:none;width:max-content;max-width:92vw}
.toast{background:var(--raise);border:1px solid var(--line2);border-left:3px solid var(--ok);padding:9px 14px;border-radius:10px;font-size:.9rem;
  animation:tin .18s ease-out}
.toast.bad{border-left-color:var(--bad)}
@keyframes tin{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
main{max-width:760px;margin:0 auto;padding:20px 16px 80px;display:flex;flex-direction:column;gap:16px}
.top .in{max-width:760px}
.steps{display:flex;gap:8px;align-items:center;color:var(--mut);font-size:.9rem}
.steps i{flex:1;height:2px;background:var(--line);border-radius:2px}
.steps span{display:flex;align-items:center;gap:8px;white-space:nowrap}
.steps b{width:24px;height:24px;border-radius:50%;border:1.5px solid var(--line2);display:grid;place-items:center;font-size:.8rem}
.steps .on{color:var(--fg)}.steps .on b{border-color:var(--ac);color:var(--ac)}
.steps .done b{background:var(--ac);border-color:var(--ac);color:#04121f}
.opts{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.meter{height:10px;background:var(--raise);border-radius:5px;overflow:hidden;border:1px solid var(--line)}
.meter>div{height:100%;width:0;background:linear-gradient(90deg,var(--ac),#9ad0ff);transition:width .08s}
.prog{height:4px;background:var(--line);border-radius:2px;overflow:hidden}.prog>div{height:100%;width:0;background:var(--ac);transition:width .25s linear}
.count{font-size:2.6rem;font-weight:700;letter-spacing:-.02em;line-height:1}
.node{border:1px solid var(--line);background:var(--panel);border-radius:16px;padding:14px 16px;display:flex;flex-direction:column;gap:12px}
.node.bad{border-color:rgba(255,107,107,.45)}
.node h3{font-size:1rem;display:flex;align-items:center;gap:10px}
.node h3 .chip{margin-left:auto}
.mets{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.mets div{background:var(--raise);border-radius:10px;padding:8px 10px}
.mets span{display:block;font-size:.76rem;color:var(--mut)}.mets b{font-weight:600}
.eqm{display:flex;align-items:stretch;gap:6px;height:64px}
.eqm div{flex:1;position:relative;background:var(--raise);border-radius:6px}
.eqm div::before{content:"";position:absolute;left:0;right:0;top:50%;height:1px;background:var(--line2)}
.eqm i{position:absolute;left:18%;right:18%;border-radius:3px;background:var(--ac)}
.eqm i.neg{background:#2f6fb3}
.eqm em{position:absolute;left:0;right:0;bottom:-18px;text-align:center;font-style:normal;font-size:.7rem;color:var(--mut)}
.eqw{padding-bottom:20px}
.tg{display:flex;gap:18px;flex-wrap:wrap}
.tg label.ck{display:flex;align-items:center;gap:8px;margin:0;color:var(--fg);font-size:.9rem}
input[type=checkbox]{appearance:none;width:20px;height:20px;border-radius:6px;border:1.5px solid var(--line2);background:var(--raise);display:inline-grid;place-items:center;flex:none;cursor:pointer}
input[type=checkbox]:checked{background:var(--ac);border-color:var(--ac)}
input[type=checkbox]:checked::after{content:"";width:5px;height:10px;border:solid #04121f;border-width:0 2.5px 2.5px 0;transform:rotate(45deg) translate(-1px,-1px)}
.applybox{display:flex;flex-direction:column;gap:12px}
.log{font:.82rem/1.6 ui-monospace,Consolas,monospace;color:var(--mut);white-space:pre-wrap;max-height:150px;overflow:auto}
@media (max-width:560px){.opts{grid-template-columns:1fr}.mets{grid-template-columns:1fr 1fr}.steps span em{display:none}}
</style></head><body>
<div class="top"><div class="in">
  <a class="brand" href="/"><svg class="logo" viewBox="0 0 26 26" fill="none" stroke="#4aa3ff" stroke-width="2" stroke-linecap="round"><path d="M3 13h2M8 8v10M13 4v18M18 8v10M23 13h-2"/></svg>SonosWiFi</a>
  <a class="btn ghost sm" style="margin-left:auto" href="/">Volver al panel</a>
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
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#000000">
<title>SonosWiFi — Añadir altavoz</title>
<style>
/* Sistema de diseño compartido por las tres páginas (panel, /calibrate, /ble). Negro puro + azul señal. */
:root{
  --bg:#000; --panel:#0a0d13; --raise:#121722; --line:#1f2735; --line2:#2b3548;
  --fg:#e9edf5; --mut:#8a93a6; --ac:#4aa3ff; --ac-bg:rgba(74,163,255,.13); --ac-ln:rgba(74,163,255,.5);
  --ok:#3ecf8e; --warn:#f5b84a; --bad:#ff6b6b;
  --font:"Segoe UI Variable Text","Segoe UI",system-ui,-apple-system,"Helvetica Neue",Roboto,sans-serif;
}
*{box-sizing:border-box;margin:0}
html{color-scheme:dark}
body{background:var(--bg);color:var(--fg);font:15px/1.45 var(--font);-webkit-font-smoothing:antialiased;
  font-variant-numeric:tabular-nums;min-height:100vh;-webkit-text-size-adjust:100%}
a{color:var(--ac);text-decoration:none}a:hover{text-decoration:underline}
button,input,select{font:inherit;color:inherit}
button{cursor:pointer}
[hidden]{display:none!important}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px;border-radius:6px}
h1{font-size:1.55rem;font-weight:650;letter-spacing:-.01em}
h2{font-size:1.02rem;font-weight:650}
.mut{color:var(--mut)}
.small{font-size:.86rem}

.btn{appearance:none;border:0;background:var(--ac);color:#04121f;font-weight:650;padding:10px 18px;border-radius:10px;min-height:42px;
  transition:filter .12s,transform .06s}
.btn:hover{filter:brightness(1.1)}.btn:active{transform:translateY(1px)}
.btn.ghost{background:var(--raise);color:var(--fg);border:1px solid var(--line2);font-weight:550}
.btn.danger{background:transparent;color:var(--bad);border:1px solid rgba(255,107,107,.45);font-weight:550}
.btn.sm{padding:6px 12px;min-height:34px;font-size:.88rem;border-radius:8px}
.btn:disabled{opacity:.4;cursor:not-allowed;filter:none}

.seg{display:inline-flex;background:var(--raise);border:1px solid var(--line);border-radius:11px;padding:3px;gap:2px;max-width:100%}
.seg button{appearance:none;border:0;background:transparent;color:var(--mut);padding:7px 14px;border-radius:8px;min-height:34px;font-weight:550;white-space:nowrap}
.seg button:hover{color:var(--fg)}
.seg button[aria-pressed=true]{background:var(--ac-bg);color:var(--ac);box-shadow:inset 0 0 0 1px var(--ac-ln)}
.seg.block{display:flex}.seg.block button{flex:1}

.switch{appearance:none;position:relative;width:46px;height:27px;border-radius:14px;border:1px solid var(--line2);background:var(--raise);flex:none;
  transition:background .15s,border-color .15s}
.switch::after{content:"";position:absolute;top:2px;left:2px;width:21px;height:21px;border-radius:50%;background:var(--mut);transition:transform .15s,background .15s}
.switch[aria-checked=true]{background:var(--ac-bg);border-color:var(--ac-ln)}
.switch[aria-checked=true]::after{transform:translateX(19px);background:var(--ac)}

input[type=range].rng{-webkit-appearance:none;appearance:none;width:100%;height:28px;background:transparent;border-radius:0;cursor:pointer;
  --f:.5;
  background:linear-gradient(to right,var(--ac) calc(var(--f)*(100% - 20px) + 10px),var(--line2) 0) no-repeat center/100% 6px}
input[type=range].rng.mid{--lo:.5;--hi:.5;
  background:linear-gradient(to right,var(--line2) calc(var(--lo)*(100% - 20px) + 10px),var(--ac) 0,var(--ac) calc(var(--hi)*(100% - 20px) + 10px),var(--line2) 0) no-repeat center/100% 6px}
.rng::-webkit-slider-runnable-track{height:28px;background:transparent}
.rng::-moz-range-track{height:28px;background:transparent}
.rng::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#fff;border:0;margin-top:4px;box-shadow:0 0 0 3px rgba(74,163,255,.38)}
.rng::-moz-range-thumb{width:20px;height:20px;border-radius:50%;background:#fff;border:0;box-shadow:0 0 0 3px rgba(74,163,255,.38)}
.rng:focus-visible{outline-offset:0}

.num,.txt,.sel{background:var(--raise);border:1px solid var(--line2);border-radius:9px;padding:9px 11px;min-height:40px;width:100%;color:var(--fg)}
.num{width:110px}
.num:focus,.txt:focus,.sel:focus{outline:2px solid var(--ac);outline-offset:1px;border-color:transparent}
.sel{appearance:none;background-image:linear-gradient(45deg,transparent 50%,var(--mut) 50%),linear-gradient(135deg,var(--mut) 50%,transparent 50%);
  background-position:calc(100% - 17px) 17px,calc(100% - 12px) 17px;background-size:5px 5px,5px 5px;background-repeat:no-repeat;padding-right:32px}
label,.lbl{display:block;color:var(--mut);font-size:.86rem;margin-bottom:5px}

.chip{display:inline-flex;align-items:center;gap:7px;padding:4px 11px;border-radius:999px;background:var(--raise);border:1px solid var(--line);font-size:.84rem;color:var(--mut)}
.chip.ac{color:var(--ac);border-color:var(--ac-ln);background:var(--ac-bg)}
.chip.warn{color:var(--warn);border-color:rgba(245,184,74,.4)}
.chip.bad{color:var(--bad);border-color:rgba(255,107,107,.4)}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--mut);flex:none}
.dot.ok{background:var(--ok)}.dot.bad{background:var(--bad)}.dot.warn{background:var(--warn)}

.top{position:sticky;top:0;z-index:30;background:rgba(0,0,0,.88);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);border-bottom:1px solid var(--line)}
.top .in{max-width:1180px;margin:auto;display:flex;align-items:center;gap:18px;padding:10px 18px;flex-wrap:wrap}
.brand{display:flex;align-items:center;gap:10px;font-weight:700;font-size:1.05rem;color:var(--fg)}
.brand:hover{text-decoration:none}
.logo{width:26px;height:26px;flex:none}
.panelbox{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:18px}
.note{border-radius:12px;padding:12px 14px;background:var(--raise);border:1px solid var(--line2);font-size:.92rem}
.note.bad{border-color:rgba(255,107,107,.5);color:#ffb0b0}.note.warn{border-color:rgba(245,184,74,.5)}

.toasts{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);display:flex;flex-direction:column;gap:6px;z-index:60;pointer-events:none;width:max-content;max-width:92vw}
.toast{background:var(--raise);border:1px solid var(--line2);border-left:3px solid var(--ok);padding:9px 14px;border-radius:10px;font-size:.9rem;
  animation:tin .18s ease-out}
.toast.bad{border-left-color:var(--bad)}
@keyframes tin{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
main{max-width:600px;margin:0 auto;padding:20px 16px 80px;display:flex;flex-direction:column;gap:16px}
.top .in{max-width:600px}
.step{display:flex;gap:14px}
.num2{flex:none;width:30px;height:30px;border-radius:50%;border:1.5px solid var(--line2);display:grid;place-items:center;font-weight:700;color:var(--mut)}
.step.cur .num2{border-color:var(--ac);color:var(--ac)}
.step.done .num2{background:var(--ac);border-color:var(--ac);color:#04121f}
.step .bd{flex:1;min-width:0;display:flex;flex-direction:column;gap:12px}
.step.lock .bd{opacity:.4;pointer-events:none}
.pills{display:grid;grid-template-columns:repeat(5,1fr);gap:8px}
.pills button{appearance:none;background:var(--raise);border:1px solid var(--line2);border-radius:12px;min-height:46px;font-weight:650;font-size:1.05rem}
.pills button:hover{border-color:var(--ac-ln)}
.pills button[aria-pressed=true]{background:var(--ac-bg);border-color:var(--ac);color:var(--ac)}
.kv{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.kv div{background:var(--raise);border:1px solid var(--line);border-radius:12px;padding:10px 12px}
.kv span{display:block;font-size:.8rem;color:var(--mut)}.kv b{font-weight:600}
.pw{position:relative}.pw button{position:absolute;right:6px;top:5px}
details.adv{border:1px solid var(--line);border-radius:14px;padding:4px 14px;background:var(--panel)}
details.adv summary{cursor:pointer;padding:10px 0;font-weight:600;color:var(--mut)}
details.adv[open] summary{color:var(--fg)}
details.adv .in2{padding:4px 0 14px;display:flex;flex-direction:column;gap:12px}
.row{display:flex;gap:8px;flex-wrap:wrap}
.log{font:.82rem/1.6 ui-monospace,Consolas,monospace;color:var(--mut);white-space:pre-wrap;max-height:150px;overflow:auto}
@media (max-width:420px){.kv{grid-template-columns:1fr}}
</style></head><body>
<div class="top"><div class="in">
  <a class="brand" href="/"><svg class="logo" viewBox="0 0 26 26" fill="none" stroke="#4aa3ff" stroke-width="2" stroke-linecap="round"><path d="M3 13h2M8 8v10M13 4v18M18 8v10M23 13h-2"/></svg>SonosWiFi</a>
  <a class="btn ghost sm" style="margin-left:auto" href="/">Volver al panel</a>
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
