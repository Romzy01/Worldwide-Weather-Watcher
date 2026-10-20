
#include "stm32l4xx_hal.h"
#include "grove_rtc_ds1307.h"
#include <stdio.h>

extern void Global_Init(void);

int main(void)
{
    Global_Init();

    printf("\r\n=== TEST RTC DS1307 ===\r\n");

    if (!GroveRTC_Init())
    {
        printf("Erreur initialisation RTC\r\n");
        while (1) {}
    }

    printf("RTC detectee !\r\n");

    RTC_DateTime date;

    while (1)
    {
        if (GroveRTC_GetDateTime(&date))
        {
            printf("Date : %02u/%02u/%04u\r\n",
                   date.day, date.month, date.year);

            printf("Heure : %02u:%02u:%02u\r\n",
                   date.hours, date.minutes, date.seconds);
        }
        else
        {
            printf("Erreur lecture RTC\r\n");
        }

        HAL_Delay(1000);
    }
}
