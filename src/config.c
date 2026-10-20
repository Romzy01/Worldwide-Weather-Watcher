/**
 * @file    config.c
 * @brief   Paramètres, sauvegarde en flash et commandes (voir config.h).
 *
 *          Sauvegarde : dernière page (2 Ko) de la banque 2 de la flash,
 *          adresse 0x080FF800, loin du programme qui commence en 0x08000000.
 *          La page est effacée puis écrite par doubles mots (64 bits).
 */
#include "config.h"
#include "console.h"
#include "grove_rtc_ds1307.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define FLASH_ADRESSE_PARAMS  0x080FF800UL
#define FLASH_BANQUE_PARAMS   FLASH_BANK_2
#define FLASH_PAGE_PARAMS     255U
#define VALEUR_CONTROLE       0x33573357UL   /* « 3W3W » */

_Static_assert(sizeof(Parametres) % 8 == 0, "Parametres doit faire un multiple de 8 octets");

Parametres params;
static bool params_modifies = false;   /* au moins un paramètre changé depuis l'entrée en configuration */

/* Tableau des paramètres de la fiche : nom, champ, minimum, maximum, défaut. */
typedef struct {
    const char *nom;
    size_t      decalage;
    int32_t     min, max, defaut;
} DefinitionParametre;

#define CHAMP(c) offsetof(Parametres, c)
static const DefinitionParametre TABLE[] = {
    {"LOG_INTERVAL",  CHAMP(log_interval_min), 1,    1440,    10},
    {"FILE_MAX_SIZE", CHAMP(file_max_size),    256,  1048576, 2048},
    {"TIMEOUT",       CHAMP(timeout_s),        1,    300,     30},
    {"LUMIN",         CHAMP(lumin),            0,    1,       1},
    {"LUMIN_LOW",     CHAMP(lumin_low),        0,    1023,    255},
    {"LUMIN_HIGH",    CHAMP(lumin_high),       0,    1023,    768},
    {"TEMP_AIR",      CHAMP(temp_air),         0,    1,       1},
    {"MIN_TEMP_AIR",  CHAMP(min_temp_air),     -40,  85,      -10},
    {"MAX_TEMP_AIR",  CHAMP(max_temp_air),     -40,  85,      60},
    {"HYGR",          CHAMP(hygr),             0,    1,       1},
    {"HYGR_MINT",     CHAMP(hygr_mint),        -40,  85,      0},
    {"HYGR_MAXT",     CHAMP(hygr_maxt),        -40,  85,      50},
    {"PRESSURE",      CHAMP(pressure),         0,    1,       1},
    {"PRESSURE_MIN",  CHAMP(pressure_min),     300,  1100,    850},
    {"PRESSURE_MAX",  CHAMP(pressure_max),     300,  1100,    1080},
};
#define NB_PARAMETRES (sizeof(TABLE) / sizeof(TABLE[0]))

static int32_t *champ(Parametres *p, const DefinitionParametre *d)
{
    return (int32_t *)((uint8_t *)p + d->decalage);
}

static uint32_t calculer_somme(const Parametres *p)
{
    const uint32_t *mot = (const uint32_t *)p;
    uint32_t somme = 0x5A5A5A5AUL;
    for (size_t i = 0; i < offsetof(Parametres, somme) / 4; i++)
        somme = (somme << 5 | somme >> 27) ^ mot[i];
    return somme;
}

void config_valeurs_defaut(Parametres *p)
{
    memset(p, 0, sizeof(*p));
    for (size_t i = 0; i < NB_PARAMETRES; i++) *champ(p, &TABLE[i]) = TABLE[i].defaut;
}

bool config_charger(Parametres *p)
{
    const Parametres *en_flash = (const Parametres *)FLASH_ADRESSE_PARAMS;
    if (en_flash->controle == VALEUR_CONTROLE && en_flash->somme == calculer_somme(en_flash)) {
        *p = *en_flash;
        return true;
    }
    config_valeurs_defaut(p);   /* flash vierge ou corrompue */
    return false;
}

bool config_sauvegarder(Parametres *p)
{
    p->controle = VALEUR_CONTROLE;
    p->reserve = 0;
    p->somme = calculer_somme(p);

    FLASH_EraseInitTypeDef effacement = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks = FLASH_BANQUE_PARAMS,
        .Page = FLASH_PAGE_PARAMS,
        .NbPages = 1,
    };
    uint32_t page_en_erreur = 0;
    bool ok;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    ok = HAL_FLASHEx_Erase(&effacement, &page_en_erreur) == HAL_OK;

    const uint64_t *double_mot = (const uint64_t *)p;
    for (size_t i = 0; ok && i < sizeof(Parametres) / 8; i++) {
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                               FLASH_ADRESSE_PARAMS + i * 8, double_mot[i]) == HAL_OK;
    }
    HAL_FLASH_Lock();

    return ok && memcmp((const void *)FLASH_ADRESSE_PARAMS, p, sizeof(Parametres)) == 0;
}

void config_afficher(const Parametres *p)
{
    for (size_t i = 0; i < NB_PARAMETRES; i++) {
        printf("  %-14s = %ld\r\n", TABLE[i].nom, (long)*champ((Parametres *)p, &TABLE[i]));
    }
}

/* Lecture d'un entier signé strict (toute la chaîne doit être un nombre). */
static bool lire_entier(const char *texte, long *valeur)
{
    char *fin;
    if (*texte == '\0') return false;
    *valeur = strtol(texte, &fin, 10);
    return *fin == '\0';
}

/*
 * Lit exactement trois entiers positifs séparés par « sep » (« 14:30:00 »,
 * « 10,14,2026 »), sans sscanf() qui tirerait toute la bibliothèque de scanf.
 */
static bool lire_trois_nombres(const char *texte, char sep, unsigned *a, unsigned *b, unsigned *c)
{
    unsigned *sortie[3] = {a, b, c};
    for (int i = 0; i < 3; i++) {
        if (*texte < '0' || *texte > '9') return false;
        unsigned v = 0;
        int chiffres = 0;
        while (*texte >= '0' && *texte <= '9' && chiffres < 5) { v = v * 10 + (unsigned)(*texte++ - '0'); chiffres++; }
        *sortie[i] = v;
        if (i < 2 && *texte++ != sep) return false;
    }
    return *texte == '\0';
}

static ResultatCommande regler_rtc(const char *nom, const char *valeur)
{
    RTC_DateTime dt;
    unsigned a, b, c;
    if (!GroveRTC_GetDateTime(&dt)) return CMD_ECHEC_MATERIEL;

    if (strcmp(nom, "CLOCK") == 0) {
        if (!lire_trois_nombres(valeur, ':', &a, &b, &c)) return CMD_HORS_DOMAINE;
        if (a > 23 || b > 59 || c > 59) return CMD_HORS_DOMAINE;
        dt.hours = (uint8_t)a; dt.minutes = (uint8_t)b; dt.seconds = (uint8_t)c;
    } else if (strcmp(nom, "DATE") == 0) {          /* DATE=MOIS,JOUR,ANNEE (fiche) */
        if (!lire_trois_nombres(valeur, ',', &a, &b, &c)) return CMD_HORS_DOMAINE;
        if (a < 1 || a > 12 || b < 1 || b > 31 || c < 2000 || c > 2099) return CMD_HORS_DOMAINE;
        dt.month = (uint8_t)a; dt.day = (uint8_t)b; dt.year = (uint16_t)c;
    } else {                                        /* DAY=MON ... SUN, 1 = lundi */
        static const char *JOURS[7] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
        int jour = -1;
        for (int i = 0; i < 7; i++) if (strcmp(valeur, JOURS[i]) == 0) jour = i + 1;
        if (jour < 0) return CMD_HORS_DOMAINE;
        dt.dayOfWeek = (uint8_t)jour;
    }
    return GroveRTC_SetDateTime(&dt) ? CMD_OK : CMD_ECHEC_MATERIEL;
}

ResultatCommande config_traiter_commande(const char *cmd)
{
    char nom[TAILLE_COMMANDE_MAX];
    const char *egal = strchr(cmd, '=');

    /* --- Commandes sans valeur --- */
    if (egal == NULL) {
        if (strcmp(cmd, "RESET") == 0) {
            config_valeurs_defaut(&params);
            params_modifies = true;
            return CMD_OK;
        }
        if (strcmp(cmd, "VERSION") == 0) {
            printf("3W v%s - lot %s\r\n", VERSION_PROGRAMME, NUMERO_LOT);
            return CMD_DEJA_REPONDU;
        }
        if (strcmp(cmd, "PARAMS") == 0) {
            config_afficher(&params);
            return CMD_DEJA_REPONDU;
        }
        if (strcmp(cmd, "EXIT") == 0) return CMD_SORTIE;
        return CMD_INCONNUE;
    }

    /* --- Commandes NOM=valeur --- */
    size_t longueur = (size_t)(egal - cmd);
    if (longueur == 0 || longueur >= sizeof(nom)) return CMD_INCONNUE;
    memcpy(nom, cmd, longueur);
    nom[longueur] = '\0';
    const char *valeur = egal + 1;

    if (strcmp(nom, "CLOCK") == 0 || strcmp(nom, "DATE") == 0 || strcmp(nom, "DAY") == 0)
        return regler_rtc(nom, valeur);

    for (size_t i = 0; i < NB_PARAMETRES; i++) {
        if (strcmp(nom, TABLE[i].nom) != 0) continue;
        long v;
        if (!lire_entier(valeur, &v) || v < TABLE[i].min || v > TABLE[i].max) {
            printf("ERREUR : valeur hors domaine (%s : %ld a %ld)\r\n",
                   TABLE[i].nom, (long)TABLE[i].min, (long)TABLE[i].max);
            return CMD_DEJA_REPONDU;
        }
        if (*champ(&params, &TABLE[i]) != (int32_t)v) params_modifies = true;
        *champ(&params, &TABLE[i]) = (int32_t)v;
        return CMD_OK;   /* écrit en flash à la sortie de configuration */
    }
    return CMD_INCONNUE;
}

bool config_enregistrer_si_modifie(void)
{
    if (!params_modifies) return true;   /* rien à écrire : la flash est ménagée */
    bool ok = config_sauvegarder(&params);
    if (ok) params_modifies = false;
    return ok;
}
