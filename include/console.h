/**
 * @file    console.h
 * @brief   Liaison série USART2 (115200 bauds, 8N1) vers le PC via le ST-Link :
 *          réception des commandes en configuration (sous interruption,
 *          tampon circulaire), affichage des mesures en maintenance.
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include "types_station.h"

/** Active la réception sous interruption et désactive le tampon de printf. */
void console_init(void);

/**
 * Lit les caractères reçus sans attendre (avec écho et effacement).
 * Une commande est complète à la réception d'un retour à la ligne.
 * @return true si une commande complète (non vide, en majuscules) est dans buffer
 */
bool console_lire_commande(char *buffer, uint16_t taille);

/** Jette les caractères reçus (hors mode configuration). @return true si des caractères ont été reçus */
bool console_vider(void);

/** Envoie une réponse à l'opérateur, suivie d'un retour à la ligne. */
void console_repondre(const char *message);

/** Affiche un relevé lisible (mode maintenance, et trace des autres modes). */
void console_afficher_mesures(const Mesures *m);

#endif
