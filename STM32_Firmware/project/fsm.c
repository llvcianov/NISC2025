#include "ch.h"
#include "hal.h"
#include <adc_module.h>
#include <com_module.h>
#include <dht11_module.h>
#include <fsm.h>
#include <json_builder.h>
#include <meas_proc.h>
#include <oled_module.h>
#include <rtc_module.h>
#include <serial_module.h>
#include <TCS3200_module.h>
#include <stdio.h>

// Variabile globale statica per mantenere lo stato corrente
static app_state_t sys_state;

// Buffer per conservare l'ultimo comando ricevuto dalla Web App
static Cmd current_cmd;

// Buffer di grandi dimensioni per costruire i messaggi JSON da inviare
#define JSON_BUFFER_SIZE 8096
static char json_buffer[JSON_BUFFER_SIZE];


void fsm_init(void) {
    sys_state = STATE_INIT;
}

void fsm_update(void) {
    switch (sys_state) {

        case STATE_INIT:
            serial_printf("[FSM] Stato: INIT\r\n");
            // Inizializza tutti i moduli necessari
            com_init();
            adc_init();
            tcs3200_init();

            // --- MODIFICA CHIAVE: Inizializzazione OLED corretta e stabile ---
            oled_init();      // 1. Configura solo i pin GPIO
            oled_start();     // 2. Avvia i driver I2C e SSD1306 e pulisce lo schermo

            rtc_module_init();
            init_runtime_storage(); // Prepara la RAM con i dati demo
            serial_printf("[FSM] Inizializzazione completata.\r\n");
            // Passa allo stato successivo
            sys_state = STATE_WAIT_RTC;
            break;

        case STATE_WAIT_RTC:
            oled_print_string("Collega...", 0, 0);
            // Controlla se è arrivato un comando dalla Web App
            if (com_receive_command(&current_cmd)) {
                // Se il comando è quello per impostare l'orologio...
                if (current_cmd.type == CMD_SET_RTC) {
                    // ...chiama la nostra nuova funzione per impostare l'RTC dal timestamp
                    rtc_set_from_timestamp(current_cmd.timestamp);
                    // E passa allo stato di invio dati
                    sys_state = STATE_SEND_INIT_DATA;
                }
                // Gestiamo anche il caso in cui l'app chieda i dati senza impostare l'RTC
                else if (current_cmd.type == CMD_GET_INIT) {
                    serial_printf("[FSM] Ricevuto GET_INIT, procedo...\r\n");
                    sys_state = STATE_SEND_INIT_DATA;
                }
            }
            break;

        case STATE_SEND_INIT_DATA:
            serial_printf("[FSM] Stato: SEND_INIT_DATA\r\n");
            oled_print_string("Invio dati...", 0, 0);
            // Costruisce il grande JSON con catalogo e istanze
            if (build_init_data(json_buffer, JSON_BUFFER_SIZE) > 0) {
                // Invia il JSON tramite il modulo di comunicazione
                com_send_json(json_buffer);
            } else {
                serial_printf("[FSM_ERR] Buffer troppo piccolo per INIT_DATA!\r\n");
            }
            // Passa allo stato di attesa comandi
            sys_state = STATE_IDLE;
            break;

        case STATE_IDLE:
            oled_print_string("Pronto.", 0, 0);
            // In questo stato, il sistema attende un'azione dall'utente
            if (com_receive_command(&current_cmd)) {
                // A seconda del comando ricevuto, cambia stato
                switch (current_cmd.type) {
                    case CMD_ADD_PLANT:   sys_state = STATE_ADD_PLANT;    break;
                    case CMD_START_MEAS:  sys_state = STATE_MEASURE;      break;
                    case CMD_GET_HISTORY: sys_state = STATE_SEND_HISTORY; break;
                    case CMD_GET_INIT:    sys_state = STATE_SEND_INIT_DATA; break; // Per ricaricare
                    default:
                        serial_printf("[FSM] Comando non gestito in IDLE: %d\r\n", current_cmd.type);
                        break;
                }
            }
            break;

        case STATE_ADD_PLANT:
            serial_printf("[FSM] Stato: ADD_PLANT\r\n");
            oled_print_string("Aggiungo...", 0, 0);

            // 1. Chiama la funzione di logica per aggiungere la pianta alla RAM
            int new_id = registra_nuova_pianta(current_cmd.nomePianta, current_cmd.idTipo);
            if (new_id > 0) {
                serial_printf("[FSM] Pianta '%s' aggiunta con ID %d.\r\n", current_cmd.nomePianta, new_id);
            } else {
                serial_printf("[FSM_ERR] Impossibile aggiungere la pianta (spazio esaurito?).\r\n");
            }

            // 2. Costruisce e invia una risposta di conferma (ACK)
            if (build_ack(json_buffer, JSON_BUFFER_SIZE) > 0) {
                com_send_json(json_buffer);
            }

            // 3. Ritorna allo stato IDLE
            sys_state = STATE_IDLE;
            break;

        case STATE_MEASURE:
            serial_printf("[FSM] Stato: MEASURE per idPianta %u\r\n", current_cmd.idPianta);
            oled_print_string("Premi...", 0, 0);

            button_wait();

            oled_print_string("Misuro...", 0, 0);

            // --- Passo 1: Lettura dei dati dai sensori ---
            int temp_c, hr_perc, umi_suolo_perc;
            float luce_lux;

            // Lettura temperatura e umidità aria
            dht11_get_media(&temp_c, &hr_perc);
            serial_printf("[FSM_MEAS] DHT11 -> Temp: %d C, Umid. Aria: %d %%\r\n", temp_c, hr_perc);

            // Lettura umidità suolo
            umi_suolo_perc = readADCPercent();
            serial_printf("[FSM_MEAS] ADC   -> Umid. Suolo: %d %%\r\n", umi_suolo_perc);

            // Lettura luce
            luce_lux = tcs3200_read_lux();
            serial_printf("[FSM_MEAS] TCS   -> Luce: %.2f Lux\r\n", luce_lux);

            // --- Passo 2: Esecuzione della logica di business ---
            Misurazione risultato_misura;
            risultato_misura = esegui_pipeline_misurazione(temp_c, (int)luce_lux, hr_perc, umi_suolo_perc, current_cmd.idPianta);


            // Controllo di sicurezza: la pipeline ha funzionato?
            if (risultato_misura.magic != MEASUREMENT_MAGIC) {
                serial_printf("[FSM_ERR] Pipeline di misurazione fallita (pianta non trovata?).\r\n");
                sys_state = STATE_IDLE;
                break;
            }
            serial_printf("[FSM_MEAS] Pipeline OK. Score totale: %d\r\n", risultato_misura.v_tot);

            // --- Passo 3: Costruzione e invio della risposta JSON ---
            if (build_measurement(&risultato_misura, json_buffer, JSON_BUFFER_SIZE) > 0) {
                com_send_json(json_buffer);
            } else {
                serial_printf("[FSM_ERR] Buffer troppo piccolo per MEAS_RESULTS!\r\n");
            }

            // Ritorna allo stato di attesa
            sys_state = STATE_IDLE;
            break;

        case STATE_SEND_HISTORY:
            serial_printf("[FSM] Stato: SEND_HISTORY per idPianta %u\r\n", current_cmd.idPianta);
            oled_print_string("Invio storico...", 0, 0);

            // 1. Chiama la funzione che costruisce il JSON dello storico
            if (build_history(current_cmd.idPianta, json_buffer, JSON_BUFFER_SIZE) > 0) {
                // 2. Invia il JSON alla web app
                com_send_json(json_buffer);
            } else {
                serial_printf("[FSM_ERR] Buffer troppo piccolo per HISTORY_RESULTS!\r\n");
            }

            // 3. Ritorna allo stato di attesa
            sys_state = STATE_IDLE;
            break;

        default:
            serial_printf("[FSM] Stato sconosciuto o non implementato: %d\r\n", sys_state);
            sys_state = STATE_IDLE; // Torna a uno stato sicuro
            break;
    }
}
