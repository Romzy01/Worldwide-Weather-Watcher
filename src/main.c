
#include "stm32l4xx_hal.h"
#include "grove_rgb.h"
#include <stdio.h>

extern void Global_Init(void);

int main(void)
{
    Global_Init();
    GroveRGB_Init();

    printf("\r\n=== TEST LED RGB ===\r\n");

    while (1)
    {
        printf("ROUGE\r\n");
        GroveRGB_SetColor(255, 0, 0);
        HAL_Delay(2000);

        printf("VERT\r\n");
        GroveRGB_SetColor(0, 255, 0);
        HAL_Delay(2000);

        printf("BLEU\r\n");
        GroveRGB_SetColor(0, 0, 255);
        HAL_Delay(2000);

        printf("LED ETEINTE\r\n");
        GroveRGB_Off();
        HAL_Delay(2000);
    }
}
