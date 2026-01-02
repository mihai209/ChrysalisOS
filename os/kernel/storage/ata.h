#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Inițializare driver ATA (detectare dispozitiv, setare mod PIO etc.) */
void ata_init(void);

/* IDENTIFY DEVICE
 * buffer: pointer la 256 de cuvinte uint16_t (512 bytes)
 * return:
 *   0  : succes
 *  -1  : niciun dispozitiv detectat
 *  -2  : bit de eroare setat în status
 *  -3  : timeout sau DRQ nu a fost setat
 */
int ata_identify(uint16_t* buffer);

/* Decode informații esențiale din buffer-ul IDENTIFY
 * model        : buffer de ieșire pentru numele modelului (ASCII, terminat cu \0)
 * model_len    : dimensiunea buffer-ului model (recomandat >= 40)
 * lba28_sectors: număr total de sectoare LBA28 (0 dacă nu e suportat)
 * return: 0 pe succes
 */
int ata_decode_identify(const uint16_t* id_buf, char* model, size_t model_len, uint32_t* lba28_sectors);

/* Citire PIO low-level (LBA28) - 1 sector (512 bytes) */
int ata_pio_read28(uint32_t lba, uint8_t* buffer);

/* Wrapper convenabil pentru citire sector */
int ata_read_sector(uint32_t lba, uint8_t* buffer);

/* Scriere PIO low-level (LBA28) - 1 sector (512 bytes)
 * return: 0 pe succes, negativ pe eroare
 * ATENȚIE: scrierea pe sectorul 0 (MBR) este blocată implicit.
 * Folosește ata_set_allow_mbr_write(1) pentru a permite temporar.
 */
int ata_write_sector(uint32_t lba, const uint8_t* buffer);

/* Permite/dezactivează scrierea pe sectorul 0 (MBR)
 * Implicit: dezactivat (siguranță)
 * enabled = 1 → permite
 * enabled = 0 → blochează din nou
 */
void ata_set_allow_mbr_write(int enabled);

#ifdef __cplusplus
}
#endif