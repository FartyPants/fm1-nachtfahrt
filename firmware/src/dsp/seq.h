/* SPDX-License-Identifier: GPL-3.0-only */
/* The sequencer's pattern (seq.c): a short row of scale steps with gate, accent, octave and skip bits, and the
 * order it plays in. Pure: no audio, no state of its own. */
#ifndef SEQ_H
#define SEQ_H
#include <stdint.h>

#define SEQ_MAX 16
typedef struct {
    int8_t deg[SEQ_MAX];   /* scale steps from the root (0 = root; 7 in a 7-note scale = the octave) */
    uint16_t gate;         /* bit i: step i plays */
    uint16_t acc;          /* bit i: accented */
    uint16_t oct;          /* bit i: an octave up */
    uint16_t skip;         /* bit i: skipped: not played and not counted */
    uint8_t len;           /* the return point: steps 0..len-1 loop, 1..SEQ_MAX */
    uint8_t base_len;      /* the pattern's own length, which the performer returns to */
} seq_t;

int seq_next(const seq_t *s, int pos);   /* the next playing position after pos */
int seq_cycle(const seq_t *s);           /* positions in one pass (len minus the skipped steps below len) */
#endif
