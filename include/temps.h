/**
 * @file    temps.h
 * @brief   Minuteries non bloquantes basées sur HAL_GetTick() (milliseconde).
 *          Elles remplacent tout HAL_Delay() long dans la boucle principale.
 */
#ifndef TEMPS_H
#define TEMPS_H

#include "types_station.h"

/** Lance une minuterie de duree_ms millisecondes. */
void minuteur_demarrer(Minuteur *t, uint32_t duree_ms);

/** Repart de zéro avec la même durée (activité détectée). */
void minuteur_rearmer(Minuteur *t);

/** Arrête la minuterie : minuteur_expire() renverra false. */
void minuteur_arreter(Minuteur *t);

/** true si la minuterie est active et que sa durée est écoulée. */
bool minuteur_expire(const Minuteur *t);

/**
 * Durée d'une « minute » de paramétrage, en millisecondes.
 * 60000 en version normale ; 1000 dans la version de démonstration
 * (option DEMO_ACCELEREE) pour que LOG_INTERVAL=10 donne 10 s.
 */
uint32_t temps_minute_ms(void);

#endif
