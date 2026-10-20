/**
 * @file    led_etat.c
 * @brief   Couleurs des modes et clignotement des codes d'erreur (voir led_etat.h).
 */
#include "led_etat.h"
#include "grove_rgb.h"
#include "stm32l4xx_hal.h"

typedef struct { uint8_t r, g, b; } Couleur;

static const Couleur ROUGE  = {255,   0,   0};
static const Couleur VERT   = {  0, 255,   0};
static const Couleur BLEU   = {  0,   0, 255};
static const Couleur JAUNE  = {255, 200,   0};
static const Couleur ORANGE = {255,  60,   0};
static const Couleur BLANC  = {255, 255, 255};

/* Tableau des codes LED de la fiche : seconde couleur et durée doublée ou non. */
typedef struct { Couleur seconde; bool seconde_longue; const char *nom; } CodeLed;

static const CodeLed CODES[] = {
    [ERR_AUCUNE]               = {{0, 0, 0}, false, "aucune"},
    [ERR_RTC]                  = {{  0,   0, 255}, false, "acces horloge RTC"},
    [ERR_GPS]                  = {{255, 200,   0}, false, "acces GPS"},
    [ERR_CAPTEUR]              = {{  0, 255,   0}, false, "acces capteur"},
    [ERR_DONNEES_INCOHERENTES] = {{  0, 255,   0}, true,  "donnees incoherentes"},
    [ERR_SD_PLEINE]            = {{255, 255, 255}, false, "carte SD pleine"},
    [ERR_SD_ECRITURE]          = {{255, 255, 255}, true,  "acces ou ecriture carte SD"},
};

#define PERIODE_CLIGNOTEMENT_MS 1000UL   /* 1 Hz */

static Mode       mode_affiche = MODE_STANDARD;
static CodeErreur erreur = ERR_AUCUNE;
static bool       phase_rouge = true;
static uint32_t   t_phase = 0;
static Couleur    couleur_envoyee = {1, 1, 1};   /* force le premier envoi */

static Couleur couleur_du_mode(Mode m)
{
    switch (m) {
    case MODE_STANDARD:      return VERT;
    case MODE_CONFIGURATION: return JAUNE;
    case MODE_MAINTENANCE:   return ORANGE;
    case MODE_ECONOMIE:      return BLEU;
    default:                 return BLANC;
    }
}

/* N'envoie la trame P9813 que si la couleur change (évite le scintillement). */
static void envoyer(Couleur c)
{
    if (c.r == couleur_envoyee.r && c.g == couleur_envoyee.g && c.b == couleur_envoyee.b)
        return;
    GroveRGB_SetColor(c.r, c.g, c.b);
    couleur_envoyee = c;
}

void led_init(void)
{
    GroveRGB_Init();
}

void led_afficher_mode(Mode mode)
{
    mode_affiche = mode;
    if (erreur == ERR_AUCUNE) envoyer(couleur_du_mode(mode));
}

void led_signaler_erreur(CodeErreur e)
{
    if (e == erreur) return;          /* ne pas relancer un clignotement en cours */
    erreur = e;
    if (e == ERR_AUCUNE) {
        envoyer(couleur_du_mode(mode_affiche));
    } else {
        phase_rouge = true;
        t_phase = HAL_GetTick();
        envoyer(ROUGE);
    }
}

CodeErreur led_erreur_affichee(void)
{
    return erreur;
}

void led_mettre_a_jour(void)
{
    if (erreur == ERR_AUCUNE) return;

    const CodeLed *code = &CODES[erreur];
    /* Durées égales : 500/500 ms. Seconde couleur 2 fois plus longue : 333/667 ms. */
    uint32_t duree_rouge = code->seconde_longue ? PERIODE_CLIGNOTEMENT_MS / 3 : PERIODE_CLIGNOTEMENT_MS / 2;
    uint32_t duree = phase_rouge ? duree_rouge : PERIODE_CLIGNOTEMENT_MS - duree_rouge;

    if (HAL_GetTick() - t_phase >= duree) {
        t_phase = HAL_GetTick();
        phase_rouge = !phase_rouge;
        envoyer(phase_rouge ? ROUGE : code->seconde);
    }
}

const char *led_nom_erreur(CodeErreur e)
{
    return (e <= ERR_SD_ECRITURE) ? CODES[e].nom : "?";
}
