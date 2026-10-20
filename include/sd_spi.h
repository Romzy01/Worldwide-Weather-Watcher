/**
 * @file    sd_spi.h
 * @brief   Accès bloc de la carte SD en SPI (couche « diskio » de FatFs).
 *          L'initialisation de la carte réutilise SDLogger_Init() du dépôt ;
 *          ce module ajoute la lecture et l'écriture de secteurs de 512 octets.
 */
#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdbool.h>

/** Marque la carte comme non initialisée (avant retrait). */
void sd_spi_invalider(void);

#endif
