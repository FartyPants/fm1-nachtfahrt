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
    printf("seq: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
