#include "ch.h"
#include "hal.h"
#include "fsm.h"
#include "serial_module.h"
//#include "serial_module.h"

int main(void) {
    // 1. Inizializzazioni di base di ChibiOS e dei driver HAL
    halInit();
    chSysInit();

    // 2. Inizializza la seriale per il debug, così possiamo vedere i log
    serial_init();
    serial_printf("\r\n--- Sistema Avviato ---\r\n");

    // 3. Inizializza la nostra macchina a stati
    fsm_init();

    // 4. Loop infinito dell'applicazione
    while(true) {
        // Chiama la funzione di aggiornamento della FSM.
        // Tutta la logica applicativa risiede qui.
        fsm_update();

        // Aggiungiamo un piccolissimo sleep per cedere il controllo alla CPU
        // ed evitare di girare a vuoto al 100% della velocità.
        // 50ms è un buon compromesso.
        chThdSleepMilliseconds(50);
    }
}
