/* SPDX-License-Identifier: GPL-3.0-only */
/* The sequencer core on the host: the pattern and its playback order. */
#include <stdio.h>
#include <string.h>
#include "seq.h"

static int fails, checks;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        checks++;                                                                                                  \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static seq_t mk(int len, unsigned skip)
{
    seq_t s;
    memset(&s, 0, sizeof s);
    s.len = (uint8_t)len;
    s.base_len = (uint8_t)len;
    s.skip = (uint16_t)skip;
    s.gate = 0xFFFFu;
    return s;
}

/* walk n calls from 0, compare to the expected order */
static void walk(const seq_t *s, const int *want, int n, const char *what)
{
    int pos = 0, i;
    for (i = 0; i < n; i++) {
        CHECK(pos == want[i], "%s: call %d gave %d, wanted %d", what, i, pos, want[i]);
        pos = seq_next(s, pos);
    }
}

static void test_next_wraps(void)
{
    seq_t s = mk(8, 0);
    CHECK(seq_next(&s, 7) == 0, "7 wraps to 0");
    CHECK(seq_next(&s, 3) == 4, "3 goes to 4");
    CHECK(seq_cycle(&s) == 8, "cycle 8");
}

static void test_next_return_point(void)
{
    static const int w[12] = {0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5};
    seq_t s = mk(6, 0);
    walk(&s, w, 12, "len 6");
}

static void test_next_skips(void)
{
    static const int w[7] = {0, 1, 3, 4, 6, 7, 0};
    seq_t s = mk(8, (1u << 2) | (1u << 5));
    walk(&s, w, 7, "skips");
    CHECK(seq_cycle(&s) == 6, "cycle 6, got %d", seq_cycle(&s));
}

static void test_next_all_skipped(void)
{
    seq_t s = mk(4, 0x000Fu), t = mk(1, 0);
    CHECK(seq_next(&s, 1) == 1, "all skipped: stay put");
    CHECK(seq_cycle(&s) == 0, "all skipped: cycle 0");
    CHECK(seq_next(&t, 0) == 0, "len 1: stays at 0");
}

static void test_next_pos_beyond_len(void)
{
    seq_t s = mk(8, 0);
    s.len = 5;
    CHECK(seq_next(&s, 7) == 0, "pos past len goes to 0");
    s.skip = 1u;
    CHECK(seq_next(&s, 7) == 1, "pos past len, 0 skipped: goes to 1");
    CHECK(seq_cycle(&s) == 4, "cycle ignores skips above len");
}

static int pop16(unsigned v)
{
    int n = 0;
    for (; v; v >>= 1)
        n += (int)(v & 1u);
    return n;
}

/* the oct and acc rules: no octave on the root, only the root accented, octaves only on the last 3 gated steps */
static void check_rules(const seq_t *s, int len, int nscale)
{
    int i, rank = 0;
    CHECK(!(s->oct & 1u), "nscale %d: no octave on step 0", nscale);
    CHECK(s->acc == 1u, "nscale %d: only bit 0 accented, got %x", nscale, (unsigned)s->acc);
    for (i = len - 1; i >= 0; i--)
        if ((s->gate >> i) & 1u)
            rank++;
        else
            CHECK(!((s->oct >> i) & 1u), "nscale %d: oct on a rest at %d", nscale, i);
    rank = 0;
    for (i = len - 1; i >= 0; i--)
        if ((s->gate >> i) & 1u) {
            rank++;
            if (rank > 3)
                CHECK(!((s->oct >> i) & 1u), "nscale %d: oct on step %d, gated rank %d from the end", nscale, i, rank);
        }
}

static void test_grow_root_first(void)
{
    uint32_t seed;
    for (seed = 1; seed <= 50; seed++) {
        uint32_t rng = seed;
        seq_t s;
        memset(&s, 0, sizeof s);
        seq_grow(&s, &rng, 7, 50, 8);
        check_rules(&s, 8, 7);
        CHECK(s.gate & 1u, "seed %u: step 0 plays", (unsigned)seed);
        CHECK(s.deg[0] == 0, "seed %u: step 0 is the root", (unsigned)seed);
        CHECK(s.acc & 1u, "seed %u: step 0 accented", (unsigned)seed);
        CHECK(s.len == 8 && s.base_len == 8 && s.skip == 0, "seed %u: len/skip", (unsigned)seed);
    }
}

static void test_grow_density(void)
{
    uint32_t rng = 5;
    seq_t s;
    memset(&s, 0, sizeof s);
    seq_grow(&s, &rng, 7, 0, 8);
    CHECK(pop16(s.gate & 0xFFu) == 2, "density 0: 2 gates, got %d", pop16(s.gate & 0xFFu));
    CHECK((s.gate & 0xFF00u) == 0, "no gates past len");
    seq_grow(&s, &rng, 7, 100, 8);
    CHECK(pop16(s.gate & 0xFFu) == 8, "density 100: 8 gates, got %d", pop16(s.gate & 0xFFu));
}

static void test_grow_in_scale(void)
{
    static const int ns[6] = {1, 2, 3, 5, 7, 12};
    int k, i;
    uint32_t seed;
    for (k = 0; k < 6; k++)
        for (seed = 1; seed <= 100; seed++) {
            uint32_t rng = seed;
            seq_t s;
            memset(&s, 0, sizeof s);
            seq_grow(&s, &rng, ns[k], 60, SEQ_MAX);
            check_rules(&s, SEQ_MAX, ns[k]);
            for (i = 0; i < SEQ_MAX; i++)
                if ((s.gate >> i) & 1u)
                    CHECK(s.deg[i] >= 0 && s.deg[i] <= 2 * ns[k], "nscale %d: deg %d out of range", ns[k], s.deg[i]);
        }
}

static void test_grow_core_cell(void)
{
    int core = 0, total = 0, i;
    uint32_t seed;
    for (seed = 1; seed <= 200; seed++) {
        uint32_t rng = seed * 2654435761u;
        seq_t s;
        memset(&s, 0, sizeof s);
        seq_grow(&s, &rng, 7, 60, 16);
        for (i = 0; i < 16; i++)
            if ((s.gate >> i) & 1u) {
                total++;
                core += (s.deg[i] == 0 || s.deg[i] == 4 || s.deg[i] == 7);
            }
    }
    CHECK(core * 10 >= total * 6, "core cell %d of %d gated steps", core, total);
}

static void test_grow_deterministic(void)
{
    uint32_t r1 = 1234, r2 = 1234;
    seq_t a, b;
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    seq_grow(&a, &r1, 7, 70, 12);
    seq_grow(&b, &r2, 7, 70, 12);
    CHECK(memcmp(&a, &b, sizeof a) == 0, "same seed, same pattern");
    CHECK(r1 == r2, "same seed, same rng after");
    CHECK(seq_rand(&r1) != 0, "rand never 0");
    {
        uint32_t z = 0;
        CHECK(seq_rand(&z) != 0 && z != 0, "zero seed is seeded");
    }
}

static void test_grow_tiny_pool(void)
{
    int nscale, density, i;
    uint32_t seed;
    for (nscale = 1; nscale <= 2; nscale++)
        for (density = 0; density <= 100; density += 25)
            for (seed = 0; seed < 20; seed++) {
                uint32_t rng = seed;
                seq_t s;
                memset(&s, 0, sizeof s);
                seq_grow(&s, &rng, nscale, density, 8);
                check_rules(&s, 8, nscale);
                CHECK(pop16(s.gate) >= 1, "tiny pool: a gate");
                for (i = 0; i < 8; i++)
                    if ((s.gate >> i) & 1u)
                        CHECK(s.deg[i] == 0 || s.deg[i] == nscale, "nscale %d: deg %d", nscale, s.deg[i]);
            }
}

/* the performer: one pattern, played on, changed mostly by its return point, skips and octaves */
static seq_t grown(uint32_t seed, int len)
{
    uint32_t rng = seed;
    seq_t s;
    memset(&s, 0, sizeof s);
    seq_grow(&s, &rng, 7, 70, len);
    return s;
}

static void test_vary_still(void)
{
    uint32_t rng = 99;
    seq_t s = grown(7, 8), o = s;
    int i;
    for (i = 0; i < 1000; i++)
        seq_vary(&s, &rng, 7, 0, 0);
    CHECK(memcmp(&s, &o, sizeof s) == 0, "motion 0 evolve 0 changes nothing");
}

static void test_vary_notes_rare(void)
{
    uint32_t rng = 4242;
    seq_t s = grown(11, 8);
    int i, changed = 0;
    for (i = 0; i < 800; i++) {
        seq_t o = s;
        seq_vary(&s, &rng, 7, 0, 100);
        CHECK(memcmp(&o.gate, &s.gate, sizeof s.gate) == 0 && o.len == s.len && o.skip == s.skip && o.oct == s.oct,
              "evolve alone moves only notes");
        changed += memcmp(o.deg, s.deg, sizeof o.deg) != 0;
    }
    CHECK(changed >= 1 && changed <= 100, "notes changed in %d of 800 calls", changed);
}

static void test_vary_return_point(void)
{
    uint32_t rng = 31337;
    seq_t s = grown(3, 8);
    int i, seen[SEQ_MAX + 1] = {0}, kinds = 0, home = 0;
    for (i = 0; i < 400; i++) {
        seq_vary(&s, &rng, 7, 100, 0);
        CHECK(s.len >= 3 && s.len <= 8, "len %d out of 3..8", s.len);
        seen[s.len] = 1;
        home += s.len == 8;
    }
    for (i = 0; i <= SEQ_MAX; i++)
        kinds += seen[i];
    CHECK(kinds >= 3, "len took %d different values", kinds);
    CHECK(home * 10 >= 400 * 3, "back at base_len %d of 400 calls", home);
    CHECK(s.base_len == 8, "base_len kept");
}

static void test_vary_root_kept(void)
{
    static const int mo[3] = {0, 50, 100}, ev[3] = {0, 50, 100};
    int a, b, i;
    for (a = 0; a < 3; a++)
        for (b = 0; b < 3; b++) {
            uint32_t rng = 17u + (uint32_t)(a * 3 + b);
            seq_t s = grown(5u + (uint32_t)a, 12);
            uint16_t acc = s.acc;
            for (i = 0; i < 1000; i++) {
                seq_vary(&s, &rng, 7, mo[a], ev[b]);
                CHECK(s.deg[0] == 0 && (s.gate & 1u), "root lost");
                CHECK(!(s.skip & 1u) && !(s.oct & 1u), "step 0 skipped or octaved");
                CHECK(s.acc == acc, "acc changed");
                CHECK(seq_cycle(&s) >= 1, "everything skipped");
            }
        }
}

/* pitch: D minor (Aeolian) at root_note 50 */
static const int aeo[7] = {0, 2, 3, 5, 7, 8, 10};
static const int pent[5] = {0, 2, 4, 7, 9};

static int in_scale(int semis, const int *st, int n)
{
    int k, pc = ((semis % 12) + 12) % 12;
    for (k = 0; k < n; k++)
        if (st[k] == pc)
            return 1;
    return 0;
}

static void test_pitch_root(void)
{
    seq_t s = mk(8, 0);
    s.deg[0] = 0;
    s.deg[1] = 7;
    s.deg[2] = 2;
    s.deg[3] = 14;
    s.oct = 1u << 2;
    CHECK(seq_pitch(&s, 0, aeo, 7, 50, 1, 0) == 50, "deg 0 is the root");
    CHECK(seq_pitch(&s, 1, aeo, 7, 50, 1, 0) == 62, "deg 7 is the octave");
    CHECK(seq_pitch(&s, 2, aeo, 7, 50, 1, 0) == 50 + 3 + 12, "deg 2 + oct bit");
    CHECK(seq_pitch(&s, 3, aeo, 7, 50, 1, 0) == 74, "deg 14: two octaves");
    CHECK(seq_pitch(&s, 0, NULL, 7, 50, 1, 5) == 50, "no scale: root");
    CHECK(seq_pitch(&s, 1, aeo, 0, 50, 0, 5) == 50, "nscale 0: root");
}

static void test_pitch_parallel(void)
{
    uint32_t rng = 8;
    seq_t s;
    int i;
    memset(&s, 0, sizeof s);
    seq_grow(&s, &rng, 7, 80, 16);
    for (i = 0; i < 16; i++)
        CHECK(seq_pitch(&s, i, aeo, 7, 50, 1, 5) == seq_pitch(&s, i, aeo, 7, 50, 1, 0) + 5, "step %d: +5", i);
}

static void test_pitch_diatonic(void)
{
    int off, n, i;
    uint32_t seed;
    seq_t t = mk(4, 0);
    for (n = 0; n < 2; n++)
        for (off = 0; off < 12; off++)
            for (seed = 1; seed <= 20; seed++) {
                uint32_t rng = seed;
                seq_t s;
                const int *st = n ? pent : aeo;
                int ns = n ? 5 : 7;
                memset(&s, 0, sizeof s);
                seq_grow(&s, &rng, ns, 80, 16);
                for (i = 0; i < 16; i++)
                    CHECK(in_scale(seq_pitch(&s, i, st, ns, 50, 0, off) - 50, st, ns),
                          "nscale %d off %d step %d: out of scale", ns, off, i);
            }
    t.deg[0] = 0;
    t.deg[1] = 2;
    CHECK(seq_pitch(&t, 0, aeo, 7, 50, 0, 3) == 53, "off 3 = degree 2: root -> 53");
    CHECK(seq_pitch(&t, 1, aeo, 7, 50, 0, 3) == 57, "off 3, deg 2 -> degree 4 -> 57");
    CHECK(seq_pitch(&t, 0, aeo, 7, 50, 0, 4) == 53, "off 4 not in scale: nearest below, 53");
    CHECK(seq_pitch(&t, 0, pent, 5, 50, 0, 5) == 54, "pentatonic off 5: nearest below is 4, 54");
    CHECK(seq_pitch(&t, 0, aeo, 7, 50, 0, 12 + 7) == 50 + 12 + 7, "off 19: an octave plus the fifth");
    CHECK(seq_pitch(&t, 0, aeo, 7, 50, 0, 0) == 50, "off 0 unchanged");
}

static void test_hist_taps(void)
{
    seq_hist_t h;
    int i, vel = -1;
    seq_hist_clear(&h);
    CHECK(seq_tap(&h, 3, &vel) == -1 && vel == 0, "cleared: none");
    for (i = 0; i < 8; i++)
        seq_hist_push(&h, 60 + i, 100 + i);
    CHECK(seq_tap(&h, 3, &vel) == 65 && vel == 105, "tap 3 is 65, got vel %d", vel);
    CHECK(seq_tap(&h, 4, NULL) == 64, "tap 4 is 64");
    CHECK(seq_tap(&h, 1, NULL) == 67 && seq_tap(&h, 8, NULL) == 60, "tap 1 latest, tap 8 oldest");
    CHECK(seq_tap(&h, 0, &vel) == -1 && vel == 0, "dt 0: none");
    CHECK(seq_tap(&h, 9, &vel) == -1, "dt 9: none");
    seq_hist_push(&h, -1, 0);
    CHECK(seq_tap(&h, 1, NULL) == -1 && seq_tap(&h, 2, NULL) == 67, "a rest is -1, ring wraps");
    seq_hist_clear(&h);
    CHECK(seq_tap(&h, 3, NULL) == -1, "after clear: none");
    for (i = 0; i < 16; i++)                     /* a bar back: the repeats' longest time */
        seq_hist_push(&h, 60 + i, 100);
    CHECK(seq_tap(&h, 16, NULL) == 60 && seq_tap(&h, 12, NULL) == 64, "tap 16 is a bar back (60)");
    CHECK(seq_tap(&h, 17, &vel) == -1 && vel == 0, "dt 17: none");
}

int main(void)
{
    test_next_wraps();
    test_next_return_point();
    test_next_skips();
    test_next_all_skipped();
    test_next_pos_beyond_len();
    test_grow_root_first();
    test_grow_density();
    test_grow_in_scale();
    test_grow_core_cell();
    test_grow_deterministic();
    test_grow_tiny_pool();
    test_vary_still();
    test_vary_notes_rare();
    test_vary_return_point();
    test_vary_root_kept();
    test_pitch_root();
    test_pitch_parallel();
    test_pitch_diatonic();
    test_hist_taps();
    printf("seq: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
