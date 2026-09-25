// Smart Safety Vest — Service Worker
//
// Responsibilities:
//   1. Client-side notifications via registration.showNotification()
//      (reliable on Android Chrome, works while the tab is backgrounded).
//   2. Background push via FCM, so fall/SOS alerts still arrive when the
//      app is fully closed, not just backgrounded.
//   3. A *minimal* app-shell cache — this app is realtime/online-first, so
//      we do NOT cache API/Firebase traffic. We only cache the static shell
//      (this HTML, style.css, app.js, the logo) so that if the network
//      drops entirely, the last-loaded screen still paints instead of a
//      blank/broken tab. Useful for field workers on intermittent signal.

const CACHE_VERSION = 'v1';
const SHELL_CACHE = 'vest-command-shell-' + CACHE_VERSION;

// Keep this list to genuinely static, same-origin files only. Do not add
// Firebase/API URLs here — this is a realtime app and stale sensor data is
// worse than no data.
const SHELL_FILES = [
  './',
  './index.html',
  './style.css',
  './app.js',
  './logo.png',
  './icon-192.png',
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(SHELL_CACHE)
      .then((cache) => cache.addAll(SHELL_FILES))
      .catch(() => { /* best-effort — a missing asset shouldn't block install */ })
  );
  // Do NOT self.skipWaiting() unconditionally anymore — see the message
  // handler below. The new worker now waits for the page to explicitly
  // tell it to take over, so a stale tab doesn't get swapped out from
  // under a mid-action user without warning.
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((names) => Promise.all(
        names.filter((n) => n !== SHELL_CACHE).map((n) => caches.delete(n))
      ))
      .then(() => self.clients.claim())
  );
});

// The page (app.js) posts this once it has registered/updated, either
// immediately (if a worker was already waiting) or after a fresh install
// finishes. This is what lets a new sw.js deploy actually take effect
// without every user having to close every tab first.
self.addEventListener('message', (event) => {
  if (event.data && event.data.type === 'SKIP_WAITING') {
    self.skipWaiting();
  }
});

// --- Fetch: network-first for navigations, so live users always get the
// latest shell when online, with a cached fallback (+ the "no connection,
// showing last known state" experience) when the network is unreachable.
// Everything else (Firebase/API calls, CDN libs) passes straight through —
// this app is not meant to work fully offline, only to degrade gracefully.
self.addEventListener('fetch', (event) => {
  const req = event.request;
  if (req.method !== 'GET') return;

  const url = new URL(req.url);
  const isSameOrigin = url.origin === self.location.origin;
  const isShellFile = isSameOrigin && (
    req.mode === 'navigate' ||
    SHELL_FILES.some((f) => url.pathname.endsWith(f.replace('./', '')))
  );
  if (!isShellFile) return; // let Firebase/CDN/etc. hit the network normally

  event.respondWith(
    fetch(req)
      .then((res) => {
        // Refresh the cached copy in the background so next offline visit
        // uses the latest known-good shell.
        const resClone = res.clone();
        caches.open(SHELL_CACHE).then((cache) => cache.put(req, resClone)).catch(() => {});
        return res;
      })
      .catch(() =>
        caches.match(req).then((cached) => {
          if (cached) return cached;
          if (req.mode === 'navigate') return caches.match('./index.html');
          return Response.error();
        })
      )
  );
});

// --- FCM background push -----------------------------------------------
// Fires for a push received while no page/tab has focus (including the app
// fully closed). The client side (app.js) is responsible for requesting
// permission and registering the FCM token — this handler just displays
// whatever payload the Cloud Function/server sent.
self.addEventListener('push', (event) => {
  let payload = {};
  try {
    payload = event.data ? event.data.json() : {};
  } catch (e) {
    payload = { notification: { title: 'Smart Safety Vest', body: event.data ? event.data.text() : 'New alert' } };
  }

  // FCM messages can carry a `notification` block (shown automatically by
  // some SDK paths) and/or a `data` block (always left to us). We build the
  // notification explicitly here so behavior is identical in both cases.
  const notif = payload.notification || {};
  const data = payload.data || {};

  const title = notif.title || data.title || 'Smart Safety Vest Alert';
  const body = notif.body || data.body || 'Check the app for details.';
  const isCritical = data.type === 'fall' || data.type === 'sos';

  const options = {
    body: body,
    icon: './assets/icon-192.png',
    badge: './assets/icon-192.png',
    tag: data.type || 'vest-alert',
    // Fall/SOS should stay on screen until the worker/supervisor dismisses
    // it, not auto-disappear like a routine sync notification.
    requireInteraction: isCritical,
    vibrate: isCritical ? [200, 100, 200, 100, 200] : [150],
    data: data,
  };

  event.waitUntil(self.registration.showNotification(title, options));
});

// Clicking a notification focuses an already-open tab, or opens a new one.
self.addEventListener('notificationclick', (event) => {
  event.notification.close();
  event.waitUntil(
    self.clients.matchAll({ type: 'window', includeUncontrolled: true }).then((clientList) => {
      for (const client of clientList) {
        if ('focus' in client) return client.focus();
      }
      if (self.clients.openWindow) return self.clients.openWindow('./');
      return null;
    })
  );
});
