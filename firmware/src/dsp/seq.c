/* SPDX-License-Identifier: GPL-3.0-only */
/* The sequencer's pattern order (seq.h). Included by amb.c, so one compilation unit; also builds alone. */
#include "seq.h"
#include <stdint.h>
#include <string.h>

/* the next playing position after pos: on to the return point and round, past the skipped steps.
 * a pos past the return point (it was just shortened) goes back to the top; all skipped: stay put */
int seq_next(const seq_t *s, int pos)
{
    int len = s->len, from = pos, i;
    if (len < 1)
        return 0;
    if (from >= len || from < 0)
        from = len - 1;
    for (i = 1; i <= len; i++) {
        int p = (from + i) % len;
        if (!((s->skip >> p) & 1u))
            return p;
    }
    return pos;
}

/* positions in one pass */
int seq_cycle(const seq_t *s)
{
    int n = 0, i;
    for (i = 0; i < s->len && i < SEQ_MAX; i++)
        n += !((s->skip >> i) & 1u);
    return n;
}

/* xorshift32; a zero state is seeded first so it never sticks */
uint32_t seq_rand(uint32_t *rng)
{
    uint32_t x = *rng;
    if (x == 0)
        x = 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *rng = x;
    return x;
}

/* grow a Berlin School pattern: the root first and accented, mostly root-fifth-octave, some rests,
 * and octave jumps only among the last few playing steps */
void seq_grow(seq_t *s, uint32_t *rng, int nscale, int density, int len)
{
    int core[3], ncore, i, n, played, last[SEQ_MAX], nlast = 0, noct;
    float f;
    if (nscale < 1)
        nscale = 1;
    if (nscale > 12)
        nscale = 12;
    if (density < 0)
        density = 0;
    if (density > 100)
        density = 100;
    if (len < 1)
        len = 1;
    if (len > SEQ_MAX)
        len = SEQ_MAX;
    memset(s, 0, sizeof *s);
    s->len = (uint8_t)len;
    s->base_len = (uint8_t)len;

    /* how many steps play: a quarter even at density 0 */
    f = (float)len * (0.25f + 0.75f * (float)density / 100.0f);
    n = (int)(f + 0.5f);
    if (n < 1)
        n = 1;
    if (n > len)
        n = len;

    /* gates: step 0 always, then random free steps */
    s->gate = 1u;
    for (played = 1; played < n;) {
        int p = 1 + (int)(seq_rand(rng) % (uint32_t)(len - 1));
        if (!((s->gate >> p) & 1u)) {
            s->gate |= (uint16_t)(1u << p);
            played++;
        }
    }

    /* the core cell: root, fifth (the step nearest 7 semitones), octave */
    core[0] = 0;
    if (nscale < 3) {
        core[1] = nscale;
        ncore = 2;
    } else {
        core[1] = (nscale * 7 + 6) / 12;
        core[2] = nscale;
        ncore = 3;
    }
    for (i = 0; i < len; i++) {
        if (!((s->gate >> i) & 1u))
            continue;
        if (i == 0) {
            s->deg[0] = 0;
            s->acc |= 1u;
            continue;
        }
        if (seq_rand(rng) % 100u < 70u || nscale < 3)
            s->deg[i] = (int8_t)core[seq_rand(rng) % (uint32_t)ncore];
        else
            s->deg[i] = (int8_t)(seq_rand(rng) % (uint32_t)nscale);   /* any other step of the scale */
        last[nlast++] = i;
    }

    /* octave flags: some of the last 1..3 playing steps, never step 0 */
    noct = 1 + (int)(seq_rand(rng) % 3u);
    for (i = nlast - 1; i >= 0 && noct > 0; i--, noct--)
        if (seq_rand(rng) & 1u)
            s->oct |= (uint16_t)(1u << last[i]);
}
