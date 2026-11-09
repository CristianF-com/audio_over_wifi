#include "audio_capture.h"
#include "udp_streamer.h"
#include "web_server.h"
#include "input_mixer.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <sys/mman.h>

static std::atomic<bool> g_running{true};
static void sigHandler(int) { g_running.store(false); }

static void usage(const char* a0) {
    fprintf(stderr, "Uso: %s [-d hw:N,0|auto] [-p puerto_http] [-P frames_periodo]\n"
                    "         [-S puerto_https] [-C cert.pem] [-K key.pem]\n"
                    "         [-c fichero_config] [-W fichero_password] [-B ayudante_ble.py]\n"
                    "         [-x dispositivo:canales]   interfaz USB extra (repetible, p. ej. -x hw:2,0:8); también <config>.inputs\n"
                    "         [-w carpeta_web]   (desarrollo: sirve panel/calibrar/ble desde rpi/web sin recompilar)\n", a0);
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);
    std::signal(SIGPIPE, SIG_IGN);

    AudioConfig acfg;
    uint16_t web_port = 8080, tls_port = 8443;
    std::string cert = "/etc/sonos_wifi/cert.pem", key = "/etc/sonos_wifi/key.pem";
    std::string cfgfile = "/var/lib/sonos_wifi/config.txt", pwfile = "/etc/sonos_wifi/password";
    std::string webdir;
    std::string blehelper = "/usr/local/lib/sonos_wifi/sonos_ble_prov.py";

    std::vector<ExtraSpec> extras_cli;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-x") && i + 1 < argc) {
            std::string a = argv[++i]; const size_t c = a.rfind(':');
            if (c == std::string::npos || c == 0) { usage(argv[0]); return 2; }
            ExtraSpec e; e.device = a.substr(0, c); e.channels = (unsigned)strtoul(a.c_str() + c + 1, nullptr, 10);
            extras_cli.push_back(e);
        } else if (!strcmp(argv[i], "-d") && i + 1 < argc) acfg.device = argv[++i];
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
            long v = strtol(argv[++i], nullptr, 10);
            if (v < 1 || v > 65535) { usage(argv[0]); return 2; }
            web_port = (uint16_t)v;
        } else if (!strcmp(argv[i], "-P") && i + 1 < argc) {
            long v = strtol(argv[++i], nullptr, 10);
            if (v < 32 || v > 4800) { usage(argv[0]); return 2; }
            acfg.period_size = (unsigned)v;
        } else if (!strcmp(argv[i], "-S") && i + 1 < argc) {
            long v = strtol(argv[++i], nullptr, 10);
            if (v < 0 || v > 65535) { usage(argv[0]); return 2; }
            tls_port = (uint16_t)v;                       // 0 = sin HTTPS
        } else if (!strcmp(argv[i], "-C") && i + 1 < argc) cert = argv[++i];
        else if (!strcmp(argv[i], "-K") && i + 1 < argc)  key  = argv[++i];
        else if (!strcmp(argv[i], "-c") && i + 1 < argc)  cfgfile = argv[++i];
        else if (!strcmp(argv[i], "-W") && i + 1 < argc)  pwfile = argv[++i];
        else if (!strcmp(argv[i], "-B") && i + 1 < argc)  blehelper = argv[++i];
        else if (!strcmp(argv[i], "-w") && i + 1 < argc)  webdir = argv[++i];
        else { usage(argv[0]); return 2; }
    }

    fprintf(stderr,
        "== SonosWiFi — UDP Unicast Streamer (protocolo v%d) ==\n"
        "  ALSA     : %s\n  Control  : :%d   Audio: :%d\n"
        "  Playout  : %d ms   Paquete: %d frames\n  Web UI   : http://0.0.0.0:%u  (HTTPS :%u para /calibrate y /ble)\n\n",
        PROTOCOL_VERSION, acfg.device.c_str(), PORT_DISCOVERY, PORT_AUDIO,
        PLAYOUT_DELAY_US / 1000, FRAMES_PER_PACKET, web_port, tls_port);

    if (mlockall(MCL_CURRENT | MCL_FUTURE))
        fprintf(stderr, "[Main] mlockall no disponible (LimitMEMLOCK) — continuo\n");

    AudioCapture capture(acfg);
    UdpStreamer  streamer;
    WebServer    web(streamer, capture, web_port);
    if (tls_port) web.setTLS(tls_port, cert, key);
    if (!webdir.empty()) { web.setWebDir(webdir); fprintf(stderr, "[Main] Web servida desde %s (modo desarrollo)\n", webdir.c_str()); }
    { std::string sd = cfgfile.empty() ? std::string("/var/lib/sonos_wifi") : cfgfile.substr(0, cfgfile.rfind('/') == std::string::npos ? 0 : cfgfile.rfind('/'));
      if (sd.empty()) sd = ".";
      web.setStateDir(sd); }   // update.state / update.log junto al fichero de configuración
    web.setBle(blehelper, cfgfile.empty() ? std::string() : cfgfile + ".ble");   // WiFi de los altavoces (0600) + ayudante Bluetooth
    {   // Contraseña opcional (HTTP Basic; usuario libre): solo si existe el fichero y no está vacío
        FILE* pf = fopen(pwfile.c_str(), "r");
        if (pf) {
            char buf[256] = {0};
            if (fgets(buf, sizeof(buf), pf)) {
                std::string pw(buf);
                while (!pw.empty() && (pw.back() == '\n' || pw.back() == '\r')) pw.pop_back();
                if (!pw.empty()) { web.setPassword(pw); fprintf(stderr, "[Main] Web protegida con contraseña (%s)\n", pwfile.c_str()); }
            }
            fclose(pf);
        }
    }
    if (!cfgfile.empty()) { streamer.setConfigPath(cfgfile); streamer.loadConfig(); }

    if (!streamer.init()) { fprintf(stderr, "[Main] Cannot init UDP streamer\n"); return 1; }
    // Interfaces USB extra (opcional): <config>.inputs + -x. Sin ellas todo sigue como siempre (solo la Traktor).
    const std::string inputs_path = cfgfile.empty() ? std::string() : cfgfile + ".inputs";
    std::vector<ExtraSpec> extras = inputs_path.empty() ? std::vector<ExtraSpec>() : InputMixer::loadFile(inputs_path);
    for (const auto& e : extras_cli) extras.push_back(e);
    const bool traktor_ok = capture.open();
    { std::string verr; if (!extras.empty() && !InputMixer::validate(extras, verr, traktor_ok)) { fprintf(stderr, "[Main] Interfaces extra ignoradas: %s\n", verr.c_str()); extras.clear(); } }
    if (!traktor_ok && extras.empty()) { fprintf(stderr, "[Main] Cannot open ALSA device\n");  return 1; }
    if (!traktor_ok) fprintf(stderr, "[Main] Sin Traktor: sigo con las interfaces extra (toman las entradas desde la 1)\n");
    InputMixer mixer(streamer, acfg.period_size);
    if (!extras.empty()) { mixer.setTraktorPresent(traktor_ok); mixer.setExtras(extras); }
    web.setInputs(extras.empty() ? nullptr : &mixer, inputs_path, extras, traktor_ok, [] { g_running.store(false); });   // reiniciar = salir limpio (systemd lo levanta)

    if (extras.empty())
        capture.setCallback([&](const int32_t* data, uint32_t frames, uint64_t ts) {
            streamer.pushFrames(data, frames, ts);
        });
    else
        capture.setCallback([&](const int32_t* data, uint32_t frames, uint64_t ts) {
            mixer.onTraktor(data, frames, ts);
        });

    if (!web.start())     { fprintf(stderr, "[Main] Cannot start web server\n"); return 1; }
    if (traktor_ok && !capture.start()) { fprintf(stderr, "[Main] Cannot start capture\n"); web.stop(); return 1; }
    if (!extras.empty()) mixer.start();

    fprintf(stderr, "[Main] Running. Ctrl+C to stop.\n");

    int rc = 0;
    uint32_t tick = 0;
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (traktor_ok && !capture.isRunning()) {          // ej. Traktor desenchufada: salir y que systemd reinicie
            fprintf(stderr, "[Main] Captura detenida — saliendo (systemd reiniciará)\n");
            rc = 1; break;
        }
        if (++tick % 150 == 0)
            fprintf(stderr, "[Main] Heartbeat — streaming OK | send_errors=%llu\n",
                    (unsigned long long)streamer.sendErrors());
    }

    fprintf(stderr, "\n[Main] Stopping...\n");
    capture.stop();
    mixer.stop();
    web.stop();
    streamer.shutdown();
    capture.close();
    fprintf(stderr, "[Main] Done.\n");
    return rc;
}
