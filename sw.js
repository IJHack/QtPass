const CACHE_NAME = "qtpass::v1.8.1-6::static";

// Cache-bust: the pages append this to asset URLs whose names do not
// change between releases (styles.css, logo.svg, the favicons), so the
// browser's HTTP cache (up to a year on this server) cannot hand out a
// stale copy after a redesign. Bump it together with CACHE_NAME.
const SW_VERSION = "?v=1.8.1-6";

const PRECACHE_URLS = [
  "/",
  "/404",
  "/advanced",
  "/changelog",
  "/changelog.1.1",
  "/changelog.1.2",
  "/changelog.1.3",
  "/changelog.1.4",
  "/changelog.1.5",
  "/changelog.1.6",
  "/changelog.beta",
  "/changelog.old",
  "/contributing",
  "/downloads",
  "/faq",
  "/getting-started",
  "/macos",
  "/privacy",
  "/screenshots",
  "/security",
  "/stylesheets/styles.css",
  "/javascripts/main.js",
  "/docs/",
  "/images/android-icon-144x144.png",
  "/images/apple-icon-120x120.png",
  "/images/apple-icon-76x76.png",
  "/images/favicon-32x32.png",
  "/images/ms-icon-144x144.png",
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
  "/images/ms-icon-70x70.png",
  "/images/android-icon-72x72.png",
  "/images/apple-icon-57x57.png",
  "/images/linux.png",
  "/images/og.png",
  "/images/android-icon-96x96.png",
  "/images/apple-icon-60x60.png",
  "/images/config.png",
  "/images/config@2x.png",
  "/images/config@2x.webp",
  "/images/config-dark.png",
  "/images/config-dark@2x.png",
  "/images/config-dark@2x.webp",
  "/images/logo.png",
  "/images/logo.svg",
  "/images/qtpass.png",
  "/images/qtpass@2x.png",
  "/images/qtpass@2x.webp",
  "/images/qtpass-dark.png",
  "/images/qtpass-dark@2x.png",
  "/images/qtpass-dark@2x.webp",
  "/images/apple-icon-114x114.png",
  "/images/apple-icon-72x72.png",
  "/images/favicon-16x16.png",
  "/images/macos.png",
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
          // "reload": straight from the server, never the HTTP cache, or a
          // fresh service worker would precache last month's stylesheet.
          response = await fetch(url, { redirect: "follow", cache: "reload" });
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

  // Third-party assets (badges, repology) are the browser's business: a
  // failing remote host should not turn into a service worker error.
  if (new URL(request.url).origin !== self.location.origin) {
    return;
  }

  // HTML: network first, so a republished page reaches returning visitors
  // immediately; fall back to the cached copy when offline.
  if (
    request.mode === "navigate" ||
    (request.headers.get("accept") || "").includes("text/html")
  ) {
    event.respondWith(
      (async () => {
        try {
          // Revalidate with the server (ETag) rather than trusting the HTTP
          // cache's max-age, so a republished page shows up on the next load.
          const networkResponse = await fetch(request, { cache: "no-cache" });
          if (networkResponse.ok) {
            const cache = await caches.open(CACHE_NAME);
            cache.put(request, networkResponse.clone());
          }
          return networkResponse;
        } catch (error) {
          const cachedResponse = await caches.match(request);
          if (cachedResponse) {
            console.log("[sw] offline, serving cached", request.url);
            return cachedResponse;
          }
          throw error;
        }
      })(),
    );
    return;
  }

  // Static assets: cache first.
  event.respondWith(
    (async () => {
      // The precache list holds bare paths; the pages ask for them with the
      // SW_VERSION query appended, so match ignoring the query.
      const cachedResponse = await caches.match(request, {
        ignoreSearch: true,
      });

      if (cachedResponse) {
        return cachedResponse;
      }

      return fetch(request);
    })(),
  );
});
