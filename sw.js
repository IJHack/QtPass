const cacheName = 'qtpass::v1.3.2::static';

self.addEventListener('install', e => {
  e.waitUntil(
    caches.open(cacheName).then(cache => {
      return cache.addAll([
        '/',
        '/404.html',
        '/changelog.1.1.html',
        '/changelog.1.2.html',
        '/changelog.beta.html',
        '/changelog.html',
        '/changelog.old.html',
        '/downloads.html',
        '/index.html',
        '/stylesheets/pygment_trac.css',
        '/stylesheets/styles.css',
        '/javascripts/main.js',
        '/javascripts/scale.fix.js',
        '/docs/',
        '/images/android-icon-144x144.png',
        '/images/apple-icon-120x120.png',
        '/images/apple-icon-76x76.png',
        '/images/favicon-32x32.png',
        '/images/ms-icon-144x144.png',
        '/images/win10.png',
        '/images/android-icon-192x192.png',
        '/images/apple-icon-144x144.png',
        '/images/apple-icon-precomposed.png',
        '/images/favicon-96x96.png',
        '/images/ms-icon-150x150.png',
        '/images/windows.png',
        '/images/android-icon-36x36.png',
        '/images/apple-icon-152x152.png',
        '/images/apple-icon.png',
        '/images/freebsd.png',
        '/images/ms-icon-310x310.png',
        '/images/android-icon-48x48.png',
        '/images/apple-icon-180x180.png',
        '/images/bg_hr.png',
        '/images/icon_download.png',
        '/images/ms-icon-70x70.png',
        '/images/android-icon-72x72.png',
        '/images/apple-icon-57x57.png',
        '/images/blacktocat.png',
        '/images/linux.png',
        '/images/og.png',
        '/images/android-icon-96x96.png',
        '/images/apple-icon-60x60.png',
        '/images/config.png',
        '/images/logo.png',
        '/images/qtpass.png',
        '/images/apple-icon-114x114.png',
        '/images/apple-icon-72x72.png',
        '/images/favicon-16x16.png',
        '/images/macos.png',
        '/images/sprite_download.png'
      ]).then(() => self.skipWaiting());
    })
  );
});
self.addEventListener('fetch', event => {
  event.respondWith(
    caches.open(cacheName).then(cache => {
      return cache.match(event.request).then(res => {
        return res || fetch(event.request)
      });
    })
  );
});