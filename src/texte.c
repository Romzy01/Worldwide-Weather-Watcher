/**
 * @file    texte.c
 * @brief   Nombres décimaux écrits en virgule fixe (voir texte.h).
 */
#include "texte.h"
#include <stdio.h>

int texte_decimal(char *dst, size_t taille, float valeur, uint8_t decimales)
{
    static const int32_t PUISSANCES[7] = {1, 10, 100, 1000, 10000, 100000, 1000000};
    if (decimales > 6) decimales = 6;
    int32_t echelle = PUISSANCES[decimales];

    /* Passage en virgule fixe avec arrondi au plus proche */
    float mise_a_l_echelle = valeur * (float)echelle;
    int32_t fixe = (int32_t)(mise_a_l_echelle + (mise_a_l_echelle >= 0.0f ? 0.5f : -0.5f));

    const char *signe = (fixe < 0) ? "-" : "";
    uint32_t absolu = (uint32_t)(fixe < 0 ? -fixe : fixe);

    if (decimales == 0) return snprintf(dst, taille, "%s%lu", signe, (unsigned long)absolu);
    return snprintf(dst, taille, "%s%lu.%0*lu", signe, (unsigned long)(absolu / (uint32_t)echelle),
                    (int)decimales, (unsigned long)(absolu % (uint32_t)echelle));
}
