#ifndef COM_MODULE_H
#define COM_MODULE_H

#include <cmd_defs.h>
#include <stdbool.h>

// Timeout per la lettura di un comando in millisecondi
#define CMD_READ_TIMEOUT 100

/**
 * @brief Inizializza il modulo di comunicazione (chiama wifi_init).
 */
void com_init(void);

/**
 * @brief Invia un messaggio JSON al modulo WiFi.
 * @details Aggiunge un terminatore '\n' al messaggio prima di inviarlo.
 * @param json_string La stringa JSON da inviare.
 */
void com_send_json(const char* json_string);

/**
 * @brief Tenta di ricevere un comando JSON dal modulo WiFi.
 * @details Legge una riga dalla seriale WiFi e ne esegue il parsing.
 *          Questa funzione non è bloccante; ritorna subito se non ci sono dati.
 * @param cmd Puntatore a una struttura Cmd che verrà riempita se un comando valido viene ricevuto.
 * @return true se un comando è stato ricevuto e parsato correttamente, false altrimenti (timeout o errore).
 */
bool com_receive_command(Cmd* cmd);

#endif
