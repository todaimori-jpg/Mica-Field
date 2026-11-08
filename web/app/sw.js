// SPDX-License-Identifier: AGPL-3.0-only
const CACHE = 'mica-field-ipad-2';
const ASSETS = ['./', './index.html', './style.css', './main.js', './view.js', './audio.js',
  './storage.js', './patch.js', './processor.js', './engine.mjs', './engine.wasm',
  './meta.json', './manifest.webmanifest', './icon-192.png', './icon-512.png'];
self.addEventListener('install', event => {
  event.waitUntil(caches.open(CACHE).then(cache => cache.addAll(ASSETS)).then(() => self.skipWaiting()));
});
self.addEventListener('activate', event => {
  event.waitUntil(caches.keys().then(keys => Promise.all(keys.filter(k => k.startsWith('mica-field-ipad-') && k !== CACHE).map(k => caches.delete(k)))).then(() => self.clients.claim()));
});
self.addEventListener('fetch', event => {
  const url = new URL(event.request.url);
  if (event.request.method !== 'GET' || url.origin !== self.location.origin) return;
  event.respondWith(caches.open(CACHE).then(async cache => {
    if (event.request.mode === 'navigate') {
      try {
        const response = await fetch(event.request);
        if (response.ok && !response.redirected) cache.put('./index.html', response.clone());
        return response;
      } catch { return (await cache.match('./index.html')) || Response.error(); }
    }
    const saved = await cache.match(event.request);
    return saved || fetch(event.request).then(response => {
      if (response.ok) cache.put(event.request, response.clone());
      return response;
    });
  }));
});
