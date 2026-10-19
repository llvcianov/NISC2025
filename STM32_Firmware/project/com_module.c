#include <com_module.h>
#include <json_parser.h>
#include <serial_module.h>
#include <wifi_module.h>
#include <string.h>
#include <stdio.h>
#include "chprintf.h"

#define COM_BUFFER_SIZE 8192
static char rx_buffer[COM_BUFFER_SIZE];
static char tx_buffer[COM_BUFFER_SIZE];

void com_init(void) {
    wifi_init();
}

void com_send_json(const char* json_string) {
    int len = chsnprintf(tx_buffer, COM_BUFFER_SIZE, "%s\n", json_string);

    if (len > 0 && len < COM_BUFFER_SIZE) {
        wifi_write(tx_buffer);
        serial_printf("[COM] >> %s", tx_buffer);

        // --- MODIFICA CHIAVE: Pulizia del buffer di ricezione ---
        // Dopo aver inviato un messaggio, puliamo il buffer di ricezione
        // per prevenire la lettura di comandi incompleti o corrotti.
        wifi_flush_input();
    } else {
        serial_printf("[COM_ERR] Errore nella creazione del JSON da inviare!\r\n");
    }
}

bool com_receive_command(Cmd* cmd) {
    size_t len = wifi_readLine(rx_buffer, COM_BUFFER_SIZE, TIME_MS2I(CMD_READ_TIMEOUT));

    if (len > 0) {
        serial_printf("[COM] << %s\r\n", rx_buffer);
        if (parse_command(rx_buffer, cmd)) {
            return true;
        } else {
            serial_printf("[COM_ERR] Parsing del comando fallito!\r\n");
            return false;
        }
    }
    return false;
}
