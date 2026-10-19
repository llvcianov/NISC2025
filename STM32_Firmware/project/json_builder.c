/* ---------------------------------------------------------------------------
 * json_builder.c
 * ---------------------------------------------------------------------------
 * In questo file risiedono le funzioni che "impacchettano" i dati in
 * messaggi JSON da inviare all'ESP8266.
 *
 * VERSIONE CORRETTA E STABILE
 * -------------------------------------------------------------------------*/

#include "hal.h"
#include "chprintf.h"
#include <cmd_defs.h>
#include <json_builder.h>
#include <string.h>

/* ... (build_init_data e build_ack rimangono invariati) ... */
int build_init_data(char* buf, size_t len) {
    size_t pos = 0;
    uint32_t nPlants = 0;
    pos += chsnprintf(buf + pos, len - pos, "{\"type\":\"INIT_DATA\",\"plantTypes\":[");
    for (int i = 0; i < MAX_PLANT_PROFILES && plant_db[i].idTipo != 0; ++i) {
        if (i > 0) pos += chsnprintf(buf + pos, len - pos, ",");
        pos += chsnprintf(buf + pos, len - pos,
                        "{\"id\":%d,\"name\":\"%s\"}",
                        plant_db[i].idTipo, plant_db[i].nomeTipo);
    }
    pos += chsnprintf(buf + pos, len - pos, "],\"registeredPlants\":[");
    const MiaPianta* plants = get_elenco_piante(&nPlants);
    for (uint32_t i = 0; i < nPlants; ++i) {
        if (i > 0) pos += chsnprintf(buf + pos, len - pos, ",");
        pos += chsnprintf(buf + pos, len - pos,
                        "{\"typeId\":%d,\"plantId\":%u,\"name\":\"%s\"}",
                        plants[i].idTipo, plants[i].idPianta, plants[i].nomePianta);
    }
    pos += chsnprintf(buf + pos, len - pos, "]}");
    return (pos < len) ? (int)pos : -1;
}

int build_ack(char* buf, size_t len) {
    return chsnprintf(buf, len, "{\"type\":\"ACK\"}");
}

int build_measurement(const Misurazione* m, char* buf, size_t len) {
    return chsnprintf(buf, len,
        "{\"type\":\"MEAS_RESULTS\","
        "\"temperatura_valore\":%d,\"luce_valore\":%d,"
        "\"umidita_esterna_valore\":%d,\"umidita_interna_valore\":%d,"
        "\"temperatura_score\":%d,\"luce_score\":%d,\"umidita_esterna_score\":%d,"
        "\"umidita_interna_score\":%d,\"score_totale\":%d,"
        "\"temperatura_stringa\":\"%s\",\"luce_stringa\":\"%s\","
        "\"umidita_esterna_stringa\":\"%s\",\"umidita_interna_stringa\":\"%s\"}",
        m->temperatura_c, m->luce_l, m->umi_out, m->umi_in,
        m->v_temp, m->v_luce, m->v_umi_out, m->v_umi_in, m->v_tot,
        score_strings[m->idx_temp_str], score_strings[m->idx_luce_str],
        score_strings[m->idx_umi_out_str], score_strings[m->idx_umi_in_str]);
}


/* ---------------------------------------------------------------------------
 * build_history (MODIFICATO)
 * ---------------------------------------------------------------------------
 * Costruisce uno storico delle misurazioni per una pianta specifica.
 * -------------------------------------------------------------------------*/
int build_history(uint32_t idPianta, char* buf, size_t len) {
    // --- MODIFICA CHIAVE: Buffer reso 'static' per evitare STACK OVERFLOW ---
    // Dichiarare un array di 100*60 byte sullo stack causava un crash.
    // 'static' alloca il buffer in un'area di memoria globale, risolvendo il problema.
    static Misurazione tmp[MAX_MEASUREMENTS];
    uint32_t count = get_measurements(tmp, MAX_MEASUREMENTS);

    int totalScore = 0;
    int entries = 0;
    size_t pos = chsnprintf(buf, len,
                          "{\"type\":\"HISTORY_RESULTS\",\"misurazioni\":[");

    for (uint32_t i = 0; i < count; ++i) {
        if (tmp[i].idPianta != idPianta) continue;

        if (entries++ > 0) {
            pos += chsnprintf(buf + pos, len - pos, ",");
        }

        // --- NOTA: Il timestamp qui è un placeholder ---
        // Se si implementa l'RTC, bisognerà sostituire tmp[i].timestamp.year
        // con una conversione del timestamp reale in secondi.
        pos += chsnprintf(buf + pos, len - pos,
                        "{\"timestamp\":%lu,"
                        "\"temperatura_score\":%d,\"luce_score\":%d,"
                        "\"umidita_esterna_score\":%d,\"umidita_interna_score\":%d,"
                        "\"score_totale\":%d}",
                        (unsigned long)tmp[i].magic, // Placeholder temporaneo
                        tmp[i].v_temp, tmp[i].v_luce,
                        tmp[i].v_umi_out, tmp[i].v_umi_in, tmp[i].v_tot);
    }

    int avg = (entries > 0) ? totalScore / entries : 0;
    pos += chsnprintf(buf + pos, len - pos, "],\"score_avg\":%d}", avg);

    return (pos < len) ? (int)pos : -1;
}
