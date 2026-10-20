/**
 * @file    journal_sd.c
 * @brief   Fichiers LOG sur la carte SD (voir journal_sd.h).
 *
 *          Chaque écriture ouvre le fichier, ajoute la ligne et le referme :
 *          la carte reste cohérente même si l'alimentation est coupée entre
 *          deux relevés. Un fichier créé commence par une ligne d'en-tête.
 */
#include "journal_sd.h"
#include "sd_spi.h"
#include "mesures.h"
#include "config.h"
#include "ff.h"
#include "texte.h"
#include <stdio.h>
#include <string.h>

static FATFS systeme_fichiers;
static bool  monte = false;

/* --- Horodatage des fichiers par FatFs, à partir de la RTC --------------- */
DWORD get_fattime(void)
{
    RTC_DateTime dt;
    if (!mesures_horodater(&dt) || dt.year < 1980) {
        return ((DWORD)(2026 - 1980) << 25) | (1UL << 21) | (1UL << 16);
    }
    return ((DWORD)(dt.year - 1980) << 25) | ((DWORD)dt.month << 21) | ((DWORD)dt.day << 16) |
           ((DWORD)dt.hours << 11) | ((DWORD)dt.minutes << 5) | ((DWORD)dt.seconds >> 1);
}

static void demonter(void)
{
    f_mount(NULL,"",0);
    sd_spi_invalider();
    monte = false;
}

bool journal_init(void)
{
    if (monte) return true;
    sd_spi_invalider();
    monte = (f_mount(&systeme_fichiers, "", 1) == FR_OK);   /* 1 = monter tout de suite */
    if (!monte) sd_spi_invalider();
    return monte;
}

bool journal_disponible(void)
{
    return monte;
}

/* Ajoute "valeur;" ou "NA;" à la ligne (virgule fixe, sans printf flottant). */
static void ajouter(char *ligne, uint16_t taille, bool valide, float valeur, uint8_t decimales, bool dernier)
{
    size_t n = strlen(ligne);
    if (valide) texte_decimal(ligne + n, taille - n, valeur, decimales);
    else        snprintf(ligne + n, taille - n, "%s", SD_VALEUR_ABSENTE);
    n = strlen(ligne);
    snprintf(ligne + n, taille - n, dernier ? "\r\n" : "%c", SD_SEPARATEUR);
}

void journal_formater_ligne(const Mesures *m, char *ligne, uint16_t taille)
{
    const RTC_DateTime *d = &m->horodatage;
    if (m->horodatage_valide)
        snprintf(ligne, taille, "%04u-%02u-%02u %02u:%02u:%02u%c", d->year, d->month, d->day,
                 d->hours, d->minutes, d->seconds, SD_SEPARATEUR);
    else
        snprintf(ligne, taille, "%s%c", SD_VALEUR_ABSENTE, SD_SEPARATEUR);

    ajouter(ligne, taille, m->pression_valide, m->env.pressure_hpa, 1, false);
    ajouter(ligne, taille, m->temp_valide, m->env.temperature_c, 1, false);
    ajouter(ligne, taille, m->hygr_valide, m->env.humidity_percent, 1, false);
    ajouter(ligne, taille, m->lumin_valide, (float)m->luminosite, 0, false);
    ajouter(ligne, taille, m->gps_valide, (float)m->gps.latitude, 5, false);
    ajouter(ligne, taille, m->gps_valide, (float)m->gps.longitude, 5, true);
}

static void nom_fichier(char *nom, const Mesures *m, unsigned revision)
{
    if (m->horodatage_valide)
        sprintf(nom, SD_FORMAT_NOM, m->horodatage.year % 100, m->horodatage.month,
                m->horodatage.day, revision);
    else
        sprintf(nom, SD_FORMAT_NOM, 0u, 0u, 0u, revision);   /* RTC en erreur */
}

/* Renomme AAMMJJ_0.LOG en AAMMJJ_N.LOG, N = première révision libre. */
static FRESULT archiver(const Mesures *m)
{
    char courant[16], archive[16];
    FILINFO info;
    nom_fichier(courant, m, 0);
    for (unsigned n = 1; n <= 99; n++) {
        nom_fichier(archive, m, n);
        if (f_stat(archive, &info) == FR_NO_FILE) {
            printf("[SD] %s plein -> archive %s\r\n", courant, archive);
            return f_rename(courant, archive);
        }
    }
    return FR_DENIED;
}

CodeErreur journal_ecrire(const Mesures *m)
{
    char ligne[TAILLE_LIGNE_MAX], nom[16];
    FILINFO info;
    FIL fichier;
    UINT ecrits;
    FRESULT r;

    if (!monte && !journal_init()) return ERR_SD_ECRITURE;   /* carte absente ou illisible */

    journal_formater_ligne(m, ligne, sizeof(ligne));
    UINT longueur = (UINT)strlen(ligne);
    nom_fichier(nom, m, 0);

    /* Rotation : la ligne ferait dépasser FILE_MAX_SIZE */
    r = f_stat(nom, &info);
    if (r == FR_OK && info.fsize + longueur > (FSIZE_t)params.file_max_size) {
        r = archiver(m);
        if (r == FR_DENIED) return ERR_SD_PLEINE;
        if (r != FR_OK) { demonter(); return ERR_SD_ECRITURE; }
        r = FR_NO_FILE;
    }
    bool nouveau = (r == FR_NO_FILE);
    if (r != FR_OK && !nouveau) { demonter(); return ERR_SD_ECRITURE; }

    r = f_open(&fichier, nom, FA_OPEN_APPEND | FA_WRITE);
    if (r == FR_DENIED) return ERR_SD_PLEINE;              /* répertoire plein */
    if (r != FR_OK) { demonter(); return ERR_SD_ECRITURE; }

    CodeErreur resultat = ERR_AUCUNE;
    if (nouveau) {
        r = f_write(&fichier, SD_ENTETE, (UINT)strlen(SD_ENTETE), &ecrits);
        if (r == FR_OK && ecrits < strlen(SD_ENTETE)) resultat = ERR_SD_PLEINE;
    }
    if (resultat == ERR_AUCUNE && r == FR_OK) {
        r = f_write(&fichier, ligne, longueur, &ecrits);
        if (r == FR_OK && ecrits < longueur) resultat = ERR_SD_PLEINE;   /* plus de cluster libre */
    }
    if (r != FR_OK) resultat = ERR_SD_ECRITURE;

    if (f_close(&fichier) != FR_OK && resultat == ERR_AUCUNE) resultat = ERR_SD_ECRITURE;
    if (resultat == ERR_SD_ECRITURE) demonter();
    else if (resultat == ERR_AUCUNE) printf("[SD] %s <- %s", nom, ligne);
    return resultat;
}

void journal_suspendre(void)
{
    if (monte) demonter();
    printf("[SD] Carte demontee : retrait possible en toute securite.\r\n");
}

bool journal_reprendre(void)
{
    bool ok = journal_init();
    printf("[SD] Remontage de la carte : %s\r\n", ok ? "OK" : "ECHEC (nouvel essai au prochain releve)");
    return ok;
}
