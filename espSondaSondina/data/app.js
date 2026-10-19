document.addEventListener('DOMContentLoaded', () => {

    // --- STATE MANAGEMENT ---
    const state = {
        plantTypes: [],
        registeredPlants: [],
        activePage: 'init-page',
        websocket: null,
        historyChart: null,
        currentHistoryData: null,
        wifiStatus: {}
    };

    // --- DOM ELEMENTS ---
    const pages = document.querySelectorAll('.page');
    const initButton = document.getElementById('init-button');
    const addPlantModal = document.getElementById('add-plant-modal');
    const addPlantForm = document.getElementById('add-plant-form');
    const modalCloseBtn = document.querySelector('.modal-close-btn');
    const toast = document.getElementById('toast-notification');
    const showChartButton = document.getElementById('show-chart-button');
    const scanWifiButton = document.getElementById('scan-wifi-button');
    const wifiConnectForm = document.getElementById('wifi-connect-form');

    // --- NAVIGATION ---
    const navigateTo = (pageId) => {
        const previousPage = state.activePage;
        pages.forEach(page => page.classList.remove('active'));
        document.getElementById(pageId).classList.add('active');
        state.activePage = pageId;

        if (pageId === 'home-page' && previousPage !== 'home-page' && previousPage !== 'init-page') {
            initializeApp(true);
        }
        if (pageId === 'settings-page') {
            updateWifiStatus();
        }
    };

    document.body.addEventListener('click', (e) => {
        if (e.target.matches('.btn-back')) {
            e.preventDefault();
            navigateTo(e.target.dataset.target);
        }
        if (e.target.matches('[data-action="go-to-settings"]')) {
            navigateTo('settings-page');
        }
    });

    // --- API & WEBSOCKET ---
    const apiFetch = async (endpoint, options) => {
        try {
            const response = await fetch(endpoint, options);
            if (!response.ok) {
                const errorData = await response.json();
                throw new Error(errorData.error || `HTTP error! status: ${response.status}`);
            }
            const contentType = response.headers.get("content-type");
            if (contentType && contentType.indexOf("application/json") !== -1) {
                return response.json();
            } else {
                return response.text();
            }
        } catch (error) {
            console.error(`Fetch error for ${endpoint}:`, error);
            showToast(error.message, 'error');
            throw error;
        }
    };
    
    const connectWebSocket = () => {
        const wsUrl = `ws://${window.location.hostname}:81/`;
        state.websocket = new WebSocket(wsUrl);

        state.websocket.onopen = () => console.log('WebSocket connected');
        state.websocket.onclose = () => {
            console.log('WebSocket disconnected. Reconnecting...');
            setTimeout(connectWebSocket, 3000);
        };
        state.websocket.onerror = (error) => console.error('WebSocket error:', error);
        
        state.websocket.onmessage = (event) => {
            const data = JSON.parse(event.data);
            if (data.type === 'LOG') return;
            if (data.type === 'MEAS_RESULTS') {
                renderMeasurementResults(data);
            } else if (data.type === 'ERR') {
                showToast(`Errore dalla sonda: ${data.msg}`, 'error');
                if (state.activePage === 'measurement-page') navigateTo('home-page');
            }
        };
    };

    // --- RENDERING & LOGICA UI ---
    
    const renderMeasurementResults = (data) => {
        const container = document.getElementById('measurement-results-container');
        const plant = state.registeredPlants.find(p => p.plantId == data.plantId);
        document.getElementById('measurement-plant-name').textContent = plant ? plant.name : `Risultati per Pianta #${data.plantId}`;
        
        const renderParam = (label, value, unit, score, advice) => {
            const scoreColor = score > 80 ? '#4CAF50' : score > 50 ? '#FFC107' : '#F44336';
            return `<div class="param-card"><h4>${label}</h4><div class="param-details"><span class="param-value">${value.toFixed(1)} ${unit}</span><span class="param-score" style="background-color: ${scoreColor};">${score.toFixed(0)}/100</span></div><p class="param-advice">${advice}</p></div>`;
        };

        container.innerHTML = `
            <div class="card score-card">
                <div class="score-value">${data.score_totale.toFixed(1)}</div>
                <div class="score-label">Punteggio Totale</div>
            </div>
            <div id="meas-ai-advice-container" class="card ai-advice hidden"></div>
            <div class="measurement-grid">
                ${renderParam('Temperatura Aria', data.temperatura_valore, '°C', data.temperatura_score, data.temperatura_stringa)}
                ${renderParam('Umidità Suolo', data.umidita_interna_valore, '%', data.umidita_interna_score, data.umidita_interna_stringa)}
                ${renderParam('Luminosità', data.luce_valore, 'lux', data.luce_score, data.luce_stringa)}
                ${renderParam('Umidità Aria', data.umidita_esterna_valore, '%', data.umidita_esterna_score, data.umidita_esterna_stringa)}
            </div>`;
        
        navigateTo('measurement-page');
        
        const aiAdviceContainer = document.getElementById('meas-ai-advice-container');
        fetchAndDisplayAiAdvice(data, aiAdviceContainer);
    };

    const renderHistory = (data) => {
        const plant = state.registeredPlants.find(p => p.plantId == data.plantId);
        const plantName = plant ? plant.name : `Pianta #${data.plantId}`;
        document.getElementById('history-plant-name').textContent = `Storico: ${plantName}`;

        const measurements = data.measurements || data.misurazioni || [];
        const score_avg = data.score_avg || 0;

        document.getElementById('history-avg-score').textContent = score_avg.toFixed(1);
        const listContainer = document.getElementById('history-list-container');
        listContainer.innerHTML = '<h3>Dettaglio Misurazioni</h3>';
        
        const chartButton = document.getElementById('show-chart-button');
        const chartContainer = document.getElementById('history-chart-container');
        chartContainer.classList.add('hidden');

        if (measurements.length === 0) {
            listContainer.innerHTML += '<p>Nessuna misurazione trovata.</p>';
            chartButton.classList.add('hidden');
        } else {
            chartButton.classList.remove('hidden');
            [...measurements].reverse().forEach(m => {
                 const item = document.createElement('div');
                 item.className = 'card measurement-item';
                 item.innerHTML = `<div class="timestamp">${new Date(m.timestamp).toLocaleString('it-IT')}</div><div class="label">Temp. Score:</div><div>${m.temperatura_score.toFixed(0)}</div><div class="label">Luce Score:</div><div>${m.luce_score.toFixed(0)}</div><div class="label">Umid. Suolo Score:</div><div>${m.umidita_interna_score.toFixed(0)}</div><div class="label">Umid. Aria Score:</div><div>${m.umidita_esterna_score.toFixed(0)}</div><div class="label">Score Totale:</div><div><strong>${m.score_totale.toFixed(1)}</strong></div>`;
                 listContainer.appendChild(item);
            });
        }
    };
    
    const renderHistoryChart = () => {
        const data = state.currentHistoryData;
        const measurements = data.measurements || data.misurazioni || [];

        if (measurements.length === 0) {
            showToast('Nessun dato per generare il grafico.', 'error');
            return;
        }
        const chartContainer = document.getElementById('history-chart-container');
        const chartButton = document.getElementById('show-chart-button');
        const canvas = document.getElementById('history-chart');
        if (!canvas) return;
        chartContainer.classList.remove('hidden');
        chartButton.classList.add('hidden');
        setTimeout(() => {
            try {
                const ctx = canvas.getContext('2d');
                if(state.historyChart) state.historyChart.destroy();
                state.historyChart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: measurements.map(m => new Date(m.timestamp).toLocaleTimeString('it-IT', { hour: '2-digit', minute: '2-digit' })),
                        datasets: [{ label: 'Punteggio Totale', data: measurements.map(m => m.score_totale), borderColor: 'var(--primary-color)', backgroundColor: 'rgba(76, 175, 80, 0.1)', fill: true, tension: 0.3 }]
                    },
                    options: { responsive: true, maintainAspectRatio: false, scales: { y: { beginAtZero: true, max: 100 } } }
                });
            } catch (error) {
                console.error("Errore grafico:", error);
                showToast("Impossibile creare il grafico.", "error");
            }
        }, 50);
    };

    const renderPlantTypes = () => {
        const container = document.getElementById('plant-types-carousel');
        const filter = document.getElementById('filter-plant-type');
        container.innerHTML = '';
        while (filter.options.length > 1) filter.remove(1);
        state.plantTypes.forEach(pt => {
            const card = document.createElement('div');
            card.className = 'plant-type-card';
            card.dataset.typeId = pt.id;
            card.innerHTML = `<div class="icon">🌱</div><h3>${pt.name}</h3>`;
            container.appendChild(card);
            const option = document.createElement('option');
            option.value = pt.id;
            option.textContent = pt.name;
            filter.appendChild(option);
        });
    };
    const renderGarden = () => {
        const container = document.getElementById('garden-plants-container');
        container.innerHTML = '';
        let plantsToRender = [...state.registeredPlants];
        const typeFilter = document.getElementById('filter-plant-type').value;
        if (typeFilter !== 'all') {
            plantsToRender = plantsToRender.filter(p => p.typeId == typeFilter);
        }
        const sortFilter = document.getElementById('filter-sort').value;
        plantsToRender.sort((a, b) => {
            if (sortFilter === 'name_asc') return a.name.localeCompare(b.name);
            if (sortFilter === 'name_desc') return b.name.localeCompare(a.name);
            return 0;
        });
        if (plantsToRender.length === 0) {
            container.innerHTML = '<p>Nessuna pianta nel tuo giardino.</p>';
            return;
        }
        plantsToRender.forEach(plant => {
            const plantType = state.plantTypes.find(pt => pt.id === plant.typeId);
            const card = document.createElement('div');
            card.className = 'card garden-plant-card';
            card.innerHTML = `
                <div class="info"><h3>${plant.name}</h3><p>${plantType ? plantType.name : 'Tipo sconosciuto'}</p></div>
                <div class="actions">
                    <button class="btn-primary" data-action="measure" data-plant-id="${plant.plantId}">Nuova Misurazione</button>
                    <button class="btn-secondary" data-action="history" data-plant-id="${plant.plantId}">Storico</button>
                </div>`;
            container.appendChild(card);
        });
    };

    const initializeApp = async (silent = false) => {
        if (!silent) showToast('Sincronizzazione in corso...');
        try {
            const data = await apiFetch('/init', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ timestamp: new Date().toISOString() })
            });
            state.plantTypes = data.plantTypes;
            state.registeredPlants = data.registeredPlants;
            renderPlantTypes();
            renderGarden();
            updateWifiStatus();
            if (!silent) {
                navigateTo('home-page');
                showToast('Sincronizzazione completata!', 'success');
            } else {
                 showToast('Giardino aggiornato!', 'success');
            }
        } catch (error) {
            if (!silent) showToast('Sincronizzazione fallita. Riprova.', 'error');
        }
    };

    const fetchAndDisplayAiAdvice = async (measurementData, container) => {
        if (!state.wifiStatus.connected) return;
        
        const plant = state.registeredPlants.find(p => p.plantId == measurementData.plantId);
        if (!plant) return; 
        const plantType = state.plantTypes.find(pt => pt.id === plant.typeId);
        if (!plantType) return;

        container.innerHTML = `<div class="spinner-container small"><div class="spinner"></div><p>Chiedo un consiglio all'AI...</p></div>`;
        container.classList.remove('hidden');

        try {
            const payload = {
                plantType: plantType.name,
                measurement: measurementData
            };
            const response = await apiFetch('/get_ai_advice', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });

            if (response.advice) {
                container.innerHTML = `<h4>🤖 Consiglio AI</h4><p>${response.advice}</p>`;
            } else {
                container.innerHTML = `<h4>🤖 Consiglio AI</h4><p><i>${response.error || "Impossibile ottenere il consiglio."}</i></p>`;
            }
        } catch (error) {
            container.classList.add('hidden');
        }
    };
    
    const updateWifiStatus = async () => {
        try {
            const status = await apiFetch('/wifi_status');
            state.wifiStatus = status;
            
            const banner = document.getElementById('wifi-status-banner');
            if (status.connected) {
                banner.innerHTML = `Connesso a <strong>${status.ssid}</strong> (IP: ${status.ip})`;
                banner.classList.remove('hidden');
            } else {
                banner.classList.add('hidden');
            }

            const settingsStatus = document.getElementById('wifi-settings-status');
            if (status.mode === 'AP') {
                settingsStatus.innerHTML = `<h4>Modalità Access Point Attiva</h4><p>SSID: <strong>${status.ssid}</strong></p><p>Per usare i consigli AI, connetti la sonda a una rete WiFi con accesso a Internet.</p>`;
            } else {
                settingsStatus.innerHTML = `<h4>Modalità Client WiFi</h4><p>Stato: ${status.connected ? `<strong>Connesso</strong> a <em>${status.ssid}</em>` : '<strong>Non Connesso</strong>'}</p><p>IP: ${status.ip}</p>`;
            }
        } catch (error) {
            console.error("Could not update wifi status", error);
        }
    };
    
    scanWifiButton.addEventListener('click', async () => {
        showToast('Scansione reti in corso...');
        scanWifiButton.disabled = true;
        try {
            const data = await apiFetch('/wifi_scan');
            const list = document.getElementById('wifi-scan-list');
            list.innerHTML = '';
            if (data.networks && data.networks.length > 0) {
                data.networks.forEach(net => {
                    const item = document.createElement('div');
                    item.className = 'wifi-scan-item';
                    item.textContent = `${net.ssid} (${net.rssi} dBm)`;
                    item.onclick = () => {
                        document.getElementById('wifi-ssid').value = net.ssid;
                        wifiConnectForm.classList.remove('hidden');
                    };
                    list.appendChild(item);
                });
            } else {
                list.innerHTML = '<p>Nessuna rete trovata.</p>';
            }
        } finally {
            scanWifiButton.disabled = false;
        }
    });

    wifiConnectForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const ssid = document.getElementById('wifi-ssid').value;
        const password = document.getElementById('wifi-password').value;
        showToast('Salvataggio credenziali e riavvio...');
        try {
            await apiFetch('/wifi_connect', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`
            });
        } catch(error) {
            // L'errore è atteso
        }
    });

    initButton.addEventListener('click', () => initializeApp(false));

    document.getElementById('plant-types-carousel').addEventListener('click', (e) => {
        const card = e.target.closest('.plant-type-card');
        if (!card) return;
        const typeId = card.dataset.typeId;
        const plantType = state.plantTypes.find(pt => pt.id == typeId);
        document.getElementById('modal-plant-type-name').textContent = `Aggiungi ${plantType.name}`;
        document.getElementById('modal-plant-type-id').value = typeId;
        addPlantModal.classList.add('active');
    });

    modalCloseBtn.addEventListener('click', () => addPlantModal.classList.remove('active'));
    addPlantModal.addEventListener('click', (e) => { if (e.target === addPlantModal) addPlantModal.classList.remove('active'); });

    addPlantForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const payload = { typeId: parseInt(document.getElementById('modal-plant-type-id').value), newPlantName: document.getElementById('modal-plant-name').value, notes: document.getElementById('modal-plant-notes').value };
        showToast('Aggiunta pianta in corso...');
        try {
            await apiFetch('/add_plant', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) });
            addPlantModal.classList.remove('active');
            addPlantForm.reset();
            showToast('Pianta aggiunta! Aggiorno il giardino...', 'success');
            await initializeApp(true);
        } catch (error) {
            showToast('Errore nell\'aggiunta della pianta.', 'error');
        }
    });

    document.getElementById('garden-plants-container').addEventListener('click', async (e) => {
        const button = e.target.closest('button');
        if (!button) return;
        const action = button.dataset.action;
        const plantId = button.dataset.plantId;

        if (action === 'measure') {
            navigateTo('measurement-page');
            document.getElementById('measurement-results-container').innerHTML = `<div class="spinner-container"><div class="spinner"></div><p>Avvio misurazione...</p><p>(In attesa di dati dalla sonda via WebSocket)</p></div>`;
            try {
                await apiFetch('/start_measurement', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ plantId: parseInt(plantId) }) });
            } catch (error) {
                navigateTo('home-page');
            }
        }

        if (action === 'history') {
             try {
                showToast('Caricamento storico...');
                const data = await apiFetch(`/measurements?plantId=${plantId}`);
                state.currentHistoryData = data;
                navigateTo('history-page');
                renderHistory(data);
                
                const aiContainer = document.getElementById('history-ai-advice-container');
                aiContainer.classList.add('hidden');
                
                const measurements = data.measurements || data.misurazioni || [];
                if (measurements.length > 0) {
                    const latestMeasurement = measurements[measurements.length - 1];
                    latestMeasurement.plantId = parseInt(plantId);
                    fetchAndDisplayAiAdvice(latestMeasurement, aiContainer);
                }
             } catch (error) {
                 console.error('Processo di caricamento storico fallito:', error);
                 showToast('Errore caricamento storico.', 'error');
                 navigateTo('home-page');
             }
        }
    });

    showChartButton.addEventListener('click', renderHistoryChart);
    document.getElementById('filter-plant-type').addEventListener('change', renderGarden);
    document.getElementById('filter-sort').addEventListener('change', renderGarden);

    const showToast = (message, type = 'info') => {
        toast.textContent = message;
        toast.className = `toast show ${type}`;
        setTimeout(() => { toast.className = 'toast'; }, 3000);
    };

    connectWebSocket();
});
