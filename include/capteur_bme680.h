/**
 * @file    capteur_bme680.h
 * @brief   Lecture compensée du BME680 (température, hygrométrie, pression).
 *
 *          GroveBME680_ReadEnvironment() du dépôt renvoie des valeurs fixes
 *          (emplacement réservé). Ce module, ajouté sans modifier le dépôt,
 *          lit les coefficients de calibration, lance une mesure en mode
 *          « forced » et applique les formules de compensation de Bosch.
 *          La détection du capteur réutilise GroveBME680_Init().
 */
#ifndef CAPTEUR_BME680_H
#define CAPTEUR_BME680_H

#include "types_station.h"

/** Détecte le capteur (0x76 ou 0x77) et lit sa calibration. */
bool bme680_init(void);

/**
 * Réalise une mesure complète.
 * @return false si le capteur ne répond pas (I2C) ou si la mesure n'aboutit pas
 */
bool bme680_lire(Env_Data *env);

#endif
