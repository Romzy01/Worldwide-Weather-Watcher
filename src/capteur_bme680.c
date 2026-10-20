/**
 * @file    capteur_bme680.c
 * @brief   Lecture compensée du BME680 (voir capteur_bme680.h).
 *
 *          Registres et formules : fiche technique Bosch BME680 (BST-BME680-DS001),
 *          compensation en virgule flottante (sections 3.3.1 à 3.3.3).
 *          La mesure de gaz n'est pas utilisée (élément chauffant coupé).
 *          Calculs en float : le FPU du Cortex-M4 ne traite que la simple
 *          précision, un double serait émulé en logiciel (plus lent, plus de flash).
 */
#include "capteur_bme680.h"
#include "grove_bme680.h"
#include "board_pins.h"

extern I2C_HandleTypeDef GROVE_I2C_HANDLE;

#define REG_CHIP_ID      0xD0
#define REG_CTRL_GAS_0   0x70
#define REG_CTRL_GAS_1   0x71
#define REG_CTRL_HUM     0x72
#define REG_CTRL_MEAS    0x74
#define REG_CONFIG       0x75
#define REG_STATUS       0x1D   /* meas_status_0 : bit 7 = nouvelles données */
#define REG_DONNEES      0x1F   /* press(3) temp(3) hum(2) à partir de 0x1F  */
#define REG_COEFF1       0x89   /* 25 octets */
#define REG_COEFF2       0xE1   /* 16 octets */
#define I2C_DELAI_MS     50
#define MESURE_DELAI_MS  200    /* une mesure x2/x4/x1 dure environ 30 ms */

static uint16_t adresse = 0;   /* adresse 8 bits retenue, 0 = absent */

/* Coefficients de calibration (noms de la fiche Bosch) */
static uint16_t par_t1; static int16_t par_t2; static int8_t par_t3;
static uint16_t par_p1; static int16_t par_p2; static int8_t par_p3;
static int16_t  par_p4; static int16_t par_p5; static int8_t par_p6;
static int8_t   par_p7; static int16_t par_p8; static int16_t par_p9;
static uint8_t  par_p10;
static uint16_t par_h1; static uint16_t par_h2; static int8_t par_h3;
static int8_t   par_h4; static int8_t par_h5; static uint8_t par_h6; static int8_t par_h7;

static bool lire(uint8_t reg, uint8_t *donnees, uint16_t n)
{
    return HAL_I2C_Mem_Read(&GROVE_I2C_HANDLE, adresse, reg, I2C_MEMADD_SIZE_8BIT,
                            donnees, n, I2C_DELAI_MS) == HAL_OK;
}

static bool ecrire(uint8_t reg, uint8_t valeur)
{
    return HAL_I2C_Mem_Write(&GROVE_I2C_HANDLE, adresse, reg, I2C_MEMADD_SIZE_8BIT,
                             &valeur, 1, I2C_DELAI_MS) == HAL_OK;
}

static bool chercher_adresse(void)
{
    static const uint16_t candidats[2] = {0x76 << 1, 0x77 << 1};
    for (int i = 0; i < 2; i++) {
        uint8_t id = 0;
        adresse = candidats[i];
        if (lire(REG_CHIP_ID, &id, 1) && id == 0x61) return true;
    }
    adresse = 0;
    return false;
}

static bool lire_calibration(void)
{
    uint8_t c[41];
    if (!lire(REG_COEFF1, c, 25) || !lire(REG_COEFF2, &c[25], 16)) return false;

    par_t1 = (uint16_t)((c[34] << 8) | c[33]);
    par_t2 = (int16_t)((c[2] << 8) | c[1]);
    par_t3 = (int8_t)c[3];

    par_p1 = (uint16_t)((c[6] << 8) | c[5]);
    par_p2 = (int16_t)((c[8] << 8) | c[7]);
    par_p3 = (int8_t)c[9];
    par_p4 = (int16_t)((c[12] << 8) | c[11]);
    par_p5 = (int16_t)((c[14] << 8) | c[13]);
    par_p7 = (int8_t)c[15];
    par_p6 = (int8_t)c[16];
    par_p8 = (int16_t)((c[20] << 8) | c[19]);
    par_p9 = (int16_t)((c[22] << 8) | c[21]);
    par_p10 = c[23];

    par_h1 = (uint16_t)((c[27] << 4) | (c[26] & 0x0F));
    par_h2 = (uint16_t)((c[25] << 4) | (c[26] >> 4));
    par_h3 = (int8_t)c[28];
    par_h4 = (int8_t)c[29];
    par_h5 = (int8_t)c[30];
    par_h6 = c[31];
    par_h7 = (int8_t)c[32];
    return par_t1 != 0 && par_p1 != 0;   /* calibration vide = capteur absent */
}

bool bme680_init(void)
{
    (void)GroveBME680_Init();               /* détection du dépôt (0x76 / 0x77) */
    if (!chercher_adresse()) return false;  /* adresse retenue pour ce module   */
    if (!lire_calibration()) return false;

    /* Gaz désactivé, filtre IIR coupé, hygrométrie x1. */
    return ecrire(REG_CTRL_GAS_0, 0x08) && ecrire(REG_CTRL_GAS_1, 0x00) &&
           ecrire(REG_CONFIG, 0x00) && ecrire(REG_CTRL_HUM, 0x01);
}

bool bme680_lire(Env_Data *env)
{
    if (adresse == 0 && !bme680_init()) return false;   /* capteur rebranché ? */

    /* Température x2, pression x4, mode « forced » (une mesure puis veille). */
    if (!ecrire(REG_CTRL_HUM, 0x01) || !ecrire(REG_CTRL_MEAS, (2 << 5) | (3 << 2) | 0x01)) {
        adresse = 0;
        return false;
    }

    uint8_t status = 0;
    uint32_t debut = HAL_GetTick();
    do {
        if (!lire(REG_STATUS, &status, 1)) { adresse = 0; return false; }
    } while (!(status & 0x80) && (HAL_GetTick() - debut) < MESURE_DELAI_MS);
    if (!(status & 0x80)) return false;

    uint8_t d[8];
    if (!lire(REG_DONNEES, d, 8)) { adresse = 0; return false; }

    uint32_t adc_p = ((uint32_t)d[0] << 12) | ((uint32_t)d[1] << 4) | (d[2] >> 4);
    uint32_t adc_t = ((uint32_t)d[3] << 12) | ((uint32_t)d[4] << 4) | (d[5] >> 4);
    uint32_t adc_h = ((uint32_t)d[6] << 8) | d[7];

    /* --- Température (°C) --- */
    float v1 = ((float)adc_t / 16384.0f - (float)par_t1 / 1024.0f) * (float)par_t2;
    float v2 = ((float)adc_t / 131072.0f - (float)par_t1 / 8192.0f);
    v2 = v2 * v2 * ((float)par_t3 * 16.0f);
    float t_fine = v1 + v2;
    float temp = t_fine / 5120.0f;

    /* --- Pression (Pa) --- */
    float p1 = (t_fine / 2.0f) - 64000.0f;
    float p2 = p1 * p1 * ((float)par_p6 / 131072.0f);
    p2 = p2 + (p1 * (float)par_p5 * 2.0f);
    p2 = (p2 / 4.0f) + ((float)par_p4 * 65536.0f);
    p1 = ((((float)par_p3 * p1 * p1) / 16384.0f) + ((float)par_p2 * p1)) / 524288.0f;
    p1 = (1.0f + (p1 / 32768.0f)) * (float)par_p1;
    float pression = 1048576.0f - (float)adc_p;
    if (p1 == 0.0f) return false;
    pression = ((pression - (p2 / 4096.0f)) * 6250.0f) / p1;
    p1 = ((float)par_p9 * pression * pression) / 2147483648.0f;
    p2 = pression * ((float)par_p8 / 32768.0f);
    float p3 = (pression / 256.0f) * (pression / 256.0f) * (pression / 256.0f) * ((float)par_p10 / 131072.0f);
    pression = pression + (p1 + p2 + p3 + ((float)par_p7 * 128.0f)) / 16.0f;

    /* --- Hygrométrie (%) --- */
    float h1 = (float)adc_h - (((float)par_h1 * 16.0f) + (((float)par_h3 / 2.0f) * temp));
    float h2 = h1 * (((float)par_h2 / 262144.0f) *
                (1.0f + (((float)par_h4 / 16384.0f) * temp) + (((float)par_h5 / 1048576.0f) * temp * temp)));
    float h3 = (float)par_h6 / 16384.0f;
    float h4 = (float)par_h7 / 2097152.0f;
    float hum = h2 + ((h3 + (h4 * temp)) * h2 * h2);
    if (hum > 100.0f) hum = 100.0f;
    if (hum < 0.0f) hum = 0.0f;

    env->temperature_c = temp;
    env->pressure_hpa = pression / 100.0f;
    env->humidity_percent = hum;
    env->gas_resistance_ohm = 0.0f;
    return true;
}
