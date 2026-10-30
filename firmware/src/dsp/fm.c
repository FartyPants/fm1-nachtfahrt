/* (the voices batch, item 5) The FM voice: fm1_open's sixteen demo recipes, played by a small FM voice of six sines.
 * Included into amb.c (a unity build) after the oscillators: it uses sine() and fastmath.h.
 *
 * Every recipe but Soft Organ is DX7 algorithm 5: three independent pairs, a modulator into a carrier (the body, the
 * colour, the transient), the first pair's modulator fed back on itself where the recipe says so. Soft Organ is
 * algorithm 32: six carriers at 1, 2, 3, 4, 6 and 8 times the note. So no six-operator engine is needed.
 *
 * - The recipes (FM_REC) are fm1_open's app/presets.c `voices[]` as they stand, and each operator's patch values are
 *   what its demo_voice() makes of them (fm_param, below): rates and levels, the output level, the ratio, the fine
 *   ratio, the detune, the key level scaling (a break point at 50, high notes darker: 8 on the carriers, 18 on the
 *   modulators) and the rate scaling (1). Velocity is left out: the layer's own loudness carries the accent (one
 *   knob, one job), so every operator takes what a velocity of 100 gives it (+16, the same for all). The LFO's slight
 *   vibrato on Warm Air, FM Strings and Wood Reed (pitch depth 2, sensitivity 2: a cent or so) is left out too.
 * - The curves are MSFA's (Google's DX7 model, as fm1_open has it in web/synth/dx7env.c and dx7note.c): an
 *   operator's envelope moves in doublings of amplitude (its level l, 2^(l - 14) the gain), each stage at the rate
 *   its qrate gives (falling in a straight line, rising toward 17 in steps), its target from the stage's level and
 *   the operator's output level; a modulator's output (+-1 at gain 1) moves its carrier's phase by as many turns;
 *   the feedback, the operator's last two outputs averaged and halved 7 times. Here in floating point, stepped at
 *   control rate (fm_ctrl: n samples at a time), the gains ramped across each step.
 * - Loudness over a note is the layer's (its envelope: the Sequences' Decay, the Lead's Attack and release, the
 *   Drone's banks), as for every other voice there; the recipe makes the tone. A carrier that would decay to silence
 *   (a struck recipe: sustain 0) holds its body level instead, so the layer's envelope ends the note. Each note is
 *   scaled so its carriers' sum at full level is 1.
 *
 * The recipes: from M-VAVE's fm1_open (app/presets.c, Copyright (c) 2026 珠海笙科智能科技有限公司 (M-VAVE)),
 * GPL-3.0-only (taken without fm1_open's linking exception). The envelope's arithmetic follows MSFA (Apache-2.0:
 * Copyright 2012, 2013 Google Inc.; Copyright 2016-2025 Pascal Gauthier; modified for FM1 by M-VAVE, 2026), its
 * tables (FM_LVL, FM_ATTACK_MIN, the qrate steps) used here under that licence, which GPL-3.0 accepts */

enum { FM_NEON_KEYS, FM_ORBIT_BELL, FM_SOFT_ORGAN, FM_ROUND_BASS, FM_WARM_AIR, FM_WOOD_REED, FM_SOFT_EP, FM_DRY_CLAV,
       FM_GUITAR, FM_MARIMBA, FM_VIBES, FM_SOFT_BRASS, FM_STRINGS, FM_SUB_BASS, FM_METAL_HIT, FM_SOFT_LEAD, FM_NREC };
typedef char fm_nrec_ok[FM_NREC == FM_NREC_HOST ? 1 : -1];   /* (amb.h's count for the tests) */
/* their names as the screen shows them (en_param_text: 9 characters at most, so a ten-letter name loses its space;
 * the screen reader says it whole: firmware/words.tsv) */
static const char *const FM_NAME[FM_NREC] = {"Neon Keys", "OrbitBell", "SoftOrgan", "RoundBass", "Warm Air",
    "Wood Reed", "Soft EP", "Dry Clav", "FM Guitar", "Marimba", "Vibes", "SoftBrass", "FMStrings", "Sub Bass",
    "Metal Hit", "Soft Lead"};

/* presets.c's recipe fields: a pair's carrier level, its ratio (coarse), detune (7: none), the modulator's level,
 * ratio and fine ratio, its decay rate and body level; a voice's carrier rates and levels, its modulators' attack and
 * release, feedback */
typedef struct { uint8_t level, coarse, detune, mod, ratio, fine, decay, body; } fm_pair_t;
typedef struct {
    uint8_t attack, decay, settle, release, body, sustain, mod_attack, mod_release, feedback;
    fm_pair_t pair[3];
} fm_rec_t;
static const fm_rec_t FM_REC[FM_NREC] = {
    {96,52,30,62,84,0,99,66,0, {{98,1,7,54,1,0,65,38},{91,1,8,60,1,0,57,58},{79,1,6,48,7,0,80,0}}},
    {99,50,28,53,70,0,99,62,0, {{96,1,7,58,3,50,59,0},{82,2,7,48,7,0,68,0},{69,3,7,35,11,0,80,0}}},
    {95,65,45,78,99,99,95,78,0, {{90,1,7,0,1,0,65,0},{87,2,7,0,1,0,65,0},{74,3,7,0,1,0,65,0}}},
    {97,72,46,82,91,84,99,86,0, {{99,1,7,42,1,0,76,32},{74,2,7,32,1,0,82,20},{0,1,7,0,1,0,80,0}}},
    {53,47,36,52,96,92,49,55,0, {{90,1,5,42,1,0,55,82},{84,1,9,34,2,0,53,76},{73,2,7,27,1,0,52,78}}},
    {84,61,46,72,94,89,78,77,1, {{95,1,7,61,2,0,63,86},{82,1,8,38,1,0,68,75},{70,3,7,29,1,0,72,60}}},
    {94,49,28,60,86,0,96,64,0, {{98,1,7,44,1,0,60,35},{86,1,8,38,3,0,66,20},{72,2,6,31,1,0,72,0}}},
    {99,75,47,80,42,0,99,83,1, {{96,1,7,67,1,0,82,18},{81,2,7,55,2,0,86,0},{0,1,7,0,1,0,80,0}}},
    {97,57,32,66,69,0,99,74,0, {{97,1,7,55,1,0,72,18},{83,2,7,46,3,0,81,0},{66,3,7,38,5,0,85,0}}},
    {99,67,37,72,34,0,99,81,0, {{98,1,7,60,3,0,85,0},{74,2,7,46,5,0,88,0},{58,3,7,32,7,0,91,0}}},
    {98,52,25,55,78,0,99,65,0, {{94,1,7,56,4,0,76,0},{76,2,8,39,3,0,81,0},{66,3,6,28,2,0,84,0}}},
    {81,62,42,72,96,90,67,75,1, {{95,1,7,67,1,0,62,90},{84,1,8,46,1,0,65,83},{70,2,6,40,2,0,68,75}}},
    {61,48,39,54,96,94,57,58,0, {{89,1,5,54,1,0,55,89},{83,1,9,36,2,0,55,83},{72,2,7,30,4,0,58,76}}},
    {95,72,44,84,96,94,99,86,0, {{99,1,7,0,1,0,80,0},{0,1,7,0,1,0,80,0},{0,1,7,0,1,0,80,0}}},
    {99,65,40,72,48,0,99,76,0, {{89,1,7,65,7,31,67,0},{70,2,7,58,11,47,73,0},{56,3,7,48,13,79,78,0}}},
    {88,60,40,71,96,90,82,75,1, {{96,1,7,45,1,0,65,83},{80,1,8,31,2,0,70,72},{71,2,6,24,1,0,74,66}}}};
static const uint8_t FM_ORG_LVL[6] = {89, 77, 84, 68, 59, 52}, FM_ORG_RATIO[6] = {1, 2, 3, 4, 6, 8};
static const uint8_t FM_LVL[20] = {0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46};
#define FM_ATTACK_MIN (1716.0f / 256.0f)         /* (a rising stage starts at least this high: MSFA's 1716 << 16) */
#define FM_VEL 16                                /* (velocity 100 at any sensitivity, in MSFA's level units) */

/* one note: operators 0..5 as the DX7's 6..1 (0, 2, 4 the pairs' modulators, 1, 3, 5 their carriers; Soft Organ's
 * all carriers): each one's phase (turns), its ratio to the note, its envelope's level (doublings) and stage (4: the
 * release done), the stage's step a sample and target, its output level and rate scaling (from the note), the gain
 * now and its step a sample (ramped to the next control step); the feedback's last two outputs; the note's scale */
typedef struct {
    float ph[6], r[6], lv[6], inc[6], tg[6], g[6], dg[6], fb0, fb1, norm;
    int16_t ol[6];
    uint8_t rs[6], ix[6], rec, down, on, act;    /* (on: the operators the recipe has, a bit each; act: those loud
                                                  * enough to count now, set at control rate: the rest not looked up) */
} fm_v;

static int fm_outlvl(int l) { return l >= 20 ? 28 + l : FM_LVL[l]; }
/* operator op's patch values in recipe rec, as demo_voice() writes them: rates R1..R4 and levels L1..L4 into rl,
 * then the output level, the ratio (coarse), the fine ratio, the detune, the key scaling's right depth, the
 * carrier's flag */
static void fm_param(int rec, int op, uint8_t *rl, uint8_t *o)
{
    const fm_rec_t *s = &FM_REC[rec];
    const fm_pair_t *p = &s->pair[op / 2];
    const int car = rec == FM_SOFT_ORGAN || (op & 1);
    rl[0] = (op & 1) ? s->attack : s->mod_attack;
    rl[1] = (op & 1) ? s->decay : p->decay;
    rl[2] = s->settle;
    rl[3] = (op & 1) ? s->release : s->mod_release;
    rl[4] = 99;
    rl[5] = (op & 1) ? s->body : p->body;
    rl[6] = (op & 1) ? s->sustain : s->sustain ? p->body : 0;
    rl[7] = 0;
    o[0] = (op & 1) ? p->level : p->mod;
    o[1] = (op & 1) ? p->coarse : p->ratio;
    o[2] = (op & 1) ? 0 : p->fine;
    o[3] = p->detune;
    o[4] = (op & 1) ? 8 : 18;
    o[5] = (uint8_t)car;
    if (rec == FM_SOFT_ORGAN) {
        o[0] = FM_ORG_LVL[op], o[1] = FM_ORG_RATIO[op], o[2] = 0, o[3] = 7;
        rl[5] = rl[6] = 99;
    }
}

static void fm_stage(fm_v *v, int op, int ix)   /* (MSFA's advance) operator op into stage ix */
{
    uint8_t rl[8], o[6];
    int lvl, q;
    v->ix[op] = (uint8_t)ix;
    if (ix > 3)
        return;
    fm_param(v->rec, op, rl, o);
    lvl = rl[4 + ix];
    if (ix == 2 && o[5] && !lvl)                 /* (a struck carrier holds its body: the layer ends the note) */
        lvl = rl[5];
    lvl = ((fm_outlvl(lvl) >> 1) << 6) + v->ol[op] - 4256;
    v->tg[op] = (float)(lvl < 16 ? 16 : lvl) * (1.0f / 256.0f);
    q = ((rl[ix] * 41) >> 6) + v->rs[op];
    q = q > 63 ? 63 : q;
    v->inc[op] = (float)((4 + (q & 3)) << (2 + (q >> 2))) * (1.0f / 16777216.0f);
}

/* a note begins (note: its MIDI number, for the scalings and the detune): every phase 0, every envelope from its
 * attack, from silence; the layer's loudness is the layer's */
static void fm_start(fm_v *v, int rec, int note)
{
    const float lf = (float)note * (1.0f / 12.0f) + 3.03135f;   /* (log2 of its frequency, as MSFA's detune takes it) */
    float sum = 0.0f;
    int op;
    v->rec = (uint8_t)rec, v->down = 1, v->on = v->act = 0;
    v->fb0 = v->fb1 = 0.0f;
    for (op = 0; op < 6; op++) {
        uint8_t rl[8], o[6];
        int sc = 0, off = note - 50 - 17, x = note / 3 - 7;
        fm_param(rec, op, rl, o);
        if (off >= 0)                            /* (the key scaling: linear, falling, over the break point) */
            sc = -((((off + 1) / 3) * o[4] * 329) >> 12);
        sc += fm_outlvl(o[0]);
        v->ol[op] = (int16_t)((sc > 127 ? 127 : sc < 0 ? 0 : sc) * 32 + FM_VEL);
        v->rs[op] = (uint8_t)((x < 0 ? 0 : x > 31 ? 31 : x) >> 3);
        v->r[op] = (o[1] ? (float)o[1] : 0.5f) * (1.0f + 0.01f * (float)o[2]) *
                   fm_exp2f(0.0209f * fm_expf(-0.396f * lf) * (1.0f / 7.0f) * lf * (float)(o[3] - 7));
        v->ph[op] = v->lv[op] = v->g[op] = v->dg[op] = 0.0f;
        if (o[0])
            v->on |= (uint8_t)(1u << op);
        fm_stage(v, op, 0);
        if (o[5] && o[0])                        /* (the carriers at full level: their sum scales the note to 1) */
            sum += fm_exp2f((float)(4032 + v->ol[op] - 4256) * (1.0f / 256.0f) - 14.0f);
    }
    v->norm = sum > 0.0f ? 1.0f / sum : 0.0f;
}

/* (control rate) every envelope n samples on; held 0: the note let go (its release, once); the gains ramped from
 * where they are to the new levels' over the next `ticks` calls of fm_tick (n, or fewer where it runs at a lower
 * rate: the Drone's pads, half) */
static void fm_ctrl(fm_v *v, int held, uint32_t n, uint32_t ticks)
{
    const float fn = (float)n, rn = 1.0f / (float)(ticks ? ticks : 1u);
    int op, act = 0;
    if (!held && v->down) {
        v->down = 0;
        for (op = 0; op < 6; op++)
            fm_stage(v, op, 3);
    }
    for (op = 0; op < 6; op++) {
        float l = v->lv[op], t;
        const int ix = v->ix[op];
        if (ix < 3 || (ix < 4 && !v->down)) {
            if (v->tg[op] > l) {
                l = fm_maxf(l, FM_ATTACK_MIN);
                l += (float)(int)(17.0f - l) * v->inc[op] * fn;
                if (l >= v->tg[op]) {
                    l = v->tg[op];
                    fm_stage(v, op, ix + 1);
                }
            } else {
                l -= v->inc[op] * fn;
                if (l <= v->tg[op]) {
                    l = v->tg[op];
                    fm_stage(v, op, ix + 1);
                }
            }
        }
        v->lv[op] = l;
        t = v->on >> op & 1u ? fm_exp2f(l - 14.0f) : 0.0f;
        if (v->rec == FM_SOFT_ORGAN || (op & 1))
            t *= v->norm;
        v->dg[op] = (t - v->g[op]) * rn;
        if (fm_maxf(t, v->g[op]) > (v->rec == FM_SOFT_ORGAN || (op & 1) ? 1e-5f : 1e-4f))
            act |= 1 << op;                      /* (a carrier under -100 dB, a modulator moving its carrier under
                                                  * 1e-4 of a turn: inaudible, skipped; a struck recipe's modulators
                                                  * die away, its note then three sines, not six) */
    }
    v->act = (uint8_t)act;
}

/* one sample at dt turns a sample (the note's: it may move every call, as the Lead glides) */
static inline float fm_tick(fm_v *v, float dt)
{
    const unsigned a = v->act;
    float y = 0.0f;
    int op;
    if (v->rec == FM_SOFT_ORGAN) {
        for (op = 0; op < 6; op++)
            if (a >> op & 1u)
                y += v->g[op] * sine(v->ph[op]);
    } else {                                     /* (an operator not counting now (act): its sine not looked up) */
        float m = a & 1u ? v->g[0] * sine(v->ph[0] + 4.0f + (v->fb0 + v->fb1) * (1.0f / 256.0f)) : 0.0f;
        if (FM_REC[v->rec].feedback)
            v->fb0 = v->fb1, v->fb1 = m;
        if (a & 2u)
            y = v->g[1] * sine(v->ph[1] + 4.0f + m);
        for (op = 2; op < 6; op += 2)
            if (a >> (op + 1) & 1u)
                y += v->g[op + 1] * sine(v->ph[op + 1] + 4.0f + (a >> op & 1u ? v->g[op] * sine(v->ph[op]) : 0.0f));
    }
    for (op = 0; op < 6; op++) {
        float p = v->ph[op] + dt * v->r[op];
        if (p >= 1.0f)
            p -= (float)(int)p;
        v->ph[op] = p;
        v->g[op] += v->dg[op];
    }
    return y;
}

static int fm_state_ok(const fm_v *v)            /* (for the tests) finite, and no denormal */
{
    int op;
    const float f[2] = {v->fb0, v->fb1};
    for (op = 0; op < 8; op++) {
        const float x = op < 6 ? v->g[op] : f[op - 6], a = fm_fabsf(x);
        if (x != x || a > 1e30f || (a > 0.0f && a < 1.17549435e-38f))
            return 0;
    }
    for (op = 0; op < 6; op++)
        if (v->ph[op] != v->ph[op] || v->ph[op] < 0.0f || v->ph[op] >= 1.0f)
            return 0;
    return 1;
}

#ifdef EN_HOST
/* (host tests only) recipe rec alone at MIDI note `note`, n samples, held for the first half, let go for the second
 * (the control step every 16, as the Sequences have it): 1 if every sample was finite; its loudest (peak), its state
 * sound after (ok) */
int en_fm_probe(int rec, int note, uint32_t n, float *peak, int *ok)
{
    static fm_v v;
    const float dt = 440.0f * fm_exp2f(((float)note - 69.0f) * (1.0f / 12.0f)) * (1.0f / EN_SR);
    uint32_t j;
    int fin = 1;
    *peak = 0.0f;
    if (rec < 0 || rec >= FM_NREC || !FM_NAME[rec][0])
        return 0;
    fm_start(&v, rec, note);
    for (j = 0; j < n; j++) {
        float y;
        if (!(j & 15u))
            fm_ctrl(&v, j < n / 2, 16, 16);
        y = fm_tick(&v, dt);
        if (y != y || fm_fabsf(y) > 1e9f)
            fin = 0;
        else if (fm_fabsf(y) > *peak)
            *peak = fm_fabsf(y);
    }
    *ok = fm_state_ok(&v);
    return fin;
}
uint32_t en_fm_probe_t1(int rec, int note, int op)
{
    static fm_v v;
    uint32_t j;
    fm_start(&v, rec, note);
    for (j = 0; j < 10u * (uint32_t)EN_SR && v.ix[op] < 1; j += 16)
        fm_ctrl(&v, 1, 16, 16);
    return j;
}
#endif
