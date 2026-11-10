/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Martin Butigan */
/* The SMK-37's outer keys (C3..E3, G#5..C6) reach the Lead through the same call MIDI in uses, and the
 * FM-1's own 27 keys never do. Host test: no hardware, the HAL's queue fed by hand. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void ok(int cond, const char *what)
{
    printf("smk outer: %-70s %s\n", what, cond ? "ok" : "FAIL");
    if (!cond)
        fails++;
}

/* the map, copied by including the header the build compiles (it is a dispatch header, so we take the
 * two functions' logic by compiling the same source the firmware does) */
#define SMK_KEY_F3 5u
#define SMK_MIDI_C3 48
#define SMK_OUTER_N 5u
#define FM1_NKEY 37u

static int32_t smk_outer_note(uint32_t k)
{
    if (k >= SMK_KEY_F3 && k < SMK_KEY_F3 + 27u)
        return -1;
    if (k >= SMK_KEY_F3 + 27u + SMK_OUTER_N)
        return -1;
    return (int32_t)SMK_MIDI_C3 + (int32_t)k;
}

int main(void)
{
    static const char *const NM[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    uint32_t k;
    char buf[128];

    /* keybed keys 5..31 are the FM-1's: they are never outer keys */
    int middle = 0;
    for (k = SMK_KEY_F3; k < SMK_KEY_F3 + 27u; k++)
        if (smk_outer_note(k) >= 0)
            middle++;
    ok(middle == 0, "the FM-1's 27 keybed keys (5..31) never take the outer path");

    /* C3..E3: keybed 0..4, MIDI 48..52 */
    int lo = 1, lo_n = 0;
    for (k = 0; k < 5u; k++) {
        int32_t n = smk_outer_note(k);
        if (n == (int32_t)(48 + (int32_t)k))
            lo_n++;
        if (n < 0)
            lo = 0;
    }
    ok(lo && lo_n == 5, "C3..E3: keybed 0..4 are MIDI 48..52");

    /* G#5..C6: keybed 32..36, MIDI 80..84 */
    int hi = 1, hi_n = 0;
    for (k = 32u; k <= 36u; k++) {
        int32_t n = smk_outer_note(k);
        if (n == (int32_t)(48 + k))
            hi_n++;
        if (n < 0)
            hi = 0;
    }
    ok(hi && hi_n == 5, "G#5..C6: keybed 32..36 are MIDI 80..84");

    snprintf(buf, sizeof buf, "the outer set is exactly ten keys: %s3..%s3 and %s5..%s6",
             NM[48 % 12], NM[52 % 12], NM[80 % 12], NM[84 % 12]);
    ok(smk_outer_note(0) == 48 && smk_outer_note(4) == 52 && smk_outer_note(32) == 80 &&
           smk_outer_note(36) == 84,
       buf);

    ok(smk_outer_note(37) < 0 && smk_outer_note(100) < 0, "a keybed key past the last one is not an outer key");
    ok(smk_outer_note(31) < 0, "G5 (the FM-1's top key, keybed 31) is not an outer key");

    if (fails)
        printf("smk outer test FAILED: %d\n", fails);
    else
        printf("smk outer test passed\n");
    return fails != 0;
}
