const CACHE_NAME = "qtpass::v1.7.0::static";

const PRECACHE_URLS = [
  "/",
  "/404.html",
  "/getting-started.html",
  "/advanced.html",
  "/changelog.1.1.html",
  "/changelog.1.2.html",
  "/changelog.1.3.html",
  "/changelog.1.4.html",
  "/changelog.1.5.html",
  "/changelog.1.6.html",
  "/changelog.beta.html",
  "/changelog.html",
  "/changelog.old.html",
  "/downloads.html",
  "/index.html",
  "/stylesheets/pygment_trac.css",
  "/stylesheets/styles.css",
  "/javascripts/main.js",
  "/javascripts/scale.fix.js",
  "/docs/",
  "/images/android-icon-144x144.png",
  "/images/apple-icon-120x120.png",
  "/images/apple-icon-76x76.png",
  "/images/favicon-32x32.png",
  "/images/ms-icon-144x144.png",
  "/images/win10.png",
  "/images/win11.png",
  "/images/android-icon-192x192.png",
  "/images/apple-icon-144x144.png",
  "/images/apple-icon-precomposed.png",
  "/images/favicon-96x96.png",
  "/images/ms-icon-150x150.png",
  "/images/windows.png",
  "/images/android-icon-36x36.png",
  "/images/apple-icon-152x152.png",
  "/images/apple-icon.png",
  "/images/freebsd.png",
  "/images/ms-icon-310x310.png",
  "/images/android-icon-48x48.png",
  "/images/apple-icon-180x180.png",
  "/images/bg_hr.png",
  "/images/icon_download.png",
  "/images/ms-icon-70x70.png",
  "/images/android-icon-72x72.png",
  "/images/apple-icon-57x57.png",
  "/images/blacktocat.png",
  "/images/linux.png",
  "/images/og.png",
  "/images/android-icon-96x96.png",
  "/images/apple-icon-60x60.png",
  "/images/config.png",
  "/images/logo.png",
  "/images/qtpass.png",
  "/images/apple-icon-114x114.png",
  "/images/apple-icon-72x72.png",
  "/images/favicon-16x16.png",
  "/images/macos.png",
  "/images/sprite_download.png",
  "/fonts/lato-latin-300-normal.woff2",
  "/fonts/lato-latin-400-italic.woff2",
  "/fonts/lato-latin-400-normal.woff2",
  "/fonts/lato-latin-700-italic.woff2",
  "/fonts/lato-latin-700-normal.woff2",
  "/fonts/lato-latin-900-italic.woff2",
  "/fonts/lato-latin-900-normal.woff2",
  "/manifest.json",
  "/favicon.ico",
];

self.addEventListener("install", (event) => {
  console.log("[sw] install started for cache", CACHE_NAME);

  event.waitUntil(
    (async () => {
      const cache = await caches.open(CACHE_NAME);

      for (const url of PRECACHE_URLS) {
        console.log("[sw] precache fetch", url);

        let response;
        try {
          response = await fetch(url, { redirect: "follow" });
        } catch (error) {
          console.error("[sw] network error while fetching", url, error);
          throw error;
        }

        console.log(
          "[sw] precache response",
          url,
          response.status,
          response.url,
        );

        if (!response.ok) {
          throw new Error(
            "[sw] precache failed for " +
              url +
              " with status " +
              response.status,
          );
        }

        await cache.put(url, response.clone());
        console.log("[sw] cached", url);
      }

      console.log("[sw] install completed, calling skipWaiting()");
      await self.skipWaiting();
    })(),
  );
});

self.addEventListener("activate", (event) => {
  console.log("[sw] activate started for cache", CACHE_NAME);

  event.waitUntil(
    (async () => {
      const cacheNames = await caches.keys();

      for (const cacheName of cacheNames) {
        if (cacheName !== CACHE_NAME) {
          console.log("[sw] deleting old cache", cacheName);
          await caches.delete(cacheName);
        }
      }

      console.log("[sw] activate completed, claiming clients");
      await self.clients.claim();
    })(),
  );
});

self.addEventListener("fetch", (event) => {
  const request = event.request;

  if (request.method !== "GET") {
    return;
  }

  event.respondWith(
    (async () => {
      const cachedResponse = await caches.match(request);

      if (cachedResponse) {
        console.log("[sw] cache hit", request.url);
        return cachedResponse;
      }

      console.log("[sw] cache miss, fetching", request.url);
      return fetch(request);
    })(),
  );
});
