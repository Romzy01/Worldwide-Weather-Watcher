/**
 * @file    texte.h
 * @brief   Mise en forme des nombres décimaux sans printf("%f").
 *
 *          printf/sscanf avec nombres à virgule tirent de la bibliothèque C
 *          les routines de conversion flottantes (dtoa, strtod) : environ 20 Ko
 *          de flash et des allocations sur le tas. Ici, la valeur est convertie
 *          en entier à virgule fixe (dixièmes, cent-millièmes...) puis écrite
 *          avec des entiers seulement.
 */
#ifndef TEXTE_H
#define TEXTE_H

#include <stdint.h>
#include <stddef.h>

/**
 * Écrit valeur avec « decimales » chiffres après la virgule (0 à 6), arrondie.
 * @return nombre de caractères écrits (sans le '\0')
 */
int texte_decimal(char *dst, size_t taille, float valeur, uint8_t decimales);

#endif
