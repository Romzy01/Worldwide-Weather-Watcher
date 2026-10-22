/**
 * @file    mesures.c
 * @brief   Acquisition non bloquante de tous les capteurs (voir mesures.h).
 *
 *          Déroulement d'un relevé :
 *            1. mesures_demarrer() lit la RTC et fait une première tentative
 *               sur le BME280 et le capteur de lumière ;
 *            2. mesures_poursuivre(), à chaque tour de boucle, réessaie les
 *               capteurs muets toutes les 200 ms et attend une trame GGA
 *               récente, sans jamais bloquer ;
 *            3. au bout de TIMEOUT secondes, un capteur toujours muet passe à NA
 *               et son compteur de timeouts augmente (erreur à partir de 2).
 */
#include "mesures.h"
#include "capteur_bme280.h"
#include "gps.h"
#include "config.h"
#include "grove_light.h"
#include "grove_rtc_ds1307.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <string.h>

#define PERIODE_ESSAI_MS     200UL    /* nouvelle tentative sur un capteur muet */
#define FRAICHEUR_GPS_MS     2000UL   /* une trame GGA de moins de 2 s est acceptée */
#define LUMIERE_ERREUR       0xFFFF   /* valeur renvoyée par GroveLight_ReadRaw() */

static bool       en_cours = false;
static uint32_t   t_debut, t_dernier_essai;
static bool       fini[NB_CAPTEURS];      /* valeur obtenue, désactivée ou NA     */
static bool       repondu[NB_CAPTEURS];   /* le capteur a répondu pendant ce relevé */
static bool       demande[NB_CAPTEURS];   /* le capteur fait partie de ce relevé  */
static uint8_t    nb_timeouts[NB_CAPTEURS];
static bool       erreur_rtc;
static CodeErreur derniere_erreur = ERR_AUCUNE;
static Mesures    courant;

bool mesures_horodater(RTC_DateTime *dt)
{
    if (GroveRTC_GetDateTime(dt)) return true;
    return GroveRTC_GetDateTime(dt);   /* une seconde tentative */
}

void mesures_init(void)
{
    GroveLight_Init();
    gps_init();
    printf("  RTC DS1307 ............ %s\r\n", GroveRTC_Init() ? "OK" : "ABSENTE");
    printf("  BME280 ................ %s\r\n", bme280_init() ? "OK" : "ABSENT");
    printf("  Capteur de lumiere .... ADC PA4\r\n");
    printf("  GPS Air530Z ........... UART4 9600 bauds (sous interruption)\r\n");
}

/* Une tentative sur chaque capteur encore attendu (hors GPS). */
static void tenter_capteurs(void)
{
    t_dernier_essai = HAL_GetTick();

    if (!fini[CAPTEUR_ENV] && bme280_lire(&courant.env)) {
        fini[CAPTEUR_ENV] = repondu[CAPTEUR_ENV] = true;
    }
    if (!fini[CAPTEUR_LUMIERE]) {
        uint16_t brut = GroveLight_ReadRaw();
        if (brut != LUMIERE_ERREUR) {
            courant.luminosite = (uint16_t)(brut >> 2);   /* 0-4095 -> 0-1023 */
            fini[CAPTEUR_LUMIERE] = repondu[CAPTEUR_LUMIERE] = true;
        }
    }
}

/* Le GPS a-t-il fourni une trame GGA récente et correcte ? */
static void verifier_gps(void)
{
    char trame[96];
    uint32_t instant;
    if (fini[CAPTEUR_GPS] || !gps_derniere_gga(trame, &instant)) return;
    if ((int32_t)(instant - t_debut) < -(int32_t)FRAICHEUR_GPS_MS) return;   /* trop ancienne */
    if (gps_analyser_gga(trame, &courant.gps)) {
        fini[CAPTEUR_GPS] = repondu[CAPTEUR_GPS] = true;   /* a répondu, avec ou sans position */
    }
}

void mesures_demarrer(bool avec_gps)
{
    memset(&courant, 0, sizeof(courant));
    memset(repondu, 0, sizeof(repondu));
    t_debut = HAL_GetTick();

    erreur_rtc = !mesures_horodater(&courant.horodatage);
    courant.horodatage_valide = !erreur_rtc;

    demande[CAPTEUR_ENV] = params.temp_air || params.hygr || params.pressure;
    demande[CAPTEUR_LUMIERE] = params.lumin != 0;
    demande[CAPTEUR_GPS] = avec_gps;
    for (int c = 0; c < NB_CAPTEURS; c++) fini[c] = !demande[c];

    en_cours = true;
    tenter_capteurs();
    verifier_gps();
}

static void finaliser(void)
{
    /* Compteurs de timeouts consécutifs */
    for (int c = 0; c < NB_CAPTEURS; c++) {
        if (!demande[c]) continue;
        if (repondu[c]) nb_timeouts[c] = 0;
        else if (nb_timeouts[c] < 255) nb_timeouts[c]++;
    }

    /* Validité de chaque valeur : capteur actif ET réponse obtenue */
    bool env = repondu[CAPTEUR_ENV];
    courant.temp_valide = env && params.temp_air;
    courant.pression_valide = env && params.pressure;
    /* Hygrométrie ignorée hors de [HYGR_MINT ; HYGR_MAXT] (fiche) */
    courant.hygr_valide = env && params.hygr &&
                          courant.env.temperature_c >= (float)params.hygr_mint &&
                          courant.env.temperature_c <= (float)params.hygr_maxt;
    courant.lumin_valide = repondu[CAPTEUR_LUMIERE];
    courant.gps_valide = repondu[CAPTEUR_GPS] && courant.gps.valid;


#ifdef GPS_DEMO_SOUTENANCE
    /* GPS reel prioritaire. Simulation uniquement sans position valide. */
    if (demande[CAPTEUR_GPS] && !courant.gps_valide)
    {
        courant.gps.latitude = 48.65183;
        courant.gps.longitude = 6.13453;
        courant.gps.valid = true;
        courant.gps.fix_quality = 0;
        courant.gps.satellites = 0;

        strcpy(courant.gps.raw_sentence, "GPS_SIMULE");
        courant.gps_valide = true;

        printf("[GPS DEMO] Position simulee utilisee\r\n");
    }
#endif

    /* Erreur à afficher, de la plus prioritaire à la moins prioritaire */
    if (erreur_rtc)                                      derniere_erreur = ERR_RTC;
    else if (nb_timeouts[CAPTEUR_GPS] >= NB_TIMEOUTS_ERREUR) derniere_erreur = ERR_GPS;
    else if (nb_timeouts[CAPTEUR_ENV] >= NB_TIMEOUTS_ERREUR ||
             nb_timeouts[CAPTEUR_LUMIERE] >= NB_TIMEOUTS_ERREUR) derniere_erreur = ERR_CAPTEUR;
    else if (!mesures_verifier_coherence(&courant))     derniere_erreur = ERR_DONNEES_INCOHERENTES;
    else                                                 derniere_erreur = ERR_AUCUNE;

    en_cours = false;
}

EtatAcquisition mesures_poursuivre(Mesures *m)
{
    if (!en_cours) { *m = courant; return ACQ_TERMINEE; }

    verifier_gps();
    if ((HAL_GetTick() - t_dernier_essai) >= PERIODE_ESSAI_MS) tenter_capteurs();

    bool tous_finis = true;
    for (int c = 0; c < NB_CAPTEURS; c++) tous_finis = tous_finis && fini[c];

    if (tous_finis || (HAL_GetTick() - t_debut) >= (uint32_t)params.timeout_s * 1000UL) {
        finaliser();   /* les capteurs encore muets restent à NA */
        *m = courant;
        return ACQ_TERMINEE;
    }
    return ACQ_EN_COURS;
}

void mesures_annuler(void)
{
    en_cours = false;
}

bool mesures_en_cours(void)
{
    return en_cours;
}

bool mesures_verifier_coherence(const Mesures *m)
{
    if (m->temp_valide && (m->env.temperature_c < (float)params.min_temp_air ||
                           m->env.temperature_c > (float)params.max_temp_air))
        return false;
    if (m->pression_valide && (m->env.pressure_hpa < (float)params.pressure_min ||
                               m->env.pressure_hpa > (float)params.pressure_max))
        return false;
    return true;
}

CodeErreur mesures_derniere_erreur(void)
{
    return derniere_erreur;
}
