/**
 * @file    gps.h
 * @brief   Réception non bloquante du GPS Air530Z (UART4, 9600 bauds).
 *
 *          Les caractères sont reçus sous interruption et assemblés en lignes
 *          en arrière-plan : la boucle principale n'attend jamais le GPS.
 *          La dernière trame GGA reçue est conservée avec son instant de
 *          réception. Le découpage de la trame est fait ici, champ par champ,
 *          car GroveGPS_ReadGGA() (strtok) saute les champs vides d'une
 *          trame sans position et décale les champs suivants.
 */
#ifndef GPS_H
#define GPS_H

#include "types_station.h"

/** Initialise l'UART (GroveGPS_Init) puis active la réception sous interruption. */
void gps_init(void);

/**
 * Copie la dernière trame GGA reçue.
 * @param trame   tampon d'au moins 96 caractères
 * @param instant reçoit HAL_GetTick() au moment de la réception
 * @return false si aucune trame GGA n'a encore été reçue
 */
bool gps_derniere_gga(char *trame, uint32_t *instant);

/**
 * Découpe une trame GGA, champs vides compris, et vérifie sa somme de contrôle.
 * @return false si la trame est mal formée ; gps->valid indique la présence d'une position
 */
bool gps_analyser_gga(const char *trame, GPS_Data *gps);

#endif
