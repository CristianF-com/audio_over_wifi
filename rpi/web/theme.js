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
