/**
 * @file    gps.c
 * @brief   Réception GPS sous interruption et découpage des trames GGA (voir gps.h).
 */
#include "gps.h"
#include "grove_gps_air530z.h"
#include "stm32l4xx_hal.h"
#include <string.h>
#include <stdlib.h>   /* atoi */

extern UART_HandleTypeDef GPS_UART_HANDLE;   /* défini par le driver du dépôt */

#define TAILLE_TRAME 96

static char     ligne_en_cours[TAILLE_TRAME];
static uint16_t longueur = 0;
static char     derniere_gga[TAILLE_TRAME];
static volatile uint32_t instant_gga = 0;
static volatile bool     gga_disponible = false;

/* Interruption UART4 : un caractère reçu, assemblage de la ligne. */
void UART4_IRQHandler(void)
{
    USART_TypeDef *u = GPS_UART_HANDLE.Instance;
    uint32_t isr = u->ISR;

    if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE))
        u->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;

    if (isr & USART_ISR_RXNE) {
        char c = (char)(u->RDR & 0xFF);
        if (c == '$') longueur = 0;                    /* début de trame */
        if (c == '\n' || c == '\r') {
            if (longueur > 6 && (strncmp(ligne_en_cours, "$GPGGA", 6) == 0 ||
                                 strncmp(ligne_en_cours, "$GNGGA", 6) == 0)) {
                ligne_en_cours[longueur] = '\0';
                memcpy(derniere_gga, ligne_en_cours, longueur + 1);
                instant_gga = HAL_GetTick();
                gga_disponible = true;
            }
            longueur = 0;
        } else if (longueur < TAILLE_TRAME - 1) {
            ligne_en_cours[longueur++] = c;
        }
    }
}

void gps_init(void)
{
    GroveGPS_Init();   /* UART4 9600 bauds, broches échangées (driver du dépôt) */
    __HAL_UART_ENABLE_IT(&GPS_UART_HANDLE, UART_IT_RXNE);
    HAL_NVIC_SetPriority(UART4_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(UART4_IRQn);
}

bool gps_derniere_gga(char *trame, uint32_t *instant)
{
    if (!gga_disponible) return false;
    __disable_irq();                  /* copie cohérente avec l'interruption */
    strcpy(trame, derniere_gga);
    *instant = instant_gga;
    __enable_irq();
    return true;
}

/*
 * Conversion ddmm.mmmm (ou dddmm.mmmm) vers des degrés décimaux, sans atof() :
 * partie entière et décimales lues chiffre par chiffre.
 */
static float nmea_vers_degres(const char *valeur, char hemisphere)
{
    int32_t entier = 0;
    float decimales = 0.0f, poids = 0.1f;
    const char *p = valeur;
    while (*p >= '0' && *p <= '9') entier = entier * 10 + (*p++ - '0');
    if (*p == '.') {
        for (p++; *p >= '0' && *p <= '9'; p++) { decimales += (float)(*p - '0') * poids; poids *= 0.1f; }
    }
    int32_t degres = entier / 100;
    float minutes = (float)(entier % 100) + decimales;
    float resultat = (float)degres + minutes / 60.0f;
    return (hemisphere == 'S' || hemisphere == 'W') ? -resultat : resultat;
}

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool gps_analyser_gga(const char *trame, GPS_Data *gps)
{
    memset(gps, 0, sizeof(*gps));
    strncpy(gps->raw_sentence, trame, sizeof(gps->raw_sentence) - 1);

    /* Somme de contrôle : XOR des caractères entre '$' et '*'. */
    const char *etoile = strchr(trame, '*');
    if (trame[0] != '$' || etoile == NULL || hex(etoile[1]) < 0 || hex(etoile[2]) < 0)
        return false;
    uint8_t somme = 0;
    for (const char *p = trame + 1; p < etoile; p++) somme ^= (uint8_t)*p;
    if (somme != (uint8_t)(hex(etoile[1]) * 16 + hex(etoile[2])))
        return false;

    /* Découpage champ par champ : deux virgules consécutives = champ vide. */
    char champs[10][16];
    int n = 0, k = 0;
    memset(champs, 0, sizeof(champs));
    for (const char *p = trame; p < etoile && n < 10; p++) {
        if (*p == ',') { n++; k = 0; continue; }
        if (k < 15) champs[n][k++] = *p;
    }
    if (n < 8) return false;

    /* champs : 0 $GPGGA, 1 heure, 2 lat, 3 N/S, 4 lon, 5 E/W, 6 fix, 7 satellites */
    gps->fix_quality = (uint8_t)atoi(champs[6]);
    gps->satellites  = (uint8_t)atoi(champs[7]);
    if (gps->fix_quality > 0 && champs[2][0] != '\0' && champs[4][0] != '\0') {
        gps->latitude  = nmea_vers_degres(champs[2], champs[3][0]);
        gps->longitude = nmea_vers_degres(champs[4], champs[5][0]);
        gps->valid = true;
    }
    return true;
}
