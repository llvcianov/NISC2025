#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <cmd_defs.h>   // Per la definizione della struct Cmd
#include <stdbool.h>    // Per il tipo 'bool'

/**
 * @brief Esegue il parsing di una stringa JSON per estrarre un comando.
 * @details Analizza il JSON in ingresso e, se valido, riempie una struttura Cmd
 *          con i dati del comando. Utilizza la libreria JSMN.
 * @param json La stringa JSON da analizzare.
 * @param out Puntatore a una struttura Cmd che verrà riempita con i risultati.
 * @return true se un comando valido è stato riconosciuto e parsato, false altrimenti.
 */
bool parse_command(const char* json, Cmd* out);

#endif // JSON_PARSER_H
