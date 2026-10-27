// Ejecuta en Node el bloque ANALYSIS_BEGIN..END de la página /calibrate.
//  (1) Escenario sintético: 5 nodos × 2 rondas, sr=44100, retardos/ganancias/ruido/eco conocidos.
//  (2) Señal REAL generada por el código C++ de la RPi (capturada por fake_esp.py): comprueba que
//      el chirp de C++ y el de JavaScript son el mismo (correlación) y que la separación es 1400 ms.
const fs = require('fs');
const html = fs.readFileSync(__dirname + '/../rpi/include/web_pages.h', 'utf8');
const code = html.split('/*ANALYSIS_BEGIN*/')[1].split('/*ANALYSIS_END*/')[0];
const lib = new Function(code + '; return {makeChirp, analyze, propose};')();
let ok = true;
const check = (n, c, x = '') => { console.log((c ? 'PASS ' : 'FAIL ') + n + (x ? '  [' + x + ']' : '')); ok = ok && c; };

const P = { slot_ms: 1400, chirp_ms: 300, f0: 250, f1: 12000, fade_ms: 10 };
let seed = 12345; const rnd = () => (seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296;
const gauss = () => Math.sqrt(-2 * Math.log(rnd() + 1e-12)) * Math.cos(2 * Math.PI * rnd());

// ── (1) sintético ──
{
  const sr = 44100, ids = [0, 1, 2, 3, 4], seq = [...ids, ...ids];
  const trueD = [0, 3200, -1100, 7777.7, 15000];     // µs (llegada relativa respecto al nominal)
  const gainDb = [0, -6, -3, -9.5, -1];
  const total = 1.3 + 1.5 + seq.length * 1.4 + 0.6, N = Math.floor(total * sr);
  const rec = new Float32Array(N);
  for (let i = 0; i < N; i++) rec[i] = 0.002 * gauss();           // ruido ambiente
  const ch = lib.makeChirp(sr, P.f0, P.f1, P.chirp_ms, P.fade_ms);
  const base = 1.3 + 1.5 + 0.0213;                                 // t0 con fracción rara
  seq.forEach((id, k) => {
    const t = base + k * 1.4 + trueD[id] * 1e-6, g = 0.3 * Math.pow(10, gainDb[id] / 20);
    const s0 = Math.round(t * sr), frac = t * sr - s0;             // aprox: redondeo (error ≤ 11 µs)
    for (let i = 0; i < ch.length; i++) {
      rec[s0 + i] += g * ch[i];
      if (s0 + i + 441 < N) rec[s0 + i + 441] += 0.3 * g * ch[i];   // eco a 10 ms (−10 dB)
    }
  });
  const t0 = Date.now();
  const res = lib.analyze(rec, sr, Object.assign({ sequence: seq }, P));
  const ms = Date.now() - t0;
  check('analyze() 5 nodos × 2 rondas sin error', !res.error && res.nodes.length === 5, res.error || '');
  console.log('   tiempo de análisis en Node: ' + ms + ' ms');
  const refD = trueD[0];
  let maxErr = 0, maxLv = 0;
  const lv0 = res.nodes[0].level_db;
  for (const n of res.nodes) {
    const e = Math.abs(n.d_us - (trueD[n.id] - refD));
    maxErr = Math.max(maxErr, e);
    maxLv = Math.max(maxLv, Math.abs((n.level_db - lv0) - gainDb[n.id]));
  }
  check('retardos relativos con error < 40 µs', maxErr < 40, maxErr.toFixed(1) + ' µs');
  check('niveles relativos con error < 0.7 dB', maxLv < 0.7, maxLv.toFixed(2) + ' dB');
  check('todos los nodos con SNR suficiente', res.nodes.every(n => n.ok), res.nodes.map(n => n.snr.toFixed(0)).join(','));
  const ps = lib.propose(res.nodes, {});
  const dmax = Math.max(...trueD);
  const errLat = Math.max(...ps.map(n => Math.abs(n.latency_us - (dmax - trueD[n.id]))));
  check('latencias propuestas = dmax − d (error < 40 µs)', errLat < 40, errLat.toFixed(1) + ' µs');
  const lowest = ps.reduce((a, b) => (a.level_db < b.level_db ? a : b));
  check('volumen propuesto: el más bajo = 255, resto ≤ 255', lowest.volume === 255 && ps.every(n => n.volume <= 255));
  const p2 = lib.propose(res.nodes, { 3: false });
  check('nodo excluido del volumen queda en 255', p2.find(n => n.id === 3).volume === 255);
  // nodo mudo: debe marcarse como no válido
  const rec2 = rec.slice(); const k4 = [4, 9];
  k4.forEach(k => { const s0 = Math.round((base + k * 1.4 + trueD[4] * 1e-6) * sr); for (let i = 0; i < ch.length + 500; i++) rec2[s0 + i] = 0.002 * gauss(); });
  const r2 = lib.analyze(rec2, sr, Object.assign({ sequence: seq, expect_s: base - 0.2 }, P));   // el móvil sabe cuándo pidió el inicio
  check('nodo mudo: marcado ok=false', r2.nodes.find(n => n.id === 4).ok === false);
  check('nodo mudo: el resto conserva su ID y su retardo (error < 40 µs)',
    r2.nodes.filter(n => n.id !== 4).every(n => n.ok && Math.abs(n.d_us - (trueD[n.id] - trueD[0])) < 40),
    r2.nodes.map(n => n.id + ':' + n.d_us.toFixed(0) + (n.ok ? '' : '!')).join(' '));
  check('grabación corta -> error controlado', !!lib.analyze(new Float32Array(sr), sr, Object.assign({ sequence: seq }, P)).error);
}

// ── (1b) EQ por bandas: altavoces con «color» distinto (respuesta en frecuencia conocida) ──
{
  const sr = 48000, ids = [0, 1, 2, 3, 4], seq = [...ids, ...ids], EDGES = [250, 500, 1000, 2000, 4000, 8000, 12000];
  const gainDb = [0, -2, -3, -3, -1];
  const tilt = (id, f) => id === 1 ? (f >= 2000 ? -6 : 0) : id === 2 ? (f >= 500 && f < 1000 ? 5 : 0) : id === 4 ? (f < 500 ? -4 : 0) : 0;
  const trueBand = ids.map(id => EDGES.slice(0, -1).map((lo, b) => gainDb[id] + tilt(id, Math.sqrt(lo * EDGES[b + 1]))));
  const total = 1.3 + 1.5 + seq.length * 1.4 + 0.6, N = Math.floor(total * sr);
  const rec = new Float32Array(N);
  for (let i = 0; i < N; i++) rec[i] = 0.002 * gauss();
  const ch = lib.makeChirp(sr, P.f0, P.f1, P.chirp_ms, P.fade_ms), r = Math.log(P.f1 / P.f0), T = P.chirp_ms / 1000;
  const base = 1.3 + 1.5 + 0.02;
  seq.forEach((id, k) => {
    const s0 = Math.round((base + k * 1.4) * sr), g = 0.3 * Math.pow(10, gainDb[id] / 20);
    for (let i = 0; i < ch.length; i++) {
      const f = P.f0 * Math.exp((i / sr) * r / T), a = g * Math.pow(10, tilt(id, f) / 20) / Math.pow(10, gainDb[id] / 20);
      const v = 0.3 * Math.pow(10, gainDb[id] / 20) * Math.pow(10, tilt(id, f) / 20) * ch[i];
      rec[s0 + i] += v; if (s0 + i + 480 < N) rec[s0 + i + 480] += 0.3 * v;      // eco a 10 ms
    }
  });
  const res = lib.analyze(rec, sr, Object.assign({ sequence: seq, eq_edges: EDGES }, P));
  check('EQ: analyze() con eq_edges sin error y con 6 bandas por nodo', !res.error && res.nodes.every(n => n.bands && n.bands.length === 6 && n.band_ok.every(Boolean)), res.error || '');
  const refB = res.nodes[0].bands; let maxB = 0;
  for (const n of res.nodes) for (let b = 0; b < 6; b++) maxB = Math.max(maxB, Math.abs((n.bands[b] - refB[b]) - (trueBand[n.id][b] - trueBand[0][b])));
  check('EQ: niveles por banda relativos con error < 1 dB', maxB < 1.0, maxB.toFixed(2) + ' dB');
  const expect = (incl) => {
    const use = ids.filter(i => incl[i] !== false), R = EDGES.slice(0, -1).map((_, b) => use.reduce((a, i) => a + trueBand[i][b], 0) / use.length);
    return Object.fromEntries(use.map(i => { const d = R.map((x, b) => x - trueBand[i][b]), m = d.reduce((a, c) => a + c, 0) / 6;
      return [i, d.map(x => Math.max(-6, Math.min(6, x - m)))]; }));
  };
  const ps = lib.propose(res.nodes, {}, {}), ex = expect({});
  let maxE = 0; for (const n of ps) n.eq.forEach((g, b) => { maxE = Math.max(maxE, Math.abs(g - ex[n.id][b])); });
  check('EQ propuesto = lo esperado (error < 1 dB por banda)', maxE < 1.0, maxE.toFixed(2) + ' dB');
  const n1 = ps.find(n => n.id === 1);
  check('EQ: el altavoz con −6 dB en agudos recibe refuerzo en 2–12 kHz y recorte (o nada) abajo', n1.eq[3] > 2 && n1.eq[4] > 2 && n1.eq[5] > 2 && n1.eq[0] <= 0.5, n1.eq.join(','));
  check('EQ: la suma por altavoz es ≈ 0 (no cambia el nivel global)', ps.every(n => Math.abs(n.eq.reduce((a, c) => a + c, 0)) < 1.5));
  check('EQ: todo dentro de ±6 dB', ps.every(n => n.eq.every(g => Math.abs(g) <= 6.0001)));
  const p3 = lib.propose(res.nodes, {}, { 3: false }), ex3 = expect({ 3: false });
  let m3 = 0; for (const n of p3) if (n.id !== 3) n.eq.forEach((g, b) => { m3 = Math.max(m3, Math.abs(g - ex3[n.id][b])); });
  check('EQ: nodo excluido (sub) no tiene EQ y no cuenta para la referencia', p3.find(n => n.id === 3).eq === null && m3 < 1.0, m3.toFixed(2));
  const rn = lib.analyze(rec, sr, Object.assign({ sequence: seq }, P));
  check('sin eq_edges: bandas = null y EQ = null (compatible)', rn.nodes.every(n => n.bands === null) && lib.propose(rn.nodes, {}, {}).every(n => n.eq === null));
}

// ── (2) señal real de la RPi (C++) ──
if (fs.existsSync('/tmp/cal_wave.f32')) {
  const buf = fs.readFileSync('/tmp/cal_wave.f32');
  const w = new Float32Array(buf.buffer, buf.byteOffset, buf.length / 4);
  const sr = 48000, pad = Math.round(1.4 * sr);
  const rec = new Float32Array(w.length + pad);
  for (let i = 0; i < rec.length; i++) rec[i] = 0.0005 * gauss();
  for (let i = 0; i < w.length; i++) rec[pad + i] += w[i] / 32768;
  const res = lib.analyze(rec, sr, Object.assign({ sequence: [0, 0] }, P));
  check('señal real C++: detecta 2 chirps', !res.error && res.slots.length === 2, res.error || '');
  if (!res.error) {
    const dt = (res.slots[1].pos - res.slots[0].pos) / sr * 1e3;
    check('señal real C++: separación 1400 ms (±0.1 ms)', Math.abs(dt - 1400) < 0.1, dt.toFixed(3) + ' ms');
    check('señal real C++: SNR alto (chirp JS == chirp C++)', res.slots.every(s => s.snr > 50), res.slots.map(s => s.snr.toFixed(0)).join(','));
  }
} else console.log('SKIP señal real (ejecuta fake_esp.py antes)');

console.log('\nRESULTADO NODE:', ok ? 'TODO OK' : 'HAY FALLOS');
process.exit(ok ? 0 : 1);
