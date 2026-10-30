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
 * chord's root (pitch class) as the note began; the phrase's motif slot (0: the theme), its block's count and the
 * intensity it was planned at (0..100) */
#define LGN 16384
static struct {
    uint32_t on, off;
    uint8_t note, sec, phr, role, croot, slot, blk, lint;
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
            lg[lg_n].slot = en_lead_slot, lg[lg_n].blk = en_lead_blk, lg[lg_n].lint = en_lead_int;
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

/* (Shimmer) secs played, the output (mono) through four band-passes (each two RBJ band-passes in a row, Q 6: a
 * shifted tone's crossfade sidebands, +-20 Hz, inside) at f[0..3]: each one's mean energy into e; the biggest step
 * between two samples (either side) into *step */
static void run_bands(float secs, const float *f, float *e, float *step)
{
    static float z[4][2][2], last[2];
    float c[4][3], acc[4] = {0.0f, 0.0f, 0.0f, 0.0f}, st = 0.0f;
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j, k, s, cnt = 0;
    memset(z, 0, sizeof z);
    for (k = 0; k < 4; k++) {
        const float w = 6.2831853f * f[k] / 44100.0f, al = sinf(w) / (2.0f * 6.0f), a0 = 1.0f + al;
        c[k][0] = al / a0;                       /* b0 (b1 0, b2 -b0) */
        c[k][1] = -2.0f * cosf(w) / a0;          /* a1 */
        c[k][2] = (1.0f - al) / a0;              /* a2 */
    }
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        bad += !en_state_ok();                   /* (the float state: the output, an int, can't show a NaN) */
        for (j = 0; j < 256; j++) {
            const float l = (float)buf[2 * j] / 8388608.0f, r = (float)buf[2 * j + 1] / 8388608.0f;
            float x = 0.5f * (l + r);
            if (cnt++)
                st = fmaxf(st, fmaxf(fabsf(l - last[0]), fabsf(r - last[1])));
            last[0] = l, last[1] = r;
            for (k = 0; k < 4; k++) {
                float y = x;
                for (s = 0; s < 2; s++) {        /* (transposed direct form II) */
                    const float o = c[k][0] * y + z[k][s][0];
                    z[k][s][0] = -c[k][1] * o + z[k][s][1];
                    z[k][s][1] = -c[k][0] * y - c[k][2] * o;
                    y = o;
                }
                acc[k] += y * y;
            }
        }
    }
    for (k = 0; k < 4; k++)
        e[k] = acc[k] / (float)(cnt > 0 ? cnt : 1);
    *step = st;
}

/* (Shimmer, fix round 1) secs played: the output's level (dB of its mean square), its peak, the share of samples over
 * 0.9; the engine's float state checked every block (en_state_ok: no NaN or Inf in the plate or the shifter) */
static void run_lvl(float secs, float *db, float *pk, float *hot)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    float s = 0.0f, p = 0.0f;
    long n = 0, h = 0;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        bad += !en_state_ok();
        for (j = 0; j < 512; j++) {
            const float x = fabsf((float)buf[j] / 8388608.0f);
            p = fmaxf(p, x);
            h += x > 0.9f;
            s += x * x;
            n++;
        }
    }
    *db = 10.0f * log10f(s / (float)(n > 0 ? n : 1) + 1e-30f);
    *pk = p;
    *hot = (float)h / (float)(n > 0 ? n : 1);
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

/* (Shimmer, fix round 2) a piece (Size and Reverb 100 if big) played 20 s; then 60 s from that one state three ways
 * (two forked children streaming their output back): not held, held, and held with nothing returned (en_shm_ret 0).
 * Out: the 5 s windows' most held over not held (dB), all 60 s's (dB), the held peak and share over 0.9, the shimmer's
 * own sound
 * (held less held-with-nothing-returned: its climbing octaves) in dB re the held mix; then let go, 4 s against the
 * same 4 s not held: its 2 s windows' most over not held (dB), its peak and not held's */
typedef struct {
    float over, all, pk, hot, own, rel_over, rel_pk, rel_npk;
} shm_ab_t;
static pid_t shm_child(int fx, float ret, int fd, float secs)
{
    pid_t pid = fork();
    if (pid == 0) {
        int b;
        en_shm_ret = ret;
        if (fx)
            en_fx(1u << EN_FX_SHIMMER);
        for (b = 0; b < (int)(secs * 44100.0f / 256.0f); b++) {
            en_render(buf, 256, 2048);
            if (write(fd, buf, sizeof buf) != (ssize_t)sizeof buf)
                _exit(1);
        }
        _exit(0);
    }
    return pid;
}
static int shm_read(int fd, int32_t *to)
{
    size_t got = 0;
    while (got < sizeof buf) {
        const ssize_t r = read(fd, (char *)to + got, sizeof buf - got);
        if (r <= 0)
            return 0;
        got += (size_t)r;
    }
    return 1;
}
static void shm_ab(int pc, int big, shm_ab_t *o)
{
    static int32_t nb[512], zb[512];
    const int W = (int)(5.0f * 44100.0f / 256.0f);
    int fn[2], fz[2], k, b, j, ok = 1;
    pid_t p1, p2;
    double sd = 0.0, sh = 0.0, sna = 0.0;
    fresh();
    piece(pc);
    if (big)
        en_set(P_SIZE, 100), en_set(P_REVERB, 100);
    en_play(1);
    run(20.0f);
    o->over = -200.0f, o->pk = o->hot = 0.0f;
    fflush(stdout);
    if (pipe(fn) || pipe(fz))
        return;
    p1 = shm_child(0, 1.0f, fn[1], 64.1f);
    p2 = shm_child(1, 0.0f, fz[1], 60.0f);
    close(fn[1]);
    close(fz[1]);
    en_fx(1u << EN_FX_SHIMMER);
    for (k = 0; k < 12 && ok; k++) {
        double s = 0.0, sn = 0.0;
        long h = 0;
        for (b = 0; b < W && ok; b++) {
            en_render(buf, 256, 2048);
            bad += !en_state_ok();
            ok = shm_read(fn[0], nb) && shm_read(fz[0], zb);
            for (j = 0; j < 512; j++) {
                const float x = (float)buf[j] / 8388608.0f, y = (float)zb[j] / 8388608.0f, z = (float)nb[j] / 8388608.0f;
                s += (double)(x * x), sn += (double)(z * z), sh += (double)(x * x), sd += (double)((x - y) * (x - y));
                o->pk = fmaxf(o->pk, fabsf(x));
                h += fabsf(x) > 0.9f;
            }
        }
        o->over = fmaxf(o->over, (float)(10.0 * log10((s + 1e-30) / (sn + 1e-30))));
        sna += sn;
        o->hot = fmaxf(o->hot, (float)h / (float)(W * 512));
    }
    close(fz[0]);
    waitpid(p2, 0, 0);
    o->own = ok ? (float)(10.0 * log10(sd / (sh + 1e-30) + 1e-30)) : 0.0f;
    o->all = (float)(10.0 * log10((sh + 1e-30) / (sna + 1e-30)));
    en_fx(0);
    o->rel_over = -200.0f, o->rel_pk = o->rel_npk = 0.0f;
    for (k = 0; k < 2 && ok; k++) {
        double s = 0.0, sn = 0.0;
        for (b = 0; b < (int)(2.0f * 44100.0f / 256.0f) && ok; b++) {
            en_render(buf, 256, 2048);
            bad += !en_state_ok();
            ok = shm_read(fn[0], nb);
            for (j = 0; j < 512; j++) {
                const float x = (float)buf[j] / 8388608.0f, z = (float)nb[j] / 8388608.0f;
                s += (double)(x * x), sn += (double)(z * z);
                o->rel_pk = fmaxf(o->rel_pk, fabsf(x)), o->rel_npk = fmaxf(o->rel_npk, fabsf(z));
            }
        }
        o->rel_over = fmaxf(o->rel_over, (float)(10.0 * log10((s + 1e-30) / (sn + 1e-30))));
    }
    close(fn[0]);
    waitpid(p1, 0, 0);
    if (!ok)
        bad++;
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
 * back; and how often a pair's call recurs (spec 7.2): its (degree interval, onset gap, last note's length) string the
 * same as one of the 4 calls before it's, exactly or transposed (answers not counted: their endings differ by design;
 * nor a sentence's phrases: not pairs) */
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
static int lg_body(uint32_t a, uint32_t b)       /* the phrases from entries a and b: the same notes but the last
                                                  * (whose ending is drawn again each pass), transposed, the same rhythm */
{
    for (;; a++, b++) {
        if (lg_last(a) != lg_last(b))
            return 0;
        if (lg_last(a))
            return 1;
        if (lg[a + 1].on - lg[a].on != lg[b + 1].on - lg[b].on ||
            (!lg_last(a + 1) && lg_deg(lg[a + 1].note) - lg_deg(lg[a].note) != lg_deg(lg[b + 1].note) - lg_deg(lg[b].note)))
            return 0;
    }
}
static void lg_numbers(lm_t *m)                  /* (the numbers, from the log as it is) */
{
    uint32_t i, steps = 0, turns = 0, recur = 0, starts[4] = {0, 0, 0, 0}, ns = 0;
    memset(m, 0, sizeof *m);
    m->notes = lg_n;
    for (i = 0; i < lg_n; i++) {
        m->phrases += (uint32_t)lg_first(i);
        if (lg_first(i) && lg[i].role == LR_CALL) {   /* a pair's call: the same as one of the 4 before it? */
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
static void lead_metrics(float secs, lm_t *m)
{
    lg_n = 0, lg_open = -1;
    run(secs);
    lg_numbers(m);
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
static uint32_t lt_leaps, lt_turns;              /* (test_motif_leap_turn over three runs: leaps, turns after them) */
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
/* from the log: the breath inside each pair (a call's last note-off to its answer's first note-on, in sixteenths: the
 * same count at every tempo), the shortest, the longest and the mean over the pairs counted; calls whose next phrase
 * isn't their answer counted apart (a call still sounding as the log ends, or its answer not begun, not counted) */
typedef struct {
    uint32_t n, lo, hi, sum, unanswered;
} br_t;
static void lg_breath(br_t *b)
{
    uint32_t i, j;
    memset(b, 0, sizeof *b);
    for (i = 0; i < lg_n; i++) {
        if (!lg_first(i) || lg[i].role != LR_CALL)
            continue;
        for (j = i; !lg_last(j); j++)
            ;
        if (j + 1 >= lg_n || lg[j].off == 0xFFFFFFFFu)
            continue;
        if (lg[j + 1].role != LR_ANSWER) {
            b->unanswered++;
            continue;
        }
        {
            const uint32_t g = lg[j + 1].on - lg[j].off;
            b->lo = !b->n || g < b->lo ? g : b->lo;
            b->hi = g > b->hi ? g : b->hi;
            b->n++, b->sum += g;
        }
    }
}
static void br_print(const char *what, const br_t *b)
{
    printf("  %s: the breath inside a pair, over %u pairs: %u .. %u sixteenths, mean %.2f (%u calls unanswered)\n", what,
           b->n, b->lo, b->hi, b->n ? (double)b->sum / (double)b->n : 0.0, b->unanswered);
}
/* (the arc's sections as they pass) secs played a block at a time, each section's span (its first sixteenth, en_bars,
 * and the first of the next: ~0u while it's still playing) recorded as it's seen */
#define SPN 512
static uint32_t sp_n, sp_on[SPN], sp_off[SPN];
static uint8_t sp_sec[SPN];
static void run_spans(float secs)
{
    int b;
    sp_n = 0;
    for (b = 0; b < (int)(secs * 44100.0f / 256.0f); b++) {
        run(256.0f / 44100.0f + 1e-6f);
        if (!sp_n || sp_sec[sp_n - 1] != en_section) {
            if (sp_n)
                sp_off[sp_n - 1] = en_bars;
            if (sp_n < SPN)
                sp_sec[sp_n] = en_section, sp_on[sp_n] = en_bars, sp_off[sp_n] = 0xFFFFFFFFu, sp_n++;
        }
    }
}
/* played until the Lead begins the first call of a block after the one playing (at most secs; 0 if it didn't) */
static int lg_wait_block(float secs)
{
    const uint8_t b0 = en_lead_blk;
    int b;
    for (b = 0; b < (int)(secs * 44100.0f / 256.0f); b++) {
        run(256.0f / 44100.0f + 1e-6f);
        if (en_lead_blk != b0 && en_lead_role == LR_CALL && lg_n && lg[lg_n - 1].phr == en_lead_phr)
            return 1;
    }
    return 0;
}
/* (the white keys write the theme) played a block at a time until sixteenth s (en_bars) has been played */
static void run_to(uint32_t s)
{
    while (en_bars < s)
        run(256.0f / 44100.0f + 1e-6f);
}
/* a figure on the white keys at once: n keys, each degree dg[k] held du[k] sixteenths (to the tick), legato (the next
 * pressed as the one before is let go); returns the sixteenth its last key was let go */
static uint32_t keys_now(const int *dg, const int *du, int n)
{
    uint32_t s = en_bars;
    int k;
    for (k = 0; k < n; k++) {
        en_lead_key(dg[k], 1);
        s += (uint32_t)du[k];
        run_to(s);
        en_lead_key(dg[k], 0);
    }
    return s;
}
static void bar_next(void)                       /* (played to the next bar line, just after the tick before it) */
{
    run_to(en_bars + 1u);
    run_to((en_bars + 15u) / 16u * 16u);
}
static uint32_t keys_play(const int *dg, const int *du, int n)   /* (the same, from the next bar line) */
{
    bar_next();
    return keys_now(dg, du, n);
}
/* nothing the Lead (channel 3) sounds: no note-on without its note-off */
static int ld_left_on(void)
{
    int k, c = 0;
    for (k = 0; k < 128; k++)
        c += sounding[2][k] > 0;
    return c + (int)ld_on_now;
}
/* (Part 3, the user: the Lead answers the keys, it doesn't repeat them) from the log: the phrase at entry i against the
 * keys' figure logged at k0 .. k0+n-1: 1 if it's the keys' answer (LR_ANSWER on the theme, slot 0, n notes, the
 * figure's rhythm: each onset apart as the figure's, the last held as long); *dif: its notes not the figure's (in how
 * many places), *dm: its notes less the figure's, in degrees, summed (its mean's distance x n). (Review 1: its notes
 * related to the figure's) la_first:
 * its first note's distance from the figure's last, in degrees; la_strong: the most a note on a strong beat (on beat 1
 * or 3 of the phrase, or held a half note; not the first, nor the last: the ending's) is from the figure's there */
static int la_first, la_strong;
static int lg_answer(uint32_t i, uint32_t k0, int n, int *dif, int *dm)
{
    int k, sa = 0, sf = 0, ok = n > 0 && i + (uint32_t)n <= lg_n && lg_first(i) && lg_last(i + (uint32_t)n - 1u) &&
                              lg[i].role == LR_ANSWER && lg[i].slot == 0;
    *dif = 0, *dm = 0, la_first = 99, la_strong = 0;
    for (k = 0; k < n && ok; k++) {
        const uint32_t a = i + (uint32_t)k, f = k0 + (uint32_t)k;
        const int dd = lg_deg(lg[a].note) - lg_deg(lg[f].note);
        if (k < n - 1)
            ok &= lg[a + 1u].on - lg[a].on == lg[f + 1u].on - lg[f].on;
        else
            ok &= lg[a].off - lg[a].on == lg[f].off - lg[f].on;
        *dif += lg[a].note != lg[f].note;
        sa += lg_deg(lg[a].note), sf += lg_deg(lg[f].note);
        if (k && k < n - 1 && (!((lg[a].on - lg[i].on) & 7u) || lg[f + 1u].on - lg[f].on >= 8u) &&
            (dd < 0 ? -dd : dd) > la_strong)
            la_strong = dd < 0 ? -dd : dd;
    }
    if (ok) {
        const int d1 = lg_deg(lg[i].note) - lg_deg(lg[k0 + (uint32_t)n - 1u].note);
        la_first = d1 < 0 ? -d1 : d1;
    }
    *dm = sa - sf;
    return ok;
}
/* (the Lead's offspring) how much motif (o, d, n entries) shares with a theme (to, td, tn), 0..100: the mean of its
 * rhythm's share (the longest run of its entries with the theme's lengths, rests where its rests are, over the theme's
 * entries) and its line's (the longest run of its intervals, in degrees, the theme's in a row: as they are, mirrored,
 * or the theme's backwards; over the theme's intervals) */
static int kin_of(const int8_t *o, const uint8_t *d, int n, const int8_t *to, const uint8_t *td, int tn)
{
    int a, b, k, rr = 0, ir = 0, na = 0, nb = 0, v, w;
    int ia[32], ib[32];                          /* (room for any motif: a played theme may grow) */
    for (a = 0; a < n; a++)
        for (b = 0; b < tn; b++) {
            for (k = 0; a + k < n && b + k < tn && d[a + k] == td[b + k] &&
                        (o[a + k] == LD_REST) == (to[b + k] == LD_REST); k++)
                ;
            rr = k > rr ? k : rr;
        }
    for (a = 0, v = -999; a < n; a++)            /* (the intervals between notes, rests passed over) */
        if (o[a] != LD_REST) {
            if (v > -999)
                ia[na++] = o[a] - v;
            v = o[a];
        }
    for (b = 0, v = -999; b < tn; b++)
        if (to[b] != LD_REST) {
            if (v > -999)
                ib[nb++] = to[b] - v;
            v = to[b];
        }
    for (w = 0; w < 3; w++)                      /* (as they are, mirrored, backwards) */
        for (a = 0; a < na; a++)
            for (b = 0; b < nb; b++) {
                for (k = 0; a + k < na && b + k < nb &&
                            ia[a + k] == (w == 0 ? ib[b + k] : w == 1 ? -ib[b + k] : -ib[nb - 1 - b - k]); k++)
                    ;
                ir = k > ir ? k : ir;
            }
    return (100 * rr / (tn ? tn : 1) + (nb ? 100 * ir / nb : 0)) / 2;
}
/* played a block at a time for secs: each fresh idea a block takes (en_lead_ideas moves), its sixteenth into at[], its
 * kinship with the theme (to, td, tn: kin_of) into kv[] and how it was made into kd[] (at most max); the kinship
 * (en_lead_kin) when it was first seen at 0 into *k0 (~0u: never), the most seen into *kmax; returns the count */
static int ideas_log(float secs, const int8_t *to, const uint8_t *td, int tn, uint32_t *at, int *kv, int *kd, int max,
                     uint32_t *k0, int *kmax)
{
    const int blocks = (int)(secs * 44100.0f / 256.0f);
    uint8_t seen = en_lead_ideas;
    int b, c = 0;
    *k0 = 0xFFFFFFFFu, *kmax = 0;
    for (b = 0; b < blocks; b++) {
        const int kn = (run(256.0f / 44100.0f + 1e-6f), en_lead_kin());
        *kmax = kn > *kmax ? kn : *kmax;
        if (!kn && *kmax && *k0 == 0xFFFFFFFFu)   /* (ended, after it began) */
            *k0 = en_bars;
        if (en_lead_ideas != seen) {
            int8_t o[32];
            uint8_t d[32];
            const int n = en_lead_idea(o, d);
            seen = en_lead_ideas;
            if (c < max)
                at[c] = en_bars, kv[c] = kin_of(o, d, n, to, td, tn), kd[c] = en_lead_kind, c++;
        }
    }
    return c;
}
/* (the Lead's offspring) a new engine playing D Aeolian at 160 in Flow, the chord held, Lead in 0, Phrases 100, Memory
 * mem: the Lead's own pairs going on, a fresh idea every block at Memory 0 (at 160: three or so in the 30 bars of
 * the kinship there) */
static void off_setup(int mem)
{
    fresh();
    lg_pool = EN_MODE_POOL[MODE_AEOLIAN], lg_root = 2;
    en_set(P_ROOT, 2);
    en_pool(lg_pool);
    en_set(P_TEMPO, 160);
    en_set(P_LEAD, 60);
    en_set(P_LDIN, 0);
    en_set(P_LDENS, 100);
    en_set(P_MOTION, 0);
    en_set(P_DRIFT, 6);
    en_set(P_LMEM, mem);
    en_play(1);
}

/* (the voices batch, item 2: Pluck) the Sequence alone on voice v (5: Pluck): every other layer, the echo, the plate,
 * the cloud and the repeats out, the cutoff still (Sweep 0); one note a bar (step 0 of 16, degree deg) at Seq oct
 * oct, root C, the chord's transposition off (Bed: the note is 48 + 12 oct + the degree's), at 120 */
static void ks_solo(int v, int oct, int deg, int tune, int decay)
{
    int8_t d[16];
    int k;
    memset(d, 0, sizeof d);
    d[0] = (int8_t)deg;
    fresh();
    en_set(P_CHORD, 0), en_set(P_LEAD, 0), en_set(P_SEQ2, 0), en_set(P_CRACKLE, 0);
    en_set(P_ECHO, 0), en_set(P_REVERB, 0), en_set(P_CLOUD, 0), en_set(P_TAPA, 0), en_set(P_TAPB, 0);
    en_set(P_SWEEP, 0), en_set(P_MI1, 0), en_set(P_TEMPO, 120), en_set(P_ROOT, 0), en_set(P_TRANS, 2);
    en_set(P_EWIDTH, 0);                         /* (the dry's widening off: its copies' slow sway bends the pitch a
                                                  * cent or two) */
    en_set(P_SEQOCT, oct), en_set(P_TUNE, tune), en_set(P_DECAY, decay), en_set(P_SVOICE, v);
    seq_write(16, 1u, d);
    for (k = 0; k < 40; k++) {                   /* (stopped, until silent: a long plate's tail from the tests before,
                                                  * en_init leaves the plate as it is, would sound under the note) */
        peak = 0.0f;
        run(0.5f);
        if (peak < 1e-5f)
            break;
    }
    en_play(1);
}

/* (the voices batch, item 5) the ten FM voices appended to both Sequences' ALGORITHM (after Pluck; Sequence 2's after
 * Sub and Pluck): named, every voice before them where it was (the saves), and each one played sound in code: the
 * Sequence alone at the lowest it reaches (Seq oct -2, Tune -50) and two octaves up, every sample finite, heard,
 * bounded, the note ending (its slot free) once stopped, its state sound; then a slot stolen while it sounds (16ths at
 * 200, Decay 100: more notes than the six slots), and Sequence 2 on one of them. How they sound is the user's, by ear */
static void test_fm_seq(void)
{
    static const char *const SEQ_FM[10] = {"FM Keys", "FM Bell", "FM EP", "FM Clav", "FM Guitar", "FMMarimba",
                                           "FM Vibes", "FM Metal", "FM Bass", "FM Sub"};
    static const int8_t RUN[16] = {0, 2, 4, 7, 9, 7, 4, 2, 0, 4, 7, 11, 7, 4, 2, 0};
    char t[16];
    int k, i;
    CHECK(en_param_info(P_SVOICE)->hi == 15 && en_param_info(P_S2VOICE)->hi == 16, "test_fm_seq_listed: hi %d %d",
          en_param_info(P_SVOICE)->hi, en_param_info(P_S2VOICE)->hi);
    for (k = 0; k < 10; k++) {
        en_param_text(P_SVOICE, 6 + k, t);
        CHECK(!strcmp(t, SEQ_FM[k]), "test_fm_seq_listed: seq voice %d %s", 6 + k, t);
        en_param_text(P_S2VOICE, 7 + k, t);
        CHECK(!strcmp(t, SEQ_FM[k]), "test_fm_seq_listed: seq2 voice %d %s", 7 + k, t);
    }
    en_param_text(P_SVOICE, 5, t);
    CHECK(!strcmp(t, "Pluck"), "test_fm_old_kept: seq voice 5 %s", t);
    en_param_text(P_S2VOICE, 5, t);
    CHECK(!strcmp(t, "Sub"), "test_fm_old_kept: seq2 voice 5 %s", t);
    en_param_text(P_S2VOICE, 6, t);
    CHECK(!strcmp(t, "Pluck"), "test_fm_old_kept: seq2 voice 6 %s", t);
    for (k = 0; k < 10; k++)
        for (i = 0; i < 2; i++) {
            ks_solo(6 + k, i ? 2 : -2, 0, i ? 0 : -50, 35);
            peak = 0.0f;
            run(2.0f);
            CHECK(!bad && peak > 1e-4f && peak < 4.0f, "test_fm_seq_render: %s at Seq oct %d: peak %f, bad %d",
                  SEQ_FM[k], i ? 2 : -2, (double)peak, bad);
            en_play(0);
            run(6.0f);
            CHECK(en_pl_on() == 0, "test_fm_seq_ends: %s at Seq oct %d: %d slots still sounding 6 s after", SEQ_FM[k],
                  i ? 2 : -2, en_pl_on());
            CHECK(en_fm_ok(), "test_fm_state: %s at Seq oct %d: not finite, or denormal", SEQ_FM[k], i ? 2 : -2);
        }
    ks_solo(12, 0, 0, 0, 100);                   /* (Vibes) */
    seq_write(16, 0xFFFFu, RUN);
    en_set(P_TEMPO, 200);
    peak = 0.0f;
    run(3.0f);
    CHECK(!bad && peak > 1e-4f && peak < 4.0f && en_fm_ok(), "test_fm_seq_steal: Vibes in 16ths: peak %f, bad %d",
          (double)peak, bad);
    {
        seq_t s;
        memset(&s, 0, sizeof s);
        s.len = s.base_len = 16;
        s.gate = 0xFFFFu;
        s.acc = 1u;
        for (k = 0; k < 16; k++)
            s.deg[k] = RUN[k];
        ks_solo(0, 0, 0, 0, 50);
        en_set(P_SEQ, 0), en_set(P_SEQ2, 80), en_set(P_S2VOICE, 15);   /* (Sequence 2 alone on Round Bass) */
        en_seq_set(1, &s);
        peak = 0.0f;
        run(3.0f);
        CHECK(!bad && peak > 1e-4f && peak < 4.0f && en_fm_ok(), "test_fm_seq2_render: Round Bass: peak %f, bad %d",
              (double)peak, bad);
    }
    en_play(0);
}

/* secs played, the output (mono, the two sides' mean) into x (at most max samples): how many */
static int run_cap(float secs, float *x, int max)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j, k = 0;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 256; j++) {
            const float y = 0.5f * ((float)buf[2 * j] + (float)buf[2 * j + 1]) / 8388608.0f;
            if (y != y)
                bad++;
            if (k < max)
                x[k++] = y;
        }
    }
    return k;
}

/* the fundamental's frequency near f0 in x[0..n) (at most 1 s of it): the Hann-windowed DFT's peak, searched over
 * +-60 cents in 1-cent steps, then parabolically on its log; 0 if the peak is at the search's edge (the note's
 * envelope is real, so its partial's spectrum sits symmetric about the partial: the decay brings no bias; an
 * autocorrelation's peak, tried first, was pulled ~40 cents by it on the low notes) */
static float dft_pitch(const float *x, int n, float f0)
{
    static double hw[44100];
    double m[121];
    int c, i, best = 0;
    if (n > 44100)
        n = 44100;
    for (i = 0; i < n; i++)
        hw[i] = (0.5 - 0.5 * cos(6.283185307179586 * (double)i / (double)n)) * (double)x[i];
    for (c = 0; c < 121; c++) {
        const double w = 6.283185307179586 * (double)f0 * pow(2.0, (double)(c - 60) / 1200.0) / 44100.0;
        const double cw = cos(w), sw = sin(w);
        double re = 0.0, im = 0.0, pr = 1.0, pi = 0.0;
        for (i = 0; i < n; i++) {
            const double t = pr * cw - pi * sw;
            re += hw[i] * pr, im -= hw[i] * pi;
            pi = pr * sw + pi * cw, pr = t;
        }
        m[c] = log(re * re + im * im + 1e-300);
        if (m[c] > m[best])
            best = c;
    }
    if (best <= 0 || best >= 120)
        return 0.0f;
    {
        const double den = m[best - 1] - 2.0 * m[best] + m[best + 1];
        const double d = den < 0.0 ? 0.5 * (m[best - 1] - m[best + 1]) / den : 0.0;
        return f0 * (float)pow(2.0, ((double)(best - 60) + d) / 1200.0);
    }
}

/* the first sample of x[0..n) over 1e-4 (the note's start), or -1 */
static int ks_onset(const float *x, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if (fabsf(x[i]) > 1e-4f)
            return i;
    return -1;
}

/* the time (s) x takes from its loudest 10 ms to 40 dB under it, times 1.5: its T60 (0: never) */
static float ks_t60(const float *x, int n)
{
    float e[400], top = 0.0f;
    int w, i, nw = n / 441 < 400 ? n / 441 : 400, at = 0;
    for (w = 0; w < nw; w++) {
        e[w] = 0.0f;
        for (i = 0; i < 441; i++)
            e[w] += x[441 * w + i] * x[441 * w + i];
        if (e[w] > top)
            top = e[w], at = w;
    }
    for (w = at; w < nw; w++)
        if (e[w] < top * 1e-4f)
            return 1.5f * (float)(w - at) * 0.01f;
    return 0.0f;
}

/* (the voices batch, item 3: Choir) the Drone alone on voice v at Colour col: every other layer, the echo, the plate,
 * the cloud, the repeats, the phaser and the master actions' motion out; the chord held (Drift Hold) on C, quick to
 * swell (Attack 0), its filter's move still (Width 0, no Intro, no Bridge, no Arc) unless a test sets them after;
 * stopped until silent first (a tail from the tests before), then playing */
static void ch_solo(int v, int col)
{
    int k;
    fresh();
    for (k = 0; k < P_NPARAMS; k++)              /* (every knob at its default first: the chord, its voicing and the
                                                  * sound the same whatever the tests before left) */
        en_set(k, en_param_info(k)->def);
    en_pool(EN_MODE_POOL[en_param_info(P_MODE)->def]);
    en_set(P_SEQ, 0), en_set(P_LEAD, 0), en_set(P_SEQ2, 0), en_set(P_CRACKLE, 0);
    en_set(P_ECHO, 0), en_set(P_REVERB, 0), en_set(P_CLOUD, 0), en_set(P_TAPA, 0), en_set(P_TAPB, 0);
    en_set(P_PHASER, 0), en_set(P_MOTION, 0), en_set(P_EVOLVE, 0), en_set(P_DRIFT, 6), en_set(P_TEMPO, 120);
    en_set(P_ROOT, 0), en_set(P_MW0, 0), en_set(P_MI0, 0), en_set(P_BRIDGE, 0), en_set(P_ARC, 0), en_set(P_EWIDTH, 0);
    en_set(P_CATT, 0), en_set(P_CHORD, 80), en_set(P_CVOICE, v), en_set(P_CCOL, col);
    en_regrow();
    for (k = 0; k < 40; k++) {
        peak = 0.0f;
        run(0.5f);
        if (peak < 1e-5f)
            break;
    }
    en_play(1);
}

/* (the voices batch, item 5) the Drone has no FM voices (the user, 2026-10-30: "we can remove the FM from the drone -
 * they don't bring anything to the table"): its six voices as before, Choir the last */
static void test_fm_drone_none(void)
{
    char t[16];
    CHECK(en_param_info(P_CVOICE)->hi == 5, "test_fm_drone_none: hi %d", en_param_info(P_CVOICE)->hi);
    en_param_text(P_CVOICE, 5, t);
    CHECK(!strcmp(t, "Choir"), "test_fm_drone_none: drone voice 5 %s", t);
}

/* (the voices batch, item 5) the Lead's three FM voices, appended after Synthi: named, Synthi where it was (the
 * saves); each alone on the white keys (its own phrases off), a note held, then a glide an octave up (legato: its FM
 * state kept), let go: every sample finite, heard, bounded, the state sound; silent once let go. The code only: how
 * they sound is the user's, by ear */
static void test_fm_lead(void)
{
    static const char *const LD_FM[3] = {"FM Lead", "FM Reed", "FM Brass"};
    static const int OFF[13] = {P_CHORD, P_SEQ, P_SEQ2, P_CRACKLE, P_CLOUD, P_MOTION, P_ECHO, P_REVERB, P_LDECHO,
                                P_LDREV, P_LDIN, P_LDENS, P_VIB};
    char t[16];
    int k, i;
    CHECK(en_param_info(P_LVOICE)->hi == 7, "test_fm_lead_listed: hi %d", en_param_info(P_LVOICE)->hi);
    for (k = 0; k < 3; k++) {
        en_param_text(P_LVOICE, 5 + k, t);
        CHECK(!strcmp(t, LD_FM[k]), "test_fm_lead_listed: lead voice %d %s", 5 + k, t);
    }
    en_param_text(P_LVOICE, 4, t);
    CHECK(!strcmp(t, "Synthi"), "test_fm_old_kept: lead voice 4 %s", t);
    for (k = 0; k < 3; k++) {
        fresh();
        for (i = 0; i < 13; i++)
            en_set(OFF[i], 0);
        en_set(P_GLIDE, 60), en_set(P_LVOICE, 5 + k);
        en_play(1);
        run(0.5f);
        peak = 0.0f;
        en_lead_key(0, 1);
        run(1.0f);
        en_lead_key(7, 1);
        en_lead_key(0, 0);
        run(1.5f);
        en_lead_key(7, 0);
        CHECK(!bad && peak > 1e-4f && peak < 4.0f && en_fm_ok(), "test_fm_lead_render: %s: peak %f, bad %d", LD_FM[k],
              (double)peak, bad);
        run(3.0f);
        peak = 0.0f;
        run(0.5f);
        CHECK(peak < 1e-4f && en_fm_ok(), "test_fm_lead_ends: %s: peak %f 3 s after it was let go", LD_FM[k],
              (double)peak);
    }
    en_play(0);
}

/* (the voices batch, item 5) a shake (HOME or a layer's button held) picks a voice from those before the FM voices
 * only (the user, 2026-10-30: "currently only add the choir, we just want to hear the fm voices"): the Drone's six,
 * Choir among them now; the Sequence's six, Sequence 2's seven, the Lead's five; any other parameter its whole range */
static void test_shake_range(void)
{
    CHECK(en_voice_shake_n(P_CVOICE) == 6, "test_shake_range: drone %d (Organ .. Choir)", en_voice_shake_n(P_CVOICE));
    CHECK(en_voice_shake_n(P_SVOICE) == 6, "test_shake_range: seq %d (2600 .. Pluck)", en_voice_shake_n(P_SVOICE));
    CHECK(en_voice_shake_n(P_S2VOICE) == 7, "test_shake_range: seq2 %d (2600 .. Pluck)", en_voice_shake_n(P_S2VOICE));
    CHECK(en_voice_shake_n(P_LVOICE) == 5, "test_shake_range: lead %d (Odyssey .. Synthi)", en_voice_shake_n(P_LVOICE));
    CHECK(en_voice_shake_n(P_NCHAR) == en_param_info(P_NCHAR)->hi - en_param_info(P_NCHAR)->lo + 1,
          "test_shake_range: atmosphere %d", en_voice_shake_n(P_NCHAR));
}

/* (the final review, 2) the Lead turned back onto an FM voice while a key holds a note it began on another voice: it
 * sounds (its FM note starts again), not silent at the end of the last FM note's release */
static void test_fm_lead_return(void)
{
    static const int OFF[13] = {P_CHORD, P_SEQ, P_SEQ2, P_CRACKLE, P_CLOUD, P_MOTION, P_ECHO, P_REVERB, P_LDECHO,
                                P_LDREV, P_LDIN, P_LDENS, P_VIB};
    float p1;
    int i;
    fresh();
    for (i = 0; i < 13; i++)
        en_set(OFF[i], 0);
    en_set(P_LVOICE, 5);                         /* (Soft Lead) */
    en_play(1);
    run(0.5f);
    en_lead_key(0, 1);
    peak = 0.0f;
    run(1.0f);
    p1 = peak;
    en_lead_key(0, 0);
    run(1.5f);                                   /* (its release done) */
    en_set(P_LVOICE, 0);                         /* (Odyssey: a note from silence) */
    en_lead_key(0, 1);
    run(0.5f);
    en_set(P_LVOICE, 5);                         /* (back to Soft Lead, the key held) */
    run(0.1f);                                   /* (Odyssey's tail in the Lead's filter dies first) */
    peak = 0.0f;
    run(0.4f);
    CHECK(!bad && peak > 0.1f * p1, "test_fm_lead_return: Soft Lead again: peak %f (first %f)", (double)peak,
          (double)p1);
    en_lead_key(0, 0);
    en_play(0);
}

/* x[0..CH_FN) (Hann) to its spectrum's envelope: the mean power over a quarter octave about each of 200 .. ~4 kHz in
 * 1/48-octave steps (ch_hz), dB, into env[0..CH_NE); *top: the share of the power above 4 kHz, dB */
#define CH_FN 32768
#define CH_NE 208
static float ch_hz(int k) { return 200.0f * powf(2.0f, (float)k / 48.0f); }
static int ch_at(float f) { return (int)(48.0f * log2f(f / 200.0f) + 0.5f); }   /* (f's step) */
static void ch_env(const float *x, float *env, float *top)
{
    static double re[CH_FN], im[CH_FN], pw[CH_FN / 2];
    double all = 0.0, hi = 0.0;
    int i, j, k, m;
    for (i = 0; i < CH_FN; i++)
        re[i] = (0.5 - 0.5 * cos(6.283185307179586 * (double)i / (double)CH_FN)) * (double)x[i], im[i] = 0.0;
    for (i = 1, j = 0; i < CH_FN; i++) {         /* (radix 2, in place) */
        int bit = CH_FN >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i];
            re[i] = re[j], re[j] = t;
        }
    }
    for (m = 2; m <= CH_FN; m <<= 1)
        for (i = 0; i < m / 2; i++) {
            const double w = -6.283185307179586 * (double)i / (double)m, c = cos(w), s = sin(w);
            for (k = i; k < CH_FN; k += m) {
                const double tr = c * re[k + m / 2] - s * im[k + m / 2], ti = s * re[k + m / 2] + c * im[k + m / 2];
                re[k + m / 2] = re[k] - tr, im[k + m / 2] = im[k] - ti;
                re[k] += tr, im[k] += ti;
            }
        }
    for (i = 1; i < CH_FN / 2; i++) {
        pw[i] = re[i] * re[i] + im[i] * im[i];
        all += pw[i];
        if ((double)i * 44100.0 / CH_FN > 4000.0)
            hi += pw[i];
    }
    *top = (float)(10.0 * log10(hi / (all + 1e-300) + 1e-300));
    for (k = 0; k < CH_NE; k++) {
        const int b0 = (int)(ch_hz(k) * 0.917f * CH_FN / 44100.0f), b1 = (int)(ch_hz(k) * 1.091f * CH_FN / 44100.0f);
        double s = 0.0;
        for (i = b0; i <= b1; i++)
            s += pw[i];
        env[k] = (float)(10.0 * log10(s / (double)(b1 - b0 + 1) + 1e-300));
    }
}

/* a click's mark: the largest step between two samples in x[0..n) over the 99th percentile of each 64 samples'
 * largest step (a chord's own peaks, two banks summed in a crossfade, stay near it, ~1.3 .. 1.6 for every voice; a
 * click, a step the size of the signal, stands well out; the median, tried first, rated every voice ~3 .. 4 with no
 * click: it measured the waveform's crest) */
static int ch_cmp(const void *a, const void *b) { return *(const float *)a < *(const float *)b ? -1 : *(const float *)a > *(const float *)b; }
static float ch_click(const float *x, int n)
{
    static float w[13 * 44100 / 64 + 1];
    float top = 0.0f;
    int i, nw = 0;
    for (i = 64; i + 64 <= n; i += 64) {
        float m = 0.0f;
        int j;
        for (j = i; j < i + 64; j++)
            m = fmaxf(m, fabsf(x[j] - x[j - 1]));
        top = fmaxf(top, m);
        w[nw++] = m;
    }
    qsort(w, (size_t)nw, sizeof w[0], ch_cmp);
    return nw ? top / fmaxf(w[nw * 99 / 100], 1e-12f) : 0.0f;
}

/* (the voices batch, item 4: the Atmospheres) the Atmosphere alone: a piece's settings (pc < 0: every knob at its
 * default), character ch at level lv, every other layer's level 0; stopped until silent first (a tail from the tests
 * before), then playing, its 2 s fade-in let pass. at_cap then plays secs: the mono mix into at_x, the two sides'
 * mean square into at_ms */
#define AT_MAX (46 * 44100)
static float at_x[AT_MAX];
static int at_n;
static double at_ms;
static void at_solo(int pc, int ch, int lv, int dry)   /* (dry: its own reverb and echo sends 0) */
{
    int k;
    en_play(0);                                  /* (stopped until silent and 5 s more first, so the Atmosphere's gate,
                                                  * the transport's and not reset by en_init, is shut: its fading out
                                                  * would move its wander and draw its dice, and each test would hear
                                                  * another wind) */
    for (k = 0; k < 40; k++) {
        peak = 0.0f;
        run(0.5f);
        if (peak < 1e-5f)
            break;
    }
    run(5.0f);
    fresh();
    for (k = 0; k < P_NPARAMS; k++)
        en_set(k, en_param_info(k)->def);
    en_pool(EN_MODE_POOL[en_param_info(P_MODE)->def]);
    if (pc >= 0)
        piece(pc);
    en_regrow();
    en_set(P_CHORD, 0), en_set(P_SEQ, 0), en_set(P_LEAD, 0), en_set(P_SEQ2, 0);
    en_set(P_NCHAR, ch), en_set(P_CRACKLE, lv);
    if (dry)
        en_set(P_NPLATE, 0), en_set(P_NECHO, 0);
    en_play(1);
    run(2.0f);
}
static void at_cap(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    double s = 0.0;
    at_n = 0;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 256; j++) {
            const double l = (double)buf[2 * j] / 8388608.0, r = (double)buf[2 * j + 1] / 8388608.0;
            if (fabs(l) > (double)peak || fabs(r) > (double)peak)
                peak = (float)fmax(fabs(l), fabs(r));
            if (l != l || r != r)
                bad++;
            s += l * l + r * r;
            if (at_n < AT_MAX)
                at_x[at_n++] = (float)(0.5 * (l + r));
        }
    }
    at_ms = s / (2.0 * (double)(at_n > 0 ? at_n : 1));
}
static void at_fft(double *re, double *im, int n)   /* (radix 2, in place) */
{
    int i, j, k, m;
    for (i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i];
            re[i] = re[j], re[j] = t;
            t = im[i], im[i] = im[j], im[j] = t;
        }
    }
    for (m = 2; m <= n; m <<= 1)
        for (i = 0; i < m / 2; i++) {
            const double w = -6.283185307179586 * (double)i / (double)m, c = cos(w), s = sin(w);
            for (k = i; k < n; k += m) {
                const double tr = c * re[k + m / 2] - s * im[k + m / 2], ti = s * re[k + m / 2] + c * im[k + m / 2];
                re[k + m / 2] = re[k] - tr, im[k + m / 2] = im[k] - ti;
                re[k] += tr, im[k] += ti;
            }
        }
}
static double aw2(double f)                      /* the A-weighting's power gain at f (IEC 61672: 0 dB at 1 kHz) */
{
    const double f2 = f * f, a = 12194.0 * 12194.0 * f2 * f2 /
                                 ((f2 + 20.6 * 20.6) * sqrt((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9)) * (f2 + 12194.0 * 12194.0)) *
                                 1.2589254;     /* (+2.00 dB) */
    return a * a;
}
/* at_x's spectrum: Welch, 8192-point Hann frames half overlapped, the mean power per bin into at_pw; each frame's own
 * level (dB) and its centroid (the power-weighted mean of log2 f over 150 Hz .. 4 kHz) into at_fl, at_fc (at most
 * AT_NF frames: how many) */
#define AT_FN 8192
#define AT_NF 512
static double at_pw[AT_FN / 2], at_fl[AT_NF], at_fc[AT_NF];
static int at_spec(void)
{
    static double re[AT_FN], im[AT_FN];
    int f, i, nf = 0;
    memset(at_pw, 0, sizeof at_pw);
    for (f = 0; f + AT_FN <= at_n && nf < AT_NF; f += AT_FN / 2, nf++) {
        double all = 0.0, cw = 0.0, cs = 0.0;
        for (i = 0; i < AT_FN; i++)
            re[i] = (0.5 - 0.5 * cos(6.283185307179586 * (double)i / AT_FN)) * (double)at_x[f + i], im[i] = 0.0;
        at_fft(re, im, AT_FN);
        for (i = 1; i < AT_FN / 2; i++) {
            const double p = re[i] * re[i] + im[i] * im[i], hz = (double)i * 44100.0 / AT_FN;
            at_pw[i] += p;
            all += p;
            if (hz >= 150.0 && hz <= 4000.0)
                cw += p, cs += p * log2(hz);
        }
        at_fl[nf] = 10.0 * log10(all + 1e-300);
        at_fc[nf] = cw > 0.0 ? cs / cw : 0.0;
    }
    for (i = 1; i < AT_FN / 2; i++)
        at_pw[i] /= (double)(nf > 0 ? nf : 1);
    return nf;
}
static double at_band(double f0, double f1)       /* at_pw's mean over f0 .. f1, dB */
{
    double s = 0.0;
    int i, n = 0;
    for (i = (int)(f0 * AT_FN / 44100.0); i <= (int)(f1 * AT_FN / 44100.0); i++)
        s += at_pw[i], n++;
    return 10.0 * log10(s / (double)(n > 0 ? n : 1) + 1e-300);
}
static double at_slope(double fa, double fb)      /* dB an octave from fa to fb (each a +-5% band) */
{
    return (at_band(fb * 0.95, fb * 1.05) - at_band(fa * 0.95, fa * 1.05)) / log2(fb / fa);
}
static double at_alevel(void)                    /* the level heard: the two sides' mean square, A-weighted by the mono
                                                  * mix's spectrum (dB, 0: full scale) */
{
    double a = 0.0, all = 0.0;
    int i;
    for (i = 1; i < AT_FN / 2; i++)
        a += at_pw[i] * aw2((double)i * 44100.0 / AT_FN), all += at_pw[i];
    return 10.0 * log10(at_ms + 1e-300) + 10.0 * log10(a / (all + 1e-300) + 1e-300);
}
static double at_corr(const double *a, const double *b, int n)   /* (Pearson's) */
{
    double ma = 0.0, mb = 0.0, sab = 0.0, saa = 0.0, sbb = 0.0;
    int i;
    for (i = 0; i < n; i++)
        ma += a[i], mb += b[i];
    ma /= (double)(n > 0 ? n : 1), mb /= (double)(n > 0 ? n : 1);
    for (i = 0; i < n; i++)
        sab += (a[i] - ma) * (b[i] - mb), saa += (a[i] - ma) * (a[i] - ma), sbb += (b[i] - mb) * (b[i] - mb);
    return sab / sqrt(saa * sbb + 1e-300);
}
static int at_dcmp(const void *a, const void *b) { return *(const double *)a < *(const double *)b ? -1 : *(const double *)a > *(const double *)b; }
static double at_pct(const double *a, int n, int pct)   /* a's pct-th percentile */
{
    static double t[AT_NF > 22050 ? AT_NF : 22050];
    memcpy(t, a, sizeof(double) * (size_t)n);
    qsort(t, (size_t)n, sizeof t[0], at_dcmp);
    return n ? t[n * pct / 100 < n ? n * pct / 100 : n - 1] : 0.0;
}
/* Rain's drops in at_x: its 2 ms frames' levels (dB); an onset where a frame stands 9 dB over the most of the three
 * before it and 12 dB over the median frame; each onset's peak (the most of the 5 frames from it), how long from
 * there it takes to fall 20 dB (ms; an onset whose fall another onset cuts short is not timed), and its pitch: the
 * strongest bin of the 23 ms from 2 ms after it (Hz). Out: the onsets' count, their median fall, the median frame
 * under the median peak (dB), the share of pitches within 300 Hz .. 6 kHz */
static void at_drops(int *ons, double *fall, double *over, double *inr)
{
    static double fr[22050], falls[4000], pks[4000];
    static double re[1024], im[1024];
    const int nfr = at_n / 88 < 22050 ? at_n / 88 : 22050;
    int k, i, nfa = 0, npk = 0, inside = 0, pitched = 0;
    double med;
    for (k = 0; k < nfr; k++) {
        double s = 0.0;
        for (i = 0; i < 88; i++)
            s += (double)at_x[88 * k + i] * (double)at_x[88 * k + i];
        fr[k] = 10.0 * log10(s / 88.0 + 1e-30);
    }
    med = at_pct(fr, nfr, 50);
    *ons = 0;
    for (k = 3; k + 5 < nfr; k++) {
        const double before = fmax(fr[k - 1], fmax(fr[k - 2], fr[k - 3]));
        double pk = fr[k];
        int at = k, e;
        if (fr[k] < before + 9.0 || fr[k] < med + 12.0)
            continue;
        ++*ons;
        for (i = k; i < k + 5; i++)
            if (fr[i] > pk)
                pk = fr[i], at = i;
        if (npk < 4000)
            pks[npk++] = pk;
        for (e = at + 1; e < nfr && fr[e] > pk - 20.0; e++)
            if (fr[e] > fr[e - 1] + 9.0)          /* (another drop: not timed) */
                break;
        if (e < nfr && fr[e] <= pk - 20.0 && nfa < 4000)
            falls[nfa++] = 2.0 * (double)(e - at);
        if (88 * k + 88 + 1024 <= at_n) {
            int b = 1;
            double bp = 0.0;
            for (i = 0; i < 1024; i++)
                re[i] = (0.5 - 0.5 * cos(6.283185307179586 * (double)i / 1024.0)) * (double)at_x[88 * k + 88 + i], im[i] = 0.0;
            at_fft(re, im, 1024);
            for (i = 2; i < 512; i++)
                if (re[i] * re[i] + im[i] * im[i] > bp)
                    bp = re[i] * re[i] + im[i] * im[i], b = i;
            pitched++;
            inside += (double)b * 44100.0 / 1024.0 >= 300.0 && (double)b * 44100.0 / 1024.0 <= 6000.0;
        }
        k += 4;                                  /* (one onset a drop) */
    }
    *fall = nfa ? at_pct(falls, nfa, 50) : 0.0;
    *over = npk ? at_pct(pks, npk, 50) - med : 0.0;
    *inr = pitched ? (double)inside / (double)pitched : 0.0;
}

/* (the voices batch, item 4: the Atmospheres, after Skiff's NOISE) about three minutes, each alone:
 * - Wind (Timewind's, at its 7): pink noise into the drifting band-pass. Below the band (50 .. 150 Hz) its skirt
 *   rises +3 dB an octave on pink, +6 on white; its centre (the frames' centroid) wanders, 10th to 90th percentile
 *   over 3/4 of an octave and more (the wander itself is the shipped one's: it measured 0.98 so);
 *   its level follows the centre (the frames' level against their centroid: correlated);
 * - Surf (Sunrain's, at its 5): browner, its spectrum falling 6 dB an octave or more over 300 Hz .. 3 kHz;
 * - both as loud as the user tuned them: the A-weighted level within 1.5 dB of the shipped engine's (measured
 *   here with the same helper: AT_WIND7, AT_SURF5);
 * - Rain, after Tape (every character before it keeps its number): drops (sparse onsets standing well over the
 *   bed, each ringing tens of ms, pitched within 300 Hz .. 6 kHz) over a quiet bed; as loud as Wind at the same
 *   level (within 3 dB);
 * - the float state (en_atm_ok: finite, no denormals) after each has played and stopped;
 * - (2026-10-30) Rain renamed Drops, its place kept; Hail after it (deep thuds over a muffled rain, the user's pick by
 *   ear, "F"), and Rain after it (rain outside, "O1"): every sample finite, heard and bounded, each as loud as Wind
 *   at the same level (within 3 dB) */
/* (the voices batch, item 5) the FM voice alone (fm.c, en_fm_probe): every one of fm1_open's 16 recipes at the lowest,
 * a middle and a high note, held a second and let go a second: every sample finite, the note heard and bounded, its
 * state finite and never denormal after (the code's soundness only: how it sounds is the user's, by ear) */
static void test_fm_voice(void)
{
    static const int NOTES[3] = {24, 60, 96};
    float peak;
    int r, k, ok;
    for (r = 0; r < FM_NREC_HOST; r++)
        for (k = 0; k < 3; k++) {
            CHECK(en_fm_probe(r, NOTES[k], 2u * (uint32_t)EN_SR, &peak, &ok), "test_fm_finite: recipe %d note %d", r,
                  NOTES[k]);
            CHECK(peak > 1e-3f && peak < 4.0f, "test_fm_bounded: recipe %d note %d peak %.4f", r, NOTES[k],
                  (double)peak);
            CHECK(ok, "test_fm_state: recipe %d note %d: state not finite, or denormal, after release", r, NOTES[k]);
        }
}

static void test_atmospheres(void)
{
#define AT_WIND7 (-47.5)                         /* (the shipped engine's, dB(A): the user's level for each) */
#define AT_SURF5 (-52.2)
    char t[16];
    int nf, ons, c;
    double lo, span, cor, al, wind25, fall, over, inr;
    en_param_text(P_NCHAR, 5, t);
    CHECK(!strcmp(t, "Tape"), "test_rain_listed: atmosphere 5 %s", t);
    en_param_text(P_NCHAR, 6, t);
    CHECK(!strcmp(t, "Drops"), "test_rain_listed: atmosphere 6 %s (Rain renamed)", t);
    en_param_text(P_NCHAR, 7, t);
    CHECK(!strcmp(t, "Hail"), "test_hail_listed: atmosphere 7 %s", t);
    en_param_text(P_NCHAR, 8, t);
    CHECK(en_param_info(P_NCHAR)->hi == 8 && !strcmp(t, "Rain"), "test_rain_out_listed: atmosphere 8 %s, hi %d", t,
          en_param_info(P_NCHAR)->hi);
    at_solo(0, 1, 7, 0);                         /* Wind, Timewind's */
    at_cap(45.0f);
    nf = at_spec();
    lo = at_slope(50.0, 150.0);
    span = at_pct(at_fc, nf, 90) - at_pct(at_fc, nf, 10);
    cor = at_corr(at_fc, at_fl, nf);
    al = at_alevel();
    printf("  wind: skirt %.1f dB/oct (50..150 Hz), centre wanders %.2f oct (10..90%%), level vs centre r %.2f, "
           "%.1f dB(A) (shipped %.1f)\n", lo, span, cor, al, AT_WIND7);
    CHECK(lo < 4.5, "test_wind_pink: the skirt below the band %.1f dB/oct (white +6, pink +3)", lo);
    CHECK(span > 0.75, "test_wind_wanders: its centre over %.2f octaves", span);
    CHECK(cor > 0.7, "test_wind_follows: its level against its centre r %.2f", cor);
    CHECK(fabs(al - AT_WIND7) < 1.5, "test_atmos_level_kept: Timewind's Wind %.1f dB(A), shipped %.1f", al, AT_WIND7);
    at_solo(5, 2, 5, 0);                         /* Surf, Sunrain's */
    at_cap(45.0f);
    at_spec();
    lo = at_slope(300.0, 3000.0);
    al = at_alevel();
    printf("  surf: %.1f dB/oct (300 Hz..3 kHz), %.1f dB(A) (shipped %.1f)\n", lo, al, AT_SURF5);
    CHECK(lo < -6.0, "test_surf_brown: %.1f dB/oct over 300 Hz .. 3 kHz", lo);
    CHECK(fabs(al - AT_SURF5) < 1.5, "test_atmos_level_kept: Sunrain's Surf %.1f dB(A), shipped %.1f", al, AT_SURF5);
    at_solo(-1, 1, 25, 1);                       /* (Wind at 25, dry, for Rain's level) */
    at_cap(20.0f);
    at_spec();
    wind25 = at_alevel();
    at_solo(-1, 6, 25, 1);                       /* Drops (was Rain), dry: its drops as they are */
    peak = 0.0f;
    bad = 0;
    at_cap(30.0f);
    at_spec();
    al = at_alevel();
    at_drops(&ons, &fall, &over, &inr);
    printf("  rain: %d drops in 30 s, falling 20 dB in %.0f ms, %.1f dB over the bed, %.0f%% pitched in range, "
           "peak %.3f, %.1f dB(A) (Wind %.1f)\n", ons, fall, over, 100.0 * inr, (double)peak, al, wind25);
    CHECK(!bad && ons >= 30 && ons <= 1200, "test_rain_sparse: %d drops in 30 s", ons);
    CHECK(over > 15.0, "test_rain_sparse: the drops %.1f dB over the bed", over);
    CHECK(fall > 10.0 && fall < 400.0, "test_rain_rings: a drop falls 20 dB in %.0f ms", fall);
    CHECK(inr > 0.9, "test_rain_range: %.0f%% of the drops pitched within 300 Hz .. 6 kHz", 100.0 * inr);
    CHECK(peak > 0.005f && peak < 0.5f, "test_rain_range: peak %.3f", (double)peak);
    CHECK(fabs(al - wind25) < 3.0, "test_rain_level: %.1f dB(A), Wind at the same level %.1f", al, wind25);
    at_solo(-1, 7, 25, 1);                       /* Hail, dry */
    peak = 0.0f;
    bad = 0;
    at_cap(30.0f);
    at_spec();
    al = at_alevel();
    printf("  hail: peak %.3f, %.1f dB(A) (Wind %.1f)\n", (double)peak, al, wind25);
    CHECK(!bad && peak > 0.005f && peak < 0.5f, "test_window_heard: peak %.3f, bad %d", (double)peak, bad);
    CHECK(fabs(al - wind25) < 3.0, "test_window_level: %.1f dB(A), Wind at the same level %.1f", al, wind25);
    at_solo(-1, 8, 25, 1);                       /* Rain (outside), dry */
    peak = 0.0f;
    bad = 0;
    at_cap(30.0f);
    at_spec();
    al = at_alevel();
    printf("  rain outside: peak %.3f, %.1f dB(A) (Wind %.1f)\n", (double)peak, al, wind25);
    CHECK(!bad && peak > 0.005f && peak < 0.5f, "test_rain_out_heard: peak %.3f, bad %d", (double)peak, bad);
    CHECK(fabs(al - wind25) < 3.0, "test_rain_out_level: %.1f dB(A), Wind at the same level %.1f", al, wind25);
    for (c = 1; c < EN_NNCHAR; c++) {            /* (each character played, stopped, the gate shut) */
        at_solo(-1, c, 60, 0);
        run(3.0f);
        en_play(0);
        run(5.0f);
        CHECK(en_atm_ok(), "test_atmos_state: %s: its state not finite, or denormal, after it stopped",
              EN_NCHAR_NAME[c]);
    }
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
        en_param_text(P_CVOICE, 2, t);           /* (the voices batch: the saw through two formants is a Horn now) */
        CHECK(!strcmp(t, "Horn"), "ctone 2: %s", t);
        /* (the voices batch, item 3) Choir appended as the Drone's sixth voice (after Glass): every voice before it
         * keeps its number, so the pieces and the saves keep their sound */
        en_param_text(P_CVOICE, 4, t);
        CHECK(!strcmp(t, "Glass"), "ctone 4: %s", t);
        en_param_text(P_CVOICE, 5, t);
        CHECK(!strcmp(t, "Choir"), "test_choir_listed: drone voice 5 %s", t);   /* (the FM pads after it since item 5:
                                                                          * test_fm_drone has the count) */
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
        /* (the voices batch, item 2) Pluck appended to both sequences' voices: the Sequence's sixth (after Glass),
         * Sequence 2's seventh (after Sub, which keeps its 5: Rubycon and the saves keep their sound) */
        en_param_text(P_SVOICE, 4, t);
        CHECK(!strcmp(t, "Glass"), "seq voice 4: %s", t);
        en_param_text(P_SVOICE, 5, t);
        CHECK(!strcmp(t, "Pluck"), "test_pluck_listed: seq voice 5 %s", t);   /* (the FM voices after it since item 5:
                                                                                * test_fm_seq has the counts) */
        en_param_text(P_S2VOICE, 5, t);
        CHECK(!strcmp(t, "Sub"), "seq2 voice 5: %s", t);
        en_param_text(P_S2VOICE, 6, t);
        CHECK(!strcmp(t, "Pluck"), "test_pluck_listed: seq2 voice 6 %s", t);
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
     * re-recorded for Timewind's Length 2 at 15, the user's 16/15, the recipe alone; 1cd60c54 then, re-recorded for
     * its Atmosphere 30 -> 7, the user's: the FM-1 sounds bright; 4bd3f4dc then, re-recorded for the voices batch's
     * Wind, pink, its level following its band: its Atmosphere's sound alone) */
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
        CHECK(r.h == 0xcfe23fbeu, "test_fm_zero: Timewind's first minute %08x, not cfe23fbe (the engine before)", r.h);
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
        static const char *const FX[EN_NFX] = {"Repeat", "Time", "Tape stop", "Shimmer", "Low-pass", "High-pass", "Sweep",
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

    /* (the voices batch, item 0) Shimmer, where Swell was (the grid's fourth, black key 5): a sine Lead alone, a key
     * held. At rest the shifter does no work (en_shm_work stays 0). Held, the plate's tail climbs: energy an octave
     * over the Lead's pitch where there was next to none, the third octave well under the first (the climb capped).
     * Let go: no step in the output, the octave still in the tail at first (no cut), then gone with the plate; and
     * the shifter stops working once its tail is out */
    {
        static const int OFF[4] = {P_CHORD, P_SEQ, P_SEQ2, P_NCHAR};
        float f[4], bn[4], bh[4], br[4], bt[4], stn, sth, str, stt, f0, rn, rh;
        uint32_t w0, w_rest, w_held, w_tail;   /* (the work counted from here: the tests before held it too) */
        int k;
        fresh();
        for (k = 0; k < 4; k++)
            en_set(OFF[k], 0);
        en_set(P_MOTION, 0);
        en_set(P_ECHO, 0);
        en_set(P_LDECHO, 0);
        en_set(P_LDREV, 100);
        en_set(P_LVOICE, 2);                     /* (a sine, its breath a whisper; vibrato 0) */
        en_set(P_VIB, 0);
        en_set(P_LDIN, 0);
        en_play(1);
        w0 = en_shm_work;
        run(2.0f);
        w_rest = en_shm_work - w0;
        en_lead_key(0, 1);
        run(1.5f);
        f0 = 440.0f * powf(2.0f, ((float)last_lead - 69.0f) / 12.0f);
        for (k = 0; k < 4; k++)
            f[k] = f0 * (float)(1 << k);         /* the pitch, an octave, two, three over it */
        run_bands(2.0f, f, bn, &stn);
        en_fx(1u << EN_FX_SHIMMER);
        run(3.0f);
        run_bands(1.0f, f, bh, &sth);
        w_held = en_shm_work - w0;
        en_fx(0);
        run_bands(0.3f, f, br, &str);
        run(4.0f);
        run_bands(1.0f, f, bt, &stt);
        w_tail = en_shm_work - w0;
        run(1.0f);
        en_lead_key(0, 0);
        rn = bn[1] / fmaxf(bn[0], 1e-20f), rh = bh[1] / fmaxf(bh[0], 1e-20f);
        printf("  shimmer: Lead %d (%.0f Hz); energy at f 2f 4f 8f: not held %.2e %.2e %.2e %.2e, held %.2e %.2e %.2e "
               "%.2e, let go %.2e %.2e, 4 s on %.2e %.2e; steps %.4f %.4f %.4f %.4f; work %u %u %u %u\n",
               last_lead, (double)f0, (double)bn[0], (double)bn[1], (double)bn[2], (double)bn[3], (double)bh[0],
               (double)bh[1], (double)bh[2], (double)bh[3], (double)br[0], (double)br[1], (double)bt[0],
               (double)bt[1], (double)stn, (double)sth, (double)str, (double)stt, w_rest, w_held, w_tail,
               en_shm_work - w0);
        CHECK(!bad && en_state_ok(), "test_shimmer: NaN or Inf in the plate or the shifter (%d blocks)", bad);
        CHECK(rh > 0.003f && rh > 30.0f * rn, "test_shimmer_octave: the octave against the pitch, held %.2e, not %.2e",
              (double)rh, (double)rn);
        CHECK(bh[3] < bh[1] / 16.0f, "test_shimmer_cap: three octaves up %.2e, not well under one %.2e",
              (double)bh[3], (double)bh[1]);
        CHECK(br[1] > 0.2f * bh[1] && bt[1] < 0.1f * bh[1],
              "test_shimmer_release: the octave held %.2e, let go %.2e (a cut?), 4 s on %.2e (still there?)",
              (double)bh[1], (double)br[1], (double)bt[1]);
        CHECK(str <= 1.25f * sth, "test_shimmer_release: a step %.4f as it's let go (held: %.4f)", (double)str,
              (double)sth);
        CHECK(w_rest == 0u && w_held > 0u && en_shm_work - w0 == w_tail,
              "test_shimmer_rest: the shifter's work at rest %u, held %u, after its tail %u then %u", w_rest, w_held,
              w_tail, en_shm_work - w0);
    }

    /* (Shimmer, fix rounds 1-3: the review found the loop running away at Size >= ~70; then how loud, how heard) held
     * 60 s, against the same 60 s not held (forked from one state): Mirage, Sunrain and Timewind as they are, held
     * 3 to 6 dB over not held across the 60 s (the floor: heard, as Swell wasn't), no 5 s window past +7, the peak
     * under 0.995, almost nothing over 0.9; (ii) Mirage at Size and Reverb 100: no window past +6; (iv) let go: no
     * louder over not held than while held; (iii) Size 100, held, the music stopped and muted: it dies away, it
     * doesn't feed itself */
    {
        static const int PC[4] = {6, 6, 5, 0}, BIG[4] = {0, 1, 0, 0};
        static const char *const NM[4] = {"Mirage", "Mirage at Size/Reverb 100", "Sunrain", "Timewind"};
        int t, k;
        for (t = 0; t < 4; t++) {
            shm_ab_t o;
            shm_ab(PC[t], BIG[t], &o);
            printf("  shimmer held 60 s, %s: %+.1f dB over not held (60 s), a window at most %+.1f, peak %.3f, over 0.9 "
                   "%.4f; its own sound %.1f dB re the mix; let go %+.1f dB over not held, peak %.3f (not held %.3f)\n",
                   NM[t], (double)o.all, (double)o.over, (double)o.pk, (double)o.hot, (double)o.own, (double)o.rel_over,
                   (double)o.rel_pk, (double)o.rel_npk);
            CHECK(!bad && o.over < (BIG[t] ? 6.0f : 7.0f) && o.pk < 0.995f && o.hot < 0.001f,
                  "test_shimmer_bounded (%s): a window %+.1f dB over not held, peak %.3f, over 0.9 %.4f (bad %d)", NM[t],
                  (double)o.over, (double)o.pk, (double)o.hot, bad);
            if (!BIG[t])
                CHECK(o.all >= 3.0f && o.all <= 6.0f, "test_shimmer_heard (%s): %+.1f dB over not held over 60 s (3 .. 6)",
                      NM[t], (double)o.all);
            /* (the peak: at Size/Reverb 100 the long tail lies on the music's own peaks there, +1 dB at most; the
             * level falls all the same) */
            CHECK(o.rel_over <= fmaxf(o.over, 0.0f) + 0.5f &&
                      o.rel_pk <= fmaxf(o.pk, o.rel_npk) * (BIG[t] ? 1.12f : 1.05f),
                  "test_shimmer_release_burst (%s): let go %+.1f dB over not held (held %+.1f), peak %.3f (held %.3f, "
                  "not held %.3f)", NM[t], (double)o.rel_over, (double)o.over, (double)o.rel_pk, (double)o.pk,
                  (double)o.rel_npk);
        }
        {
            float db, pk, hot, first = 0.0f, last = 0.0f;
            fresh();
            piece(6);
            en_set(P_SIZE, 100), en_set(P_REVERB, 100), en_set(P_ECHO, 0), en_set(P_FEEDBACK, 0);
            en_play(1);
            run(15.0f);
            en_fx(1u << EN_FX_SHIMMER);
            run(10.0f);
            en_play(0);
            en_mute(0xFFu);
            printf("  shimmer held, Size 100, the music stopped and muted (5 s windows):");
            for (k = 0; k < 8; k++) {
                run_lvl(5.0f, &db, &pk, &hot);
                printf(" %.1f", (double)db);
                if (!k)
                    first = db;
                last = db;
            }
            printf(" dB\n");
            en_fx(0);
            en_mute(0);
            CHECK(!bad && last < first - 30.0f && last < -70.0f,
                  "test_shimmer_selfosc: held over silence %.1f dB, 35 s on %.1f dB (it feeds itself)", (double)first,
                  (double)last);
        }
    }

    /* (the voices batch, item 2) Pluck: a plucked string (Karplus-Strong), the Sequence alone. (i) In tune from the
     * lowest note either sequence reaches (24, the clamp, at Tune -50: ~31.8 Hz, the delay line's whole length) to the
     * highest (108); (ii) Decay sets how long it rings, as it does the other voices' (their amplitude's T60, 1.5 x
     * 80 .. 250 ms); (iii) about as loud as 2600 at the same knobs; (iv) after a long silence its state finite, no
     * denormals; (v) a voice taken for a new note no louder than the oscillator voices' are, the notes paired */
    {
        static float x[2 * 44100];
        static const int OCT[10] = {-2, -2, -1, -1, 1, 1, 2, 2, 2, 2}, DEG[10] = {0, 0, 0, 5, 0, 5, 0, 7, 14, 21},
                         TUNE[10] = {-50, 0, 0, 0, 0, 50, 0, 0, 0, 0};
        float t60[2], lv[5][2], st[2], sl[2][3];
        int k, n, on;
        for (k = 0; k < 10; k++) {
            const uint32_t s0 = en_ks_struck;
            float f, p, c;
            int note;
            ks_solo(5, OCT[k], DEG[k], TUNE[k], 100);
            n = run_cap(1.2f, x, 2 * 44100);
            on = ks_onset(x, n);
            note = sq_n ? sq_note[0] : -1;
            f = 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f + (float)TUNE[k] / 1200.0f);
            p = on >= 0 ? dft_pitch(x + on, n - on, f) : 0.0f;
            c = p > 0.0f ? 1200.0f * log2f(p / f) : 999.0f;
            printf("  pluck: note %d%+d cents (%.3f Hz): heard at %.3f Hz, %+.2f cents\n", note, TUNE[k], (double)f,
                   (double)p, (double)c);
            CHECK(en_ks_struck > s0 && fabsf(c) < 2.0f && !bad,
                  "test_pluck_pitch: note %d%+d cents %+.2f cents off (struck %u, bad %d)", note, TUNE[k], (double)c,
                  en_ks_struck - s0, bad);
            if (!k)
                CHECK(note == 24, "test_pluck_pitch: the lowest note %d (24)", note);
        }
        for (k = 0; k < 2; k++) {                /* (ii) Decay 0 and 100, note 48 */
            const float nom = 1.5f * 0.08f * powf(2.0f, 0.0164f * (float)(k ? 100 : 0));
            ks_solo(5, 0, 0, 0, k ? 100 : 0);
            n = run_cap(1.2f, x, 2 * 44100);
            on = ks_onset(x, n);
            t60[k] = on >= 0 ? ks_t60(x + on, n - on) : 0.0f;
            printf("  pluck: Decay %d: T60 %.3f s (the other voices' %.3f)\n", k ? 100 : 0, (double)t60[k], (double)nom);
            CHECK(t60[k] > 0.6f * nom && t60[k] < 1.25f * nom, "test_pluck_decay: Decay %d T60 %.3f s (%.3f)",
                  k ? 100 : 0, (double)t60[k], (double)nom);
        }
        CHECK(t60[1] > 2.3f * t60[0] && t60[1] < 4.0f * t60[0], "test_pluck_decay: 100 over 0 x%.2f (x3.1)",
              (double)(t60[1] / fmaxf(t60[0], 1e-6f)));
        for (k = 0; k < 5; k++) {                /* (iii) Pluck and 2600, notes 24 .. 72, the knobs' defaults */
            int v;
            for (v = 0; v < 2; v++) {
                double e = 0.0;
                int i;
                ks_solo(v ? 0 : 5, k - 2, 0, 0, 35);
                n = run_cap(1.0f, x, 2 * 44100);
                on = ks_onset(x, n);
                for (i = on; on >= 0 && i < on + 26460 && i < n; i++)
                    e += (double)x[i] * (double)x[i];
                lv[k][v] = (float)(10.0 * log10(e + 1e-30));
            }
            printf("  pluck: note %d, the note's energy: Pluck %.1f dB, 2600 %.1f dB\n", 48 + 12 * (k - 2),
                   (double)lv[k][0], (double)lv[k][1]);
            CHECK(fabsf(lv[k][0] - lv[k][1]) < 3.0f, "test_pluck_level: note %d Pluck %.1f dB, 2600 %.1f dB",
                  48 + 12 * (k - 2), (double)lv[k][0], (double)lv[k][1]);
        }
        ks_solo(5, -2, 0, 0, 100);               /* (iv) the lowest notes, then 30 s of silence */
        en_set(P_DENSITY, 100);
        run(3.0f);
        CHECK(en_ks_ok(), "test_pluck_state: not finite, or denormal, while playing");
        en_play(0);
        run(30.0f);
        CHECK(en_ks_ok() && !bad, "test_pluck_state: after 30 s of silence not finite, or denormal (bad %d)", bad);
        for (k = 0; k < 2; k++) {                /* (v) both sequences crowded: Pluck, then 2600 and Moog */
            static int was[2][128];              /* (the sequences' channels as the tests before left them: only
                                                  * this run's notes are counted; the last test still sees all) */
            int c, nt, stuck = 0;
            fresh();
            memcpy(was[0], sounding[1], sizeof was[0]);
            memcpy(was[1], sounding[3], sizeof was[1]);
            en_set(P_SVOICE, k ? 0 : 5), en_set(P_S2VOICE, k ? 1 : 6);
            en_set(P_SEQ, 70), en_set(P_SEQ2, 60), en_set(P_DENSITY, 100), en_set(P_TAPA, 100), en_set(P_TAPB, 100);
            en_set(P_TAPAT, 0), en_set(P_TEMPO, 160), en_set(P_DECAY, 100), en_set(P_S2DEC, 100);
            en_set(P_SEQOCT, 1);                 /* (up an octave: the top notes, Pluck's level lifted most) */
            en_pl_steal = 0.0f;
            en_play(1);
            run_lvl(10.0f, &sl[k][0], &sl[k][1], &sl[k][2]);
            st[k] = en_pl_steal;
            en_play(0);
            run(2.0f);
            for (c = 1; c < 4; c += 2)
                for (nt = 0; nt < 128; nt++)
                    stuck += sounding[c][nt] != was[c >> 1][nt];
            CHECK(!stuck && !bad, "test_pluck_notes (%s): %d notes not paired (bad %d)", k ? "2600" : "Pluck", stuck,
                  bad);
        }
        printf("  pluck: a voice taken while sounding, its last sample at most: Pluck %.5f, 2600/Moog %.5f; the mix "
               "%.1f dB, peak %.3f, over 0.9 %.5f (2600/Moog %.1f dB, %.3f, %.5f)\n", (double)st[0], (double)st[1],
               (double)sl[0][0], (double)sl[0][1], (double)sl[0][2], (double)sl[1][0], (double)sl[1][1],
               (double)sl[1][2]);
        CHECK(fabsf(sl[0][0] - sl[1][0]) < 3.0f && sl[0][2] <= sl[1][2] + 0.001f,
              "test_pluck_crowded: the mix %.1f dB (oscillators %.1f), over 0.9 %.5f (%.5f)", (double)sl[0][0],
              (double)sl[1][0], (double)sl[0][2], (double)sl[1][2]);
        CHECK(st[0] <= 1.5f * st[1] + 0.002f, "test_pluck_steal: a voice taken at %.5f (the oscillators' %.5f)",
              (double)st[0], (double)st[1]);
    }

    /* (the voices batch, item 3) Choir: the Drone's formant choir, the Drone alone, about two and a half minutes of
     * music. (i) Its spectrum peaks at the vowel's first two formants (Hillenbrand et al. 1995, men: 'oo' 378 / 997 Hz
     * at Colour -25, 'ah' 768 / 1333 Hz at Colour 0, the move still), and the vowel is where Colour puts it; (ii) the
     * Move page moves the vowel, slowly, with the Drone's filter motion; (iii) about as loud as Strings at the same
     * Colour; (iv) no brighter on top (over 4 kHz) than Strings; (v) a chord's crossfade, the vowel moving, doesn't
     * click; (vi) its state finite, no denormals, after half a minute at level 0 and half a minute stopped */
    {
        static float x[13 * 44100], env[CH_NE];
        static const float F[5][2] = {{378.0f, 997.0f}, {497.0f, 910.0f}, {768.0f, 1333.0f}, {580.0f, 1799.0f},
                                      {342.0f, 2322.0f}};
        float lv[4][2], top[4][2], ck[3], at, pr, ct[2][2];
        int k, n, v, j;
        for (k = 0; k < 2; k++) {                /* (i) 'oo', then 'ah' */
            const int vw = k ? 2 : 0;
            static float env2[CH_NE], h[CH_NE];
            float g[3], best = 0.0f;
            int i;
            ch_solo(1, k ? 0 : -25);             /* (the same chord on Strings: the same notes, its partials where the
                                                  * Choir's are; what the Choir has over it is its formants) */
            run(1.5f);
            run_cap(0.8f, x, 13 * 44100);
            ch_env(x, env2, &at);
            ch_solo(5, k ? 0 : -25);
            run(1.5f);
            n = run_cap(0.8f, x, 13 * 44100);
            ch_env(x, env, &at);
            /* (the balance the vowel sets: 'ah's F1 over 'oo's, 'ah's F2 over 'oo's; a voice without formants moves
             * them only by Colour's low-pass, a few dB) */
            ct[k][0] = env[ch_at(768.0f)] - env[ch_at(378.0f)];
            ct[k][1] = env[ch_at(1333.0f)] - env[ch_at(997.0f)];
            for (i = 0; i < CH_NE; i++)
                h[i] = env[i] - env2[i];
            {                                    /* (over Strings, the most within +-10% of F1 and of F2 against the
                                                  * median over 200 Hz .. 4 kHz: where the Choir puts its energy) */
                static float hs[CH_NE];
                float mx;
                for (i = 0; i < CH_NE; i++)
                    hs[i] = h[i];
                qsort(hs, CH_NE, sizeof hs[0], ch_cmp);
                for (j = 0; j < 2; j++) {
                    mx = -999.0f;
                    for (i = ch_at(0.9f * F[vw][j]); i <= ch_at(1.1f * F[vw][j]); i++)
                        mx = fmaxf(mx, h[i]);
                    pr = mx - hs[CH_NE / 2];
                    printf("  choir: vowel %d, F%d %.0f Hz: over Strings %.1f dB over its median\n", vw, j + 1,
                           (double)F[vw][j], (double)pr);
                    CHECK(n >= CH_FN && pr >= (j ? 3.0f : 6.0f) && !bad,
                          "test_choir_formants: vowel %d F%d %.0f Hz only %.1f dB over Strings' median", vw, j + 1,
                          (double)F[vw][j], (double)pr);
                }
            }
            /* (the formants themselves, as set, whatever the chord: the cascade's gain peaks within 5% of the table's
             * F1 and F2, each standing 3 dB or more over the gain halfway (in octaves) between them) */
            for (j = 0; j < 2; j++) {
                float f, at2 = 0.0f;
                best = 0.0f;
                for (f = 0.9f * F[vw][j]; f <= 1.1f * F[vw][j]; f += 1.0f)
                    if (en_ch_gain(f) > best)
                        best = en_ch_gain(f), at2 = f;
                g[j] = at2;
            }
            g[2] = en_ch_gain(sqrtf(F[vw][0] * F[vw][1]));
            printf("  choir: vowel %d, the formants' own peaks at %.0f and %.0f Hz (%.1f, %.1f dB over the gain between)\n",
                   vw, (double)g[0], (double)g[1], (double)(20.0f * log10f(en_ch_gain(g[0]) / g[2])),
                   (double)(20.0f * log10f(en_ch_gain(g[1]) / g[2])));
            CHECK(fabsf(g[0] / F[vw][0] - 1.0f) < 0.05f && fabsf(g[1] / F[vw][1] - 1.0f) < 0.05f &&
                  en_ch_gain(g[0]) > 1.41f * g[2] && en_ch_gain(g[1]) > 1.41f * g[2],
                  "test_choir_formants: vowel %d, its formants at %.0f and %.0f Hz (%.0f, %.0f)", vw, (double)g[0],
                  (double)g[1], (double)F[vw][0], (double)F[vw][1]);
            CHECK(fabsf(en_ch_vowel - (float)vw) < 0.05f, "test_choir_vowel: Colour %d: the vowel at %.2f (%d)",
                  k ? 0 : -25, (double)en_ch_vowel, vw);
        }
        printf("  choir: 'oo' to 'ah': 768 over 378 Hz %+.1f -> %+.1f dB, 1333 over 997 Hz %+.1f -> %+.1f dB\n",
               (double)ct[0][0], (double)ct[1][0], (double)ct[0][1], (double)ct[1][1]);
        CHECK(ct[1][0] - ct[0][0] >= 10.0f && ct[1][1] - ct[0][1] >= 10.0f,
              "test_choir_formants: 'oo' to 'ah' moved F1's balance %.1f dB, F2's %.1f dB (10)",
              (double)(ct[1][0] - ct[0][0]), (double)(ct[1][1] - ct[0][1]));
        {                                        /* (ii) the Move page: Width and Rate 100, Time 50; 40 s */
            float lo = 9.0f, hi = -9.0f, prev = 0.0f, jump = 0.0f, dev = 0.0f;
            ch_solo(5, 0);
            en_set(P_MW0, 100), en_set(P_MR0, 100), en_set(P_MT0, 50);
            for (k = 0; k < 400; k++) {
                float p, want;
                run(0.1f);
                p = en_ch_vowel, want = fminf(fmaxf(2.0f + 2.0f * en_fo[0], 0.0f), 4.0f);
                lo = fminf(lo, p), hi = fmaxf(hi, p);
                if (k)
                    jump = fmaxf(jump, fabsf(p - prev));
                if (k > 20)
                    dev = fmaxf(dev, fabsf(p - want));
                prev = p;
            }
            printf("  choir: moving, the vowel from %.2f to %.2f, at most %.3f in 0.1 s, %.3f off the motion's\n",
                   (double)lo, (double)hi, (double)jump, (double)dev);
            CHECK(hi - lo >= 1.0f && jump <= 0.25f && dev <= 0.2f && !bad,
                  "test_choir_moves: the vowel %.2f .. %.2f, %.3f in 0.1 s, %.3f off", (double)lo, (double)hi,
                  (double)jump, (double)dev);
        }
        for (k = 0; k < 4; k++)                  /* (iii, iv) Colour -25, 0, 25, 50: Choir, then Strings */
            for (v = 0; v < 2; v++) {
                double e = 0.0;
                ch_solo(v ? 1 : 5, 25 * (k - 1));
                run(1.5f);
                n = run_cap(0.8f, x, 13 * 44100);
                for (j = 0; j < n; j++)
                    e += (double)x[j] * (double)x[j];
                lv[k][v] = (float)(10.0 * log10(e / (double)(n > 0 ? n : 1) + 1e-30));
                ch_env(x, env, &top[k][v]);
            }
        for (k = 0; k < 4; k++) {
            printf("  choir: Colour %d: Choir %.1f dB (over 4 kHz %.1f dB), Strings %.1f dB (%.1f dB)\n", 25 * (k - 1),
                   (double)lv[k][0], (double)top[k][0], (double)lv[k][1], (double)top[k][1]);
            CHECK(k == 3 || fabsf(lv[k][0] - lv[k][1]) < 3.0f, "test_choir_level: Colour %d Choir %.1f dB, Strings %.1f dB",
                  25 * (k - 1), (double)lv[k][0], (double)lv[k][1]);
            CHECK(k < 1 || top[k][0] <= top[k][1], "test_choir_warm: Colour %d over 4 kHz Choir %.1f dB, Strings %.1f dB",
                  25 * (k - 1), (double)top[k][0], (double)top[k][1]);
        }
        {   /* (iii, review round 1) about as loud as Strings at every corner of Reso and Colour (-50, 0, +50 each:
             * narrower formants ring louder, a dark Colour leaves 'oo's F1 standing where Strings' saws are cut),
             * and at Chord 100, Reso +50 and 0, Colour slammed end to end every half second, its peak no more
             * than 2 dB over Strings' */
            static const int RS[3] = {-50, 0, 50}, CL[3] = {-50, 0, 50};
            float lr[3][3][2], pk[2][2], worst = 0.0f;
            int r, c;
            for (r = 0; r < 3; r++)
                for (c = 0; c < 3; c++)
                    for (v = 0; v < 2; v++) {
                        double e = 0.0;
                        ch_solo(v ? 1 : 5, CL[c]);   /* (over 8 s of chords changing every two bars: one chord
                                                      * alone puts its partials near the formants or not, +-1..2 dB) */
                        en_set(P_CRES, RS[r]), en_set(P_DRIFT, 0), en_set(P_ROOT, 5);
                        run(1.5f);
                        n = run_cap(8.0f, x, 13 * 44100);
                        for (j = 0; j < n; j++)
                            e += (double)x[j] * (double)x[j];
                        lr[r][c][v] = (float)(10.0 * log10(e / (double)(n > 0 ? n : 1) + 1e-30));
                    }
            for (r = 0; r < 3; r++) {
                printf("  choir: Reso %+d, Colour -50 / 0 / +50: Choir over Strings %+.1f / %+.1f / %+.1f dB\n", RS[r],
                       (double)(lr[r][0][0] - lr[r][0][1]), (double)(lr[r][1][0] - lr[r][1][1]),
                       (double)(lr[r][2][0] - lr[r][2][1]));
                for (c = 0; c < 3; c++)
                    worst = fmaxf(worst, fabsf(lr[r][c][0] - lr[r][c][1]));
            }
            CHECK(worst <= 2.0f, "test_choir_level_corners: Choir %.1f dB from Strings at its worst corner (2)",
                  (double)worst);
            for (r = 0; r < 2; r++)
                for (v = 0; v < 2; v++) {
                    ch_solo(v ? 1 : 5, -50);
                    en_set(P_CHORD, 100), en_set(P_CRES, r ? 0 : 50);
                    run(1.5f);
                    peak = 0.0f;
                    for (k = 0; k < 12; k++) {
                        en_set(P_CCOL, k & 1 ? -50 : 50);
                        run(0.5f);
                    }
                    pk[r][v] = peak;
                }
            printf("  choir: Chord 100, Colour slammed: peak Choir %.3f / Strings %.3f (Reso +50), %.3f / %.3f (Reso 0)\n",
                   (double)pk[0][0], (double)pk[0][1], (double)pk[1][0], (double)pk[1][1]);
            CHECK(pk[0][0] <= 1.259f * pk[0][1] && pk[1][0] <= 1.259f * pk[1][1] && !bad,
                  "test_choir_level_corners: Chord 100 peak %.3f / %.3f (Strings %.3f / %.3f, +2 dB)", (double)pk[0][0],
                  (double)pk[1][0], (double)pk[0][1], (double)pk[1][1]);
        }
        for (v = 1; v >= 0; v--) {               /* (v) a chord every two bars, quick swells, the vowel moving: 12 s */
            ch_solo(v ? 1 : 5, 0);               /* (fresh: the Drone's notes counted from 0) */
            en_set(P_DRIFT, 0), en_set(P_DREL, 0), en_set(P_MW0, 100), en_set(P_MR0, 100);
            n = run_cap(12.0f, x, 13 * 44100);
            ck[v] = ch_click(x + 44100, n - 44100);
            CHECK(dr_n >= 10u, "test_choir_crossfade: %u Drone notes in 12 s (%s)", dr_n, v ? "Strings" : "Choir");
        }
        {                                        /* (the mark sees a click: Choir's run with a step of its own RMS
                                                  * put in at 6 s) */
            double e = 0.0;
            for (j = 44100; j < n; j++)
                e += (double)x[j] * (double)x[j];
            for (j = 6 * 44100; j < n; j++)
                x[j] += (float)sqrt(e / (double)(n - 44100));
            ck[2] = ch_click(x + 44100, n - 44100);
        }
        printf("  choir: a crossfade's largest step over the 99th percentile: Choir %.2f, Strings %.2f; Choir with a "
               "click put in %.2f\n", (double)ck[0], (double)ck[1], (double)ck[2]);
        CHECK(ck[0] <= 2.0f && ck[2] > 2.0f && !bad, "test_choir_crossfade: a step %.2f x the usual (Strings %.2f; a "
              "click put in %.2f)", (double)ck[0], (double)ck[1], (double)ck[2]);
        ch_solo(5, 0);                           /* (vi) */
        run(3.0f);
        CHECK(en_ch_ok(), "test_choir_state: not finite, or denormal, while playing");
        en_set(P_CHORD, 0);
        run(30.0f);
        CHECK(en_ch_ok(), "test_choir_state: after 30 s at level 0 not finite, or denormal");
        en_set(P_CHORD, 80);
        run(1.0f);
        en_play(0);
        run(30.0f);
        CHECK(en_ch_ok() && !bad, "test_choir_state: after 30 s stopped not finite, or denormal (bad %d)", bad);
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
    {   /* (the user, 2026-10-28) a piece change returns Filter, Wet and Energy to 0: Stratosfear pushed (Energy +50,
         * Wet +30, Filter -20), then Sunrain: the three 0, its Density, levels and sends its recipe's */
        static const int P[] = {P_DENSITY, P_SEQ, P_SEQ2, P_SWEEP, P_DECAY, P_TAPB, P_CUTOFF, P_SQECHO, P_ECHO, P_S2ECHO,
                                P_LDECHO, P_DRECHO};
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        int k, off = 0, pushed;
        fresh();
        piece(4);
        en_play(1);
        run(2.0f);
        en_macro(EN_MAC_ENERGY, 50);
        en_macro(EN_MAC_DUB, 30);
        en_macro(EN_MAC_FILTER, -20);
        run(0.05f);
        en_piece(4, pp, &pl);
        pushed = en_mac[EN_MAC_ENERGY] == 50 && en_mac[EN_MAC_DUB] == 30 && en_mac[EN_MAC_FILTER] == -20 &&
                 en_heard(P_SEQ) != pp[P_SEQ];
        piece(5);
        run(0.05f);
        en_piece(5, pp, &pl);
        for (k = 0; k < (int)(sizeof P / sizeof P[0]); k++)
            if (en_heard(P[k]) != pp[P[k]]) {
                printf("  after the piece change: parameter %d heard %d, its recipe %d\n", P[k], en_heard(P[k]), pp[P[k]]);
                off++;
            }
        CHECK(pushed && !en_mac[EN_MAC_FILTER] && !en_mac[EN_MAC_DUB] && !en_mac[EN_MAC_ENERGY] && !off,
              "test_piece_macros_reset: pushed %d; after the change Filter %d, Wet %d, Energy %d, %d parameters moved",
              pushed, en_mac[EN_MAC_FILTER], en_mac[EN_MAC_DUB], en_mac[EN_MAC_ENERGY], off);
    }
    /* (the user, 2026-10-28) the master actions move by themselves, Motion's (in the knobs' units, -50..50; the screen
     * shows twice: Shut 30 = -15): Sunrain at Motion 0, ten minutes, none moves; at Motion 100 with Arc 1 (its rise
     * 134 bars, the peak at bar 107), Filter within +-15 reaching near both ends in its first two rounds (128 bars),
     * Wet within 0..25 reaching near 25, Energy 0 at the start and near 25 at the rise's peak; never a jump (a step at
     * most between two looks half a second apart) */
    {
        int k, moved = 0, fmin = 99, fmax = -99, wmin = 99, wmax = -99, e0 = 0, epk = 0, jump = 0, was[3] = {0, 0, 0}, i;
        fresh();
        piece(5);
        en_set(P_MOTION, 0);
        en_play(1);
        for (k = 0; k < 1200; k++) {
            run(0.5f);
            moved += en_mac[0] || en_mac[1] || en_mac[2];
        }
        CHECK(!moved, "test_mac_motion0: the master actions moved in %d of 1200 looks at Motion 0", moved);
        fresh();
        piece(5);
        en_set(P_MOTION, 100);
        en_set(P_ARC, 1);
        en_play(1);
        for (k = 0; (int)(en_bars / 16u) < 128; k++) {
            const int bar = (int)(en_bars / 16u);
            run(0.5f);
            fmin = en_mac[0] < fmin ? en_mac[0] : fmin, fmax = en_mac[0] > fmax ? en_mac[0] : fmax;
            wmin = en_mac[1] < wmin ? en_mac[1] : wmin, wmax = en_mac[1] > wmax ? en_mac[1] : wmax;
            if (bar < 4)
                e0 = en_mac[2] > e0 ? en_mac[2] : e0;
            if (bar >= 104 && bar <= 110)
                epk = en_mac[2] > epk ? en_mac[2] : epk;
            for (i = 0; i < 3; i++) {
                jump += abs(en_mac[i] - was[i]) > 1;
                was[i] = en_mac[i];
            }
        }
        printf("  master actions, Motion 100, 128 bars: Filter %d..%d, Wet %d..%d, Energy %d at the start, %d at the peak\n",
               fmin, fmax, wmin, wmax, e0, epk);
        CHECK(fmin >= -15 && fmin <= -13 && fmax <= 15 && fmax >= 13, "test_mac_filter: Filter %d..%d", fmin, fmax);
        CHECK(wmin >= 0 && wmax <= 25 && wmax >= 23, "test_mac_wet: Wet %d..%d", wmin, wmax);
        CHECK(e0 <= 1 && epk >= 23 && epk <= 25, "test_mac_energy: Energy %d at the start, %d at the peak", e0, epk);
        CHECK(!jump, "test_mac_smooth: %d jumps", jump);
        /* Arc 0: Energy stays 0 (no arc, no build), the Filter moves */
        fresh();
        piece(5);
        en_set(P_MOTION, 100);
        en_set(P_ARC, 0);
        en_play(1);
        for (moved = 0, e0 = 0, k = 0; k < 240; k++) {
            run(0.5f);
            e0 += en_mac[2] != 0;
            moved += en_mac[0] != 0;
        }
        CHECK(!e0 && moved, "test_mac_arc0: Energy off 0 in %d looks, Filter moved in %d", e0, moved);
        /* the hand holds: Energy +40 by hand (+80 on the screen) held till the section changes, then the motion takes
         * it back without a jump (eight bars), well under where the hand left it */
        {
            int sec, held = 1, bar0, hand_jump = 0, v = 40;
            fresh();
            piece(5);
            en_set(P_MOTION, 100);
            en_play(1);
            run(8.0f);
            en_macro(EN_MAC_ENERGY, 40);
            sec = en_section;
            for (k = 0; k < 2400 && en_section == sec; k++) {
                run(0.25f);
                held &= en_section != sec || en_mac[2] == 40;
            }
            bar0 = (int)(en_bars / 16u);
            for (k = 0; (int)(en_bars / 16u) < bar0 + 12 && k < 400; k++) {
                run(0.25f);
                hand_jump += abs(en_mac[2] - v) > 1;
                v = en_mac[2];
            }
            printf("  Energy by hand 40: held %d till the section change (%d looks), then %d twelve bars on\n", held, k, v);
            CHECK(held && sec != en_section && !hand_jump && v <= 26,
                  "test_mac_hand: held %d, section %d -> %d, %d jumps, %d twelve bars after", held, sec, en_section,
                  hand_jump, v);
        }
        /* Lock holds all three where they are; let go, they go on without a jump */
        {
            int same = 1, w[3], lock_jump = 0;
            fresh();
            piece(5);
            en_set(P_MOTION, 100);
            en_play(1);
            run(40.0f);
            en_lock(1);
            run(0.05f);
            for (i = 0; i < 3; i++)
                w[i] = en_mac[i];
            for (k = 0; k < 80; k++) {
                run(0.5f);
                for (i = 0; i < 3; i++)
                    same &= en_mac[i] == w[i];
            }
            en_lock(0);
            for (k = 0; k < 80; k++) {
                run(0.5f);
                for (i = 0; i < 3; i++) {
                    lock_jump += abs(en_mac[i] - w[i]) > 1;
                    w[i] = en_mac[i];
                }
            }
            CHECK(same && !lock_jump, "test_mac_lock: held %d, %d jumps after", same, lock_jump);
        }
        /* a sequence the piece keeps silent (level 0) stays silent under Energy (Cyborg's Sequence 2); at 1, pushed */
        fresh();
        en_set(P_SEQ2, 0);
        en_macro(EN_MAC_ENERGY, 50);
        run(0.05f);
        k = en_heard(P_SEQ2);
        en_set(P_SEQ2, 1);
        run(0.05f);
        CHECK(k == 0 && en_heard(P_SEQ2) == 31, "test_energy_silent_seq: Sequence 2 at 0 heard %d, at 1 heard %d", k,
              en_heard(P_SEQ2));
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

    /* (BERLIN, the Lead's memory) the Lead's numbers over 20 minutes of Stratosfear, Motion 100 (Solo comes every few
     * sections), Memory 70: its motifs kept a block at a time and played again (the steps and the turns hold when
     * blocks replay) */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        lm_t m;
        fresh();
        piece(4);
        en_piece(4, pp, &pl);
        lg_pool = pl, lg_root = pp[P_ROOT];
        en_set(P_MOTION, 100);
        en_set(P_LMEM, 70);
        en_play(1);
        lead_metrics(1200.0f, &m);
        lm_print("test_lead_metrics: 20 min of Stratosfear's Lead, Motion 100, Memory 70", &m);
        CHECK(m.notes > 100u && m.phrases > 20u, "test_lead_metrics: %u notes, %u phrases", m.notes, m.phrases);
        /* (spec 5, 7.2: at Memory 70 a pair's call is one of the 4 before it again, exactly or transposed, 50 .. 75% of
         * the time: remembered, but not a loop) */
        CHECK(m.pair_recur >= 0.5f && m.pair_recur <= 0.75f && m.compared >= 30u,
              "test_recurrence: Memory 70: %.1f%% of %u calls recur (50 .. 75)", (double)(100.0f * m.pair_recur),
              m.compared);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
        /* (the generator: mostly steps, as Temperley's Essen lines, its motifs kept and varied too; after a leap of 5
         * semitones or more, back the other way at least 60% of the time: over this run, test_pairs' and
         * test_theme_callbacks', 5 leaps or more: remembered motifs, the smoothest of a few draws kept and their
         * endings reached by a step or a third, leap seldom: under 10 in the 90 minutes, Task 4) */
        CHECK(m.step_share > 0.5f && m.intervals >= 100u, "test_motif_steps: %.1f%% of %u intervals 2 semitones or less",
              (double)(100.0f * m.step_share), m.intervals);
        lt_leaps = m.leaps;
        lt_turns = (uint32_t)(m.leap_turn * (float)m.leaps + 0.5f);
        lg_tension();
    }

    /* (BERLIN, the Lead's memory) pairs: 20 minutes in D minor at 140 BPM, the walk's chords, Motion 100, Lead in 0,
     * Phrases 100, Memory 0 (a fresh motif every pair), Arc 1 (its rise every 134 bars). Every call answered (the next
     * phrase); the answer ends on the chord's root or fifth, the call on another chord tone or the 9th (the chord as it
     * was at the phrase's start). The breath inside a pair 1 .. 2 beats (the user, listening to Wahnfried: a call
     * left two bars without its answer "feels unsatisfying"; spec 2: short breaths inside a pair, long ones between).
     * At Memory 0 a call recurs under 10% of the time (spec 5). Intensity (spec 3.1): in Solo near the rise's peak
     * (planned at 0.8 or more) the pairs' phrases have more notes and sit higher than in Flow */
    {
        const uint32_t pl = EN_MODE_POOL[MODE_AEOLIAN];
        uint32_t i, calls = 0, answers = 0, unanswered = 0, late = 0, bad_call = 0, bad_ans = 0, nochord = 0;
        uint32_t ph[2] = {0, 0}, nt[2] = {0, 0}, pitch[2] = {0, 0};   /* (phrases, their notes, their pitches: Flow,
                                                                       * Solo near the peak) */
        lm_t m;
        br_t br;
        fresh();
        en_set(P_ROOT, 2);
        en_pool(pl);
        lg_pool = pl, lg_root = 2;
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_set(P_MOTION, 100);
        en_set(P_TEMPO, 140);
        en_set(P_ARC, 1);
        en_set(P_LMEM, 0);
        en_play(1);
        lead_metrics(1200.0f, &m);
        for (i = 0; i < lg_n; i++) {            /* (intensity: each call's and answer's notes and pitches, by where it
                                                  * was planned) */
            const int w = lg[i].sec == EN_SEC_FLOW ? 0 : lg[i].sec == EN_SEC_SOLO && lg[i].lint >= 80u ? 1 : -1;
            uint32_t j;
            if (!lg_first(i) || (lg[i].role != LR_CALL && lg[i].role != LR_ANSWER) || w < 0)
                continue;
            ph[w]++;
            for (j = i;; j++) {
                nt[w]++, pitch[w] += lg[j].note;
                if (lg_last(j))
                    break;
            }
        }
        lg_breath(&br);
        br_print("test_pairs: 140 BPM", &br);
        CHECK(br.n >= 20u && br.lo >= 4u && br.hi <= 8u,
              "test_call_answer_breath: 140 BPM: %u pairs, the breath %u .. %u sixteenths (1 .. 2 beats: 4 .. 8)", br.n,
              br.lo, br.hi);
        CHECK(m.pair_recur < 0.1f && m.compared >= 20u, "test_recurrence: Memory 0: %.1f%% of %u calls recur (under 10)",
              (double)(100.0f * m.pair_recur), m.compared);
        printf("  test_intensity: Flow %u phrases, %.2f notes, mean pitch %.1f; Solo near the peak %u phrases, %.2f "
               "notes, mean pitch %.1f\n", ph[0], ph[0] ? (double)nt[0] / (double)ph[0] : 0.0,
               nt[0] ? (double)pitch[0] / (double)nt[0] : 0.0, ph[1], ph[1] ? (double)nt[1] / (double)ph[1] : 0.0,
               nt[1] ? (double)pitch[1] / (double)nt[1] : 0.0);
        CHECK(ph[0] >= 10u && ph[1] >= 10u && nt[1] * ph[0] > nt[0] * ph[1] && pitch[1] * nt[0] > pitch[0] * nt[1],
              "test_intensity: Solo near the peak not busier and higher than Flow (phrases %u / %u)", ph[0], ph[1]);
        lt_leaps += m.leaps;
        lt_turns += (uint32_t)(m.leap_turn * (float)m.leaps + 0.5f);
        en_set(P_TEMPO, en_param_info(P_TEMPO)->def);
        en_set(P_ARC, 0);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
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
                ok = j + 1 >= lg_n;                  /* (the run's last phrase: its ending maybe not played yet) */
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
                if (pc != pcs[0] && pc != p5 && j + 1 < lg_n) {
                    if (!bad_ans) {
                        uint32_t k;
                        printf("  an answer ending on %d (chord root %d, fifth %d), at %u:", pc, pcs[0], p5, lg[i].on);
                        for (k = i; k <= j; k++)
                            printf(" %u/%u", lg[k].note, lg[k].off - lg[k].on);
                        printf("\n");
                    }
                    bad_ans++;
                }
            }
        }
        lm_print("test_pairs: 20 min, D minor, Motion 100, Memory 0", &m);
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

    /* (BERLIN, the Lead's memory) the theme's returns (spec 3.2, 7.4): 20 minutes of Wahnfried (80 BPM), Motion 70,
     * Lead in 0: the theme (slot 0) begins a phrase (a call, its answer, a sentence's) at least once in every Solo and
     * every Bridge the run saw whole (each section's span as it played, not the Lead's log: a Solo without a note
     * fails); its returns per section printed (phrases of the theme begun in it). And the breath inside a pair
     * at 80 BPM: 1 .. 2 beats, every pair (the user's ruling, as test_pairs' at 140) */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        uint32_t s, i, seen[EN_NSEC] = {0}, themed[EN_NSEC] = {0}, returns[EN_NSEC] = {0};
        lm_t m;
        br_t br;
        fresh();
        piece(1);
        en_piece(1, pp, &pl);
        lg_pool = pl, lg_root = pp[P_ROOT];
        en_set(P_MOTION, 70);
        en_set(P_LDIN, 0);
        en_play(1);
        lg_n = 0, lg_open = -1;
        run_spans(1200.0f);
        for (s = 0; s + 1 < sp_n; s++) {         /* (the last span still playing: not counted) */
            uint32_t k = 0;
            if (sp_sec[s] != EN_SEC_SOLO && sp_sec[s] != EN_SEC_BRIDGE)
                continue;
            for (i = 0; i < lg_n; i++)
                k += lg_first(i) && lg[i].slot == 0 && lg[i].on >= sp_on[s] && lg[i].on < sp_off[s] &&
                     (lg[i].role == LR_CALL || lg[i].role == LR_ANSWER || lg[i].role == LR_SEQ);
            seen[sp_sec[s]]++;
            themed[sp_sec[s]] += k > 0u;
            returns[sp_sec[s]] += k;
            if (!k)
                printf("  a %s (sixteenths %u .. %u) without the theme\n", EN_SEC_NAME[sp_sec[s]], sp_on[s], sp_off[s]);
        }
        printf("  test_theme_callbacks: Wahnfried 20 min, Motion 70: the theme in %u of %u Solos (%.2f returns each), "
               "%u of %u Bridges (%.2f each)\n", themed[EN_SEC_SOLO], seen[EN_SEC_SOLO],
               seen[EN_SEC_SOLO] ? (double)returns[EN_SEC_SOLO] / (double)seen[EN_SEC_SOLO] : 0.0, themed[EN_SEC_BRIDGE],
               seen[EN_SEC_BRIDGE], seen[EN_SEC_BRIDGE] ? (double)returns[EN_SEC_BRIDGE] / (double)seen[EN_SEC_BRIDGE] : 0.0);
        CHECK(seen[EN_SEC_SOLO] >= 2u && seen[EN_SEC_BRIDGE] >= 2u && themed[EN_SEC_SOLO] == seen[EN_SEC_SOLO] &&
                  themed[EN_SEC_BRIDGE] == seen[EN_SEC_BRIDGE],
              "test_theme_callbacks: the theme in %u of %u Solos, %u of %u Bridges", themed[EN_SEC_SOLO],
              seen[EN_SEC_SOLO], themed[EN_SEC_BRIDGE], seen[EN_SEC_BRIDGE]);
        lg_numbers(&m);
        lm_print("test_theme_callbacks: Wahnfried 20 min, Motion 70", &m);
        lt_leaps += m.leaps;
        lt_turns += (uint32_t)(m.leap_turn * (float)m.leaps + 0.5f);
        CHECK(lt_leaps >= 5u && 10u * lt_turns >= 6u * lt_leaps,
              "test_motif_leap_turn: a turn after %u of %u leaps (this run, test_lead_metrics' and test_pairs')", lt_turns,
              lt_leaps);
        lg_breath(&br);
        br_print("test_theme_callbacks: Wahnfried, 80 BPM", &br);
        CHECK(br.n >= 15u && br.lo >= 4u && br.hi <= 8u && !br.unanswered,
              "test_call_answer_breath: 80 BPM: %u pairs, the breath %u .. %u sixteenths (1 .. 2 beats: 4 .. 8), %u "
              "unanswered", br.n, br.lo, br.hi, br.unanswered);
    }

    /* (BERLIN, the Lead's memory) the register follows the arc, the theme's too (review 1: an octave lift kept in the
     * theme ratcheted it up for good): Cyborg (70 BPM), Arc 1 (its rise every 134 bars, ~7.7 minutes), Motion 100 (a
     * Solo every few sections), Lead in 0, Memory 50, 25 minutes (three rises: the one long run, its fault showed only
     * over several arcs). In Flow the theme's mean pitch within 7 semitones of the line's other motifs'; near the rise's
     * peak (Solo, planned at 0.8 or more) the theme higher than in Flow. Intensity at Memory 50 too (spec 3.1;
     * test_intensity's Memory 0 in test_pairs): the pairs' phrases in the rise's top (Solo planned at 0.7 or more: at 0.8
     * too few in Cyborg's slow pairs) higher than in Flow, their notes printed */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        uint32_t i, j, nn[5] = {0}, sum[5] = {0}, ph[5] = {0};   /* (notes, their pitches, phrases: the theme in Flow, the
                                                                    * rest in Flow, the theme near the peak; all pairs'
                                                                    * phrases in Flow, near the peak) */
        lm_t m;
        fresh();
        piece(7);
        en_piece(7, pp, &pl);
        lg_pool = pl, lg_root = pp[P_ROOT];
        en_set(P_MOTION, 100);
        en_set(P_LDIN, 0);
        en_set(P_ARC, 1);
        en_set(P_LMEM, 50);
        en_play(1);
        lead_metrics(1500.0f, &m);
        for (i = 0; i < lg_n; i++) {
            const int fl = lg[i].sec == EN_SEC_FLOW, pk = lg[i].sec == EN_SEC_SOLO && lg[i].lint >= 80u,
                      hi = lg[i].sec == EN_SEC_SOLO && lg[i].lint >= 70u;
            int w[2] = {-1, -1}, q;
            if (!lg_first(i))
                continue;
            w[0] = fl ? (lg[i].slot ? 1 : 0) : pk && !lg[i].slot ? 2 : -1;
            if (lg[i].role == LR_CALL || lg[i].role == LR_ANSWER)
                w[1] = fl ? 3 : hi ? 4 : -1;
            for (q = 0; q < 2; q++) {
                if (w[q] < 0)
                    continue;
                ph[w[q]]++;
                for (j = i;; j++) {
                    nn[w[q]]++, sum[w[q]] += lg[j].note;
                    if (lg_last(j))
                        break;
                }
            }
        }
        lm_print("test_theme_register: Cyborg 25 min, Arc 1, Motion 100, Memory 50", &m);
        printf("  test_theme_register: the theme in Flow %.1f (%u phrases), the rest in Flow %.1f (%u), the theme near "
               "the peak %.1f (%u)\n", nn[0] ? (double)sum[0] / (double)nn[0] : 0.0, ph[0],
               nn[1] ? (double)sum[1] / (double)nn[1] : 0.0, ph[1], nn[2] ? (double)sum[2] / (double)nn[2] : 0.0, ph[2]);
        printf("  test_intensity: Memory 50: Flow %u phrases, %.2f notes, mean pitch %.1f; Solo at 0.7+ %u phrases, "
               "%.2f notes, mean pitch %.1f\n", ph[3], ph[3] ? (double)nn[3] / (double)ph[3] : 0.0,
               nn[3] ? (double)sum[3] / (double)nn[3] : 0.0, ph[4], ph[4] ? (double)nn[4] / (double)ph[4] : 0.0,
               nn[4] ? (double)sum[4] / (double)nn[4] : 0.0);
        CHECK(ph[0] >= 4u && ph[1] >= 4u && ph[2] >= 2u && 7u * nn[0] * nn[1] >= (sum[0] * nn[1] > sum[1] * nn[0] ?
              sum[0] * nn[1] - sum[1] * nn[0] : sum[1] * nn[0] - sum[0] * nn[1]) && sum[2] * nn[0] > sum[0] * nn[2],
              "test_theme_register: the theme in Flow %u phrases, the rest %u, the theme near the peak %u: in Flow within "
              "7 semitones of the line, higher near the peak?", ph[0], ph[1], ph[2]);
        CHECK(ph[3] >= 8u && ph[4] >= 8u && sum[4] * nn[3] > sum[3] * nn[4],
              "test_intensity: Memory 50: Solo at 0.7+ not higher than Flow (phrases %u / %u)", ph[3], ph[4]);
        en_set(P_ARC, 0);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
    }

    /* (BERLIN, the Lead's memory) Memory turned in the middle of a block (Review Focus 5): D minor, Flow for ever
     * (Motion 0: intensity 0.2, no section's or peak's return of the theme to cut a block), Lead in 0, Phrases 100. At
     * Memory 100 a block is 2 pairs on one motif; turned to 0 after its first call, that block still plays its 2 pairs,
     * and the blocks after it are 1 pair each; turned back to 100 after such a block's call, it stays 1 pair and the
     * next is 2 again. The turned block is the line's idea heard twice already: its second call is its first again
     * (Memory 100's, held for the block: no variation), not varied as Memory 0 would. Every call answered, one note at
     * a time, nothing left sounding */
    {
        const uint32_t pl = EN_MODE_POOL[MODE_AEOLIAN];
        uint32_t i, k, left = 0;
        int got = 1, ok = 1, b, same = 1, ideaA = 0;
        uint8_t bk[6];                           /* (the blocks watched: A begun at 100, turned to 0; B, C at 0; D begun
                                                  * at 0, turned to 100; E at 100; F only begun) */
        br_t br;
        fresh();
        memset(sounding[2], 0, sizeof sounding[2]);
        en_set(P_ROOT, 2);
        en_pool(pl);
        lg_pool = pl, lg_root = 2;
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_set(P_MOTION, 0);
        en_set(P_ARC, 0);
        en_set(P_LMEM, 100);
        ld_ons = ld_mono_bad = ld_on_now = 0;
        en_play(1);
        lg_n = 0, lg_open = -1;
        got &= lg_wait_block(400.0f);            /* (past the first block, the theme, and the second, the idea's first:
                                                  * the third keeps the idea, heard twice) */
        got &= lg_wait_block(400.0f);
        got &= lg_wait_block(400.0f);
        bk[0] = en_lead_blk;
        en_set(P_LMEM, 0);
        for (b = 1; b < 4 && got; b++) {
            got &= lg_wait_block(400.0f);
            bk[b] = en_lead_blk;
        }
        en_set(P_LMEM, 100);
        got &= lg_wait_block(400.0f);
        bk[4] = en_lead_blk;
        got &= lg_wait_block(400.0f);
        bk[5] = en_lead_blk;
        en_play(0);
        run(1.0f);
        for (b = 0; b < 5 && got; b++) {         /* (each watched block: its calls, on one slot) */
            uint32_t calls = 0, slot = 99, c0 = 0;
            const uint32_t want = b == 0 || b == 4 ? 2u : 1u;
            for (i = 0; i < lg_n; i++)
                if (lg_first(i) && lg[i].role == LR_CALL && lg[i].blk == bk[b]) {
                    if (calls && lg[i].slot != slot)
                        ok = 0;
                    if (!b && calls == 1u && !lg_body(c0, i)) {   /* (block A, its idea heard twice before: turned
                                                  * to 0 after its first call, its second as the first, Memory 100's) */
                        printf("  test_memory_turn: block A's second call not its first again\n");
                        same = 0;
                    }
                    if (!b && !calls)
                        c0 = i, ideaA = lg[i].slot != 0;
                    slot = lg[i].slot;
                    calls++;
                }
            if (calls != want) {
                printf("  test_memory_turn: block %d (%u): %u calls, %u wanted\n", b, bk[b], calls, want);
                ok = 0;
            }
        }
        lg_breath(&br);
        for (k = 0; k < 128u; k++)
            left += sounding[2][k] != 0;
        CHECK(got && ok && same && ideaA && !br.unanswered && !ld_mono_bad && !left,
              "test_memory_turn: blocks seen %d, lengths as decided %d, block A's calls the same %d (its idea %d), %u calls "
              "unanswered, %u notes over another, %u left sounding", got, ok, same, ideaA, br.unanswered, ld_mono_bad,
              left);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
    }

    /* (BERLIN, the Lead's memory) motifs played again over written chords of other kinds (Task 3's open item): F
     * Dorian, Loop of Fm7, Bb, Csus4, Eb, Gm (a change every two bars), Memory 70, Motion 0, 6 minutes. A held note
     * (6 sixteenths or more, not its phrase's last) off the chord at its phrase's start (as the Lead takes a written
     * chord: its root, third, fifth and seventh, each on the scale's nearest degree) is a tension, the next note 1 or 2 semitones under it on that chord.
     * The steps printed (short calm motifs over chords a third apart: under half here before blocks too) */
    {
        static const int CR[5] = {5, 10, 0, 3, 7}, CQ[5] = {EN_CQ_MINOR, EN_CQ_MAJOR, EN_CQ_SUS4, EN_CQ_MAJOR, EN_CQ_MINOR};
        static const int CX[5] = {EN_CX_7, EN_CX_TRIAD, EN_CX_TRIAD, EN_CX_TRIAD, EN_CX_TRIAD};
        const uint32_t pl = EN_MODE_POOL[MODE_DORIAN];
        uint32_t i, held = 0, unres = 0, cp = 0, mask[5];
        int k, j, iv[4], st[12];
        lm_t m;
        en_scale(pl, st);
        for (k = 0; k < 5; k++)                  /* (each chord's tones as the Lead takes them: its seventh's form, each
                                                  * tone on the scale's degree nearest it) */
            for (en_chord_iv(CQ[k], EN_CX_7, iv), mask[k] = 0, j = 0; j < 4; j++)
                mask[k] |= 1u << (5 + st[en_deg_of(pl, CR[k] + iv[j] - 5)]) % 12;
        fresh();
        en_set(P_ROOT, 5);
        en_pool(pl);
        lg_pool = pl, lg_root = 5;
        en_set(P_DRIFT, 0);
        for (k = 0; k < EN_NPROG; k++)
            en_set(P_CR0 + k, k < 5 ? CR[k] : 0), en_set(P_CQ0 + k, k < 5 ? CQ[k] : EN_CQ_EMPTY),
                en_set(P_CX0 + k, k < 5 ? CX[k] : 0);
        en_set(P_PMODE, EN_PLOOP);
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_set(P_MOTION, 0);
        en_set(P_LMEM, 70);
        en_regrow();
        en_play(1);
        lead_metrics(360.0f, &m);
        for (i = 0; i < lg_n; i++) {
            int dn;
            if (lg_first(i))
                for (cp = 0, k = 0; k < 5; k++)
                    cp |= CR[k] == lg[i].croot ? mask[k] : 0u;
            if (lg_last(i) || lg[i].off == 0xFFFFFFFFu || lg[i].off - lg[i].on < 6u || cp >> lg[i].note % 12u & 1u)
                continue;
            held++;
            dn = (int)lg[i].note - (int)lg[i + 1].note;
            if (dn < 1 || dn > 2 || !(cp >> lg[i + 1].note % 12u & 1u)) {
                if (!unres)
                    printf("  held %u (%u sixteenths) then %u (the chord's pitch classes %03x)\n", lg[i].note,
                           lg[i].off - lg[i].on, lg[i + 1].note, cp);
                unres++;
            }
        }
        lm_print("test_motif_written_chords: 6 min, F Dorian, five written chords, Memory 70", &m);
        printf("  test_motif_written_chords: %u held tones off the chord, %u not resolved\n", held, unres);
        CHECK(m.phrases >= 20u && !unres,
              "test_motif_written_chords: %u phrases, %u held tones off the chord not resolved a step down onto it",
              m.phrases, unres);
        en_set(P_PMODE, en_param_info(P_PMODE)->def);
        en_set(P_DRIFT, en_param_info(P_DRIFT)->def);
        for (k = 0; k < EN_NPROG; k++)
            en_set(P_CR0 + k, 0), en_set(P_CQ0 + k, EN_CQ_EMPTY), en_set(P_CX0 + k, 0);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
    }

    /* (BERLIN, the Lead's memory, spec 3.5 and 7.3; Part 3, the user: "it basically repeats what I played") the white
     * keys write the theme, and the Lead answers them: D Aeolian at 120, Motion 0 (Flow), the chord held, Lead in 0
     * (the Lead's own phrases going on). 2 keys (a gesture): nothing taken, the theme as it was. 4 keys within a bar
     * (degrees 2 4 3 6, 4 2 2 8 sixteenths, legato): the theme the figure (its degrees from the chord's root, its
     * lengths); not played back: the Lead's next phrase is its answer (LR_ANSWER on slot 0, the figure's rhythm, its
     * notes not the figure's in half the places or more but related (review 1): their mean within 3 degrees of the
     * figure's, its first a step or a third from the figure's last, its strong beats on chord tones next to the
     * figure's there, within 2 degrees), beginning 1-2 beats after the figure closed (two beats after the last let go:
     * 11 .. 17 sixteenths after it) */
    {
        static const int FG[4] = {2, 4, 3, 6}, FD[4] = {4, 2, 2, 8}, GG[2] = {4, 5}, GD[2] = {4, 4};
        const uint32_t pl = EN_MODE_POOL[MODE_AEOLIAN];
        int8_t off0[32], off[32];
        uint8_t dur0[32], dur[32];
        int n0, n1, nt, r, k, inbuf, okt, ans, dif, dm;
        uint32_t s, bl;
        fresh();
        en_set(P_ROOT, 2);
        en_pool(pl);
        lg_pool = pl, lg_root = 2;
        en_set(P_TEMPO, 120);
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_set(P_MOTION, 0);
        en_set(P_DRIFT, 6);                      /* (Hold: the chord stays) */
        en_play(1);
        run(12.0f);                              /* (the theme thought of: its 4 draws, a bar each) */
        n0 = en_lead_theme(off0, dur0);
        keys_play(GG, GD, 2);
        inbuf = en_lead_cap();
        run(16.0f);
        n1 = en_lead_theme(off, dur);
        CHECK(inbuf == 2 && en_lead_cap() == 0 && n0 > 0 && n1 == n0 && !memcmp(off, off0, (size_t)n0) &&
                  !memcmp(dur, dur0, (size_t)n0),
              "test_capture: 2 keys: %d notes in the buffer, %d after; the theme %d entries, then %d (a gesture "
              "changes nothing)", inbuf, en_lead_cap(), n0, n1);
        bar_next();
        lg_n = 0, lg_open = -1;
        s = keys_now(FG, FD, 4);
        run_to(s + 8u * 16u);
        nt = en_lead_theme(off, dur);
        r = en_deg_of(pl, en_chord_root - 2);
        okt = nt == 4;
        for (k = 0; k < 4 && okt; k++)
            okt &= off[k] == FG[k] - r && dur[k] == FD[k];
        ans = lg_answer(4, 0, 4, &dif, &dm);
        bl = lg_n > 4u ? lg[4].on : 0;
        if (!okt || !ans)
            for (k = 0; k < (int)lg_n && k < 16; k++)
                printf("  test_capture: note %u at +%d for %d, phrase %u, role %u, slot %u\n", lg[k].note,
                       (int)(lg[k].on - lg[0].on), (int)(lg[k].off - lg[k].on), lg[k].phr, lg[k].role, lg[k].slot);
        CHECK(okt && ans && 2 * dif >= 4 && dm <= 3 * 4 && dm >= -3 * 4 && la_first <= 2 && la_strong <= 2 &&
                  bl >= s + 11u && bl <= s + 17u,
              "test_capture: 4 keys: the theme the figure %d (%d entries); its answer %d (its notes not the figure's "
              "in %d places of 4, >= 2; its mean %+d/4 degrees from the figure's, within 3; its first %d degrees from "
              "the figure's last, its strong beats at most %d from the figure's, both within 2), at +%d sixteenths "
              "after the last let go (11 .. 17)", okt, nt, ans, dif, dm, la_first, la_strong, (int)(bl - s));
        printf("  test_capture: the answer %d sixteenths after the last key let go\n", (int)(bl - s));
        {                                        /* (a player's timing: each key pressed and let go 30 ms late or 20
                                                  * ms early, a quarter of a sixteenth: the figure on the grid all the
                                                  * same, its lengths as meant) */
            static const int LG[4] = {5, 4, 2, 3}, LD[4] = {2, 6, 4, 4};
            static const float JIT[5] = {0.03f, -0.02f, 0.03f, 0.03f, -0.02f};
            int ok;
            bar_next();
            s = en_bars + 16u;
            for (k = 0; k <= 4; k++) {           /* (each key on sixteenth s: its tick + JIT[k], at 120 a sixteenth
                                                  * 125 ms after the tick before) */
                run_to(s);
                run(0.125f + JIT[k]);
                if (k)
                    en_lead_key(LG[k - 1], 0);
                if (k < 4) {
                    en_lead_key(LG[k], 1);
                    s += (uint32_t)LD[k];
                }
            }
            run(16.0f);
            nt = en_lead_theme(off, dur);
            r = en_deg_of(pl, en_chord_root - 2);
            ok = nt == 4;
            for (k = 0; k < 4 && ok; k++)
                ok &= off[k] == LG[k] - r && dur[k] == LD[k];
            CHECK(ok, "test_capture: keys a little off the grid: the theme %d entries, lengths %d %d %d %d (2 6 4 4)", nt,
                  dur[0], dur[1], dur[2], dur[3]);
        }
    }

    /* (BERLIN, the Lead's memory, spec 7.3; Part 3) a captured theme is answered in any section, whatever Lead in and
     * the roll: (a) in Emerge (the Drone's Intro 24, Lead in 0): the keys' answer (lg_answer), in Emerge, and no other
     * Lead phrase in Emerge; (b) before Lead in (64 bars, Flow): the keys' answer, and the Lead's own phrases still
     * waiting (nothing else from it in the 8 bars after) */
    {
        static const int FG[5] = {9, 8, 7, 8, 10}, FD[5] = {2, 2, 4, 4, 4};
        int pass;
        for (pass = 0; pass < 2; pass++) {
            uint32_t s, i, other = 0, insec = 0;
            int ans, dif, dm;
            fresh();
            lg_pool = EN_MODE_POOL[MODE_AEOLIAN], lg_root = 2;
            en_set(P_ROOT, 2);
            en_pool(lg_pool);
            en_set(P_TEMPO, 120);
            en_set(P_LEAD, 60);
            en_set(P_LDENS, 100);
            en_set(P_MOTION, 0);
            en_set(P_DRIFT, 6);
            en_set(P_LDIN, pass ? 64 : 0);
            en_set(P_MI0, pass ? 0 : 24);
            en_play(1);
            run(6.0f);
            bar_next();
            lg_n = 0, lg_open = -1;
            s = keys_now(FG, FD, 5);
            run_to(s + 8u * 16u);
            ans = lg_answer(5, 0, 5, &dif, &dm);
            for (i = 5; i < lg_n; i++) {
                other += !ans || i >= 10u;
                insec += lg[i].sec == (pass ? EN_SEC_FLOW : EN_SEC_EMERGE);
            }
            if (!ans)
                for (i = 0; i < lg_n && i < 16u; i++)
                    printf("  test_capture_any_section: note %u at +%d for %d, phrase %u, role %u, slot %u\n",
                           lg[i].note, (int)(lg[i].on - lg[0].on), (int)(lg[i].off - lg[i].on), lg[i].phr, lg[i].role,
                           lg[i].slot);
            CHECK(ans && !other && insec == lg_n - 5u && en_section == (pass ? EN_SEC_FLOW : EN_SEC_EMERGE),
                  "test_capture_any_section: %s: the keys' answer %d; %u other Lead notes, %u of %u in the section",
                  pass ? "before Lead in" : "in Emerge", ans, other, insec, lg_n - 5u);
        }
    }

    /* (BERLIN, the Lead's memory, spec 3.5) PANIC and a piece change clear the capture buffer, not the bank (a piece
     * change re-seeds the bank): (a) 2 keys, PANIC, 2 keys: 2 in the buffer, nothing taken; (b) the keys' answer begun,
     * PANIC: nothing left sounding, the buffer empty, the theme still the figure; (c) the keys' answer begun, a piece
     * change: nothing left sounding, the buffer empty, the figure gone from the bank; (d) 2 keys, a piece change, 2
     * keys: 2 in the buffer */
    {
        static const int FG[4] = {7, 9, 8, 11}, FD[4] = {4, 2, 2, 8};
        const uint32_t pl = EN_MODE_POOL[MODE_AEOLIAN];
        int8_t off0[32], off[32];
        uint8_t dur0[32], dur[32];
        int n0, n1, ca, cd, got, left, pass, pl0, pl1;
        fresh();
        en_set(P_ROOT, 2);
        en_pool(pl);
        lg_pool = pl, lg_root = 2;
        en_set(P_TEMPO, 120);
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_MOTION, 0);
        en_set(P_DRIFT, 6);
        en_play(1);
        run(12.0f);
        n0 = en_lead_theme(off0, dur0);
        keys_play(FG, FD, 2);                    /* (a) */
        en_panic();
        keys_now(FG + 2, FD + 2, 2);             /* (at once: without PANIC, one figure of 4) */
        ca = en_lead_cap();
        run(16.0f);
        n1 = en_lead_theme(off, dur);
        CHECK(ca == 2 && n1 == n0 && !memcmp(off, off0, (size_t)n0) && !memcmp(dur, dur0, (size_t)n0),
              "test_lead_memory_panic: 2 keys, PANIC, 2 keys: %d in the buffer (2), the theme as it was %d", ca,
              n1 == n0 && !memcmp(off, off0, (size_t)n0));
        for (pass = 0; pass < 2; pass++) {       /* (b), (c) */
            uint8_t p0;
            int b;
            memset(sounding[2], 0, sizeof sounding[2]);
            ld_on_now = 0;
            run_to(keys_play(FG, FD, 4) + 9u);   /* (the figure taken: two beats after the last let go) */
            p0 = en_lead_phr;
            for (got = 0, b = 0; b < 2000 && !got; b++) {
                run(256.0f / 44100.0f + 1e-6f);
                got = en_lead_phr != p0 && en_lead_slot == 0 && en_lead_role == LR_ANSWER;
            }
            n0 = en_lead_theme(off0, dur0);      /* (the figure, taken) */
            run(0.3f);
            pl0 = en_lead_played();
            if (pass)
                piece(4);
            else
                en_panic();
            run(256.0f / 44100.0f + 1e-6f);      /* (a block: the change taken, no draw yet) */
            pl1 = en_lead_played();
            run(0.1f - 256.0f / 44100.0f);
            cd = en_lead_cap();
            CHECK(pl0 && pl1 == !pass, "test_lead_memory_panic: %s: the theme the keys' %d before, %d after (1, %d)",
                  pass ? "a piece change" : "PANIC", pl0, pl1, !pass);
            run(20.0f);
            en_mute(EN_M_BASS);                  /* (the Lead muted: what it sounds let go; anything left is stuck) */
            run(0.05f);
            left = ld_left_on();
            en_mute(0);
            n1 = en_lead_theme(off, dur);
            CHECK(got && !left && !cd &&
                      (pass ? n1 != n0 || memcmp(off, off0, (size_t)n0) : n1 == n0 && !memcmp(off, off0, (size_t)n0)),
                  "test_lead_memory_panic: %s in the keys' answer (begun %d): %d left sounding, %d in the buffer; the "
                  "theme %d entries, was %d", pass ? "a piece change" : "PANIC", got, left, cd, n1, n0);
            if (pass) {
                en_set(P_TEMPO, 120);
                en_set(P_LDIN, 0);
                en_set(P_MOTION, 0);
            }
        }
        keys_play(FG, FD, 2);                    /* (d) */
        piece(4);
        en_set(P_TEMPO, 120);
        keys_now(FG + 2, FD + 2, 2);
        cd = en_lead_cap();
        CHECK(cd == 2, "test_lead_memory_panic: 2 keys, a piece change, 2 keys: %d in the buffer (2)", cd);
        run(4.0f);
    }

    /* (BERLIN, the Lead's memory, review 1 of Task 5) only what the Lead plays from the keys is written: D Aeolian at
     * 120, Flow, the chord held. (e) 3 keys (lengths 4 2 6), the Lead muted, a 4th key held 8 sixteenths and let go:
     * the theme the 3, the last still 6 (the unheard key neither lengthens it nor is its quiet's). (f) 3 keys, STOP,
     * PLAY, a key held 8: the buffer empty at the STOP, nothing taken (the theme as before). (g) MIDI out off and on
     * in the middle of the keys' answer: nothing left sounding */
    {
        static const int FG[4] = {2, 4, 3, 5}, FD[4] = {4, 2, 6, 8};
        const uint32_t pl = EN_MODE_POOL[MODE_AEOLIAN];
        int8_t off0[32], off[32];
        uint8_t dur0[32], dur[32];
        int nt, n0, cs, got, b, left;
        uint8_t p0;
        fresh();
        en_set(P_ROOT, 2);
        en_pool(pl);
        lg_pool = pl, lg_root = 2;
        en_set(P_TEMPO, 120);
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_MOTION, 0);
        en_set(P_DRIFT, 6);
        en_play(1);
        run(12.0f);
        keys_play(FG, FD, 3);                    /* (e) */
        en_mute(EN_M_BASS);
        keys_now(FG + 3, FD + 3, 1);
        en_mute(0);
        run(8.0f);
        nt = en_lead_theme(off, dur);
        CHECK(nt == 3 && dur[0] == 4 && dur[1] == 2 && dur[2] == 6,
              "test_capture_unwritten: 3 keys, a 4th while the Lead is muted: the theme %d entries, lengths %d %d %d "
              "(4 2 6)", nt, dur[0], dur[1], dur[2]);
        n0 = en_lead_theme(off0, dur0);          /* (f) */
        keys_play(FG, FD, 3);
        en_play(0);
        run(0.3f);
        cs = en_lead_cap();
        en_play(1);
        keys_now(FG + 3, FD + 3, 1);
        run(8.0f);
        nt = en_lead_theme(off, dur);
        CHECK(!cs && nt == n0 && !memcmp(off, off0, (size_t)n0) && !memcmp(dur, dur0, (size_t)n0),
              "test_capture_unwritten: 3 keys, STOP, PLAY, a key: %d in the buffer at the STOP (0); the theme %d "
              "entries, was %d (as it was)", cs, nt, n0);
        memset(sounding[2], 0, sizeof sounding[2]);   /* (g) */
        ld_on_now = 0;
        run_to(keys_play(FG, FD, 4) + 9u);       /* (the figure taken) */
        p0 = en_lead_phr;
        for (got = 0, b = 0; b < 2000 && !got; b++) {
            run(256.0f / 44100.0f + 1e-6f);
            got = en_lead_phr != p0 && en_lead_slot == 0 && en_lead_role == LR_ANSWER;
        }
        run(0.3f);
        en_set(P_MIDI, 0);
        run(0.4f);
        en_set(P_MIDI, 1);
        run(20.0f);
        en_mute(EN_M_BASS);
        run(0.05f);
        left = ld_left_on();
        en_mute(0);
        CHECK(got && !left,
              "test_capture_unwritten: MIDI out off and on in the keys' answer (begun %d): %d left sounding", got,
              left);
    }

    /* (BERLIN, the Lead's memory, review 1 of Task 5: the controller's ruling) a theme the player played comes back at
     * the arc's moments even under Memory 20 (spec 2's anti-goal: a theme the user played thrown away at once): D
     * Aeolian at 120, Motion 100, Lead in 0, Memory 10; 4 keys in Flow; within 6 minutes, after the keys' answer, a
     * call on the theme (slot 0) with the figure's intervals (its first return: not yet varied) */
    {
        static const int FG[4] = {7, 9, 8, 11}, FD[4] = {4, 2, 2, 8};
        uint32_t i, s, back = 0, at = 0;
        fresh();
        lg_pool = EN_MODE_POOL[MODE_AEOLIAN], lg_root = 2;
        en_set(P_ROOT, 2);
        en_pool(lg_pool);
        en_set(P_TEMPO, 120);
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_MOTION, 100);
        en_set(P_LMEM, 10);
        en_play(1);
        run(6.0f);
        bar_next();
        lg_n = 0, lg_open = -1;
        s = keys_now(FG, FD, 4);
        run(360.0f);
        for (i = lg_find(4, LR_CALL); i < lg_n && !back; i = lg_find(i + 1u, LR_CALL)) {
            int k, ok = lg[i].slot == 0 && lg[i].on > s + 64u && i + 4u <= lg_n;
            for (k = 0; k < 2 && ok; k++)   /* (the last note the ending's) */
                ok &= lg_deg(lg[i + (uint32_t)k + 1u].note) - lg_deg(lg[i + (uint32_t)k].note) == FG[k + 1] - FG[k];
            if (ok)
                back = 1, at = (lg[i].on - s) / 16u;
        }
        CHECK(back, "test_capture_memory_low: Memory 10: the keys' theme not heard again in 6 minutes");
        printf("  test_capture_memory_low: the keys' theme back %u bars after them\n", at);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
    }

    /* (BERLIN, the Lead's memory, Part 3: the user, "it basically repeats what I played last from my longer motive")
     * D Aeolian at 120, Flow, the chord held, Lead in 0, Phrases 100, Memory 50: a 12-note figure within 4 bars (40
     * sixteenths) is kept whole, up to the capture's limit (EN_LEAD_CPMAX: its last that many entries); the Lead's next
     * phrase is its answer, not the figure: the kept figure's rhythm, its notes not the figure's in half the places or
     * more but related (as test_capture's), its last note on the chord, beginning 1-2 beats after the figure closed
     * (11 .. 17 sixteenths after the last let go); then, not on Flow's roll, the line's first offspring within 8 bars
     * of the answer's end */
    {
        static const int FG[12] = {4, 5, 6, 7, 6, 5, 4, 3, 4, 6, 5, 4}, FD[12] = {4, 2, 2, 4, 4, 2, 2, 4, 4, 4, 4, 4};
        const int kept = 12 < EN_LEAD_CPMAX ? 12 : EN_LEAD_CPMAX, k0 = 12 - kept;
        int8_t off[32];
        uint8_t dur[32];
        int nt, k, ok, ans, dif, dm, pcs[4], p9, p5, nc, ct = 0, kind = -1;
        uint32_t s, bl, end, at = 0;
        uint8_t id0;
        off_setup(50);
        en_set(P_TEMPO, 120);
        run(12.0f);
        bar_next();
        lg_n = 0, lg_open = -1;
        id0 = en_lead_ideas;
        s = keys_now(FG, FD, 12);
        run_to(s + 6u * 16u);
        nt = en_lead_theme(off, dur);
        for (k = 0, ok = nt == kept; k < kept && ok; k++)
            ok &= dur[k] == FD[k0 + k] && (!k || off[k] - off[k - 1] == FG[k0 + k] - FG[k0 + k - 1]);
        ans = lg_answer(12, (uint32_t)k0, kept, &dif, &dm);
        bl = lg_n > 12u ? lg[12].on : 0;
        if (ans) {                               /* (its last note on the chord) */
            nc = lg_chord(lg[12u + (uint32_t)kept - 1u].croot, pcs, &p9, &p5);
            for (k = 0; k < nc; k++)
                ct |= lg[12u + (uint32_t)kept - 1u].note % 12 == pcs[k];
        }
        if (!ok || !ans)
            for (k = 0; k < (int)lg_n && k < 40; k++)
                printf("  test_capture_answer: note %u at +%d for %d, phrase %u, role %u, slot %u\n", lg[k].note,
                       (int)(lg[k].on - lg[0].on), (int)(lg[k].off - lg[k].on), lg[k].phr, lg[k].role, lg[k].slot);
        printf("  test_capture_answer: %d kept of 12; the answer at +%d sixteenths, %d of %d notes not the figure's, "
               "its mean %+d/%d degrees from it, its first %d degrees from the figure's last, its strong beats within "
               "%d\n", nt, (int)(bl - s), dif, kept, dm, kept, la_first, la_strong);
        CHECK(ok && ans && 2 * dif >= kept && dm <= 3 * kept && dm >= -3 * kept && la_first <= 2 && la_strong <= 2 &&
                  ct && bl >= s + 11u && bl <= s + 17u,
              "test_capture_answer: the theme the figure's last %d %d (%d entries); its answer %d (%d of %d notes not "
              "the figure's; its mean %+d/%d degrees from the figure's, within 3; its first %d degrees from the "
              "figure's last, its strong beats at most %d from the figure's, both within 2; its last on the chord %d), "
              "at +%d sixteenths after the last let go (11 .. 17)", kept, ok, nt, ans, dif, kept, dm, kept, la_first,
              la_strong, ct, (int)(bl - s));
        end = ans ? lg[12u + (uint32_t)kept - 1u].off : en_bars;
        while (en_lead_ideas == id0 && en_bars < end + 12u * 16u)
            run(256.0f / 44100.0f + 1e-6f);
        if (en_lead_ideas != id0 && lg_n > 12u + (uint32_t)kept && lg[12u + (uint32_t)kept].slot != 0)
            at = (lg[12u + (uint32_t)kept].on - end) / 16u, kind = en_lead_kind;   /* (its first note: the phrase
                                                  * after the answer) */
        printf("  test_capture_answer: the first offspring %u bars after the answer, made %d\n", at, kind);
        CHECK(en_lead_ideas != id0 && at <= 8u && kind > LK_PLAIN,
              "test_capture_answer: the first offspring %d, %u bars after the answer ends (<= 8), made %d (not 0)",
              en_lead_ideas != id0, at, kind);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
    }

    /* (BERLIN, the Lead's memory, Part 3 review 1) the keys' answer where the figure was played, whatever the chord
     * does between: D Aeolian at 120, Flow, Drift 2 bars, Lead in 0; a 4-note figure (20 sixteenths) from a bar line
     * where the chord changed, so it closes a beat before the next change and the answer begins on it (the root
     * moved): the answer's mean within 3 degrees of the figure's, and related to it as test_capture's (its frame the
     * figure's root, not the chord's now) */
    {
        static const int FG[4] = {4, 6, 5, 3}, FD[4] = {4, 4, 4, 8};
        uint32_t i, x = 0, s;
        int ans, dif, dm, sa = 0, sf = 0, moved, k, r0;
        off_setup(50);
        en_set(P_TEMPO, 120);
        en_set(P_DRIFT, 0);                      /* (2 bars) */
        run(8.0f);
        for (r0 = en_chord_root; !x && en_bars < 64u * 16u;) {   /* (a bar line the chord changed on) */
            run(256.0f / 44100.0f + 1e-6f);
            if (en_chord_root != r0)
                x = en_bars / 16u * 16u;
        }
        run_to(x + 32u);                         /* (the next change's bar line: the figure on it) */
        lg_n = 0, lg_open = -1;
        s = keys_now(FG, FD, 4);
        run_to(s + 8u * 16u);
        ans = lg_answer(4, 0, 4, &dif, &dm);
        for (k = 0; k < 4 && ans; k++)
            sa += lg_deg(lg[4u + (uint32_t)k].note), sf += lg_deg(lg[(uint32_t)k].note);
        moved = ans && lg[4].croot != lg[3].croot;
        for (i = 0; i < lg_n && i < 8u && !ans; i++)
            printf("  test_capture_chord: note %u at +%d, root %u, role %u slot %u\n", lg[i].note,
                   (int)(lg[i].on - lg[0].on), lg[i].croot, lg[i].role, lg[i].slot);
        printf("  test_capture_chord: the answer's mean %+d/4 degrees from the figure's, the root %u then %u\n",
               sa - sf, ans ? lg[3].croot : 0, ans ? lg[4].croot : 0);
        CHECK(ans && moved && sa - sf <= 12 && sf - sa <= 12 && la_first <= 2 && la_strong <= 2,
              "test_capture_chord: the keys' answer %d, over a new root %d, its mean %+d/4 degrees from the figure's "
              "(within 3); its first %d degrees from the figure's last, its strong beats at most %d from the figure's "
              "(both within 2)", ans, moved, sa - sf, la_first, la_strong);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
    }

    /* (BERLIN, the Lead's memory, Part 3 review 1: the controller's ruling) a played theme is varied note by note, but
     * never its first half twice (that would put the player's first half in place of their second for good): a 12-note
     * figure taken, then 300 variations of it (en_lead_vary): its rhythm the figure's every time */
    {
        static const int FG[12] = {4, 5, 6, 7, 6, 5, 4, 3, 4, 6, 5, 4}, FD[12] = {4, 2, 2, 4, 4, 2, 2, 4, 4, 4, 4, 4};
        const int kept = 12 < EN_LEAD_CPMAX ? 12 : EN_LEAD_CPMAX, k0 = 12 - kept;
        int8_t off[32];
        uint8_t dur[32];
        int v, k, nt, bad = 0;
        off_setup(50);
        run(6.0f);
        keys_play(FG, FD, 12);
        run(2.0f);
        for (v = 0; v < 300; v++) {
            en_lead_vary(0);
            nt = en_lead_theme(off, dur);
            for (k = 0, bad += nt != kept; k < kept && nt == kept; k++)
                bad += dur[k] != FD[k0 + k];
        }
        CHECK(en_lead_played() && !bad, "test_capture_vary: 300 variations of a played theme: %d lengths not the "
              "figure's (0), the keys' %d", bad, en_lead_played());
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
    }

    /* (BERLIN, the Lead's memory, Part 3) the keys' theme comes back whole later: D Aeolian at 120, Motion 100, Lead in
     * 0, Memory 10 (a block a pair; a played theme back at any Memory); the 12-note figure in Flow; within 4 minutes a
     * call on the theme (slot 0) with all the kept figure's notes, its rhythm and its intervals (but the ending's); or
     * its values doubled, a calm pass's way (at CP_MAX 8 the kept 28 sixteenths may be: every note then held, so put
     * on the chord), with the figure's contour (each interval's way: up, down, the same) */
    {
        static const int FG[12] = {4, 5, 6, 7, 6, 5, 4, 3, 4, 6, 5, 4}, FD[12] = {4, 2, 2, 4, 4, 2, 2, 4, 4, 4, 4, 4};
        const int kept = 12 < EN_LEAD_CPMAX ? 12 : EN_LEAD_CPMAX;
        const uint32_t k0 = 12u - (uint32_t)kept;
        uint32_t i, s, back = 0, at = 0;
        int sec;
        fresh();
        lg_pool = EN_MODE_POOL[MODE_AEOLIAN], lg_root = 2;
        en_set(P_ROOT, 2);
        en_pool(lg_pool);
        en_set(P_TEMPO, 120);
        en_set(P_LEAD, 60);
        en_set(P_LDIN, 0);
        en_set(P_MOTION, 100);
        en_set(P_LMEM, 10);
        en_play(1);
        run(6.0f);
        bar_next();
        lg_n = 0, lg_open = -1;
        s = keys_now(FG, FD, 12);
        for (sec = 0; sec < 240 && !back; sec += 5) {
            run(5.0f);
            for (i = lg_find(12, LR_CALL); i < lg_n && !back; i = lg_find(i + 1u, LR_CALL)) {
                int k, ok = lg[i].slot == 0 && lg[i].on > s + 64u && i + (uint32_t)kept <= lg_n &&
                            lg_last(i + (uint32_t)kept - 1u) && lg[i + (uint32_t)kept - 1u].off != 0xFFFFFFFFu;
                const uint32_t x = ok && lg[i + 1u].on - lg[i].on == 2u * (uint32_t)FD[k0] ? 2u : 1u;
                for (k = 0; k + 1 < kept && ok; k++) {
                    ok &= lg[i + (uint32_t)k + 1u].on - lg[i + (uint32_t)k].on == x * (uint32_t)FD[k0 + (uint32_t)k];
                    if (k + 2 < kept) {          /* (the last note the ending's) */
                        const int d = lg_deg(lg[i + (uint32_t)k + 1u].note) - lg_deg(lg[i + (uint32_t)k].note),
                                  f = FG[k0 + (uint32_t)k + 1u] - FG[k0 + (uint32_t)k];
                        ok &= x == 1u ? d == f : (d > 0) == (f > 0) && (d < 0) == (f < 0);
                    }
                }
                if (ok)
                    back = 1, at = (lg[i].on - s) / 16u;
            }
        }
        if (!back)                               /* (what came back on the theme instead) */
            for (i = lg_find(12, LR_CALL); i < lg_n; i = lg_find(i + 1u, LR_CALL))
                if (lg[i].slot == 0) {
                    uint32_t j;
                    printf("  test_capture_return: a call on the theme at +%u:", (lg[i].on - s) / 16u);
                    for (j = i; j < lg_n && (j == i || !lg_first(j)); j++)
                        printf(" %d/%u", lg_deg(lg[j].note), lg[j].off - lg[j].on);
                    printf("\n");
                }
        printf("  test_capture_return: the keys' theme back whole %u bars after them\n", at);
        CHECK(back, "test_capture_return: the 12-note figure's theme (%d kept) not heard whole again in 4 minutes",
              kept);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
    }

    /* (BERLIN, the Lead's memory: the device's audio block near its limit) no tick of the grid draws more than one
     * motif: Stratosfear (no intro) 90 s, Motion 100, Lead in 0, Memory 85; a piece change in the middle of a call to
     * Sunrain, 90 s; 4 white keys captured there, 60 s (an offspring of them drawn); Cyborg 60 s, its Lead
     * made to forget half way (FORGET). (The engine counts its draws a tick, the most) */
    {
        static const int FG[4] = {7, 9, 8, 11}, FD[4] = {4, 2, 2, 8};
        fresh();
        en_lead_dmax = 0;
        piece(4);
        en_set(P_MOTION, 100);
        en_set(P_LDIN, 0);
        en_play(1);
        run(90.0f);
        lg_n = 0, lg_open = -1;
        lg_wait(LR_CALL, 120.0f);
        run(0.2f);
        piece(5);
        en_set(P_MOTION, 100);
        en_set(P_LDIN, 0);
        run(90.0f);
        keys_play(FG, FD, 4);
        run(4.0f);
        {                                        /* (the capture taken: its lengths, its intervals) */
            int8_t off[32];
            uint8_t dur[32];
            int nt = en_lead_theme(off, dur), k, ok = nt == 4;
            for (k = 0; k < 4 && ok; k++)
                ok &= dur[k] == FD[k] && (!k || off[k] - off[k - 1] == FG[k] - FG[k - 1]);
            CHECK(ok, "test_lead_one_draw: the keys' figure not taken in Sunrain (the theme %d entries)", nt);
        }
        {                                        /* (the offspring drawn: the line's ideas from the keys' theme) */
            static const int8_t TO[4] = {7, 9, 8, 11};
            static const uint8_t TD[4] = {4, 2, 2, 8};
            uint32_t at[32], k0;
            int kv[32], kd[32], km, c = ideas_log(56.0f, TO, TD, 4, at, kv, kd, 32, &k0, &km), k, offs = 0;
            for (k = 0; k < c; k++)
                offs += kd[k] != LK_PLAIN;
            CHECK(offs >= 1, "test_lead_one_draw: no offspring of the keys' theme in Sunrain (%d ideas)", c);
        }
        piece(7);
        en_set(P_LDIN, 0);
        run(30.0f);
        en_lead_forget();                        /* (the Lead's FORGET: new ideas, its theme's draws again) */
        run(30.0f);
        CHECK(en_lead_dmax == 1, "test_lead_one_draw: the most motifs drawn in one tick %u (1)", en_lead_dmax);
    }

    /* (BERLIN, the Lead's offspring: the user, "at some point the melody should diverge and start going its own way")
     * off_setup at Memory 0 (a fresh idea every block). A figure on the keys (5 notes, a dotted rhythm no template has):
     * the line's next fresh ideas grow out of it and stray further. Kinship (kin_of: its rhythm, its intervals as they
     * are, mirrored or backwards) of the first 3 ideas taken while the kinship lasts (the figure played twice, a
     * minute apart: in Flow at Memory 0 the Lead takes one or two in its 45 s) well over that of the same run's ideas
     * without the keys; those taken 16 bars or more after it's ended (45 s at Memory 0) the generator's own (made
     * plain) and sharing no more than those (+ 10: a few ideas each). Without the keys no kinship, ever (the rest: the
     * goldens). A new figure: kinship again, the next idea its offspring, close kin (made from its head or its
     * contour: kin_of scores a 2-note head low, so the kind is asked) */
    {
        static const int FG[5] = {7, 11, 10, 8, 9}, FD[5] = {3, 3, 2, 6, 10}, F2[5] = {9, 8, 6, 7, 4},
                         D2[5] = {6, 2, 2, 4, 12};
        int8_t to[5], t2[5];
        uint8_t td[5], d2[5];
        uint32_t at[64], k0, kb0;
        int kv[64], kd[64], nb, na, k, kmax, kmb, kmin = 100, rep, ce = 0, cl = 0, early_plain = 0, late_off = 0,
            base_off = 0;
        long sb = 0, se = 0, sl = 0;
        for (k = 0; k < 5; k++)
            to[k] = (int8_t)FG[k], td[k] = (uint8_t)FD[k], t2[k] = (int8_t)F2[k], d2[k] = (uint8_t)D2[k];
        off_setup(0);                            /* (without the keys: the baseline) */
        run(12.0f);
        bar_next();
        nb = ideas_log(130.0f, to, td, 5, at, kv, kd, 64, &kb0, &kmb);
        for (k = 0; k < nb; k++)
            sb += kv[k], base_off += kd[k] != LK_PLAIN;
        CHECK(nb >= 6 && kmb == 0 && !base_off,
              "test_offspring_none_without_keys: %d ideas, the most kinship %d, %d not the generator's own (0, 0)", nb,
              kmb, base_off);
        off_setup(0);                            /* (with them: the figure twice, a minute apart, then 130 s) */
        run(12.0f);
        for (rep = 0; rep < 2; rep++) {
            keys_play(FG, FD, 5);
            na = ideas_log(rep ? 130.0f : 60.0f, to, td, 5, at, kv, kd, 64, &k0, &kmax);
            for (k = 0; k < na; k++) {
                printf("  test_offspring_kin: figure %d, idea %d at bar %u: kinship %d, made %d%s\n", rep + 1, k,
                       at[k] / 16u, kv[k], kd[k], k0 != 0xFFFFFFFFu && at[k] >= k0 ? " (the kinship ended)" : "");
                if (at[k] < k0 && ce < 3)
                    se += kv[k], ce++, early_plain += kd[k] == LK_PLAIN;
                else if (rep && k0 != 0xFFFFFFFFu && at[k] >= k0 + 16u * 16u)
                    sl += kv[k], cl++, late_off += kd[k] != LK_PLAIN;
            }
            kmin = kmax < kmin ? kmax : kmin;
        }
        printf("  test_offspring_kin: kinship with the figure: the first %d ideas %ld, the last %d %ld, without the keys "
               "%ld (%d ideas)\n", ce, ce ? se / ce : 0, cl, cl ? sl / cl : 0, nb ? sb / nb : 0, nb);
        CHECK(kmin >= 95 && ce >= 2 && !early_plain && nb && se * nb >= (sb + 25L * nb) * ce,
              "test_offspring_kin: the kinship at most %d (>= 95); the first %d ideas (>= 2, %d plain) %ld, without the "
              "keys %ld (+ 25 wanted)", kmin, ce, early_plain, ce ? se / ce : 0, nb ? sb / nb : 0);
        CHECK(k0 != 0xFFFFFFFFu && cl >= 2 && !late_off && sl * nb <= (sb + 10L * nb) * cl,
              "test_offspring_kin: after the kinship's end, %d ideas (>= 2, %d offspring) %ld, without the keys %ld (+ 10 "
              "at most)", cl, late_off, cl ? sl / cl : 0, nb ? sb / nb : 0);
        keys_play(F2, D2, 5);                    /* (a new figure: the kinship again) */
        na = ideas_log(40.0f, t2, d2, 5, at, kv, kd, 64, &k0, &kmax);
        printf("  test_offspring_kin: a new figure: the next idea's kinship with it %d, made %d\n", na ? kv[0] : -1,
               na ? kd[0] : -1);
        CHECK(kmax >= 95 && na >= 1 && (kd[0] == LK_HEAD || kd[0] == LK_SEQ || kd[0] == LK_MIRROR),
              "test_offspring_kin: a new figure: the kinship at most %d (>= 95); its next idea (%d) made %d (close "
              "kin: its head continued 1, a sequence 2, mirrored 3)", kmax, na, na ? kd[0] : -1);
    }

    /* (BERLIN, the Lead's offspring: its pace, the controller's ruling) off_setup at Memory 0, 50 and 100, the same
     * figure: the kinship falls with the bars after it's taken, ending 45 s after it at Memory 0 (30 bars at 160), 90 s
     * at 50 (60), 150 s at 100 (100): 40 bars on, 0, 34, 60 (+-3) */
    {
        static const int FG[5] = {7, 11, 10, 8, 9}, FD[5] = {3, 3, 2, 6, 10}, MEM[3] = {0, 50, 100}, WANT[3] = {0, 34, 60};
        int k, kn[3], ok = 1;
        uint32_t s, end = 0;
        for (k = 0; k < 3; k++) {
            off_setup(MEM[k]);
            run(12.0f);
            s = keys_play(FG, FD, 5) + 8u;       /* (taken two beats after the last let go) */
            run_to(s + 1u);
            if (!k) {                            /* (Memory 0: the bar it ends) */
                while (en_lead_kin() && en_bars < s + 50u * 16u)
                    run(256.0f / 44100.0f + 1e-6f);
                end = (en_bars - s) / 16u;
            }
            run_to(s + 40u * 16u + 1u);
            kn[k] = en_lead_kin();
            ok &= kn[k] >= WANT[k] - 3 && kn[k] <= WANT[k] + 3;
        }
        printf("  test_offspring_pace: 40 bars on, the kinship %d %d %d at Memory 0 50 100; ended at Memory 0 %u bars "
               "on\n", kn[0], kn[1], kn[2], end);
        CHECK(ok && kn[0] < kn[1] && kn[1] < kn[2] && end >= 29u && end <= 31u,
              "test_offspring_pace: 40 bars on, the kinship %d %d %d (0 34 60), at Memory 0 ended %u bars on (30)", kn[0],
              kn[1], kn[2], end);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
    }

    /* (BERLIN, the Lead's offspring, review 1: at high Memory the line keeps its idea, so the offspring stray further
     * only if the kinship's move is a reason for a fresh one) off_setup at Memory 100, a figure: over its pace (150 s)
     * and 30 s on, 3 fresh ideas or more, each made a kind further from it (close, further, its rhythm, the Lead's
     * own),
     * the last the generator's own; then 60 s more and no fresh idea (Memory 100 keeps it) */
    {
        static const int FG[5] = {7, 11, 10, 8, 9}, FD[5] = {3, 3, 2, 6, 10};
        static const int BAND[7] = {0, 3, 3, 3, 2, 2, 1};   /* (LK_* by how close: as ld_offspring's bands) */
        int8_t to[5];
        uint8_t td[5];
        uint32_t at[64], k0;
        int kv[64], kd[64], c, k, km, down = 1;
        uint8_t id0;
        for (k = 0; k < 5; k++)
            to[k] = (int8_t)FG[k], td[k] = (uint8_t)FD[k];
        off_setup(100);
        run(12.0f);
        keys_play(FG, FD, 5);
        c = ideas_log(180.0f, to, td, 5, at, kv, kd, 64, &k0, &km);
        for (k = 0; k < c; k++) {
            printf("  test_offspring_memory: Memory 100, idea %d at bar %u: kinship %d, made %d\n", k, at[k] / 16u,
                   kv[k], kd[k]);
            down &= !k || BAND[kd[k]] < BAND[kd[k - 1]];
        }
        id0 = en_lead_ideas;
        run(60.0f);
        CHECK(c >= 3 && down && kd[c - 1] == LK_PLAIN && en_lead_ideas == id0,
              "test_offspring_memory: Memory 100: %d fresh ideas in the pace and 30 s (>= 3), each further %d, the "
              "last "
              "made %d (0: the Lead's own); %d more in the 60 s after (0)", c, down, c ? kd[c - 1] : -1,
              (int)(uint8_t)(en_lead_ideas - id0));
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
    }

    /* (BERLIN, the Lead's FORGET: the user's design, PERFORM's top key) en_lead_forget(): the Lead forgets everything it
     * remembers. off_setup at Memory 50, a figure taken (its offspring due), forgotten in the middle of its call: the
     * note let go at once, the theme gone (no entries, not the keys'), no kinship, the keys' buffer empty; then new
     * ideas as at a piece's start: a theme thought of within a bar or two (not the figure), the Lead's next phrase on
     * it (slot 0) within 6 bars (its 4 draws, a bar each, then the theme due at once: review 1), three times over;
     * the line's first idea the generator's own. 2 keys of a figure, forgotten, then 2 more: 2 in the buffer,
     * nothing taken. At most one draw a tick throughout */
    {
        static const int FG[5] = {7, 11, 10, 8, 9}, FD[5] = {3, 3, 2, 6, 10};
        int8_t off[32];
        uint8_t dur[32];
        int got = 0, b, nt, n0, pl, kn, cp, left, same, call, idea, kind = -1, at = -1, back, k, worst = 0;
        uint8_t p0, id0;
        uint32_t f, s;
        off_setup(50);
        en_lead_dmax = 0;
        run(12.0f);
        memset(sounding[2], 0, sizeof sounding[2]);
        ld_on_now = 0;
        s = keys_play(FG, FD, 5) + 8u;          /* (taken two beats after the last let go) */
        run_to(s + 1u);
        p0 = en_lead_phr;
        for (b = 0; b < 2000 && !got; b++) {     /* (a phrase on it begun, whatever its role) */
            run(256.0f / 44100.0f + 1e-6f);
            got = en_lead_phr != p0 && en_lead_slot == 0 && en_lead_played();
        }
        run(0.2f);
        en_lead_forget();
        run(256.0f / 44100.0f + 1e-6f);          /* (a block: taken) */
        f = en_bars;
        left = ld_left_on();
        nt = en_lead_theme(off, dur), pl = en_lead_played(), kn = en_lead_kin(), cp = en_lead_cap();
        CHECK(got && !left && !nt && !pl && !kn && !cp,
              "test_lead_forget: forgotten in a phrase on the keys' theme (begun %d): %d left sounding; the theme %d "
              "entries, the "
              "keys' %d, kinship %d, %d in the buffer (all 0)", got, left, nt, pl, kn, cp);
        while (at < 0 && en_bars < f + 8u * 16u) {   /* (the theme thought of again: its first draw, at a bar line) */
            run(256.0f / 44100.0f + 1e-6f);
            if (en_lead_theme(off, dur) > 0)
                at = (int)((en_bars - f) / 16u);
        }
        id0 = en_lead_ideas;
        for (p0 = en_lead_phr, back = -1; back < 0 && en_bars < f + 24u * 16u;) {   /* (its next phrase, any role) */
            run(256.0f / 44100.0f + 1e-6f);
            if (en_lead_phr != p0)
                back = (int)((en_bars - f) / 16u);
        }
        call = back >= 0 && en_lead_slot == 0;
        nt = en_lead_theme(off, dur);
        for (b = 0, same = nt == 5; b < 5 && same; b++)
            same = dur[b] == FD[b];
        for (b = 0, idea = 0; b < (int)(60.0f * 44100.0f / 256.0f) && !idea; b++) {
            run(256.0f / 44100.0f + 1e-6f);
            if (en_lead_ideas != id0)
                idea = 1, kind = en_lead_kind;
        }
        printf("  test_lead_forget: a theme thought of %d bars after, %d entries; a phrase on it %d bars after; the "
               "next "
               "idea made %d\n", at, nt, back, kind);
        CHECK(at >= 0 && at <= 2 && !same && call && back <= 6 && idea && kind == LK_PLAIN && !en_lead_played(),
              "test_lead_forget: after it: a theme thought of %d bars on (<= 2), the figure %d (0); a phrase on it "
              "%d, %d bars on (<= 6); the next idea %d, made %d (plain: 0); the keys' theme %d (0)", at, same, call,
              back, idea, kind, en_lead_played());
        keys_play(FG, FD, 2);                    /* (a figure in the middle of being played) */
        en_lead_forget();
        run(256.0f / 44100.0f + 1e-6f);
        cp = en_lead_cap();
        keys_now(FG + 2, FD + 2, 2);
        n0 = en_lead_cap();
        run(4.0f);
        CHECK(!cp && n0 == 2 && !en_lead_played(),
              "test_lead_forget: 2 keys, FORGET, 2 keys: %d in the buffer after it (0), %d after them (2); the keys' "
              "theme %d (0: nothing taken)", cp, n0, en_lead_played());
        for (k = 0; k < 3; k++) {                /* (again, three times: its next phrase within 6 bars each time) */
            run(7.0f);
            en_lead_forget();
            run(256.0f / 44100.0f + 1e-6f);
            for (f = en_bars, p0 = en_lead_phr, back = -1; back < 0 && en_bars < f + 24u * 16u;) {
                run(256.0f / 44100.0f + 1e-6f);
                if (en_lead_phr != p0)
                    back = (int)((en_bars - f) / 16u);
            }
            worst = back < 0 || back > worst ? (back < 0 ? 99 : back) : worst;
        }
        CHECK(worst <= 6, "test_lead_forget: forgotten three times more: its next phrase at most %d bars after (<= 6)",
              worst);
        CHECK(en_lead_dmax == 1, "test_lead_forget: the most motifs drawn in one tick %u (1)", en_lead_dmax);
        en_set(P_LMEM, en_param_info(P_LMEM)->def);
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

    test_atmospheres();
    test_fm_voice();
    test_fm_seq();
    test_fm_drone_none();
    test_fm_lead();
    test_shake_range();
    test_fm_lead_return();

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

    /* (the final review, 2026-10-28) a MIDI Start while playing: the Lead's note let go at once (it used to sound on
     * through the whole intro, the Lead silent in Emerge: 105 s in Timewind); and the scale changed in the middle of
     * a phrase: the rest of it isn't played in the old scale */
    {
        const uint32_t P0 = EN_MODE_POOL[0], P1 = EN_MODE_POOL[2];   /* (D Ionian, D Phrygian: four degrees apart) */
        int b, k, sw = 0, held;
        fresh();
        en_set(P_ROOT, 2);
        en_set(P_ARC, 0);
        en_set(P_LDIN, 0);
        en_set(P_LDENS, 100);
        en_set(P_MOTION, 70);
        en_pool(P0);
        ld_pool = P0;
        en_play(1);
        for (k = 0; k < 6; k++) {                /* (six calls, each caught at its first note, the scale turned) */
            for (b = 0; b < 40000 && !(ld_on_now && en_lead_role == LR_CALL && en_section != EN_SEC_EMERGE); b++)
                run(256.0f / 44100.0f + 1e-6f);
            if (!ld_on_now)
                break;
            ld_pool = (k & 1) ? P0 : P1;
            en_pool(ld_pool);
            run(256.0f / 44100.0f + 1e-6f);
            ld_outscale = 0;                     /* (the turn's own block: the note sounding is the old scale's) */
            run(4.0f);
            sw += (int)ld_outscale;
            for (b = 0; b < 4000 && en_lead_role == LR_CALL; b++)
                run(256.0f / 44100.0f + 1e-6f);
        }
        CHECK(k == 6 && !sw, "test_lead_scale_turn: %d calls caught, %d notes after the turn in the old scale", k, sw);
        for (b = 0; b < 40000 && !(ld_on_now && en_section != EN_SEC_EMERGE); b++)
            run(256.0f / 44100.0f + 1e-6f);
        held = (int)ld_on_now;
        en_clock(EN_CLK_START);                  /* (a DAW's Start, the FM-1 already playing) */
        run(2.0f * 256.0f / 44100.0f + 1e-6f);
        CHECK(held && !ld_on_now && !ld_left_on(), "test_lead_start: a Lead note %d before Start, %d after",
              held, (int)ld_on_now);
        en_clock(EN_CLK_STOP);
        run(1.0f);                               /* (the external clock let go: 100 blocks without its ticks) */
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
        int k, atm_bad = 0, tw_atm;
        float atm_worst = 0.0f;
        int16_t tw[P_NPARAMS];
        uint16_t twp;
        en_piece(0, tw, &twp);
        tw_atm = tw[P_CRACKLE];
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
            int q;
            for (q = 0; q < 4; q++) {             /* (Energy held at 0 by hand: its own motion, Motion's, would push
                                                  * Repeat B in, which this test keeps off) */
                en_macro(EN_MAC_ENERGY, 0);
                run(0.25f);
            }
            if (k >= 5) {                         /* (once its gate has opened: Timewind's own Atmosphere level) */
                const float want = (float)tw_atm * 0.0035f, dif = fabsf(en_atm_gain - want);
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
     * again: no wander, no Bridge; Energy held at 0 by hand: its own motion, Motion's, would move Sweep) */
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
            en_macro(EN_MAC_ENERGY, 0);
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
        /* (the user, 2026-10-28: the FM-1's speaker and headphones bright, the noise far too loud) the Atmosphere's
         * level by its type: Surf 5, Wind 7, Tape and Space 10; none: the default kept */
        for (a = 0; a < EN_NPIECES; a++) {
            static const int LV[EN_NNCHAR] = {25, 7, 5, 10, -1, 10, -1};   /* (None Wind Surf Space Air Tape Rain; Air, Rain:
                                                                           * unused) */
            const int c = pa[a][P_NCHAR];
            CHECK(c >= 0 && c < EN_NNCHAR && LV[c] >= 0 && pa[a][P_CRACKLE] == LV[c],
                  "test_pieces_atmos_levels: %s: %s at %d", en_piece_name(a), c >= 0 && c < EN_NNCHAR ? EN_NCHAR_NAME[c] : "?",
                  pa[a][P_CRACKLE]);
        }
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
        CHECK(pa[1][P_SEQ] == 30 && pa[1][P_MI1] == 16 && pa[1][P_SEQ2] == 50 && pa[1][P_SEQ2OCT] == -1 &&
                  pa[1][P_LVOICE] == 3 && pa[1][P_LEAD] == 54 && pa[1][P_CVOICE] == 1 && pa[1][P_CHORD] == 95 &&
                  pa[1][P_CRES] == -5,
              "test_pieces_wahnfried: Seq %d intro %d, Seq 2 %d oct %d, Lead voice %d level %d, Drone voice %d level %d "
              "reso %d (the user's: 30 16, 50 -1, Theremin 54, Strings 95 -5)", pa[1][P_SEQ], pa[1][P_MI1],
              pa[1][P_SEQ2], pa[1][P_SEQ2OCT], pa[1][P_LVOICE], pa[1][P_LEAD], pa[1][P_CVOICE], pa[1][P_CHORD],
              pa[1][P_CRES]);
        CHECK(pa[3][P_SVOICE] == 0 && pa[3][P_LVOICE] == 4, "test_pieces_voices: Rubycon Sequence %d (2600: 0), Lead %d "
              "(Synthi: 4) (the user's ear)", pa[3][P_SVOICE], pa[3][P_LVOICE]);
        CHECK(pa[7][P_SEQ] == 20 && pa[7][P_SEQ2] == 0 && pa[7][P_LEAD] == 70,
              "test_pieces_cyborg: Sequence %d, Sequence 2 %d, Lead %d (the user's: 20, 0, 70)", pa[7][P_SEQ],
              pa[7][P_SEQ2], pa[7][P_LEAD]);
        CHECK(pa[2][P_CVOICE] == 5 && pa[2][P_CHORD] == 70,
              "test_pieces_phaedra: Drone voice %d, level %d (the user's: Choir 5, 70)", pa[2][P_CVOICE], pa[2][P_CHORD]);
        /* (the Lead's memory, spec 3.3) each piece's Memory (the Arc page's fourth knob): the report's starting
         * values, to tune by ear */
        for (a = 0; a < EN_NPIECES; a++) {
            static const int LMEM[EN_NPIECES] = {55, 40, 75, 45, 85, 60, 50, 35};
            CHECK(pa[a][P_LMEM] == LMEM[a], "test_pieces_memory: %s Memory %d (spec 3.3: %d)", en_piece_name(a),
                  pa[a][P_LMEM], LMEM[a]);
        }
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
        {                                        /* (the Lead's memory) changed in the middle of a Lead's call: its note
                                                  * let go, no answer in the new piece's Emerge */
            uint32_t i, emerge = 0, k, left = 0;
            int got;
            piece(4);
            en_set(P_LDIN, 0);
            memset(sounding[2], 0, sizeof sounding[2]);
            ld_on_now = 0;
            run(1.0f);
            lg_n = 0, lg_open = -1;
            got = lg_wait(LR_CALL, 240.0f);
            run(0.1f);
            piece(0);
            lg_n = 0, lg_open = -1;
            run(20.0f);
            for (i = 0; i < lg_n; i++)
                emerge += lg[i].sec == EN_SEC_EMERGE;
            for (k = 0; k < 128u; k++)
                left += sounding[2][k] > 0;
            CHECK(got && en_section == EN_SEC_EMERGE && !emerge && !ld_on_now && !left,
                  "test_piece_change_arc: a call %d, then %u Lead notes in the new piece's Emerge, %u left sounding", got,
                  emerge, left);
        }
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

    /* (BERLIN) Wahnfried (the user, 2026-10-28: its Sequence at 30, Sequence 2 at 50) and Cyborg (its Sequence at 20)
     * play their sequences now: ten minutes, Wahnfried both, Cyborg its Sequence and Sequence 2 silent */
    {
        int k;
        for (k = 1; k < 8; k += 6) {
            fresh();
            piece(k);
            en_play(1);
            run(600.0f);
            if (k == 7)
                CHECK(sq_n > 0 && s2_n == 0, "test_cyborg_sequence: %u Sequence notes, %u Sequence 2", sq_n, s2_n);
            else
                CHECK(sq_n > 0 && s2_n > 0, "test_wahnfried_sequences: %u Sequence notes, %u Sequence 2", sq_n, s2_n);
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
