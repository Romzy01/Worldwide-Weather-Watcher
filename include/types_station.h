/**
 * @file    types_station.h
 * @brief   Types partagés par tous les modules de la station 3W.
 *          Complète cesi_types.h du dépôt (RTC_DateTime, GPS_Data, Env_Data),
 *          qui est réutilisé sans modification.
 */
#ifndef TYPES_STATION_H
#define TYPES_STATION_H

#include <stdint.h>
#include <stdbool.h>
#include "cesi_types.h"

/* ------------------------------------------------------------------------ */
/* Identification du programme (réponse à la commande VERSION)              */
/* ------------------------------------------------------------------------ */
#define VERSION_PROGRAMME   "1.0"
#define NUMERO_LOT          "LOT-001"

/* ------------------------------------------------------------------------ */
/* Constantes de temps et de taille                                          */
/* ------------------------------------------------------------------------ */
#define DELAI_APPUI_LONG_MS     5000UL  /* appui long sur un bouton : 5 s        */
#define DELAI_ANTIREBOND_MS     30UL    /* filtrage des rebonds des boutons      */
#define INACTIVITE_CONFIG_MIN   30UL    /* configuration : 30 min sans commande  */
#define PERIODE_MAINTENANCE_MS  2000UL  /* maintenance : un affichage toutes les 2 s */
#define NB_TIMEOUTS_ERREUR      2       /* capteur en erreur après 2 timeouts    */
#define TAILLE_LIGNE_MAX        128     /* une ligne du fichier LOG              */
#define TAILLE_COMMANDE_MAX     64      /* une commande de la console            */

/* ------------------------------------------------------------------------ */
/* Énumérations                                                              */
/* ------------------------------------------------------------------------ */

/** Les 4 modes de fonctionnement définis par la fiche. */
typedef enum {
    MODE_STANDARD,       /* acquisition + écriture SD, LED verte              */
    MODE_CONFIGURATION,  /* commandes série, acquisition coupée, LED jaune    */
    MODE_MAINTENANCE,    /* mesures en direct, SD retirable, LED orange       */
    MODE_ECONOMIE        /* intervalle doublé, GPS 1 fois sur 2, LED bleue     */
} Mode;

/** Une valeur par ligne du tableau des codes LED (clignotement à 1 Hz). */
typedef enum {
    ERR_AUCUNE,
    ERR_RTC,                  /* rouge / bleu                              */
    ERR_GPS,                  /* rouge / jaune                             */
    ERR_CAPTEUR,              /* rouge / vert                              */
    ERR_DONNEES_INCOHERENTES, /* rouge / vert, vert 2 fois plus long       */
    ERR_SD_PLEINE,            /* rouge / blanc                             */
    ERR_SD_ECRITURE           /* rouge / blanc, blanc 2 fois plus long     */
} CodeErreur;

/** Événements produits par le module boutons (seuls les appuis longs comptent). */
typedef enum {
    BOUTON_AUCUN,
    BOUTON_ROUGE_LONG,   /* rouge maintenu 5 s */
    BOUTON_VERT_LONG     /* vert maintenu 5 s  */
} EvenementBouton;

/** Capteurs suivis par le compteur de timeouts. */
typedef enum {
    CAPTEUR_ENV,         /* BME680 : pression, température, hygrométrie */
    CAPTEUR_LUMIERE,
    CAPTEUR_GPS,
    NB_CAPTEURS
} Capteur;

/** Avancement d'une acquisition non bloquante. */
typedef enum {
    ACQ_EN_COURS,        /* au moins un capteur attendu, TIMEOUT non écoulé */
    ACQ_TERMINEE         /* relevé complet (valeurs ou NA)                   */
} EtatAcquisition;

/** Résultat du traitement d'une commande de configuration. */
typedef enum {
    CMD_OK,              /* commande appliquée (et sauvegardée si besoin) */
    CMD_DEJA_REPONDU,    /* VERSION, PARAMS : la réponse est déjà affichée */
    CMD_INCONNUE,
    CMD_HORS_DOMAINE,
    CMD_ECHEC_MATERIEL,  /* RTC ou flash qui ne répond pas */
    CMD_SORTIE           /* EXIT : retour au mode standard demandé */
} ResultatCommande;

/* ------------------------------------------------------------------------ */
/* Structures                                                                */
/* ------------------------------------------------------------------------ */

/**
 * Paramètres modifiables en mode configuration.
 * Copie en RAM utilisée par tout le programme, sauvegardée en flash interne
 * (émulation de l'EEPROM) à chaque modification. Tous les champs sont des
 * entiers 32 bits : la structure est traitée par un tableau générique
 * (nom, minimum, maximum) dans config.c. Taille multiple de 8 octets,
 * imposée par la programmation de la flash par double mot.
 */
typedef struct {
    int32_t log_interval_min;  /* LOG_INTERVAL, minutes, défaut 10        */
    int32_t file_max_size;     /* FILE_MAX_SIZE, octets, défaut 2048      */
    int32_t timeout_s;         /* TIMEOUT, secondes, défaut 30            */
    int32_t lumin;             /* LUMIN, 0 ou 1                           */
    int32_t lumin_low;         /* LUMIN_LOW, 0-1023, défaut 255           */
    int32_t lumin_high;        /* LUMIN_HIGH, 0-1023, défaut 768          */
    int32_t temp_air;          /* TEMP_AIR, 0 ou 1                        */
    int32_t min_temp_air;      /* MIN_TEMP_AIR, -40 à 85, défaut -10      */
    int32_t max_temp_air;      /* MAX_TEMP_AIR, -40 à 85, défaut 60       */
    int32_t hygr;              /* HYGR, 0 ou 1                            */
    int32_t hygr_mint;         /* HYGR_MINT, -40 à 85, défaut 0           */
    int32_t hygr_maxt;         /* HYGR_MAXT, -40 à 85, défaut 50          */
    int32_t pressure;          /* PRESSURE, 0 ou 1                        */
    int32_t pressure_min;      /* PRESSURE_MIN, 300-1100 hPa, défaut 850  */
    int32_t pressure_max;      /* PRESSURE_MAX, 300-1100 hPa, défaut 1080 */
    uint32_t controle;         /* valeur fixe : détecte une flash vierge  */
    uint32_t somme;            /* somme de contrôle des champs précédents */
    uint32_t reserve;          /* complète à un multiple de 8 octets      */
} Parametres;

/**
 * Un relevé complet, qui devient une ligne du fichier LOG.
 * Un indicateur *_valide à false fait écrire NA à la place de la valeur
 * (capteur muet après TIMEOUT, désactivé, hors plage d'hygrométrie,
 * GPS sans position ou non relevé en mode économie).
 */
typedef struct {
    RTC_DateTime horodatage;
    Env_Data     env;          /* pression, température, hygrométrie */
    uint16_t     luminosite;   /* ramenée sur 0-1023                 */
    GPS_Data     gps;
    bool horodatage_valide;
    bool temp_valide;
    bool hygr_valide;
    bool pression_valide;
    bool lumin_valide;
    bool gps_valide;
} Mesures;

/** Minuterie non bloquante : expirée quand HAL_GetTick() - debut_ms >= duree_ms. */
typedef struct {
    uint32_t debut_ms;
    uint32_t duree_ms;
    bool     actif;
} Minuteur;

#endif /* TYPES_STATION_H */
