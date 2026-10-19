const CACHE_NAME = 'plantpen-v1';
// Lista dei file fondamentali dell'applicazione da salvare in cache.
const ASSETS_TO_CACHE = [
    '/',
    '/index.html',
    '/styles.css',
    '/app.js',
    '/manifest.json',
    'https://cdn.jsdelivr.net/npm/chart.js'
];

/**
 * Evento 'install': viene chiamato quando il service worker viene installato per la prima volta.
 * Qui apriamo la nostra cache e salviamo tutti i file statici necessari
 * per far funzionare l'app offline.
 */
self.addEventListener('install', (event) => {
  console.log('Service Worker: Installazione...');
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => {
        console.log('Service Worker: Caching dei file dell\'app...');
        // Aggiunge tutti i file definiti in ASSETS_TO_CACHE.
        return cache.addAll(ASSETS_TO_CACHE);
      })
  );
});

/**
 * Evento 'activate': viene chiamato dopo l'installazione, quando il nuovo
 * service worker viene attivato. È il momento giusto per pulire le vecchie cache.
 */
self.addEventListener('activate', (event) => {
    console.log('Service Worker: Attivazione...');
    event.waitUntil(
        caches.keys().then((cacheNames) => {
            return Promise.all(
                cacheNames.map((cacheName) => {
                    // Se il nome della cache non corrisponde a quella attuale, la elimina.
                    if (cacheName !== CACHE_NAME) {
                        console.log('Service Worker: Pulizia vecchia cache:', cacheName);
                        return caches.delete(cacheName);
                    }
                })
            );
        })
    );
});

/**
 * Evento 'fetch': intercetta tutte le richieste di rete fatte dalla pagina.
 * Implementa una strategia "Cache falling back to network".
 */
self.addEventListener('fetch', (event) => {
  // Ignora le richieste non-GET, come le chiamate alle API (POST) o i WebSocket.
  if (event.request.method !== 'GET') {
    return;
  }
  
  event.respondWith(
    // Prova a cercare la risorsa nella cache.
    caches.match(event.request)
      .then((response) => {
        // Se la risorsa viene trovata nella cache, la restituisce immediatamente.
        if (response) {
          // console.log('Service Worker: Risorsa trovata in cache:', event.request.url);
          return response;
        }

        // Se la risorsa non è in cache, la richiede alla rete.
        // console.log('Service Worker: Risorsa non in cache, fetching dalla rete:', event.request.url);
        return fetch(event.request);
      })
  );
});

