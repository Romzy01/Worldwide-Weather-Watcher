
#include "setup.h"
#include "config.h"
#include "console.h"
#include "boutons.h"
#include "led_etat.h"
#include "mesures.h"
#include "modes.h"

#include "stm32l4xx_hal.h"
#include <stdio.h>

int main(void)
{
    // Initialisation de la STM32
    Global_Init();

    // Initialisation de la console serie
    console_init();

    printf("\r\n=== WORLDWIDE WEATHER WATCHER ===\r\n");

    // Chargement des parametres
    config_charger(&params);

    // Initialisation des composants
    boutons_init();
    led_init();
    mesures_init();

    // Demarrage du mode STANDARD ou CONFIGURATION
    modes_init();

    // Boucle principale
    while (1)
    {
        // Detection des appuis longs
        EvenementBouton evenement = boutons_lire_evenement();
        modes_traiter_evenement(evenement);

        // Execution du mode actif
        modes_executer();

        // Actualisation de la LED RGB
        led_mettre_a_jour();

        HAL_Delay(10);
    }
}
