#include "wifi_module.h"
#include "chprintf.h"
#include <string.h>

// Timeout inter-carattere robusto: abbastanza lungo per tollerare le pause,
// ma non così lungo da bloccare il sistema.
#define WIFI_INTERCHAR_TIMEOUT_MS 75

static SerialConfig wifi_cfg = {
    115200, 0, USART_CR2_STOP1_BITS, 0
};

#define WIFI_TX_PORT   GPIOC
#define WIFI_TX_PIN    4
#define WIFI_RX_PORT   GPIOC
#define WIFI_RX_PIN    5
#define WIFI_SD        SD1

void wifi_init(void) {
    palSetPadMode(WIFI_TX_PORT, WIFI_TX_PIN, PAL_MODE_ALTERNATE(7));
    palSetPadMode(WIFI_RX_PORT, WIFI_RX_PIN, PAL_MODE_ALTERNATE(7));
    sdStart(&WIFI_SD, &wifi_cfg);
}

size_t wifi_write(const char *data) {
    return sdWrite(&WIFI_SD, (const uint8_t *)data, strlen(data));
}

/**
 * @brief Svuota il buffer di ricezione della seriale.
 */
void wifi_flush_input(void) {
    uint8_t dummy_buffer[16];
    // Legge piccoli chunk con un timeout brevissimo finché il buffer non è vuoto.
    while (sdReadTimeout(&WIFI_SD, dummy_buffer, sizeof(dummy_buffer), TIME_MS2I(10)) > 0) {
        // Il loop continua finché sdReadTimeout restituisce byte letti
    }
}

size_t wifi_readLine(char *buf, size_t maxlen, systime_t timeout) {
    size_t idx = 0;
    uint8_t c;
    msg_t result;

    result = sdReadTimeout(&WIFI_SD, &c, 1, timeout);
    if (result == 0) return 0;

    if (c == '\n' || c == '\r') return 0;
    buf[idx++] = (char)c;

    systime_t interchar_timeout = TIME_MS2I(WIFI_INTERCHAR_TIMEOUT_MS);
    while (idx < maxlen - 1) {
        result = sdReadTimeout(&WIFI_SD, &c, 1, interchar_timeout);
        if (result == 0) break;
        if (c == '\n') break;
        if (c != '\r') buf[idx++] = (char)c;
    }

    buf[idx] = '\0';
    return idx;
}

size_t wifi_read(uint8_t *buf, size_t len, systime_t timeout) {
    return sdReadTimeout(&WIFI_SD, buf, len, timeout);
}
