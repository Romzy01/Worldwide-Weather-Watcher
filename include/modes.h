/**
 * @file    modes.h
 * @brief   Machine à états de la station : choix du mode courant, transitions
 *          entre les 4 modes et exécution du mode actif.
 *          Seul module autorisé à changer de mode.
 */
#ifndef MODES_H
#define MODES_H

#include "types_station.h"

/** Choisit le mode de démarrage : bouton rouge enfoncé → CONFIGURATION, sinon STANDARD. */
void modes_init(void);

/** Applique la transition du tableau pour (mode courant, événement bouton). */
void modes_traiter_evenement(EvenementBouton evenement);

/** Exécute une étape du mode courant ; ne bloque jamais. */
void modes_executer(void);

/**
 * Change de mode : actions de sortie de l'ancien mode, mémorisation du mode
 * précédent, actions d'entrée du nouveau mode, mise à jour de la LED.
 */
void modes_changer(Mode nouveau);

/** Mode actif. */
Mode modes_courant(void);

/** Étape de chaque mode, appelée par modes_executer(). */
void mode_standard(void);
void mode_economie(void);
void mode_configuration(void);
void mode_maintenance(void);

#endif
