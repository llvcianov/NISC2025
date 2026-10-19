# Sonda Smart per Piante - Progetto ESP8266 + STM32 (v4.0)

Questa versione introduce importanti funzionalità per rendere il debug e l'utilizzo del dispositivo più robusti e flessibili.

---

## Funzionalità Principali

- **Flag Globale per la Simulazione**  
  Dalla pagina del simulatore è ora possibile attivare o disattivare un "master switch" che abilita o disabilita completamente tutta la logica di simulazione nel firmware.  
  Quando disattivato, il dispositivo si affiderà esclusivamente all'STM32.

- **Log Seriale Pulito e Controllabile**  
  I messaggi di debug del firmware (stato WiFi, IP, etc.) possono essere attivati o disattivati tramite un interruttore nella pagina del simulatore.  
  Questo permette di avere un output sulla porta seriale contenente solo il protocollo JSON, facilitando il debug della comunicazione.

- **Connessione WiFi Intelligente con Timeout**  
  All'avvio, il dispositivo tenterà di connettersi alla rete WiFi salvata per 30 secondi.  
  Se non ci riesce, avvierà in automatico il suo Access Point, garantendo di essere sempre raggiungibile.

---

## Requisiti Hardware e Software

I requisiti rimangono invariati rispetto alla versione precedente.

---

## Configurazione API Google Gemini

La configurazione della chiave API di Gemini nel file `esp_plant_probe.ino` è ancora necessaria per la funzionalità di consiglio AI.

---

## Istruzioni per l'Installazione e l'Uso

Le istruzioni per il caricamento dei file e del firmware non cambiano.

---

## Utilizzo delle Nuove Funzionalità

- Naviga alla pagina del simulatore:  
  [http://plantpen.local/simulator](http://plantpen.local/simulator)

- **Attiva/Disattiva Simulatore Master**  
  Usa il primo interruttore per decidere se l'ESP8266 deve usare la sua logica interna di simulazione (ON)  
  o se deve comunicare esclusivamente con l'STM32 (OFF).

- **Abilita/Disabilita Log Seriale**  
  Usa il secondo interruttore per controllare se i messaggi di debug del firmware (non JSON) devono essere stampati sulla porta seriale.  
  Disattivalo per vedere solo il protocollo di comunicazione pulito.
