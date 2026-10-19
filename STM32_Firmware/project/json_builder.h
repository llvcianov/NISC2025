#ifndef JSON_BUILDER_H
#define JSON_BUILDER_H

#include <meas_proc.h> // Per le definizioni di Misurazione, MiaPianta, etc.
#include <stddef.h>    // Per il tipo 'size_t'

/**
 * @brief Costruisce il JSON con i dati di inizializzazione (catalogo e istanze).
 * @param buf Buffer di destinazione per il JSON.
 * @param len Dimensione del buffer.
 * @return Numero di caratteri scritti o -1 in caso di errore (buffer insufficiente).
 */
int build_init_data(char* buf, size_t len);

/**
 * @brief Costruisce un semplice JSON di conferma (ACK).
 * @param buf Buffer di destinazione per il JSON.
 * @param len Dimensione del buffer.
 * @return Numero di caratteri scritti o valore negativo in caso di errore.
 */
int build_ack(char* buf, size_t len);

/**
 * @brief Costruisce il JSON che serializza una singola misurazione.
 * @param m Puntatore alla misurazione da serializzare.
 * @param buf Buffer di destinazione per il JSON.
 * @param len Dimensione del buffer.
 * @return Numero di caratteri scritti o valore negativo in caso di errore.
 */
int build_measurement(const Misurazione* m, char* buf, size_t len);

/**
 * @brief Costruisce il JSON con lo storico delle misurazioni per una pianta.
 * @param idPianta ID della pianta di cui si vuole lo storico.
 * @param buf Buffer di destinazione per il JSON.
 * @param len Dimensione del buffer.
 * @return Numero di caratteri scritti o -1 in caso di errore (buffer insufficiente).
 */
int build_history(uint32_t idPianta, char* buf, size_t len);

#endif // JSON_BUILDER_H
