/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument on the host: the pure parts (Euclid, scales, chords, voicings, grooves, names, texts) and
 * the render (sound, no NaN, bounded; the kick on the floor, the chord drifts and holds, never on a
 * diminished fifth, freeze holds, the arrangement moves with Motion, throws, the loops evolve). */
#define _POSIX_C_SOURCE 200809L                 /* (fork, for runs compared from one state) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include "amb.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

/* MIDI out: counted; the kick's note-ons (36 on channel 10) by the bar they fall in, and each bar's
 * section (as a note-on from the grid sees it; 0xFF: no note that bar) */
static uint32_t midi_count, kicks, kick_off_beat, drum_notes37, claps;
static int bass_lo = 999, bass_hi = -1, bass_pcs;  /* the bass's note-ons (channel 3): its range, its pitch classes */
#define KBARS 512
static uint8_t kick_bar[KBARS], bar_sec[KBARS];
/* (BERLIN) the sequence's note-ons (channel 2) in order: the sixteenth, the note, the velocity; every channel's
 * notes sounding (note-ons less note-offs); note-ons on channel 10 and above (the drums', which are gone) */
#define SQN 8192
static uint32_t sq_n, sq_at[SQN], drum_ch_ons, s2_n, s2_at[SQN];
static uint8_t sq_note[SQN], sq_vel[SQN], s2_note[SQN], s2_vel[SQN], dr_note[SQN];
static uint32_t ld_first = 0xFFFFFFFFu;          /* (the Lead's first note-on, its sixteenth) */
static uint32_t dr_n, dr_at[SQN];
static uint32_t ld_ons, ld_groups, ld_mono_bad, ld_outscale, ld_last_off = 0xFFFFFFFFu, ld_on_now, ld_pool = 0xFFFu;
static int last_lead;                            /* (the Lead's last note-on) */
static int sounding[16][128];
/* (BERLIN, the filter motion) the sequences' velocities against the accent rule (vel_lo >= 0: 120 or vel_lo, else counted
 * in vel_bad); the Sequence's note-ons by the arc's section, and all of them */
static int vel_lo = -1;
static uint32_t vel_bad, sec_ons[EN_NSEC], ch2_ons;
/* (BERLIN, the Lead's memory) the Lead's notes (channel 3) logged in order: onset and end (sixteenths; end ~0u: still
 * sounding), the note, the phrase it belongs to (the engine's count, en_lead_phr) and its role, the section, the
 * chord's root (pitch class) as the note began */
#define LGN 16384
static struct {
    uint32_t on, off;
    uint8_t note, sec, phr, role, croot;
} lg[LGN];
static uint32_t lg_n;
static int lg_open = -1;                         /* (the note sounding, its entry; -1: none) */
static void lg_end(void)
{
    if (lg_open >= 0)
        lg[lg_open].off = en_bars;
    lg_open = -1;
}
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t b = en_bars / 16u;
    midi_count++;
    if (st == 0x92u && d2 > 0u) {
        lg_end();
        if (lg_n < LGN) {
            lg[lg_n].on = en_bars, lg[lg_n].off = 0xFFFFFFFFu, lg[lg_n].phr = en_lead_phr;
            lg[lg_n].note = (uint8_t)d1, lg[lg_n].sec = en_section, lg[lg_n].role = en_lead_role;
            lg[lg_n].croot = en_chord_root;
            lg_open = (int)lg_n++;
        }
    } else if (st == 0x82u || (st == 0x92u && !d2) || (st == 0xB2u && d1 == 123u)) {
        lg_end();
    }
    if ((st & 0xF0u) == 0x90u && d2 > 0u) {
        sounding[st & 15u][d1 & 127u]++;
        if ((st & 15u) == 1u) {
            ch2_ons++;
            sec_ons[en_section < EN_NSEC ? en_section : 0]++;
        }
        if (vel_lo >= 0 && ((st & 15u) == 1u || (st & 15u) == 3u) && d2 != 120u && d2 != (uint32_t)vel_lo)
            vel_bad++;
        if ((st & 15u) >= 9u)
            drum_ch_ons++;
        if ((st & 15u) == 1u && sq_n < SQN)
            sq_at[sq_n] = en_bars, sq_note[sq_n] = (uint8_t)d1, sq_vel[sq_n] = (uint8_t)d2, sq_n++;
        if ((st & 15u) == 2u) {                  /* the Lead: one at a time, its phrases, in the scale */
            ld_mono_bad += ld_on_now;
            ld_on_now = 1;
            ld_ons++;
            if (ld_last_off == 0xFFFFFFFFu || en_bars - ld_last_off >= 16u)
                ld_groups++;
            ld_outscale += !(ld_pool >> ((d1 + 120u - 2u) % 12u) & 1u);
            last_lead = (int)d1;
        }
        if ((st & 15u) == 0u && dr_n < SQN)
            dr_at[dr_n] = en_bars, dr_note[dr_n] = (uint8_t)d1, dr_n++;
        if ((st & 15u) == 3u && s2_n < SQN)
            s2_at[s2_n] = en_bars, s2_note[s2_n] = (uint8_t)d1, s2_vel[s2_n] = (uint8_t)d2, s2_n++;
        if ((st & 15u) == 2u && ld_first == 0xFFFFFFFFu)
            ld_first = en_bars;
    } else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) {
        sounding[st & 15u][d1 & 127u]--;
        if ((st & 15u) == 2u)
            ld_on_now = 0, ld_last_off = en_bars;
    }
    else if ((st & 0xF0u) == 0xB0u && d1 == 123u && (st & 15u) != 1u && (st & 15u) != 3u)
        memset(sounding[st & 15u], 0, sizeof sounding[0]);   /* all notes off: the channel quiet, as a synth takes it
                                                              * (but the sequences', which end each note themselves) */
    if ((st & 0xF0u) == 0x90u && d2 > 0u && b < KBARS)
        bar_sec[b] = en_section;
    if (st == 0x92u && d2 > 0u) {
        bass_lo = (int)d1 < bass_lo ? (int)d1 : bass_lo;
        bass_hi = (int)d1 > bass_hi ? (int)d1 : bass_hi;
        bass_pcs |= 1 << (d1 % 12u);
    }
    if (st == 0x99u && d1 == 37u && d2 > 0u)
        drum_notes37++;
    if (st == 0x99u && d1 == 39u && d2 > 0u)
        claps++;
    if (st == 0x99u && d1 == 36u && d2 > 0u) {
        kicks++;
        kick_off_beat += (en_bars & 3u) != 0u;
        if (b < KBARS)
            kick_bar[b]++;
    }
}

static uint32_t pat(const char *s)
{
    uint32_t m = 0;
    int i;
    for (i = 0; s[i]; i++)
        if (s[i] == 'x')
            m |= 1u << i;
    return m;
}

static int popcount(uint32_t m)
{
    int c = 0;
    while (m) {
        c += (int)(m & 1u);
        m >>= 1;
    }
    return c;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static int bad;
static void run(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
        }
    }
}

static uint32_t run_hash(float secs)            /* run, and an FNV hash of every sample */
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    uint32_t h = 2166136261u;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
            h = (h ^ (uint32_t)buf[j]) * 16777619u;
        }
    }
    return h;
}

/* from here, in a child process: the effects fx held for secs, the hash of what it plays (two of these start from
 * the same state to the bit, whatever the tests before left) */
static uint32_t hash_child(uint32_t fx, float secs)
{
    int fd[2];
    pid_t pid;
    uint32_t h = 0;
    fflush(stdout);
    if (pipe(fd))
        return 0;
    pid = fork();
    if (pid == 0) {
        close(fd[0]);
        if (fx)
            en_fx(fx);
        h = run_hash(secs);
        if (write(fd[1], &h, sizeof h) != (ssize_t)sizeof h)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &h, sizeof h) != (ssize_t)sizeof h)
        h = 0;
    close(fd[0]);
    waitpid(pid, 0, 0);
    return h;
}

/* a new engine, stopped, the count from the top (en_init leaves the transport, en_bars, as they were) */

static void fresh(void)
{
    en_play(0);
    run(0.01f);
    en_init();
    en_bars = 0;
    bad = 0;
    memset(kick_bar, 0, sizeof kick_bar);
    sq_n = s2_n = dr_n = 0;
    memset(bar_sec, 0xFF, sizeof bar_sec);
}

/* (BERLIN) a pattern written into the Sequence: len steps, gate bits, the degrees; nothing varies it (Motion and
 * Evolve 0), the chord held (Drift Hold), so every pass is the same */
static void seq_write(int len, uint16_t gate, const int8_t *degs)
{
    seq_t s;
    int i;
    memset(&s, 0, sizeof s);
    s.len = s.base_len = (uint8_t)len;
    s.gate = gate;
    s.acc = 1u;
    for (i = 0; i < len; i++)
        s.deg[i] = degs[i];
    en_set(P_MOTION, 0);
    en_set(P_EVOLVE, 0);
    en_set(P_DRIFT, 6);
    en_seq_set(0, &s);
}

/* the sequence's note-ons in sixteenth s: how many, and the first's note and velocity */
static int seq_at(uint32_t s, int *note, int *vel)
{
    uint32_t i;
    int c = 0;
    for (i = 0; i < sq_n; i++)
        if (sq_at[i] == s) {
            if (!c)
                *note = sq_note[i], *vel = sq_vel[i];
            c++;
        }
    return c;
}

/* bars b0..b1-1 of the record: each has `even` note-ons on its even sixteenths and `odd` on its odd ones, each odd
 * one the note (and velocity) three sixteenths before it; returns the bars that did not */
static int bars_tap_a(uint32_t b0, uint32_t b1, int even, int odd)
{
    uint32_t b, k;
    int badbars = 0;
    for (b = b0; b < b1; b++) {
        int ne = 0, no = 0, ok = 1;
        for (k = 0; k < 16u; k++) {
            int n = 0, v = 0, n3 = 0, v3 = 0, c = seq_at(16u * b + k, &n, &v);
            if (k & 1u) {
                no += c;
                if (c && (!seq_at(16u * b + k - 3u, &n3, &v3) || n3 != n || v3 != v))
                    ok = 0;
            } else
                ne += c;
        }
        badbars += !ok || ne != even || no != odd;
    }
    return badbars;
}

static void piece(int i)                         /* a piece, as the UI changes to one */
{
    int16_t pp[P_NPARAMS];
    uint16_t pl;
    int p;
    en_piece(i, pp, &pl);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, pp[p]);
    en_pool(pl);
    en_regrow();
}

/* (BERLIN) the filter motion's parameters, by layer: the Drone, the Sequence, the Lead, Sequence 2, the Atmosphere */
static const int FO_W[5] = {P_MW0, P_MW1, P_MW2, P_MW3, P_MW4}, FO_T[5] = {P_MT0, P_MT1, P_MT2, P_MT3, P_MT4},
                 FO_R[5] = {P_MR0, P_MR1, P_MR2, P_MR3, P_MR4}, FO_I[5] = {P_MI0, P_MI1, P_MI2, P_MI3, P_MI4};
static void fo_all(int w, int t, int r, int in)  /* every layer's Move page alike (-1: as it is) */
{
    int l;
    for (l = 0; l < 5; l++) {
        if (w >= 0)
            en_set(FO_W[l], w);
        if (t >= 0)
            en_set(FO_T[l], t);
        if (r >= 0)
            en_set(FO_R[l], r);
        if (in >= 0)
            en_set(FO_I[l], in);
    }
}
/* from here, in a child process: parameter p to v (p < 0: none), secs played; out: the Sequence's offset then and its
 * last note's cutoff (two of these from one state, to the bit) */
static void probe_child(int p, int v, float secs, float *out)
{
    int fd[2];
    pid_t pid;
    float r[2] = {0.0f, 0.0f};
    fflush(stdout);
    if (pipe(fd))
        return;
    pid = fork();
    if (pid == 0) {
        close(fd[0]);
        if (p >= 0)
            en_set(p, v);
        run(secs);
        r[0] = en_fo[1], r[1] = en_pluck_cut[0];
        if (write(fd[1], r, sizeof r) != (ssize_t)sizeof r)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], r, sizeof r) != (ssize_t)sizeof r)
        r[0] = r[1] = -99.0f;
    close(fd[0]);
    waitpid(pid, 0, 0);
    out[0] = r[0], out[1] = r[1];
}

static uint32_t chord_sig(void)
{
    return (uint32_t)en_chord[0] | (uint32_t)en_chord[1] << 7 | (uint32_t)en_chord[2] << 14 |
           (uint32_t)en_chord[3] << 21;
}

/* the chord playing, by its root: on the degree whose pitch class it is, its tones and name; 0 if none */
static int chord_now(uint32_t pl, int root, int *pcs, char *name)
{
    int st[12], m = en_scale(pl, st), d, n;
    for (d = 0; d < m; d++)
        if ((root + st[d]) % 12 == en_chord_root) {
            n = en_chord_tones(pl, root, d, pcs);
            en_chord_name(pcs, n, name);
            return n;
        }
    return 0;
}

/* (BERLIN, the Lead's memory) a note as a scale degree from the root (any octave: 7 a degree an octave up, in a
 * 7-note scale), in the pool lg_pool on root lg_root; -999 if it isn't in the scale */
static uint32_t lg_pool = 0xAADu;
static int lg_root;
static int lg_deg(int note)
{
    int st[12], m = en_scale(lg_pool, st), r = note - lg_root + 120, k;
    for (k = 0; k < m; k++)
        if (st[k] == r % 12)
            return (r / 12 - 10) * m + k;
    return -999;
}

/* the log cleared, secs played, then the Lead's numbers (the report's "logging four numbers"): the share of its
 * intervals within a phrase of 2 semitones or less; after a leap of 5 or more, how often the next interval turns
 * back; and how often a call recurs (spec 7.2): its (degree interval, onset gap, last note's length) string the same
 * as one of the 4 calls before it's, exactly or transposed (answers not counted: their endings differ by design) */
typedef struct {
    float step_share, leap_turn, pair_recur;
    uint32_t notes, phrases, intervals, leaps, compared;
} lm_t;
static int lg_first(uint32_t i) { return !i || lg[i - 1].phr != lg[i].phr; }
static int lg_last(uint32_t i) { return i + 1 >= lg_n || lg[i + 1].phr != lg[i].phr; }
static int lg_same(uint32_t a, uint32_t b)       /* the phrases from entries a and b: the same string (transposed) */
{
    for (;; a++, b++) {
        const int la = lg_last(a), lb = lg_last(b);
        if (la != lb || lg[a].off == 0xFFFFFFFFu || lg[b].off == 0xFFFFFFFFu)
            return 0;
        if (la)
            return lg[a].off - lg[a].on == lg[b].off - lg[b].on;
        if (lg[a + 1].on - lg[a].on != lg[b + 1].on - lg[b].on ||
            lg_deg(lg[a + 1].note) - lg_deg(lg[a].note) != lg_deg(lg[b + 1].note) - lg_deg(lg[b].note) ||
            lg_deg(lg[a].note) < -99 || lg_deg(lg[b].note) < -99)
            return 0;
    }
}
static void lead_metrics(float secs, lm_t *m)
{
    uint32_t i, steps = 0, turns = 0, recur = 0, starts[4] = {0, 0, 0, 0}, ns = 0;
    memset(m, 0, sizeof *m);
    lg_n = 0, lg_open = -1;
    run(secs);
    m->notes = lg_n;
    for (i = 0; i < lg_n; i++) {
        m->phrases += (uint32_t)lg_first(i);
        if (lg_first(i) && lg[i].role != LR_ANSWER) {   /* a call: the same as one of the 4 before it? */
            uint32_t k, hit = 0;
            for (k = 0; k < ns && k < 4u; k++)
                hit |= (uint32_t)lg_same(starts[k], i);
            if (ns) {
                m->compared++;
                recur += hit;
            }
            for (k = 3; k > 0; k--)
                starts[k] = starts[k - 1];
            starts[0] = i;
            ns++;
        }
        if (!lg_last(i)) {
            const int d = (int)lg[i + 1].note - (int)lg[i].note;
            m->intervals++;
            steps += d <= 2 && d >= -2;
            if ((d >= 5 || d <= -5) && !lg_last(i + 1)) {
                const int e = (int)lg[i + 2].note - (int)lg[i + 1].note;
                m->leaps++;
                turns += d > 0 ? e < 0 : e > 0;
            }
        }
    }
    m->step_share = m->intervals ? (float)steps / (float)m->intervals : 0.0f;
    m->leap_turn = m->leaps ? (float)turns / (float)m->leaps : 0.0f;
    m->pair_recur = m->compared ? (float)recur / (float)m->compared : 0.0f;
}
/* the scale's chord whose root is pitch class croot (lg_pool on lg_root): its tones (count; 0: none), the pitch class
 * of its 9th (the degree above its root) and of its fifth (the tone nearest 7 semitones above the root) */
static int lg_chord(int croot, int *pcs, int *p9, int *p5)
{
    int st[12], m = en_scale(lg_pool, st), d, n, k, best = 99;
    for (d = 0; d < m; d++)
        if ((lg_root + st[d]) % 12 == croot) {
            n = en_chord_tones(lg_pool, lg_root, d, pcs);
            *p9 = (lg_root + st[(d + 1) % m]) % 12;
            *p5 = pcs[0];
            for (k = 1; k < n; k++) {
                const int iv = (pcs[k] - pcs[0] + 12) % 12, e = iv > 7 ? iv - 7 : 7 - iv;
                if (e < best)
                    best = e, *p5 = pcs[k];
            }
            return n;
        }
    return 0;
}
/* (the generator's tension note) from the log, counted in tn_held: a note held 6 sixteenths or more on a tone off
 * the chord (the chord at its phrase's start), not its phrase's last; in tn_unres those whose next note isn't 1 or 2
 * semitones under it, on a chord tone */
static uint32_t tn_held, tn_unres;
static void lg_tension(void)
{
    uint32_t i;
    int pcs[4], p9, p5, n = 0;
    for (i = 0; i < lg_n; i++) {
        int k, ct = 0, nt = 0, dn;
        if (lg_first(i))
            n = lg_chord(lg[i].croot, pcs, &p9, &p5);
        if (lg_last(i) || !n || lg[i].off == 0xFFFFFFFFu || lg[i].off - lg[i].on < 6u)
            continue;
        for (k = 0; k < n; k++)
            ct |= lg[i].note % 12 == pcs[k], nt |= lg[i + 1].note % 12 == pcs[k];
        if (ct)
            continue;
        tn_held++;
        dn = (int)lg[i].note - (int)lg[i + 1].note;
        if (dn < 1 || dn > 2 || !nt) {
            if (!tn_unres)
                printf("  held %u (%u sixteenths) then %u (chord root %d)\n", lg[i].note, lg[i].off - lg[i].on,
                       lg[i + 1].note, pcs[0]);
            tn_unres++;
        }
    }
}
/* from the log: the first entry of the first phrase with role `role` at or after entry i (lg_n: none) */
static uint32_t lg_find(uint32_t i, int role)
{
    for (; i < lg_n; i++)
        if (lg_first(i) && lg[i].role == role)
            return i;
    return lg_n;
}
/* played until the Lead begins a phrase of role `role` (at most secs; 0 if it didn't), a block at a time */
static int lg_wait(int role, float secs)
{
    const uint8_t p0 = en_lead_phr;
    int b;
    for (b = 0; b < (int)(secs * 44100.0f / 256.0f); b++) {
        run(256.0f / 44100.0f + 1e-6f);
        if (en_lead_phr != p0 && en_lead_role == role && lg_n && lg[lg_n - 1].phr == en_lead_phr)
            return 1;
    }
    return 0;
}
static void lm_print(const char *what, const lm_t *m)
{
    printf("  %s: %u notes, %u phrases; steps (<= 2 semitones) %.1f%% of %u intervals, a turn after a leap %.1f%% of %u, "
           "recurring %.1f%% of %u\n", what, m->notes, m->phrases, (double)(100.0f * m->step_share), m->intervals,
           (double)(100.0f * m->leap_turn), m->leaps, (double)(100.0f * m->pair_recur), m->compared);
}

int main(void)
{
    int pcs[4], n, i;
    char name[16];

    /* Euclid: the classic patterns */
    CHECK(en_euclid(3, 8, 0) == pat("x..x..x."), "E(3,8) %x", en_euclid(3, 8, 0));
    CHECK(en_euclid(5, 8, 0) == pat("x.xx.xx."), "E(5,8) %x", en_euclid(5, 8, 0));
    CHECK(en_euclid(4, 16, 0) == pat("x...x...x...x..."), "E(4,16)");
    CHECK(en_euclid(0, 16, 0) == 0, "E(0,16)");
    CHECK(en_euclid(7, 7, 0) == 0x7Fu, "E(7,7)");
    for (i = 0; i <= 12; i++)
        CHECK(popcount(en_euclid(i, 12, 5)) == i, "E(%d,12) has %d hits", i, popcount(en_euclid(i, 12, 5)));
    CHECK(en_euclid(1, 7, 3) == 1u << 3, "rotation");

    /* scales and chords */
    {
        int st[12];
        CHECK(en_scale(EN_MODE_POOL[MODE_DORIAN], st) == 7 && st[2] == 3 && st[6] == 10, "Dorian steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_PENT_MIN], st) == 5, "pentatonic minor has 5");
        CHECK(en_scale(0, st) == 1, "the root is always in");
    }
    n = en_chord_tones(EN_MODE_POOL[MODE_IONIAN], 0, 0, pcs);
    CHECK(n == 4 && pcs[0] == 0 && pcs[1] == 4 && pcs[2] == 7 && pcs[3] == 11, "C Ionian I = Cmaj7");
    CHECK(!strcmp(en_chord_name(pcs, n, name), "CM7"), "name %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 0, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "Dm7"), "D Dorian i: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 3, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "G7"), "D Dorian IV: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_IONIAN], 0, 6, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "Bm7b5"), "C Ionian vii: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_PENT_MAJ], 0, 0, pcs);
    CHECK(n == 3, "pentatonic chords have three tones (%d)", n);
    n = en_chord_tones(1u | 1u << 4 | 1u << 7 | 1u << 9, 0, 0, pcs);   /* four notes: no doubled tones */
    CHECK(n == 3 && pcs[0] != pcs[1] && pcs[1] != pcs[2] && pcs[0] != pcs[2], "4-note pool: distinct");
    n = en_chord_tones(1u | 1u << 4 | 1u << 7, 0, 1, pcs);
    CHECK(n == 3, "3-note pool: all three (%d)", n);

    /* voicings: four rising notes from the root, within two octaves, no pitch class twice, in the pool */
    {
        int iv[4], m, d, s, st[12];
        en_voicing(EN_MODE_POOL[MODE_DORIAN], 0, 0, iv);
        CHECK(iv[0] == 0 && iv[1] == 7 && iv[2] == 10 && iv[3] == 15, "Dorian i open: %d %d %d %d", iv[0], iv[1],
              iv[2], iv[3]);
        en_voicing(EN_MODE_POOL[MODE_DORIAN], 0, 1, iv);
        CHECK(iv[0] == 0 && iv[1] == 10 && iv[2] == 14 && iv[3] == 15, "Dorian i 9th: %d %d %d %d",
              iv[0], iv[1], iv[2], iv[3]);
        for (m = 0; m < EN_NMODES; m++) {
            int ns = en_scale(EN_MODE_POOL[m], st);
            for (d = 0; d < ns; d++)
                for (s = 0; s < 2; s++) {
                    int k, j, ok = 1;
                    en_voicing(EN_MODE_POOL[m], d, s, iv);
                    ok &= iv[0] == 0 && iv[3] <= 26;
                    for (k = 1; k < 4; k++)
                        ok &= iv[k] > iv[k - 1];
                    for (k = 0; k < 4; k++) {
                        for (j = 0; j < k; j++)
                            ok &= iv[k] % 12 != iv[j] % 12;
                        ok &= EN_MODE_POOL[m] >> ((st[d] + iv[k]) % 12) & 1u;
                    }
                    CHECK(ok, "%s degree %d shape %d: %d %d %d %d", EN_MODE_NAME[m], d, s, iv[0], iv[1], iv[2], iv[3]);
                }
        }
    }

    /* drift */
    CHECK(en_drift_bars(0) == 2 && en_drift_bars(5) == 64 && en_drift_bars(6) == 0, "drift bars");

    /* texts */
    {
        char t[12];
        en_param_text(P_DRIFT, 6, t);
        CHECK(!strcmp(t, "Hold"), "drift 6: %s", t);
        en_param_text(P_DRIFT, 4, t);
        CHECK(!strcmp(t, "32 bars"), "drift 4: %s", t);
        en_param_text(P_DRIFT, 0, t);
        CHECK(!strcmp(t, "2 bars"), "drift 0: %s", t);
        en_param_text(P_ETIME, 1, t);
        CHECK(!strcmp(t, "1/8."), "etime 1: %s", t);
        en_param_text(P_ETIME, 5, t);
        CHECK(!strcmp(t, "1 bar"), "etime 5: %s", t);
        en_param_text(P_CVOICE, 0, t);
        CHECK(!strcmp(t, "Organ"), "ctone 0: %s", t);
        en_param_text(P_CVOICE, 1, t);
        CHECK(!strcmp(t, "Strings"), "ctone 1: %s", t);
        en_param_text(P_TAPAT, 1, t);
        CHECK(!strcmp(t, "1/8."), "repeat A time 1: %s", t);
        en_param_text(P_TAPBT, 2, t);
        CHECK(!strcmp(t, "1/4"), "repeat B time 2: %s", t);
        en_param_text(P_TAPAT, 5, t);
        CHECK(!strcmp(t, "1 bar"), "repeat A time 5: %s", t);
        en_param_text(P_SEQGRID, 0, t);
        CHECK(!strcmp(t, "8ths"), "grid 0: %s", t);
        en_param_text(P_TRANS, 2, t);
        CHECK(!strcmp(t, "Bed"), "transpose 2: %s", t);
        en_param_text(P_SVOICE, 0, t);
        CHECK(!strcmp(t, "2600"), "seq voice 0: %s", t);
        en_param_text(P_CATT, 40, t);
        CHECK(!strcmp(t, "40"), "attack 40, unsigned: %s", t);
        en_param_text(P_LOCT, 1, t);
        CHECK(!strcmp(t, "+1") && en_param_info(P_LOCT)->lo == -2 && en_param_info(P_LOCT)->hi == 1, "lead oct: %s", t);
        en_param_text(P_LOCT, -2, t);
        CHECK(!strcmp(t, "-2"), "lead oct -2: %s", t);
        en_param_text(P_SEQ2OCT, 1, t);
        CHECK(!strcmp(t, "+1"), "seq2 oct: %s", t);
        en_param_text(P_MODE, MODE_DORIAN, t);
        CHECK(!strcmp(t, "Dorian"), "mode: %s", t);
        for (i = 0; i < P_NPARAMS; i++) {
            const en_param_t *pi = en_param_info(i);
            CHECK(pi->lo <= pi->def && pi->def <= pi->hi, "%s: default %d out of %d..%d", pi->name, pi->def, pi->lo,
                  pi->hi);
            en_param_text(i, pi->hi, t);
            CHECK(strlen(t) < 10, "%s: text too long", pi->name);
        }
    }

    /* (BERLIN, spec 3) the filter motion at rest: Width, Rate, Intro and Bridge 0 (and Motion 0, Arc 0, Evolve 0),
     * Timewind's first minute is to the bit what the engine without the filter motion played with Sweep 0, Emerge 0,
     * Arc 0, Motion 0, Evolve 0 (build/tmp/fmzero_old.c on fd1d378: the first render of a process, as this one;
     * af050088 then, re-recorded for the echo's x2 send and 1/4. time, which alone moved it: checked by reverting them;
     * 1d4f1422 then, re-recorded for Timewind's own recipe: Sequence 2 at 60, Reverb 50, the user's; f09ec04f then,
     * re-recorded for Timewind's Length 2 at 15, the user's 16/15, the recipe alone) */
    {   /* (in a child: this process stays as it started, for the render's tests below) */
        int k, fd[2];
        pid_t pid;
        struct {
            uint32_t h;
            float fo[5];
        } r;
        memset(&r, 0, sizeof r);
        fflush(stdout);
        if (pipe(fd) == 0 && (pid = fork()) >= 0) {
            if (!pid) {
                close(fd[0]);
                en_init();
                piece(0);
                fo_all(0, -1, 0, 0);
                en_set(P_BRIDGE, 0);
                en_set(P_ARC, 0);
                en_set(P_MOTION, 0);
                en_set(P_EVOLVE, 0);
                en_play(1);
                r.h = run_hash(60.0f);
                for (k = 0; k < 5; k++)
                    r.fo[k] = en_fo[k];
                if (write(fd[1], &r, sizeof r) != (ssize_t)sizeof r)
                    _exit(1);
                _exit(0);
            }
            close(fd[1]);
            if (read(fd[0], &r, sizeof r) != (ssize_t)sizeof r)
                r.h = 0;
            close(fd[0]);
            waitpid(pid, 0, 0);
        }
        CHECK(r.h == 0x1cd60c54u, "test_fm_zero: Timewind's first minute %08x, not 1cd60c54 (the engine before)", r.h);
        for (k = 0; k < 5; k++)
            CHECK(r.fo[k] == 0.0f, "test_fm_zero: layer %d's offset %f", k, (double)r.fo[k]);
    }

    /* the render: silent at rest, sound when playing, never NaN, bounded */
    en_init();
    en_set(P_CRACKLE, 0);
    run(1.0f);
    CHECK(peak == 0.0f, "silent before play (%f)", (double)peak);
    en_set(P_CRACKLE, en_param_info(P_CRACKLE)->def);
    en_play(1);
    peak = sum2 = 0.0f;
    run(20.0f);
    CHECK(!bad, "NaN in the output");
    CHECK(peak > 0.1f && peak < 0.95f, "peak %f", (double)peak);
    printf("  20 s playing: peak %.3f, rms %.4f, bars %u, kicks %u\n", (double)peak,
           (double)sqrtf(sum2 / (20.0f * 44100.0f * 2.0f)), en_bars / 16u, kicks);
    CHECK(en_bars / 16u >= 7u, "the grid ran (%u sixteenths: 8 bars in 20 s at 96)", en_bars);

    /* the chord drifts: at 2 bars, it moves every 4 s or so; never onto a diminished fifth; in the pool */
    {
        static const int MODES[2] = {MODE_IONIAN, MODE_DORIAN};
        int mi;
        for (mi = 0; mi < 2; mi++) {
            uint32_t pl = EN_MODE_POOL[MODES[mi]], last;
            int changes = 0, dim = 0, out = 0, k, seen = 0;
            fresh();
            en_set(P_ROOT, 0);
            en_pool(pl);
            en_set(P_DRIFT, 0);
            en_play(1);
            run(0.1f);
            last = chord_sig();
            for (k = 0; k < 4 * 180; k++) {
                int t[4], nt;
                uint32_t s;
                run(0.25f);
                s = chord_sig();
                changes += s != last;
                last = s;
                nt = chord_now(pl, 0, t, name);
                CHECK(nt, "%s: chord root %d off the scale", EN_MODE_NAME[MODES[mi]], en_chord_root);
                if (nt) {
                    seen |= 1 << en_chord_root;
                    dim += strstr(name, "b5") || strstr(name, "dim");
                }
                for (i = 0; i < EN_NPAD; i++) {
                    out += !(pl >> (en_chord[i] % 12) & 1u);
                    dim += en_chord[i] % 12 == (en_chord_root + 6) % 12;
                }
            }
            CHECK(changes >= 30, "%s: %d chord changes in 3 min at 2 bars", EN_MODE_NAME[MODES[mi]], changes);
            CHECK(popcount((uint32_t)seen) >= 3, "%s: only %d roots", EN_MODE_NAME[MODES[mi]], popcount((uint32_t)seen));
            CHECK(!dim, "%s: %d times on a diminished fifth", EN_MODE_NAME[MODES[mi]], dim);
            CHECK(!out, "%s: %d chord notes off the pool", EN_MODE_NAME[MODES[mi]], out);
            printf("  %s, 3 min at 2 bars: %d chord changes, %d roots\n", EN_MODE_NAME[MODES[mi]], changes,
                   popcount((uint32_t)seen));
        }
    }

    /* Drift Hold: one chord */
    {
        uint32_t held;
        int k, moved = 0;
        fresh();
        en_set(P_DRIFT, 6);
        en_play(1);
        run(0.1f);
        held = chord_sig();
        for (k = 0; k < 120; k++) {
            run(1.0f);
            moved += chord_sig() != held;
        }
        CHECK(!moved && en_drift == 0.0f, "Hold: the chord moved (%d s)", moved);
        en_next();                               /* but Next still moves it, on the bar */
        run(2.5f);
        CHECK(chord_sig() != held, "Next while holding: no change");
    }

    /* freeze (dub hold): the chord holds, though Drift is at its shortest; then moves again */
    {
        uint32_t held;
        int k, moved = 0;
        en_set(P_DRIFT, 0);
        run(5.0f);
        en_freeze(1);
        run(0.1f);
        CHECK(en_frozen, "not frozen");
        held = chord_sig();
        for (k = 0; k < 60; k++) {
            run(1.0f);
            moved += chord_sig() != held;
        }
        CHECK(!moved, "the chord moved while frozen (%d s)", moved);
        en_freeze(0);
        for (k = 0, moved = 0; k < 10; k++) {   /* (it may move and come home again: any move counts) */
            run(1.0f);
            moved += chord_sig() != held;
        }
        CHECK(moved, "the chord stayed after the freeze");
    }

    /* a piece's parameters all at once (a piece change, the boot): none of them, nor PLAY after them, dropped */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        int k;
        fresh();
        en_piece(3, pp, &pl);
        for (k = 0; k < P_NPARAMS; k++)
            en_set(k, pp[k]);
        en_pool(pl);
        en_regrow();
        en_play(1);
        run(3.0f);
        CHECK(en_playing && en_bars > 16u && peak > 0.02f, "a piece's parameters, then PLAY: not playing (%u)", en_bars);
    }

    /* the safety valve (the classics pass): under load (en_load over 75, the FM-1's meter) the engine sheds: no new
     * noise hits, the width and the rumble eased off, tails shorter; the music goes on, nothing broken, and back */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        int k;
        uint32_t w0, w1, w2;
        float p_shed;
        fresh();
        en_piece(6, pp, &pl);                    /* (Mirage: Space, the phaser, the long reverb: a wide load) */
        for (k = 0; k < P_NPARAMS; k++)
            en_set(k, pp[k]);
        en_pool(pl);
        en_regrow();
        en_play(1);
        run(10.0f);
        w0 = en_nev_w;
        en_load = 95;
        peak = 0.0f, bad = 0;
        run(10.0f);
        p_shed = peak;
        w1 = en_nev_w;
        en_load = 40;
        run(10.0f);
        w2 = en_nev_w;
        printf("  the valve: noise events %u in 10 s shedding, %u after; peak %.3f shedding\n", w1 - w0, w2 - w1, (double)p_shed);
        CHECK(bad == 0 && p_shed > 0.02f && w2 - w1 > w1 - w0, "the valve: bad %d, peak %.3f, events %u then %u",
              bad, (double)p_shed, w1 - w0, w2 - w1);
    }

    /* the chords' slots, Loop: Fm7, Bb, (a gap: Bb holds), C6, played in turn a change every two bars, round again
     * (the rest gaps: the loop is four long); the slot playing edited, heard at once; to Endless: the one playing up
     * to slot 1; the labels */
    {
        static const int WANT[6] = {5, 10, 0, 5, 10, 0};
        int k, seen[16], ns = 0, ok = 1, last = -1, holds = 0;
        char nm[16];
        fresh();
        en_set(P_ROOT, 5);
        en_pool(EN_MODE_POOL[MODE_DORIAN]);
        en_set(P_DRIFT, 0);
        en_set(P_CR0, 5), en_set(P_CQ0, EN_CQ_MINOR), en_set(P_CX0, EN_CX_7);
        en_set(P_CR1, 10), en_set(P_CQ1, EN_CQ_MAJOR);
        en_set(P_CR2, 0), en_set(P_CQ2, EN_CQ_EMPTY), en_set(P_CX2, 0);
        en_set(P_CR3, 0), en_set(P_CQ3, EN_CQ_MAJOR), en_set(P_CX3, EN_CX_6);
        for (k = 4; k < EN_NPROG; k++)
            en_set(P_CR0 + k, 0), en_set(P_CQ0 + k, EN_CQ_EMPTY), en_set(P_CX0 + k, 0);
        en_set(P_PMODE, EN_PLOOP);               /* (Loop) */
        en_regrow();
        en_play(1);
        for (k = 0; k < 900 && ns < 16; k++) {
            run(0.1f);
            if (en_chord_root != last) {
                last = en_chord_root;
                seen[ns++] = last;
            }
            holds += en_prog_pos == 2 && en_chord_root == 10;
        }
        for (k = 0; k < 6; k++)
            ok &= seen[k] == WANT[k];
        CHECK(ns >= 6 && ok, "the slots' roots: %d %d %d %d %d %d (%d changes)", seen[0], seen[1], seen[2], seen[3], seen[4],
              seen[5], ns);
        CHECK(holds > 0, "the empty slot didn't hold the chord before it");
        while (en_chord_slot != 0)
            run(0.1f);
        en_set(P_CR0, 7);                        /* the slot playing, edited: G minor 7 now */
        run(0.05f);
        CHECK(en_chord_root == 7, "the slot playing, edited: root %d", en_chord_root);
        en_set(P_PMODE, 1);                      /* to Endless (Wander, by hand: turned round): slot 1 plays, the gaps */
        en_prog_rotate();                        /* stay, the walk goes on */
        run(0.05f);
        CHECK(en_prog_pos == 0 && en_prog_slot[0][1] == EN_CQ_MINOR && en_prog_slot[0][0] == 7,
              "to Endless: slot 1 %d/%d/%d at %d", en_prog_slot[0][0], en_prog_slot[0][1], en_prog_slot[0][2], en_prog_pos);
        CHECK(en_prog_slot[2][1] == 0 && en_prog_slot[2][0] == 0, "to Endless: the gap filled");
        CHECK(!strcmp(en_chord_label(5, EN_CQ_MINOR, EN_CX_7, nm), "Fm7"), "label %s", nm);
        CHECK(!strcmp(en_chord_label(0, EN_CQ_SUS4, EN_CX_7, nm), "C7sus4"), "label %s", nm);
        CHECK(!strcmp(en_chord_label(10, EN_CQ_MAJOR, EN_CX_MAJ7, nm), "BbM7"), "label %s", nm);
        CHECK(!strcmp(en_chord_label(3, EN_CQ_EMPTY, 0, nm), "-"), "label %s", nm);
        {   /* Endless from a fresh piece: eight from the walk; on a change, all up one, the walk writes slot 8; a gap
             * stays a gap; the walk's follow the key */
            int j, before[EN_NPROG][3], walk = 1, up = 1;
            fresh();
            en_set(P_ROOT, 5);
            en_pool(EN_MODE_POOL[MODE_DORIAN]);
            en_set(P_DRIFT, 0);
            en_set(P_PMODE, 0);
            for (j = 0; j < EN_NPROG; j++)
                en_set(P_CR0 + j, 0), en_set(P_CQ0 + j, EN_CQ_EMPTY), en_set(P_CX0 + j, 0);
            en_regrow();
            run(0.05f);
            for (j = 0; j < EN_NPROG; j++)
                walk &= en_prog_slot[j][1] == 0 && en_prog_slot[j][0] >= 1 && en_prog_slot[j][0] <= 7;
            CHECK(walk && en_prog_slot[0][0] == 1, "Endless: a fresh piece's eight from the walk, from home (%d)", en_prog_slot[0][0]);
            en_set(P_CR3, 0), en_set(P_CQ3, EN_CQ_EMPTY);   /* a gap at slot 4 */
            en_set(P_CR5, 0), en_set(P_CQ5, EN_CQ_MAJOR), en_set(P_CX5, EN_CX_7);   /* a C7 written at slot 6 */
            run(0.05f);
            for (j = 0; j < EN_NPROG; j++)
                for (k = 0; k < 3; k++)
                    before[j][k] = en_prog_slot[j][k];
            en_play(1);
            {
                uint32_t g = en_prog_gen;
                for (k = 0; k < 200 && en_prog_gen == g; k++)
                    run(0.05f);
            }
            for (j = 0; j < EN_NPROG - 1; j++)
                for (k = 0; k < 3; k++)
                    up &= en_prog_slot[j][k] == before[j + 1][k];
            CHECK(up, "Endless: the slots didn't move up one");
            CHECK(en_prog_slot[2][0] == 0 && en_prog_slot[2][1] == 0, "Endless: the gap didn't stay a gap");
            CHECK(en_prog_slot[4][1] == EN_CQ_MAJOR && en_prog_slot[4][2] == EN_CX_7, "Endless: the written C7 moved");
            CHECK(en_prog_slot[7][1] == 0 && en_prog_slot[7][0] >= 1, "Endless: the walk didn't write slot 8");
            CHECK(en_prog_pos == 0, "Endless: slot %d plays", en_prog_pos);
            {   /* the walk's character turned by hand: its slots walked again (not the one playing); yours and the
                 * gaps kept */
                int diff = 0, keep = 1, p0[3];
                for (j = 0; j < 3; j++)
                    p0[j] = en_prog_slot[0][j];
                for (j = 0; j < EN_NPROG; j++)
                    for (k = 0; k < 3; k++)
                        before[j][k] = en_prog_slot[j][k];
                for (k = 0; k < 4 && !diff; k++) {   /* (a few tries: the walk may write the same by chance) */
                    en_set(P_PMODE, EN_WALK_FREE);
                    en_prog_rewalk();
                    run(0.02f);
                    for (j = 1; j < EN_NPROG; j++)
                        diff += before[j][1] == 0 && before[j][0] && en_prog_slot[j][0] != before[j][0];
                }
                for (j = 0; j < EN_NPROG; j++)
                    if (before[j][1] || !before[j][0])
                        keep &= en_prog_slot[j][0] == before[j][0] && en_prog_slot[j][1] == before[j][1];
                CHECK(diff > 0, "re-walk: the walk's slots didn't change");
                CHECK(keep, "re-walk: a written slot or a gap changed");
                CHECK(en_prog_slot[0][0] == p0[0] && en_prog_slot[0][1] == p0[1], "re-walk: the slot playing changed");
            }
        }
        {   /* the walk's character: Home, Wander, Free, each more varied (fewer A-B-A bounces, home less often) */
            int st, pp[3] = {0}, hm[3] = {0};
            for (st = 0; st < 3; st++) {
                int r[64], nr = 0, j, last = -1;
                fresh();
                en_set(P_ROOT, 5);
                en_pool(EN_MODE_POOL[MODE_DORIAN]);
                en_set(P_DRIFT, 0);
                en_set(P_PMODE, st);
                for (j = 0; j < EN_NPROG; j++)
                    en_set(P_CR0 + j, 0), en_set(P_CQ0 + j, EN_CQ_EMPTY), en_set(P_CX0 + j, 0);
                en_regrow();
                en_play(1);
                for (j = 0; j < 6000 && nr < 64; j++) {
                    run(0.05f);
                    if (en_chord_root != last || j == 0)
                        last = en_chord_root, r[nr++] = last;
                }
                for (j = 2; j < nr; j++)
                    pp[st] += r[j] == r[j - 2];
                for (j = 0; j < nr; j++)
                    hm[st] += r[j] == 5;
                printf("  the walk, %s: %d changes, %d bounces (A-B-A), %d at home\n", st == 0 ? "Home" : st == 1 ? "Wander" : "Free",
                       nr, pp[st], hm[st]);
            }
            CHECK(pp[1] < pp[0] && pp[2] < pp[0], "the walk: Wander (%d) and Free (%d) bounce less than Home (%d)", pp[1], pp[2], pp[0]);
            CHECK(hm[2] < hm[0], "the walk: Free (%d) at home less than Home (%d)", hm[2], hm[0]);
        }
        for (k = 0; k < EN_NCQ * EN_NCX; k++) {   /* every voicing: four rising notes, within two octaves */
            int iv[4];
            en_chord_iv(k / EN_NCX, k % EN_NCX, iv);
            ok = iv[0] == 0 && iv[1] > iv[0] && iv[2] > iv[1] && iv[3] > iv[2] && iv[3] <= 24;
            CHECK(ok, "voicing %d/%d: %d %d %d %d", k / EN_NCX, k % EN_NCX, iv[0], iv[1], iv[2], iv[3]);
        }
    }

    /* the effects grid: each effect alone, then all sixteen at once, then let go: no NaN, under full scale,
     * and back to the music after */
    {
        int e;
        for (e = 0; e < EN_NFX; e++) {
            fresh();
            en_play(1);
            run(2.0f);
            en_fx(1u << e);
            run(3.0f);
            CHECK(!bad && peak < 0.99f, "effect %s: peak %f", EN_FX_NAME[e], (double)peak);
            en_fx(0);
            run(2.0f);
            CHECK(!bad && peak > 0.02f, "after %s: silent (%f)", EN_FX_NAME[e], (double)peak);
        }
        fresh();
        en_play(1);
        run(2.0f);
        en_fx(0xFFFFu);
        run(6.0f);
        CHECK(!bad && peak < 0.995f, "all sixteen effects: peak %f", (double)peak);
        en_fx(0);
        run(4.0f);
        CHECK(!bad && peak > 0.02f && peak < 0.99f, "after all effects: %f", (double)peak);
    }

    /* (BERLIN) the grid's sixteen, in its order */
    {
        static const char *const FX[EN_NFX] = {"Repeat", "Time", "Tape stop", "Swell", "Low-pass", "High-pass", "Sweep",
                                               "Phaser", "Reverb", "Take-off", "Freeze", "Wash", "Octave", "Return",
                                               "Twitter", "Supernova"};
        int e;
        for (e = 0; e < EN_NFX; e++)
            CHECK(!strcmp(EN_FX_NAME[e], FX[e]), "test_fx_names: effect %d is %s, not %s", e, EN_FX_NAME[e], FX[e]);
    }

    /* (BERLIN) Octave: a written pattern of eight, two passes as written, then two with Octave held: every step's note
     * twelve over its own; let go: as written again. Return: held, only the first four steps' notes */
    {
        static const int8_t D[8] = {0, 2, 4, 1, 3, 5, 7, 6};
        int want[8], i, k, ok = 1, n0, oct_ok = 1, ret_ok = 1, back_ok = 1;
        uint32_t n_ret;
        fresh();
        en_set(P_TEMPO, 120);
        en_set(P_SEQ2, 0);
        en_set(P_TAPA, 0);
        en_set(P_TAPB, 0);
        en_set(P_DENSITY, 100);
        seq_write(8, 0xFFu, D);
        en_play(1);
        run(4.0f);                               /* (two bars at 120: two passes of 16 sixteenths in 8ths) */
        n0 = (int)sq_n;
        for (i = 0; i < 8 && n0 >= 8; i++)
            want[i] = sq_note[n0 - 8 + i];
        CHECK(n0 >= 8, "test_fx_octave: %d Sequence notes in 4 s", n0);
        for (i = 0; i < 8 && n0 >= 16; i++)
            ok &= sq_note[n0 - 16 + i] == want[i];   /* (the passes alike: the test can trust the pattern) */
        CHECK(ok, "test_fx_octave: the written pattern's passes differ");
        en_fx(1u << EN_FX_OCTAVE);
        run(4.0f);
        for (k = n0 + 1; k < (int)sq_n; k++)     /* (the first note may be the one already on its way) */
            oct_ok &= sq_note[k] == want[(k - n0) % 8] + 12;
        CHECK(oct_ok && (int)sq_n - n0 >= 8, "test_fx_octave: held, not an octave over the pattern (%d notes)",
              (int)sq_n - n0);
        en_fx(0);
        run(0.5f);
        n0 = (int)sq_n;
        run(4.0f);
        for (k = n0; k < (int)sq_n; k++) {
            int f = 0;
            for (i = 0; i < 8; i++)
                f |= sq_note[k] == want[i];
            back_ok &= f;
        }
        CHECK(back_ok, "test_fx_octave: let go, not back to the pattern");
        en_fx(1u << EN_FX_RETURN);
        run(0.5f);
        n0 = (int)sq_n;
        run(4.0f);
        for (k = n0; k < (int)sq_n; k++) {
            int f = 0;
            for (i = 0; i < 4; i++)
                f |= sq_note[k] == want[i];
            ret_ok &= f;
        }
        n_ret = sq_n - (uint32_t)n0;
        CHECK(ret_ok && n_ret >= 8u, "test_fx_return: held, a note outside the first four steps (%u notes)", n_ret);
        en_fx(0);
    }

    /* (BERLIN) Sweep moves the Sequence's filter (the Drone alone: the same to the bit; the Sequence alone: not);
     * Phaser on everything (the Sequence alone: not the same) */
    {
        static const int OFF[5] = {P_CHORD, P_SEQ, P_LEAD, P_SEQ2, P_NCHAR};
        int e, layer;
        static const int FXS[2] = {EN_FX_SWEEP, EN_FX_PHASER};
        for (e = 0; e < 2; e++)
            for (layer = 0; layer < 2; layer++) {   /* 0: the Drone alone; 1: the Sequence alone */
                uint32_t h[3];
                int k;
                fresh();
                for (k = 0; k < 5; k++)
                    en_set(OFF[k], 0);
                en_set(layer ? P_SEQ : P_CHORD, 70);
                en_play(1);
                run(2.0f);
                h[0] = hash_child(0, 4.0f);
                h[1] = hash_child(1u << FXS[e], 4.0f);
                h[2] = hash_child(0, 4.0f);      /* (the control: not held, again) */
                CHECK(h[0] && h[0] == h[2], "test_fx_%s: two runs from one state differ (the control)",
                      e ? "phaser" : "sweep");
                if (FXS[e] == EN_FX_SWEEP && !layer)
                    CHECK(h[0] == h[1], "test_fx_sweep: the Drone alone changed by Sweep");
                else if (layer)
                    CHECK(h[0] != h[1], "test_fx_%s: the Sequence alone unchanged", e ? "phaser" : "sweep");
            }
    }

    /* (BERLIN) from silence (every layer at 0): Take-off (tapped) and Twitter (held) are heard; Supernova held, the
     * echo's tail grows (the music stopped by its levels, the echo left ringing: louder than without) */
    {
        static const int OFF[5] = {P_CHORD, P_SEQ, P_LEAD, P_SEQ2, P_NCHAR};
        static const int FXS[2] = {EN_FX_TAKEOFF, EN_FX_TWITTER};
        float tail[2];
        int e, k, held;
        for (e = 0; e < 2; e++) {
            float p0;
            fresh();
            for (k = 0; k < 5; k++)
                en_set(OFF[k], 0);
            en_play(1);
            run(2.0f);                           /* (the start's tail gone: the echo's default 1/4. is long) */
            peak = 0.0f;
            run(1.0f);
            p0 = peak;
            peak = 0.0f;
            en_fx(1u << FXS[e]);
            run(e ? 2.0f : 0.1f);
            en_fx(0);
            run(e ? 0.1f : 2.0f);
            CHECK(p0 < 1e-4f && peak > 0.01f, "test_fx_%s: from silence %.5f, then %.4f", e ? "twitter" : "takeoff",
                  (double)p0, (double)peak);
        }
        for (held = 0; held < 2; held++) {
            fresh();
            en_set(P_ECHO, 80);
            en_play(1);
            run(10.0f);
            for (k = 0; k < 5; k++)
                en_set(OFF[k], 0);
            if (held)
                en_fx(1u << EN_FX_SUPERNOVA);
            run(2.0f);
            sum2 = 0.0f;
            run(2.0f);
            tail[held] = sqrtf(sum2 / (2.0f * 44100.0f * 2.0f));
            en_fx(0);
        }
        printf("  the echo's tail, 2-4 s after the music stops: %.4f, Supernova held %.4f\n", (double)tail[0],
               (double)tail[1]);
        CHECK(tail[1] > 2.0f * tail[0], "test_fx_supernova: the tail %.4f held, %.4f not", (double)tail[1],
              (double)tail[0]);
        fresh();                                 /* (nothing in the echo: Supernova has nothing to grow, and adds none) */
        for (k = 0; k < 5; k++)
            en_set(OFF[k], 0);
        en_set(P_ECHO, 0);
        en_play(1);
        run(1.0f);
        peak = 0.0f;
        en_fx(1u << EN_FX_SUPERNOVA);
        run(3.0f);
        en_fx(0);
        CHECK(peak < 1e-4f, "test_fx_supernova: from silence, held: %.4f", (double)peak);
    }

    /* (BERLIN) the black keys' Transpose: a written pattern played with Transpose held is the same pattern written three
     * scale steps up */
    {
        static const int8_t A[8] = {0, 2, 4, 1, 3, 5, 7, 6};
        int8_t B[8];
        int i, k, nt[2], notes[2][8], ok = 1;
        for (i = 0; i < 8; i++)
            B[i] = (int8_t)(A[i] + 3);
        for (k = 0; k < 2; k++) {
            fresh();
            en_set(P_TEMPO, 120);
            en_set(P_SEQ2, 0);
            en_set(P_TAPA, 0);
            en_set(P_TAPB, 0);
            en_set(P_DENSITY, 100);
            seq_write(8, 0xFFu, k ? A : B);
            if (k)
                en_move(EN_MV_TRANSPOSE);
            en_play(1);
            run(4.5f);
            nt[k] = (int)sq_n;
            for (i = 0; i < 8 && (int)sq_n >= 16; i++)
                notes[k][i] = sq_note[sq_n - 8 + i];
        }
        for (i = 0; i < 8; i++)
            ok &= notes[0][i] == notes[1][i];
        if (!ok)
            for (i = 0; i < 8; i++)
                printf("  transpose: step %d: written up %d, held %d\n", i, notes[0][i], notes[1][i]);
        CHECK(ok && nt[0] >= 16 && nt[1] >= 16, "test_move_transpose: held, not the pattern three steps up");
        en_move(0);
    }

    /* (BERLIN) everything at once: every layer's Width and Rate 100, Bridge 100, Motion 100, Rise and Close and all
     * sixteen effects held, 160 BPM, a minute: nothing broken, the offsets bounded; let go: back under -0.5 dBFS */
    {
        static const int W[5] = {P_MW0, P_MW1, P_MW2, P_MW3, P_MW4}, R[5] = {P_MR0, P_MR1, P_MR2, P_MR3, P_MR4};
        int k, okf = 1;
        fresh();
        piece(4);
        en_set(P_TEMPO, 160);
        en_set(P_MOTION, 100);
        en_set(P_BRIDGE, 100);
        for (k = 0; k < 5; k++)
            en_set(W[k], 100), en_set(R[k], 100);
        en_play(1);
        en_fx(0xFFFFu);
        en_move(EN_MV_RISE | EN_MV_CLOSE | EN_MV_TRANSPOSE);
        bad = 0;
        run(60.0f);
        for (k = 0; k < 5; k++)
            okf &= en_fo[k] == en_fo[k] && fabsf(en_fo[k]) < 12.0f;
        en_fx(0);
        en_move(0);
        run(10.0f);
        peak = 0.0f;
        run(5.0f);
        CHECK(!bad && okf && peak < 0.944f, "test_fm_extremes: bad %d, offsets ok %d, peak after %.3f", bad, okf,
              (double)peak);
    }

    /* (BERLIN) PERFORM finished: the Lead muted stops its note (and a key held while muted plays nothing); the Drone
     * muted sends no MIDI (its notes let go at the mute); Energy's Repeat B works; Wet moves every layer's echo alike */
    {
        uint32_t ons;
        int k, held = 0;
        fresh();
        en_set(P_MOTION, 0);
        en_set(P_LDIN, 0);
        en_play(1);
        run(1.0f);
        en_lead_key(3, 1);
        run(0.3f);
        CHECK(ld_on_now, "test_lead_mute_stops: the key's note not on");
        en_mute(EN_M_BASS);
        run(0.05f);
        CHECK(!ld_on_now, "test_lead_mute_stops: still sounding 50 ms after the mute");
        en_lead_key(3, 0);
        ons = ld_ons;
        en_lead_key(5, 1);
        run(1.0f);
        CHECK(ld_ons == ons, "test_lead_mute_stops: a key played the muted Lead");
        en_lead_key(5, 0);
        en_mute(0);

        fresh();
        en_set(P_DRIFT, 0);                      /* (a chord every two bars) */
        en_play(1);
        run(3.0f);
        en_mute(EN_M_CHORD);
        run(0.05f);
        for (k = 0; k < 128; k++)
            held += sounding[0][k] > 0;
        CHECK(held == 0, "test_drone_mute_midi: %d channel-1 notes still on at the mute", held);
        dr_n = 0;
        run(120.0f);
        CHECK(dr_n == 0, "test_drone_mute_midi: %u channel-1 notes while muted", dr_n);
        en_mute(0);

        /* (the final review) its level to 0 lets its notes go as the mute does; back up, one chord at a time (no
         * notes left held under the new ones) */
        fresh();
        en_set(P_DRIFT, 0);
        en_play(1);
        run(3.0f);
        en_set(P_CHORD, 0);
        run(20.0f);
        for (held = 0, k = 0; k < 128; k++)
            held += sounding[0][k] > 0;
        CHECK(held == 0, "test_drone_level0_midi: %d channel-1 notes still on at level 0", held);
        en_set(P_CHORD, 60);
        run(30.0f);
        for (held = 0, k = 0; k < 128; k++)
            held += sounding[0][k] > 0;
        CHECK(held <= 5, "test_drone_level0_midi: %d channel-1 notes on after level back (one chord: 5)", held);

        fresh();
        en_set(P_TAPA, 0);
        en_set(P_TAPB, 0);
        en_set(P_DENSITY, 0);                    /* (Energy fills the steps too: from 0, rests stay for Repeat B) */
        en_play(1);
        run(1.0f);
        en_macro(EN_MAC_ENERGY, 50);
        k = (int)en_seq_reps[0];
        run(10.0f);
        CHECK((int)en_seq_reps[0] > k, "test_energy_repeat_b: Energy +50, no Repeat B note");
        en_macro(EN_MAC_ENERGY, 0);
    }
    {   /* Wet: the Sequence alone and Sequence 2 alone, the echo's peak at Wet 0 and -50: down alike */
        float pk[2][2];
        int l, w;
        for (l = 0; l < 2; l++)
            for (w = 0; w < 2; w++) {
                fresh();
                en_set(P_CHORD, 0);
                en_set(P_LEAD, 0);
                en_set(P_NCHAR, 0);
                en_set(l ? P_SEQ : P_SEQ2, 0);
                en_set(l ? P_SEQ2 : P_SEQ, 70);
                en_macro(EN_MAC_DUB, w ? -50 : 0);
                en_play(1);
                run(4.0f);
                en_echo_pk = 0.0f;
                run(8.0f);
                pk[l][w] = en_echo_pk;
                en_macro(EN_MAC_DUB, 0);
            }
        printf("  Wet -50: the echo x%.2f (the Sequence alone), x%.2f (Sequence 2 alone)\n",
               (double)(pk[0][1] / (pk[0][0] + 1e-9f)), (double)(pk[1][1] / (pk[1][0] + 1e-9f)));
        CHECK(pk[1][1] / (pk[1][0] + 1e-9f) < 1.3f * pk[0][1] / (pk[0][0] + 1e-9f),
              "test_wet_all: Sequence 2's echo not moved by Wet as the Sequence's");
    }

    /* every voice of every layer: it sounds, without NaN, under full scale */
    {
        static const int VP[3] = {P_CVOICE, P_SVOICE, P_LVOICE}, LV[3] = {P_CHORD, P_SEQ, P_LEAD};
        int l, v;
        for (l = 0; l < 3; l++)
            for (v = en_param_info(VP[l])->lo; v <= en_param_info(VP[l])->hi; v++) {
                fresh();
                en_set(P_DENSITY, 80);
                en_set(LV[l], 70);
                en_set(VP[l], v);
                en_regrow();
                en_play(1);
                run(6.0f);
                CHECK(!bad && peak > 0.02f && peak < 0.99f, "voice %d of %s: peak %f", v, en_param_info(VP[l])->name, (double)peak);
            }
    }

    /* a new key while playing: the chord's notes in it at once */
    {
        uint32_t pent = EN_MODE_POOL[MODE_PENT_MIN];
        en_set(P_ROOT, 2);
        en_pool(pent);
        en_set(P_PMODE, 0);                      /* (a fresh piece: every slot a gap, so the walk starts from home) */
        for (i = 0; i < EN_NPROG; i++)
            en_set(P_CR0 + i, 0), en_set(P_CQ0 + i, EN_CQ_EMPTY), en_set(P_CX0 + i, 0);
        en_regrow();
        run(0.1f);
        for (i = 0; i < EN_NPAD; i++) {
            CHECK(pent >> ((en_chord[i] - 2 + 120) % 12) & 1u, "chord note %d not in the pool", en_chord[i]);
            CHECK(en_chord[i] >= 36 && en_chord[i] <= 84, "chord note %d out of range", en_chord[i]);
        }
        CHECK(en_chord_root == 2, "regrow: the home chord (root %d)", en_chord_root);
    }

    /* the dub echo at its wildest: Feedback 100, Echo 100, frozen and not; bounded, and it dies away */
    {
        float p1, rms;
        fresh();
        en_set(P_FEEDBACK, 100);
        en_set(P_ECHO, 100);
        en_set(P_EVOLVE, 100);
        en_set(P_MOTION, 100);
        en_play(1);
        bad = 0;
        peak = sum2 = 0.0f;
        run(180.0f);
        p1 = peak;
        rms = sqrtf(sum2 / (180.0f * 44100.0f * 2.0f));
        /* (0.98, from 0.97 in 1.3.0: the master +1.5 dB and the noise bed into the echo put the soft clip a little
         * higher, 0.969 here; the echo is as bounded as it was) */
        CHECK(!bad && peak < 0.98f, "Feedback 100: peak %f", (double)peak);
        en_freeze(1);
        peak = 0.0f;
        run(30.0f);
        CHECK(!bad && peak < 0.98f, "Feedback 100 frozen: peak %f", (double)peak);
        en_freeze(0);
        en_set(P_CRACKLE, 0);
        en_play(0);
        run(60.0f);
        peak = 0.0f;
        run(1.0f);
        printf("  3 min at Feedback 100: peak %.3f, rms %.4f; a minute after stop: %.4f\n", (double)p1,
               (double)rms, (double)peak);
        CHECK(peak < 0.3f, "Feedback 100: still %f a minute after stopping", (double)peak);
    }

    /* every setting at its extremes: still bounded */
    {
        int p;
        fresh();
        en_play(1);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->hi);
        en_set(P_MIDI, 1);
        peak = 0.0f;
        run(30.0f);
        CHECK(!bad && peak < 1.0f, "all at max: peak %f", (double)peak);
        en_set(P_CVOICE, 0);
        run(30.0f);
        CHECK(!bad && peak < 1.0f, "all at max, saws: peak %f", (double)peak);
        printf("  a minute at max: peak %.3f\n", (double)peak);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->lo);
        run(30.0f);
        CHECK(!bad, "all at min: NaN");
    }

    /* (BERLIN) no drums: nothing on channel 10 or above in a minute */
    fresh();
    drum_ch_ons = 0;
    en_play(1);
    run(60.0f);
    CHECK(!drum_ch_ons, "test_no_drums: %u note-ons on the drum channels", drum_ch_ons);

    /* (BERLIN) the sequence on its grid: eighths, every note-on on an even sixteenth (no repeats); sixteenths, on odd
     * ones too */
    {
        uint32_t i, odd = 0;
        static const int8_t D[8] = {0, 4, 7, 2, 0, 4, 7, 5};
        fresh();
        en_set(P_TAPA, 0);
        en_set(P_TAPB, 0);
        seq_write(8, 0x00FFu, D);
        en_play(1);
        run(20.0f);
        for (i = 0; i < sq_n; i++)
            odd += sq_at[i] & 1u;
        CHECK(sq_n > 40u && !odd, "test_seq_on_grid: eighths: %u of %u on odd sixteenths", odd, sq_n);
        fresh();
        en_set(P_TAPA, 0);
        en_set(P_TAPB, 0);
        en_set(P_SEQGRID, 1);
        seq_write(8, 0x00FFu, D);
        en_play(1);
        run(20.0f);
        for (i = odd = 0; i < sq_n; i++)
            odd += sq_at[i] & 1u;
        CHECK(odd > 20u, "test_seq_on_grid: sixteenths: %u on odd sixteenths", odd);
        en_set(P_SEQGRID, 0);
    }

    /* (BERLIN) Repeat A: eight notes on the eighths, a dotted-eighth repeat at the same level, once: each bar eight
     * originals on the even sixteenths and eight repeats on the odd ones, each the note three sixteenths before;
     * at 96, 60 and 160 BPM alike (test_taps_tempo_extremes) */
    {
        static const int8_t D[8] = {0, 4, 7, 2, 0, 4, 7, 5};
        static const int TEMPI[3] = {96, 60, 160};
        int k;
        for (k = 0; k < 3; k++) {
            int badbars;
            fresh();
            en_set(P_TEMPO, TEMPI[k]);
            en_set(P_TAPA, 100);
            en_set(P_TAPAT, 1);
            en_set(P_TAPB, 0);
            seq_write(8, 0x00FFu, D);
            en_play(1);
            run(45.0f);
            badbars = bars_tap_a(1u, en_bars / 16u - 1u, 8, 8);
            CHECK(en_bars / 16u > 8u && !badbars, "test_tap_a at %d BPM: %d of %u bars wrong", TEMPI[k], badbars,
                  en_bars / 16u - 2u);
        }
        en_set(P_TEMPO, en_param_info(P_TEMPO)->def);
    }

    /* (BERLIN) the repeats' times are the echo's: a lone note on the downbeat, Repeat A at 1/4. (6 sixteenths): each bar
     * that note and its repeat six sixteenths later, nothing else */
    {
        static const int8_t D[8] = {0, 4, 7, 2, 0, 4, 7, 5};
        uint32_t b;
        int badbars = 0;
        fresh();
        en_set(P_TAPA, 100);
        en_set(P_TAPAT, 3);
        en_set(P_TAPB, 0);
        seq_write(8, 0x0001u, D);
        en_play(1);
        run(30.0f);
        for (b = 1u; b + 1u < en_bars / 16u; b++) {
            int n0 = 0, v0 = 0, n6 = 0, v6 = 0, total = 0, n, v;
            uint32_t k;
            for (k = 0; k < 16u; k++)
                total += seq_at(16u * b + k, &n, &v);
            if (total != 2 || !seq_at(16u * b, &n0, &v0) || !seq_at(16u * b + 6u, &n6, &v6) || n0 != n6 || v0 != v6)
                badbars++;
        }
        CHECK(en_bars / 16u > 8u && !badbars, "test_tap_times: Repeat A at 1/4.: %d bars wrong", badbars);
    }

    /* (BERLIN) Repeat B (a quarter) only into a rest: step 5 rested, it fills that eighth with the note four
     * sixteenths before (step 3's), and nowhere else; at 60 and 160 BPM */
    {
        static const int8_t D[8] = {0, 4, 7, 2, 0, 4, 7, 5};
        static const int TEMPI[2] = {60, 160};
        uint32_t b;
        int k;
        for (k = 0; k < 2; k++) {
            int badbars = 0;
            fresh();
            en_set(P_TEMPO, TEMPI[k]);
            en_set(P_TAPA, 0);
            en_set(P_TAPB, 100);
            en_set(P_TAPBT, 2);
            seq_write(8, 0x00DFu, D);
            en_play(1);
            run(45.0f);
            for (b = 1u; b + 1u < en_bars / 16u; b++) {
                int n10 = 0, v10 = 0, n6 = 0, v6 = 0, total = 0, n, v;
                uint32_t kk;
                for (kk = 0; kk < 16u; kk++)
                    total += seq_at(16u * b + kk, &n, &v);
                if (total != 8 || seq_at(16u * b + 10u, &n10, &v10) != 1 || !seq_at(16u * b + 6u, &n6, &v6) ||
                    n10 != n6 || v10 != v6)
                    badbars++;
            }
            CHECK(en_bars / 16u > 8u && !badbars, "test_tap_b_into_rests at %d BPM: %d bars wrong", TEMPI[k], badbars);
        }
        en_set(P_TEMPO, en_param_info(P_TEMPO)->def);
    }

    /* (BERLIN) Sequence 2 at Length 7 against the Sequence's 8: its note-ons repeat every 7 eighths, and the two
     * together every 56 (28 beats) */
    {
        uint32_t i, j, from = 16u * 4u, bad7 = 0, bad56 = 0, n7 = 0;
        fresh();
        en_set(P_SEQ2, 60);
        en_set(P_SEQLEN, 8);
        en_set(P_SEQ2LEN, 7);
        en_set(P_MOTION, 0);
        en_set(P_EVOLVE, 0);
        en_set(P_DRIFT, 6);
        en_set(P_TAPA, 0);
        en_regrow();
        en_play(1);
        run(150.0f);
        for (i = 0; i < s2_n; i++) {             /* each one 14 sixteenths later again, the same note */
            int ok = 0;
            if (s2_at[i] < from || s2_at[i] + 14u >= en_bars)
                continue;
            n7++;
            for (j = i + 1; j < s2_n && s2_at[j] <= s2_at[i] + 14u; j++)
                ok |= s2_at[j] == s2_at[i] + 14u && s2_note[j] == s2_note[i];
            bad7 += !ok;
        }
        for (i = 0; i < sq_n; i++) {             /* and the Sequence's, 112 sixteenths later */
            int ok = 0;
            if (sq_at[i] < from || sq_at[i] + 112u >= en_bars)
                continue;
            for (j = i + 1; j < sq_n && sq_at[j] <= sq_at[i] + 112u; j++)
                ok |= sq_at[j] == sq_at[i] + 112u && sq_note[j] == sq_note[i];
            bad56 += !ok;
        }
        CHECK(n7 > 20u && !bad7, "test_seq2_lengths: Sequence 2 not every 7 eighths (%u of %u)", bad7, n7);
        CHECK(!bad56, "test_seq2_lengths: the two not together every 56 eighths (%u)", bad56);
    }

    /* (BERLIN) a piece's patterns, kept: taken out, grown afresh, put back: the same */
    {
        uint8_t saved[EN_SEQ_N], again[EN_SEQ_N];
        fresh();
        en_play(1);
        run(1.0f);
        en_seq_get(saved);
        en_regrow();
        run(0.1f);
        en_seq_get(again);
        CHECK(memcmp(saved, again, EN_SEQ_N), "test_seq_roundtrip: a regrow changed nothing");
        en_seq(saved);
        en_regrow();
        run(0.1f);
        en_seq_get(again);
        CHECK(!memcmp(saved, again, EN_SEQ_N), "test_seq_roundtrip: not put back as it was");
        en_seq(0);
    }

    /* (BERLIN) Pattern: the lanes as written, never varied: steps 0, 2, 4, 6 of 8 only (sixteenths 0, 4, 8, 12 of
     * each bar), for 64 bars with Motion and Evolve at 100 */
    {
        uint32_t i, off = 0;
        fresh();
        en_set(P_TAPA, 0);
        en_set(P_TAPB, 0);
        en_set(P_SEQLEN, 8);
        en_set(P_MOTION, 100);
        en_set(P_EVOLVE, 100);
        en_set(P_SGATE0, 0x0055);
        en_set(P_SMODE, 1);
        en_play(1);
        run(64.0f * 2.5f);
        for (i = 0; i < sq_n; i++)
            off += (sq_at[i] & 3u) != 0u;
        /* (the arc's Drift takes the sequences out a while: notes in most bars, not all) */
        CHECK(en_bars / 16u >= 63u && sq_n >= 100u && !off, "test_pattern_mode: %u of %u off the written steps (%u bars)",
              off, sq_n, en_bars / 16u);
        en_set(P_SMODE, 0);
    }

    /* (BERLIN) the Drone holds: Drift 32 bars, two minutes at 96 (48 bars): its notes (channel 1) start only when a
     * chord starts, on a bar line (no stabs), three to five of them, in the scale */
    {
        uint32_t i, groups = 0, offbar = 0, outscale = 0, badsize = 0, last = 0xFFFFFFFFu, size = 0;
        const uint32_t pl = EN_MODE_POOL[MODE_AEOLIAN];
        fresh();
        en_set(P_ROOT, 2);
        en_pool(pl);
        en_set(P_CHORD, 60);
        en_set(P_DRIFT, 4);
        en_play(1);
        run(120.0f);
        for (i = 0; i < dr_n; i++) {
            if (dr_at[i] != last) {
                if (groups && (size < 3u || size > 5u))
                    badsize++;
                groups++, size = 0, last = dr_at[i];
            }
            size++;
            offbar += (dr_at[i] & 15u) != 0u;
            outscale += !(pl >> ((dr_note[i] + 120 - 2) % 12) & 1u);
        }
        if (groups && (size < 3u || size > 5u))
            badsize++;
        CHECK(groups >= 1u && groups <= en_bars / 16u / 32u + 2u, "test_drone_holds: %u chords in %u bars", groups,
              en_bars / 16u);
        CHECK(!badsize && !outscale, "test_drone_holds: %u chords not of 3-5 notes, %u notes off the scale", badsize,
              outscale);
        CHECK(!offbar, "test_drone_no_stabs: %u notes off the bar line", offbar);
    }

    /* (BERLIN) the phaser at full: the Drone alone, a minute, bounded */
    fresh();
    en_set(P_SEQ, 0);
    en_set(P_SEQ2, 0);
    en_set(P_CHORD, 100);
    en_set(P_PHASER, 100);
    en_set(P_CVOICE, 1);                         /* (Strings: the ensemble too) */
    en_play(1);
    peak = 0.0f;
    run(60.0f);
    CHECK(!bad && peak > 0.01f && peak < 0.95f, "test_phaser_bounded: peak %f", (double)peak);

    /* (BERLIN) the Lead: four minutes at Phrases 50: its notes (channel 3) in phrases with a bar or more of silence
     * between, four or more of them; one note at a time; all in the scale */
    fresh();
    ld_pool = EN_MODE_POOL[MODE_AEOLIAN];
    en_set(P_ROOT, 2);
    en_pool(ld_pool);
    en_set(P_LEAD, 60);
    en_set(P_LDENS, 50);
    ld_ons = ld_groups = ld_mono_bad = ld_outscale = ld_on_now = 0;
    ld_last_off = 0xFFFFFFFFu;
    en_play(1);
    run(240.0f);
    CHECK(ld_groups >= 4u && ld_ons > ld_groups, "test_lead_phrases: %u phrases, %u notes", ld_groups, ld_ons);
    CHECK(!ld_outscale, "test_lead_phrases: %u notes off the scale", ld_outscale);
    CHECK(!ld_mono_bad, "test_lead_mono: %u notes over another", ld_mono_bad);
    printf("  4 min of the Lead at Phrases 50: %u phrases, %u notes\n", ld_groups, ld_ons);
    ld_pool = 0xFFFu;

    /* (BERLIN) the Lead at its longest glide, a minute: bounded */
    fresh();
    en_set(P_LEAD, 100);
    en_set(P_LDENS, 100);
    en_set(P_GLIDE, 100);
    en_set(P_VIB, 100);
    en_play(1);
    peak = 0.0f;
    run(60.0f);
    CHECK(!bad && peak < 0.95f, "test_lead_glide_bounded: peak %f", (double)peak);

    /* (BERLIN, the Lead's memory) the Lead's numbers over 30 minutes of Stratosfear, Motion 100 (Solo comes every few
     * sections): printed (today's the baseline; the plan's later tasks assert them) */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        lm_t m;
        fresh();
        piece(4);
        en_piece(4, pp, &pl);
        lg_pool = pl, lg_root = pp[P_ROOT];
        en_set(P_MOTION, 100);
        en_play(1);
        lead_metrics(1800.0f, &m);
        lm_print("test_lead_metrics: 30 min of Stratosfear's Lead, Motion 100", &m);
        CHECK(m.notes > 100u && m.phrases > 20u, "test_lead_metrics: %u notes, %u phrases", m.notes, m.phrases);
        /* (the generator: mostly steps, as Temperley's Essen lines; after a leap of 5 semitones or more, back the
         * other way at least 60% of the time, over 10 leaps or more) */
        CHECK(m.step_share > 0.5f && m.intervals >= 100u, "test_motif_steps: %.1f%% of %u intervals 2 semitones or less",
              (double)(100.0f * m.step_share), m.intervals);
        CHECK(m.leap_turn >= 0.6f && m.leaps >= 10u, "test_motif_leap_turn: a turn after %.1f%% of %u leaps",
              (double)(100.0f * m.leap_turn), m.leaps);
        lg_tension();
    }

    /* (BERLIN, the Lead's memory) pairs: 20 minutes in D minor, the walk's chords, Motion 100, Lead in 0, Phrases 100.
     * Every call answered (the next phrase, beginning within 2 bars of the call's end); the answer ends on the chord's
     * root or fifth, the call on another chord tone or the 9th (the chord as it was at the phrase's start) */
    {
        const uint32_t pl = EN_MODE_POOL[MODE_AEOLIAN];
        uint32_t i, calls = 0, answers = 0, unanswered = 0, late = 0, bad_call = 0, bad_ans = 0, nochord = 0;
        lm_t m;
        fresh();
        en_set(P_ROOT, 2);
        en_pool(pl);
        lg_pool = pl, lg_root = 2;
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_set(P_MOTION, 100);
        en_play(1);
        lead_metrics(1200.0f, &m);
        for (i = 0; i < lg_n; i++) {
            int pcs[4], p9 = -1, p5 = -1, n, pc, k, ok = 0;
            uint32_t j = i;
            if (!lg_first(i))
                continue;
            while (!lg_last(j))
                j++;
            n = lg_chord(lg[i].croot, pcs, &p9, &p5);
            pc = lg[j].note % 12;
            if (!n) {
                nochord++;
                continue;
            }
            if (lg[i].role == LR_CALL) {
                calls++;
                for (k = 1; k < n; k++)
                    ok |= pc == pcs[k];
                ok |= pc == p9;
                if (!ok) {
                    if (!bad_call)
                        printf("  a call ending on %d (chord root %d, its 9th %d)\n", pc, pcs[0], p9);
                    bad_call++;
                }
                if (j + 1 >= lg_n) {
                    if (lg[j].off != 0xFFFFFFFFu && en_bars - lg[j].off > 40u)   /* (a call still sounding as the run
                                                                                 * ends: its answer not due yet) */
                        unanswered++;
                } else if (lg[j + 1].role != LR_ANSWER) {
                    unanswered++;
                } else if (lg[j].off == 0xFFFFFFFFu || lg[j + 1].on - lg[j].off > 32u) {
                    late++;
                }
            } else if (lg[i].role == LR_ANSWER) {
                answers++;
                if (pc != pcs[0] && pc != p5) {
                    if (!bad_ans)
                        printf("  an answer ending on %d (chord root %d, fifth %d)\n", pc, pcs[0], p5);
                    bad_ans++;
                }
            }
        }
        lm_print("test_pairs: 20 min, D minor, Motion 100", &m);
        CHECK(calls >= 20u && answers >= calls - 1u && !unanswered && !late,
              "test_pairs: %u calls, %u answers, %u unanswered, %u answered late", calls, answers, unanswered, late);
        CHECK(!bad_call && !bad_ans && !nochord, "test_pairs: endings: %u calls not open, %u answers not closed (%u "
              "phrases on a chord off the scale)", bad_call, bad_ans, nochord);
        lg_tension();
        CHECK(tn_held >= 10u && !tn_unres, "test_motif_tension: %u held tones off the chord (this run and "
              "test_lead_metrics'), %u not resolved a step down onto it", tn_held, tn_unres);
    }

    /* (BERLIN, the Lead's memory) a chord change in the middle of a motif: its notes rendered at the phrase's start
     * (the call's notes all from the chord it began on, the note in flight across the change not struck again), and
     * the next pass, the answer, transposed to the new chord (the same rhythm, its offsets from the new root). The
     * motif 6 6 6 6 8 sixteenths from a bar line: the next chord (en_next) comes during its third note */
    {
        static const int8_t OFF[5] = {0, 1, 2, 4, 2};
        static const uint8_t DUR[5] = {6, 6, 6, 6, 8};
        const uint32_t pl = EN_MODE_POOL[MODE_AEOLIAN];
        int tries, done = 0, ok = 0, st[12], ns = en_scale(pl, st);
        fresh();
        en_set(P_ROOT, 2);
        en_pool(pl);
        lg_pool = pl, lg_root = 2;
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_set(P_MOTION, 0);
        en_set(P_DRIFT, 6);                      /* (Hold: only en_next moves the chord) */
        en_play(1);
        run(1.0f);
        for (tries = 0; tries < 8 && !done; tries++) {
            int r0, r1;
            uint32_t c, a, k;
            lg_n = 0, lg_open = -1;
            en_lead_motif(1, OFF, DUR, 5);
            if (!lg_wait(LR_CALL, 120.0f))
                break;
            r0 = en_chord_root;
            en_next();
            run(25.0f);
            r1 = en_chord_root;
            c = lg_find(0, LR_CALL);
            a = lg_find(c, LR_ANSWER);
            if (r1 == r0 || a >= lg_n || a < c + 5u || a + 5u > lg_n)
                continue;
            done = 1;
            ok = lg[c].croot == r0 && lg[a].croot == r1 && lg_last(c + 4u) && lg_last(a + 4u);
            for (k = 0; k < 5u && ok; k++) {
                ok &= lg[c + k].on - lg[c].on == lg[a + k].on - lg[a].on;   /* (the same rhythm) */
                if (k < 4u)                      /* (the last note: the ending's) */
                    ok &= lg_deg(lg[c + k].note) - 5 * ns - en_deg_of(pl, r0 - 2) == OFF[k] &&
                          lg_deg(lg[a + k].note) - 5 * ns - en_deg_of(pl, r1 - 2) == OFF[k];
            }
            ok &= lg[c + 2].on - lg[c].on == 12u && lg[c + 3].on - lg[c].on == 18u;   /* (nothing struck at 16) */
            if (!ok)
                for (k = 0; k < 5u; k++)
                    printf("  call %u at +%u, answer %u at +%u (roots %d %d)\n", lg[c + k].note, lg[c + k].on - lg[c].on,
                           lg[a + k].note, lg[a + k].on - lg[a].on, r0, r1);
        }
        CHECK(done && ok, "test_motif_chord_change: tried %d, done %d, ok %d", tries, done, ok);
    }

    /* (BERLIN, the Lead's memory) rests in a motif: 4 sixteenths of a note, a rest of 4, a note of 4, a rest of 2, the
     * last note 6. Each rest lets the note go (its note-off at the rest's start) and holds silence its length; the
     * answer the same, its last note twice as long */
    {
        static const int8_t OFF[5] = {0, LD_REST, 2, LD_REST, 4};
        static const uint8_t DUR[5] = {4, 4, 4, 2, 6};
        static const uint32_t ON[3] = {0, 8, 14}, END[3] = {4, 12, 20};
        uint32_t c = 0, a = 0, k;
        int ok = 0, got;
        fresh();
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_set(P_MOTION, 0);
        en_play(1);
        run(1.0f);
        lg_n = 0, lg_open = -1;
        en_lead_motif(2, OFF, DUR, 5);
        got = lg_wait(LR_CALL, 120.0f);
        run(25.0f);
        if (got) {
            c = lg_find(0, LR_CALL);
            a = lg_find(c, LR_ANSWER);
            ok = a + 3u <= lg_n && a == c + 3u && lg_last(c + 2u) && lg_last(a + 2u);
            for (k = 0; k < 3u && ok; k++)
                ok &= lg[c + k].on - lg[c].on == ON[k] && lg[c + k].off - lg[c].on == END[k] &&
                      lg[a + k].on - lg[a].on == ON[k] && lg[a + k].off - lg[a].on == (k < 2u ? END[k] : 26u);
            if (!ok)
                for (k = 0; k < 3u && c + k < lg_n; k++)
                    printf("  call note %u: on +%u off +%u\n", k, lg[c + k].on - lg[c].on, lg[c + k].off - lg[c].on);
        }
        CHECK(got && ok, "test_lead_rests: a call %d, its rests as written %d", got, ok);
    }

    /* (BERLIN, the Lead's memory) the generated Lead is silent in Emerge (spec 3.1): the Drone's Intro 24 (Emerge 24
     * bars), Lead in 0, Phrases 100: no Lead note before Flow, and some after */
    {
        uint32_t i, in_emerge = 0, after = 0;
        fresh();
        en_set(P_MI0, 24);
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_set(P_MOTION, 0);
        en_play(1);
        lg_n = 0, lg_open = -1;
        run(120.0f);
        for (i = 0; i < lg_n; i++) {
            in_emerge += lg[i].sec == EN_SEC_EMERGE;
            after += lg[i].sec != EN_SEC_EMERGE;
        }
        CHECK(!in_emerge && after, "test_lead_emerge_silent: %u Lead notes in Emerge, %u after", in_emerge, after);
    }

    /* (BERLIN, the Lead's memory) tiny scales: pools of 1, 2 and 3 notes and the minor pentatonic, on D; 150 s each,
     * Lead in 0, Phrases 100, Motion 100: the Lead plays, every note in the scale and within 36..100, one at a time,
     * nothing left sounding when stopped (its weights and its step down onto a chord tone never stuck) */
    {
        const uint32_t POOLS[4] = {0x001u, 0x081u, 0x089u, EN_MODE_POOL[MODE_PENT_MIN]};
        int p;
        for (p = 0; p < 4; p++) {
            uint32_t i, range = 0, left = 0, k;
            fresh();
            ld_pool = POOLS[p];
            en_set(P_ROOT, 2);
            en_pool(ld_pool);
            en_set(P_LEAD, 60);
            en_set(P_LDIN, 0);
            en_set(P_LDENS, 100);
            en_set(P_MOTION, 100);
            ld_ons = ld_outscale = ld_mono_bad = ld_on_now = 0;
            lg_n = 0, lg_open = -1;
            en_play(1);
            run(150.0f);
            en_play(0);
            run(1.0f);
            for (i = 0; i < lg_n; i++)
                range += lg[i].note < 36 || lg[i].note > 100;
            for (k = 0; k < 128u; k++)
                left += sounding[2][k] != 0;
            CHECK(ld_ons >= 4u && !ld_outscale && !ld_mono_bad && !range && !left,
                  "test_motif_tiny_scale: pool %03x: %u notes, %u off the scale, %u over another, %u outside 36..100, "
                  "%u left sounding", ld_pool, ld_ons, ld_outscale, ld_mono_bad, range, left);
        }
        ld_pool = 0xFFFu;
    }

    /* (BERLIN, the Lead's memory) a phrase the register would clip: F minor, Lead oct +1 and a motif two octaves up
     * (offsets 7 9 11 14 12 10 9): the whole phrase an octave down, not its high notes clamped at 100 (an E, off the
     * scale); then Lead oct -2 and the motif two octaves down: the phrase up. The call's and the answer's notes within
     * 36..100 and in the scale, their steps (but the last, the ending's) the motif's */
    {
        static const int8_t HI[7] = {7, 9, 11, 14, 12, 10, 9}, LO[7] = {-7, -9, -11, -14, -12, -10, -9};
        static const uint8_t DUR[7] = {4, 4, 4, 4, 4, 4, 8};
        const uint32_t pl = EN_MODE_POOL[MODE_AEOLIAN];
        int pass, ok[2] = {0, 0}, got[2] = {0, 0};
        fresh();
        en_set(P_ROOT, 5);
        en_pool(pl);
        lg_pool = pl, lg_root = 5;
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_set(P_MOTION, 0);
        en_play(1);
        run(1.0f);
        for (pass = 0; pass < 2; pass++) {
            const int8_t *off = pass ? LO : HI;
            uint32_t c = 0, a, k, j;
            uint8_t ph;
            en_set(P_LOCT, pass ? -2 : 1);
            lg_n = 0, lg_open = -1;
            en_lead_motif(1, off, DUR, 7);
            got[pass] = lg_wait(LR_CALL, 120.0f);
            ph = en_lead_phr;
            run(25.0f);
            if (!got[pass])
                continue;
            while (c < lg_n && lg[c].phr != ph)
                c++;
            a = lg_find(c + 1u, LR_ANSWER);
            ok[pass] = a == c + 7u && a + 7u <= lg_n && lg_last(c + 6u) && lg_last(a + 6u);
            for (j = 0; j < 2u && ok[pass]; j++) {
                const uint32_t s = j ? a : c;
                for (k = 0; k < 7u; k++)
                    ok[pass] &= lg[s + k].note >= 36 && lg[s + k].note <= 100 && lg_deg(lg[s + k].note) > -99;
                for (k = 0; k + 2u < 7u; k++)
                    ok[pass] &= lg_deg(lg[s + k + 1u].note) - lg_deg(lg[s + k].note) == off[k + 1u] - off[k];
            }
            if (!ok[pass])
                for (k = c; k < lg_n && k < c + 14u; k++)
                    printf("  pass %d: note %u (degree %d), role %u\n", pass, lg[k].note, lg_deg(lg[k].note), lg[k].role);
        }
        en_set(P_LOCT, 0);
        CHECK(got[0] && got[1] && ok[0] && ok[1], "test_lead_clip: high: a call %d, kept %d; low: a call %d, kept %d",
              got[0], ok[0], got[1], ok[1]);
    }

    /* (BERLIN) the echo ping-pongs: one note into it alone (no repeats, no plate, no cloud, the others silent), Width
     * 100: its first repeat at least 6 dB louder on one side, its second on the other */
    {
        static const int8_t D[16] = {0};
        const float step = 44100.0f * 60.0f / (96.0f * 4.0f), at = 32.0f * step, d = 3.0f * step;   /* (the dotted eighth) */
        float e[2][2] = {{0.0f, 0.0f}, {0.0f, 0.0f}};
        uint32_t t = 0;
        int b, j, w;
        fresh();
        en_set(P_TEMPO, 96);
        en_set(P_CHORD, 0);
        en_set(P_LEAD, 0);
        en_set(P_SEQ2, 0);
        en_set(P_CRACKLE, 0);
        en_set(P_REVERB, 0);
        en_set(P_CLOUD, 0);
        en_set(P_TAPA, 0);
        en_set(P_TAPB, 0);
        en_set(P_ECHO, 100);
        en_set(P_ETIME, 1);
        en_set(P_FEEDBACK, 50);
        en_set(P_EWIDTH, 100);
        seq_write(16, 0x0001u, D);               /* (a note every two bars: the one at sixteenth 32 measured) */
        en_play(1);
        for (b = 0; b < (int)((at + 3.0f * d) / 256.0f) + 2; b++) {
            en_render(buf, 256, 2048);
            for (j = 0; j < 256; j++, t++)
                for (w = 0; w < 2; w++) {        /* (a window a sixteenth long from a quarter before each repeat) */
                    const float t0 = at + (float)(w + 1) * d - 0.25f * step;
                    if ((float)t >= t0 && (float)t < t0 + step) {
                        const float l = (float)buf[2 * j] / 8388608.0f, r = (float)buf[2 * j + 1] / 8388608.0f;
                        e[w][0] += l * l, e[w][1] += r * r;
                    }
                }
        }
        printf("  ping-pong: first repeat L/R %.1f dB, second %.1f dB\n", (double)(10.0f * log10f((e[0][0] + 1e-12f) / (e[0][1] + 1e-12f))),
               (double)(10.0f * log10f((e[1][0] + 1e-12f) / (e[1][1] + 1e-12f))));
        CHECK((e[0][0] > 4.0f * e[0][1] && e[1][1] > 4.0f * e[1][0]) || (e[0][1] > 4.0f * e[0][0] && e[1][0] > 4.0f * e[1][1]),
              "test_echo_pingpong: the repeats not side to side");
        en_set(P_EWIDTH, en_param_info(P_EWIDTH)->def);
    }

    /* (BERLIN) the Atmosphere: each character half a minute alone, sound, no NaN, bounded */
    {
        int c;
        for (c = 0; c < EN_NNCHAR; c++) {
            fresh();
            en_set(P_SEQ, 0);
            en_set(P_SEQ2, 0);
            en_set(P_CHORD, 0);
            en_set(P_LEAD, 0);
            en_set(P_CRACKLE, 60);
            en_set(P_NCHAR, c);
            en_play(1);
            peak = 0.0f;
            run(30.0f);
            if (!c)                              /* (None: silence) */
                CHECK(!bad && peak < 0.005f, "test_atmos_chars: None: peak %f (no atmosphere: under -46 dB)", (double)peak);
            else
                CHECK(!bad && peak > 0.01f && peak < 0.8f, "test_atmos_chars: %s: peak %f", EN_NCHAR_NAME[c], (double)peak);
        }
    }

    /* (BERLIN) the white keys play the Lead: degree d of D Aeolian (from D4 up), rising, in the scale, one at a time,
     * let go with its note-off; the Lead's own phrases wait while a key is held and two bars after */
    {
        int d, last = 0, rising = 1, before;
        const uint32_t pl = EN_MODE_POOL[MODE_AEOLIAN];
        fresh();
        ld_pool = pl;
        en_set(P_ROOT, 2);
        en_pool(pl);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_play(1);
        run(0.5f);
        ld_ons = ld_outscale = ld_mono_bad = ld_on_now = 0;
        for (d = 0; d < 16; d++) {
            en_lead_key(d, 1);
            run(0.3f);
            if (en_lead_deg != d)
                rising = 0;
            if (d && last_lead <= last)
                rising = 0;
            last = last_lead;
            en_lead_key(d, 0);
            run(0.1f);
        }
        CHECK(ld_ons == 16u && rising && !ld_outscale && !ld_mono_bad && !ld_on_now,
              "test_lead_keys: %u notes, rising %d, %u off the scale, %u over another, %u still on", ld_ons, rising,
              ld_outscale, ld_mono_bad, ld_on_now);
        en_lead_key(0, 1);
        run(0.3f);                               /* (the key's own note in first) */
        before = (int)ld_ons;
        run(8.0f);
        CHECK((int)ld_ons == before, "test_lead_keys: the Lead's own phrases played while a key was held");
        en_lead_key(0, 0);
        ld_pool = 0xFFFu;
    }

    /* (BERLIN) Pattern: the Octaves lane puts its step an octave up */
    {
        int n0 = 0, n1 = 0, v;
        uint32_t i;
        fresh();
        en_set(P_TAPA, 0);
        en_set(P_DRIFT, 6);
        en_set(P_SGATE0, 0x0001);
        en_set(P_SOCT0, 0);
        en_set(P_SMODE, 1);
        en_play(1);
        run(3.0f);
        for (i = 0; i < sq_n; i++)
            if (sq_at[i] == 16u)
                n0 = sq_note[i];
        fresh();
        en_set(P_TAPA, 0);
        en_set(P_DRIFT, 6);
        en_set(P_SGATE0, 0x0001);
        en_set(P_SOCT0, 0x0001);
        en_set(P_SMODE, 1);
        en_play(1);
        run(3.0f);
        for (i = 0; i < sq_n; i++)
            if (sq_at[i] == 16u)
                n1 = sq_note[i];
        (void)v;
        CHECK(n0 && n1 == n0 + 12, "test_pattern_octaves: %d, with its octave %d", n0, n1);
    }

    /* (BERLIN) the layers come in one by one, by their filters (spec 3.3): Sequence 2's Intro 0, its notes from its
     * first bar and its filter open; Intro 8, its notes from the first bar too (no hidden level) but its filter closed
     * at first (4 octaves under its knob at least) and open from bar 8; Lead in 8: no Lead before bar 8, and some
     * after */
    {
        uint32_t i, first0 = 0xFFFFFFFFu, first8 = 0xFFFFFFFFu, lastb = 0xFFFFFFFFu;
        int closed = 1, open = 1, k;
        fresh();
        en_set(P_SEQ2, 60);
        en_set(P_MI3, 0);
        en_set(P_MW3, 0);
        en_play(1);
        run(10.0f);
        for (i = 0; i < s2_n; i++)
            first0 = s2_at[i] < first0 ? s2_at[i] : first0;
        CHECK(first0 < 16u && en_fo[3] == 0.0f, "test_layer_entries: Intro 0: first note at sixteenth %u, offset %f",
              first0, (double)en_fo[3]);
        fresh();
        en_set(P_SEQ2, 60);
        en_set(P_MI3, 8);
        en_set(P_MW3, 0);
        en_set(P_BRIDGE, 0);
        en_set(P_LDIN, 8);
        en_set(P_LEAD, 60);
        en_set(P_LDENS, 100);
        ld_first = 0xFFFFFFFFu;
        en_play(1);
        for (k = 0; k < 480; k++) {
            uint32_t b;
            run(0.25f);
            b = (en_bars - 1u) / 16u;            /* (the bar playing: en_bars counts the sixteenth just struck) */
            if (b == lastb)
                continue;
            lastb = b;
            if (b < 1u && en_fo[3] > -4.0f)
                closed = 0;
            if (b >= 8u && fabsf(en_fo[3]) > 0.01f)
                open = 0;
        }
        for (i = 0; i < s2_n; i++)
            first8 = s2_at[i] < first8 ? s2_at[i] : first8;
        CHECK(first8 < 16u && closed && open, "test_layer_entries: Intro 8: first note at %u, closed at first %d, open "
              "from bar 8 %d", first8, closed, open);
        CHECK(ld_first != 0xFFFFFFFFu && ld_first >= 8u * 16u, "test_layer_entries: Lead in 8: first Lead note at %u",
              ld_first);
    }

    /* (BERLIN) the sections: Motion 0 stays in Flow; Motion 100 goes through Solo and the Bridge too */
    {
        int k, m, secs[EN_NSEC];
        static const int MOT[2] = {100, 0};
        for (m = 0; m < 2; m++) {
            memset(secs, 0, sizeof secs);
            fresh();
            en_set(P_MOTION, MOT[m]);
            en_play(1);
            for (k = 0; k < 600; k++) {
                run(1.0f);
                secs[en_section < EN_NSEC ? en_section : 0]++;
            }
            printf("  10 min at Motion %d: Emerge %d s, Flow %d, Solo %d, Bridge %d\n", MOT[m], secs[EN_SEC_EMERGE],
                   secs[EN_SEC_FLOW], secs[EN_SEC_SOLO], secs[EN_SEC_BRIDGE]);
            if (MOT[m])
                CHECK(secs[EN_SEC_FLOW] > 0 && secs[EN_SEC_SOLO] > 0 && secs[EN_SEC_BRIDGE] > 0,
                      "test_sections: Motion 100: Flow %d s, Solo %d, Bridge %d", secs[EN_SEC_FLOW], secs[EN_SEC_SOLO],
                      secs[EN_SEC_BRIDGE]);
            else
                CHECK(secs[EN_SEC_FLOW] == 600, "test_sections: Motion 0 left Flow (%d s in Flow)", secs[EN_SEC_FLOW]);
        }
    }

    /* (BERLIN) the arc: at 160 BPM (a bar 1.5 s), the Sequence's Intro 32: Emerge for the first 32 bars, never again;
     * the Sequence playing from the start (no hidden level) but its filter 4.5 octaves under its knob at least through
     * the first quarter (it emerges late); over 30 minutes at Motion 70 every section is heard */
    {
        uint32_t b, lastb = 0xFFFFFFFFu;
        int emerge_first = 1, emerge_after = 0, closed = 1, seen[EN_NSEC] = {0, 0, 0, 0}, k;
        fresh();
        en_set(P_TEMPO, 160);
        en_set(P_MI1, 32);
        en_set(P_SWEEP, 0);
        en_set(P_ARC, 1);
        en_set(P_MOTION, 70);
        en_play(1);
        for (k = 0; k < 1200; k++) {             /* 30 minutes */
            run(1.5f);
            b = en_bars / 16u;
            if (b == lastb)
                continue;
            lastb = b;
            seen[en_section < EN_NSEC ? en_section : 0] = 1;
            if (b < 32u && en_section != EN_SEC_EMERGE)
                emerge_first = 0;
            if (b > 33u && en_section == EN_SEC_EMERGE)
                emerge_after = 1;
            if (b < 8u && en_fo[1] > -4.5f)
                closed = 0;
        }
        CHECK(emerge_first && !emerge_after, "test_starts_emerge: Emerge for the first 32 bars %d, again after %d",
              emerge_first, emerge_after);
        CHECK(closed && sq_n > 0u && sq_at[0] < 32u, "test_emerge_sequence_late: closed %d; its first note at %u",
              closed, sq_n ? sq_at[0] : 0u);
        CHECK(seen[0] && seen[1] && seen[2] && seen[3], "test_sections_all: Emerge %d Flow %d Solo %d Bridge %d", seen[0],
              seen[1], seen[2], seen[3]);
    }

    /* (BERLIN) the Sequence's wander (its Width, Sweep): in Flow (Motion 0, no Arc, no Bridge), Sweep 100, ten
     * minutes: its offset wanders over much of its +-2 octaves, eased (a bar's step, sampled, at most 1.25 octave at
     * Time 50); Sweep 0: still, at 0 */
    {
        int k;
        static const int SW[2] = {100, 0};
        for (k = 0; k < 2; k++) {
            float lo = 9.0f, hi = -9.0f, last = -99.0f, step = 0.0f;
            uint32_t lastb = 0xFFFFFFFFu;
            int j;
            fresh();
            en_set(P_MOTION, 0);
            en_set(P_ARC, 0);
            en_set(P_BRIDGE, 0);
            en_set(P_SWEEP, SW[k]);
            en_play(1);
            for (j = 0; j < 2400; j++) {
                run(0.25f);
                if (en_bars / 16u == lastb)
                    continue;
                lastb = en_bars / 16u;
                lo = fminf(lo, en_fo[1]), hi = fmaxf(hi, en_fo[1]);
                if (last > -99.0f)
                    step = fmaxf(step, fabsf(en_fo[1] - last));
                last = en_fo[1];
            }
            printf("  10 min, Sweep %d: the Sequence's offset %.2f .. %.2f octaves, a bar's step at most %.3f\n", SW[k],
                   (double)lo, (double)hi, (double)step);
            if (SW[k])
                CHECK(hi - lo >= 1.5f && hi <= 2.0f && lo >= -2.0f && step <= 1.25f, "test_sweep_moves: %.2f .. %.2f, "
                      "step %.3f", (double)lo, (double)hi, (double)step);
            else
                CHECK(lo == 0.0f && hi == 0.0f, "test_sweep_moves: Sweep 0 moved (%.2f .. %.2f)", (double)lo, (double)hi);
        }
    }

    /* (BERLIN) the swell: every return from the Bridge to Flow has the swell in the two bars before it (Motion 100,
     * twenty minutes at 160) */
    {
        uint32_t lastb = 0xFFFFFFFFu;
        int k, prev = -1, swell1 = 0, swell2 = 0, returns = 0, bad = 0;
        fresh();
        en_set(P_TEMPO, 160);
        en_set(P_MOTION, 100);
        en_play(1);
        for (k = 0; k < 4800; k++) {
            run(0.25f);
            if (en_bars / 16u == lastb)
                continue;
            lastb = en_bars / 16u;
            if (prev == EN_SEC_BRIDGE && en_section == EN_SEC_FLOW) {
                returns++;
                bad += !(swell1 || swell2);
            }
            swell2 = swell1, swell1 = en_swell, prev = en_section;
        }
        CHECK(returns > 0 && !bad, "test_swell_before_return: %d returns, %d without a swell", returns, bad);
    }

    /* (BERLIN, spec 3) the filter motion: no hidden levels. Timewind with every Intro 16, Bridge 100, Motion 100, 20
     * minutes at 160 (Sequence 2 heard, Repeat B off): every Sequence and Sequence 2 note-on's velocity is the accent
     * rule's (120, or 120 - 86 Accent / 100: no arc factor), notes in every section; the Atmosphere's gain its level's */
    {
        int k, atm_bad = 0;
        float atm_worst = 0.0f;
        fresh();
        piece(0);
        fo_all(-1, -1, -1, 16);
        en_set(P_BRIDGE, 100);
        en_set(P_MOTION, 100);
        en_set(P_TEMPO, 160);
        en_set(P_SEQ2, 60);
        en_set(P_TAPB, 0);
        vel_lo = 120 - 86 * en_param_info(P_SACCENT)->def / 100;
        vel_bad = 0;
        memset(sec_ons, 0, sizeof sec_ons);
        en_play(1);
        for (k = 0; k < 1200; k++) {
            run(1.0f);
            if (k >= 5) {                         /* (once its gate has opened: Timewind's Atmosphere 30) */
                const float want = 30.0f * 0.0035f, dif = fabsf(en_atm_gain - want);
                atm_worst = fmaxf(atm_worst, dif);
                atm_bad += dif > 1e-3f * want;
            }
        }
        vel_lo = -1;
        printf("  20 min, every Intro 16, Bridge 100: Sequence notes in Emerge %u, Flow %u, Solo %u, Bridge %u; %u off the "
               "accent rule\n", sec_ons[EN_SEC_EMERGE], sec_ons[EN_SEC_FLOW], sec_ons[EN_SEC_SOLO], sec_ons[EN_SEC_BRIDGE],
               vel_bad);
        CHECK(!vel_bad && sec_ons[EN_SEC_EMERGE] && sec_ons[EN_SEC_FLOW] && sec_ons[EN_SEC_SOLO] &&
                  sec_ons[EN_SEC_BRIDGE] && s2_n,
              "test_fm_no_hidden_level: %u velocities not 120 or %d; notes by section %u %u %u %u, Sequence 2 %u", vel_bad,
              120 - 86 * en_param_info(P_SACCENT)->def / 100, sec_ons[0], sec_ons[1], sec_ons[2], sec_ons[3], s2_n);
        CHECK(!atm_bad, "test_fm_no_hidden_level: the Atmosphere's gain off its level %d times (by %.5f)", atm_bad,
              (double)atm_worst);
    }

    /* (BERLIN) at rest, all arc long: Width, Rate, Intro and Bridge 0, no Arc, Motion 100 (every section), ten minutes
     * at 160: every layer's offset exactly 0 (the filters at their knobs) */
    {
        int k, l, moved = 0, bridge = 0;
        fresh();
        piece(0);
        fo_all(0, -1, 0, 0);
        en_set(P_BRIDGE, 0);
        en_set(P_ARC, 0);
        en_set(P_MOTION, 100);
        en_set(P_TEMPO, 160);
        en_play(1);
        for (k = 0; k < 2400; k++) {
            run(0.25f);
            bridge |= en_section == EN_SEC_BRIDGE;
            for (l = 0; l < 5; l++)
                moved += en_fo[l] != 0.0f || en_fo_dir[l] != 0;
        }
        CHECK(!moved && bridge, "test_fm_zero: %d offsets not 0 (a Bridge heard %d)", moved, bridge);
    }

    /* (BERLIN) relative (spec 3.1): during an intro and during a bridge, the Sequence's Cutoff turned up 10: its offset
     * the same as without the turn (two runs from one state), its notes' cutoff up by the knob's own step (x2^0.55) */
    {
        int k, at;
        for (at = 0; at < 2; at++) {
            float a[2], b[2];
            const float want = powf(2.0f, 0.55f);
            fresh();
            piece(0);                             /* (Timewind: the Sequence's Intro 32) */
            en_set(P_TEMPO, 160);
            en_set(P_CUTOFF, 45);
            if (at) {                             /* (a bridge: Motion 100, Bridge 100; its move well in) */
                en_set(P_MI1, 0);
                en_set(P_MOTION, 100);
                en_set(P_BRIDGE, 100);
            }
            en_play(1);
            if (at) {
                for (k = 0; k < 4000 && en_section != EN_SEC_BRIDGE; k++)
                    run(0.05f);
                run(9.0f);                        /* (six bars in: its move eased in) */
            } else {
                run(9.0f);                        /* (six bars into the 32 of the intro) */
            }
            probe_child(-1, 0, 3.0f, a);
            probe_child(P_CUTOFF, 55, 3.0f, b);
            printf("  relative, %s: offset %.3f / %.3f, the cutoff %.1f -> %.1f Hz (x%.4f)\n", at ? "a bridge" : "an intro",
                   (double)a[0], (double)b[0], (double)a[1], (double)b[1], (double)(b[1] / a[1]));
            CHECK(a[0] == b[0] && (at ? en_section == EN_SEC_BRIDGE && fabsf(a[0]) > 0.3f : a[0] < -2.0f) &&
                      fabsf(b[1] / a[1] - want) < 1e-3f * want,
                  "test_fm_relative (%s): offset %f with the turn, %f without; cutoff x%f, the knob's x%f",
                  at ? "bridge" : "intro", (double)b[0], (double)a[0], (double)(b[1] / a[1]), (double)want);
        }
    }

    /* (BERLIN) the intro: the Sequence's Intro 16 (no wander, no Bridge, no Arc): 4 octaves under at least at the first
     * bar, open (within 0.01) from bar 16 */
    {
        uint32_t lastb = 0xFFFFFFFFu, b;
        int k, closed = 1, open = 1, seen16 = 0;
        fresh();
        en_set(P_TEMPO, 160);
        en_set(P_MI1, 16);
        en_set(P_SWEEP, 0);
        en_set(P_BRIDGE, 0);
        en_set(P_ARC, 0);
        en_set(P_MOTION, 0);
        en_play(1);
        for (k = 0; k < 400; k++) {
            run(0.1f);
            b = (en_bars - 1u) / 16u;            /* (the bar playing: en_bars counts the sixteenth just struck) */
            if (b == lastb)
                continue;
            lastb = b;
            if (b < 1u && en_fo[1] > -4.0f)
                closed = 0;
            if (b >= 16u)
                seen16 = 1, open &= fabsf(en_fo[1]) <= 0.01f;
        }
        CHECK(closed && open && seen16, "test_fm_intro: closed at first %d, open from bar 16 %d", closed, open);
    }

    /* (BERLIN) the Lead's cap: its Width 100, Rate 100, the quickest Time, Bridge 100, Motion 100, 30 minutes at 160:
     * its offset within +-3/4 octave (and near it at times) */
    {
        int k;
        float hi = 0.0f;
        fresh();
        en_set(P_TEMPO, 160);
        en_set(P_MW2, 100);
        en_set(P_MR2, 100);
        en_set(P_MT2, 0);
        en_set(P_BRIDGE, 100);
        en_set(P_MOTION, 100);
        en_play(1);
        for (k = 0; k < 7200; k++) {
            run(0.25f);
            hi = fmaxf(hi, fabsf(en_fo[2]));
        }
        printf("  30 min, the Lead at Width 100, Bridge 100: its offset at most %.3f octave\n", (double)hi);
        CHECK(hi <= 0.75f + 1e-4f && hi > 0.5f, "test_fm_lead_cap: the Lead's offset reached %f", (double)hi);
    }

    /* (BERLIN) divergence: every layer Width 50, Rate 50 (no Bridge, no Arc), ten minutes: in a fifth of the bars at
     * least, they are not all moving the same way */
    {
        uint32_t lastb = 0xFFFFFFFFu;
        int k, l, bars = 0, apart = 0;
        fresh();
        fo_all(50, -1, 50, 0);
        en_set(P_BRIDGE, 0);
        en_set(P_ARC, 0);
        en_set(P_MOTION, 0);
        en_play(1);
        for (k = 0; k < 2400; k++) {
            run(0.25f);
            if (en_bars / 16u == lastb)
                continue;
            lastb = en_bars / 16u;
            bars++;
            for (l = 1; l < 5; l++)
                if (en_fo_dir[l] != en_fo_dir[0]) {
                    apart++;
                    break;
                }
        }
        printf("  10 min, every layer Width 50, Rate 50: %d of %d bars not all moving alike\n", apart, bars);
        CHECK(bars > 100 && apart * 5 >= bars, "test_fm_diverge: %d of %d bars apart", apart, bars);
    }

    /* (BERLIN) a new start restarts the intros: Timewind mid-intro, Stratosfear, then Timewind again: its Sequence from
     * 4 octaves under at least; once open, no layer beyond its Width's range, the Bridge's and (the Sequence) the rise */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        int k, l, beyond = 0;
        float worst = 0.0f;
        fresh();
        piece(0);
        en_play(1);
        run(10.0f);
        piece(4);
        run(5.0f);
        piece(0);
        run(0.1f);
        CHECK(en_fo[1] <= -4.0f && en_section == EN_SEC_EMERGE, "test_fm_restart: Timewind again: the Sequence at %f "
              "(section %d)", (double)en_fo[1], en_section);
        en_piece(0, pp, &pl);
        run(100.0f);                              /* (past its 32 bars at 96) */
        for (k = 0; k < 1200; k++) {
            run(0.25f);
            for (l = 0; l < 5; l++) {
                const float lim = l == 2 ? 0.75f
                                  : 2.0f * (float)pp[FO_W[l]] * 0.01f + 3.0f * (float)pp[P_BRIDGE] * 0.01f + (l == 1 ? 1.0f : 0.0f);
                if (fabsf(en_fo[l]) > lim + 1e-4f)
                    beyond++, worst = fmaxf(worst, fabsf(en_fo[l]) - lim);
            }
        }
        CHECK(!beyond, "test_fm_restart: %d offsets beyond their range (by %f)", beyond, (double)worst);
    }

    /* (BERLIN, the final review) Rate turned up is heard within its own interval: at Rate 1 the next move is ~45+ bars
     * away; turned to 100, the Sequence moves again within 4 bars (100's longest wait 2.5). Intro turned up mid-piece
     * doesn't shut a filter that is already open (it is the start's: the next start takes it) */
    {
        float f0, most = 0.0f;
        int k;
        fresh();
        en_set(P_TEMPO, 120);
        en_set(P_MOTION, 0);
        en_set(P_EVOLVE, 0);
        en_set(P_ARC, 0);
        en_set(P_BRIDGE, 0);
        fo_all(0, 0, 0, 0);
        en_set(P_MW1, 100);
        en_set(P_MR1, 1);
        en_play(1);
        run(80.0f);                              /* (40 bars: the first move long done, the next not yet) */
        f0 = en_fo[1];
        en_set(P_MR1, 100);
        for (k = 0; k < 32; k++) {               /* (4 bars) */
            run(0.25f);
            most = fmaxf(most, fabsf(en_fo[1] - f0));
        }
        CHECK(most > 0.05f, "test_fm_rate_turn: Rate 1 -> 100, the Sequence moved %f octaves in 4 bars", (double)most);

        fresh();
        en_set(P_MOTION, 0);
        en_set(P_ARC, 0);
        en_set(P_BRIDGE, 0);
        fo_all(0, 0, 0, 0);
        en_play(1);
        run(10.0f);
        en_set(P_MI1, 32);
        run(1.0f);
        CHECK(en_fo[1] > -0.5f, "test_fm_intro_turn: Intro 0 -> 32 mid-piece shut the Sequence to %f", (double)en_fo[1]);
        en_set(P_MI1, 0);
    }

    /* (BERLIN) the clock: every layer Width 100, Time 10, Rate 100, Bridge 100, Motion 100, at 60 BPM, then 160 mid-move:
     * no offset changes by more than 0.05 octave from one 10 ms to the next; stopped, they stay still */
    {
        static int32_t b10[2 * 441];
        float last[5], jump = 0.0f, held[5];
        int k, l, still = 1;
        fresh();
        fo_all(100, 10, 100, 0);
        en_set(P_BRIDGE, 100);
        en_set(P_MOTION, 100);
        en_set(P_TEMPO, 60);
        en_play(1);
        run(20.0f);
        for (l = 0; l < 5; l++)
            last[l] = en_fo[l];
        for (k = 0; k < 9000; k++) {             /* (90 s, the tempo changed after 2) */
            if (k == 200)
                en_set(P_TEMPO, 160);
            en_render(b10, 441, 2048);
            for (l = 0; l < 5; l++) {
                jump = fmaxf(jump, fabsf(en_fo[l] - last[l]));
                last[l] = en_fo[l];
            }
        }
        en_play(0);
        run(0.1f);
        for (l = 0; l < 5; l++)
            held[l] = en_fo[l];
        run(10.0f);
        for (l = 0; l < 5; l++)
            still &= en_fo[l] == held[l];
        printf("  the clock: 60 -> 160 BPM, an offset's step at most %.4f octave in 10 ms\n", (double)jump);
        CHECK(jump <= 0.05f && jump > 0.0f, "test_fm_clock: a step of %f octave in 10 ms", (double)jump);
        CHECK(still, "test_fm_clock: stopped, the offsets moved");
    }

    /* (BERLIN) no ending (spec 3.5): the shortest Arc (134 bars), two hours at 160: never Emerge again once it has left
     * it, the Sequence playing in every ten minutes, its long rise repeating (its peak, +1 octave, reached again and
     * again: no wander, no Bridge) */
    {
        uint32_t ons = 0;
        int k, left = 0, back = 0, silent = 0, peaks = 0, high = 0;
        fresh();
        en_set(P_TEMPO, 160);
        en_set(P_ARC, 1);
        en_set(P_MI1, 8);
        en_set(P_SWEEP, 0);
        en_set(P_BRIDGE, 0);
        en_set(P_MOTION, 70);
        en_play(1);
        ch2_ons = 0;
        for (k = 0; k < 7200; k++) {
            run(1.0f);
            if (en_section != EN_SEC_EMERGE)
                left = 1;
            else if (left)
                back = 1;
            if (en_fo[1] > 0.9f && !high)
                peaks++;
            high = en_fo[1] > 0.9f ? 1 : en_fo[1] < 0.0f ? 0 : high;
            if (k % 600 == 599) {
                silent += ch2_ons == ons;
                ons = ch2_ons;
            }
        }
        printf("  2 h at the shortest Arc: Emerge again %d, ten minutes without the Sequence %d, %d peaks\n", back, silent,
               peaks);
        CHECK(left && !back && !silent && peaks >= 30, "test_no_ending: Emerge again %d, silent windows %d, peaks %d", back,
              silent, peaks);
    }

    /* (BERLIN) the eight pieces: their names; no two alike (at least 4 of Tempo, Root, Mode, the Sequence's voice, the
     * Drone's, the Lead's, Sequence 2 on, Repeat B on, the Atmosphere differ: the "samey" anti-goal); not every piece
     * has an Atmosphere (three at least None, the user); the Lead dreamy in all of them (its reverb and echo 90) */
    {
        static const char *const NAME[EN_NPIECES] = {"Timewind", "Wahnfried", "Phaedra", "Rubycon", "Stratosfear",
                                                     "Sunrain", "Mirage", "Cyborg"};
        static const int KEY[] = {P_TEMPO, P_ROOT, P_MODE, P_SVOICE, P_CVOICE, P_LVOICE, P_NCHAR, P_MI1, P_MI3};
        int16_t pa[EN_NPIECES][P_NPARAMS];
        uint16_t pl;
        int a, b, k, none = 0, worst = 99, wa = 0, wb = 0, dreamy = 1;
        for (a = 0; a < EN_NPIECES; a++) {
            en_piece(a, pa[a], &pl);
            CHECK(!strcmp(en_piece_name(a), NAME[a]), "test_pieces_names: piece %d is %s", a, en_piece_name(a));
            none += pa[a][P_NCHAR] == 0;
            dreamy &= pa[a][P_LDREV] == 90 && pa[a][P_LDECHO] == 90;
        }
        for (a = 0; a < EN_NPIECES; a++)
            for (b = a + 1; b < EN_NPIECES; b++) {
                int d = (pa[a][P_SEQ2] > 0) != (pa[b][P_SEQ2] > 0);
                d += (pa[a][P_TAPB] > 0) != (pa[b][P_TAPB] > 0);
                for (k = 0; k < (int)(sizeof KEY / sizeof KEY[0]); k++)
                    d += pa[a][KEY[k]] != pa[b][KEY[k]];
                if (d < worst)
                    worst = d, wa = a, wb = b;
            }
        CHECK(worst >= 4, "test_pieces_distinct: %s and %s differ in only %d", en_piece_name(wa), en_piece_name(wb),
              worst);
        CHECK(none >= 3, "test_pieces_atmos_none: %d pieces without an Atmosphere", none);
        CHECK(dreamy, "test_pieces_lead_dreamy: a piece's Lead reverb or echo not 90");
    }

    /* (BERLIN, filter motion spec 9) the pieces' moves: every piece moves some layer (a Width and a Rate above 0);
     * Timewind's Sequence comes in slowly (Intro 24+), Stratosfear's and Sunrain's straight in; Rubycon's Sequence 2
     * after its Sequence; Wahnfried and Cyborg (no Sequence 2) move on the Drone (Width and Rate 40+); Timewind's Bridge
     * moderate (30..60); the Lead's Width low everywhere (30 at most). The echo's default time 1/4. and Repeat A's
     * elsewhere (1/8.), so A no longer lands on the echo's first repeat (the user; the echo report) */
    {
        static const int MW[5] = {P_MW0, P_MW1, P_MW2, P_MW3, P_MW4}, MR[5] = {P_MR0, P_MR1, P_MR2, P_MR3, P_MR4};
        int16_t pa[EN_NPIECES][P_NPARAMS];
        uint16_t pl;
        int a, l, moves;
        for (a = 0; a < EN_NPIECES; a++) {
            en_piece(a, pa[a], &pl);
            for (moves = 0, l = 0; l < 5; l++)
                moves += pa[a][MW[l]] > 0 && pa[a][MR[l]] > 0;
            CHECK(moves, "test_pieces_move: %s moves no layer", en_piece_name(a));
            CHECK(pa[a][P_MW2] <= 30, "test_pieces_move: %s Lead width %d", en_piece_name(a), pa[a][P_MW2]);
        }
        CHECK(pa[0][P_MI1] >= 24, "test_pieces_move: Timewind Seq intro %d", pa[0][P_MI1]);
        CHECK(pa[0][P_SEQ2] == 60 && pa[0][P_REVERB] == 50, "test_pieces_timewind: Seq 2 %d, Reverb %d (the user: 60, 50)",
              pa[0][P_SEQ2], pa[0][P_REVERB]);
        CHECK(pa[0][P_BRIDGE] >= 30 && pa[0][P_BRIDGE] <= 60, "test_pieces_move: Timewind Bridge %d", pa[0][P_BRIDGE]);
        CHECK(!pa[4][P_MI1] && !pa[4][P_MI3] && !pa[5][P_MI1] && !pa[5][P_MI3],
              "test_pieces_move: Stratosfear/Sunrain intros %d %d / %d %d", pa[4][P_MI1], pa[4][P_MI3], pa[5][P_MI1],
              pa[5][P_MI3]);
        CHECK(pa[3][P_MI3] > pa[3][P_MI1], "test_pieces_move: Rubycon Seq2 intro %d, Seq %d", pa[3][P_MI3], pa[3][P_MI1]);
        /* (the user, 2026-10-28) the lengths, Sequence / Sequence 2: Sunrain 8/7 ("works best"), Timewind 16/15, Mirage
         * 12/11; Rubycon's octaves, Sequence -2, Sequence 2 0; Cyborg's Sequence at 20, Sequence 2 still silent */
        CHECK(pa[5][P_SEQLEN] == 8 && pa[5][P_SEQ2LEN] == 7, "test_pieces_lengths: Sunrain %d/%d", pa[5][P_SEQLEN],
              pa[5][P_SEQ2LEN]);
        CHECK(pa[0][P_SEQLEN] == 16 && pa[0][P_SEQ2LEN] == 15, "test_pieces_lengths: Timewind %d/%d", pa[0][P_SEQLEN],
              pa[0][P_SEQ2LEN]);
        CHECK(pa[6][P_SEQLEN] == 12 && pa[6][P_SEQ2LEN] == 11, "test_pieces_lengths: Mirage %d/%d", pa[6][P_SEQLEN],
              pa[6][P_SEQ2LEN]);
        CHECK(pa[3][P_SEQOCT] == -2 && pa[3][P_SEQ2OCT] == 0, "test_pieces_octaves: Rubycon %d/%d", pa[3][P_SEQOCT],
              pa[3][P_SEQ2OCT]);
        CHECK(pa[7][P_SEQ] == 20 && pa[7][P_SEQ2] == 0, "test_pieces_cyborg: Sequence %d, Sequence 2 %d", pa[7][P_SEQ],
              pa[7][P_SEQ2]);
        for (a = 1; a < EN_NPIECES; a += 6)
            CHECK(pa[a][P_MW0] >= 40 && pa[a][P_MR0] >= 40, "test_pieces_move: %s Drone width %d rate %d",
                  en_piece_name(a), pa[a][P_MW0], pa[a][P_MR0]);
        CHECK(en_param_info(P_ETIME)->def == 3 && en_param_info(P_TAPAT)->def == 1,
              "test_echo_defaults: Echo time %d, Repeat A time %d", en_param_info(P_ETIME)->def,
              en_param_info(P_TAPAT)->def);
    }

    /* (BERLIN) every piece, three minutes from its start: nothing broken, below clipping (-0.5 dBFS) and heard (RMS
     * above -40 dBFS) */
    {
        int i;
        for (i = 0; i < EN_NPIECES; i++) {
            float rms;
            fresh();
            piece(i);
            en_play(1);
            peak = 0.0f, sum2 = 0.0f, bad = 0;
            run(180.0f);
            rms = sqrtf(sum2 / (180.0f * 44100.0f * 2.0f));
            printf("  %-11s 3 min: peak %.3f (%.1f dBFS), rms %.4f (%.1f dBFS)\n", en_piece_name(i), (double)peak,
                   20.0 * log10((double)peak + 1e-9), (double)rms, 20.0 * log10((double)rms + 1e-9));
            CHECK(!bad && peak < 0.944f && rms > 0.01f, "test_pieces_render: %s: bad %d, peak %.3f, rms %.4f",
                  en_piece_name(i), bad, (double)peak, (double)rms);
        }
    }

    /* (BERLIN) Length moves the return point and keeps the notes (the review): 8 -> 9 -> 8, the eight steps as they
     * were; 8 -> 12 -> 8 too, and at 12 the new steps grown (a note at least among them) */
    {
        uint8_t b0[EN_SEQ_N], b1[EN_SEQ_N], b2[EN_SEQ_N];
        int i, same = 1, same2 = 1;
        fresh();
        en_set(P_MOTION, 0);
        en_set(P_EVOLVE, 0);
        en_set(P_SEQLEN, 8);
        en_regrow();
        en_play(1);
        run(0.5f);
        en_seq_get(b0);
        en_set(P_SEQLEN, 9);
        run(0.05f);
        en_set(P_SEQLEN, 8);
        run(0.05f);
        en_seq_get(b1);
        for (i = 0; i < 8; i++)
            same &= b0[i] == b1[i];
        same &= (b0[32] == b1[32]) && b0[36] == b1[36] && b0[40] == b1[40];
        CHECK(same, "test_length_keeps_notes: 8 -> 9 -> 8 changed the pattern");
        en_set(P_SEQLEN, 12);
        run(0.05f);
        en_seq_get(b2);
        CHECK((b2[32] | b2[33] << 8) >> 8 & 0xFu, "test_length_keeps_notes: at 12, steps 9-12 all rests (%02x%02x)",
              b2[33], b2[32]);
        en_set(P_SEQLEN, 8);
        run(0.05f);
        en_seq_get(b1);
        for (i = 0; i < 8; i++)
            same2 &= b0[i] == b1[i];
        same2 &= b0[32] == b1[32];
        CHECK(same2, "test_length_keeps_notes: 8 -> 12 -> 8 changed the pattern");
    }

    /* (BERLIN) PANIC frees a Lead key held (its release lost): the note ends, and the Lead plays its phrases again */
    {
        uint32_t before;
        fresh();
        en_set(P_LDIN, 0);
        en_set(P_MOTION, 0);
        en_set(P_LDENS, 100);
        en_play(1);
        run(1.0f);
        en_lead_key(3, 1);
        run(1.0f);
        en_panic();
        run(0.5f);
        CHECK(!ld_on_now, "test_panic_frees_lead_key: the held note still on after PANIC");
        before = ld_ons;
        run(90.0f);
        CHECK(ld_ons > before, "test_panic_frees_lead_key: no Lead note in 90 s after PANIC");
    }

    /* (BERLIN) a piece changed while playing starts its own arc: Timewind (a long intro: Emerge) playing, then
     * Stratosfear (no intro): in Flow within two bars; and back to Timewind: in Emerge */
    {
        fresh();
        piece(0);
        en_play(1);
        run(10.0f);
        CHECK(en_section == EN_SEC_EMERGE, "test_piece_change_arc: Timewind not in Emerge (%d)", en_section);
        piece(4);
        run(2.0f);
        CHECK(en_section == EN_SEC_FLOW, "test_piece_change_arc: Stratosfear in section %d, not Flow", en_section);
        run(10.0f);
        piece(0);
        run(2.0f);
        CHECK(en_section == EN_SEC_EMERGE, "test_piece_change_arc: back to Timewind, section %d, not Emerge",
              en_section);
    }

    /* (BERLIN) Timewind plays both sequences (the user: Sequence 2 at 60): ten minutes, notes from each */
    {
        fresh();
        piece(0);
        en_play(1);
        run(600.0f);
        CHECK(s2_n > 0 && sq_n > 0, "test_timewind_sequences: %u Sequence 2 notes (%u Sequence)", s2_n, sq_n);
    }

    /* (the user, 2026-10-28) Motion moves only the Sequence's return point: Sunrain (8/7) at Motion 100, ten minutes,
     * Sequence 2 always 7 steps (no drift to 8/6, 8/5), the Sequence's length moving */
    {
        int k, s2_off = 0, s1_moved = 0;
        fresh();
        piece(5);
        en_set(P_MOTION, 100);
        en_play(1);
        for (k = 0; k < 1200; k++) {
            run(0.5f);
            s2_off += en_seq_len[1] != 7;
            s1_moved += en_seq_len[0] != 8;
        }
        CHECK(!s2_off && s1_moved, "test_seq2_length: Sequence 2 off its 7 in %d of 1200 looks; the Sequence's moved in %d",
              s2_off, s1_moved);
    }

    /* (the user, 2026-10-28) Sequence 2's octave moves with Motion: at each section change (not Emerge's start), with
     * Motion/100, one octave from its home, up or down (the other way at the range's edge), back home at the next;
     * never two away; Motion 0 never; the hand (Octave 2 set) cancels it at once. Sunrain (Sequence 2 at 0): Motion
     * 0 for ten minutes, never; Motion 100 for fifteen, shifts that come and go at the section changes, its notes an
     * octave away while shifted; home at the top (1) only down, at the bottom (-2) only up */
    {
        int k, sh, was = 0, sec, was_sec, ever = 0, back = 0, bad = 0, stuck = 0, mixed;
        uint32_t n0;
        long sum[3] = {0, 0, 0}, cnt[3] = {0, 0, 0};
        fresh();
        piece(5);
        en_set(P_MOTION, 0);
        en_play(1);
        for (k = 0; k < 1200; k++) {
            run(0.5f);
            ever += en_s2oct_shift != 0;
        }
        CHECK(!ever, "test_s2oct_motion0: shifted in %d of 1200 looks at Motion 0", ever);
        fresh();
        piece(5);
        en_set(P_MOTION, 100);
        en_play(1);
        was_sec = en_section;
        for (k = 0; k < 1800; k++) {
            n0 = s2_n;
            run(0.5f);
            sh = en_s2oct_shift;
            sec = en_section;
            bad += sh < -1 || sh > 1;
            if (sec != was_sec && was != 0)
                stuck += sh != 0;        /* (shifted, the next section change brings it home) */
            if (sec == was_sec && sh != was)
                bad++;                   /* (it moves only at a section change) */
            ever += sh != 0 && was == 0;
            back += sh == 0 && was != 0;
            mixed = sh != was;
            if (!mixed && sh >= -1 && sh <= 1)
                for (; n0 < s2_n && n0 < SQN; n0++)
                    sum[sh + 1] += s2_note[n0], cnt[sh + 1]++;
            was = sh, was_sec = sec;
        }
        printf("  Sequence 2's octave at Motion 100, 15 min: %d shifts, %d back; mean notes %.1f / %.1f / %.1f\n", ever,
               back, cnt[0] ? (double)sum[0] / (double)cnt[0] : 0.0, cnt[1] ? (double)sum[1] / (double)cnt[1] : 0.0,
               cnt[2] ? (double)sum[2] / (double)cnt[2] : 0.0);
        CHECK(ever >= 3 && back >= 3 && !bad && !stuck, "test_s2oct_motion100: %d shifts, %d back, %d bad, %d not home",
              ever, back, bad, stuck);
        for (k = 0; k < 3; k += 2)       /* (a shift's notes an octave from home's, give or take the patterns) */
            if (cnt[k] > 20 && cnt[1] > 20) {
                const double d = (double)sum[k] / (double)cnt[k] - (double)sum[1] / (double)cnt[1];
                CHECK(k ? d > 8.0 && d < 16.0 : d < -8.0 && d > -16.0, "test_s2oct_notes: shift %d moved the notes %.1f",
                      k - 1, d);
            }
        for (k = 0; k < 1200 && !en_s2oct_shift; k++)   /* (the hand: Octave 2 set while shifted, home at once) */
            run(0.5f);
        sh = en_s2oct_shift;
        en_set(P_SEQ2OCT, 0);
        run(0.05f);
        CHECK(sh && !en_s2oct_shift, "test_s2oct_hand: shifted %d, then %d after Octave 2 set", sh, en_s2oct_shift);
        {   /* (the edges: home 1 only down, home -2 only up) */
            static const int HOME[2] = {1, -2};
            int e;
            for (e = 0; e < 2; e++) {
                int up = 0, down = 0;
                en_set(P_SEQ2OCT, HOME[e]);
                for (k = 0; k < 900; k++) {
                    run(0.5f);
                    up += en_s2oct_shift > 0, down += en_s2oct_shift < 0;
                }
                CHECK(e ? !down && up : !up && down, "test_s2oct_edge: home %d: up %d, down %d", HOME[e], up, down);
            }
        }
    }

    /* (BERLIN) Wahnfried is beatless: ten minutes, no note of either sequence; Cyborg (the user, 2026-10-28: its
     * Sequence at 20) plays its Sequence, Sequence 2 silent */
    {
        static const int BEATLESS[2] = {1, 7};
        int k;
        for (k = 0; k < 2; k++) {
            fresh();
            piece(BEATLESS[k]);
            en_play(1);
            run(600.0f);
            if (BEATLESS[k] == 7)
                CHECK(sq_n > 0 && s2_n == 0, "test_cyborg_sequence: %u Sequence notes, %u Sequence 2", sq_n, s2_n);
            else
                CHECK(sq_n == 0 && s2_n == 0, "test_beatless: %s: %u Sequence notes, %u Sequence 2",
                      en_piece_name(BEATLESS[k]), sq_n, s2_n);
        }
    }

    /* (BERLIN) every note-on has its note-off: two minutes of piece changes, tempo changes, freeze, mutes, PANIC and
     * stops with repeats pending, then stop and let it ring out */
    {
        int c, nt, k, stuck = 0;
        fresh();
        memset(sounding, 0, sizeof sounding);
        en_set(P_TAPA, 100);
        en_set(P_TAPB, 100);
        en_play(1);
        for (k = 0; k < 40; k++) {
            switch (k % 8) {
            case 0: piece((k / 8) % EN_NPIECES); break;
            case 1: en_set(P_TEMPO, 60 + (k * 37) % 101); break;
            case 2: en_freeze(1); break;
            case 3: en_freeze(0); break;
            case 4: en_mute(EN_M_PINGS); break;
            case 5: en_mute(0); break;
            case 6: en_panic(); break;
            default: en_play(0), run(0.5f), en_play(1); break;
            }
            run(3.0f);
        }
        en_play(0);
        run(10.0f);
        for (c = 0; c < 16; c++)
            for (nt = 0; nt < 128; nt++)
                if (sounding[c][nt] > 0 || ((c == 1 || c == 3) && sounding[c][nt])) {   /* (an extra note-off is
                                                  * harmless, but the sequences' must pair exactly) */
                    if (!stuck)
                        printf("  stuck: channel %d note %d (%d)\n", c + 1, nt, sounding[c][nt]);
                    stuck++;
                }
        CHECK(!stuck, "test_no_stuck_notes: %d notes left sounding", stuck);
    }

    /* stop: the tails die away (but the vinyl turns on) */
    fresh();
    en_play(1);
    run(20.0f);
    en_play(0);
    run(30.0f);
    peak = 0.0f;
    run(1.0f);
    printf("  30 s after stop, crackle on: %.4f\n", (double)peak);
    CHECK(peak < 0.05f, "quiet after stopping, crackle on (%f)", (double)peak);
    en_set(P_CRACKLE, 0);
    run(1.0f);
    peak = 0.0f;
    run(1.0f);
    CHECK(peak < 0.01f, "quiet after stopping (%f)", (double)peak);
    CHECK(midi_count > 50u, "MIDI out (%u)", midi_count);

    printf(fails ? "amb: %d FAILED\n" : "amb: ok\n", fails);
    return fails != 0;
}
