/* Tema de la interfaz: «graphite» (por defecto), «dark» (negro), «light» o «auto» (sigue al sistema; resuelve a graphite o light). Se guarda en localStorage (por navegador).
   Se ejecuta en el <head> (antes de pintar) para que no haya destello; los botones [data-theme-btn] rotan Grafito → Oscuro → Claro → Automático. */
(function () {
  var KEY = 'sonos_theme', SKEY = 'sonos_shape', ZKEY = 'sonos_size', ORDER = ['graphite', 'dark', 'light', 'auto'], NAME = { dark: 'Oscuro', graphite: 'Grafito', light: 'Claro', auto: 'Automático' };
  var META = { dark: '#000000', graphite: '#1c1c1e', light: '#d5d9e0' };
  var ICON = {
    dark: '<path d="M20 14.5A8 8 0 0 1 9.5 4 8 8 0 1 0 20 14.5z"/>',
    graphite: '<rect x="3.5" y="5" width="17" height="14" rx="3"/><path d="M3.5 14h17" /><path d="M3.5 14v2a3 3 0 0 0 3 3h11a3 3 0 0 0 3-3v-2z" fill="currentColor"/>',
    light: '<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/>',
    auto: '<circle cx="12" cy="12" r="8.5"/><path d="M12 3.5v17a8.5 8.5 0 0 0 0-17z" fill="currentColor"/>'
  };
  var mq = window.matchMedia ? matchMedia('(prefers-color-scheme: light)') : null;
  function pref() { var p = 'graphite'; try { p = localStorage.getItem(KEY) || 'graphite'; } catch (e) {} return ORDER.indexOf(p) < 0 ? 'graphite' : p; }
  // Forma: «square» (por defecto: todos los radios a 0 salvo los altavoces del radar) o «round» (píldoras). ?forma=recta / ?forma=redonda la fija y se guarda.
  function shape() { var v = 'square'; try { v = localStorage.getItem(SKEY) || 'square'; } catch (e) {} return v === 'round' ? 'round' : 'square'; }
  function setShape(v) { try { localStorage.setItem(SKEY, v === 'square' ? 'square' : 'round'); } catch (e) {} apply(); }
  try { var q = /[?&]forma=(recta|redonda)/.exec(location.search); if (q) setShape(q[1] === 'recta' ? 'square' : 'round'); } catch (e) {}
  // Tamaño del texto: «s» (pequeño), «m» (por defecto) o «l» (grande). Se guarda en este navegador.
  function size() { var v = 'm'; try { v = localStorage.getItem(ZKEY) || 'm'; } catch (e) {} return v === 's' || v === 'l' ? v : 'm'; }
  function setSize(v) { try { localStorage.setItem(ZKEY, v === 's' || v === 'l' ? v : 'm'); } catch (e) {} apply(); }
  function apply() {
    document.documentElement.setAttribute('data-shape', shape());
    document.documentElement.setAttribute('data-size', size());
    var p = pref(), t = p === 'auto' ? (mq && mq.matches ? 'light' : 'graphite') : p;
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
  window.SonosTheme = { get: pref, set: set, next: next, apply: apply, shape: setShape, getShape: shape, size: setSize, getSize: size,
    reset: function () { try { localStorage.removeItem(KEY); localStorage.removeItem(SKEY); localStorage.removeItem(ZKEY); } catch (e) {} apply(); } };
  apply();
  if (mq && mq.addEventListener) mq.addEventListener('change', apply);
  addEventListener('storage', function (e) { if (e.key === KEY || e.key === SKEY || e.key === ZKEY) apply(); });
  document.addEventListener('DOMContentLoaded', function () {
    var bs = document.querySelectorAll('[data-theme-btn]');
    for (var i = 0; i < bs.length; i++) bs[i].addEventListener('click', next);
    apply();
  });
})();

/* Lado de la muesca en horizontal (como en interval-hiit): html.notch-l si está a la izquierda, html.notch-r si está a la derecha;
   el otro lado solo necesita un margen chico para las esquinas redondeadas. */
(function () {
  var r = document.documentElement;
  function f() {
    var o = window.orientation, a = window.screen && screen.orientation && screen.orientation.angle;
    if (o == null && a != null) o = a === 270 ? -90 : a;
    r.classList.toggle('notch-l', o === 90);
    r.classList.toggle('notch-r', o === -90 || o === 270);
  }
  f(); addEventListener('orientationchange', f); addEventListener('resize', f);
  try { screen.orientation.addEventListener('change', f); } catch (e) {}
})();

/* Entrada con clave dentro de la web (no el diálogo Basic del navegador, lento en la app instalada): todas las peticiones de la página
   llevan «X-Requested-With: sonos» (el servidor entonces no manda WWW-Authenticate); ante un 401 de /api/ se muestra esta pantalla,
   que envía la clave a POST /api/login y deja una cookie de sesión. */
(function () {
  if (!window.fetch || window.__sonosLogin) return;
  window.__sonosLogin = true;
  var of = window.fetch.bind(window), box = null;
  function esc() { var el = document.getElementById('sonos_login'); if (el) el.remove(); box = null; }
  function show() {
    if (box || !document.body) return;
    box = document.createElement('div'); box.id = 'sonos_login'; box.setAttribute('role', 'dialog'); box.setAttribute('aria-modal', 'true');
    box.style.cssText = 'position:fixed;inset:0;z-index:99999;display:flex;align-items:center;justify-content:center;background:var(--bg);padding:max(16px,env(safe-area-inset-top)) 16px 16px;overflow:auto';
    box.innerHTML = '<form id="sl_f" style="width:min(360px,100%);display:grid;gap:12px;font-family:var(--font);color:var(--fg)">' +
      '<h1 style="margin:0 0 4px;font-size:1.5rem;font-weight:500">AStream</h1>' +
      '<p style="margin:0 0 6px;color:var(--mut)">Escribe la contraseña para entrar.</p>' +
      '<label for="sl_u" style="font-size:.9rem;color:var(--mut)">Usuario (opcional)</label>' +
      '<input id="sl_u" autocomplete="username" autocapitalize="none" style="min-height:44px;padding:8px 12px;background:var(--panel);color:var(--fg);border:1px solid var(--ctl);font-size:1rem">' +
      '<label for="sl_p" style="font-size:.9rem;color:var(--mut)">Contraseña</label>' +
      '<input id="sl_p" type="password" autocomplete="current-password" required style="min-height:44px;padding:8px 12px;background:var(--panel);color:var(--fg);border:1px solid var(--ctl);font-size:1rem">' +
      '<div id="sl_e" role="alert" style="min-height:1.2em;color:var(--bad-fg);font-size:.9rem"></div>' +
      '<button class="btn" id="sl_b" type="submit">Entrar</button></form>';
    document.body.appendChild(box);
    var f = document.getElementById('sl_f'), p = document.getElementById('sl_p'), e = document.getElementById('sl_e'), b = document.getElementById('sl_b');
    setTimeout(function () { try { p.focus(); } catch (x) {} }, 50);
    f.addEventListener('submit', function (ev) {
      ev.preventDefault(); b.disabled = true; e.textContent = '';
      of('/api/login', { method: 'POST', headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'sonos' }, body: JSON.stringify({ pass: p.value }) })
        .then(function (r) { if (r.ok) { esc(); location.reload(); } else { e.textContent = r.status === 401 ? 'Contraseña incorrecta.' : 'No se pudo entrar.'; b.disabled = false; p.select(); } })
        .catch(function () { e.textContent = 'Sin conexión con la Raspberry.'; b.disabled = false; });
    });
  }
  window.fetch = function (input, init) {
    var url = typeof input === 'string' ? input : (input && input.url) || '';
    var same = url.charAt(0) === '/' || url.indexOf(location.origin) === 0;
    if (same) { init = init || {}; var h = new Headers(init.headers || (typeof input !== 'string' && input.headers) || {}); h.set('X-Requested-With', 'sonos'); init.headers = h; }
    return of(input, init).then(function (r) {
      if (r.status === 401 && same && url.indexOf('/api/login') < 0) {
        if (document.body) show(); else document.addEventListener('DOMContentLoaded', show);
      }
      return r;
    });
  };
})();
