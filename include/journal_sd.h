/**
 * @file    journal_sd.h
 * @brief   Enregistrement des relevés sur la carte SD via FatFs.
 *          Une ligne horodatée par relevé dans AAMMJJ_0.LOG.
 *          Fichier plein (FILE_MAX_SIZE) : il est renommé AAMMJJ_N.LOG
 *          (N = première révision libre) et l'écriture repart dans un
 *          nouveau AAMMJJ_0.LOG vide.
 */
#ifndef JOURNAL_SD_H
#define JOURNAL_SD_H

#include "types_station.h"

#define SD_SEPARATEUR      ';'
#define SD_VALEUR_ABSENTE  "NA"
#define SD_FORMAT_NOM      "%02u%02u%02u_%u.LOG"   /* AA, MM, JJ, révision */
#define SD_ENTETE          "DATE HEURE;PRESSION_HPA;TEMP_AIR_C;HYGR_PCT;LUMIN;LATITUDE;LONGITUDE\r\n"

/** Initialise la carte (SDLogger_Init + passage SPI rapide) et monte FatFs. */
bool journal_init(void);

/** true si le système de fichiers est monté. */
bool journal_disponible(void);

/**
 * Construit la ligne à écrire : date, heure, puis une valeur par capteur
 * séparée par ';', NA si la donnée est invalide. Termine par \r\n.
 */
void journal_formater_ligne(const Mesures *m, char *ligne, uint16_t taille);

/**
 * Ajoute une ligne au fichier du jour, révision 0, en archivant d'abord
 * le fichier s'il dépasserait FILE_MAX_SIZE.
 * @return ERR_AUCUNE, ERR_SD_PLEINE ou ERR_SD_ECRITURE
 */
CodeErreur journal_ecrire(const Mesures *m);

/** Ferme tout et démonte : la carte peut être retirée sans risque. */
void journal_suspendre(void);

/** Réinitialise et remonte la carte après la maintenance. */
bool journal_reprendre(void);

#endif
