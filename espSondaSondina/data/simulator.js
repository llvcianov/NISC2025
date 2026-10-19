document.addEventListener('DOMContentLoaded', () => {
    const logContainer = document.getElementById('log-container');
    const masterSimToggle = document.getElementById('master-sim-toggle');
    const serialDebugToggle = document.getElementById('serial-debug-toggle');
    let websocket;

    // --- FUNZIONI DI CONTROLLO ---
    const sendToggleCommand = async (endpoint, enabled) => {
        try {
            const response = await fetch(`${endpoint}?enable=${enabled}`);
            const text = await response.text();
            log(text, 'SYSTEM');
        } catch (error) {
            log(`Errore durante l'invio del comando a ${endpoint}.`, 'ERROR');
        }
    };

    masterSimToggle.addEventListener('change', () => {
        sendToggleCommand('/toggle_master_simulation', masterSimToggle.checked);
    });

    serialDebugToggle.addEventListener('change', () => {
        sendToggleCommand('/toggle_serial_debug', serialDebugToggle.checked);
    });

    // --- WEBSOCKET E LOGICA DI COMUNICAZIONE ---
    const connectWebSocket = () => {
        const wsUrl = `ws://${window.location.hostname}:81/`;
        websocket = new WebSocket(wsUrl);

        websocket.onopen = () => log('WebSocket Connesso al server ESP8266.');
        websocket.onclose = () => {
            log('WebSocket Disconnesso. Riconnessione tra 3 secondi...');
            setTimeout(connectWebSocket, 3000);
        };
        websocket.onerror = (error) => log('Errore WebSocket: ' + JSON.stringify(error));
        
        websocket.onmessage = (event) => {
            try {
                const message = JSON.parse(event.data);
                if (message.type === 'LOG') {
                    log(message.data, message.source);
                    // La logica di risposta automatica è stata rimossa da qui
                    // per evitare conflitti con la simulazione master nel firmware.
                }
            } catch (e) {
                log(`Messaggio non-JSON ricevuto: ${event.data}`, 'RAW');
            }
        };
    };

    // --- UTILITY DI LOGGING ---
    const log = (data, source = 'SYSTEM') => {
        const entry = document.createElement('div');
        entry.className = 'log-entry';
        const sourceSpan = document.createElement('span');
        sourceSpan.className = 'log-source';
        sourceSpan.classList.add(`source-${source.toLowerCase().replace('_', '-')}`);
        sourceSpan.textContent = `[${source}]`;
        const dataPre = document.createElement('pre');
        dataPre.className = 'log-data';
        try {
            // Formatta il JSON per una migliore leggibilità
            dataPre.textContent = JSON.stringify(JSON.parse(data), null, 2);
        } catch (e) {
            dataPre.textContent = data;
        }
        entry.appendChild(sourceSpan);
        entry.appendChild(dataPre);
        logContainer.appendChild(entry);
        logContainer.scrollTop = logContainer.scrollHeight;
    };

    // --- INIZIALIZZAZIONE ---
    connectWebSocket();
});
