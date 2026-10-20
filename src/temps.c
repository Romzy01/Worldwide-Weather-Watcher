/**
 * @file    temps.c
 * @brief   Minuteries non bloquantes (voir temps.h).
 *          La soustraction HAL_GetTick() - debut reste juste au passage
 *          à zéro du compteur 32 bits (environ 49 jours).
 */
#include "temps.h"
#include "stm32l4xx_hal.h"

void minuteur_demarrer(Minuteur *t, uint32_t duree_ms)
{
    t->debut_ms = HAL_GetTick();
    t->duree_ms = duree_ms;
    t->actif = true;
}

void minuteur_rearmer(Minuteur *t)
{
    t->debut_ms = HAL_GetTick();
    t->actif = true;
}

void minuteur_arreter(Minuteur *t)
{
    t->actif = false;
}

bool minuteur_expire(const Minuteur *t)
{
    return t->actif && (HAL_GetTick() - t->debut_ms) >= t->duree_ms;
}

uint32_t temps_minute_ms(void)
{
#ifdef DEMO_ACCELEREE
    return 1000UL;
#else
    return 60000UL;
#endif
}
