/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument on the host: the pure parts (Euclid, scales, chords, voicings, grooves, names, texts) and
 * the render (sound, no NaN, bounded; the kick on the floor, the chord drifts and holds, never on a
 * diminished fifth, freeze holds, the arrangement moves with Motion, throws, the loops evolve). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
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
static uint8_t sq_note[SQN], sq_vel[SQN], s2_note[SQN];
static int sounding[16][128];
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t b = en_bars / 16u;
    midi_count++;
    if ((st & 0xF0u) == 0x90u && d2 > 0u) {
        sounding[st & 15u][d1 & 127u]++;
        if ((st & 15u) >= 9u)
            drum_ch_ons++;
        if ((st & 15u) == 1u && sq_n < SQN)
            sq_at[sq_n] = en_bars, sq_note[sq_n] = (uint8_t)d1, sq_vel[sq_n] = (uint8_t)d2, sq_n++;
        if ((st & 15u) == 3u && s2_n < SQN)
            s2_at[s2_n] = en_bars, s2_note[s2_n] = (uint8_t)d1, s2_n++;
    } else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u)
        sounding[st & 15u][d1 & 127u]--;
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

/* a new engine, stopped, the count from the top (en_init leaves the transport, en_bars, as they were) */

static void fresh(void)
{
    en_play(0);
    run(0.01f);
    en_init();
    en_bars = 0;
    bad = 0;
    memset(kick_bar, 0, sizeof kick_bar);
    sq_n = s2_n = 0;
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

    /* grooves, drift */
    CHECK(en_groove(0) == 1u, "Drone: the downbeat");
    for (i = 0; i < 5; i++)
        CHECK(en_groove(i) & 0xFFFFu && !(en_groove(i) >> 16), "groove %d: %x", i, en_groove(i));
    CHECK(en_groove(1) == 0x4444u, "Offbeat: %x", en_groove(1));
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
        CHECK(!strcmp(t, "Saws"), "ctone 0: %s", t);
        en_param_text(P_CVOICE, 1, t);
        CHECK(!strcmp(t, "Keys"), "ctone 1: %s", t);
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

    /* Evolve 0, Motion 0: a strict loop, every stab every bar */
    {
        uint32_t first = 0, t0;
        int k, diff = 0;
        fresh();
        en_set(P_EVOLVE, 0);
        en_set(P_MOTION, 0);
        en_set(P_DRIFT, 6);
        en_regrow();
        en_play(1);
        run(2.5f);
        first = en_stab_pat;
        t0 = en_throws;
        for (k = 0; k < 60; k++) {
            run(2.0f);
            diff += en_stab_pat != first;
        }
        CHECK(first && !diff && en_throws == t0, "Evolve 0: the stabs changed (%d of 60, %u throws)", diff, en_throws - t0);
    }

    /* Throws: 0 none (Motion 0: no fills), 100 well over 50 (the same draws) */
    {
        uint32_t n[3];
        int k;
        static const int TV[3] = {0, 50, 100};
        for (k = 0; k < 3; k++) {
            fresh();
            en_seed(7);
            en_set(P_EVOLVE, 100);
            en_set(P_MOTION, 0);
            en_set(P_CTHROW, TV[k]);
            en_regrow();
            en_play(1);
            n[k] = en_throws;
            run(60.0f);
            n[k] = en_throws - n[k];
        }
        printf("  throws in a minute at Throws 0 / 50 / 100: %u / %u / %u\n", n[0], n[1], n[2]);
        CHECK(n[0] == 0 && n[1] > 0 && n[2] * 2u > n[1] * 3u, "Throws: %u / %u / %u", n[0], n[1], n[2]);
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
        en_piece(7, pp, &pl);                    /* (Haze: Dust, the cloud, the widest load on the FM-1) */
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

    /* every voice of every layer: it sounds, without NaN, under full scale */
    {
        static const int VP[3] = {P_CVOICE, P_SVOICE, P_BVOICE}, LV[3] = {P_CHORD, P_SEQ, P_BASS};
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

    /* the arrangement: at Motion 100, it leaves Full now and then; at Motion 0, never */
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
            printf("  10 min at Motion %d: Full %d s, Break %d, Groove %d, Thin %d\n", MOT[m], secs[SEC_FULL],
                   secs[SEC_BREAK], secs[SEC_GROOVE], secs[SEC_THIN]);
            if (MOT[m])
                CHECK(secs[SEC_FULL] < 500 && secs[SEC_FULL] > 150 && secs[SEC_BREAK] > 0,
                      "Motion 100: Full %d s, Break %d s", secs[SEC_FULL], secs[SEC_BREAK]);
            else
                CHECK(secs[SEC_FULL] == 600, "Motion 0 left Full (%d s Full)", secs[SEC_FULL]);
        }
    }

    /* throws: with Evolve high, stabs go into the echo */
    {
        uint32_t t0, lo, hi;
        fresh();
        en_set(P_EVOLVE, 0);
        en_set(P_MOTION, 0);
        en_play(1);
        t0 = en_throws;
        run(120.0f);
        lo = en_throws - t0;
        fresh();
        en_set(P_EVOLVE, 100);
        en_play(1);
        t0 = en_throws;
        run(120.0f);
        hi = en_throws - t0;
        printf("  throws in 2 min: %u at Evolve 0, %u at Evolve 100\n", lo, hi);
        CHECK(hi >= 3u && lo == 0u, "throws: %u at Evolve 100, %u at 0", hi, lo);   /* (fewer fills since the classics pass: a fill throws its last stab) */
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
        CHECK(en_bars / 16u >= 63u && sq_n >= 4u * 63u && !off, "test_pattern_mode: %u of %u off the written steps (%u bars)",
              off, sq_n, en_bars / 16u);
        en_set(P_SMODE, 0);
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
