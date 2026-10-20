/**
 * @file    config.h
 * @brief   Paramètres de la station : valeurs par défaut, sauvegarde en flash
 *          interne (le STM32L476 n'a pas d'EEPROM : une page de flash l'émule)
 *          et traitement des commandes reçues en mode configuration.
 *          Les commandes modifient la copie en RAM ; la flash n'est écrite
 *          qu'une fois, à la sortie de configuration, et seulement si un
 *          paramètre a changé (endurance d'environ 10 000 cycles).
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "types_station.h"

/** Copie en RAM des paramètres, lue par tous les modules. */
extern Parametres params;

/**
 * Charge les paramètres depuis la flash. Si la valeur de contrôle ou la somme
 * est fausse (flash vierge ou corrompue), applique les valeurs par défaut.
 * @return true si des paramètres sauvegardés ont été retrouvés
 */
bool config_charger(Parametres *p);

/** Écrit les paramètres en flash. @return false en cas d'échec. */
bool config_sauvegarder(Parametres *p);

/** Remet toutes les valeurs par défaut de la fiche (commande RESET). */
void config_valeurs_defaut(Parametres *p);

/**
 * Analyse et exécute une commande de la console :
 *   PARAMETRE=valeur (domaine vérifié), RESET, VERSION, PARAMS, EXIT,
 *   CLOCK=HH:MM:SS, DATE=MM,JJ,AAAA, DAY=XXX (réglage de la RTC).
 */
ResultatCommande config_traiter_commande(const char *cmd);

/** Écrit les paramètres en flash s'ils ont changé (sortie de configuration). */
bool config_enregistrer_si_modifie(void);

/** Affiche tous les paramètres sur la console (commande PARAMS). */
void config_afficher(const Parametres *p);

#endif
