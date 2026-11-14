/* SPDX-License-Identifier: GPL-3.0-only */
/* firmware/src/fm1_flash_parts.h: the flash parts this firmware's storage may use.
 *
 * The stock part (the Puya P25Q80, JEDEC 85 60 14) and the one the units sold since autumn 2026 carry
 * (the UC25HQ80, B3 60 14) are accepted; anything else is not: the FM-1 then runs from RAM and writes
 * nothing. Both accepted parts are 1 MiB and take the same plain SPI NOR commands, so the driver in
 * firmware/hal/fm1_flash.h needs no change: what changes is which answer the boot accepts. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fm1_flash_parts.h"        /* -Ifirmware/src, like the other host tests */

static int fails;

static void check(const char *name, int got, int want)
{
    printf("  %-24s %s\n", name, got == want ? "ok" : "FAIL");
    if (got != want)
        fails++;
}

static void check_name(const char *name, uint32_t id, const char *want)
{
    const char *got = EN_FLASH_PART_NAME(id);
    printf("  %-24s %s\n", name, !strcmp(got, want) ? "ok" : "FAIL");
    if (strcmp(got, want))
        fails++;
}

int main(void)
{
    check("Puya P25Q80", EN_FLASH_JEDEC_OK(0x856014u), 1);
    check("UC25HQ80", EN_FLASH_JEDEC_OK(0xB36014u), 1);
    check("no part answered", EN_FLASH_JEDEC_OK(0u), 0);
    check("all ones", EN_FLASH_JEDEC_OK(0xFFFFFFu), 0);
    check("Winbond 4 MiB", EN_FLASH_JEDEC_OK(0xEF4016u), 0);       /* a larger part: not this layout */
    check("GigaDevice", EN_FLASH_JEDEC_OK(0xC84019u), 0);
    check("near-miss Puya", EN_FLASH_JEDEC_OK(0x856015u), 0);
    check("near-miss UC25", EN_FLASH_JEDEC_OK(0xB36015u), 0);
    check_name("name: stock", 0x856014u, "Puya P25Q80");
    check_name("name: newer unit", 0xB36014u, "UC25HQ80");
    check_name("name: other", 0xEF4016u, "unknown part");
    printf("%s\n", fails ? "FLASH PARTS FAILED" : "flash parts ok");
    return fails ? 1 : 0;
}
