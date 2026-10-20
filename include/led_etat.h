/**
 * @file    led_etat.h
 * @brief   Affichage de l'état sur la LED RGB chaînable (GroveRGB).
 *          Couleur continue par mode : vert standard, jaune configuration,
 *          bleu économie, orange maintenance.
 *          Erreur : clignotement à 1 Hz entre le rouge et une seconde couleur,
 *          selon le tableau des codes LED de la fiche.
 */
#ifndef LED_ETAT_H
#define LED_ETAT_H

#include "types_station.h"

/** Initialise la LED (GroveRGB_Init). */
void led_init(void);

/** Mémorise le mode et allume sa couleur continue (si aucune erreur). */
void led_afficher_mode(Mode mode);

/**
 * Démarre le clignotement d'un code d'erreur.
 * ERR_AUCUNE rétablit la couleur du mode courant.
 */
void led_signaler_erreur(CodeErreur erreur);

/** Code d'erreur actuellement affiché. */
CodeErreur led_erreur_affichee(void);

/**
 * Alterne les deux couleurs d'un code d'erreur au bon moment
 * (500 ms / 500 ms, ou 333 ms rouge / 667 ms pour une couleur 2 fois plus longue).
 * Appelée à chaque tour de boucle ; n'envoie rien si la couleur ne change pas.
 */
void led_mettre_a_jour(void);

/** Nom lisible d'un code d'erreur (affichage console). */
const char *led_nom_erreur(CodeErreur erreur);

#endif
