
#include "stm32l4xx_hal.h"
#include "boutons.h"
#include <stdio.h>

extern void Global_Init(void);

int main(void)
{
    Global_Init();
    boutons_init();

    printf("\r\n=== TEST BOUTONS ===\r\n");
    printf("Maintiens un bouton pendant 5 secondes\r\n");

    while (1)
    {
        EvenementBouton evenement = boutons_lire_evenement();

        if (evenement == BOUTON_ROUGE_LONG)
        {
            printf("Bouton ROUGE : appui long detecte !\r\n");
        }
        else if (evenement == BOUTON_VERT_LONG)
        {
            printf("Bouton VERT : appui long detecte !\r\n");
        }

        HAL_Delay(10);
    }
}
