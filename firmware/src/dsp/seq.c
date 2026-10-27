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
