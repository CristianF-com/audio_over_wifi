/* Service worker de AStream. Solo se registra en contexto seguro (HTTPS o localhost).
 * - /api/*: SIEMPRE a la red (el estado en vivo no se cachea).
 * - Páginas e iconos: red primero y, si no hay red, la última copia (la interfaz abre sin conexión y avisa de que no hay datos).
 * @BUILD@ lo sustituye scripts/gen_web_pages.py por un hash de las páginas: cada cambio invalida la caché. */
const CACHE = 'sonoswifi-@BUILD@';
const SHELL = ['/', '/calibrate', '/ble', '/manifest.webmanifest', '/icons/icon-192.png', '/fonts/roboto-400.woff2', '/fonts/roboto-500.woff2'];

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
});
