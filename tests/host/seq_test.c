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

int main(void)
{
    test_next_wraps();
    test_next_return_point();
    test_next_skips();
    test_next_all_skipped();
    test_next_pos_beyond_len();
    printf("seq: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
