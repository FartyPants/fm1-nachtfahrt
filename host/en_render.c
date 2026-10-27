/* SPDX-License-Identifier: GPL-3.0-only */
/* The engine alone, to a WAV: for listening to changes quickly (no UI, no screen).
 *
 *   en_render OUT.wav SECS [piece=N] [NAME=V ...] [freeze=T0-T1] [seed=S] [log]
 *
 * piece=N takes piece N's recipe (0..7) first; NAME=V sets a parameter by its name as amb.c's table
 * has it, spaces as underscores (Echo_time=2, Cutoff=60); freeze=T0-T1 holds from T0 to T1 seconds;
 * NAME=V@T sets it at T seconds instead; fx=N@T0-T1 holds the effects grid's effect N (0..15) from T0 to T1; log prints each bar's section and stabs, and the chord as it moves.
 *   cc -O2 -ffp-contract=off -D_GNU_SOURCE -DEN_HOST -Ifirmware/src/dsp -o build/host/en_render host/en_render.c \
 *      firmware/src/dsp/amb.c -lm */
#define EN_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st, (void)d1, (void)d2; }

static void wr32(FILE *f, uint32_t v) { fputc((int)(v & 255), f), fputc((int)(v >> 8 & 255), f), fputc((int)(v >> 16 & 255), f), fputc((int)(v >> 24), f); }
static void wr16(FILE *f, uint32_t v) { fputc((int)(v & 255), f), fputc((int)(v >> 8 & 255), f); }

static int find_param(const char *name, int len)
{
    int i, k;
    for (i = 0; i < P_NPARAMS; i++) {
        const char *n = en_param_info(i)->name;
        for (k = 0; k < len && n[k]; k++)
            if (n[k] != name[k] && !(n[k] == ' ' && name[k] == '_'))
                break;
        if (k == len && !n[k])
            return i;
    }
    return -1;
}

int main(int argc, char **argv)
{
    static int32_t buf[2 * 256];
    int16_t par[P_NPARAMS];
    uint16_t pool;
    FILE *f;
    double secs;
    uint32_t frames, done = 0, last16 = 0;
    int i, logit = 0, piece = 0, sec_was = -1;
    double fz0 = -1, fz1 = -1, fx0[16], fx1[16];
    int nfx = 0, fxn[16];
    uint32_t fxwas = 0;
    unsigned seed = 0;
    struct { double t; int p, v; } ev[64];
    int nev = 0, e;
    if (argc < 3) {
        fprintf(stderr, "usage: en_render OUT.wav SECS [piece=N] [NAME=V ...] [freeze=T0-T1] [seed=S] [log]\n");
        return 2;
    }
    secs = atof(argv[2]);
    en_init();
    for (i = 3; i < argc; i++)
        if (!strncmp(argv[i], "piece=", 6))
            piece = atoi(argv[i] + 6);
    en_piece(piece, par, &pool);
    for (i = 0; i < P_NPARAMS; i++)
        if (i != P_TUNE && i != P_MIDI)
            en_set(i, par[i]);
    en_pool(pool);
    for (i = 3; i < argc; i++) {
        const char *a = argv[i], *eq = strchr(a, '=');
        if (!strcmp(a, "log")) {
            logit = 1;
        } else if (!strncmp(a, "freeze=", 7)) {
            sscanf(a + 7, "%lf-%lf", &fz0, &fz1);
        } else if (!strncmp(a, "fx=", 3) && nfx < 16) {
            sscanf(a + 3, "%d@%lf-%lf", &fxn[nfx], &fx0[nfx], &fx1[nfx]);
            nfx++;
        } else if (!strncmp(a, "seed=", 5)) {
            seed = (unsigned)atoi(a + 5);
        } else if (eq && strncmp(a, "piece=", 6)) {
            int p = find_param(a, (int)(eq - a));
            const char *at = strchr(eq, '@');
            if (p < 0) {
                fprintf(stderr, "no parameter %.*s\n", (int)(eq - a), a);
                return 2;
            }
            if (at && nev < 64) {
                ev[nev].t = atof(at + 1), ev[nev].p = p, ev[nev].v = atoi(eq + 1);
                nev++;
            } else {
                en_set(p, atoi(eq + 1));
            }
        }
    }
    if (seed)
        en_seed(seed);
    en_regrow();
    en_play(1);
    frames = (uint32_t)(secs * 44100.0);
    f = fopen(argv[1], "wb");
    if (!f)
        return 1;
    fwrite("RIFF", 1, 4, f), wr32(f, 36 + frames * 4), fwrite("WAVEfmt ", 1, 8, f), wr32(f, 16), wr16(f, 1), wr16(f, 2);
    wr32(f, 44100), wr32(f, 44100 * 4), wr16(f, 4), wr16(f, 16), fwrite("data", 1, 4, f), wr32(f, frames * 4);
    while (done < frames) {
        uint32_t n = frames - done > 256 ? 256 : frames - done, j;
        double t = done / 44100.0;
        if (fz0 >= 0) {
            static int fzs;
            int want = t >= fz0 && t < fz1;
            if (want != fzs)
                en_freeze(fzs = want);
        }
        {
            uint32_t m = 0;
            for (e = 0; e < nfx; e++)
                if (t >= fx0[e] && t < fx1[e])
                    m |= 1u << fxn[e];
            if (m != fxwas)
                en_fx(fxwas = m);
        }
        for (e = 0; e < nev; e++)
            if (ev[e].t >= 0 && t >= ev[e].t) {
                en_set(ev[e].p, ev[e].v);
                ev[e].t = -1;
            }
        en_render(buf, n, 2048);
        for (j = 0; j < 2 * n; j++)
            wr16(f, (uint32_t)(uint16_t)(int16_t)(buf[j] >> 8));
        done += n;
        if (logit && en_bars / 16u != last16 / 16u) {
            char nm[16];
            int pcs[4], k;
            for (k = 0; k < 4; k++)
                pcs[k] = en_chord[k] % 12;
            pcs[0] = en_chord_root;
            printf("%6.1fs bar %3u %-6s%s stabs ", t, en_bars / 16u, EN_SEC_NAME[en_section],
                   en_section != sec_was ? "*" : " ");
            for (k = 0; k < 16; k++)
                putchar(en_stab_pat >> k & 1u ? 'x' : '.');
            printf("  chord %s (%d %d %d %d) arc %+.2f throws %u%s\n", en_chord_name(pcs, en_chord_n, nm), en_chord[0],
                   en_chord[1], en_chord[2], en_chord[3], (double)en_arc, en_throws, en_frozen ? " HOLD" : "");
            sec_was = en_section;
        }
        last16 = en_bars;
    }
    fclose(f);
    return 0;
}
