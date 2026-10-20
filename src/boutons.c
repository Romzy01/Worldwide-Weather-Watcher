/**
 * @file    boutons.c
 * @brief   Boutons sur interruption externe (EXTI), appuis longs traités dans
 *          la boucle principale (voir boutons.h).
 *
 *          Répartition du travail (principe du prosit 6) :
 *            - ISR EXTI : une seule chose, noter l'instant du changement de niveau ;
 *            - boucle   : anti-rebond (niveau stable 30 ms), mesure de la durée
 *                         d'appui, émission d'un événement à 5 s.
 *          Même si la boucle est occupée (écriture SD d'environ 1 s), l'instant
 *          exact du début d'appui est conservé : l'appui long n'est pas raté,
 *          il est seulement signalé un peu plus tard.
 */
#include "boutons.h"
#include "grove_button.h"
#include "board_pins.h"

typedef struct {
    volatile uint32_t t_front;      /* écrit par l'ISR : instant du dernier front */
    volatile bool     front;        /* écrit par l'ISR : un front a eu lieu       */
    bool     stable;                /* état filtré : true = appuyé                */
    uint32_t t_appui;               /* début de l'appui filtré                    */
    bool     evenement_emis;        /* appui long de cet appui déjà signalé       */
} EtatBouton;

static EtatBouton rouge, vert;

static bool lire_brut(bool est_rouge)
{
    bool bouton1 = (est_rouge == (BOUTON_ROUGE_EST_BOUTON1 != 0));
    bool presse = bouton1 ? GroveButton1_IsPressed() : GroveButton2_IsPressed();
#if BOUTONS_ACTIFS_NIVEAU_HAUT
    presse = !presse;
#endif
    return presse;
}

/* ---- Interruptions : quelques instructions, aucune temporisation -------- */
static void noter_front(uint16_t broche)
{
    bool bouton1 = (broche == BUTTON1_PIN);
    EtatBouton *b = (bouton1 == (BOUTON_ROUGE_EST_BOUTON1 != 0)) ? &rouge : &vert;
    b->t_front = HAL_GetTick();
    b->front = true;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BUTTON1_PIN || GPIO_Pin == BUTTON2_PIN) noter_front(GPIO_Pin);
}

void EXTI15_10_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(BUTTON1_PIN); }   /* PA10 */
void EXTI3_IRQHandler(void)     { HAL_GPIO_EXTI_IRQHandler(BUTTON2_PIN); }   /* PB3  */

/* ---- Boucle principale -------------------------------------------------- */

/* Met à jour un bouton ; renvoie true au moment où l'appui atteint 5 s. */
static bool mettre_a_jour(EtatBouton *b, bool brut)
{
    uint32_t maintenant = HAL_GetTick();

    /* Niveau différent de l'état filtré et stable depuis 30 ms : changement retenu */
    if (brut != b->stable && (maintenant - b->t_front) >= DELAI_ANTIREBOND_MS) {
        b->stable = brut;
        b->front = false;
        if (brut) {
            b->t_appui = b->t_front;       /* instant réel du front, pas du constat */
            b->evenement_emis = false;
        }
    }

    if (b->stable && !b->evenement_emis && (maintenant - b->t_appui) >= DELAI_APPUI_LONG_MS) {
        b->evenement_emis = true;           /* un seul événement par appui */
        return true;
    }
    return false;
}

void boutons_init(void)
{
    GroveButton_Init();                     /* driver du dépôt : horloges + entrées */

    /* Même broches, passées en entrée avec interruption sur les deux fronts. */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLDOWN;              /* même tirage que le driver */
    gpio.Pin = BUTTON1_PIN;
    HAL_GPIO_Init(BUTTON1_PORT, &gpio);
    gpio.Pin = BUTTON2_PIN;
    HAL_GPIO_Init(BUTTON2_PORT, &gpio);

    rouge = (EtatBouton){0};
    vert = (EtatBouton){0};
    /* Un bouton déjà enfoncé à l'allumage ne produit pas d'appui long. */
    rouge.stable = lire_brut(true);
    vert.stable = lire_brut(false);
    rouge.evenement_emis = vert.evenement_emis = true;

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    HAL_NVIC_SetPriority(EXTI3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}

bool boutons_rouge_presse(void)
{
    /* Trois lectures concordantes espacées de 10 ms (démarrage uniquement). */
    for (int i = 0; i < 3; i++) {
        if (!lire_brut(true)) return false;
        HAL_Delay(10);
    }
    return true;
}

EvenementBouton boutons_lire_evenement(void)
{
    bool rouge_long = mettre_a_jour(&rouge, lire_brut(true));
    bool vert_long = mettre_a_jour(&vert, lire_brut(false));

    if (rouge_long) return BOUTON_ROUGE_LONG;
    if (vert_long) return BOUTON_VERT_LONG;
    return BOUTON_AUCUN;
}
