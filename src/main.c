
#include "stm32l4xx_hal.h"
#include "grove_light.h"
#include <stdio.h>

extern void Global_Init(void);

int main(void)
{
    Global_Init();

    GroveLight_Init();

    printf("\r\n=== TEST LUMINOSITE ===\r\n");

    while (1)
    {
        uint16_t luminosite = GroveLight_ReadRaw();

        if (luminosite == 0xFFFF)
        {
            printf("Erreur lecture capteur\r\n");
        }
        else
        {
            printf("Luminosite : %u\r\n", luminosite);
        }

        HAL_Delay(1000);
    }
}
