/* SPDX-License-Identifier: GPL-3.0-only */
/* Which flash parts this firmware's storage may use (main.c's boot, main_fm1.c's gate,
 * plat_fm1.c's saves, ota.c's identity reply).
 *
 * The stock FM-1 carries a Puya P25Q80 (JEDEC 85 60 14, 1 MiB). Units sold since autumn 2026
 * (mostly AliExpress) carry a UC25HQ80 (JEDEC B3 60 14, 1 MiB, the same plain SPI NOR commands:
 * 0x9F read, 0x06 WREN, 0x20 erase 4K, 0x02 page program, 0x0B fast read, 0x05/0x35 status)
 * instead. A firmware that only knows the Puya takes those units for unknown: flash_ok stays 0,
 * so plat_store_save returns -9 (SAVE failed on the screen), the project and the panel
 * calibration stay in RAM, and ota_session is never taken, so no firmware can be installed
 * from the unit itself.
 *
 * Same policy as fm1-base's device/fm1_flash_ok.h (MPL-2.0, Charles Vestal / Laurens), restated
 * here because this firmware carries its own copy of the HAL rather than the fm1-base device
 * layer. An app that has checked a part for itself defines EN_FLASH_JEDEC_OK before this header.
 *
 * Any other answer: RAM only, nothing written. */
#ifndef EN_FLASH_PARTS_H
#define EN_FLASH_PARTS_H

/* the Puya P25Q80 (the stock part) and the UC25HQ80 (the newer units' part): both 1 MiB */
#ifndef EN_FLASH_JEDEC_OK
#define EN_FLASH_JEDEC_OK(id) ((id) == 0x856014u || (id) == 0xB36014u)
#endif

/* for the screen and the installer: which part an FM-1 has (0: not read / not sent) */
#ifndef EN_FLASH_PART_NAME
#define EN_FLASH_PART_NAME(id) ((id) == 0x856014u ? "Puya P25Q80" :                    \
                                (id) == 0xB36014u ? "UC25HQ80" : "unknown part")
#endif

#endif
