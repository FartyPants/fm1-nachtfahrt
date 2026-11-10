/* SPDX-License-Identifier: GPL-3.0-only */
/* NACHTFAHRT on the SMK-37: the input HAL against the app's expectations.
 *
 * Build with -DBOARD_SMK37=1. tests/host/encoder_test.c already covers the FM-1 encoder
 * decoder; this one checks the parts a port can get wrong silently:
 *
 *  - the API fm1_input.h provides and the app calls is all there (fm1_in.notes / .buttons,
 *    fm1_input_init / tick / scan, fm1_input_edges, fm1_enc_take, fm1_led_key);
 *  - the SMK panel is bigger than the FM-1's: 37 keybed keys, 18 buttons, 8 encoders, 16 pads,
 *    and the app still sees only the FM-1's 27 note keys and 14 buttons through plat_*;
 *  - keys 5..31 are the FM-1's 27 note keys (F3..G5), so a keybed press maps to the same bit
 *    the FM-1 would have set;
 *  - the pads' white-key map and the piano layout cover the FM-1's 16 white keys and give no
 *    note twice.
 *
 * The hardware side (SPI2, the ADCs, the USRTRIM curves) is not exercised: it needs a unit. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fm1_cc.h"
#if !BOARD_SMK37
#error "build with -DBOARD_SMK37=1"
#endif

#include "smk_input.h"

/* the FM-1 panel the app still expects (plat.h); repeated here so the test does not pull the
 * whole application in */
enum { NB = 14 };
#define NKEYS 27u

static int bad;
static void check(const char *what, int ok)
{
    printf("smk input: %-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

int main(void)
{
    uint32_t k, w, i, seen;
    uint32_t seen_white = 0;

    /* the SMK panel, as the HAL declares it */
    check("the panel: 16 columns, 37 keybed keys, 18 buttons, 8 encoders, 16 pads",
          FM1_NCOL == 16u && FM1_NKEY == 37u && FM1_NBTN == 18u && FM1_NENC == 8u && SMK_NPAD == 16u);
    check("keys 5..31 are the FM-1's 27 note keys (F3..G5)",
          SMK_KEY_F3 == 5u && SMK_KEY_F3 + NKEYS - 1u == 31u);

    /* the API the app calls. plat_fm1.c and panel.c use these; a missing one is a link error
     * on the device and would only show up there, so name them here. */
    /* the API the app calls: unreferenced static functions would warn, so call them in a dead
     * branch the compiler still has to type-check */
    if (0) {
        fm1_input_init();
        fm1_input_tick();
        fm1_input_scan();
        fm1_input_edges(0);
        fm1_input_note_edges();
        (void)fm1_enc_take(0);
        fm1_led_key(0, 0);
        (void)smk_extkey_take();
    }
    check("the API the app calls exists with the FM-1 signatures", 1);
    check("fm1_in carries what the app reads (notes, buttons) and the port's extras",
          sizeof fm1_in.notes == sizeof(uint32_t) && sizeof fm1_in.buttons == sizeof(uint32_t) &&
          sizeof fm1_in.keys == sizeof(uint64_t) &&
          sizeof fm1_in.key_vel == FM1_NKEY);

    /* the two note sources: keybed keys 5..31 and the pads, both feed fm1_in.notes */
    check("fm1_in has the two note sources (keybed, pads) and 37-key state",
          sizeof fm1_in.knotes == sizeof(uint32_t) && sizeof fm1_in.pnotes == sizeof(uint32_t) &&
          (sizeof fm1_in.ks_last) / sizeof fm1_in.ks_last[0] == FM1_NKEY);

    /* the pads as white keys: the FM-1's 16 white keys of its 27, each exactly once */
    for (w = 0; w < SMK_NPAD; w++) {
        if (SMK_WHITE_NOTE[w] >= NKEYS) seen_white = 0xFFFFFFFFu;
        if (seen_white & (1u << SMK_WHITE_NOTE[w])) seen_white = 0xFFFFFFFFu;
        else seen_white |= 1u << SMK_WHITE_NOTE[w];
    }
    check("the pads' white keys: 16 distinct note keys of the FM-1's 27", seen_white != 0xFFFFFFFFu);

    /* pad <-> white key is a bijection, in both the step grid and the keyboard layout */
    seen = 0;
    for (w = 0; w < SMK_NPAD; w++) {
        smk_pad_grid = 0;
        i = SMK_PAD_OF_WHITE(w);
        if (i >= SMK_NPAD || seen & (1u << i)) { seen = 0xFFFFFFFFu; break; }
        seen |= 1u << i;
        if (SMK_WHITE_OF_PAD(i) != w) { seen = 0xFFFFFFFFu; break; }
    }
    check("pad <-> white key is a bijection (keyboard layout)", seen != 0xFFFFFFFFu);

    seen = 0;
    for (w = 0; w < SMK_NPAD; w++) {
        smk_pad_grid = 1;
        i = SMK_PAD_OF_WHITE(w);
        if (i >= SMK_NPAD || seen & (1u << i)) { seen = 0xFFFFFFFFu; break; }
        seen |= 1u << i;
        if (SMK_WHITE_OF_PAD(i) != w) { seen = 0xFFFFFFFFu; break; }
    }
    smk_pad_grid = 0;
    check("pad <-> white key is a bijection (step grid)", seen != 0xFFFFFFFFu);

    /* the piano layout: notes inside the FM-1's 27, no duplicate sounding note */
    seen = 0;
    for (w = 0; w < SMK_NPAD; w++) {
        int8_t n = SMK_PIANO_NOTE[w];
        if (n < 0) continue;                          /* a black key with no pad */
        if (n >= (int8_t)NKEYS || seen & (1u << n)) { seen = 0xFFFFFFFFu; break; }
        seen |= 1u << n;
    }
    check("the piano layout: the notes it gives are distinct and inside the 27", seen != 0xFFFFFFFFu);

    /* the SMK's extra controls do not steal the FM-1's ids */
    check("the FM-1's two octave buttons keep ids 4 and 5",
          FM1_BTN_OCT_DOWN == 4 && FM1_BTN_OCT_UP == 5);
    /* the FM-1's ids are the layer's own enum (FM1_BTN_OCT_DOWN = 4, FM1_BTN_OCT_UP = 5); the SMK's
     * four extras are matrix ids the panel calibration assigns, not ids in the FM-1's range */
    check("the FM-1's 14 ids fit the SMK panel's 18 buttons",
          NB == 14 && FM1_NBTN == 18u && FM1_BTN_OCT_DOWN < NB && FM1_BTN_OCT_UP < NB);

    /* every keybed key is addressable and no two keys share a matrix position */
    {
        uint32_t dup = 0, dupc = 0, j;
        for (k = 0; k < FM1_NKEY; k++) {
            if (SMK_KEYC[k][0] == SMK_KEYC[k][1]) dup = 1;
            for (j = 0; j < FM1_NKEY; j++)
                if (j != k && (SMK_KEYC[j][0] == SMK_KEYC[k][0] || SMK_KEYC[j][1] == SMK_KEYC[k][0] ||
                               SMK_KEYC[j][0] == SMK_KEYC[k][1] || SMK_KEYC[j][1] == SMK_KEYC[k][1]))
                    dupc = 1;
        }
        check("all 37 keybed keys: two distinct contacts, no two keys sharing a matrix position",
              !dup && !dupc);
    }

    printf("%s\n", bad ? "SMK INPUT TEST FAILED" : "smk input test passed");
    return bad != 0;
}
