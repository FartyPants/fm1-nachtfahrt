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

uint32_t seq_rand(uint32_t *rng);   /* xorshift32; a zero state is seeded first, so it never sticks at 0 */
/* grow a pattern from the root: nscale notes in the scale (1..12), density 0..100 of the len steps play, len 1..SEQ_MAX */
void seq_grow(seq_t *s, uint32_t *rng, int nscale, int density, int len);
/* the performer, called at most once per 4 bars: one structural move (return point, a skip or an octave) with
 * probability motion/100, and a note change with probability evolve/100 x 1/8. deg[0] and gate bit 0 never change.
 * len_moves 0: no return-point moves, a skip or an octave only (Sequence 2's: its length stays the knob's) */
void seq_vary(seq_t *s, uint32_t *rng, int nscale, int motion, int evolve, int len_moves);

/* the MIDI note of step i. steps: the scale's semitones from the root, ascending from 0 (en_scale's output), nscale
 * of them; root_note: the piece's root as a MIDI note in the voice's register; chord_off: the chord walk's root in
 * semitones above the key's root (0..11).
 * parallel 1: the whole shape shifted by chord_off semitones (it may leave the scale);
 * parallel 0: shifted by the chord root's scale degree (it stays in the scale, the shape bends) */
int seq_pitch(const seq_t *s, int i, const int *steps, int nscale, int root_note, int parallel, int chord_off);

/* the last 16 sixteenths (a bar), for the repeat taps; note -1: nothing played */
#define SEQ_HIST 16
typedef struct {
    int16_t note[SEQ_HIST];
    uint8_t vel[SEQ_HIST];
    uint8_t pos;   /* where the next push goes */
} seq_hist_t;
void seq_hist_clear(seq_hist_t *h);
void seq_hist_push(seq_hist_t *h, int note, int vel);   /* once per sixteenth, after the step (-1: nothing played) */
/* the note played dt sixteenths before the current one (dt 1..SEQ_HIST, 1 = the latest push; the current one not yet
 * pushed); its velocity into *vel (may be NULL); -1 and vel 0 if none or dt out of range */
int seq_tap(const seq_hist_t *h, int dt, int *vel);
#endif
