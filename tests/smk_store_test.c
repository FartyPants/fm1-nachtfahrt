/* SPDX-License-Identifier: GPL-3.0-only */
/* NACHTFAHRT on the SMK-37: the store's object map against the board's flash allow-list.
 *
 * Build with -DBOARD_SMK37=1. Every 4 KiB sector of the 1 MiB part is put through the app's
 * allow-list (FL_RANGE_OK / FL_GUARD, fm1_flash_map.h) and through the RAM driver's own net
 * (FL_PROTECTED): neither may ever reach the head 0x0-0x3FFF, BTIF / USRTRIM 0xC0000-0xC1FFF,
 * the stock USR 0xF4000-0xFDFFF or key_mac 0xFF000-.
 *
 * A unit's key, pad and wheel calibration lives at 0xC1000 and cannot be read back, so a wrong
 * address in storage.c is not a bug to find on the device: this test has to stop it here. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fm1_cc.h"
#if !BOARD_SMK37
#error "build with -DBOARD_SMK37=1"
#endif

#include "fm1_flash_map.h"

/* storage.c pulls its flash hooks from the unity build; give it stubs here (they are only
 * called by a test that actually reads or writes, which this one does not). */
static int st_read(uint32_t off, void *dst, uint32_t n) { (void)off; (void)dst; (void)n; return -1; }
static int st_erase(uint32_t off) { (void)off; return -1; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { (void)off; (void)src; (void)n; return -1; }
enum { OBJ_PROJ, OBJ_PANEL, OBJ_NOBJ };   /* plat.h's objects */
#include "../firmware/src/storage.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("smk store: %-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* the ranges a unit cannot get back */
static int protected_sector(uint32_t s)
{
    return s < FL_HEAD_HI || (s >= FL_CAL_LO && s < FL_CAL_HI) ||
           (s >= FL_USRRES_LO && s < FL_USRRES_HI) || s >= FL_KEYMAC_LO;
}

int main(void)
{
    uint32_t s, o, c, o2, c2;
    uint32_t leak_app = 0, leak_guard = 0, distinct = 1, in_store = 1;

    for (s = 0; s < 0x100000u; s += ST_SECTOR) {
        if (!protected_sector(s))
            continue;
        /* a whole sector, a page at its start, a page at its end: none may pass */
        leak_app += FL_RANGE_OK(s, ST_SECTOR) || FL_STORE_OK(s, ST_SECTOR) ||
                    FL_RANGE_OK(s, 256u) || FL_RANGE_OK(s + ST_SECTOR - 256u, 256u);
        leak_guard += !FL_PROTECTED(s, ST_SECTOR) || !FL_PROTECTED(s + ST_SECTOR - 256u, 256u);
    }
    check("no protected sector passes the app's allow-list (erase, page)", leak_app == 0);
    check("the RAM driver's own net (FL_PROTECTED) covers every protected sector", leak_guard == 0);

    /* a range that starts below a protected range and runs into it is refused, and so is a wrap */
    check("straddling ranges refused: 0xBF000+0x2000, 0x3000+0x2000, 0xC1000+0x1000",
          !FL_GUARD(0xBF000u, 0x2000u) && !FL_GUARD(0x3000u, 0x2000u) && !FL_GUARD(0xC1000u, 0x1000u) &&
          FL_PROTECTED(0xBF000u, 0x2000u) && FL_PROTECTED(0x3000u, 0x2000u));

    /* every object of the store: both copies inside FL_DATA, and no two copies in one sector */
    for (o = 0; o < OBJ_NOBJ; o++)
        for (c = 0; c < 2u; c++)
            in_store &= FL_STORE_OK(st_sector(o, c), ST_SECTOR) && st_sector(o, c) < FL_DATA_HI;
    check("storage.c: every object (A/B) lies inside FL_DATA", in_store);

    for (o = 0; o < OBJ_NOBJ; o++)
        for (c = 0; c < 2u; c++)
            for (o2 = 0; o2 < OBJ_NOBJ; o2++)
                for (c2 = 0; c2 < 2u; c2++)
                    if ((o != o2 || c != c2) && st_sector(o, c) == st_sector(o2, c2))
                        distinct = 0;
    check("storage.c: no two copies share a sector", distinct);

    /* the map itself: what the SMK build must see */
    check("FL_DATA 0x9D000..0xBB000, FL_OTA 0xBB000..0xC0000",
          FL_DATA_LO == 0x9D000u && FL_DATA_HI == 0xBB000u &&
          FL_OTA_LO == 0xBB000u && FL_OTA_HI == 0xC0000u);
    check("FL_PLAIN_LO = the VM start 0x9D000", FL_PLAIN_LO == 0x9D000u);
    check("FL_JEDEC_OK: the SMK part 0xEB6014 and 0x856014 only",
          FL_JEDEC_OK(0xEB6014u) && FL_JEDEC_OK(0x856014u) &&
          !FL_JEDEC_OK(0u) && !FL_JEDEC_OK(0xFFFFFFu) && !FL_JEDEC_OK(0xEB6015u));

    /* the FM-1 addresses must not survive into the SMK build. 0xFC000 is in the protected stock
     * USR area (refused), 0xDC000 lands in the sample-slot window (allowed, other firmware's data):
     * the objects moved off both. */
    check("the FM-1 addresses 0xFC000 and 0xDC000 are not used on the SMK",
          st_sector(OBJ_PROJ, 0) != 0xFC000u && st_sector(OBJ_PANEL, 0) != 0xDC000u &&
          !FL_GUARD(0xFC000u, ST_SECTOR) &&
          st_sector(OBJ_PROJ, 0) == 0x9D000u && st_sector(OBJ_PANEL, 0) == 0xA2000u);

    printf("%s\n", bad ? "SMK STORE TEST FAILED" : "smk store test passed");
    return bad != 0;
}
