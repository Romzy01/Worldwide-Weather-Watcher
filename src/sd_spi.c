/**
 * @file    sd_spi.c
 * @brief   Couche « diskio » de FatFs pour la carte SD en SPI (voir sd_spi.h).
 *
 *          SDLogger_Init() du dépôt réalise la séquence d'initialisation
 *          (CMD0, CMD8, ACMD41, CMD58) à vitesse lente. Ce fichier ajoute :
 *            - le passage du SPI à 1 MHz une fois la carte prête ;
 *            - la lecture (CMD17) et l'écriture (CMD24) d'un secteur de 512 octets ;
 *            - la synchronisation demandée par FatFs (CTRL_SYNC).
 */
#include "sd_spi.h"
#include "sd_logger.h"
#include "board_pins.h"
#include "ff.h"
#include "diskio.h"

extern SPI_HandleTypeDef SD_SPI_HANDLE;

#define CMD17  17   /* READ_SINGLE_BLOCK */
#define CMD24  24   /* WRITE_BLOCK       */
#define JETON_DONNEES  0xFE

static volatile DSTATUS etat = STA_NOINIT;
static bool carte_sdhc = false;   /* SDHC : adressage par secteur, SDSC : par octet */

static void regler_vitesse(uint32_t prediviseur)
{
    SD_SPI_HANDLE.Init.BaudRatePrescaler = prediviseur;
    HAL_SPI_Init(&SD_SPI_HANDLE);
}

static uint8_t echanger(uint8_t octet)
{
    uint8_t recu = 0xFF;
    HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &octet, &recu, 1, 50);
    return recu;
}

static void selectionner(void)   { HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET); }
static void deselectionner(void) { HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET); echanger(0xFF); }

static bool attendre_prete(uint32_t delai_ms)
{
    uint32_t debut = HAL_GetTick();
    do {
        if (echanger(0xFF) == 0xFF) return true;
    } while ((HAL_GetTick() - debut) < delai_ms);
    return false;
}

/* Envoie une commande ; la carte reste sélectionnée. Renvoie la réponse R1. */
static uint8_t commande(uint8_t cmd, uint32_t argument)
{
    deselectionner();
    selectionner();
    if (!attendre_prete(500)) return 0xFF;

    echanger(0x40 | cmd);
    echanger((uint8_t)(argument >> 24));
    echanger((uint8_t)(argument >> 16));
    echanger((uint8_t)(argument >> 8));
    echanger((uint8_t)argument);
    echanger(0x01);   /* CRC ignoré en mode SPI hors CMD0/CMD8 */

    uint8_t r1 = 0xFF;
    for (int i = 0; i < 10 && (r1 & 0x80); i++) r1 = echanger(0xFF);
    return r1;
}

void sd_spi_invalider(void)
{
    etat = STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;

    regler_vitesse(SPI_BAUDRATEPRESCALER_256);   /* < 400 kHz pour l'initialisation */
    if (!SDLogger_Init()) {
        etat = STA_NOINIT;
        return etat;
    }
    carte_sdhc = (SDLogger_GetCardType() == SDLOGGER_CARD_SDHC);
    if (!carte_sdhc) {   /* SDSC : imposer des blocs de 512 octets (CMD16) */
        commande(16, 512);
        deselectionner();
    }
    regler_vitesse(SPI_BAUDRATEPRESCALER_4);     /* 4 MHz / 4 = 1 MHz */
    etat = 0;
    return etat;
}

DSTATUS disk_status(BYTE pdrv)
{
    return (pdrv == 0) ? etat : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != 0 || count == 0) return RES_PARERR;
    if (etat & STA_NOINIT) return RES_NOTRDY;

    for (UINT n = 0; n < count; n++, buff += 512) {
        uint32_t adresse = carte_sdhc ? (uint32_t)(sector + n) : (uint32_t)(sector + n) * 512UL;
        if (commande(CMD17, adresse) != 0x00) { deselectionner(); return RES_ERROR; }

        uint32_t debut = HAL_GetTick();
        uint8_t jeton;
        do { jeton = echanger(0xFF); } while (jeton == 0xFF && (HAL_GetTick() - debut) < 200);
        if (jeton != JETON_DONNEES) { deselectionner(); return RES_ERROR; }

        for (int i = 0; i < 512; i++) buff[i] = echanger(0xFF);
        echanger(0xFF); echanger(0xFF);   /* CRC */
        deselectionner();
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != 0 || count == 0) return RES_PARERR;
    if (etat & STA_NOINIT) return RES_NOTRDY;

    for (UINT n = 0; n < count; n++, buff += 512) {
        uint32_t adresse = carte_sdhc ? (uint32_t)(sector + n) : (uint32_t)(sector + n) * 512UL;
        if (commande(CMD24, adresse) != 0x00) { deselectionner(); return RES_ERROR; }

        echanger(0xFF);
        echanger(JETON_DONNEES);
        for (int i = 0; i < 512; i++) echanger(buff[i]);
        echanger(0xFF); echanger(0xFF);   /* CRC fictif */

        uint8_t reponse = echanger(0xFF) & 0x1F;
        if (reponse != 0x05) { deselectionner(); return RES_ERROR; }   /* données refusées */
        if (!attendre_prete(500)) { deselectionner(); return RES_ERROR; }
        deselectionner();
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)buff;
    if (pdrv != 0) return RES_PARERR;
    if (etat & STA_NOINIT) return RES_NOTRDY;

    if (cmd == CTRL_SYNC) {
        selectionner();
        bool ok = attendre_prete(500);
        deselectionner();
        return ok ? RES_OK : RES_ERROR;
    }
    return RES_PARERR;   /* les autres requêtes ne servent qu'à f_mkfs, non utilisé */
}
