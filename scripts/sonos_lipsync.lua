-- sonos_lipsync.lua — script de mpv: ajusta SOLO el desfase de audio al retardo actual de SonosWiFi.
-- El audio llega a los altavoces `playout` ms después que la imagen; este script pone en mpv un
-- audio-delay negativo de ese valor para que el audio se reproduzca ese tiempo ANTES (lip-sync).
-- Se actualiza al abrir cada vídeo y cada `interval` s (si cambias la latencia en el panel, se sigue sola).
--
-- Instalación: copiar este archivo a  ~/.config/mpv/scripts/   (Windows: %APPDATA%\mpv\scripts\)
-- Configuración (en mpv.conf o en la línea de comandos):
--   script-opts=sonos_lipsync-host=192.168.1.10,sonos_lipsync-port=8080
-- Opciones: host, port (HTTP del panel), interval (s), user/pass (si pusiste contraseña a la web), https=yes|no.
-- Requiere `curl` en el PATH (viene con Windows 10+, macOS y Linux).
local mp      = require 'mp'
local utils   = require 'mp.utils'
local options = require 'mp.options'
local msg     = require 'mp.msg'

local o = { host = "192.168.1.10", port = 8080, interval = 10, user = "sonos", pass = "", https = false }
options.read_options(o, "sonos_lipsync")

local last = nil

local function apply(delay_s)
    if last ~= nil and math.abs(last - delay_s) < 0.0005 then return end
    last = delay_s
    mp.set_property_number("audio-delay", delay_s)
    msg.info(string.format("audio-delay = %.0f ms", delay_s * 1000))
    mp.osd_message(string.format("SonosWiFi: audio-delay %.0f ms", delay_s * 1000), 2)
end

local function poll()
    local url = string.format("%s://%s:%d/api/latency", o.https and "https" or "http", o.host, o.port)
    local args = { "curl", "-s", "-m", "2" }
    if o.https then args[#args + 1] = "-k" end                       -- el certificado del panel es autofirmado
    if o.pass ~= "" then args[#args + 1] = "-u"; args[#args + 1] = o.user .. ":" .. o.pass end
    args[#args + 1] = url
    mp.command_native_async({ name = "subprocess", args = args, capture_stdout = true, playback_only = false },
        function(ok, res)
            if not ok or not res or res.status ~= 0 or not res.stdout or res.stdout == "" then
                msg.warn("no se pudo leer " .. url .. " (¿RPi apagada o IP incorrecta?)")
                return
            end
            local j = utils.parse_json(res.stdout)
            if j and type(j.mpv_audio_delay_s) == "number" then apply(j.mpv_audio_delay_s)
            else msg.warn("respuesta inesperada: " .. res.stdout) end
        end)
end

mp.register_event("file-loaded", poll)
mp.add_periodic_timer(o.interval, poll)
poll()
