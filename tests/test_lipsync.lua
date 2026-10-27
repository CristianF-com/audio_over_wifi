-- Prueba el script de mpv con un «mpv» simulado y la API REAL del harness (http://127.0.0.1:18080).
local log, delay, osd = {}, nil, 0
local fake; fake = {
  mp = { set_property_number = function(k, v) if k == "audio-delay" then delay = v end end,
         osd_message = function() osd = osd + 1 end, register_event = function(e, f) fake.ev = f end,
         add_periodic_timer = function(s, f) fake.timer = f end,
         command_native_async = function(t, cb)
           local p = io.popen(table.concat(t.args, " ") .. " 2>/dev/null"); local out = p:read("*a"); local ok, _, code = p:close()
           cb(true, { status = code or 0, stdout = out }) end },
  utils = { parse_json = function(s) local v = s:match('"mpv_audio_delay_s"%s*:%s*(-?[%d%.eE+-]+)'); return v and { mpv_audio_delay_s = tonumber(v) } or nil end },
  options = { read_options = function(o) o.host = "127.0.0.1"; o.port = 18080 end },
  msg = { info = function() end, warn = function(m) log[#log + 1] = m end },
}
package.preload["mp"] = function() return fake.mp end
package.preload["mp.utils"] = function() return fake.utils end
package.preload["mp.options"] = function() return fake.options end
package.preload["mp.msg"] = function() return fake.msg end
dofile(arg[1])
local fails = 0
local function check(n, c) print((c and "PASS " or "FAIL ") .. n); if not c then fails = fails + 1 end end
check("sonos_lipsync.lua: al cargar fija audio-delay = -playout (−0.080 s)", delay ~= nil and math.abs(delay + 0.080) < 1e-6)
check("muestra el aviso en pantalla una vez", osd == 1)
fake.timer(); check("sin cambios no repite el aviso", osd == 1)
os.execute("curl -s -m 2 -X POST -H 'Content-Type: application/json' -d '{\"extra_ms\": 15}' http://127.0.0.1:18080/api/latency >/dev/null")
fake.ev(); check("tras cambiar extra_ms=15 en el panel: audio-delay = −0.095 s", math.abs(delay + 0.095) < 1e-6)
os.execute("curl -s -m 2 -X POST -H 'Content-Type: application/json' -d '{\"extra_ms\": 0}' http://127.0.0.1:18080/api/latency >/dev/null")
fake.timer(); check("el temporizador periódico vuelve a −0.080 s", math.abs(delay + 0.080) < 1e-6)
fake.options.read_options = function(o) o.host = "127.0.0.1"; o.port = 1 end
print(fails == 0 and "RESULTADO LIPSYNC: TODO OK" or "RESULTADO LIPSYNC: HAY FALLOS")
os.exit(fails == 0 and 0 or 1)
