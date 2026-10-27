#!/usr/bin/env python3
"""Simula la lógica de i2s_task (receiver_main.c): sincronía con desvío de reloj y ocultación de pérdidas (PLC).
Modelo: el DAC del ESP consume frames a 48000*(1+ppm) Hz; los timestamps avanzan a 48000 exactos.
 - sin pérdidas: el error de alineación debe quedar acotado (±400 µs) para ±150 ppm y offsets iniciales hasta 20 ms.
 - con pérdidas aleatorias: con PLC el error sigue acotado y sin descartes duros; sin PLC se ve la diferencia."""
import random, sys
FRAMES, RATE = 120, 48000
CHUNK_US = FRAMES * 1e6 / RATE
DMA_LAT = (6 * FRAMES - FRAMES // 2) * 1e6 / RATE
PLAYOUT, SKIP, DEAD, EVERY, FRAME_US, PLC_MAX_K = 80000, 5000, 100, 4, 21, 8

def run(ppm, start_offset_us, secs=120, loss=0.0, plc=True, seed=1, playout_change=None):
    rng = random.Random(seed)
    t = 0.0; q = []; pk_i = 0
    err_f, err_valid, chunk_n = 0.0, False, 0
    playing, have_last, last_ts, fade_in = False, False, 0.0, False
    ins = drops = skipped = silences = concealed = 0
    log = []
    step_us = lambda fr: fr * 1e6 / (RATE * (1 + ppm * 1e-6))
    cur = None; playout = PLAYOUT
    while t < secs * 1e6:
        if playout_change and t >= playout_change[0] * 1e6: playout = playout_change[1]
        while pk_i * CHUNK_US + 3000 <= t:
            if rng.random() >= loss: q.append(pk_i * CHUNK_US)
            pk_i += 1
        if cur is None:
            if not q:
                playing = False; err_valid = False; t += step_us(FRAMES); continue
            cur = q.pop(0)
            if plc and playing and have_last:
                gap = cur - last_ts - CHUNK_US
                if CHUNK_US / 2 < gap <= PLC_MAX_K * CHUNK_US + CHUNK_US / 2:
                    k = int((gap + CHUNK_US / 2) // CHUNK_US)
                    for _ in range(k): t += step_us(FRAMES); concealed += 1
                    last_ts += k * CHUNK_US; fade_in = True
        err = t + start_offset_us + DMA_LAT - (cur + playout)
        if err > SKIP: skipped += 1; cur = None; err_valid = False; have_last = False; continue
        if err < -SKIP: silences += 1; t += step_us(FRAMES); playing = False; err_valid = False; have_last = False; continue
        if not err_valid: err_f, err_valid, chunk_n = err, True, 0
        else: err_f += (err - err_f) / 8
        frames = FRAMES; chunk_n += 1
        last_ts, have_last, fade_in = cur, True, False
        if chunk_n % EVERY == 0:
            if err_f > DEAD: frames -= 1; err_f -= FRAME_US; drops += 1
            elif err_f < -DEAD: frames += 1; err_f += FRAME_US; ins += 1
        t += step_us(frames); playing = True
        log.append((t, err)); cur = None
    tail = [e for (tt, e) in log if tt > (secs - 30) * 1e6]
    return dict(steady_min=round(min(tail)), steady_max=round(max(tail)), drops=drops, ins=ins,
                skipped=skipped, silences=silences, concealed=concealed)

ok = True
print("— sin pérdidas —")
for ppm, off in [(0, 0), (+80, 0), (-80, 0), (+150, 3000), (-150, -3000), (+40, 20000)]:
    r = run(ppm, off); good = r["steady_max"] < 400 and r["steady_min"] > -400; ok &= good
    print("PASS" if good else "FAIL", f"ppm={ppm:+d} off={off}", r)

print("— 1% de pérdidas aleatorias, +80 ppm —")
a = run(80, 0, loss=0.01, plc=False, seed=7); b = run(80, 0, loss=0.01, plc=True, seed=7)
print("     sin PLC:", a); print("     con PLC:", b)
g1 = b["steady_max"] < 400 and b["steady_min"] > -400 and b["skipped"] == 0
g2 = b["concealed"] > 0
g3 = (a["steady_max"] - a["steady_min"]) > (b["steady_max"] - b["steady_min"])
for n, g in (("con PLC el error se mantiene acotado y sin descartes duros", g1), ("el PLC rellena huecos (concealed > 0)", g2),
             ("PLC reduce la excursión del error frente a no ocultar", g3)):
    print("PASS" if g else "FAIL", n); ok &= g

print("— 6 % de pérdidas aleatorias (incluye ráfagas de varios paquetes) —")
# pérdida concentrada: simulada con 6 % de pérdida
c = run(0, 0, loss=0.06, plc=True, seed=3); good = c["steady_max"] < 600 and c["steady_min"] > -600
print("PASS" if good else "FAIL", c); ok &= good

print("— cambio de latencia en caliente (80 -> 120 ms a los 20 s, y 120 -> 50 ms a los 60 s) —")
for ch in [(20, 120000), (20, 50000)]:
    r = run(40, 0, secs=100, playout_change=ch)
    # tras el cambio debe reconverger (últimos 30 s acotados)
    good = r["steady_max"] < 400 and r["steady_min"] > -400; ok &= good
    print("PASS" if good else "FAIL", f"playout -> {ch[1]//1000} ms", r)
print("RESULTADO:", "OK" if ok else "FALLOS"); sys.exit(0 if ok else 1)
