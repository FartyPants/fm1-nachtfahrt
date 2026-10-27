// SPDX-License-Identifier: GPL-3.0-only
// The golden renders: each of the eight pieces played 60 s from en_init, the samples FNV-1a hashed, compared
// with tests/host/golden.txt ("<piece> <hash8hex>" per line). A change that should not change the sound
// (a clean-up, a refactor) must keep all eight; a change to the sound on purpose re-records them and says so:
//   golden_test [--write] [golden.txt]
#include <stdio.h>
#include <string.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static int32_t buf[512];

static uint32_t render_piece(int i)
{
    int16_t pp[P_NPARAMS];
    uint16_t pl;
    uint32_t h = 2166136261u;
    int p, b, j;
    en_init();
    en_piece(i, pp, &pl);
    for (p = 0; p < P_NPARAMS; p++) en_set(p, pp[p]);
    en_pool(pl);
    en_regrow();
    en_play(1);
    for (b = 0; b < (int)(60.0f * 44100.0f / 256.0f); b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) h = (h ^ (uint32_t)buf[j]) * 16777619u;
    }
    return h;
}

int main(int argc, char **argv)
{
    const char *path = "tests/host/golden.txt";
    int write = 0, a, i, ok = 0;
    uint32_t want[EN_NPIECES];
    int have[EN_NPIECES];
    FILE *f;
    for (a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "--write")) write = 1;
        else path = argv[a];
    }
    if (write) {
        f = fopen(path, "w");
        if (!f) { printf("golden: can't write %s\n", path); return 1; }
        for (i = 0; i < EN_NPIECES; i++) {
            uint32_t h = render_piece(i);
            fprintf(f, "%d %08x\n", i, h);
            printf("%d %08x\n", i, h);
        }
        fclose(f);
        printf("golden: %d written to %s\n", EN_NPIECES, path);
        return 0;
    }
    memset(have, 0, sizeof have);
    f = fopen(path, "r");
    if (!f) { printf("golden: can't read %s (golden_test --write records it)\n", path); return 1; }
    {
        int n;
        unsigned int h;
        while (fscanf(f, "%d %x", &n, &h) == 2)
            if (n >= 0 && n < EN_NPIECES) { want[n] = (uint32_t)h; have[n] = 1; }
    }
    fclose(f);
    for (i = 0; i < EN_NPIECES; i++) {
        uint32_t h = render_piece(i);
        if (have[i] && h == want[i]) ok++;
        else if (have[i]) printf("piece %d: %08x, golden %08x\n", i, h, want[i]);
        else printf("piece %d: %08x, no golden\n", i, h);
    }
    printf("golden: %d/%d match\n", ok, EN_NPIECES);
    return ok == EN_NPIECES ? 0 : 1;
}
