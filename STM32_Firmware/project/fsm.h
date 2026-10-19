#ifndef FSM_H
#define FSM_H

// Definizione di tutti gli stati possibili della nostra applicazione
typedef enum {
    STATE_INIT,             // Stato iniziale di setup
    STATE_WAIT_RTC,         // Attesa impostazione orologio
    STATE_SEND_INIT_DATA,   // Invio dati iniziali alla web app
    STATE_IDLE,             // Attesa comandi
    STATE_ADD_PLANT,        // Aggiunta di una nuova pianta
    STATE_MEASURE,          // Esecuzione di una misurazione
    STATE_SEND_HISTORY      // Invio dello storico misurazioni
} app_state_t;

/**
 * @brief Inizializza la macchina a stati.
 * @details Imposta lo stato iniziale del sistema.
 */
void fsm_init(void);

/**
 * @brief Esegue un ciclo della macchina a stati.
 * @details Contiene la logica principale dell'applicazione, che viene
 *          eseguita in base allo stato corrente.
 */
void fsm_update(void);

#endif // FSM_H
