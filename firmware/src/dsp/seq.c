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

/* one note for a step: mostly the core cell (root, fifth, octave), sometimes any step of the scale */
static int seq_pick_deg(uint32_t *rng, int nscale)
{
    int core[3], ncore;
    if (nscale < 1)
        nscale = 1;
    if (nscale > 12)
        nscale = 12;
    core[0] = 0;
    if (nscale < 3) {
        core[1] = nscale;
        ncore = 2;
    } else {
        core[1] = (nscale * 7 + 6) / 12;   /* the fifth: the step nearest 7 semitones */
        core[2] = nscale;
        ncore = 3;
    }
    if (seq_rand(rng) % 100u < 70u || nscale < 3)
        return core[seq_rand(rng) % (uint32_t)ncore];
    return (int)(seq_rand(rng) % (uint32_t)nscale);   /* any other step of the scale */
}

/* grow a Berlin School pattern: the root first and accented, mostly root-fifth-octave, some rests,
 * and octave jumps only among the last few playing steps */
void seq_grow(seq_t *s, uint32_t *rng, int nscale, int density, int len)
{
    int i, n, played, last[SEQ_MAX], nlast = 0, noct;
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

    for (i = 0; i < len; i++) {
        if (!((s->gate >> i) & 1u))
            continue;
        if (i == 0) {
            s->deg[0] = 0;
            s->acc |= 1u;
            continue;
        }
        s->deg[i] = (int8_t)seq_pick_deg(rng, nscale);
        last[nlast++] = i;
    }

    /* octave flags: some of the last 1..3 playing steps, never step 0 */
    noct = 1 + (int)(seq_rand(rng) % 3u);
    for (i = nlast - 1; i >= 0 && noct > 0; i--, noct--)
        if (seq_rand(rng) & 1u)
            s->oct |= (uint16_t)(1u << last[i]);
}

/* the performer: one structural move (return point, a skip or an octave) with probability motion/100, and
 * independently a note change with probability evolve/100 x 1/8. step 0 is never touched, acc never changes */
void seq_vary(seq_t *s, uint32_t *rng, int nscale, int motion, int evolve)
{
    int cand[SEQ_MAX], n = 0, i, len = s->len, base = s->base_len;
    if (evolve > 100)
        evolve = 100;
    if (len > SEQ_MAX)
        len = SEQ_MAX;

    if (motion > 0 && seq_rand(rng) % 100u < (uint32_t)motion) {
        switch (seq_rand(rng) % 3u) {
        case 0:   /* return point: away from base_len by 1..3 (not below 3), home, or away again */
            if (base >= 3) {
                int lo = base - 3 < 3 ? 3 : base - 3, to;
                if (len == base) {
                    to = base - 1 - (int)(seq_rand(rng) % 3u);
                    s->len = (uint8_t)(to < 3 ? 3 : to);
                } else if (seq_rand(rng) & 1u) {
                    s->len = (uint8_t)base;
                } else {
                    s->len = (uint8_t)(lo + (int)(seq_rand(rng) % (uint32_t)(base - lo + 1)));
                }
            }
            break;
        case 1:   /* skip: flip a gated step in 1..len-1 */
            for (i = 1; i < len; i++)
                if ((s->gate >> i) & 1u)
                    cand[n++] = i;
            if (n)
                s->skip ^= (uint16_t)(1u << cand[seq_rand(rng) % (uint32_t)n]);
            break;
        default:   /* octave: flip one of the last 3 gated steps, never step 0 */
            for (i = len - 1; i >= 1 && n < 3; i--)
                if ((s->gate >> i) & 1u)
                    cand[n++] = i;
            if (n)
                s->oct ^= (uint16_t)(1u << cand[seq_rand(rng) % (uint32_t)n]);
            break;
        }
    }

    /* a note change: rare, one gated step other than the root */
    if (evolve > 0 && seq_rand(rng) % 800u < (uint32_t)evolve) {
        for (n = 0, i = 1; i < len; i++)
            if ((s->gate >> i) & 1u)
                cand[n++] = i;
        if (n)
            s->deg[cand[seq_rand(rng) % (uint32_t)n]] = (int8_t)seq_pick_deg(rng, nscale);
    }
}

/* the note of scale degree d (0 up, past nscale climbs the octaves) */
static int seq_degnote(const int *steps, int nscale, int d, int root_note)
{
    if (d < 0)
        d = 0;
    return root_note + 12 * (d / nscale) + steps[d % nscale];
}

int seq_pitch(const seq_t *s, int i, const int *steps, int nscale, int root_note, int parallel, int chord_off)
{
    int d = s->deg[i], up = ((s->oct >> i) & 1u) ? 12 : 0, k, j;
    if (!steps || nscale < 1)
        return root_note;
    if (parallel)
        return seq_degnote(steps, nscale, d, root_note) + up + chord_off;
    /* the chord root's scale degree: the scale step at or just below its pitch class */
    for (k = 0, j = 0; j < nscale; j++)
        if (steps[j] <= chord_off % 12)
            k = j;
    return seq_degnote(steps, nscale, d + k, root_note) + up + 12 * (chord_off / 12);
}

void seq_hist_clear(seq_hist_t *h)
{
    int i;
    for (i = 0; i < SEQ_HIST; i++) {
        h->note[i] = -1;
        h->vel[i] = 0;
    }
    h->pos = 0;
}

void seq_hist_push(seq_hist_t *h, int note, int vel)
{
    h->note[h->pos] = (int16_t)note;
    h->vel[h->pos] = (uint8_t)vel;
    h->pos = (uint8_t)((h->pos + 1u) % SEQ_HIST);
}

int seq_tap(const seq_hist_t *h, int dt, int *vel)
{
    int idx;
    if (dt < 1 || dt > SEQ_HIST) {
        if (vel)
            *vel = 0;
        return -1;
    }
    idx = (h->pos - dt + SEQ_HIST) % SEQ_HIST;
    if (vel)
        *vel = h->note[idx] < 0 ? 0 : h->vel[idx];
    return h->note[idx];
}
