/* Tema de la interfaz: «dark» (por defecto), «light» o «auto» (sigue al sistema). Se guarda en localStorage (por navegador).
   Se ejecuta en el <head> (antes de pintar) para que no haya destello; los botones [data-theme-btn] rotan Oscuro → Claro → Automático. */
(function () {
  var KEY = 'sonos_theme', ORDER = ['dark', 'light', 'auto'], NAME = { dark: 'Oscuro', light: 'Claro', auto: 'Automático' };
  var ICON = {
    dark: '<path d="M20 14.5A8 8 0 0 1 9.5 4 8 8 0 1 0 20 14.5z"/>',
    light: '<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/>',
    auto: '<circle cx="12" cy="12" r="8.5"/><path d="M12 3.5v17a8.5 8.5 0 0 0 0-17z" fill="currentColor"/>'
  };
  var mq = window.matchMedia ? matchMedia('(prefers-color-scheme: light)') : null;
  function pref() { var p = 'dark'; try { p = localStorage.getItem(KEY) || 'dark'; } catch (e) {} return ORDER.indexOf(p) < 0 ? 'dark' : p; }
  function apply() {
    var p = pref(), t = p === 'auto' ? (mq && mq.matches ? 'light' : 'dark') : p;
    document.documentElement.setAttribute('data-theme', t);
    var m = document.querySelector('meta[name=theme-color]'); if (m) m.setAttribute('content', t === 'light' ? '#f3f5f9' : '#000000');
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
})();
