/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 Martin Butigan */
/* What the SMK-37's 16 pad LEDs show, and the pad gestures the FM-1 has not got (ui.c's own handlers).
 *
 * The owner's design (2026-11-12):
 *   pads 1..11  the FM-1's eleven black keys, in its order: the effects, the moves and the Lead's forget.
 *               The SMK's own black keys therefore play notes (the keybed is a piano), which is what the
 *               FM-1 cannot do: on the FM-1 a black key IS an effect. PERF/BLACK_KEY: F#3 G#3 A#3 C#4 D#4
 *               F#4 G#4 A#4 C#5 D#5 F#5 = note keys 1 3 5 8 10 13 15 17 20 22 25.
 *               The FM-1 has 11 (black_set); the SMK's pads hold the first 11 and the last five are dark.
 *   pads 12..16 nothing (the three the owner called useless, and two more).
 *   colour      red = off, green = on (the owner's choice). While a pad is held the screen's own
 *               PERFORM state is shown instead (a layer's mute/solo, the moves), the same state the
 *               FM-1's keys show.
 *
 * Everything goes through ui.c's handlers (black_set, perf_key), so a pad and a key can never disagree.
 * Only the SMK build compiles this file (main.c includes it under BOARD_SMK37). */

#define SMK_RGB_OFF 0x180000u                   /* (dim: an LED on a lit panel) */
#define SMK_RGB_ON 0x001800u
#define SMK_RGB_YEL 0x181000u                   /* (the left-over pads: a label, not a control) */
#define SMK_RGB_WHITE 0x101010u

static uint16_t smk_pad_was;                    /* pads down in the last frame */

/* the FM-1's black keys: BLACK_KEY/BLACK_DO/BLACK_NAME live in ui.c, included before this file */
static const char *smk_pad_name(uint32_t p)
{
    int b;
    for (b = 0; b < NBLACK && BLACK_KEY[b] != SMK_PAD_BLACK[p]; b++)
        ;
    return b < NBLACK ? BLACK_NAME[b] : 0;
}

static void smk_pads_tick(void)
{
    int32_t v;
    smk_pad_black = 1;                          /* (the pads are the FM-1's black keys, the HAL's own queue) */
    while ((v = smk_pad_take()) >= 0) {
        uint32_t p = (uint32_t)v >> 8;
        uint16_t bit = (uint16_t)(1u << p);
        if (p >= SMK_NPAD || SMK_PAD_BLACK[p] == 0xFFu)
            continue;                            /* (a left-over pad: dark and silent) */
        if (v & 0xFFu) {
            smk_pad_was |= bit;
            black_set((int)p, 1);
            {
                const char *n = smk_pad_name(p);
                if (n)
                    ui_say(n, 0);
            }
        } else {
            smk_pad_was &= (uint16_t)~bit;
            black_set((int)p, 0);                /* (as the FM-1's key: the effect ends with the release) */
        }
    }
}

/* the pads' colours: the held ones the screen's PERFORM state, the rest red (off) or green (on).
 * The colour goes to the scan (smk_input.h smk_pad_led), which lights the pad from its pressure in the
 * ISR; smk_rgb_service() then sends the colours to the LEDs from the main loop. */
static void smk_pads_leds(void)
{
    uint32_t p;
    for (p = 0; p < SMK_NPAD; p++) {
        uint32_t rgb;
        if (SMK_PAD_BLACK[p] == 0xFFu) {         /* the left-over pads: yellow, a label */
            rgb = SMK_RGB_YEL;
        } else if ((smk_pad_was >> p & 1u) && ui.perf) {   /* held, PERFORM: what the key means there */
            int i;
            for (i = 0; i < PERF_N && PERF_KEY[i] != SMK_PAD_BLACK[p]; i++)
                ;
            if (i < 5)
                rgb = perf_sounds(i) ? SMK_RGB_ON : SMK_RGB_OFF;
            else if (i >= 10)
                rgb = (ui.solo & LAYER_MUTE[i - 10]) ? SMK_RGB_ON : SMK_RGB_OFF;
            else
                rgb = (ui.moves >> (i - 5) & 1u) ? SMK_RGB_ON : SMK_RGB_OFF;
        } else {
            const int d = BLACK_DO[p];           /* the effect or move the pad plays: on or off */
            int on = d >= 100 ? (ui.mv >> (d - 100) & 1u) : (ui.bfx >> d & 1u);
            rgb = on ? SMK_RGB_ON : SMK_RGB_OFF;
        }
        smk_pad_led[p] = (uint8_t)(rgb ? 1u : 0u);   /* (the light: the scan does the rest) */
        smk_rgb_set(p, rgb);
    }
}

/* (the frame: smk_controls.h calls this from smk_controls_tick, then the faders, the wheels and the keys) */
static void smk_pads_frame(void) { smk_pads_tick(); smk_pads_leds(); }
