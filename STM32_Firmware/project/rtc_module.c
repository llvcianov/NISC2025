#include <rtc_module.h>
#include <serial_module.h>
#include "chprintf.h"
#include "hal_rtc.h"

// Driver RTC
static RTCDriver *rtc = &RTCD1;

// --- INIZIO NUOVA FUNZIONE DI CONVERSIONE ---

/**
 * @brief Funzione helper per determinare se un anno è bisestile.
 * @param year L'anno da controllare.
 * @return 1 se bisestile, 0 altrimenti.
 */
static int is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * @brief Converte un timestamp UNIX in una struttura RTCDateTime.
 * @param unix_timestamp  Secondi trascorsi dal 1/1/1970.
 * @param[out] timespec   Puntatore alla struttura RTCDateTime da popolare.
 */
static void unix_to_datetime(uint32_t unix_timestamp, RTCDateTime *timespec) {
    uint32_t days, seconds_in_day;
    uint16_t year;
    const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Calcola i secondi trascorsi dall'inizio del giorno corrente
    seconds_in_day = unix_timestamp % 86400;
    timespec->millisecond = (seconds_in_day % 60) * 1000;
    timespec->millisecond += ((seconds_in_day / 60) % 60) * 60000;
    timespec->millisecond += (seconds_in_day / 3600) * 3600000;

    // Calcola il numero totale di giorni dall'epoca UNIX
    days = unix_timestamp / 86400;
    timespec->dayofweek = (days + 4) % 7 + 1; // 1/1/1970 era Giovedì

    // --- CICLO CORRETTO ---
    // Sottrae i giorni di ogni anno fino a trovare l'anno corrente.
    for (year = 1970; days >= (uint32_t)(365 + is_leap_year(year)); year++) {
        days -= (365 + is_leap_year(year));
    }

    timespec->year = year - 1980; // Offset richiesto da ChibiOS

    // Calcola mese e giorno
    for (timespec->month = 1; timespec->month <= 12; timespec->month++) {
        uint8_t dim = days_in_month[timespec->month - 1];
        if (timespec->month == 2 && is_leap_year(year)) {
            dim = 29; // Febbraio in anno bisestile
        }
        if (days < dim) {
            break;
        }
        days -= dim;
    }
    timespec->day = days + 1;
}

// --- FINE SEZIONE CONVERSIONE ---

void rtc_module_init(void){
    rtcObjectInit(rtc);
}

void rtc_set_from_timestamp(uint32_t timestamp) {
    RTCDateTime timespec;
    unix_to_datetime(timestamp, &timespec); // Usa la nostra funzione corretta
    rtcSetTime(rtc, &timespec);
    serial_printf("[RTC] Timestamp %u impostato correttamente.\r\n", timestamp);
}

void rtc_get_time(char *dateBuff, size_t dateBuffSize, char *timeBuff, size_t timeBuffSize){
    RTCDateTime dt;
    rtcGetTime(rtc, &dt);

    chsnprintf(dateBuff, dateBuffSize, "%02u/%02u/%04u",
               (unsigned)dt.day, (unsigned)dt.month, (unsigned)dt.year + 1980);

    uint32_t msec = dt.millisecond;
    uint32_t hours = msec / 3600000;
    msec %= 3600000;
    uint32_t minutes = msec / 60000;
    msec %= 60000;
    uint32_t seconds = msec / 1000;

    chsnprintf(timeBuff, timeBuffSize, "%02u:%02u:%02u",
               (unsigned)hours, (unsigned)minutes, (unsigned)seconds);
}
