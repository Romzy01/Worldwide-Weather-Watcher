/**
 * @file    console.c
 * @brief   Console série sur USART2 (voir console.h).
 *          Réception sous interruption dans un tampon circulaire : aucun
 *          caractère n'est perdu, même pendant une écriture sur la carte SD.
 *          L'émission utilise printf(), redirigé vers USART2 par _write() (setup.c).
 */
#include "console.h"
#include "config.h"
#include "texte.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

extern UART_HandleTypeDef huart2;   /* défini dans setup.c */

#define TAILLE_TAMPON 128           /* puissance de 2 */

static volatile uint8_t  tampon[TAILLE_TAMPON];
static volatile uint16_t tete = 0, queue = 0;
static uint16_t longueur_commande = 0;

void USART2_IRQHandler(void)
{
    USART_TypeDef *u = huart2.Instance;
    uint32_t isr = u->ISR;

    if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE))
        u->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;

    if (isr & USART_ISR_RXNE) {
        uint8_t c = (uint8_t)(u->RDR & 0xFF);
        uint16_t suivant = (tete + 1) & (TAILLE_TAMPON - 1);
        if (suivant != queue) {     /* tampon plein : caractère ignoré */
            tampon[tete] = c;
            tete = suivant;
        }
    }
}

static bool lire_caractere(char *c)
{
    if (queue == tete) return false;
    *c = (char)tampon[queue];
    queue = (queue + 1) & (TAILLE_TAMPON - 1);
    return true;
}

void console_init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);   /* printf part immédiatement */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

bool console_lire_commande(char *buffer, uint16_t taille)
{
    char c;
    while (lire_caractere(&c)) {
        if (c == '\r' || c == '\n') {
            if (longueur_commande == 0) continue;       /* ligne vide ignorée */
            buffer[longueur_commande] = '\0';
            longueur_commande = 0;
            printf("\r\n");
            return true;
        }
        if ((c == '\b' || c == 0x7F) && longueur_commande > 0) {   /* effacement */
            longueur_commande--;
            printf("\b \b");
            continue;
        }
        if (isprint((unsigned char)c) && c != ' ' && longueur_commande < taille - 1) {
            c = (char)toupper((unsigned char)c);        /* commandes en majuscules */
            buffer[longueur_commande++] = c;
            putchar(c);                                  /* écho */
        }
    }
    return false;
}

bool console_vider(void)
{
    char c;
    bool recu = false;
    while (lire_caractere(&c)) recu = true;
    longueur_commande = 0;
    return recu;
}

void console_repondre(const char *message)
{
    printf("%s\r\n", message);
}

static const char *niveau_lumiere(uint16_t l)
{
    if (l < params.lumin_low) return "faible";
    if (l > params.lumin_high) return "forte";
    return "moyenne";
}

/* Affiche " NOM=valeur unité" ou " NOM=NA". */
static void afficher_valeur(const char *nom, bool valide, float valeur, uint8_t decimales, const char *unite)
{
    char nombre[16];
    if (!valide) { printf(" %s=NA", nom); return; }
    texte_decimal(nombre, sizeof(nombre), valeur, decimales);
    printf(" %s=%s%s", nom, nombre, unite);
}

void console_afficher_mesures(const Mesures *m)
{
    const RTC_DateTime *d = &m->horodatage;
    if (m->horodatage_valide) printf("[%02u/%02u/%04u %02u:%02u:%02u]", d->day, d->month, d->year,
                                     d->hours, d->minutes, d->seconds);
    else printf("[RTC NA]");

    afficher_valeur("P", m->pression_valide, m->env.pressure_hpa, 1, " hPa");
    afficher_valeur("T", m->temp_valide, m->env.temperature_c, 1, " C");
    afficher_valeur("H", m->hygr_valide, m->env.humidity_percent, 1, " %");
    if (m->lumin_valide) printf(" L=%u (%s)", m->luminosite, niveau_lumiere(m->luminosite));
    else                 printf(" L=NA");
    

if (m->gps_valide) {
    char lat[16], lon[16];

    texte_decimal(lat, sizeof(lat), (float)m->gps.latitude, 5);
    texte_decimal(lon, sizeof(lon), (float)m->gps.longitude, 5);

    if (strcmp(m->gps.raw_sentence, "GPS_SIMULE") == 0) {
        printf(" GPS=%s,%s (SIMULE)\r\n", lat, lon);
    } else {
        printf(" GPS=%s,%s (%u sat)\r\n",
               lat, lon, m->gps.satellites);
    }
} else {
    printf(" GPS=NA\r\n");
}

}
