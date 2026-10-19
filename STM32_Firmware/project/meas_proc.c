#include <meas_proc.h>
#include <string.h> // Necessario per memset e memcpy
#include <stdlib.h>

// ============================================================================
// Database Dinamico in RAM (Struttura invariata)
// ============================================================================
static struct {
    MiaPianta elenco_piante[MAX_USER_PLANTS];
    uint32_t numero_piante_attuali;
    Misurazione elenco_misure[MAX_MEASUREMENTS];
    uint32_t numero_misure_attuali;
} ram_db;

// ============================================================================
// Funzione di Inizializzazione (Invariata)
// ============================================================================
void init_runtime_storage(void) {
    memset(&ram_db, 0, sizeof(ram_db));
    memcpy(ram_db.elenco_piante, demo_plant_list, sizeof(demo_plant_list));
    ram_db.numero_piante_attuali = DEMO_PLANTS_COUNT;
    memcpy(ram_db.elenco_misure, demo_measurement_list, sizeof(demo_measurement_list));
    ram_db.numero_misure_attuali = DEMO_MEAS_COUNT;
}

// ============================================================================
// Funzioni di Gestione Piante (CORRETTE)
// ============================================================================

/**
 * @brief CORRETTO: Registra una nuova pianta con un ID univoco.
 * @details Calcola il massimo ID esistente e assegna un ID pari a max+1,
 *          evitando duplicati con gli ID delle piante demo.
 * @return L'ID della nuova pianta (>=1) o 0 in caso di errore (spazio esaurito).
 */
int registra_nuova_pianta(const char* nomePianta, int idTipo) {
    if (ram_db.numero_piante_attuali >= MAX_USER_PLANTS) {
        return 0; // Spazio esaurito
    }

    // --- LOGICA CORRETTA PER GENERARE UN ID UNIVOCO ---
    uint32_t max_id = 0;
    for (uint32_t i = 0; i < ram_db.numero_piante_attuali; i++) {
        if (ram_db.elenco_piante[i].idPianta > max_id) {
            max_id = ram_db.elenco_piante[i].idPianta;
        }
    }
    uint32_t nuovoId = max_id + 1;
    // --- FINE LOGICA CORRETTA ---

    uint32_t idx = ram_db.numero_piante_attuali;
    ram_db.elenco_piante[idx].idPianta = nuovoId;
    ram_db.elenco_piante[idx].idTipo = idTipo;
    strncpy(ram_db.elenco_piante[idx].nomePianta, nomePianta, MAX_PLANT_NAME_LEN - 1);
    ram_db.elenco_piante[idx].nomePianta[MAX_PLANT_NAME_LEN - 1] = '\0';
    ram_db.numero_piante_attuali++;
    return nuovoId;
}

/**
 * @brief CORRETTO: Recupera una pianta dall'archivio in RAM tramite il suo ID.
 * @details Esegue una ricerca lineare nell'array delle piante per trovare
 *          quella con l'ID corrispondente, senza fare assunzioni sulla posizione.
 * @return Un puntatore const alla pianta se trovata, altrimenti NULL.
 */
const MiaPianta* get_mia_pianta_by_id(uint32_t idPianta) {
    // Scorre tutte le piante attualmente registrate in RAM.
    for (uint32_t i = 0; i < ram_db.numero_piante_attuali; ++i) {
        // Confronta l'ID di ogni pianta con quello cercato.
        if (ram_db.elenco_piante[i].idPianta == idPianta) {
            // Se trova una corrispondenza, restituisce un puntatore a quella pianta.
            return &ram_db.elenco_piante[i];
        }
    }
    // Se il ciclo finisce senza trovare la pianta, restituisce NULL.
    return NULL;
}

// ============================================================================
// Altre funzioni (tutte invariate)
// ============================================================================

const MiaPianta* get_elenco_piante(uint32_t* count) {
    *count = ram_db.numero_piante_attuali;
    return ram_db.elenco_piante;
}

const PlantProfile* get_plant_profile(int idTipo) {
    for (size_t i = 0; i < MAX_PLANT_PROFILES; ++i) {
        if (plant_db[i].idTipo == idTipo) {
            return &plant_db[i];
        }
    }
    return NULL;
}

static void save_measurement_in_ram(const Misurazione *m) {
    if (ram_db.numero_misure_attuali >= MAX_MEASUREMENTS) {
        return;
    }
    ram_db.elenco_misure[ram_db.numero_misure_attuali] = *m;
    ram_db.numero_misure_attuali++;
}

static void valuta_parametro(int valore, int riferimento, int *score, uint8_t *idx_str, int idx_base) {
    if (riferimento <= 0) { *score = 0; *idx_str = idx_base; return; }
    int diff = valore - riferimento;
    int perc = ((int64_t)diff * 100) / riferimento;
    if (perc >= -20 && perc <= 20) { *score = 25; *idx_str = idx_base; }
    else if (perc < -20 && perc >= -40) { *score = 20; *idx_str = idx_base + 1; }
    else if (perc > 20 && perc <= 40) { *score = 20; *idx_str = idx_base + 2; }
    else if (perc < -40) { *score = 10; *idx_str = idx_base + 3; }
    else { *score = 10; *idx_str = idx_base + 4; }
}

Misurazione esegui_pipeline_misurazione(int temp, int luce, int umi_out, int umi_in, uint32_t idPianta) {
    Misurazione m;
    memset(&m, 0, sizeof(Misurazione));

    const MiaPianta* p = get_mia_pianta_by_id(idPianta); // Ora questa chiamata funzionerà
    if (!p) {
        m.magic = 0;
        return m;
    }

    m.magic = MEASUREMENT_MAGIC;
    m.idPianta = idPianta;
    m.idTipo = p->idTipo;
    // TODO: Popolare m.timestamp se si aggiunge un RTC

    m.temperatura_c = temp;
    m.luce_l = luce;
    m.umi_out = umi_out;
    m.umi_in = umi_in;

    const PlantProfile *prof = get_plant_profile(p->idTipo);
    if (prof) {
        valuta_parametro(temp, prof->temp_ott, &m.v_temp, &m.idx_temp_str, 0);
        valuta_parametro(luce, prof->luce_ott, &m.v_luce, &m.idx_luce_str, 5);
        valuta_parametro(umi_out, prof->umi_out_ott, &m.v_umi_out, &m.idx_umi_out_str, 10);
        valuta_parametro(umi_in, prof->umi_in_ott, &m.v_umi_in, &m.idx_umi_in_str, 15);
        m.v_tot = m.v_temp + m.v_luce + m.v_umi_out + m.v_umi_in + (rand()%20) - (rand()%25);
    }

    save_measurement_in_ram(&m);
    return m;
}

uint32_t get_measurements(Misurazione *buffer, uint32_t max_count) {
    if (buffer == NULL || max_count == 0) {
        return 0;
    }
    uint32_t count_to_copy = ram_db.numero_misure_attuali;
    if (count_to_copy > max_count) {
        count_to_copy = max_count;
    }
    if (count_to_copy > 0) {
        memcpy(buffer, ram_db.elenco_misure, count_to_copy * sizeof(Misurazione));
    }
    return count_to_copy;
}
