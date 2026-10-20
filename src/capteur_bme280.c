
#include "capteur_bme280.h"
#include "stm32l4xx_hal.h"
#include <stdint.h>

extern I2C_HandleTypeDef hi2c1;

#define BME_ADDR (0x76 << 1)

static uint16_t T1, P1;
static int16_t T2, P2, P3, P4, P5, P6, P7, P8, P9;
static int8_t T3, H3, H6;
static uint8_t H1;
static int16_t H2, H4, H5;
static float t_fine;
static bool initialise = false;

static uint16_t u16(const uint8_t *b)
{
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static int16_t s16(const uint8_t *b)
{
    return (int16_t)u16(b);
}

static int16_t signe12(uint16_t valeur)
{
    return (int16_t)((valeur & 0x800)
        ? (int32_t)valeur - 4096 : valeur);
}

static bool lire(uint8_t reg, uint8_t *buf, uint16_t n)
{
    return HAL_I2C_Mem_Read(&hi2c1, BME_ADDR, reg,
             I2C_MEMADD_SIZE_8BIT, buf, n, 100) == HAL_OK;
}

static bool ecrire(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c1, BME_ADDR, reg,
             I2C_MEMADD_SIZE_8BIT, &val, 1, 100) == HAL_OK;
}

bool bme280_init(void)
{
    uint8_t id, c[26], h[7];

    initialise = false;

    if (!lire(0xD0, &id, 1) || id != 0x60)
        return false;

    if (!lire(0x88, c, 26) || !lire(0xE1, h, 7))
        return false;

    T1 = u16(c);
    T2 = s16(c + 2);
    T3 = (int8_t)c[4];

    P1 = u16(c + 6);
    P2 = s16(c + 8);
    P3 = s16(c + 10);
    P4 = s16(c + 12);
    P5 = s16(c + 14);
    P6 = s16(c + 16);
    P7 = s16(c + 18);
    P8 = s16(c + 20);
    P9 = s16(c + 22);

    H1 = c[25];
    H2 = s16(h);
    H3 = (int8_t)h[2];
    H4 = signe12(((uint16_t)h[3] << 4) | (h[4] & 15));
    H5 = signe12(((uint16_t)h[5] << 4) | (h[4] >> 4));
    H6 = (int8_t)h[6];

    if (T1 == 0 || P1 == 0)
        return false;

    // Humidite x1
    if (!ecrire(0xF2, 0x01)) return false;

    // Temperature x1, pression x1, mode normal
    if (!ecrire(0xF4, 0x27)) return false;

    // Configuration du capteur
    if (!ecrire(0xF5, 0xA0)) return false;

    initialise = true;
    return true;
}

bool bme280_lire(Env_Data *env)
{
    if (env == 0)
        return false;

    if (!initialise && !bme280_init())
        return false;

    uint8_t d[8];

    if (!lire(0xF7, d, 8))
    {
        initialise = false;
        return false;
    }

    int32_t adcP = ((int32_t)d[0] << 12)
                 | ((int32_t)d[1] << 4)
                 | (d[2] >> 4);

    int32_t adcT = ((int32_t)d[3] << 12)
                 | ((int32_t)d[4] << 4)
                 | (d[5] >> 4);

    int32_t adcH = ((int32_t)d[6] << 8) | d[7];

    if (adcT == 0x80000 || adcP == 0x80000
        || adcH == 0x8000)
        return false;

    float a, b, p, hum;

    // Compensation temperature
    a = ((float)adcT / 16384.0f
       - (float)T1 / 1024.0f) * T2;

    b = ((float)adcT / 131072.0f
       - (float)T1 / 8192.0f);

    b = b * b * T3;
    t_fine = a + b;

    env->temperature_c = t_fine / 5120.0f;

    // Compensation pression
    a = t_fine / 2.0f - 64000.0f;
    b = a * a * P6 / 32768.0f;
    b += a * P5 * 2.0f;
    b = b / 4.0f + P4 * 65536.0f;

    a = (P3 * a * a / 524288.0f
       + P2 * a) / 524288.0f;

    a = (1.0f + a / 32768.0f) * P1;

    if (a == 0.0f)
        return false;

    p = 1048576.0f - adcP;
    p = (p - b / 4096.0f) * 6250.0f / a;

    a = P9 * p * p / 2147483648.0f;
    b = p * P8 / 32768.0f;

    p += (a + b + P7) / 16.0f;

    env->pressure_hpa = p / 100.0f;

    // Compensation humidite
    hum = t_fine - 76800.0f;

    hum = (adcH - (H4 * 64.0f
        + H5 / 16384.0f * hum))
        * (H2 / 65536.0f
        * (1.0f + H6 / 67108864.0f * hum
        * (1.0f + H3 / 67108864.0f * hum)));

    hum *= (1.0f - H1 * hum / 524288.0f);

    if (hum > 100.0f) hum = 100.0f;
    if (hum < 0.0f) hum = 0.0f;

    env->humidity_percent = hum;

    // Le BME280 ne mesure pas les gaz
    env->gas_resistance_ohm = 0.0f;

    return true;
}
