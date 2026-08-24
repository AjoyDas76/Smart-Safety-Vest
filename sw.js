// Smart Safety Vest — Service Worker
// Purpose: lets the app show notifications via
// registration.showNotification() instead of the plain Notification
// constructor. This is required for reliable notifications on Android
// Chrome and improves delivery while the app tab is in the background.
// It does not do offline caching — this app is realtime/online-first.

self.addEventListener('install', () => {
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(self.clients.claim());
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
