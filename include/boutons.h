/**
 * @file    boutons.h
 * @brief   Deux boutons sur interruption externe (EXTI10 pour PA10, EXTI3 pour PB3)
 *          et détection des appuis longs de 5 s, sans bloquer la boucle principale.
 *          L'ISR note seulement l'instant du front ; l'anti-rebond et la durée
 *          d'appui sont traités dans la boucle (GroveButton1/2_IsPressed()).
 */
#ifndef BOUTONS_H
#define BOUTONS_H

#include "types_station.h"

/*
 * Correspondance avec le Grove Dual Button branché sur le port D2 :
 * bouton 1 = PA10 (D2), bouton 2 = PB3 (D3). À inverser ici si besoin.
 */
#ifndef BOUTON_ROUGE_EST_BOUTON1
#define BOUTON_ROUGE_EST_BOUTON1  0
#endif

/*
 * Le driver considère un bouton appuyé quand la broche est au niveau bas.
 * Si l'essai sur la carte montre le contraire, compiler avec
 * -DBOUTONS_ACTIFS_NIVEAU_HAUT=1.
 */
#ifndef BOUTONS_ACTIFS_NIVEAU_HAUT
#define BOUTONS_ACTIFS_NIVEAU_HAUT 0
#endif

/** Initialise les broches (GroveButton_Init), puis les interruptions EXTI sur les deux fronts. */
void boutons_init(void);

/** État instantané (filtré) du bouton rouge, utilisé au démarrage. */
bool boutons_rouge_presse(void);

/**
 * Mesure la durée d'appui de chaque bouton à chaque appel.
 * Un seul événement est produit par appui, au moment où les 5 s sont atteintes.
 * @return BOUTON_ROUGE_LONG, BOUTON_VERT_LONG ou BOUTON_AUCUN
 */
EvenementBouton boutons_lire_evenement(void);

#endif
