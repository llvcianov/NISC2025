#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include "ch.h"
#include "hal.h"
#include <stddef.h>
#include <stdint.h>

void wifi_init(void);
size_t wifi_write(const char *data);
size_t wifi_readLine(char *buf, size_t maxlen, systime_t timeout);
size_t wifi_read(uint8_t *buf, size_t len, systime_t timeout);

/**
 * @brief Svuota il buffer di ricezione della seriale.
 * @details Legge e scarta tutti i byte pendenti nel buffer di ricezione
 *          per prevenire la lettura di dati parziali o corrotti.
 */
void wifi_flush_input(void);

#endif
