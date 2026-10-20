
#include "stm32l4xx_hal.h"
#include "grove_gps_air530z.h"
#include <stdio.h>

extern void Global_Init(void);

int main(void)
{
    Global_Init();
    GroveGPS_Init();

    char trame[128];

    printf("\r\n=== TEST GPS AIR530Z ===\r\n");

    while (1)
    {
        if (GroveGPS_ReadLine(trame, sizeof(trame), 2000))
        {
            printf("GPS : %s\r\n", trame);
        }
        else
        {
            printf("Aucune donnee GPS recue\r\n");
        }

        HAL_Delay(200);
    }
}
