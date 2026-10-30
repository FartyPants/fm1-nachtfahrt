/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (amb.h). One compilation unit, -O2. */
#include "amb.h"
#include "fastmath.h"
#include "seq.c"

/* (BERLIN: DUB-1's stabs, bass, pings, drums, phrase plan and section machine are gone, and with them the draws
 * its dead parts made on the shared dice: the arc (the arc's sections, the filter motion) is the only movement) */

#ifdef EN_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
/* (Ionian and Aeolian shown as Major and Minor) */
const char *const EN_MODE_NAME[EN_NMODES] = {"Major", "Dorian", "Phrygian", "Lydian", "Mixolyd.", "Minor",
                                             "Pent maj", "Pent min"};
#define B(a, b, c, d, e, f, g) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e) | 1u << (f) | 1u << (g))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    B(0, 2, 4, 5, 7, 9, 11), B(0, 2, 3, 5, 7, 9, 10), B(0, 1, 3, 5, 7, 8, 10), B(0, 2, 4, 6, 7, 9, 11),
    B(0, 2, 4, 5, 7, 9, 10), B(0, 2, 3, 5, 7, 8, 10), B(0, 2, 4, 7, 9, 9, 9),  B(0, 3, 5, 7, 10, 10, 10)};
#undef B
const uint8_t EN_LOOP_LEN[EN_NLOOP] = {13, 17, 23, 29};
const uint8_t EN_DRUM_N[EN_NDRUM] = {16, 16, 16, 16, 16};
const char *const EN_SEC_NAME[EN_NSEC] = {"Emerge", "Flow", "Solo", "Bridge"};   /* (BERLIN's: the arc) */

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Chord", 0, 100, 60},    {"Sequence", 0, 100, 70},    {"Lead", 0, 100, 40},     {"Sequence 2", 0, 100, 60},
    {"Density", 0, 100, 60},  {"Evolve", 0, 100, 40},   {"Drift", 0, 6, 4},       {"Repeat A", 0, 100, 100},
    {"Echo", 0, 100, 55},     {"Echo time", 0, 5, 3},   {"Reverb", 0, 100, 35},   {"Feedback", 0, 100, 60},
    {"Cutoff", 0, 100, 45},   {"Decay", 0, 100, 35},    {"Sweep", 0, 100, 50}, {"Motion", 0, 100, 50},
    {"Tune", -50, 50, 0},     {"MIDI out", 0, 1, 1},    {"Tempo", 60, 160, 96},  {"Root", 0, 11, 5},
    {"Mode", 0, EN_NMODES - 1, MODE_DORIAN}, {"Seq oct", -2, 2, 0}, {"Chord voice", 0, EN_NCV - 1, 0},
    {"Seq2 oct", -2, 1, -1},   {"Grid", 0, 1, 0},      {"Transpose", 0, 2, 0},
    {"Repeat A time", 0, 5, 1},    {"Repeat B", 0, 100, 0},
    {"Length", 3, 16, 8},    {"Atmosphere", 0, 100, 25},  {"Repeat B time", 0, 5, 2},     {"Size", 0, 100, 55},
    {"Cloud", 0, 100, 30},
    {"Seq voice", 0, EN_NSV - 1, 0}, {"Lead voice", 0, EN_NBV - 1, 0}, {"Seq2 voice", 0, EN_NS2V - 1, 1},
    {"Attack", 0, 100, 40},   {"Colour", -50, 50, 0},   {"Resonance", -50, 50, 0}, {"Release", 0, 100, 50},
    {"Pedal", 0, 100, 70},    {"Seq reverb", 0, 100, 50}, {"Seq echo", 0, 100, 50}, {"Lead reverb", 0, 100, 90},
    {"Lead oct", -2, 1, 0},   {"Vibrato", 0, 100, 40},  {"Lead echo", 0, 100, 90}, {"Glide", 0, 100, 25},
    {"Atmos time", 0, 100, 50}, {"Drone reverb", 0, 100, 50}, {"Drone echo", 0, 100, 50}, {"Seq2 echo", 0, 100, 50},
    {"Drone width", 0, 100, 20}, {"Drone time", 0, 100, 50}, {"Drone rate", 0, 100, 30}, {"Drone intro", 0, 64, 0},
    {"Decay 2", 0, 100, 35},    {"Seq time", 0, 100, 50}, {"Seq rate", 0, 100, 30}, {"Seq intro", 0, 64, 0},
    {"Lead width", 0, 100, 20}, {"Lead attack", 0, 100, 10}, {"Lead time", 0, 100, 50}, {"Lead rate", 0, 100, 30},
    {"Root 1", 0, 11, 0}, {"Root 2", 0, 11, 0}, {"Root 3", 0, 11, 0}, {"Root 4", 0, 11, 0},
    {"Root 5", 0, 11, 0}, {"Root 6", 0, 11, 0}, {"Root 7", 0, 11, 0}, {"Root 8", 0, 11, 0},
    {"Kind 1", 0, EN_NCQ - 1, 0}, {"Kind 2", 0, EN_NCQ - 1, 0}, {"Kind 3", 0, EN_NCQ - 1, 0}, {"Kind 4", 0, EN_NCQ - 1, 0},
    {"Kind 5", 0, EN_NCQ - 1, 0}, {"Kind 6", 0, EN_NCQ - 1, 0}, {"Kind 7", 0, EN_NCQ - 1, 0}, {"Kind 8", 0, EN_NCQ - 1, 0},
    {"Type 1", 0, EN_NCX - 1, 0}, {"Type 2", 0, EN_NCX - 1, 0}, {"Type 3", 0, EN_NCX - 1, 0}, {"Type 4", 0, EN_NCX - 1, 0},
    {"Type 5", 0, EN_NCX - 1, 0}, {"Type 6", 0, EN_NCX - 1, 0}, {"Type 7", 0, EN_NCX - 1, 0}, {"Type 8", 0, EN_NCX - 1, 0},
    {"Play", 0, 6, 1},
    {"Steps", -32768, 32767, 0}, {"Steps 2", -32768, 32767, 0}, {"Octaves", -32768, 32767, 0},
    {"Octaves 2", -32768, 32767, 0},  {"Skips", -32768, 32767, 0},
    {"Accents", -32768, 32767, 0},   {"Accents 2", -32768, 32767, 0},   {"Seq2 intro", 0, 64, 0},
    {"Lead intro", 0, 64, 0},    {"Seq2 width", 0, 100, 20},
    {"Chance", 0, 100, 100}, {"Chance 2", 0, 100, 100}, {"Lead in", 0, 64, 16}, {"Seq2 time", 0, 100, 50},
    {"Seq2 rate", 0, 100, 30},
    {"Accent", 0, 100, 50}, {"Seq play", 0, 1, 0},
    {"Noise tone", -50, 50, 0}, {"Noise density", -50, 50, 0}, {"Air", -50, 50, 0}, {"Noise drift", -50, 50, 0},
    {"Noise", 0, EN_NNCHAR - 1, 0},
    {"Phaser", 0, 100, 40},   {"Cutoff 2", 0, 100, 40},  {"Phrases", 0, 100, 50},   {"Seq2 reverb", 0, 100, 50},
    {"Lead tone", 0, 100, 50}, {"Length 2", 3, 16, 7}, {"Blur", 0, 100, 50}, {"Echo tone", 0, 100, 50},
    {"Width", 0, 100, 50},    {"Ring", 0, 100, 50},
    {"Noise width", 0, 100, 50}, {"Noise plate", 0, 100, 50}, {"Noise echo", 0, 100, 50}, {"Atmos width", 0, 100, 20},
    {"Noise LFO", 0, 100, 0}, {"Arc", 0, 100, 0},
    {"Atmos rate", 0, 100, 30}, {"Atmos intro", 0, 64, 0}, {"Bridge", 0, 100, 40}, {"Memory", 0, 100, 50},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }
/* (the voices batch, item 5) a shake's voices (the user, 2026-10-30: "currently only add the choir, we just want to
 * hear the fm voices"): those before the FM voices, the Drone's six with Choir; the FM voices only by ALGORITHM */
int en_voice_shake_n(int p)
{
    switch (p) {
    case P_CVOICE: return EN_CV_FM;
    case P_SVOICE: return EN_SV_FM;
    case P_S2VOICE: return EN_S2V_FM;
    case P_LVOICE: return EN_BV_FM;
    default: return en_param_info(p)->hi - en_param_info(p)->lo + 1;
    }
}

/* (BERLIN) the pieces: eight recipes for the whole machine (the FM-1's PRESETS, the web app's cards), each what it
 * sets over the defaults (PARAMS), in pairs, to -1. Tune and MIDI out are never set here: they belong to the player.
 * Keys and tempos are starting points (spec 6: from analysis sites, to tune by ear); the Lead is dreamy in all of them
 * (its reverb and echo 90, the defaults: the user); an Atmosphere only where it suits, of the four the user likes.
 * (Voices: the Sequence's 2600 Moog Synthi Pulse Glass Pluck (0..5) and the FM voices (6..15), Sequence 2's the
 *  five, Sub, Pluck (0..6) and the FM voices (7..16);
 *  the Drone's Organ Strings Horn Saws Glass Choir and the FM pads Warm Air, FM Strings, Soft Organ (6..8); the Lead's Odyssey Mini Flute Theremin Synthi
 *  and the FM voices Soft Lead, Wood Reed, Soft Brass (5..7). Modes: 0 Ionian,
 *  1 Dorian, 2 Phrygian, 5 Aeolian. Repeat times: 1/8 1/8. 1/4 1/4. 1/2 1 bar. Drift: 2 4 8 16 32 64 bars, Hold.
 *  Play: the walk Home, Wander, Free. Grid: 8ths, 16ths. Atmosphere: None Wind Surf Space Air Tape Rain. The moves (filter motion spec 9): MW/MT/MR/MI a layer's
 *  Width, Time, Rate and Intro in bars (0 Drone, 1 Sequence, 2 Lead, 3 Sequence 2, 4 Atmosphere; the Sequence's Width
 *  is Sweep); the Lead's Width kept low; Bridge the middle section's move. Memory: how much the Lead keeps its ideas,
 *  the Lead's memory spec 3.3's starting values, to tune by ear; every piece's set, Mirage's 50 the default too, so
 *  the table reads whole) */
static const int16_t PC_TIMEWIND[] = {        /* Schulze, Bayreuth Return: an organ through the phaser; Sequence 2
                                                * at its default 60 and the reverb at 50, the lengths 16/15 (the user's
                                                * ear) */
    P_TEMPO, 96, P_ROOT, 2, P_MODE, MODE_AEOLIAN,
    P_SVOICE, 0, P_SEQLEN, 16, P_SEQ2LEN, 15, P_SEQGRID, 1, P_DENSITY, 65, P_REVERB, 50,
    P_TAPB, 50, P_TAPBT, 2,
    P_CVOICE, 0, P_PHASER, 60, P_DRIFT, 3, P_PMODE, EN_WALK_HOME,
    P_LVOICE, 0, P_LDIN, 32,
    P_NCHAR, 1, P_CRACKLE, 7,
    P_MW0, 30, P_MR0, 25, P_MW2, 15, P_MW4, 30, P_BRIDGE, 45,
    P_MI1, 32, P_ARC, 25, P_MOTION, 45, P_LMEM, 55, -1};
static const int16_t PC_WAHNFRIED[] = {       /* Schulze, Wahnfried 1883: roots walking with no home, strings, the
                                                * Sequence a faint heartbeat under them (the user's ear) */
    P_TEMPO, 80, P_ROOT, 10, P_MODE, MODE_DORIAN,
    P_SEQ, 30, P_MI1, 16, P_SEQ2, 50, P_SEQ2OCT, -1,
    P_CHORD, 95, P_CVOICE, 1, P_PEDAL, 85, P_PHASER, 50, P_CRES, -5, P_DRIFT, 2, P_PMODE, EN_WALK_FREE,
    P_LEAD, 54, P_LVOICE, 3, P_LTONE, 75, P_VIB, 55, P_LDENS, 60, P_LDIN, 8,
    P_FEEDBACK, 75, P_REVERB, 45,
    P_MW0, 45, P_MT0, 60, P_MR0, 45, P_MW2, 15, P_BRIDGE, 30,
    P_ARC, 20, P_MOTION, 55, P_LMEM, 40, -1};
static const int16_t PC_PHAEDRA[] = {         /* Tangerine Dream, Phaedra: a Moog bass line, Mellotron flutes and choir */
    P_TEMPO, 149, P_ROOT, 2, P_MODE, MODE_AEOLIAN,
    P_SVOICE, 1, P_SEQOCT, -1, P_SEQLEN, 16, P_DENSITY, 70, P_SWEEP, 85,
    P_SEQ2, 40, P_S2VOICE, 1, P_SEQ2OCT, 0, P_SEQ2LEN, 15, P_MI3, 0,
    P_CHORD, 70, P_CVOICE, 5, P_DRIFT, 4,         /* (the user's ear: the Drone a formant Choir, at 70) */
    P_LVOICE, 2, P_LDIN, 24,
    P_NCHAR, 5, P_CRACKLE, 10,
    P_MW0, 25, P_MW3, 30, P_MR3, 40, P_MW4, 25,
    P_MI1, 16, P_ARC, 30, P_MOTION, 55, P_LMEM, 75, -1};
static const int16_t PC_RUBYCON[] = {         /* Tangerine Dream, Rubycon: dark, two sequences out of the drone (the
                                                * octaves and voices the user's ear: the Sequence -2 on the 2600, Sequence 2 0,
                                                * the Lead the Synthi) */
    P_TEMPO, 144, P_ROOT, 5, P_MODE, MODE_AEOLIAN,
    P_SVOICE, 0, P_SEQOCT, -2, P_SEQLEN, 8, P_CUTOFF, 32, P_SWEEP, 60,
    P_SEQ2, 55, P_S2VOICE, 5, P_SEQ2OCT, 0, P_SEQ2LEN, 7, P_MI3, 32,
    P_TAPB, 40, P_TAPBT, 4,
    P_CHORD, 65, P_CVOICE, 0, P_CCOL, -20, P_PEDAL, 85, P_DRIFT, 4,
    P_LVOICE, 4, P_LDIN, 32,
    P_MW0, 35, P_MR0, 25, P_MW3, 30, P_BRIDGE, 50,
    P_MI1, 16, P_ARC, 40, P_MOTION, 65, P_LMEM, 45, -1};
static const int16_t PC_STRATOSFEAR[] = {     /* Tangerine Dream, Stratosfear: brighter, driving, the Lead forward */
    P_TEMPO, 138, P_ROOT, 4, P_MODE, MODE_AEOLIAN,
    P_SVOICE, 3, P_SEQLEN, 16, P_DENSITY, 70, P_CUTOFF, 55,
    P_SEQ2, 60, P_S2VOICE, 0, P_SEQ2LEN, 15, P_MI3, 0,
    P_CVOICE, 1, P_DRIFT, 3,
    P_LEAD, 52, P_LVOICE, 1, P_LDENS, 65, P_LDIN, 12,
    P_MR1, 40, P_MW3, 30, P_MR3, 40, P_BRIDGE, 35,
    P_ARC, 20, P_MOTION, 50, P_LMEM, 85, -1};
static const int16_t PC_SUNRAIN[] = {         /* Ashra, New Age of Earth: major, the strongest repeats, bright strings;
                                                * the lengths 8/7 (the user: "works best") */
    P_TEMPO, 130, P_ROOT, 2, P_MODE, MODE_IONIAN,
    P_SVOICE, 3, P_SEQLEN, 8, P_CUTOFF, 55,
    P_SEQ2, 50, P_S2VOICE, 4, P_SEQ2OCT, 0, P_SEQ2LEN, 7, P_MI3, 0,
    P_TAPA, 100, P_TAPB, 100, P_TAPBT, 3, P_ECHO, 65, P_FEEDBACK, 65,
    P_CVOICE, 1, P_DRIFT, 3,
    P_LVOICE, 0, P_LDIN, 16,
    P_NCHAR, 2, P_CRACKLE, 5,
    P_MW3, 25, P_MW4, 25, P_BRIDGE, 35,
    P_ARC, 25, P_MOTION, 45, P_LMEM, 60, -1};
static const int16_t PC_MIRAGE[] = {          /* Schulze, Mirage: icy, a glass sequence, sparse, a long reverb; the
                                                * lengths 12/11 (the user's) */
    P_TEMPO, 95, P_ROOT, 6, P_MODE, MODE_AEOLIAN,
    P_SVOICE, 4, P_SEQLEN, 12, P_SEQGRID, 1, P_DENSITY, 40, P_SWEEP, 30,
    P_SEQ2, 45, P_S2VOICE, 4, P_SEQ2OCT, 0, P_SEQ2LEN, 11, P_MI3, 24,
    P_TAPB, 60, P_TAPBT, 4,
    P_CVOICE, 4, P_PHASER, 85, P_DRIFT, 4,
    P_LVOICE, 3, P_LDIN, 24,
    P_REVERB, 55, P_SIZE, 90,
    P_NCHAR, 3, P_CRACKLE, 10,
    P_MW0, 25, P_MR0, 20, P_MW4, 35, P_MR4, 20,
    P_MI1, 16, P_ARC, 20, P_MOTION, 40, P_LMEM, 50, -1};
static const int16_t PC_CYBORG[] = {          /* Schulze, Cyborg: early, raw, deep organ drones beating; a quiet
                                                * Sequence (20, the user's: at its defaults), no Sequence 2; the Lead
                                                * up front (70, the user's) */
    P_TEMPO, 70, P_ROOT, 0, P_MODE, MODE_PHRYGIAN,
    P_SEQ, 20, P_SEQ2, 0,
    P_CHORD, 70, P_CVOICE, 0, P_CCOL, 25, P_PEDAL, 90, P_PHASER, 20, P_DRIFT, 4, P_PMODE, EN_WALK_WANDER,
    P_LEAD, 70, P_LVOICE, 3, P_LDENS, 30, P_LOCT, -1, P_LDIN, 8,
    P_MW0, 50, P_MT0, 70, P_MR0, 45, P_MW2, 10, P_BRIDGE, 30,
    P_ARC, 15, P_MOTION, 35, P_LMEM, 35, -1};
static const struct {
    const char *name;
    const int16_t *set;
} PIECES[EN_NPIECES] = {
    {"Timewind", PC_TIMEWIND}, {"Wahnfried", PC_WAHNFRIED}, {"Phaedra", PC_PHAEDRA},   {"Rubycon", PC_RUBYCON},
    {"Stratosfear", PC_STRATOSFEAR}, {"Sunrain", PC_SUNRAIN}, {"Mirage", PC_MIRAGE}, {"Cyborg", PC_CYBORG},
};

const char *en_piece_name(int i) { return PIECES[(unsigned)i < EN_NPIECES ? i : 0].name; }
void en_piece(int i, int16_t *par, uint16_t *pool)
{
    const int16_t *s;
    int p;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = PARAMS[p].def;
    for (s = PIECES[i].set; s[0] >= 0; s += 2)
        par[s[0]] = s[1];
    *pool = EN_MODE_POOL[par[P_MODE]];
}

const char *const EN_CV_NAME[EN_NCV] = {"Organ", "Strings", "Horn", "Saws", "Glass", "Choir", "Warm Air", "FMStrings",
                                        "SoftOrgan"};   /* (BERLIN: the Drone's;
                                                 * the voices batch: Horn was Choir, its place kept; the formant Choir
                                                 * appended: pieces and saves sound the same) */
const char *const EN_BV_NAME[EN_NBV] = {"Odyssey", "Mini", "Flute", "Theremin", "Synthi", "Soft Lead", "Wood Reed",
                                        "SoftBrass"};   /* (BERLIN: the Lead's) */
const char *const EN_CQ_NAME[EN_NCQ] = {"Empty", "Major", "Minor", "Sus2", "Sus4", "Dim", "Aug"};
const char *const EN_CX_NAME[EN_NCX] = {"Triad", "6", "7", "Maj7", "9", "11", "Add9"};
static const char *const ETIME_NAME[6] = {"1/8", "1/8.", "1/4", "1/4.", "1/2", "1 bar"};
static const float ETIME_BEATS[6] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f};
static const uint8_t DRIFT_BARS[7] = {2, 4, 8, 16, 32, 64, 0};

int en_drift_bars(int v) { return DRIFT_BARS[(unsigned)v < 7u ? v : 6]; }

static char *itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

/* (the voices batch) a sequence's voice knob (l: 0 the Sequence, 1 Sequence 2) as the engine's voice, EN_SV_NAME's
 * index: Pluck was appended to both lists, so the Sequence's 5 is Pluck (6 here) where Sequence 2's 5 is Sub; the FM
 * voices after it (item 5), the Sequence's 6.. their 7..; a value out of range: 2600 */
#define SV_PLUCK 6
#define SV_FM0 7                                 /* (the engine's first FM voice: 7 .. 16, SV_FM_REC's) */
static int sv_of(int l, int v)
{
    if (l)
        return (unsigned)v < EN_NS2V ? v : 0;
    return (unsigned)v < EN_NSV ? (v < EN_SV_FM - 1 ? v : v + 1) : 0;
}

void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    if (i == P_ETIME)
        s = ETIME_NAME[(unsigned)v < 6u ? v : 0];
    else if (i == P_MIDI)
        s = ONOFF[v ? 1 : 0];
    else if (i == P_CVOICE)
        s = EN_CV_NAME[(unsigned)v < EN_NCV ? v : 0];
    else if (i == P_SVOICE)
        s = EN_SV_NAME[sv_of(0, v)];
    else if (i == P_LVOICE)
        s = EN_BV_NAME[(unsigned)v < EN_NBV ? v : 0];
    else if (i == P_S2VOICE)
        s = EN_SV_NAME[sv_of(1, v)];
    else if (i >= P_CR0 && i <= P_CR7)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    else if (i >= P_CQ0 && i <= P_CQ7)
        s = EN_CQ_NAME[(unsigned)v < EN_NCQ ? v : 0];
    else if (i >= P_CX0 && i <= P_CX7)
        s = EN_CX_NAME[(unsigned)v < EN_NCX ? v : 0];
    else if (i == P_PMODE)
        s = v & EN_PLOOP ? "Loop" : "Endless";
    else if (i == P_NCHAR)
        s = EN_NCHAR_NAME[(unsigned)v < EN_NNCHAR ? v : 0];
    else if (i == P_SMODE)
        s = v ? "Pattern" : "Auto";
    else if (i == P_SEQGRID)
        s = v ? "16ths" : "8ths";
    else if (i == P_TAPAT || i == P_TAPBT)
        s = ETIME_NAME[(unsigned)v < 6u ? v : 0];          /* the echo's times: 1/8 .. 1 bar */
    else if (i == P_TRANS)
        s = v == 2 ? "Bed" : v ? "Diatonic" : "Parallel";
    else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    else if (i == P_DRIFT && !en_drift_bars(v))
        s = "Hold";
    if (s) {
        int k = 0;
        while (s[k] && k < 9) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i == P_DRIFT) {
        b = itoa_s(en_drift_bars(v), b);
        *b++ = ' ';
        *b++ = 'b';
        *b++ = 'a';
        *b++ = 'r';
        *b++ = 's';
        *b = 0;
        return;
    }
    if (i == P_FEEDBACK) {                       /* (1.3.0) as the tail's length, to -60 dB: 1.5 .. 15 s */
        int ds = (int)(15.0f * fm_exp2f((float)v * (1.0f / 30.0f)) + 0.5f);   /* (in tenths) */
        b = itoa_s(ds / 10, b);
        *b++ = '.';
        *b++ = (char)('0' + ds % 10);
        *b++ = ' ';
        *b++ = 's';
        *b = 0;
        return;
    }
    if (PARAMS[i].lo < 0 && v > 0)               /* (a signed value, above its zero) */
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------- the pure parts --- */
uint32_t en_euclid(int k, int n, int rot)
{
    /* Bjorklund's algorithm (as Toussaint's "The Euclidean Algorithm Generates Traditional Musical
     * Rhythms"): k groups "x" and n - k groups ".", the remainders folded onto the heads until at most
     * one remains. Groups are bit strings, LSB first: (bits, length). */
    uint32_t gb[32], m = 0;
    uint8_t gl[32];
    int ng, heads, tails, i, pos = 0;
    if (n <= 0 || n > 32 || k <= 0)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++) {
        gb[i] = i < k ? 1u : 0u;
        gl[i] = 1;
    }
    ng = n;
    heads = k;
    tails = n - k;
    while (tails > 1 && heads > 0) {
        int pairs = heads < tails ? heads : tails;
        for (i = 0; i < pairs; i++) {           /* a tail onto each head */
            gb[i] |= gb[ng - pairs + i] << gl[i];
            gl[i] = (uint8_t)(gl[i] + gl[ng - pairs + i]);
        }
        if (heads > tails) {                    /* the unpaired heads become the new tails */
            tails = heads - pairs;
            heads = pairs;
        } else {
            tails -= pairs;
            heads = pairs;
        }
        ng = heads + tails;
    }
    for (i = 0; i < ng; i++) {
        uint32_t j;
        for (j = 0; j < gl[i]; j++)
            if (gb[i] >> j & 1u)
                m |= 1u << (((pos + (int)j + rot) % n + n) % n);
        pos += gl[i];
    }
    return m;
}

int en_scale(uint32_t pool, int *steps)
{
    int i, m = 0;
    pool |= 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[m++] = i;
    return m;
}

/* the chord's tones in stacking order: the root, then every other scale step (thirds in a seven-note
 * scale, fourths in a pentatonic), then what's left; up to max of them, distinct */
static int stack(uint32_t pool, int root, int deg, int *pcs, int max)
{
    static const uint8_t OFF[7] = {0, 2, 4, 6, 1, 3, 5};
    int st[12], m = en_scale(pool, st), n = 0, i, j;
    deg = ((deg % m) + m) % m;
    for (i = 0; i < 7 && n < max; i++) {
        int pc = (root + st[(deg + OFF[i]) % m]) % 12, dup = 0;
        for (j = 0; j < n; j++)
            dup |= pcs[j] == pc;
        if (!dup)
            pcs[n++] = pc;
    }
    return n;
}

int en_chord_tones(uint32_t pool, int root, int deg, int *pcs)
{
    int st[12];
    return stack(pool, root, deg, pcs, en_scale(pool, st) >= 6 ? 4 : 3);
}

void en_voicing(uint32_t pool, int deg, int shape, int *iv)
{
    int t[5], n = stack(pool, 0, deg, t, 5), i, j, r = t[0], d[5];
    for (i = 0; i < 5; i++)                      /* intervals above the chord's root; a short pool repeats */
        d[i] = (t[i < n ? i : i % n] - r + 12) % 12;
    if (shape) {                                 /* root, 7th, 10th, 14th: a 9th without its fifth */
        iv[0] = 0;
        iv[1] = d[3];
        iv[2] = d[1] + 12;
        iv[3] = d[n >= 5 ? 4 : 2] + 12;
    } else {                                     /* root, 5th, 7th, 10th: the open 7th */
        iv[0] = 0;
        iv[1] = d[2];
        iv[2] = d[3];
        iv[3] = d[1] + 12;
    }
    for (i = 1; i < 4; i++) {                    /* ascending, no two alike */
        for (j = i; j > 0 && iv[j] < iv[j - 1]; j--) {
            int x = iv[j];
            iv[j] = iv[j - 1];
            iv[j - 1] = x;
        }
    }
    for (i = 1; i < 4; i++)
        while (iv[i] <= iv[i - 1])
            iv[i] += 12;
}

const char *en_chord_name(const int *pcs, int n, char *buf)
{
    uint32_t iv = 0;
    int i, k = 0, third, fifth;
    const char *q = "", *r = EN_NOTE_NAME[pcs[0] % 12];
    for (i = 1; i < n; i++)
        iv |= 1u << ((pcs[i] - pcs[0] + 12) % 12);
    third = iv >> 4 & 1u ? 4 : iv >> 3 & 1u ? 3 : 0;
    fifth = iv >> 7 & 1u ? 7 : iv >> 6 & 1u ? 6 : iv >> 8 & 1u ? 8 : 0;
    if (third == 4) {
        q = fifth == 8 ? "aug" : iv >> 11 & 1u ? "M7" : iv >> 10 & 1u ? "7" : iv >> 9 & 1u ? "6"
            : iv >> 2 & 1u ? "add9" : "";
    } else if (third == 3) {
        if (fifth == 6)
            q = iv >> 10 & 1u ? "m7b5" : iv >> 9 & 1u ? "dim7" : "dim";
        else
            q = iv >> 10 & 1u ? "m7" : iv >> 11 & 1u ? "mM7" : iv >> 9 & 1u ? "m6" : iv >> 2 & 1u ? "madd9" : "m";
    } else {
        q = iv >> 5 & 1u ? (iv >> 10 & 1u ? "7sus4" : "sus4") : iv >> 2 & 1u ? "sus2" : "5";
    }
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_MUTE, C_PERF, C_FX, C_SEQ, C_MACRO, C_PROT, C_REWALK, C_LOCK, C_SEQSET, C_LKEY, C_MOVE, C_LFORGET };
#define QN 512u                                 /* (a piece is every parameter at once: room for twice over and more) */
_Static_assert(QN >= 2u * (P_NPARAMS + 8), "the command queue can't hold a piece's parameters twice over");
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_pool(uint32_t pool) { post(C_POOL, 0, (int)(pool & 0xFFFu)); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_mute(uint32_t m) { post(C_MUTE, (int)(m & 0xFFu), 0); }
void en_perf(uint32_t f) { post(C_PERF, (int)(f & 0xFFu), 0); }
void en_fx(uint32_t m) { post(C_FX, 0, (int)(m & 0xFFFFu)); }
void en_move(uint32_t m) { post(C_MOVE, 0, (int)(m & 0xFFu)); }
static uint8_t mac_sent;                         /* (the UI's turns of the master actions, counted: 6 bits) */
volatile uint8_t en_mac_got;                     /* (the engine's: the count with the last it took) */
void en_macro(int i, int v)                      /* (sent as 0..100) */
{
    mac_sent = (uint8_t)((mac_sent + 1u) & 63u);
    post(C_MACRO, (i & 3) | mac_sent << 2, v + 50);
}
int en_macro_now(int i) { return en_mac_got == mac_sent ? en_mac[i] : EN_MAC_BUSY; }
void en_lock(int on) { post(C_LOCK, on != 0, 0); }
void en_prog_rotate(void) { post(C_PROT, 0, 0); }
void en_prog_rewalk(void) { post(C_REWALK, 0, 0); }
const char *const EN_FX_NAME[EN_NFX] = {"Repeat", "Time", "Tape stop", "Shimmer", "Low-pass", "High-pass", "Sweep",
                                        "Phaser", "Reverb", "Take-off", "Freeze", "Wash", "Octave", "Return", "Twitter",
                                        "Supernova"};

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen, en_ext;
volatile uint8_t en_chord[EN_NPAD], en_chord_root, en_chord_n;
volatile float en_drift, en_pad_level, en_bass_level;
volatile uint8_t en_loop_pos[EN_NLOOP];
volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];
volatile float en_loop_glow[EN_NLOOP];
volatile uint8_t en_drum_pos[EN_NDRUM];
volatile uint32_t en_drum_pat[EN_NDRUM];
volatile float en_drum_glow[EN_NDRUM];
volatile uint32_t en_bars;
volatile uint8_t en_pulse_rest;
volatile uint8_t en_pad_note[EN_NPAD];
volatile float en_pad_amp[EN_NPAD];
volatile uint8_t en_section, en_phrase_bar;
volatile uint32_t en_stab_pat, en_throws, en_stabs;
volatile float en_stab_vel, en_echo_ms, en_echo_g, en_echo_in, en_duck;
volatile uint8_t en_stab_thrown;
volatile uint16_t en_lane[6];
volatile int16_t en_scope[EN_SCOPE_N];
volatile uint32_t en_scope_w;
volatile uint32_t en_hits[5];
volatile float en_hit_vel[5];
volatile float en_layer_act[5];                  /* (BERLIN) each layer's activity, for the meters (layer_act) */
volatile uint32_t en_seq_hits[2], en_seq_reps[2];   /* (BERLIN) each sequence's notes, and its repeats, so far */
static float atm_g_out;                          /* (the Atmosphere's gain this chunk, for its meter) */
volatile float en_atm_gain;
volatile float en_echo_pk;
volatile uint8_t en_load;
static int32_t shed_left;                        /* samples the shedding lasts (a heavy block: half a second) */
#define SHED (shed_left > 0)
volatile uint8_t en_nev_k[EN_NEV_N], en_nev_s[EN_NEV_N], en_nev_p[EN_NEV_N];
volatile uint32_t en_nev_w;
static void nev(int k, float s, float p)         /* a noise event, for the screen (only the ones that show) */
{
    uint32_t w = en_nev_w;
    if (s < 0.08f)
        return;
    en_nev_k[w % EN_NEV_N] = (uint8_t)k;
    en_nev_s[w % EN_NEV_N] = (uint8_t)(fm_minf(s, 1.0f) * 255.0f);
    en_nev_p[w % EN_NEV_N] = (uint8_t)(fm_clampf(p, 0.0f, 1.0f) * 255.0f);
    en_nev_w = w + 1u;
}
static void hit(int l, float vel) { en_hit_vel[l] = vel; en_hits[l]++; }
static float scope_acc;
static uint32_t scope_ph;
static uint32_t mutes, perf, fxm, fxm_was;      /* performance: the layers muted, the moves held; the effects held */
static float perf_sw, perf_echo, mb_c[4], mb_s[4];   /* (mb: Open's and Shut's filter on the music, the kick left out) */                /* the filter pushed open (+) or shut (-), the echo pushed */

static int16_t par[P_NPARAMS];
/* the noise's controls (P_NCOL .. P_NCHAR: render_crackle reads them) */
typedef struct {
    float colour, grain, room, drift;           /* dark .. bright; smooth .. granular; dry .. roomy; steady .. restless */
    uint8_t character;                          /* NCHAR[] */
} noise_ctl_t;
static noise_ctl_t nctl;
static uint8_t atm_char;                        /* (BERLIN) the Atmosphere's character (render_atmos) */
static uint32_t atm_rng;                        /* (its random: en_init's seed) */
static void atm_reset(void);
static int ec_pp;                               /* (BERLIN) the echo ping-pongs (twice its time fits the tape) */
static float ec_g = 0.6f, ec_lpc = 0.68f, ec_wide = 1.0f;   /* the echo's Blur, Tone, Width (and the chord's and the
                                                             * pings': widen) */
static int8_t mac[EN_NMAC];                     /* the master actions, -50..50 */
volatile int8_t en_mac[EN_NMAC];
static int8_t pofs[P_NPARAMS];
static float dub_up;                            /* Dub turned up, 0..1: the echo darker, feeding back past its knob */                  /* what they add to the piece's parameters */
static int pe(int p)                            /* a parameter as heard: the piece's, moved by the master actions */
{
    int v = par[p] + (par[p] || (p != P_SEQ && p != P_SEQ2) ? pofs[p] : 0);   /* (a sequence the piece keeps silent,
                                                  * level 0, stays silent: Energy doesn't wake it) */
    return v < PARAMS[p].lo ? PARAMS[p].lo : v > PARAMS[p].hi ? PARAMS[p].hi : v;
}
static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale */
static int deg;                                 /* the chord's scale degree */
static int chord_pcs[4], chord_nt;
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (par[P_MIDI])
        en_midi_out(st, d1, d2);
}

static float rel(int p) { return (float)par[p] * 0.02f; }   /* a sound page's value, -1 .. +1 */

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }

/* a soft clip, cheap: x - 4/27 x^3, flat at +-1.5 (to +-1) */
static inline float soft(float x)                /* tanh, nearly: x (27 + x^2) / (27 + 9 x^2), 1 past 3 */
{
    float x2;
    if (x > 3.0f)
        return 1.0f;
    if (x < -3.0f)
        return -1.0f;
    x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
static inline float sat(float x)
{
    if (x > 1.5f)
        return 1.0f;
    if (x < -1.5f)
        return -1.0f;
    return x - 0.148148148f * x * x * x;
}

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)              /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return sint[i] + (sint[i + 1] - sint[i]) * f;
}
static float orgt[SN + 1];                      /* (BERLIN) the Drone's Organ: four drawbars, 1, 1/2, 1/3, 1/4, in one */
static inline float organ(float ph)
{
    float x = ph * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return orgt[i] + (orgt[i + 1] - orgt[i]) * f;
}

static inline float blep_saw(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}

static void svf_coefs(float *c, float f, float k)   /* TPT SVF (Zavalishin): a1, a2, a3, k */
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 40.0f, 16000.0f) * (1.0f / EN_SR));
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}

#include "fm.c"                                  /* (the voices batch, item 5) the FM voice: fm1_open's recipes */

/* ------------------------------------------------- slow random motion --- */
#define NMOD 2                                  /* the echo's: 0 its band-pass and wow, 1 its first head's place */
static struct {
    float v, tgt, rate;
    uint32_t left;                              /* samples until a new target */
} mod[NMOD];

static void mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD; i++) {
        if (mod[i].left <= n) {
            float secs = 7.0f + 33.0f * frand();
            mod[i].tgt = frand() * 2.0f - 1.0f;
            mod[i].left = (uint32_t)(secs * EN_SR);
            mod[i].rate = (float)n / (secs * EN_SR * 0.5f);
        } else {
            mod[i].left -= n;
        }
        mod[i].v += (mod[i].tgt - mod[i].v) * fm_minf(mod[i].rate, 1.0f);
    }
}
static float motion, evolve;                    /* 0..1: Motion, Evolve */

/* -------------------------------------------------------------- chord --- */
/* the chord as voiced: its four notes (the Drone's, drone_chord), its level (the Drone's) */
typedef struct {
    uint8_t note;
} stab_t;
static stab_t stab[EN_NPAD];
static float chord_vol;
static int shape, root_note = 55;               /* the voicing's shape, the chord's root as voiced (MIDI) */
/* the Drone's voices by number (EN_CV_NAME; a regrow's voicing follows them, regrow: once the dub's names, the same
 * numbers); Choir (CV_CHOIR) below; the FM pads (the voices batch, item 5) from CV_FM0: fm.c's recipes, and their
 * level against the other voices', one constant, the user's to set by ear */
enum { CV_ORGAN, CV_STRINGS, CV_HORN, CV_SAWS, CV_GLASS };
#define CV_FM0 EN_CV_FM
static const uint8_t CV_FM_REC[EN_NCV - CV_FM0] = {FM_WARM_AIR, FM_STRINGS, FM_SOFT_ORGAN};
#define CV_FM_G 1.0f
static int cvoice;                              /* the chord's voice now */

/* ------------------------------------------------------------------ the grid --- */
static float swing;                              /* (never set: the grid straight) */
static float step_len, pulse_left;               /* a sixteenth, in samples; to the next MIDI clock pulse */
static uint32_t cnoise = 4321u;

/* ------------------------------------------------------------------ space --- */
static float echo_send, rev_send, echo_len, echo_len_t, echo_fb = 0.6f, ein_gain = 1.0f, echo_t60 = 6.0f;
static float fb_cap = 2.0f;                     /* stopped, the feedback eases under 90%: the tails die */
static float fx_em = 1.0f, fx_ek = 0.001f, fx_ts = 1.0f;   /* the grid on the echo: its time's multiple, its easing; the tape stop */
static float fx_nova;                            /* (BERLIN) the grid's Supernova: the echo's feedback past full, eased */
static float rev_in_gain = 1.0f;                /* falls to 0 while frozen */
static float tape_lpc = 0.55f, tape_hpc = 0.08f; /* the echo's feedback band-pass */
static float crackle_vol, cloud_amt;

/* ------------------------------------------------------- the arrangement --- */
static uint32_t sixteenth, bar, chord_bars;    /* bars since the start, since the chord moved */
static int pulse;                                /* (the MIDI clock's pulse in the sixteenth, 0..5: 0 the tick's) */
static uint8_t lock;                             /* (PERFORM's Lock: the sequences' and the noise's groove kept) */
static void noise_pattern(void);
static int cr_dbars;                            /* bars till the drifts' next targets (on a barline) */
static void noise_step(int s);
static uint8_t nz_drawn;
static uint8_t change_pending;

/* -------------------------------------------------------------- harmony --- */
static void scale_set(void)
{
    nsteps = en_scale(pool, steps);
}

/* the chord to the four voices: the voicing on the degree, its root moved the least from the last one,
 * in C3..B3 (as a chord memory: the shape kept, transposed whole; 1.3.0 up from F#2..F3, out of the low-mids) */
static int chord_slot = -1;                     /* the slot whose chord sounds (-1: one of the scale) */
/* ------------------------------------------------------------------ the drone --- */
/* (BERLIN) the Drone: the chord held, its four notes and the root an octave under (a pedal), swelling in over Attack
 * and away over Release; a new chord swells in on the other bank while the old one dies. Its voices: Organ
 * (drawbars), Strings (a saw, into the ensemble), Horn (a saw through two fixed formants, an 'ah': was Choir),
 * Saws (two, apart), Glass (FM), Choir (two saws a note through three moving formants, a vowel, into the ensemble:
 * choir(), below), the FM pads (fm.c: Warm Air, FM Strings, Soft Organ); the bus through the phaser. MIDI channel 1: a chord's notes on as it starts, off as it goes */
#define NDR 5
typedef struct {
    uint8_t note;
    float ph, ph2, dt, gl, gr, s[2], f1[2], f2[2], vp;   /* (vp: Choir's slow drift, its phase) */
    fm_v fm;                                     /* (an FM pad's note: fm.c) */
    float fy0, fy1;                              /* (its last two outputs at half the rate: render_drone) */
} drv_t;
static drv_t drv[2][NDR];
static uint8_t dr_bank, dr_on[2];
static uint8_t dr_fmpar;                         /* (the FM pads' half rate: 1 if the next chunk's first sample is
                                                  * between two, kept across chunks of any length) */
static uint8_t ch_live;                          /* (Choir, below) 0: from rest (its states cleared, its vowel where it
                                                  * should be) */
static float dr_lvl[2], dr_up = 0.0001f, dr_down = 0.00005f, ph_depth, ph_lfo, phz[2][6], ph_last[2];
static uint32_t dr_rng = 0x6A09E667u;
static float dr_pedal = 0.7f, dr_rev = 1.0f, dr_echo = 1.0f;   /* (the pedal's level; the sends, x1 at 50) */          /* (its own: the voices' phases leave the music's draws alone) */

static uint8_t dr_midi[2];                        /* the banks whose notes went out on MIDI: their note-offs owed */
static int dr_heard(void) { return !(mutes & EN_M_CHORD) && par[P_CHORD] > 0; }   /* (its MIDI only while heard) */
static void dr_midi_off(int b)                   /* a bank's MIDI notes let go (muted, level 0, or dying away) */
{
    int i;
    if (!dr_midi[b])
        return;
    dr_midi[b] = 0;
    for (i = 0; i < NDR; i++)
        midi(0x80, drv[b][i].note, 0);
}
static void drone_off(int b)                     /* a bank dies away: its notes off */
{
    if (!dr_on[b])
        return;
    dr_on[b] = 0;
    dr_midi_off(b);
}

static void drone_chord(void)                    /* the chord as voiced (stab[].note), swelling in on the other bank */
{
    static const float PAN[NDR] = {-0.45f, 0.4f, -0.15f, 0.2f, 0.0f};
    const int nb = dr_bank ^ 1;
    int i;
    drone_off(dr_bank);
    drone_off(nb);
    for (i = 0; i < NDR; i++) {
        drv_t *v = &drv[nb][i];
        const int n = i < EN_NPAD ? stab[i].note : stab[0].note - 12;   /* (the low root: the pedal) */
        const float p = (PAN[i] + 1.0f) * (FM_PI / 4.0f);
        v->note = (uint8_t)n;
        v->dt = mtof((float)n) * (1.0f / EN_SR);
        v->fy0 = v->fy1 = 0.0f;
        if (cvoice >= CV_FM0)                    /* (an FM pad: its recipe from the top at this note; any other voice:
                                                  * none yet, so turning onto a pad starts one, render_drone) */
            fm_start(&v->fm, CV_FM_REC[cvoice - CV_FM0], n);
        else
            v->fm.rec = FM_NREC;
        v->ph = (float)(seq_rand(&dr_rng) >> 8) * (1.0f / 16777216.0f);
        v->ph2 = (float)(seq_rand(&dr_rng) >> 8) * (1.0f / 16777216.0f);
        v->vp = v->ph;                           /* (Choir's drift from a place of its own: no new draw, so every
                                                  * voice's phases stay as they were) */
        v->gl = fm_cosf(p);
        v->gr = fm_sinf(p);
        if (dr_heard())
            midi(0x90, (uint32_t)n, 70);
    }
    dr_on[nb] = 1;
    dr_midi[nb] = (uint8_t)dr_heard();
    dr_bank = (uint8_t)nb;
}

static void voice_chord(void)
{
    int iv[4], i, pc, n;
    if (chord_slot >= 0 && par[P_CQ0 + chord_slot]) {   /* a slot's chord, as written */
        int r = par[P_CR0 + chord_slot];
        en_chord_iv(par[P_CQ0 + chord_slot], par[P_CX0 + chord_slot], iv);
        for (i = 0; i < 4; i++)
            chord_pcs[i] = (r + iv[i]) % 12;
        {                                        /* (in name order for the bass: the root, its third, its fifth) */
            int t[4];
            en_chord_iv(par[P_CQ0 + chord_slot], EN_CX_7, t);
            chord_pcs[1] = (r + t[3] - 12) % 12;
            chord_pcs[2] = (r + t[1]) % 12;
            chord_pcs[3] = (r + t[2]) % 12;
        }
        chord_nt = 4;
        deg = en_deg_of(pool, r - par[P_ROOT]);
    } else {
        chord_slot = -1;
        chord_nt = en_chord_tones(pool, par[P_ROOT], deg, chord_pcs);
        en_voicing(pool, deg, shape, iv);
    }
    en_chord_slot = (int8_t)chord_slot;
    pc = chord_pcs[0];
    n = root_note + ((pc - root_note) % 12 + 12) % 12;
    if (n - root_note > 6)
        n -= 12;
    while (n < 48)
        n += 12;
    while (n > 59)
        n -= 12;
    root_note = n;
    for (i = 0; i < EN_NPAD; i++) {
        stab[i].note = (uint8_t)(n + iv[i]);
        en_chord[i] = stab[i].note;
    }
    en_chord_root = (uint8_t)chord_pcs[0];
    en_chord_n = (uint8_t)chord_nt;
    if (en_playing)
        drone_chord();
}

/* the chords' slots: how far the progression runs (to its last filled slot; 0: none, or Play at Random), a
 * slot's chord built from its kind and type, and a degree for a note above the root (the pings take the
 * scale's notes around it: nearest in the pool, the lower on a tie) */
volatile int8_t en_prog_pos = -1, en_chord_slot = -1;
volatile int8_t en_prog_slot[EN_NPROG][3];
volatile uint32_t en_prog_gen;
static int slot_gap(int i) { return !par[P_CQ0 + i] && !par[P_CR0 + i]; }
static int slot_walk(int i) { return !par[P_CQ0 + i] && par[P_CR0 + i]; }
static int prog_len(void)                       /* Loop: to the last slot that isn't a gap (0: none) */
{
    int i, n = 0;
    for (i = 0; i < EN_NPROG; i++)
        if (!slot_gap(i))
            n = i + 1;
    return n;
}
static void prog_publish(void)                  /* the slots, for the UI to take */
{
    int i;
    for (i = 0; i < EN_NPROG; i++) {
        en_prog_slot[i][0] = (int8_t)par[P_CR0 + i];
        en_prog_slot[i][1] = (int8_t)par[P_CQ0 + i];
        en_prog_slot[i][2] = (int8_t)par[P_CX0 + i];
    }
    en_prog_gen++;
}
void en_chord_iv(int q, int x, int *iv)
{
    static const int8_t THIRD[EN_NCQ] = {3, 4, 3, 2, 5, 3, 4}, FIFTH[EN_NCQ] = {7, 7, 7, 7, 7, 6, 8};
    int t = THIRD[(unsigned)q < EN_NCQ ? q : 0], f = FIFTH[(unsigned)q < EN_NCQ ? q : 0],
        s = q == EN_CQ_DIM ? 9 : 10, i, j;   /* the seventh: minor (a diminished seventh on Dim) */
    switch ((unsigned)x < EN_NCX ? x : 0) {
    case EN_CX_6: iv[0] = 0, iv[1] = f, iv[2] = 9, iv[3] = t + 12; break;
    case EN_CX_7: iv[0] = 0, iv[1] = f, iv[2] = s, iv[3] = t + 12; break;
    case EN_CX_MAJ7: iv[0] = 0, iv[1] = f, iv[2] = 11, iv[3] = t + 12; break;
    case EN_CX_9: iv[0] = 0, iv[1] = s, iv[2] = t + 12, iv[3] = 14; break;
    case EN_CX_11: iv[0] = 0, iv[1] = s, iv[2] = t + 12, iv[3] = 17; break;
    case EN_CX_ADD9: iv[0] = 0, iv[1] = f, iv[2] = t + 12, iv[3] = 14; break;
    default: iv[0] = 0, iv[1] = f, iv[2] = 12, iv[3] = t + 12; break;   /* the triad, open, its root doubled */
    }
    for (i = 1; i < 4; i++)                      /* the type on the sus's own note (sus2 with a 9, sus4 with an 11): */
        for (j = 0; j < i; j++)                  /* the fifth, an octave up, instead */
            if (iv[i] == iv[j])
                iv[i] = f + 12;
    for (i = 1; i < 4; i++)                      /* ascending, no two alike */
        for (j = i; j > 0 && iv[j] < iv[j - 1]; j--) {
            int y = iv[j];
            iv[j] = iv[j - 1];
            iv[j - 1] = y;
        }
    for (i = 1; i < 4; i++)
        while (iv[i] <= iv[i - 1])
            iv[i] += 12;
}
void en_walk_chord(uint32_t pool, int root, int deg, int *r, int *q, int *x)
{
    int pcs[4], n = en_chord_tones(pool, root, deg, pcs), t, f, s;
    t = (pcs[1] - pcs[0] + 12) % 12;
    f = (pcs[2 < n ? 2 : 0] - pcs[0] + 12) % 12;
    s = n >= 4 ? (pcs[3] - pcs[0] + 12) % 12 : -1;
    *r = pcs[0];
    *q = t == 2 ? EN_CQ_SUS2 : t == 5 ? EN_CQ_SUS4 : t == 3 ? (f == 6 ? EN_CQ_DIM : EN_CQ_MINOR)
       : (f == 8 ? EN_CQ_AUG : EN_CQ_MAJOR);
    *x = s == 10 || (s == 9 && *q == EN_CQ_DIM) ? EN_CX_7 : s == 11 ? EN_CX_MAJ7 : s == 9 ? EN_CX_6 : EN_CX_TRIAD;
}
const char *en_chord_label(int root, int q, int x, char *b)
{
    static const char *const QS[EN_NCQ] = {"", "", "m", "sus2", "sus4", "dim", "aug"};
    static const char *const XS[EN_NCX] = {"", "6", "7", "M7", "9", "11", "add9"};
    const char *r = EN_NOTE_NAME[(unsigned)root < 12u ? root : 0], *qs, *xs;
    int k = 0, sus;
    q = (unsigned)q < EN_NCQ ? q : 0;
    x = (unsigned)x < EN_NCX ? x : 0;
    if (!q) {
        b[0] = '-', b[1] = 0;
        return b;
    }
    qs = QS[q], xs = XS[x], sus = q == EN_CQ_SUS2 || q == EN_CQ_SUS4;
    while (*r)
        b[k++] = *r++;
    if (sus) {                                   /* the type before a sus: "C7sus4" */
        while (*xs)
            b[k++] = *xs++;
        while (*qs)
            b[k++] = *qs++;
    } else {
        while (*qs)
            b[k++] = *qs++;
        while (*xs)
            b[k++] = *xs++;
    }
    b[k] = 0;
    return b;
}
int en_deg_of(uint32_t pl, int rel)
{
    int st[12], m = en_scale(pl, st), i, best = 0, bd = 99;
    rel = ((rel % 12) + 12) % 12;
    for (i = 0; i < m; i++) {
        int d = (st[i] - rel + 12) % 12, e = d > 6 ? 12 - d : d;
        if (e < bd) {
            bd = e;
            best = i;
        }
    }
    return best;
}
static int prog_pos = -1;

/* to another degree: home most of all, then the subdominant, the seventh, the dominant; away from home,
 * home again more often than not. Never a diminished fifth (a half-diminished chord is no dub chord) */
/* the walk: the next degree after d (and before it, d2), in Play's character (0 Home, 1 Wander, 2 Free; Loop: as
 * Home). Away from home, back (Home 65%, Wander 35%, Free 15%); else a weighted choice, never the same degree,
 * never a diminished one: Home leans on the IV, then the VII and the V; Wander spreads wider; Free is even. Wander
 * and Free rarely step straight back to d2 (no ping-pong) */
const char *const EN_WALK_NAME[EN_NWALK] = {"Home", "Wander", "Free"};
#define PLAY_LOOP (par[P_PMODE] & EN_PLOOP)
static int walk_shape;                          /* the walk's voicing, now and then the other (Evolve) */
static int walk_prev;                           /* the degree before the walk's last */
static int walk_from(int d)
{
    static const float W7[3][7] = {{0.0f, 0.4f, 1.0f, 3.0f, 1.5f, 1.5f, 2.0f},
                                   {0.0f, 1.2f, 1.2f, 2.0f, 1.6f, 1.8f, 1.4f},
                                   {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};
    static const float HOME[3] = {0.65f, 0.35f, 0.15f}, BACK[3] = {1.0f, 0.3f, 0.15f};
    const int st = (par[P_PMODE] & 3) < EN_NWALK ? par[P_PMODE] & 3 : EN_WALK_WANDER, d2 = walk_prev;
    float w[12], sum = 0.0f, r;
    int i;
    walk_prev = d;
    if (d != 0 && frand() < HOME[st] * (d2 == 0 && st ? 0.6f : 1.0f)) {   /* (just back from home: less often again) */
        i = 0;
    } else {
        for (i = 0; i < nsteps; i++) {
            int t[3], n = stack(pool, 0, i, t, 3);
            w[i] = i == d || i == 0 ? 0.0f : nsteps == 7 ? W7[st][i] : 1.0f;
            if (i == d2)
                w[i] *= BACK[st];
            if (n >= 3 && (t[2] - t[0] + 12) % 12 == 6)
                w[i] = 0.0f;
            sum += w[i];
        }
        r = frand() * sum;
        for (i = 0; i < nsteps; i++) {
            r -= w[i];
            if (r <= 0.0f && w[i] > 0.0f)
                break;
        }
        if (i >= nsteps)
            i = nsteps > 1 ? 1 : 0;
    }
    if (frand() < evolve * 0.1f)
        walk_shape ^= 1;                         /* now and then, the other voicing */
    return i;
}
static int walk_last;                           /* the degree the walk wrote last (it goes on from there) */
static void slot_walk_set(int k, int d)         /* slot k: the walk's degree d */
{
    par[P_CQ0 + k] = 0;
    par[P_CR0 + k] = (int16_t)(d + 1);
    par[P_CX0 + k] = (int16_t)walk_shape;
    walk_last = d;
}
static void walk_fill(void)                     /* Endless with every slot a gap (a fresh piece): eight from home */
{
    int k, d = 0;
    for (k = 0; k < EN_NPROG; k++)
        if (!slot_gap(k))
            return;
    walk_prev = 0;
    slot_walk_set(0, 0);
    for (k = 1; k < EN_NPROG; k++) {
        d = walk_from(d);
        slot_walk_set(k, d);
    }
    prog_publish();
}
static void slot_play(int k)                    /* slot k sounds: its chord as written, the walk's, or (a gap) held */
{
    if (par[P_CQ0 + k]) {
        chord_slot = k;
        voice_chord();
    } else if (par[P_CR0 + k]) {
        chord_slot = -1;
        deg = (par[P_CR0 + k] - 1) % (nsteps ? nsteps : 1);
        shape = par[P_CX0 + k] & 1;
        voice_chord();
    }
    chord_bars = 0;
}

static void chord_change(void)
{
    int k;
    if (PLAY_LOOP) {                            /* Loop: the slots in turn, a gap holding the chord before it */
        int n = prog_len();
        if (n) {
            prog_pos = (prog_pos + 1) % n;
            en_prog_pos = (int8_t)prog_pos;
            slot_play(prog_pos);
        }
        chord_bars = 0;
        return;
    }
    /* Endless: all up a slot (a gap stays a gap), the walk writes slot 8; slot 1 sounds */
    for (k = 0; k < EN_NPROG - 1; k++) {
        par[P_CR0 + k] = par[P_CR0 + k + 1];
        par[P_CQ0 + k] = par[P_CQ0 + k + 1];
        par[P_CX0 + k] = par[P_CX0 + k + 1];
    }
    slot_walk_set(EN_NPROG - 1, walk_from(walk_last));
    prog_pos = 0;
    en_prog_pos = 0;
    prog_publish();
    slot_play(0);
}

/* ---------------------------------------------------------------- the arc --- */
/* (BERLIN, spec 5; the filter motion's spec 3) how a piece builds: Emerge while a layer's Intro is still opening its
 * filter (the longest Intro's bars), then Flow (the sequences; the Lead now and then), Solo (the Lead in full) and
 * Bridge (every layer's filter takes a move of its own, Bridge's: the filter motion), 16 .. 64 bars each (Motion:
 * shorter; at 0, Flow for ever), a swell into Flow from the Bridge (the echo up, its last two bars). No ending: once it
 * has opened it goes on for ever. Every level is the knobs' (no arc factor on any velocity or gain) */
static uint8_t ar_sec = EN_SEC_FLOW, ar_first;
static int ar_left = 64, ar_bar;
static float ar_pos;                             /* (bars into the Sequence's long rise: the filter motion's clock) */
static float ar_rise = 0.5f;                     /* (the long rise as the Lead's intensity reads it, 0..1: 0.5 with no
                                                  * Arc) */
static uint8_t ar_cue;                           /* (a moment for the Lead's theme: Solo entered, a Bridge begun, the
                                                  * rise past its peak; the Lead takes it) */
static float ar_push, ar_cr_env, ar_cr_f = 5000.0f, ar_cr_g = 1.0f;   /* (the swell; the grid's Take-off) */
static uint32_t ar_rng;
volatile uint8_t en_swell;
#ifdef EN_HOST
volatile uint32_t en_shm_work;
volatile float en_shm_ret = 1.0f;
#endif
static uint32_t s2o_rng;                         /* (Sequence 2's octave move: its own dice, the arc's kept as they were) */
static int ar_len(void) { return par[P_ARC] ? 128 + 640 * par[P_ARC] / 100 : 0; }
static int ar_sec_len(void) { return 16 + (int)(48.0f * (1.0f - motion)); }

/* ------------------------------------------------------- the filter motion --- */
/* (BERLIN, spec 3) each layer's brightness (the Drone's Colour, the Sequence's and Sequence 2's Cutoff, the Lead's
 * Tone, the Atmosphere's Colour) moved by an offset in octaves from its knob, never a value of its own (fo_mul, where
 * the knob's value becomes a frequency: turning the knob moves it all at once). The offset is the sum of the layer's
 * wander (its Move page: a new move every Rate, to a target within +-Width, nearer 0 a little more likely, eased over
 * Time; each layer its own dice), its intro (from closed, -5 octaves, open in Intro bars, slowly first: once, at the
 * piece's start), the Bridge's move (each layer darker or brighter, Bridge's, eased in over its first four bars and out
 * over its last four) and, the Sequence's alone, the long rise (over Arc's bars to a peak at 80% and back, repeating).
 * The Lead's wander and bridge within +-3/4 octave, a third of the others' bridge, its moves never quicker than four
 * bars. The clock: the transport's (or the MIDI clock's) pulses, eased between them; still while stopped */
enum { FO_DRONE, FO_SEQ, FO_LEAD, FO_SEQ2, FO_ATM, NFO };
static const uint8_t FO_W[NFO] = {P_MW0, P_MW1, P_MW2, P_MW3, P_MW4}, FO_T[NFO] = {P_MT0, P_MT1, P_MT2, P_MT3, P_MT4},
                     FO_R[NFO] = {P_MR0, P_MR1, P_MR2, P_MR3, P_MR4}, FO_I[NFO] = {P_MI0, P_MI1, P_MI2, P_MI3, P_MI4};
static const uint32_t FO_SEED[NFO] = {0x5BE0CD19u, 0xCBBB9D5Du, 0x629A292Au, 0x9159015Au, 0x152FECD8u};
#define FO_LEAD_CAP 0.75f
volatile float en_fo[NFO];
volatile int8_t en_fo_dir[NFO];
static float fo[NFO];                            /* the offsets now, as heard (octaves) */
static float fo_v[NFO], fo_a[NFO], fo_b[NFO], fo_p[NFO] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f}, fo_next[NFO];   /* the wander,
                                                  * -1..1 of Width: where it is, its move from and to, how far along it
                                                  * (1: arrived), bars to the next */
static float fo_bs[NFO], fo_be;                  /* the Bridge's move by layer (-1..1 of Bridge), how far in (0..1) */
static float fo_ib;                              /* bars since the piece started (the intros) */
static int16_t fo_in[NFO];                       /* the intros as they were at the start (turned down: sooner) */
static uint32_t mvm;                             /* (BERLIN) the black keys' moves held: EN_MV_* */
static float mv_rise, mv_close;                  /* Rise and Close, eased (0..1) */
static uint32_t fo_rng[NFO], fo_pulses, fo_pp;   /* (the clock: pulses so far, the last counted, how far through the */
static float fo_f = 1.0f;                        /* pulse after it) */
static float fo_u(int l) { return (float)(seq_rand(&fo_rng[l]) >> 8) * (1.0f / 16777216.0f); }
static float fo_ease(float x) { return x * x * (3.0f - 2.0f * x); }
static float fo_mul(int l)                       /* the factor on the knob's frequency (exactly 1 at no offset) */
{
    const float o = fo[l];
    return o == 0.0f ? 1.0f : fm_exp2f(fm_clampf(o, -6.0f, 6.0f));
}
static int fo_intro_max(void)
{
    int l, m = 0;
    for (l = 0; l < NFO; l++)
        m = pe(FO_I[l]) > m ? pe(FO_I[l]) : m;
    return m;
}
static void fo_bridge(void)                      /* a Bridge begins: each layer's move, darker or brighter */
{
    int l;
    for (l = 0; l < NFO; l++) {
        const float s = fo_u(l) < 0.5f ? -1.0f : 1.0f;
        fo_bs[l] = s * (0.6f + 0.4f * fo_u(l));
    }
}
/* (the user, 2026-10-28) the master actions move by themselves, Motion's: Filter a slow sine between Shut 30 and Open
 * 30 (as its knob shows them: +-15 of its -50..50), 64 bars round, opening first; Wet from 0 up to +50 (25) and back,
 * 48 bars round; Energy with the arc's long rise, 0 at its start, +50 (25) at its peak, 0 with no Arc. Each by
 * Motion/100. The hand (a knob, or SELECT on HOME for Energy) holds its value till the next section change; then the
 * motion takes over again from it, the difference gone over eight bars (no jump); Lock holds all three where they are.
 * A piece, or the transport from the top, from their start (ar_start); once a chunk, a call to macro() only when a
 * value moves a whole step */
#define AM_N 3                                   /* (Filter, Wet, Energy: EN_MAC_*'s first three) */
#define AM_GLIDE 8.0f                            /* (bars: the hand's value back to the motion's) */
static float am_ph[2];                           /* bars into Filter's and Wet's rounds */
static float am_off[AM_N], am_rate[AM_N];        /* the glide: where it is less the motion, shrinking at rate a bar */
static uint8_t am_held, am_stuck;                /* the knobs the hand holds; held (or locked) the chunk before */
static void macro(int i, int v);
static void am_step(float d, float rise)        /* rise: the arc's long rise, 0..1 (0: no Arc) */
{
    static const float DEPTH[AM_N] = {15.0f, 25.0f, 25.0f}, ROUND[2] = {64.0f, 48.0f};
    float t[AM_N];
    int i;
    for (i = 0; i < 2; i++)
        if ((am_ph[i] += d) >= ROUND[i])
            am_ph[i] -= ROUND[i];
    t[0] = fm_sinf(am_ph[0] * (FM_TWO_PI / 64.0f));
    t[1] = 0.5f - 0.5f * fm_cosf(am_ph[1] * (FM_TWO_PI / 48.0f));
    t[2] = rise;
    for (i = 0; i < AM_N; i++) {
        float v;
        int r;
        t[i] *= DEPTH[i] * motion;
        if ((am_held >> i & 1u) || lock) {
            am_stuck |= (uint8_t)(1u << i);
            continue;
        }
        if (am_stuck >> i & 1u) {               /* let go: on from where it is */
            am_stuck &= (uint8_t)~(1u << i);
            am_off[i] = (float)mac[i] - t[i];
            am_rate[i] = fm_fabsf(am_off[i]) * (1.0f / AM_GLIDE);
        }
        am_off[i] = am_off[i] > 0.0f ? fm_maxf(am_off[i] - am_rate[i] * d, 0.0f) : fm_minf(am_off[i] + am_rate[i] * d, 0.0f);
        v = fm_clampf(t[i] + am_off[i], -50.0f, 50.0f);
        r = (int)(v + (v < 0.0f ? -0.5f : 0.5f));
        if (r != mac[i])
            macro(i, r);
    }
}
static void fo_step(uint32_t n)                  /* once a chunk: the clock on, each layer's offset */
{
    float d = 0.0f, rise = 0.0f, be;
    int l;
    if (fo_pulses != fo_pp) {                    /* pulses since: to the start of the last one */
        d = (float)(fo_pulses - fo_pp) - fo_f;
        fo_pp = fo_pulses;
        fo_f = 0.0f;
    }
    if (en_playing && step_len > 0.0f) {         /* this chunk: on through the pulse, never past its end */
        const float a = fm_minf((float)n * 6.0f / step_len, 1.0f - fo_f);
        d += a;
        fo_f += a;
    }
    d = fm_maxf(d, 0.0f) * (1.0f / 96.0f);       /* (bars) */
    if (fo_ib < 4096.0f)
        fo_ib += d;
    if (ar_len()) {                              /* the Sequence's long rise: up to a peak at 80%, back, again */
        const float L = (float)ar_len(), was = ar_pos;
        float x;
        ar_pos += d;
        while (ar_pos >= L)
            ar_pos -= L;
        x = ar_pos / L;
        rise = 2.0f * (x < 0.8f ? x * 1.25f : (1.0f - x) * 5.0f) - 1.0f;
        if (was < 0.8f * L && ar_pos >= 0.8f * L)   /* (past its peak: the Lead's theme, spec 3.2) */
            ar_cue = 1;
    }
    am_step(d, ar_len() ? 0.5f * (rise + 1.0f) : 0.0f);
    ar_rise = ar_len() ? 0.5f * (rise + 1.0f) : 0.5f;
    be = ar_sec == EN_SEC_BRIDGE && ar_left > 4 ? 1.0f : 0.0f;   /* the Bridge: in over four bars, out over its last */
    fo_be = be > fo_be ? fm_minf(fo_be + 0.25f * d, 1.0f) : fm_maxf(fo_be - 0.25f * d, 0.0f);
    be = fo_ease(fo_be) * 3.0f * (float)pe(P_BRIDGE) * 0.01f;
    {   /* the black keys' Rise and Close: in over about a second, out over about three */
        const float up = (float)n / EN_SR, dn = (float)n / (3.0f * EN_SR);
        mv_rise = mvm & EN_MV_RISE ? fm_minf(mv_rise + up, 1.0f) : fm_maxf(mv_rise - dn, 0.0f);
        mv_close = mvm & EN_MV_CLOSE ? fm_minf(mv_close + up, 1.0f) : fm_maxf(mv_close - dn, 0.0f);
    }
    for (l = 0; l < NFO; l++) {
        const int lead = l == FO_LEAD, r = pe(FO_R[l]), in = pe(FO_I[l]) < fo_in[l] ? pe(FO_I[l]) : fo_in[l];
        const float t = fm_maxf(fm_exp2f((float)pe(FO_T[l]) * 0.05f), lead ? 4.0f : 1.0f);   /* 1 .. 32 bars */
        float o;
        if (d > 0.0f) {
            if (fo_p[l] < 1.0f)
                fo_p[l] = fm_minf(fo_p[l] + d / t, 1.0f);
            if (r > 0 && (fo_next[l] = fm_minf(fo_next[l], 2.5f * fm_exp2f((float)(100 - r) * 0.05f)) - d) <= 0.0f) {
                /* a new move, from where it is (the wait never longer than the Rate now allows: turned up, soon) */
                const float u = 2.0f * fo_u(l) - 1.0f;
                fo_a[l] = fo_v[l];
                fo_b[l] = u * (0.6f + 0.4f * fm_fabsf(u));   /* (nearer 0 a little more likely) */
                fo_p[l] = 0.0f;
                fo_next[l] = 2.0f * fm_exp2f((float)(100 - r) * 0.05f) * (0.75f + 0.5f * fo_u(l));   /* ~2 .. 64 bars */
            }
        }
        fo_v[l] = fo_a[l] + (fo_b[l] - fo_a[l]) * fo_ease(fo_p[l]);
        o = (lead ? FO_LEAD_CAP : 2.0f) * (float)pe(FO_W[l]) * 0.01f * fo_v[l] + (lead ? be * (1.0f / 3.0f) : be) * fo_bs[l] +
            2.0f * fo_ease(mv_rise) - 4.0f * fo_ease(mv_close);   /* (the black keys' Rise and Close) */
        if (lead)
            o = fm_clampf(o, -FO_LEAD_CAP, FO_LEAD_CAP);
        if (l == FO_SEQ)
            o += rise;
        if (in > 0 && fo_ib < (float)in) {       /* the intro: closed, opening slowly first */
            const float x = fo_ib / (float)in;
            o -= 5.0f * (1.0f - x * x);
        }
        if (d > 0.0f) {
            const float v = (o - fo[l]) / d;     /* (octaves a bar) */
            en_fo_dir[l] = (int8_t)(v > 0.02f ? 1 : v < -0.02f ? -1 : 0);
        } else if (!en_playing) {
            en_fo_dir[l] = 0;
        }
        fo[l] = o;
        en_fo[l] = o;
    }
}

static void ar_start(void)                       /* the arc from its start: Emerge (an Intro opening), or Flow at once;
                                                  * the intros and the long rise from their start */
{
    int i;
    ar_bar = 0;
    ar_pos = 0.0f;
    ar_cue = 0;
    fo_ib = 0.0f;
    for (i = 0; i < NFO; i++)
        fo_in[i] = (int16_t)pe(FO_I[i]);
    en_swell = 0;
    ar_push = 0.0f;
    en_s2oct_shift = 0;                          /* (Sequence 2 at its home octave) */
    am_ph[0] = am_ph[1] = 0.0f;                  /* (the master actions' motion from its start, on from where they are) */
    am_stuck = (1u << AM_N) - 1u;
    if (fo_intro_max()) {
        ar_sec = EN_SEC_EMERGE;
        ar_left = fo_intro_max();
    } else {
        ar_sec = EN_SEC_FLOW;
        ar_left = ar_sec_len();
    }
    en_section = ar_sec;
}
/* (the user, 2026-10-28) Sequence 2's octave, Motion's: at a section change, shifted, home; at home, with Motion/100,
 * one octave away, up or down (the other way at the range's edge, -2..1): never two away. Lock keeps it as it is;
 * the hand (its Octave set, apply), a new piece (ar_start) and Motion 0 bring it home. The knob keeps home: saved */
static void s2o_section(void)
{
    const int h = par[P_SEQ2OCT];
    int d;
    if (lock)
        return;
    if (en_s2oct_shift) {
        en_s2oct_shift = 0;
        return;
    }
    if (motion <= 0.005f || (float)(seq_rand(&s2o_rng) >> 8) * (1.0f / 16777216.0f) >= motion)
        return;
    d = seq_rand(&s2o_rng) & 1u ? 1 : -1;
    if (h + d > PARAMS[P_SEQ2OCT].hi || h + d < PARAMS[P_SEQ2OCT].lo)
        d = -d;
    en_s2oct_shift = (int8_t)d;
}
static void ar_tick_bar(void)                    /* a bar line (the first, at the start, already counted) */
{
    if (ar_first) {
        ar_first = 0;
        return;
    }
    ar_bar++;
    if (--ar_left <= 0) {
        const float r = (float)(seq_rand(&ar_rng) >> 8) * (1.0f / 16777216.0f);
        const uint8_t was = ar_sec;
        switch (ar_sec) {
        case EN_SEC_EMERGE: ar_sec = EN_SEC_FLOW; break;
        case EN_SEC_FLOW:
            if (motion > 0.005f)
                ar_sec = r < 0.6f ? EN_SEC_SOLO : EN_SEC_BRIDGE;
            break;
        case EN_SEC_SOLO: ar_sec = r < 0.7f ? EN_SEC_FLOW : EN_SEC_BRIDGE; break;
        default: ar_sec = EN_SEC_FLOW; break;
        }
        ar_left = ar_sec_len();
        if (ar_sec == EN_SEC_BRIDGE)
            fo_bridge();
        if (ar_sec != was) {
            s2o_section();
            am_held = 0;                         /* (the hand lets go of the master actions) */
            if (ar_sec == EN_SEC_SOLO || ar_sec == EN_SEC_BRIDGE)
                ar_cue = 1;                      /* (the Lead's theme back: Solo entered, a Bridge begun) */
        }
    }
    en_swell = ar_sec == EN_SEC_BRIDGE && ar_left <= 2;
    en_section = ar_sec;
    ar_push = en_swell ? 0.5f : 0.0f;
}

/* ------------------------------------------------------------- the sequences --- */
/* (BERLIN) two step sequences (seq.c: the pattern, its performer, its pitch, the repeats' history), each note a
 * pluck (render_plucks): the Sequence (layer 0, MIDI channel 2) with its two repeats, played again as notes
 * (Repeat A, Repeat B: only into a rest), and Sequence 2 (layer 1, channel 4). A pattern grown here is thinned by
 * Density as it plays; one written (en_seq_set) plays as written */
#define NPLUCK 6
typedef struct {
    uint8_t on, note, layer, voice;
    float ph, ph2, dt, dt2, amp, att, adec, env, edec, gl, gr, cut, c1[4], c2[4], s1[2], s2[2];
    float last;                                  /* (its last output sample, before the pan: a steal's step) */
    uint16_t ks_w, ks_r;                         /* (Pluck: its string, ks_line[its slot]) where it writes, where it
                                                  * reads (the delay's whole samples behind) */
    float ks_fr, ks_fb, ks_a1, ks_x1, ks_g;      /* the delay's fraction; the loop's gain a period; the last sample
                                                  * read and the last read between two (the average's); the output's
                                                  * gain (the velocity's, from the line's units) */
    fm_v fm;                                     /* (the voices batch, item 5) an FM voice's note (fm.c) */
} pluck_t;
static pluck_t pluck[NPLUCK];

/* (the voices batch, item 2) Pluck: a plucked string, Karplus-Strong. After fm1_open's third_party/synth/pluck.c
 * (M-VAVE's FM1 adaptation of DaisySP's String; MIT licence, notice below), written again for this engine: a burst of
 * noise a period long in a delay line, read back a period later (between two samples: the fraction) through the
 * classic two-sample average (a gentle low-pass whose delay is half a sample at every frequency, so the pitch stays
 * true up to the top: pluck.c's one-pole there drifted the high notes sharp) and the loop's gain, written in again.
 * One knob, one job: Cutoff (with its motion, PERFORM's Sweep, the grid's Sweep) is the burst's brightness, at the
 * strike (a one-pole on the noise, ~3.5 octaves over the cutoff, where the other voices' filters start); Decay how
 * long it rings: the loop's gain gives the fundamental the other voices' amplitude T60 (1.5 x 80 .. 250 ms), the
 * average's and the fraction's loss made up; Accent, the velocity, its level. Each voice's line, int16 (units of
 * 1/KS_Q: the burst +-1 at most, +-2 with its mean taken out), is long enough for the lowest note the sequences
 * reach: 24 (seq_note's clamp; the Octave cell only lifts) at Tune -50, 31.77 Hz: 1388 samples a period, read up to
 * 1389 back. The noise has its own dice: Pluck changes the sound, never what the piece plays.
 *
 * pluck.c: Copyright (c) 2020 Electrosmith, Corp, Emilie Gillet; 2026 M-VAVE. MIT licence:
 *   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 *   documentation files (the "Software"), to deal in the Software without restriction, including without limitation
 *   the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
 *   to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *   The above copyright notice and this permission notice shall be included in all copies or substantial portions of
 *   the Software.
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 *   THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 *   CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *   IN THE SOFTWARE. */
#define KS_N 1400                                /* (a line: 1389 needed, above) */
#define KS_Q 16383.0f                            /* (the line's units a 1) */
#define KS_GAIN 1.78f                            /* (its level: a note's energy as 2600's at the same knobs, +-1.5 dB
                                                  * from 24 to 72 (the tests); the higher, the sooner its partials die,
                                                  * so ~2 dB an octave more over 48, to 84) */
static int16_t ks_line[NPLUCK][KS_N] EN_POOL;
static uint32_t ks_rng = 0x6C078965u;
static seq_t sq[2], sq_pend[2];
static seq_hist_t sq_hist[2];
static int sq_pos[2];
static uint8_t sq_wrap[2], sq_bars[2], sq_written[2], sq_prio[2][SEQ_MAX];
static uint32_t sq_rng = 0x2545F491u;
static float seq_vol, seq2_vol, sq_cut = 600.0f, sq_edec = 0.9995f, sq_adec = 0.9997f;
static float sq2_cut = 450.0f, sq2_edec = 0.9995f, sq2_adec = 0.9997f, sq_rev[2] = {1.0f, 1.0f}, sq_echo[2] = {1.0f, 1.0f};
volatile float en_pluck_cut[2];                  /* (each sequence's last note's cutoff, for the tests) */
volatile uint8_t en_seq_len[2];
volatile int8_t en_s2oct_shift;
volatile uint16_t en_seq_lane[5], en_seq_acc[2];   /* (the Pattern page's lanes as Auto plays them: each sequence's
                                                  * steps (as Density thins them), octaves; the Sequence's skips; accents) */
const char *const EN_SV_NAME[EN_NS2V] = {"2600", "Moog", "Synthi", "Pulse", "Glass", "Sub", "Pluck", "Neon Keys",
    "OrbitBell", "Soft EP", "Dry Clav", "FM Guitar", "Marimba", "Vibes", "Metal Hit", "RoundBass", "Sub Bass"};
/* (the voices batch, item 5) the sequences' FM voices, SV_FM0 on: fm.c's recipes, in the user's order (2026-10-30:
 * "plucked for arps", and the two basses); their level against the other voices', one constant, the user's to set
 * by ear */
static const uint8_t SV_FM_REC[EN_NS2V - SV_FM0] = {FM_NEON_KEYS, FM_ORBIT_BELL, FM_SOFT_EP, FM_DRY_CLAV, FM_GUITAR,
                                                     FM_MARIMBA, FM_VIBES, FM_METAL_HIT, FM_ROUND_BASS, FM_SUB_BASS};
#define SV_FM_G 1.0f

/* a piece's patterns, kept (en_seq_get, en_seq: EN_SEQ_N bytes): each one's degrees (16 + 16), steps, octaves and
 * accents (two bytes each, low first) and Density's order (16 + 16) */
static uint8_t seq_pend_b[EN_SEQ_N], seq_have;
void en_seq_get(uint8_t *b)
{
    int l, i;
    for (i = 0; i < EN_SEQ_N; i++)
        b[i] = 0;
    for (l = 0; l < 2; l++) {
        for (i = 0; i < SEQ_MAX; i++) {
            b[16 * l + i] = (uint8_t)sq[l].deg[i];
            b[44 + 16 * l + i] = sq_prio[l][i];
        }
        b[32 + 2 * l] = (uint8_t)(sq[l].gate & 255u), b[33 + 2 * l] = (uint8_t)(sq[l].gate >> 8);
        b[36 + 2 * l] = (uint8_t)(sq[l].oct & 255u), b[37 + 2 * l] = (uint8_t)(sq[l].oct >> 8);
        b[40 + 2 * l] = (uint8_t)(sq[l].acc & 255u), b[41 + 2 * l] = (uint8_t)(sq[l].acc >> 8);
    }
}
void en_seq(const uint8_t *b)
{
    int i;
    for (i = 0; b && i < EN_SEQ_N; i++)
        seq_pend_b[i] = b[i];
    post(C_SEQ, b ? 1 : 0, 0);
}

void en_seq_set(int l, const seq_t *p)
{
    sq_pend[l & 1] = *p;
    post(C_SEQSET, l & 1, 0);
}

static int sq_audible(int l)
{
    if (perf & EN_PF_DROP)                       /* (PERFORM's Drop: both sequences out) */
        return 0;
    return l ? seq2_vol > 0.0f && !(mutes & (EN_M_KICK | EN_M_HATS)) : seq_vol > 0.0f && !(mutes & EN_M_PINGS);
}

#ifdef EN_HOST
volatile uint32_t en_ks_struck;
volatile float en_pl_steal;
static int ks_fine(float x) { return x == x && fm_fabsf(x) < 1e30f && (x == 0.0f || fm_fabsf(x) >= 1.17549435e-38f); }
int en_ks_ok(void)                               /* (host tests) the strings' float state: finite, no denormals */
{
    int i, ok = 1;
    for (i = 0; i < NPLUCK; i++) {
        const pluck_t *v = &pluck[i];
        ok &= ks_fine(v->ks_fr) & ks_fine(v->ks_fb) & ks_fine(v->ks_a1) & ks_fine(v->ks_x1) & ks_fine(v->ks_g) &
              ks_fine(v->amp) & ks_fine(v->att) & ks_fine(v->last);
    }
    return ok;
}
int en_pl_on(void)                               /* (host tests) the sequence voices sounding */
{
    int i, n = 0;
    for (i = 0; i < NPLUCK; i++)
        n += pluck[i].on;
    return n;
}
#endif

static float fx_swm;                             /* (the grid's Sweep, below: the burst's brightness moves with it) */
/* (Pluck) v struck (pluck_voice: its note, level, length and cutoff set): its line filled with the burst, a period and
 * two (what the first reads, the fraction's neighbour and the average's), its mean taken out (no thump); the delay
 * and the loop's gain from the period */
static void ks_strike(pluck_t *v, int16_t *line)
{
    const float p = 1.0f / v->dt, d = fm_clampf(p - 0.5f, 2.0f, (float)(KS_N - 3)), w = FM_TWO_PI * v->dt,
                b = 1.0f - fm_expf(-FM_TWO_PI * fm_minf(v->cut * 11.3137085f * fx_swm, 0.45f * EN_SR) * (1.0f / EN_SR));
    const int len = (int)d, nb = len + 2;
    const float fr = d - (float)len;
    float lp = 0.0f, loss;
    int32_t sum = 0, mean;
    int i;
    for (i = 0; i < nb; i++) {                   /* (white noise, -1 .. 1, through the one-pole) */
        int k;
        ks_rng = ks_rng * 1664525u + 1013904223u;
        lp += b * ((float)(int32_t)ks_rng * (1.0f / 2147483648.0f) - lp);
        k = (int)(lp * KS_Q);
        line[KS_N - 1 - i] = (int16_t)k;
        sum += k;
    }
    mean = sum / nb;
    for (i = 0; i < nb; i++)
        line[KS_N - 1 - i] = (int16_t)(line[KS_N - 1 - i] - mean);
    v->ks_fr = fr;
    v->ks_w = 0;
    v->ks_r = (uint16_t)(KS_N - len);
    v->ks_a1 = (float)line[KS_N - len - 1];
    v->ks_x1 = v->ks_a1 + fr * ((float)line[KS_N - len - 2] - v->ks_a1);
    /* the loop's gain: the amplitude's decay over a period (Decay's T60), over what the average (cos w/2) and the
     * fraction's line take from the fundamental each time round; under 1, whatever (the top notes, a long Decay) */
    loss = fm_cosf(0.5f * w) * fm_sqrtf(1.0f - 2.0f * fr * (1.0f - fr) * (1.0f - fm_cosf(w)));
    v->ks_fb = fm_minf(fm_expf(p * fm_logf(v->adec)) / loss, 0.9995f);
    v->ks_g = v->amp * (KS_GAIN / KS_Q) * fm_exp2f(0.34f * fm_minf((float)v->note - 48.0f, 36.0f) * (1.0f / 12.0f));
#ifdef EN_HOST
    en_ks_struck++;
#endif
}

static void pluck_voice(int l, int n, int vel, int rep)   /* rep: a repeat, panned across from the note */
{
    int i, best = 0;
    float lo = 9.0f, p;
    pluck_t *v;
    if (!sq_audible(l) || vel <= 0)
        return;
    for (i = 0; i < NPLUCK; i++) {               /* a free voice, or the quietest */
        float a = pluck[i].on ? pluck[i].amp : -1.0f;
        if (a < lo)
            lo = a, best = i;
    }
    v = &pluck[best];
    if (v->on) {
        midi(v->layer ? 0x83u : 0x81u, v->note, 0);
#ifdef EN_HOST
        if (fm_fabsf(v->last) > en_pl_steal)
            en_pl_steal = fm_fabsf(v->last);
#endif
    }
    v->on = 1;
    v->note = (uint8_t)n;
    v->layer = (uint8_t)l;
    v->voice = (uint8_t)sv_of(l, par[l ? P_S2VOICE : P_SVOICE]);
    v->dt = mtof((float)n) * (1.0f / EN_SR);
    v->dt2 = v->dt * (v->voice == 1 ? 1.004f : v->voice == 4 ? 3.5f : 0.5f);   /* Moog's second saw, Glass's
                                                                                * modulator, Sub's octave under */
    v->ph = v->ph2 = 0.0f;
    if (v->voice >= SV_FM0)                      /* (an FM voice: its recipe from the top, at this note) */
        fm_start(&v->fm, SV_FM_REC[v->voice - SV_FM0], n);
    v->amp = (float)vel * (1.0f / 127.0f);
    v->att = 0.0f;
    v->env = 1.0f;
    v->adec = l ? sq2_adec : sq_adec;
    v->edec = l ? sq2_edec : sq_edec;
    v->cut = l ? sq2_cut * fo_mul(FO_SEQ2) : sq_cut * fo_mul(FO_SEQ) * (perf & EN_PF_SWEEP ? 4.0f : 1.0f);   /* (the knob's,
                                                  * moved by the filter motion; PERFORM's Sweep held: the Sequence's two
                                                  * octaves open) */
    en_pluck_cut[l] = v->cut;
    v->s1[0] = v->s1[1] = v->s2[0] = v->s2[1] = 0.0f;
    v->last = 0.0f;
    if (v->voice == SV_PLUCK)
        ks_strike(v, ks_line[best]);
    p = ((l ? 0.0f : rep ? 0.35f : -0.25f) + 1.0f) * (FM_PI / 4.0f);
    v->gl = fm_cosf(p);
    v->gr = fm_sinf(p);
    en_loop_glow[l] = 1.0f;
    hit(1, v->amp);
    if (rep)
        en_seq_reps[l]++;
    else
        en_seq_hits[l]++;
    midi(l ? 0x93u : 0x91u, (uint32_t)n, (uint32_t)vel);
}

static void seq_publish(void)                    /* the lanes as Auto plays them, for the Pattern page */
{
    int l, i;
    for (l = 0; l < 2; l++) {
        uint16_t g = 0;
        for (i = 0; i < SEQ_MAX && i < sq[l].len; i++)
            if ((sq[l].gate >> i & 1u) && (sq_written[l] || sq_prio[l][i] < 25 + 3 * pe(P_DENSITY) / 4))
                g |= (uint16_t)(1u << i);
        en_seq_lane[l] = g;
        en_seq_lane[2 + l] = sq[l].oct;
        en_seq_acc[l] = sq[l].acc;
        en_seq_len[l] = sq[l].len;
    }
    en_seq_lane[4] = sq[0].skip;
}

static uint8_t sq_grown[2];                     /* (the steps a pattern has had grown: Length back over them keeps them) */
static void seq_regrow(int l)                    /* a pattern grown afresh: every step, Density thins it as it plays */
{
    int i;
    seq_grow(&sq[l], &sq_rng, nsteps ? nsteps : 7, 100, l ? par[P_SEQ2LEN] : par[P_SEQLEN]);
    sq_grown[l] = sq[l].len;
    for (i = 0; i < SEQ_MAX; i++)
        sq_prio[l][i] = (uint8_t)(seq_rand(&sq_rng) % 100u);
    sq_prio[l][0] = 0;                           /* (the root, always) */
    sq_written[l] = 0;
    sq_pos[l] = 0;
    sq_bars[l] = sq_wrap[l] = 0;
    seq_hist_clear(&sq_hist[l]);
    seq_publish();
}

/* (BERLIN) Length turned: the return point moved, the notes kept (the review: it grew all afresh); steps past any
 * grown so far grown now, from a pattern of the new length, and the rest as they were */
static void seq_resize(int l, int len)
{
    seq_t *q = &sq[l], t;
    int i;
    if (len < 1)
        len = 1;
    if (len > SEQ_MAX)
        len = SEQ_MAX;
    if (len > sq_grown[l]) {
        seq_grow(&t, &sq_rng, nsteps ? nsteps : 7, 100, len);
        for (i = sq_grown[l]; i < len; i++) {
            const uint16_t m = (uint16_t)(1u << i);
            q->deg[i] = t.deg[i];
            q->gate = (uint16_t)((q->gate & ~m) | (t.gate & m));
            q->oct = (uint16_t)((q->oct & ~m) | (t.oct & m));
            q->acc = (uint16_t)((q->acc & ~m) | (t.acc & m));
            sq_prio[l][i] = (uint8_t)(seq_rand(&sq_rng) % 100u);
        }
        sq_grown[l] = (uint8_t)len;
    }
    q->len = q->base_len = (uint8_t)len;
    if (sq_pos[l] >= len)
        sq_pos[l] = 0;
    seq_publish();
}

static void seq_take(void)                      /* a piece's kept patterns, back (its lengths as it has them) */
{
    int l, i;
    for (l = 0; l < 2; l++) {
        seq_t *q = &sq[l];
        for (i = 0; i < SEQ_MAX; i++) {
            q->deg[i] = (int8_t)seq_pend_b[16 * l + i];
            sq_prio[l][i] = seq_pend_b[44 + 16 * l + i];
        }
        q->gate = (uint16_t)(seq_pend_b[32 + 2 * l] | seq_pend_b[33 + 2 * l] << 8);
        q->oct = (uint16_t)(seq_pend_b[36 + 2 * l] | seq_pend_b[37 + 2 * l] << 8);
        q->acc = (uint16_t)(seq_pend_b[40 + 2 * l] | seq_pend_b[41 + 2 * l] << 8);
        q->skip = 0;
        q->len = q->base_len = (uint8_t)par[l ? P_SEQ2LEN : P_SEQLEN];
        sq_grown[l] = q->len;                    /* (a kept pattern: its own steps, to its last note past its length) */
        for (i = q->len; i < SEQ_MAX; i++)
            if (q->gate >> i & 1u)
                sq_grown[l] = (uint8_t)(i + 1);
        sq_written[l] = 0;
        sq_pos[l] = 0;
        sq_bars[l] = sq_wrap[l] = 0;
        seq_hist_clear(&sq_hist[l]);
    }
}

static int seq_note(const seq_t *q, int l, int s)   /* step s's note, transposed by the chord as the piece says */
{
    seq_t t;
    const int root = par[P_ROOT] % 12, tr = par[P_TRANS],
              off = tr == 2 ? 0 : ((int)en_chord_root % 12 - root + 12) % 12,
              base = 48 + root + 12 * (l ? par[P_SEQ2OCT] + en_s2oct_shift : par[P_SEQOCT]);
    int n;
    if (mvm & EN_MV_TRANSPOSE) {                 /* (BERLIN) the black keys' Transpose: three scale steps up */
        t = *q;
        t.deg[s] = (int8_t)(t.deg[s] + 3);
        q = &t;
    }
    n = seq_pitch(q, s, steps, nsteps, base, tr == 0, off);
    return n < 24 ? 24 : n > 108 ? 108 : n;
}

static void seq_tick(void)                       /* a sixteenth: the sequences' steps, the repeats */
{
    static const uint8_t DT[6] = {2, 3, 4, 6, 8, 16};   /* the repeats' times (the echo's), in sixteenths */
    const int on_grid = par[P_SEQGRID] || !(sixteenth & 1u);
    int l;
    if (!(sixteenth & 15u)) {
        ar_tick_bar();
        seq_publish();
    }
    if (!(sixteenth & 15u))                      /* a bar line: the performer, when a pass ended on it (at most every
                                                  * 4 bars), or every 16 */
        for (l = 0; l < 2; l++)
            if (++sq_bars[l] >= 16u || (sq_wrap[l] && sq_bars[l] >= 4u)) {
                if (!lock && !sq_written[l] && !par[P_SMODE])
                    seq_vary(&sq[l], &sq_rng, nsteps ? nsteps : 7, pe(P_MOTION), pe(P_EVOLVE), !l);   /* (Sequence
                                                  * 2's length stays the knob's: the user, 2026-10-28) */
                sq_bars[l] = 0;
            }
    for (l = 0; l < 2; l++) {
        int played = -1, vel = 0, n, v;
        if (on_grid) {
            seq_t q = sq[l];                     /* (Pattern: the lanes' steps, octaves, skips, accents, its length) */
            const int pat = par[P_SMODE] != 0, ch = par[l ? P_SCH1 : P_SCH0];
            int st;
            if (pat) {
                q.gate = (uint16_t)par[l ? P_SGATE1 : P_SGATE0];
                q.oct = (uint16_t)par[l ? P_SOCT1 : P_SOCT0];
                q.acc = (uint16_t)par[l ? P_SACC1 : P_SACC0];
                q.skip = l ? 0u : (uint16_t)par[P_SSKIP0];
                q.len = q.base_len = (uint8_t)par[l ? P_SEQ2LEN : P_SEQLEN];
            }
            st = sq_pos[l] < q.len ? sq_pos[l] : 0;
            if ((q.gate >> st & 1u) && !(q.skip >> st & 1u) &&
                (sq_written[l] || pat || sq_prio[l][st] < 25 + 3 * pe(P_DENSITY) / 4) && (ch >= 100 || irand(100) < ch)) {
                played = seq_note(&q, l, st) + (!l && fxm >> EN_FX_OCTAVE & 1u ? 12 : 0);   /* (the grid's Octave) */
                vel = q.acc >> st & 1u ? 120 : 120 - 86 * par[P_SACCENT] / 100;   /* the root accented (Accent 50: 77,
                                                                               * as Synth Seeker's 121 and 77); nothing
                                                                               * more (no hidden level: spec 3) */
                pluck_voice(l, played, vel, 0);
            }
            sq_pos[l] = seq_next(&q, st);
            if (fxm >> EN_FX_RETURN & 1u && sq_pos[l] >= 4)   /* (the grid's Return: round its first four steps) */
                sq_pos[l] = 0;
            sq_wrap[l] = (uint8_t)(sq_pos[l] <= st);
            en_loop_pos[l] = (uint8_t)st;
        }
        if (!l) {                                /* the repeats: A always, B only into a rest */
            if (pe(P_TAPA) && (n = seq_tap(&sq_hist[0], DT[par[P_TAPAT] % 6], &v)) >= 0)
                pluck_voice(0, n, v * pe(P_TAPA) / 100, 1);   /* (pe: PERFORM's Energy moves them too) */
            if (pe(P_TAPB) && (!on_grid || played < 0) && (n = seq_tap(&sq_hist[0], DT[par[P_TAPBT] % 6], &v)) >= 0)
                pluck_voice(0, n, v * pe(P_TAPB) / 100, 1);
        }
        seq_hist_push(&sq_hist[l], played, vel);
    }
}

/* ------------------------------------------------------------------ the lead --- */
/* (BERLIN) the Lead: one voice, gliding, its vibrato coming in late; it plays its motifs in pairs (below), never in
 * Emerge, a breath between pairs (Phrases: how often). Voices: Odyssey (a saw and a pulse), Mini (three saws), Flute
 * (a sine and breath), Theremin (a sine, always wavering), Synthi (a square), the FM voices (fm.c: Soft Lead, Wood
 * Reed, Soft Brass). MIDI channel 3 */
static struct {
    uint8_t on, note;                            /* sounding (its gate); the note it glides to */
    float cur, ph, ph2, ph3, vph, env, fenv, vt, s1[2], s2[2], c1[4], c2[4];
    fm_v fm;                                     /* (an FM voice's note: fm.c) */
} lead;
/* (the voices batch, item 5) the Lead's FM voices, from BV_FM0: fm.c's recipes; their level against the other
 * voices', one constant, the user's to set by ear */
#define BV_FM0 EN_BV_FM
static const uint8_t BV_FM_REC[EN_NBV - BV_FM0] = {FM_SOFT_LEAD, FM_WOOD_REED, FM_SOFT_BRASS};
#define BV_FM_G 1.0f
#ifdef EN_HOST
int en_fm_ok(void)                               /* (host tests, the voices batch, item 5) every FM voice's state sound */
{
    int i, ok = fm_state_ok(&lead.fm);
    for (i = 0; i < NPLUCK; i++)
        ok &= fm_state_ok(&pluck[i].fm);
    for (i = 0; i < 2 * NDR; i++)
        ok &= fm_state_ok(&drv[i / NDR][i % NDR].fm);
    return ok;
}
int en_fm_drone_stage(int op) { return drv[dr_bank][0].fm.ix[op]; }
int en_fm_drone_note(void) { return drv[dr_bank][0].note; }
#endif
static float lead_vol, ld_gk = 0.004f, ld_tone = 1.0f, ld_att = 0.0009f, ld_rev = 1.0f, ld_echo = 1.0f;
#define CP_MAX EN_LEAD_CPMAX                     /* (the keys' figure: the most entries kept, amb.h) */
#define LD_MOTN (((CP_MAX > 8 ? CP_MAX : 8) + 1) & ~1)   /* (a motif's room: the keys' theme's, or the generator's
                                                  * 8; even, for its first half twice: ld_frag) */
static uint8_t ld_note[LD_MOTN], ld_dur[LD_MOTN], ld_n, ld_i, ld_left, ld_rest;
static int8_t ld_dg[LD_MOTN], ld_key = -1;       /* (the phrase's degrees; the degree held on the keys, -1: none) */
volatile int8_t en_lead_deg = -1;                /* (the degree the Lead sounds, for the keys' light; -1: none) */
static uint32_t ld_rng = 0x3C6EF372u;
/* (BERLIN, the Lead's memory: the report's "remembered-motif Lead") four motifs, each up to LD_MOTN entries (a drawn
 * one up to 7; a theme the keys played up to CP_MAX): a scale degree
 * from the chord root's degree (LD_REST: a rest) and its length in sixteenths; slot 0 the theme (never a fresh draw's).
 * Rendered at a phrase's start from the chord then (a motif again over a new chord: transposed; a note in flight never
 * moves). Played in pairs: a call, its last note an open ending (a chord tone not the root, now and then the 9th),
 * then after a breath of a beat or two (on a beat; the user: an answer two bars late "feels unsatisfying") its answer,
 * the same rhythm with a closed ending (the root, or 30% the fifth) held twice as long. The pairs come in blocks on one
 * motif (spec 7.1): 1 pair at Memory under 20, else 2 (calm) or 3 (busy), a breath of bars between them. A block plays
 * the line's own idea (one of slots 1..3): at a block's start it's kept (Memory's chance), else half the time varied
 * once (not before it's been heard twice), half the time a fresh one drawn into the oldest of slots 1..3; from its third
 * hearing any pass may vary it once (1 - Memory a pass). Memory is read at a block's start and held for it. A busy
 * block of 3 half the time a sentence instead: the motif, again shifted a degree or two, its first half twice, then the
 * run (the cadence) down (or 30% up) into a closed ending. A calm block now and then (the Bridge's half the time) is
 * played with its values doubled. The theme (slot 0, the piece's first idea, or a figure played on the white keys:
 * ld_take) breaks in for one pair when Solo begins, after the rise's peak, when a Bridge begins (cutting a block short
 * at its next pair; not before Lead in, nor while the theme is playing) and after 6 blocks without it; then the idea
 * goes on. Under Memory 20 no theme comes back: a
 * fresh motif every pair. Intensity (spec 3.1, from the arc, at every phrase): Flow and Bridge 0.2, Solo 0.4 + 0.6 the
 * rise; more notes and shorter values in a fresh motif, a shorter breath, the register's centre up to 5 degrees higher
 * (walking a degree a pair); a motif remembered is played in the octave nearest the register's home, an octave up in
 * the top of the rise (an octave keeps its harmony; its notes stay as they were made) */
typedef struct {
    int8_t off[LD_MOTN];
    uint8_t dur[LD_MOTN];
    uint8_t n, heard;                            /* (entries, 1..LD_MOTN; passes played) */
    uint16_t cm;                                 /* (the chord it was made on: its tones over the root, ld_cmask; over
                                                  * another kind of chord its held notes are fitted; LD_KEEP: written,
                                                  * played in the octave it was written in; LD_PLAYED: the keys'
                                                  * theme, back at the arc's moments at any Memory) */
} ld_motif_t;
#define LD_KEEP 0x8000u
#define LD_PLAYED 0x2000u
#define LD_CM 0x0FFFu
static ld_motif_t ld_bank[4];
static uint8_t ld_slot, ld_fresh = 1, ld_role, ld_due;   /* the block's motif; the next fresh draw's slot; the phrase's
                                                  * role; the next phrase due (1: the group's, after a breath of beats;
                                                  * 2: the block's next pair, after a breath of bars) */
static uint8_t ld_blen, ld_pair, ld_step, ld_sent, ld_since, ld_cb, ld_gap;   /* the block: its pairs (0: none yet),
                                                  * pairs begun, the phrase's place in its pair or sentence, a sentence;
                                                  * blocks since the theme, the theme due; sixteenths to the breath's end */
static uint8_t ld_idea;                          /* (the line's own idea: its slot, 1..3; 0 none yet) */
static uint8_t ld_mem, ld_aug, ld_prep, ld_pint; /* the block's Memory (as it was at its start) and values doubled;
                                                  * the next fresh motif's draws so far and its intensity (x20; + 32 x its
                                                  * kind of offspring, ld_band) */
static int8_t ld_next = -1;                      /* (a slot the next pair plays; -1: a fresh draw) */
static int8_t ld_reg, ld_shift, ld_end, ld_oct;  /* the register's centre (degrees over its home); a sentence's
                                                  * shift; the degree the last phrase ended on; the pass's octave (its
                                                  * transposition, octaves: from the register now, not kept in the motif) */
static float ld_int;                             /* intensity 0..1 (from the arc) */
/* (spec 3.5, 7.3) the white keys write the theme: each key the Lead plays from them while the music runs is written
 * down, its degree and its length in sixteenths of the grid (counted tick by tick while it's held, each key on the
 * sixteenth nearest it; a breath under a beat before the next key joins the note before it, a beat or more is a
 * rest). Two beats with no key held close the figure (ld_take): 3 notes or more in its last 4 bars (CP_MAX entries at
 * most) are the theme; fewer, a gesture, let go. (Part 3, the user: "it basically repeats what I played") the Lead
 * doesn't play it back: the player's figure was the call, and the Lead answers it, as after a call of its own (1-2
 * beats on, in any section, whatever Lead in and the roll: the user asked by playing): the figure's rhythm, the
 * generator's notes, near where it was played, closing on the chord. The figure itself comes back whole at the theme's
 * moments. After the answer, the next block at the bar line after a breath (not on the roll): the line's fresh ideas
 * are its offspring for a while (ld_offspring), the first then. PANIC and a piece change let the figure go (not the
 * theme; a piece change draws a new bank) */
static int8_t cp_dg[CP_MAX];                     /* (the figure: degrees as played; LD_REST a rest) */
static uint8_t cp_du[CP_MAX], cp_n, cp_q, ld_cap;   /* (their lengths; entries; sixteenths since a key was let go;
                                                  * the key held is the figure's last note: CP_HELD; the keys' answer
                                                  * due: CP_ANS; the keys' answer playing, the next block due after it:
                                                  * CP_NEXT) */
#define CP_HELD 2u
#define CP_ANS 4u
#define CP_NEXT 8u
static int8_t cp_r;                              /* (the chord root's degree when the keys' theme was taken: the
                                                  * frame of its degrees, for its answer) */
static uint8_t ld_kin = 255;                     /* (bars since the keys' theme was taken: its offspring's clock,
                                                  * ld_kinship; 255: none, or their time over) */
static uint8_t ld_iband;                         /* (the kind of offspring the line's idea was made as, ld_band: a
                                                  * block's reason for a fresh one when the kinship has moved on) */
volatile uint8_t en_lead_role, en_lead_phr;
#ifdef EN_HOST
volatile uint8_t en_lead_slot, en_lead_blk, en_lead_int;   /* (test readouts: host builds only) */
volatile uint8_t en_lead_dmax;                   /* (the most motifs drawn in a tick; ld_draws: this tick's) */
static uint8_t ld_draws;
#endif

static void ld_reseed(void)                      /* a new piece, or FORGET: new ideas (the bank drawn again as it's
                                                  * needed) */
{
    int k;
    for (k = 0; k < 4; k++) {
        ld_bank[k].n = ld_bank[k].heard = 0;
        ld_bank[k].cm = 0;                       /* (the keys' theme gone with it: no LD_PLAYED left to bring an
                                                  * empty slot back at the arc's moments) */
    }
    ld_fresh = 1;
    ld_next = -1;
    ld_due = ld_blen = ld_pair = ld_step = ld_sent = ld_since = ld_cb = ld_idea = ld_prep = ld_aug = 0;   /* (its first
                                                  * block the theme) */
    ld_reg = 0;
    ld_n = 0;                                    /* (a phrase in flight let go, its answer not due: the new piece's
                                                  * Emerge stays silent) */
    cp_n = cp_q = ld_cap = 0;                    /* (a figure on the keys let go, a theme of theirs not answered) */
    ld_kin = 255;                                /* (no offspring of it) */
    ld_iband = 0;
}
static uint32_t ld_cmask(void);
#ifdef EN_HOST
int en_heard(int p) { return pe(p); }
void en_lead_motif(int slot, const int8_t *off, const uint8_t *dur, int n)
{
    ld_motif_t *m = &ld_bank[slot & 3];
    int i;
    n = n < 1 ? 1 : n > LD_MOTN ? LD_MOTN : n;
    for (i = 0; i < n; i++)
        m->off[i] = off[i], m->dur[i] = dur[i];
    m->n = (uint8_t)n;
    m->heard = 0;
    m->cm = (uint16_t)(ld_cmask() | LD_KEEP);
    ld_next = (int8_t)(slot & 3);
    ld_prep = 0;
}
int en_lead_cap(void)
{
    int i, c = 0;
    for (i = 0; i < cp_n; i++)
        c += cp_dg[i] != LD_REST;
    return c;
}
int en_lead_theme(int8_t *off, uint8_t *dur)
{
    int i;
    for (i = 0; i < ld_bank[0].n; i++)
        off[i] = ld_bank[0].off[i], dur[i] = ld_bank[0].dur[i];
    return ld_bank[0].n;
}
int en_lead_played(void) { return (ld_bank[0].cm & LD_PLAYED) != 0; }
volatile uint8_t en_lead_ideas, en_lead_kind;
static int ld_kinship(void);
int en_lead_kin(void) { return ld_kinship(); }
static void ld_vary(ld_motif_t *m);
void en_lead_vary(int slot) { ld_vary(&ld_bank[slot & 3]); }
int en_lead_idea(int8_t *off, uint8_t *dur)
{
    int i;
    if (!ld_idea)
        return 0;
    for (i = 0; i < ld_bank[ld_idea].n; i++)
        off[i] = ld_bank[ld_idea].off[i], dur[i] = ld_bank[ld_idea].dur[i];
    return ld_bank[ld_idea].n;
}
#endif

static int ld_audible(void) { return lead_vol > 0.0f && !(mutes & EN_M_BASS); }

static void lead_off(void)                       /* the note let go */
{
    if (lead.on) {
        lead.on = 0;
        midi(0x82, lead.note, 0);
    }
    en_lead_deg = -1;
}
void en_lead_key(int d, int on) { post(C_LKEY, d, on); }
void en_lead_forget(void) { post(C_LFORGET, 0, 0); }

static int ld_deg_raw(int d)                     /* scale degree d (any) as a note, in the Lead's register */
{
    const int ns = nsteps ? nsteps : 7, o = (d + 64 * ns) / ns - 64, k = (d + 64 * ns) % ns;
    return 60 + par[P_ROOT] % 12 + 12 * par[P_LOCT] + 12 * o + (nsteps ? steps[k] : 2 * k);
}
static int ld_deg_note(int d)                    /* (kept within 36 .. 100) */
{
    const int n = ld_deg_raw(d);
    return n < 36 ? 36 : n > 100 ? 100 : n;
}

static int ld_cdeg(int k) { return en_deg_of(pool, chord_pcs[k] - par[P_ROOT]); }   /* chord tone k's degree */
static int ld_near(int d, int c)                 /* degree c (0..ns-1) in the octave nearest degree d */
{
    const int ns = nsteps ? nsteps : 7, k = ((c - d) % ns + ns) % ns;
    return d + (2 * k > ns ? k - ns : k);
}
static uint32_t ld_cmask(void)                   /* the chord's tones as degrees over its root (bit k: k up; the root's
                                                  * always) */
{
    const int ns = nsteps ? nsteps : 7, r = ld_cdeg(0);
    uint32_t cm = 1u;
    int k;
    for (k = 0; k < chord_nt; k++)
        cm |= 1u << ((ld_cdeg(k) - r + ns) % ns);
    return cm;
}
static int ld_on(int d, uint32_t cm)             /* degree d over the chord's root a chord tone */
{
    const int ns = nsteps ? nsteps : 7;
    return (int)(cm >> (((d % ns) + ns) % ns) & 1u);
}
static int ld_snap(int d, uint32_t cm)           /* the chord tone nearest degree d over the root (the lower if two) */
{
    const int ns = nsteps ? nsteps : 7;
    int k;
    for (k = 0; k < ns; k++) {
        if (ld_on(d - k, cm))
            return d - k;
        if (ld_on(d + k, cm))
            return d + k;
    }
    return d;
}

/* a fresh motif, as a player shapes one (the report's "new motifs"): its rhythm first, from the templates below (1, 2
 * or 4 bars, the long values on beats 1 and 3; about 2 + 4 intensity notes: whole and half notes when calm, a run of
 * 8ths or 16ths about a held note when busy), then its pitches one by one, each drawn by the product range x proximity
 * x key (Temperley's): near a centre the contour moves (an arch half the time, a descent 30%, for a call a rise and
 * hold 20%; less far in a motif of few notes), near the note before (mostly steps), on the chord on the strong beats. After a leap of 3 degrees or more
 * (or 5 semitones) the next note turns back; a note held a dotted quarter or longer is a chord tone, but for the
 * tension note (0.2 + 0.4 intensity of the motifs): a tone off the chord a degree above a chord tone (the 6th first,
 * then the 4th), held half as long again, the next note the chord tone under it. Offsets from the chord root's
 * degree; the register's centre an octave and a half over the Lead's root (and ld_reg higher: intensity's) */
#define LD_RST 0x80u                             /* (in a template: a rest, its length below) */
#define LD_HELD 6                                /* (sixteenths: a held note) */
static const uint8_t LD_RHY[][7] = {
    {16, 16}, {8, 8}, {24, 8},
    {8, 8, 16}, {12, 4, 16}, {4, 4, 8}, {8, LD_RST | 4, 4, 16},
    {8, 4, 4, 16}, {12, 2, 2, 16}, {24, 8, 16, 16},
    {8, 4, 4, 8, 8}, {6, 2, 8, 8, 8}, {4, 2, 2, 8, 16}, {12, LD_RST | 2, 2, 4, 4, 8},
    {8, 2, 2, 2, 2, 16}, {12, 1, 1, 1, 1, 16}, {4, 4, 2, 2, 4, 16},
};
#define LD_NRHY ((int)(sizeof LD_RHY / sizeof LD_RHY[0]))
static const uint8_t LD_R[7] = {16, 15, 12, 8, 5, 2, 1};   /* range: by degrees from the centre */
static const uint8_t LD_P[8] = {4, 16, 6, 2, 1, 1, 1, 1};   /* proximity: by degrees from the note before (the
                                                              * report's 4 16 8 4 2 1 1 2, steeper: held notes are
                                                              * chord tones, a third apart) */

static int ld_rhy_n(int j)                       /* a template's notes (its rests not counted) */
{
    int i, n = 0;
    for (i = 0; i < 7 && LD_RHY[j][i]; i++)
        n += !(LD_RHY[j][i] & LD_RST);
    return n;
}

static int ld_rhy_pick(int tgt)                  /* a template drawn for about tgt notes (its weight by its notes'
                                                  * distance from the target) */
{
    static const uint8_t W[3] = {6, 3, 1};
    uint32_t tot = 0, x;
    int j;
    for (j = 0; j < LD_NRHY; j++) {
        const int e = ld_rhy_n(j) - tgt;
        tot += e > -3 && e < 3 ? W[e < 0 ? -e : e] : 0u;
    }
    x = seq_rand(&ld_rng) % tot;
    for (j = 0;; j++) {
        const int e = ld_rhy_n(j) - tgt;
        const uint32_t v = e > -3 && e < 3 ? W[e < 0 ? -e : e] : 0u;
        if (x < v || j + 1 >= LD_NRHY)
            break;
        x -= v;
    }
    return j;
}
/* (the Lead's offspring) a note the guide asks for (y, over the chord's root), as a drawn note must be: after a leap
 * into the note before it (turn: +1 up, -1 down), back the other way (a step); held (6 sixteenths or more), a chord
 * tone, the nearest (the nearest that way, after a leap), but for a tension resolved a step down onto one (res) */
static int ld_fit(int y, int prev, int turn, int held, int res, uint32_t cm)
{
    if (turn && (turn > 0 ? y <= prev : y >= prev))
        y = prev + turn;
    if (held && !res && !ld_on(y, cm)) {
        int a = ld_snap(y, cm);
        if (turn && (turn > 0 ? a <= prev : a >= prev))
            for (a = prev + turn; !ld_on(a, cm); a += turn)   /* (the root's always a chord tone: it ends) */
                ;
        y = a;
    }
    return y;
}
#define LD_FREE 127                              /* (a guide's entry: drawn, not asked for) */
/* the generator; g (the Lead's offspring): the rhythm given (m's entries, its rests LD_REST), and the notes asked for
 * (g[i], LD_FREE: drawn as ever, near the note before; no tension note of its own) */
static void ld_gen(ld_motif_t *m, float intensity, int role, const int8_t *g)
{
    const int ns = nsteps ? nsteps : 7, r = ld_cdeg(0), w = ns < 6 ? ns : 6, c0 = ns + ns / 2 - r + ld_reg;
    const float amp = ns < 7 ? (float)ns / 7.0f : 1.0f;
    const uint32_t cm = ld_cmask();              /* (the chord's tones, as degrees over its root) */
    int i, j, k, n = 0, nn = 0, p = 0, at = 0, shape, tp = -1, tc = -1, tx = 0, prev = 0, pd = 0, ps = 0, last = -1;
    uint32_t tot = 0, x;
    const uint8_t *t;
#ifdef EN_HOST
    ld_draws++;
#endif
    intensity = intensity < 0.0f ? 0.0f : intensity > 1.0f ? 1.0f : intensity;
    if (!g) {                                    /* the rhythm */
        j = ld_rhy_pick((int)(2.0f + 4.0f * intensity + 0.5f));
        t = LD_RHY[j];
        nn = ld_rhy_n(j);
        for (n = 0; n < 7 && t[n]; n++) {
            m->dur[n] = (uint8_t)(t[n] & ~LD_RST);
            m->off[n] = t[n] & LD_RST ? (int8_t)LD_REST : 0;
        }
    } else
        for (n = m->n, i = 0; i < n; i++)
            if (m->off[i] != LD_REST)
                nn++, last = i;
    x = seq_rand(&ld_rng) % (role == LR_CALL ? 100u : 80u);
    shape = x < 50u ? 0 : x < 80u ? 1 : 2;       /* (an arch, a descent, a rise and hold) */
    if (!g && seq_rand(&ld_rng) % 100u < (uint32_t)(20.0f + 40.0f * intensity)) {   /* the tension note: where, which
                                                  * tone */
        int best = -1, q = 0, cand = 0;
        for (k = 1; k < ns; k++)                 /* (off the chord, a degree over a chord tone: the 6th, the 4th, ...) */
            if (!(cm >> k & 1u) && cm >> (k - 1) & 1u) {
                const int s = (ld_deg_raw(r + k) - ld_deg_raw(r) + 120) % 12,
                          v = s == 8 || s == 9 ? 3 : s == 5 || s == 6 ? 2 : 1;
                if (v > best)
                    best = v, tc = k;
            }
        for (i = 0; i + 1 < n; i++) {            /* (an inner note a quarter or longer, a note after it, then another) */
            if (m->off[i] == LD_REST)
                continue;
            if (q >= 1 && q + 2 < nn && m->dur[i] >= 4 && m->off[i + 1] != LD_REST &&
                seq_rand(&ld_rng) % (uint32_t)++cand == 0u)
                tp = q;
            q++;
        }
        if (tc < 0)
            tp = -1;
    }
    for (i = 0; i < n; i++) {                    /* the pitches */
        const int dur = m->dur[i], strong = !(at & 7) || dur >= 8, held = dur >= LD_HELD,
                  turn = p >= 2 && (pd >= 3 || pd <= -3 || ps >= 5 || ps <= -5) ? (pd > 0 ? -1 : 1) : 0;
        int cen, d = 0;
        float u, c;
        if (m->off[i] == LD_REST) {
            at += dur;
            continue;
        }
        u = nn > 1 ? (float)p / (float)(nn - 1) : 0.0f;
        c = shape == 0 ? 2.0f - 6.0f * (u < 0.5f ? 0.5f - u : u - 0.5f)    /* (-1 .. +2 .. -1) */
          : shape == 1 ? 2.0f - 4.0f * u                                    /* (+2 .. -2) */
                       : (u < 0.5f ? 6.0f * u : 3.0f) - 1.0f;              /* (-1 .. +2, held) */
        c *= amp * (nn < 6 ? (float)(nn - 1) * 0.2f : 1.0f);
        cen = c0 + (int)(c < 0.0f ? c - 0.5f : c + 0.5f);
        if (p == tp) {                           /* the tension note: tone tc nearest the note before, from no leap down
                                                  * (its step down then turns), turning if it must */
            d = prev - 2;
            while (((d % ns) + ns) % ns != tc)
                d++;
            if (turn && (turn < 0 ? d >= prev : d <= prev))
                tp = -1;
            else {
                const int e = dur + dur / 2;
                m->dur[i] = (uint8_t)(e > 32 ? 32 : e);
                if (m->dur[i + 1] > dur / 2 + 1)     /* (the time from the note after: the beats after it kept) */
                    m->dur[i + 1] = (uint8_t)(m->dur[i + 1] - dur / 2);
                tx = d;
            }
        }
        if (g && g[i] != LD_FREE) {              /* (asked for: as a drawn note must be; held off the chord only if
                                                  * the note after it, not the last, is asked a step down onto it, and
                                                  * no leap down into it turns that one up) */
            int y = ld_fit(g[i], prev, turn, 0, 0, cm), jn, res = 0;
            for (jn = i + 1; jn < n && m->off[jn] == LD_REST; jn++)
                ;
            if (held && jn < n && jn != last && g[jn] == y - 1 && ld_on(y - 1, cm)) {
                const int dd = y - prev, ds = ld_deg_raw(r + y) - ld_deg_raw(r + prev);
                res = !p || !(dd <= -3 || ds <= -5);
            }
            d = ld_fit(y, prev, turn, held, res, cm);
        } else if (tp >= 0 && p == tp + 1)       /* its resolution: a degree down, a chord tone */
            d = tx - 1;
        else if (p != tp) {                     /* (the draw: nothing to draw from, proximity let go, then the turn) */
            uint32_t wt[13];
            int pass;
            for (pass = 0, tot = 0; pass < 3 && !tot; pass++)
                for (k = 0; k <= 2 * w; k++) {
                    const int y = cen - w + k, cls = ((y % ns) + ns) % ns, f = y > prev ? y - prev : prev - y;
                    uint32_t v = LD_R[k > w ? k - w : w - k];
                    if (p && !pass)
                        v *= f < 8 ? LD_P[f] : 0u;
                    if (turn && pass < 2 && (turn > 0 ? y <= prev : y >= prev))
                        v = 0u;
                    v *= cm >> cls & 1u ? (strong && !cls ? 10u : strong ? 6u : 4u) : held ? 0u : strong ? 1u : 4u;
                    tot += wt[k] = v;
                }
            x = tot ? seq_rand(&ld_rng) % tot : 0u;
            for (k = 0; k < 2 * w && x >= wt[k]; k++)
                x -= wt[k];
            d = cen - w + k;
        }
        if (p) {
            pd = d - prev;
            ps = ld_deg_raw(r + d) - ld_deg_raw(r + prev);
        }
        prev = d;
        m->off[i] = (int8_t)d;
        at += m->dur[i];                         /* (as held: a tension's longer) */
        p++;
    }
    m->n = (uint8_t)n;
    m->heard = 0;
    m->cm = (uint16_t)cm;
}
#ifdef EN_HOST
static uint8_t ld_dk, ld_kd[4];                  /* (how the last draw was made; each slot's, LK_*: test readouts) */
#define LD_DK(k) (ld_dk = (uint8_t)(k))
#define LD_KD(s) (ld_kd[s] = ld_dk)
#else
#define LD_DK(k) ((void)0)
#define LD_KD(s) ((void)0)
#endif
static void ld_motif_new(ld_motif_t *m, float intensity, int role)
{
    ld_gen(m, intensity, role, 0);
    LD_DK(LK_PLAIN);
}
static int ld_smooth(const ld_motif_t *m)        /* how smooth: fewer leaps (5 semitones or more) first, then more
                                                  * steps (2 or less) for its intervals ((steps + 1) / (intervals + 2), in
                                                  * 1/64ths: a short one not smooth for its shortness alone); +1024: no
                                                  * leap and steps at least half its intervals, smooth enough */
{
    const int r = ld_cdeg(0);
    int i, p = -1, lp = 0, st = 0, iv = 0;
    for (i = 0; i < m->n; i++)
        if (m->off[i] != LD_REST) {
            if (p >= 0) {
                const int d = ld_deg_raw(r + m->off[i]) - ld_deg_raw(r + m->off[p]);
                lp += d >= 5 || d <= -5;
                st += d <= 2 && d >= -2;
                iv++;
            }
            p = i;
        }
    return 64 * (8 - lp) + 64 * (st + 1) / (iv + 2) + (!lp && 2 * st >= iv ? 1024 : 0);
}
static int ld_count(const ld_motif_t *m)         /* its notes (its rests not counted) */
{
    int i, c = 0;
    for (i = 0; i < m->n; i++)
        c += m->off[i] != LD_REST;
    return c;
}
static float ld_level(void)                      /* intensity now (spec 3.1) */
{
    return ar_sec == EN_SEC_SOLO ? 0.4f + 0.6f * ar_rise : ar_sec == EN_SEC_EMERGE ? 0.0f : 0.2f;
}
/* (the Lead's offspring; the user: "at some point the melody should diverge and start going its own way") while slot
 * 0 is a theme the keys played, the line's fresh ideas grow out of it, each a little further as its kinship falls:
 * 100 when it's taken, 0 after 45 s at Memory 0, 90 s at 50, 150 s at 100 (in bars at the tempo; Memory and the
 * tempo as they are now); then the Lead's own ideas again, till the keys play another */
static int ld_kinship(void)                      /* 0..100 */
{
    const int mem = par[P_LMEM], s = mem <= 50 ? 45 + 9 * mem / 10 : 90 + 6 * (mem - 50) / 5,
              bars = s * par[P_TEMPO] / 240;
    return ld_kin == 255 || !(ld_bank[0].cm & LD_PLAYED) || ld_kin >= bars ? 0 : 100 - 100 * ld_kin / bars;
}
/* an offspring of the keys' theme (one draw: the generator's, asked for its notes, ld_gen): over kinship 66 its head
 * (2-4 notes) continued by the generator (a template's rhythm after it), or its head and the head again from the chord
 * tone above or below its first note (a sequence), or its contour mirrored; over 33 its notes backwards (its rhythm
 * forwards), or its tail (2-4 notes) continued; then its rhythm alone, the generator's notes. Its notes asked for
 * where a drawn note may go (ld_fit: a turn after a leap, a held one on the chord), an octave or more nearer the
 * register's centre (the theme where it was played); over the chord now (degrees from its root, as every motif).
 * Returns how it was made (LK_*) */
static int ld_offspring(ld_motif_t *m, float in, int kin)
{
    const ld_motif_t *t = &ld_bank[0];
    const int ns = nsteps ? nsteps : 7, c0 = ns + ns / 2 - ld_cdeg(0) + ld_reg, cap = (int)sizeof m->off;
    const uint32_t cm = ld_cmask();
    int8_t g[sizeof m->off];
    int nt[sizeof m->off], c = 0, i, k, kind, h, e, n = 0, sum = 0, gc = 0, bk = 0, bd = 1 << 30;
    for (i = 0; i < t->n && c < cap; i++)
        if (t->off[i] != LD_REST)
            nt[c++] = i;
    if (c < 3) {                                 /* (never: a theme is 3 notes or more) */
        ld_gen(m, in, LR_CALL, 0);
        return LK_PLAIN;
    }
    k = (int)(seq_rand(&ld_rng) % 6u);
    kind = kin > 66 ? (k % 3 == 0 ? LK_HEAD : k % 3 == 1 ? LK_SEQ : LK_MIRROR) : kin > 33 ? (k & 1 ? LK_BACK : LK_TAIL)
                                                                                      : LK_RHY;
    h = c - 1 < 4 ? c - 1 : 4;                   /* (a fragment: 2 notes up to 4, fewer than the theme's) */
    h = 2 + (int)(seq_rand(&ld_rng) % (uint32_t)(h - 1));
    e = nt[h - 1] + 1;                           /* (the head's entries) */
    if (kind == LK_SEQ && 2 * e > cap)
        kind = LK_HEAD;
    if (kind == LK_HEAD || kind == LK_SEQ) {
        for (i = 0; i < e && n < cap; i++, n++)
            m->dur[n] = t->dur[i], m->off[n] = g[n] = t->off[i];
        if (kind == LK_SEQ) {                    /* (again from another chord tone: the next above or below) */
            int up = t->off[nt[0]] + 1, dn = t->off[nt[0]] - 1;
            while (!ld_on(up, cm))
                up++;
            while (!ld_on(dn, cm))
                dn--;
            k = (seq_rand(&ld_rng) & 1u ? up : dn) - t->off[nt[0]];
            for (i = 0; i < e; i++, n++) {
                m->dur[n] = t->dur[i], m->off[n] = t->off[i];
                g[n] = (int8_t)(t->off[i] == LD_REST ? LD_REST : t->off[i] + k);
            }
        }
    } else if (kind == LK_TAIL) {
        for (i = nt[c - h]; i < t->n && n < cap; i++, n++)
            m->dur[n] = t->dur[i], m->off[n] = g[n] = t->off[i];
    } else                                       /* (its rhythm whole: mirrored, backwards, or new notes) */
        for (i = 0, k = c; i < t->n && n < cap; i++, n++) {
            m->dur[n] = t->dur[i], m->off[n] = t->off[i];
            g[n] = (int8_t)(t->off[i] == LD_REST ? LD_REST : kind == LK_MIRROR ? 2 * t->off[nt[0]] - t->off[i]
                                                           : kind == LK_BACK   ? t->off[nt[--k]]
                                                                               : LD_FREE);
        }
    if (kind == LK_HEAD || kind == LK_TAIL) {    /* continued: a template's rhythm after it, for the notes left */
        const int tg = (int)(2.0f + 4.0f * in + 0.5f) - h;
        const uint8_t *r = LD_RHY[ld_rhy_pick(tg < 2 ? 2 : tg)];
        for (i = 0; i < 7 && r[i] && n < cap; i++, n++) {
            m->dur[n] = (uint8_t)(r[i] & ~LD_RST);
            m->off[n] = r[i] & LD_RST ? (int8_t)LD_REST : 0;
            g[n] = LD_FREE;
        }
    }
    for (i = 0; i < n; i++)                      /* (the octave nearest the register's centre) */
        if (m->off[i] != LD_REST && g[i] != LD_FREE)
            sum += g[i], gc++;
    for (k = -3; k <= 3 && gc; k++) {
        const int d = sum + (k * ns - c0) * gc;
        if ((d < 0 ? -d : d) < bd)
            bd = d < 0 ? -d : d, bk = k;
    }
    for (i = 0; i < n; i++)
        if (m->off[i] != LD_REST && g[i] != LD_FREE)
            g[i] = (int8_t)(g[i] + bk * ns);
    m->n = (uint8_t)n;
    ld_gen(m, in, LR_CALL, g);
    return kind;
}
static int ld_kin_due(void)                      /* the kinship a fresh idea is made with: the first after the keys'
                                                  * theme (the line without an idea) close kin, whenever it comes (it
                                                  * grows out of it at once), while the kinship lasts */
{
    const int k = ld_kinship();
    return k && !ld_idea && k <= 66 ? 67 : k;
}
static int ld_band(void)                         /* (the offspring due: 3 close, 2 further, 1 its rhythm, 0 none) */
{
    const int k = ld_kin_due();
    return k > 66 ? 3 : k > 33 ? 2 : k > 0;
}
static void ld_idea_new(ld_motif_t *m, float in)   /* a fresh idea for the line (slots 1..3): an offspring of the
                                                  * keys' theme while its kinship lasts, else the generator's own */
{
    const int k = ld_kin_due();
    if (k) {
        const int kind = ld_offspring(m, in, k);
        LD_DK(kind);
        (void)kind;
    } else
        ld_motif_new(m, in, LR_CALL);
}
/* the next fresh motif thought of ahead, one draw a bar while nothing is planned that tick (no tick of the grid draws
 * more than one motif: the device's audio block is near its limit in the busiest pieces): before the first block the
 * theme (slot 0, the smoothest of 4 draws: heard again and again, a rare draw of leaps would set the whole piece's
 * way); after it, into the oldest of slots 1..3, the first smooth enough of 3 draws, else the smoothest, there till a
 * block wants a fresh motif (thought of again if intensity moves on before that, or the kinship of the keys'
 * theme to another kind of offspring); slots 1..3 the keys' theme's offspring while its kinship lasts (ld_idea_new) */
static void ld_think(void)
{
    const int th = !ld_blen, sl = th ? 0 : ld_fresh, tries = th ? 4 : 3, in20 = (int)(ld_level() * 20.0f + 0.5f),
              kb = th ? 0 : ld_band();
    const float in = th ? 0.5f : ld_level();
    ld_motif_t *m = &ld_bank[sl];
    if (sl == ld_next || (th ? m->heard != 0 : sl == ld_idea))
        return;
    if (!th && ld_prep && ((ld_pint >> 5) != kb || in20 - (ld_pint & 31) > 3 || (ld_pint & 31) - in20 > 3))
        ld_prep = 0;                             /* (intensity moved on, or the keys' theme's kinship to another kind of
                                                  * offspring, or to none: thought of again) */
    if (ld_prep >= tries)
        return;
    if (!ld_prep) {
        if (th)
            ld_motif_new(m, in, LR_CALL);
        else
            ld_idea_new(m, in);                  /* (the keys' theme's offspring while its kinship lasts) */
        LD_KD(sl);
        ld_pint = (uint8_t)(in20 | kb << 5);
    } else {
        ld_motif_t b;
        if (th)
            ld_motif_new(&b, in, LR_CALL);
        else
            ld_idea_new(&b, in);
        if (ld_smooth(&b) + (th && ld_count(&b) >= 3 ? 4096 : 0) > ld_smooth(m) + (th && ld_count(m) >= 3 ? 4096 : 0)) {
            *m = b;                              /* (the theme a figure: 3 notes or more first) */
            LD_KD(sl);
        }
    }
    ld_prep = (uint8_t)(!th && ld_smooth(m) >= 1024 ? tries : ld_prep + 1);
}

static int ld_fold(int d)                        /* (a note an octave down or up if the register would clip it: its
                                                  * pitch class kept) */
{
    const int ns = nsteps ? nsteps : 7;
    while (ld_deg_raw(d) > 100)
        d -= ns;
    while (ld_deg_raw(d) < 36)
        d += ns;
    return d;
}
static int ld_ending1(int d, int closed, int nine)   /* the role's ending nearest degree d (the closed one's tone, nine:
                                                  * the fifth's; the open one's, nine: the 9th) */
{
    const int r = ld_cdeg(0);
    int k, best = 99, e = r, c;
    if (closed) {                                /* the root, or the chord tone nearest a fifth above it */
        if (nine) {
            int bk = 1;
            for (k = 1; k < chord_nt; k++) {
                const int iv = (chord_pcs[k] - chord_pcs[0] + 12) % 12, x = iv > 7 ? iv - 7 : 7 - iv;
                if (x < best)
                    best = x, bk = k;
            }
            e = ld_cdeg(bk);
        }
        return ld_near(d, e);
    }
    if (!nine)                                   /* open: another chord tone (the nearest) */
        for (k = 1; k < chord_nt; k++)
            if ((c = ld_cdeg(k)) != r) {
                const int t = ld_near(d, c), x = t > d ? t - d : d - t;
                if (x < best)
                    best = x, e = t;
            }
    return best < 99 ? e : ld_near(d, (r + 1) % (nsteps ? nsteps : 7));   /* (or the 9th) */
}
static int ld_ending(int d, int closed, int pen, int turn)   /* the phrase's last degree d moved to the role's ending
                                                  * (closed: the root, or 30% the fifth; open: another chord tone, or 5 +
                                                  * 20 intensity % the 9th), nearest d; but nearest the note before it
                                                  * (pen) if that's more than two degrees from it (an ending reached, not
                                                  * leapt to); but after a leap into pen (turn: +1 up, -1 down) the one
                                                  * that turns, nearest d if the one nearest pen doesn't */
{
    const int nine = closed ? chord_nt > 1 && seq_rand(&ld_rng) % 100u < 30u
                            : seq_rand(&ld_rng) % 100u < (uint32_t)(5.0f + 20.0f * ld_int);
    const int e = ld_ending1(d, closed, nine);
    int f;
    if (e - pen <= 2 && pen - e <= 2)
        return e;
    f = ld_ending1(pen, closed, nine);
    if (!turn || (turn > 0 ? f > pen : f < pen))
        return f;
    return (turn > 0 ? e > pen : e < pen) ? e : f;
}

/* the variations (the report's table but its free ones: the exact repeat and the chord move every pass has, the octave
 * and the doubled values are a pass's way of playing it, not changes kept): a passing note nudged a degree; the ending
 * moved a third (the re-fit then lands on another tone); the tail (1..3 notes) redrawn by steps, a held note a chord
 * tone; the first half twice. One of them, tried in turn till one can; none makes a leap (a motif remembered is heard
 * again and again: one varied into leaps would set the whole line's way). On the motif's own chord (cm: as it was
 * made) */
static int ld_hangs(const int8_t *o, const uint8_t *du, int i, int last, int r, uint32_t cm, int lastok)   /* note i
                                                  * (of degrees o, the chord's root r) held off the chord and not resolved
                                                  * a step down onto it by the note after (onto the last only if lastok:
                                                  * the last is re-fitted) */
{
    int j;
    if (o[i] == LD_REST || du[i] < LD_HELD || ld_on(o[i] - r, cm))
        return 0;
    for (j = i + 1; o[j] == LD_REST; j++)
        ;
    return !((lastok || j != last) && o[j] == o[i] - 1 && ld_on(o[j] - r, cm));
}
static int ld_frag(ld_motif_t *m)                /* the first half, twice (its half not ending on a held tone off the
                                                  * chord, nor a rest: a tension's resolution, the line, kept whole) */
{
    const uint32_t cm = m->cm & LD_CM;           /* (the chord it was made on) */
    int h = (m->n + 1) / 2, i;
    while (h > 1 && (m->off[h - 1] == LD_REST || (m->dur[h - 1] >= LD_HELD && !ld_on(m->off[h - 1], cm))))
        h--;
    if (m->n < 2 || m->off[0] == LD_REST || m->off[h - 1] - m->off[0] > 2 || m->off[0] - m->off[h - 1] > 2 ||
        2 * h > (int)sizeof m->off)
        return 0;                                /* (its halves joined by a step or a third: no leap made; room for
                                                  * both) */
    for (i = 0; i < h; i++)
        m->off[h + i] = m->off[i], m->dur[h + i] = m->dur[i];
    m->n = (uint8_t)(2 * h);
    return 1;
}
static int ld_bad(const ld_motif_t *m)          /* its held tones off the chord (as made) not resolved a step down onto
                                                  * it before the last note (the last is re-fitted: no resolution) */
{
    int i, last = -1, n = 0;
    for (i = 0; i < m->n; i++)
        if (m->off[i] != LD_REST)
            last = i;
    for (i = 0; i < last; i++)
        n += ld_hangs(m->off, m->dur, i, last, 0, m->cm & LD_CM, 0);
    return n;
}
static int ld_turn(const ld_motif_t *m, int i)   /* the way note i must go after a leap into the note before it (a
                                                  * leap: 3 degrees or more, or 5 semitones; +1 up, -1 down; 0 free) */
{
    const int r = ld_cdeg(0);
    int pv, pp, dd, ds;
    for (pv = i - 1; pv >= 0 && m->off[pv] == LD_REST; pv--)
        ;
    for (pp = pv - 1; pp >= 0 && m->off[pp] == LD_REST; pp--)
        ;
    if (pp < 0)
        return 0;
    dd = m->off[pv] - m->off[pp];
    ds = ld_deg_raw(r + m->off[pv]) - ld_deg_raw(r + m->off[pp]);
    return dd >= 3 || dd <= -3 || ds >= 5 || ds <= -5 ? (dd > 0 ? -1 : 1) : 0;
}
static int ld_op(ld_motif_t *m, int op, int last)   /* variation op tried on m: 1 if made */
{
    const int r = ld_cdeg(0);
    const uint32_t cm = m->cm & LD_CM;
    int i;
    if (op == 0) {                               /* a passing note (short, not after a held one: a tension's
                                                  * resolution kept) a degree up or down, a step or a third from the
                                                  * notes either side (no leap made) */
        int c = 0, pick = -1, pv = -1, nv = 0, nx, s;
        for (i = 0; i < last; pv = m->off[i] == LD_REST ? pv : i, i++) {
            if (m->off[i] == LD_REST || m->dur[i] >= LD_HELD || (pv >= 0 && m->dur[pv] >= LD_HELD))
                continue;
            for (nx = i + 1; m->off[nx] == LD_REST; nx++)
                ;
            for (s = -1; s <= 1; s += 2) {
                const int v = m->off[i] + s;
                if ((pv < 0 || (v - m->off[pv] <= 2 && m->off[pv] - v <= 2)) && v - m->off[nx] <= 2 &&
                    m->off[nx] - v <= 2 && seq_rand(&ld_rng) % (uint32_t)++c == 0u)
                    pick = i, nv = v;
            }
        }
        if (pick < 0)
            return 0;
        m->off[pick] = (int8_t)nv;
        return 1;
    }
    if (op == 1) {                               /* the ending moved a third (back the other way still, after a leap
                                                  * into the note before it) */
        const int s = seq_rand(&ld_rng) & 1u ? 2 : -2, t = ld_turn(m, last);
        int v = m->off[last] + s, pv;
        for (pv = last - 1; pv >= 0 && m->off[pv] == LD_REST; pv--)
            ;
        if (t && pv >= 0 && (t > 0 ? v <= m->off[pv] : v >= m->off[pv]))
            v = m->off[last] - s;
        if (t && pv >= 0 && (t > 0 ? v <= m->off[pv] : v >= m->off[pv]))
            return 0;
        m->off[last] = (int8_t)v;
        return 1;
    }
    if (op == 2) {                               /* the tail redrawn by steps (a step 4, a third 1, the note again 1;
                                                  * turning after a leap; a held note on the chord; after a held tone off
                                                  * it, its resolution a step down) */
        int k = 1 + (int)(seq_rand(&ld_rng) % 3u), from = last, prev, pp = -99, j, first = 1;
        for (j = 1; j < k && from > 0; ) {   /* (from: the tail's first note; a note left before it) */
            from--;
            j += m->off[from] != LD_REST;
        }
        for (j = from - 1; j >= 0 && m->off[j] == LD_REST; j--)
            ;
        if (j < 0)
            return 0;
        prev = m->off[j];
        for (i = j - 1; i >= 0 && m->off[i] == LD_REST; i--)
            ;
        if (i >= 0)
            pp = m->off[i];
        for (i = from; i <= last; i++) {
            int d, s;
            if (m->off[i] == LD_REST)
                continue;
            if (first && m->dur[j] >= LD_HELD && !ld_on(prev, cm))
                d = prev - 1;
            else {
                const int ds = pp > -99 ? ld_deg_raw(r + prev) - ld_deg_raw(r + pp) : 0;
                const int turn = pp > -99 && (prev - pp >= 3 || pp - prev >= 3 || ds >= 5 || ds <= -5) ?
                                 (prev > pp ? -1 : 1) : 0;
                uint32_t wt[5], tw = 0, y;
                for (s = -2; s <= 2; s++) {
                    uint32_t w = s == 1 || s == -1 ? 4u : 1u;
                    if ((m->dur[i] >= LD_HELD && !ld_on(prev + s, cm)) || (turn && s * turn <= 0))
                        w = 0u;
                    tw += wt[s + 2] = w;
                }
                if (!tw)
                    d = ld_snap(prev, cm);
                else {
                    y = seq_rand(&ld_rng) % tw;
                    for (s = 0; s < 4 && y >= wt[s]; s++)
                        y -= wt[s];
                    d = prev + s - 2;
                }
            }
            pp = prev;
            prev = d;
            first = 0;
            m->off[i] = (int8_t)d;
        }
        return 1;
    }
    return op == 3 && ld_frag(m);
}
static void ld_vary(ld_motif_t *m)
{
    const uint32_t x = seq_rand(&ld_rng) % 100u;
    const ld_motif_t was = *m;
    const int bad = ld_bad(m);                   /* (as it was: held tones off the chord not resolved) */
    int op, tries, i, last = -1;
    for (i = 0; i < m->n; i++)
        if (m->off[i] != LD_REST)
            last = i;
    if (last < 0)
        return;
    op = x < 38u ? 0 : x < 57u ? 1 : x < 88u ? 2 : 3;   /* (the nudge 38, the ending 19, the tail 31, the half 12) */
    for (tries = 0; tries < 4; tries++, op = (op + 1) % 4) {   /* (one that leaves no more held tones off the
                                                  * chord unresolved than there were: else undone, the next tried; never
                                                  * the first half twice of a theme the player played: their second half
                                                  * would be gone for good) */
        if (op == 3 && (m->cm & LD_PLAYED))
            continue;
        if (ld_op(m, op, last) && ld_bad(m) <= bad)
            return;
        *m = was;
    }
}
/* the sentence's cadence (the old Lead's slow run, kept for this): 3..6 steps down (30% up) from where the line is,
 * into a closed ending (the root or the fifth) held, a quarter first; eighths, or sixteenths at its busiest */
static void ld_cadence(ld_motif_t *m)
{
    const int k = 3 + (int)(seq_rand(&ld_rng) % 4u), dn = seq_rand(&ld_rng) % 100u < 70u ? 1 : -1, r = ld_cdeg(0);
    const int e = ld_ending(ld_end - dn * k, 1, ld_end - dn * k, 0);
    int i;
    for (i = 0; i <= k; i++) {
        m->off[i] = (int8_t)(e + dn * (k - i) - r);
        m->dur[i] = (uint8_t)(!i ? 4 : i == k ? 8 : ld_int >= 0.85f ? 1 : 2);
    }
    m->n = (uint8_t)(k + 1);
}
/* a new block (spec 7.1): Memory taken as it is now, for the whole block; its motif (the slot asked for; the theme
 * when it's due, at a piece's first, or after 6 blocks without it at Memory 20 or more: a return of it one pair; else
 * the line's own idea, one of slots 1..3, as it was: kept, Memory's chance, or varied once, or a fresh one, the one
 * thought of ahead (ld_think); a fresh one too, at any Memory, when the keys' theme's kinship has moved on to another
 * kind of offspring or to none: ld_iband; the theme's returns come and go, the idea they broke into goes on after
 * them), its pairs, a sentence or not, its values doubled or not. Intensity isn't latched: lead_plan reads it at every
 * phrase.
 * Returns 1 if the motif is as it was (kept, or the theme back: this pass may still vary it) */
static int ld_block(void)
{
    const int asked = ld_next >= 0, first = !ld_blen;
    int same = 1, k, tot = 0, back;
    ld_mem = (uint8_t)par[P_LMEM];
    back = !asked && !first && (ld_mem >= 20 || (ld_bank[0].cm & LD_PLAYED)) && (ld_cb || ld_since >= 6);   /* (the
                                                  * theme back: not under Memory 20, but the keys' always: a theme the
                                                  * player played isn't thrown away) */
    if (asked) {
        ld_slot = (uint8_t)ld_next;
        ld_next = -1;
        same = 0;
        if (ld_slot)
            ld_idea = ld_slot;
    } else if (back || first) {
        ld_slot = 0;
        if (first)
            ld_prep = 0;                         /* (the theme heard: thinking ahead goes on to the oldest slot) */
    } else {
        const int kb = ld_band(), moved = ld_idea && ld_iband != kb;   /* (the keys' theme's kinship moved on to
                                                  * another kind of offspring, or to none, since the line's idea was
                                                  * made: a fresh one, at any Memory, each further from it, then the
                                                  * Lead's own; never without the keys, and before the dice: the same
                                                  * draws as ever) */
        ld_slot = ld_idea;
        if (!ld_idea || moved || seq_rand(&ld_rng) % 100u >= (uint32_t)ld_mem) {
            same = 0;
            if (!moved && ld_idea && ld_bank[ld_idea].heard >= 2 && seq_rand(&ld_rng) & 1u) {
                ld_vary(&ld_bank[ld_idea]);
            } else if (ld_prep || !ld_idea || moved) {   /* (a fresh one, the one thought of ahead: and before it's
                                                  * been heard twice, never varied) */
                ld_slot = ld_fresh;
                ld_fresh = (uint8_t)(ld_fresh % 3 + 1);
                if (!ld_prep || (ld_pint >> 5) != kb) {   /* (none thought of yet: the line's first idea drawn now;
                                                  * after the keys' theme, as its first offspring: ld_idea still 0;
                                                  * or the one thought of is of a kind gone by) */
                    ld_idea_new(&ld_bank[ld_slot], ld_int);
                    LD_KD(ld_slot);
                }
                ld_iband = (uint8_t)kb;
                ld_idea = ld_slot;
                ld_prep = 0;
#ifdef EN_HOST
                en_lead_ideas++;
                en_lead_kind = ld_kd[ld_slot];
#endif
            } else
                same = 1;                        /* (none thought of yet: the idea kept a block more; no planning tick
                                                  * draws as well) */
        }
    }
    if (!ld_bank[ld_slot].n) {
        if (ld_slot)
            ld_idea_new(&ld_bank[ld_slot], ld_int);
        else
            ld_motif_new(&ld_bank[ld_slot], ld_int, LR_CALL);
        LD_KD(ld_slot);
    }
    ld_cb = 0;
    ld_since = ld_slot ? (uint8_t)(ld_since < 255 ? ld_since + 1 : 255) : 0;
    ld_blen = (uint8_t)(ld_mem < 20 || (!ld_slot && (back || asked)) ? 1 : ld_int < 0.5f ? 2 : 3);   /* (the
                                                  * theme back, or the keys': one pair) */
    for (k = 0; k < ld_bank[ld_slot].n; k++)
        tot += ld_bank[ld_slot].dur[k];
    ld_aug = (uint8_t)(!asked && !(ld_bank[ld_slot].cm & LD_KEEP) && tot <= 32 && ld_int < 0.3f &&
                       seq_rand(&ld_rng) % 100u < (ar_sec == EN_SEC_BRIDGE ? 50u : 10u));   /* (the values doubled:
                                                  * calm, the Bridge's half the time) */
    ld_sent = (uint8_t)(!asked && ld_blen == 3 && seq_rand(&ld_rng) % 100u < 50u);
    ld_pair = (uint8_t)(ld_sent ? ld_blen - 1 : 0);   /* (a sentence the whole block) */
    if (ld_sent) {
        const uint32_t s = seq_rand(&ld_rng) % 4u;
        ld_shift = (int8_t)(s < 2u ? (int)s - 2 : (int)s - 1);   /* (-2, -1, 1, 2) */
    }
#ifdef EN_HOST
    en_lead_blk++;
#endif
    return same;
}

static void lead_plan(void)                      /* the next phrase: the group's next (an answer, a sentence's next), or
                                                  * a pair's call (the block's next, or a new block's first) */
{
    const int r = ld_cdeg(0), ns = nsteps ? nsteps : 7;
    const uint32_t cm = ld_cmask();
    ld_motif_t t;
    int i, last = -1, lo = 999, hi = -999, sh = 0, aug, keys;
    ld_int = ld_level();
    if (!ld_bank[0].n)                           /* (the theme, a piece's first idea, if it wasn't thought of ahead: a
                                                  * figure of about 4 notes, whatever the section it's first heard in) */
        ld_motif_new(&ld_bank[0], 0.5f, LR_CALL);
    if (ld_due == 1) {
        ld_step++;
    } else {
        const int tg = ld_int > 0.4f ? (int)((ld_int - 0.4f) * (5.0f / 0.6f) + 0.5f) : 0;
        ld_reg = (int8_t)(ld_reg + (tg > ld_reg) - (tg < ld_reg));   /* (the register walking to intensity's) */
        if ((ld_next >= 0 || ld_cb || ld_pair >= ld_blen ? ld_block() : 1) && ld_bank[ld_slot].heard >= 2 &&
            seq_rand(&ld_rng) % 100u >= (uint32_t)ld_mem)
            ld_vary(&ld_bank[ld_slot]);         /* (a pass, from its third: 1 - Memory a pass, one variation; the
                                                  * block's Memory) */
        ld_pair++;
        ld_step = 0;
        if (ld_bank[ld_slot].heard < 255)
            ld_bank[ld_slot].heard++;
    }
    ld_due = 0;
    ld_role = (uint8_t)(ld_sent ? (ld_step < 2 ? LR_SEQ : ld_step == 2 ? LR_FRAG : LR_CADENCE)
                                : ld_step ? LR_ANSWER : LR_CALL);
    t = ld_bank[ld_slot];
    keys = ld_role == LR_ANSWER && !ld_slot && (ld_cap & CP_ANS);
    ld_cap &= (uint8_t)~CP_ANS;
    if (keys) {                                  /* the keys' answer (Part 3: not a copy of them, the parrot the user
                                                  * heard; review 1: "your rhythm, related notes, a closing ending"):
                                                  * their rhythm, the generator's notes (one draw: this tick plans, so
                                                  * it thinks of nothing else) drawn around the figure's own middle,
                                                  * its first note a step or a third from the figure's last (it picks
                                                  * up where the player stopped), each note on a strong beat (as the
                                                  * generator's: on beat 1 or 3, or held a half note) the chord tone
                                                  * next to the figure's there (its outline kept, its notes not the
                                                  * same); in the figure's frame (its degrees from the root it was
                                                  * taken on, cp_r: the chord may have moved since); then the closed
                                                  * ending as any answer's, not held longer (their rhythm kept) */
        int8_t g[LD_MOTN];
        const int8_t reg = ld_reg;
        int sf = 0, c = 0, fl = 0, first = -1, lst = -1, at = 0, f;
        for (i = 0; i < t.n; i++)
            if (t.off[i] != LD_REST) {
                sf += t.off[i] + cp_r, c++, fl = t.off[i] + cp_r, lst = i;   /* (degrees over the scale's root) */
                first = first < 0 ? i : first;
            }
        for (i = 0; i < t.n; at += t.dur[i], i++) {
            const int x = t.off[i] + cp_r - r;   /* (the figure's note there, over the chord's root now) */
            int a[4], na = 0, d;
            g[i] = LD_FREE;
            if (t.off[i] == LD_REST || i == lst)
                continue;
            if (i == first) {                    /* (a step or a third from its last: a chord tone if one is) */
                for (d = -2; d <= 2; d++)
                    if (d && ld_on(fl - r + d, cm))
                        a[na++] = fl - r + d;
                if (!na)
                    a[na++] = fl - r + 1, a[na++] = fl - r - 1;
            } else if (!(at & 7) || t.dur[i] >= 8) {   /* (the nearest chord tone not the figure's) */
                for (d = 1; d <= 2 && !na; d++) {
                    if (ld_on(x - d, cm))
                        a[na++] = x - d;
                    if (ld_on(x + d, cm))
                        a[na++] = x + d;
                }
            }
            if (na)
                g[i] = (int8_t)a[seq_rand(&ld_rng) % (uint32_t)na];
        }
        f = c ? (sf >= 0 ? (2 * sf + c) / (2 * c) : -((c - 2 * sf) / (2 * c))) : ns + ns / 2;   /* (its middle) */
        ld_reg = (int8_t)(f - ns - ns / 2);      /* (the draw's centre there: ld_gen's c0) */
        ld_gen(&t, ld_int, LR_ANSWER, g);
        ld_reg = reg;
        t.cm |= LD_KEEP;                         /* (this pass in the octave it's made in: no register's octave) */
        ld_cap |= CP_NEXT;
    }
    if (ld_role == LR_FRAG) {                    /* (the first half twice, if it can: no leap, no tension left hanging) */
        ld_motif_t f = t;
        if (ld_frag(&f) && ld_bad(&f) <= ld_bad(&t))
            t = f;
    } else if (ld_role == LR_CADENCE)
        ld_cadence(&t);
    ld_n = t.n;
    aug = ld_aug && !ld_sent;                    /* (never a sentence's, its cadence: both are a block's choice, calm
                                                  * and busy; kept apart here too) */
    for (i = 0; i < ld_n; i++) {
        ld_dur[i] = (uint8_t)(aug && t.dur[i] < 16 ? 2 * t.dur[i] : aug ? 32 : t.dur[i]);
        ld_dg[i] = t.off[i] == LD_REST ? (int8_t)LD_REST
                                       : (int8_t)(r + t.off[i] + (ld_role == LR_SEQ && ld_step ? ld_shift : 0));
        if (t.off[i] != LD_REST)
            last = i;
    }
    if (last >= 0 && ld_role != LR_CADENCE && !(t.cm & LD_KEEP)) {   /* the pass in the octave nearest the register's
                                                  * centre for intensity now (its home, an octave and a half over the
                                                  * Lead's root, up to an octave higher at the rise's peak: intensity
                                                  * 0.4 .. 1), chosen at the pass's first phrase, kept for its others: a
                                                  * motif heard again in the top of the rise an octave up, down again
                                                  * after (an octave keeps its harmony; the motif isn't changed: a fresh
                                                  * one, drawn near ld_reg's walk, stays where it's drawn; the cadence
                                                  * goes on from where the line is); the keys' theme's first pass in
                                                  * the octave it was played in (the Lead answers the player there) */
        if (!ld_step) {
            const int tc = 10 * (ns + ns / 2) +   /* (in tenths of a degree) */
                           (ld_int > 0.4f ? (int)((ld_int - 0.4f) * (10.0f / 0.6f) * (float)ns + 0.5f) : 0);
            int k, d, bd = 1 << 30, sum = 0, c = 0;
            for (i = 0; i < ld_n; i++)
                if (ld_dg[i] != LD_REST)
                    sum += ld_dg[i], c++;
            for (k = -2; k <= 2; k++) {          /* (the nearest; of two as near, the one moved less) */
                d = 10 * (sum + k * ns * c) - tc * c;
                d = d < 0 ? -d : d;
                if (d < bd || (d == bd && (k < 0 ? -k : k) < (ld_oct < 0 ? -ld_oct : ld_oct)))
                    bd = d, ld_oct = (int8_t)k;
            }
        }
        for (i = 0; i < ld_n; i++)
            if (ld_dg[i] != LD_REST)
                ld_dg[i] = (int8_t)(ld_dg[i] + ld_oct * ns);
    } else if (last < 0 && !ld_step)
        ld_oct = 0;                              /* (a pass of rests only: no octave kept from the one before) */
    if (last >= 0) {
        int pv = last - 1, pp, turn = 0;
        while (pv >= 0 && ld_dg[pv] == LD_REST)
            pv--;
        for (pp = pv - 1; pp >= 0 && ld_dg[pp] == LD_REST; pp--)
            ;
        if (pp >= 0) {                           /* (a leap into the note before the ending: the ending turns back) */
            const int dd = ld_dg[pv] - ld_dg[pp], ds = ld_deg_raw(ld_dg[pv]) - ld_deg_raw(ld_dg[pp]);
            turn = dd >= 3 || dd <= -3 || ds >= 5 || ds <= -5 ? (dd > 0 ? -1 : 1) : 0;
        }
        if (ld_role != LR_CADENCE)              /* (the cadence's ending its own) */
            ld_dg[last] = (int8_t)ld_ending(ld_dg[last], ld_role == LR_ANSWER, pv >= 0 ? ld_dg[pv] : ld_dg[last], turn);
        if ((ld_role == LR_ANSWER && !keys) || ld_role == LR_CADENCE)
            ld_dur[last] = (uint8_t)(ld_dur[last] < 16 ? 2 * ld_dur[last] : 32);
    }
    for (i = 0; i < last && (cm != (t.cm & LD_CM) || (ld_role == LR_SEQ && ld_step) || aug); i++)   /* a held note
                                                  * a chord tone (a tension excepted: resolved a step down onto one), the
                                                  * motif over another kind of chord than it was made on, shifted, or its
                                                  * values doubled */
        if (ld_hangs(ld_dg, ld_dur, i, last, r, cm, 1))
            ld_dg[i] = (int8_t)(r + ld_snap(ld_dg[i] - r, cm));
    for (i = 0; i < ld_n; i++)                   /* (its range) */
        if (ld_dg[i] != LD_REST) {
            const int x = ld_deg_raw(ld_dg[i]);
            lo = x < lo ? x : lo;
            hi = x > hi ? x : hi;
        }
    while (hi + sh > 100 && lo + sh - 12 >= 36)  /* the whole phrase an octave down or up if the register would clip it
                                                  * (its shape kept, every note in the scale; a note folded alone only
                                                  * if the phrase can't fit) */
        sh -= 12;
    while (lo + sh < 36 && hi + sh + 12 <= 100)
        sh += 12;
    for (i = 0; i < ld_n; i++) {
        if (ld_dg[i] != LD_REST)
            ld_dg[i] = (int8_t)ld_fold(ld_dg[i] + sh / 12 * ns);
        ld_note[i] = (uint8_t)(ld_dg[i] == LD_REST ? 0 : ld_deg_raw(ld_dg[i]));
    }
    if (last >= 0)
        ld_end = ld_dg[last];
    ld_i = 0;
    ld_left = 0;
    en_lead_role = ld_role;
#ifdef EN_HOST
    en_lead_slot = ld_slot;
    en_lead_int = (uint8_t)(ld_int * 100.0f + 0.5f);
#endif
    en_lead_phr++;
}

static void cp_put(int d, int len)               /* an entry on the figure (its oldest let go if it's full, and a rest
                                                  * left first after it) */
{
    int i;
    while (cp_n && (cp_n >= CP_MAX || cp_dg[0] == LD_REST)) {
        for (i = 1; i < cp_n; i++)
            cp_dg[i - 1] = cp_dg[i], cp_du[i - 1] = cp_du[i];
        cp_n--;
    }
    cp_dg[cp_n] = (int8_t)d;
    cp_du[cp_n++] = (uint8_t)len;
}
/* (a key in the first half of a sixteenth is on the tick before it: that tick, counted for what came before, is its;
 * a key on the grid, a little early or late, is heard on the grid) */
static int cp_early(void) { return pulse >= 1 && pulse <= 3; }
static void cp_key(int d, int held)              /* a key the Lead plays from the keys (held: one held before it, let
                                                  * go by this one: legato), written on the figure */
{
    const int e = cp_early();
    ld_cap &= (uint8_t)~CP_HELD;
    if (!en_playing || !lead.on)                 /* (no grid to count it on, or the Lead silent: nothing written, the
                                                  * key held not the figure's: its quiet counts on) */
        return;
    if (cp_n && held) {
        if (e && cp_du[cp_n - 1])
            cp_du[cp_n - 1]--;
    } else if (cp_n) {                           /* (the breath since the last let go: a rest if a beat or more, else
                                                  * the note before's) */
        const int q = cp_q - (e && cp_q);
        if (q >= 4)
            cp_put(LD_REST, q);
        else
            cp_du[cp_n - 1] = (uint8_t)(cp_du[cp_n - 1] + q > 32 ? 32 : cp_du[cp_n - 1] + q);
    }
    cp_put(d, e);
    cp_q = 0;
    ld_cap |= CP_HELD;
}
static void cp_off(void)                         /* the key let go (in the first half of a sixteenth: the tick before
                                                  * it the quiet's) */
{
    if ((ld_cap & CP_HELD) && cp_n && cp_early() && cp_du[cp_n - 1])
        cp_du[cp_n - 1]--, cp_q = 1;
    ld_cap &= (uint8_t)~CP_HELD;
}
/* the figure on the keys taken as the theme (now when they've been quiet two beats; an explicit REC could call it as
 * well): its last 4 bars at most, 3 notes or more, else let go (a gesture). Its degrees from the chord's root now, its
 * kind of chord now; into slot 0 (never a fresh draw's; heard 1, the player's: varied not before its third hearing).
 * The pair as if the Lead had just played it as its call (a block of one pair on slot 0, the block's Memory now): its
 * answer due after the breath inside a pair, the keys' (CP_ANS: lead_plan). Returns 1 if taken */
static int ld_take(void)
{
    ld_motif_t *m = &ld_bank[0];
    const int r = ld_cdeg(0);
    int i, k = cp_n, tot = 0, c = 0;
    for (i = 0; i < cp_n; i++)
        cp_du[i] = (uint8_t)(cp_du[i] ? cp_du[i] : 1);   /* (a key let go before a tick: a sixteenth) */
    while (k > 0 && tot + cp_du[k - 1] <= 64)
        tot += cp_du[--k];
    while (k < cp_n && cp_dg[k] == LD_REST)
        k++;
    for (i = k; i < cp_n; i++)
        c += cp_dg[i] != LD_REST;
    if (c >= 3) {
        for (i = k; i < cp_n; i++) {
            m->off[i - k] = (int8_t)(cp_dg[i] == LD_REST ? LD_REST : cp_dg[i] - r);
            m->dur[i - k] = cp_du[i];
        }
        m->n = (uint8_t)(cp_n - k);
        m->heard = 1;
        m->cm = (uint16_t)(ld_cmask() | LD_PLAYED);
        cp_r = (int8_t)r;
        ld_next = -1;
        ld_slot = 0;                             /* (the call was the player's) */
        ld_blen = ld_pair = 1;
        ld_step = ld_sent = ld_aug = ld_since = ld_cb = 0;
        ld_mem = (uint8_t)par[P_LMEM];
        ld_role = LR_CALL;
        ld_due = 1;
        ld_gap = (uint8_t)(ld_int < 0.3f ? 4 : 3);   /* (as after a call: its answer 1-2 beats on) */
        ld_cap = (uint8_t)((ld_cap & ~CP_NEXT) | CP_ANS);
        ld_prep = 0;                             /* (the theme's draws, before the first block: none needed now; after
                                                  * it, the idea thought of ahead: its offspring thought of instead) */
        ld_idea = 0;                             /* (the line's idea let go: after the keys' answer, at any Memory,
                                                  * its first offspring) */
        ld_kin = 0;                              /* (its offspring's clock, from now) */
    }
    cp_n = cp_q = 0;
    return c >= 3;
}

static void lead_note(int n)                     /* the next note: legato (it glides there), or from silence */
{
    const int legato = lead.on;
    lead_off();
    if (!ld_audible())
        return;
    if (!legato && lead.env < 0.05f)
        lead.cur = (float)n;                     /* (from silence: no glide) */
    if ((unsigned)par[P_LVOICE] < EN_NBV && par[P_LVOICE] >= BV_FM0) {   /* (an FM voice: its recipe from the top;
                                                  * legato, it glides on as it is) */
        if (!legato)
            fm_start(&lead.fm, BV_FM_REC[par[P_LVOICE] - BV_FM0], n);
    } else                                       /* (another voice: no FM note, so turning back onto an FM voice
                                                  * starts one: render_lead) */
        lead.fm.rec = FM_NREC;
    lead.note = (uint8_t)n;
    lead.on = 1;
    lead.vt = 0.0f;
    lead.fenv = 1.0f;
    hit(2, 0.8f);
    midi(0x92, (uint32_t)n, 90);
}

static void lead_tick(void)                      /* a sixteenth: the phrase goes on, or the breath */
{
    int planned = 0;
    if (ar_cue) {                                /* the arc's moment for the theme (dropped, not kept, before Lead in; under
                                                  * Memory 20, the wandering end: a fresh motif every pair, but for a
                                                  * theme the player played; and while the theme is playing, its answer
                                                  * too) */
        ar_cue = 0;
        if (ar_sec != EN_SEC_EMERGE && ar_bar >= par[P_LDIN] &&
            ((ld_blen ? ld_mem : par[P_LMEM]) >= 20 || (ld_bank[0].cm & LD_PLAYED)) &&
            !(ld_slot == 0 && ld_blen && (ld_pair < ld_blen || ld_due == 1 || ld_n)))
            ld_cb = 1;
    }
    if (!(sixteenth & 15u) && ld_kin != 255 && (ld_kin++, !ld_kinship()))
        ld_kin = 255;                            /* (the offspring's clock: a bar on; their time over, for good, till
                                                  * the keys play again) */
    if (cp_n) {                                  /* the figure on the keys: the key held a sixteenth longer, or a
                                                  * sixteenth more of quiet after it (two beats: taken, or let go) */
        if (ld_key >= 0 && (ld_cap & CP_HELD))
            cp_du[cp_n - 1] = (uint8_t)(cp_du[cp_n - 1] < 32 ? cp_du[cp_n - 1] + 1 : 32);
        else if (++cp_q >= 8)
            ld_take();
    }
    if (ld_key >= 0)                             /* (the keys play it: its own phrases wait) */
        return;
    if (!ld_n && ld_due == 1) {                  /* inside a pair (or a sentence): the next phrase on the first beat
                                                  * after its breath */
        if (ld_gap)
            ld_gap--;
        else if (!(sixteenth & 3u))
            lead_plan(), planned = 1;
    } else if (!(sixteenth & 15u) && !ld_n) {    /* a bar line, between pairs */
        if (ld_rest)
            ld_rest--;
        else if (ar_sec != EN_SEC_EMERGE && ar_bar >= par[P_LDIN] && (ld_blen || ld_prep >= 4 || ld_next >= 0) &&
                 (ld_due || ld_cb || seq_rand(&ld_rng) % 100u < (uint32_t)(20 + 6 * pe(P_LDENS) / 10) /
                                                                   (ar_sec == EN_SEC_FLOW ? 3u : 1u)))
            lead_plan(), planned = 1;            /* (the block's next pair, or the theme due, at once; a block not
                                                  * before Lead in bars from the piece's start, never in Emerge, nor
                                                  * before the theme's been thought of: its 4 draws, a bar each; Solo and
                                                  * Bridge its own, Flow now and then) */
    }
    if (!(sixteenth & 15u) && !planned)          /* (a bar line with nothing planned: the next motif thought of) */
        ld_think();
    if (!ld_n)
        return;
    if (ld_left) {
        ld_left--;
        if (ld_left)
            return;
    }
    if (ld_i < ld_n) {
        if (ld_dg[ld_i] == LD_REST) {
            lead_off();                          /* (a rest: the note let go, silence its length) */
        } else {
            lead_note(ld_note[ld_i]);
            en_lead_deg = lead.on ? ld_dg[ld_i] : -1;
        }
        ld_left = ld_dur[ld_i++];
        return;
    }
    lead_off();                                  /* the phrase done: after a call its answer, after a sentence's phrase
                                                  * its next, on the first beat a beat on (a little more when calm: 1 to
                                                  * 2 beats of breath, at every tempo); after an answer or the cadence a
                                                  * breath of (1 + 4 (1 - intensity)) (1.5 - Phrases) bars, then the
                                                  * block's next pair (or the theme due); after a block's last, 1 or 2
                                                  * bars more, then the roll */
    ld_n = 0;
    if (ld_role == LR_CALL || ld_role == LR_SEQ || ld_role == LR_FRAG) {
        ld_due = 1;
        ld_gap = (uint8_t)(ld_int < 0.3f ? 4 : 3);
    } else {
        const int g = (int)((1.0f + 4.0f * (1.0f - ld_int)) * (1.5f - (float)pe(P_LDENS) * 0.01f) + 0.5f);
        if (ld_cap & CP_NEXT) {                  /* (after the keys' answer: the next block, its first offspring, at the
                                                  * bar line after a breath, not on the roll) */
            ld_cap &= (uint8_t)~CP_NEXT;
            ld_due = 2;
            ld_rest = (uint8_t)g;
        } else {
            ld_due = (uint8_t)(ld_pair < ld_blen || ld_cb ? 2 : 0);
            ld_rest = (uint8_t)(ld_due ? g : g + 1 + (int)(seq_rand(&ld_rng) & 1u));
        }
    }
}
/* the phrase in flight ended now (the scale or the key turned: its notes were rendered in the old one): the note let
 * go (not a key's), the rest not played; on the next tick the phrase is done as usual (after a call, its answer, in
 * the new scale) */
static void ld_cut(void)
{
    if (ld_key < 0)
        lead_off();
    if (ld_i < ld_n || ld_left) {
        ld_n = ld_i;
        ld_left = (uint8_t)(ld_left ? 1 : 0);
    }
}

/* ------------------------------------------------------- the bar line --- */
static void bar_start(void)
{
    int pb = (int)(bar & 7u), db = en_drift_bars(par[P_DRIFT]);
    en_phrase_bar = (uint8_t)pb;                 /* (eight bars: the screen's, and Evolve's nudges on the first) */
    if (cr_dbars > 0)                            /* (the noise's drifts: their next targets on a barline) */
        cr_dbars--;
    if (pb == 0 && bar > 0 && !lock)             /* (Lock: the noise's groove kept) */
        noise_pattern();                         /* (the noise's groove: new, or the last one again) */
    if (!en_frozen && (change_pending || (db && bar > 0 && chord_bars >= (uint32_t)db))) {
        change_pending = 0;
        chord_change();
    }
    en_drift = db ? fm_clampf((float)chord_bars / (float)db, 0.0f, 1.0f) : 0.0f;
    chord_bars++;
    bar++;
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static void tick(void)
{
    int st = (int)(sixteenth & 15u), d;
    if (st == 0)
        bar_start();
    noise_step((int)(sixteenth & 31u));         /* (the noise's groove: two bars) */
    for (d = 0; d < EN_NDRUM; d++)
        en_drum_pos[d] = (uint8_t)st;
    seq_tick();
    lead_tick();
#ifdef EN_HOST
    if (ld_draws > en_lead_dmax)                 /* (the draws this tick; any made between ticks counted with it) */
        en_lead_dmax = ld_draws;
    ld_draws = 0;
#endif
    sixteenth++;
    en_bars = sixteenth;
}

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in FoMni): a predelay, four input diffusers,
 * then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping lowpass, an
 * allpass and a delay, feeding the other half. Stereo out from seven taps a side. The paper's lengths
 * are for 29761 Hz; DS() scales them to 44.1 kHz. */
/* (the classics pass: the plate at half the rate, 22.05 kHz: a quarter of the engine's time halved, its memory too;
 * nothing of it above ~10 kHz was heard: its input band-limited, its damping) */
#define DS(n) (((n) * 7409 + 5000) / 10000)           /* 22050 / 29761 */
#define PRE_N 662                                    /* 30 ms */
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_decay_t = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;
static float pl_acc, pl_ol, pl_or;               /* (the plate at half the rate: the last input, its last output) */
static uint32_t pl_ph;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}

static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += 0.84f * (x - pl_bw);                /* (0.6 at the full rate) */
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0002f * lfo_c, c = lfo_c - 0.0002f * s;   /* (the same speed at half the rate) */
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.64f * (b - pl_damp_l);        /* (0.4 at the full rate) */
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += 0.64f * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}

static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------ Shimmer: the plate climbing --- */
/* (the voices batch, item 0) Shimmer, the grid's fourth and the fifth black key (where Swell was): while held, the
 * plate's tail pitched an octave up and fed back into the plate, so each time round it climbs another octave: a
 * shimmer reverb. The shifter is the classic rotating delay, after Felucca's OCT UP and its SHIMMER (perform.c, Leo
 * Kuroshita, Hügelton Instruments, GPL-3.0-only; Skiff's too), written again in float: two taps of a short line,
 * half a sweep apart, each coming a sample nearer every sample (the pitch doubled) from ~49 ms back to ~3 ms, then
 * jumping back; a raised-cosine crossfade hides each one's jump (its weight 0 there; the curve a smoothstep, near a
 * raised cosine, the two always summing to one); read between samples, linear. It runs inside the plate's loop,
 * mono (as the plate's input is) and at a quarter of the rate, 11.025 kHz (every other plate sample: its input
 * low-passed at ~2.5 kHz first, one pole, then the mean of two, so less folds over; its output held for two):
 * 1024 floats from the pool. After it two one-pole low-passes (~2.2 kHz: the FM-1's speaker is bright) darken each
 * climb, and a gain under one holds it: the third octave well under the first.
 * (fix round 1: the review) A gain under one isn't enough: the plate gathers what goes in, and at Size ~70 and over
 * the loop fed itself until the master's clip held it (Mirage as it is, then the music stopped: it rang on for ever).
 * So the return is bounded by the music: what goes back in is never more than SHM_R times what the music sends into
 * the plate (their levels followed once a chunk: the music's up in ~50 ms and down over ~1 s, the return's both ways
 * in ~0.1 s), and the plate holds at most about that much more than the music gives it. The music stopped, the
 * return falls with it and the tail dies as the plate's own; a soft clip after it is the last guard. Most of the
 * runaway was under the music (the crossfade's ~21 Hz and below, gathered by the plate): two high-pass poles at
 * ~150 Hz on the return take it out.
 * (fix rounds 2, 3) The feedback is less on a long plate (SHM_GS: the plate gathers more the longer it rings), so a
 * long plate at Size 100 doesn't jump; the dry into the plate is a fixed share (SHM_DRY: round 2 tied it to the
 * piece's own send, and Shimmer all but vanished in Sunrain: put back).
 * Let go, the feedback eases out (~0.15 s) and what is in the plate dies with it, not cut; once out, the shifter
 * stops (no work at rest), and the next press starts from an empty line (the taps read nothing older than the press;
 * its first samples faded in: no step where they begin) */
#define SHM_N 1024
#define SHM_LO 32.0f                            /* the taps' shortest delay, samples at 11.025 kHz; the sweep 512 */
#define SHM_SPAN 512.0f
#define SHM_FADE 32u
#define SHM_G 0.7f                              /* the feedback: an octave's pass under the last, */
#define SHM_GS 3.5f                             /* (and less on a long plate: at most this x (1 - decay^2), the plate's
                                                 * gathering: 0.7 to Size ~87, 0.63 at Mirage's 90, 0.34 at 100) */
#define SHM_HPC 0.0846f                         /* (1 - e^(-2 pi 150 / 11025)) */
#define SHM_R 1.0f                              /* the return at most this times the music into the plate (levels) */
#define SHM_DRY 0.25f                           /* (held, the dry into the plate too: something to climb; the one
                                                 * place it is set, for the user's ear) */
static float shm_buf[SHM_N] EN_POOL;
static uint32_t shm_w, shm_len, shm_odd, shm_nm, shm_ns;
static float shm_ph, shm_in, shm_out, shm_p1, shm_lp1, shm_lp2, shm_hp1, shm_hp2, fx_shm;
static float shm_em, shm_es, shm_envm, shm_envs, shm_k = SHM_G;   /* (the levels: sums this chunk, followed; the gain) */
static inline float shm_tap(float d)            /* d samples back from the newest, between two; 0 before the press */
{
    const uint32_t i = (uint32_t)d;
    const float f = d - (float)i;
    float a, b;
    if (i + 1u >= shm_len)
        return 0.0f;
    a = shm_buf[(shm_w - i) & (SHM_N - 1u)];
    b = shm_buf[(shm_w - i - 1u) & (SHM_N - 1u)];
    return a + (b - a) * f;
}
static inline float shimmer(float x, float g)   /* one sample at the plate's rate: x its last output; out what goes
                                                 * back into it (g: the feedback now) */
{
    float t, wa, y;
    shm_p1 += 0.51f * (x - shm_p1);             /* (a pole at ~2.5 kHz, at 22.05 kHz: before the line's half rate) */
    if ((shm_odd ^= 1u) != 0u) {                /* (the first of two: kept, the last output held) */
        shm_in = shm_p1;
        return shm_out;
    }
    x = 0.5f * (shm_in + shm_p1);
    if (shm_len < SHM_FADE)
        x *= (float)shm_len * (1.0f / (float)SHM_FADE);
    shm_buf[shm_w & (SHM_N - 1u)] = x;
    if (shm_len < SHM_N)
        shm_len++;
    t = shm_ph < 0.5f ? 2.0f * shm_ph : 2.0f - 2.0f * shm_ph;   /* (tap A's share: 0 where it jumps back, at 0) */
    wa = t * t * (3.0f - 2.0f * t);
    y = wa * shm_tap(SHM_LO + SHM_SPAN * (1.0f - shm_ph)) +
        (1.0f - wa) * shm_tap(SHM_LO + SHM_SPAN * (shm_ph < 0.5f ? 0.5f - shm_ph : 1.5f - shm_ph));
    shm_w++;
    shm_ph += 1.0f / SHM_SPAN;                  /* (each tap a sample nearer a sample: twice the pitch) */
    if (shm_ph >= 1.0f)
        shm_ph -= 1.0f;
    shm_lp1 += 0.715f * (y - shm_lp1);          /* (two poles at ~2.2 kHz, at 11.025 kHz) */
    shm_lp2 += 0.715f * (shm_lp1 - shm_lp2);
    shm_hp1 += SHM_HPC * (shm_lp2 - shm_hp1);   /* (and the lows out, two poles at ~150 Hz: the crossfade's 21 Hz and */
    shm_hp2 += SHM_HPC * (shm_lp2 - shm_hp1 - shm_hp2);   /* what is under it would build in the plate unheard) */
    y = shm_lp2 - shm_hp1 - shm_hp2;
    shm_es += y * y;                            /* (the return's level, before its gain) */
    shm_ns++;
#ifdef EN_HOST
    en_shm_work++;
#endif
    shm_out = 0.5f * sat(2.0f * g * y);         /* (no more than +-0.5 back in) */
    return shm_out;
}
static void shm_level(float t)                  /* once a chunk (t: its length, s): the levels followed, the gain */
{
    float m = shm_nm ? shm_em / (float)shm_nm : 0.0f, s = shm_ns ? shm_es / (float)shm_ns : shm_envs, g;
    shm_envm += (m - shm_envm) * fm_minf(t / (m > shm_envm ? 0.05f : 1.0f), 1.0f);
    shm_envs += (s - shm_envs) * fm_minf(t / 0.1f, 1.0f);
    shm_envm = fm_flush(shm_envm), shm_envs = fm_flush(shm_envs);
    g = fm_minf(SHM_G, SHM_GS * (1.0f - pl_decay * pl_decay));
    shm_k = shm_envs * g * g > SHM_R * SHM_R * shm_envm ? SHM_R * fm_sqrtf(shm_envm / shm_envs) : g;
#ifdef EN_HOST
    shm_k *= en_shm_ret;                         /* (host tests: 0, the dry's lift alone, nothing back in) */
#endif
    shm_em = shm_es = 0.0f;
    shm_nm = shm_ns = 0;
    shm_p1 = fm_flush(shm_p1);                   /* (the one-poles' tails, as the engine's others) */
    shm_lp1 = fm_flush(shm_lp1), shm_lp2 = fm_flush(shm_lp2), shm_hp1 = fm_flush(shm_hp1), shm_hp2 = fm_flush(shm_hp2);
}
static void shm_stop(void)                      /* out: the line empty, from the top */
{
    fx_shm = 0.0f;
    shm_len = shm_w = shm_odd = shm_nm = shm_ns = 0;
    shm_ph = shm_in = shm_out = shm_p1 = shm_lp1 = shm_lp2 = shm_hp1 = shm_hp2 = 0.0f;
    shm_em = shm_es = shm_envm = shm_envs = 0.0f;
    shm_k = SHM_G;
}
#ifdef EN_HOST
static int fin(float x) { return x == x && fm_fabsf(x) < 1e30f; }
int en_state_ok(void)                           /* (host tests) the plate's and the shifter's state finite */
{
    return fin(pl_ol) && fin(pl_or) && fin(pl_acc) && fin(pl_damp_l) && fin(pl_damp_r) && fin(shm_out) &&
           fin(shm_lp1) && fin(shm_lp2) && fin(shm_hp1) && fin(shm_hp2) && fin(fx_shm);
}
#endif

/* ------------------------------------------------------------ echo: tape --- */
/* The dub echo: a tape loop (up to ~5.6 s, synced to the tempo) at a quarter rate (11 kHz, 16-bit,
 * mono), with two heads: the first plays the loop's length, the second a part of it (3/4 at first),
 * each panned to its side. In the feedback, a band-pass (each repeat thinner and darker) that the sweep
 * and a slow drift move, and a soft clip: the feedback may pass 100% and the echo swell on itself without
 * running away. With Motion the heads wander, the feedback leans now on one, now on the other, and now
 * and then the second slides to another part of the loop (the repeats bend in pitch on the way). A
 * thrown stab pushes the feedback up a little. Frozen (dub hold), the loop stops listening and plays
 * what it holds, sample for sample, for ever. */
#define TAPE_N 62000                             /* 5.6 s at 11.025 kHz */
#define TAPE_SR 11025.0f
static int16_t tape[TAPE_N] EN_POOL;
static int tape_w, tape_ph;
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_c1, tape_p3, tape_c3;   /* (3: the right's, a
                                                 * few ms behind the left: Width) */
static float hg[2] = {1.0f, 0.0f};              /* the first head's pan gains: left, right */

static inline float tape_read(float d)           /* d quarter-rate samples ago */
{
    int k = (int)d, i0, i1;
    float f = d - (float)k, a, c;
    i0 = tape_w - k;
    i1 = i0 - 1;
    if (i0 < 0)
        i0 += TAPE_N;
    if (i1 < 0)
        i1 += TAPE_N;
    a = (float)tape[i0];
    c = (float)tape[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void echo_time(void)
{
    int i = par[P_ETIME];
    float beat, t;
    if (par[P_TEMPO] <= 0)
        return;                                  /* not yet (en_init applies in order) */
    beat = 60.0f / (float)par[P_TEMPO];
    t = ETIME_BEATS[i] * beat;
    if (t * TAPE_SR > (float)(TAPE_N - 8))
        t = (float)(TAPE_N - 8) / TAPE_SR;
    echo_len_t = t * TAPE_SR;
    if (echo_len <= 0.0f)
        echo_len = echo_len_t;
}

/* once a chunk: where the head sits, the band-pass */
static void heads_step(void)
{
    float p, sw;
    /* (1.3.0) the loop's gain from the tail's length: whatever the echo's time, the repeats are gone (-60 dB) in
     * Feedback's seconds; long delays vanish as quickly as short ones (Dub and the throws still push past it) */
    echo_fb = fm_exp2f(-9.966f * (echo_len_t * (1.0f / TAPE_SR)) / echo_t60);
    /* (1.3.0: the first head near the middle, drifting with Motion; Width spreads it: 0 mono, 2 as wide as before) */
    p = fm_clampf(ec_wide * (0.0f + 0.25f * (0.4f + 0.6f * motion) * mod[1].v), -1.0f, 1.0f);
    p = (p + 1.0f) * (FM_PI / 4.0f);
    hg[0] = fm_cosf(p);
    hg[1] = fm_sinf(p);
    /* the band-pass: ~220..800 Hz below (1.3.0: from ~60..500, so the repeats thin; the classics pass: from ~165..700), ~1.1..2.6 kHz above, moving
     * up together */
    dub_up = 0.0f;                               /* (1.3.0: PERFORM's second knob is Wet now, not Dub) */
    sw = fm_clampf(0.45f + 0.3f * perf_sw + 0.5f * motion * mod[0].v - 0.3f * dub_up, 0.0f, 1.0f);   /* (Dub up: darker) */
    tape_hpc = 0.12f + 0.24f * sw * sw;          /* (the classics pass: ~220 Hz at the bottom, was ~165) */
    tape_lpc = 0.58f + 0.3f * sw;
}

/* one sample: the input (a mono send) to the tape's heads (el, er) */
/* 1.3.0, the echo dissolving (build/research/dubtechno/REPORT2.md, variant D): what enters the tape band-limited (Tone),
 * each repeat through five allpasses in the feedback (Blur: their gain), so a repeat comes back softer and darker
 * than the last; one head (the second only while a throw rings), the Echoplex's short taps out of the mix. (The
 * classics pass, REPORT9: the first repeat clean, a copy of the strike on its time; the blur grows with each pass,
 * the dub way. The input's allpasses moved into the feedback: they smeared even the first.) At the tape's rate: the
 * heads read where the strike comes back (the output) and, the allpasses' delay earlier, what goes round again */
#define EAP_FB 994                               /* (37 + 89 + 151 + 276 + 441: 3 to 40 ms) */
static int16_t eap[EAP_FB] EN_POOL;             /* (int16 at 1/8000: +-4) */
static int eap_i[5];
static float ec_lp, ec_hp;
static inline float eapf(int k, int off, int n, float x)
{
    int16_t *b = eap + off;
    float d = (float)b[eap_i[k]] * (1.0f / 8000.0f), y = d - ec_g * x, t = (x + ec_g * y) * 8000.0f;
    b[eap_i[k]] = (int16_t)(t > 32767.0f ? 32767.0f : t < -32767.0f ? -32767.0f : t);
    if (++eap_i[k] >= n)
        eap_i[k] = 0;
    return y;
}
static inline float ec_in(float x)
{
    ec_lp += ec_lpc * (x - ec_lp);
    ec_hp += 0.18f * (ec_lp - ec_hp);            /* (~350 Hz: the repeats above the chord's low mids) */
    return ec_lp - ec_hp;
}
static inline float ec_fb(float x)
{
    return eapf(4, 553, 441, eapf(3, 277, 276, eapf(2, 126, 151, eapf(1, 37, 89, eapf(0, 0, 37, x)))));
}

static inline void echoes(float in, float *el, float *er)
{
    tape_acc += in;
    if (++tape_ph >= 4) {                        /* the tape, a quarter of the time */
        float d, h1, h3, f1, fb, w;
        tape_ph = 0;
        echo_len += (echo_len_t * fx_em - echo_len) * fx_ek;   /* (the grid: the time doubled, the tape slowing) */
        if (echo_len > (float)(TAPE_N - 8))      /* (Tape stop slows a long echo past the tape: it stops at its end;
                                                  * the hold below reads echo_len itself. Reported by genkigenki) */
            echo_len = (float)(TAPE_N - 8);
        d = echo_len * (1.0f + 0.002f * motion * mod[0].v);   /* wow */
        if (d > (float)(TAPE_N - 4))
            d = (float)(TAPE_N - 4);
        /* (BERLIN) ping-pong, from the one tape (Schulze's Revox): what goes round is read at twice the time, so the
         * repeats at d, 3d .. come out on the left (h1) and those at 2d, 4d .. on the right (h3); while twice the
         * time fits the tape, else the dub's way (the right a little later) */
        ec_pp = 2.0f * d < (float)(TAPE_N - 8);
        h1 = tape_read(d);                       /* (the heads: what comes out, on its time) */
        h3 = ec_pp ? tape_read(2.0f * d)
                   : tape_read(fm_minf(d + 88.0f * fm_minf(ec_wide, 2.0f), (float)(TAPE_N - 4)));
        f1 = tape_read(fm_maxf((ec_pp ? 2.0f * d : d) - (float)EAP_FB, 2.0f));   /* (and what goes round, the
                                                  * diffusers' delay earlier) */
        if (en_frozen) {                         /* dub hold: the loop, as it is */
            int k = (int)echo_len, i0 = tape_w - k;
            int16_t s = tape[i0 < 0 ? i0 + TAPE_N : i0];
            if (++tape_w >= TAPE_N)
                tape_w = 0;
            tape[tape_w] = s;
        } else {
            const float nova = fx_nova;          /* (the grid's Supernova) */
            float g = fm_minf(echo_fb + 0.12f * perf_echo + 0.1f * dub_up + 0.35f * nova,
                              fb_cap + 0.3f * nova);   /* (Dub: past the knob; Supernova, past full) */
            if (ec_pp)
                g *= g;                          /* (two repeats a turn round: the feedback squared) */
            tape_lp += tape_lpc * (f1 - tape_lp);   /* each time round: thinner, darker */
            tape_hp += tape_hpc * (tape_lp - tape_hp);
            fb = ec_fb(tape_lp - tape_hp);       /* (and more dissolved) */
            w = ec_in(tape_acc * 0.25f) + 0.7f * sat(fb * g * (1.0f / 0.7f));
            if (++tape_w >= TAPE_N)
                tape_w = 0;
            tape[tape_w] = s16(w);
        }
        tape_acc = 0.0f;
        tape_p1 = tape_c1;
        tape_p3 = tape_c3;
        tape_c3 = h3;
        tape_c1 = h1;
    }
    {   /* between the tape's samples: a line from the last to the next; then to the field */
        float f = (float)tape_ph * 0.25f;
        float h1 = tape_p1 + (tape_c1 - tape_p1) * f;
        float h3 = tape_p3 + (tape_c3 - tape_p3) * f;
        if (ec_pp) {                             /* (Width: 0 every repeat both sides, from 50 side to side) */
            const float x = 1.0f - fm_minf(ec_wide, 1.0f);
            *el = hg[0] * (h1 + x * h3);
            *er = hg[1] * (h3 + x * h1);
        } else {
            *el = hg[0] * h1;
            *er = hg[1] * (h1 + fm_minf(0.6f * ec_wide, 1.0f) * (h3 - h1));   /* (the later head blended in: 60% at
                                                  * Width 50, all from 84) */
        }
    }
}

/* ------------------------------------------------- cloud: a granular reverb --- */
/* After JIANT's CLOUD (github.com/juanjiant-bit/JIANT, GPL-3.0): what goes to the reverb, written at a
 * quarter rate (11 kHz, 16-bit) into a 0.74 s line, read by up to six grains born at random (denser as Cloud
 * turns up and as the filter opens), each 80..250 ms through a triangle window, from anywhere behind the
 * write, panned at random, a few cents off; now and then an octave down (rarely up). The grains go into the
 * plate, so they bloom into its tail, and a little of them back into the line: the wash builds on itself.
 * Dub hold stops the line too: the grains keep scattering over what it holds. */
#define CL_LEN 8192
#define CL_NG 6
static int16_t cl_buf[CL_LEN];                  /* (RAM, not the pool: the pool is nearly full) */
static int cl_w, cl_ph, cl_live;
static int32_t cl_next;
static float cl_acc, cl_fb, cl_pl, cl_pr, cl_cl, cl_cr, cl_dens, pl_back;
static struct {
    float pos, inc, gl, gr;
    int32_t age, len;
} cl_g[CL_NG];

static uint32_t cl_rng = 0x2545F491u;          /* its own: the Cloud knob leaves the music's choices be */
static float cl_frand(void)
{
    cl_rng = cl_rng * 1664525u + 1013904223u;
    return (float)(cl_rng >> 8) * (1.0f / 16777216.0f);
}
static int cl_irand(int n) { return (int)(cl_frand() * (float)n); }

static void cloud_birth(void)                   /* a grain, in the first voice free */
{
    int k, back;
    float r = cl_frand(), pk, p;
    for (k = 0; k < CL_NG && cl_g[k].len; k++)
        ;
    if (k >= CL_NG)
        return;
    pk = r < 0.2f ? 0.5f : r < 0.24f ? 2.0f : 1.0f;
    cl_g[k].len = 880 + cl_irand(1900);
    cl_g[k].inc = pk * (0.997f + 0.006f * cl_frand());
    back = 300 + (pk > 1.0f ? cl_g[k].len : 0) + cl_irand(CL_LEN - 3200 - (pk > 1.0f ? cl_g[k].len : 0));
    cl_g[k].pos = (float)((cl_w - back + CL_LEN) % CL_LEN);
    cl_g[k].age = 0;
    p = (cl_frand() * 1.6f - 0.8f + 1.0f) * (FM_PI / 4.0f);
    cl_g[k].gl = fm_cosf(p);
    cl_g[k].gr = fm_sinf(p);
    cl_live++;
}

/* one sample: the input (mono) in, the grains out (l, r) */
static inline void cloud(float in, float *ol, float *or_)
{
    cl_acc += in;
    if (++cl_ph >= 4) {                         /* the line and the grains, a quarter of the time */
        float l = 0.0f, r = 0.0f;
        int k;
        cl_ph = 0;
        if (!en_frozen && !(fxm >> EN_FX_FREEZE & 1u)) {   /* (the grid's Freeze: the cloud alone holds) */
            if (++cl_w >= CL_LEN)
                cl_w = 0;
            cl_buf[cl_w] = s16(cl_acc * 0.25f + (0.25f + 0.3f * cloud_amt) * cl_fb);
        }
        cl_acc = 0.0f;
        if (--cl_next <= 0) {
            int gap = 1500 - (int)(1100.0f * cl_dens);   /* 136 .. 36 ms apart, on average */
            cl_next = gap / 2 + cl_irand(gap);
            if (cl_dens > 0.0f && !SHED)
                cloud_birth();
        }
        if (cl_live) {
            for (k = 0; k < CL_NG; k++) {
                int32_t a = cl_g[k].age, L = cl_g[k].len;
                int i0;
                float f, x, w;
                if (!L)
                    continue;
                i0 = (int)cl_g[k].pos;
                f = cl_g[k].pos - (float)i0;
                x = (float)cl_buf[i0] + ((float)cl_buf[i0 + 1 < CL_LEN ? i0 + 1 : 0] - (float)cl_buf[i0]) * f;
                w = (float)(a < L - a ? a : L - a) * (2.0f / (float)L);
                x *= w * (1.0f / 12000.0f);
                l += x * cl_g[k].gl;
                r += x * cl_g[k].gr;
                cl_g[k].pos += cl_g[k].inc;
                if (cl_g[k].pos >= (float)CL_LEN)
                    cl_g[k].pos -= (float)CL_LEN;
                if (++cl_g[k].age >= L) {
                    cl_g[k].len = 0;
                    cl_live--;
                }
            }
        }
        l *= 0.55f;
        r *= 0.55f;
        cl_fb = 0.5f * (l + r);
        cl_pl = cl_cl;
        cl_pr = cl_cr;
        cl_cl = l;
        cl_cr = r;
    }
    {   /* between the quarter-rate samples: a line from the last to the next */
        float f = (float)cl_ph * 0.25f;
        *ol = cl_pl + (cl_cl - cl_pl) * f;
        *or_ = cl_pr + (cl_cr - cl_pr) * f;
    }
}

/* ------------------------------------------------------------ parameters --- */
/* (1.3.0) an LFO synced to the bar, a sine from the grid's place, eased: the noise's (lfo_n), its rings' tone over a
 * bar, +-1 octave at 100. (BERLIN: the stabs' filter LFO, lfo_c, is gone; its name was the plate's modulation
 * oscillator's too, and its line here damped the plate's modulation every chunk: the plate is its own again) */
static float lfo_n;
static void lfo_step(void)
{
    const float pos = (float)(sixteenth & 63u) + (float)pulse * (1.0f / 6.0f);   /* (four bars of sixteenths) */
    lfo_n += (0.67f * (float)par[P_NLFO] * 0.01f * fm_sinf(6.2831853f * pos * (1.0f / 16.0f)) - lfo_n) * 0.2f;
}
static uint32_t ext_age;

static void tempo_set(void) { step_len = EN_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f); }

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    v = pe(p);                                   /* (as the master actions move it) */
    switch (p) {
    case P_CHORD:
        chord_vol = (float)(v * v) * 0.0001f;
        if (!v)                                  /* (at 0, as muted: its MIDI notes let go) */
            dr_midi_off(0), dr_midi_off(1);
        break;
    case P_SEQ: seq_vol = (float)(v * v) * 0.0001f; break;
    case P_LEAD: lead_vol = (float)(v * v) * 0.0001f; break;
    case P_GLIDE: ld_gk = 1.0f - fm_expf(-1.0f / ((0.001f + 0.0015f * (float)v) * EN_SR)); break;   /* ~1 .. 150 ms (the
                                                  * user: 37 .. 49 ms sounds right, more is a theremin) */
    case P_SEQ2: seq2_vol = (float)(v * v) * 0.0001f; break;
    case P_SEQLEN: seq_resize(0, v); break;
    case P_SEQ2LEN: seq_resize(1, v); break;
    case P_ECHO: echo_send = (float)(v * v) * 0.0001f; break;
    case P_ETIME: echo_time(); break;
    case P_REVERB: rev_send = (float)(v * v) * 0.0001f; break;
    case P_FEEDBACK: echo_t60 = 1.5f * fm_exp2f((float)v * (1.0f / 30.0f)); break;   /* the tail: 1.5 s .. 15 s (heads_step) */
    case P_SIZE: pl_decay_t = 0.5f + 0.0045f * (float)v; break;      /* to 0.95 */
    case P_CUTOFF:                               /* 110 Hz .. 5.4 kHz: the plucks' (the sequences) */
        sq_cut = 110.0f * fm_exp2f((float)v * 0.055f);
        break;
    case P_DECAY: case P_CVOICE:                 /* the Drone's voice; the plucks' length */
        cvoice = (unsigned)par[P_CVOICE] < EN_NCV ? par[P_CVOICE] : 0;
        {   /* the plucks: the filter's envelope 80 .. 250 ms to -60 dB, the sound half as long again */
            const float t = 0.08f * fm_exp2f((float)pe(P_DECAY) * 0.0164f);
            sq_edec = fm_expf(-6.9f / (t * EN_SR));
            sq_adec = fm_expf(-6.9f / (1.5f * t * EN_SR));
        }
        break;
    case P_MOTION:
        motion = (float)v * 0.01f;
        if (!v)
            en_s2oct_shift = 0;                  /* (Motion 0: Flow for ever, Sequence 2 home) */
        break;
    case P_SEQ2OCT: en_s2oct_shift = 0; break;   /* (the hand wins: its Octave, home, the shift gone) */
    case P_EVOLVE: evolve = (float)v * 0.01f; break;
    case P_CRACKLE:                              /* (1.3.0: straight, not squared: a bed at 15 too; 35 as before,
                                                  * 100 -9 dB; the palette widening as it turns up) */
        crackle_vol = (float)v * 0.0035f;
        break;
    case P_CLOUD: cloud_amt = (float)v * 0.01f; break;
    case P_CR0: case P_CR1: case P_CR2: case P_CR3: case P_CR4: case P_CR5: case P_CR6: case P_CR7:
    case P_CQ0: case P_CQ1: case P_CQ2: case P_CQ3: case P_CQ4: case P_CQ5: case P_CQ6: case P_CQ7:
    case P_CX0: case P_CX1: case P_CX2: case P_CX3: case P_CX4: case P_CX5: case P_CX6: case P_CX7:
        if (nsteps && prog_pos >= 0 && (p - P_CR0) % 8 == prog_pos)
            slot_play(prog_pos);                 /* the slot playing, edited: heard at once */
        prog_publish();
        break;
    case P_PMODE:                                /* (the walk's character, turned by hand: C_REWALK) */
        if (v & EN_PLOOP) {                      /* to Loop: the eight as they are, on from the one playing */
            if (prog_pos < 0 && prog_len())
                prog_pos = 0;
        } else {                                 /* Endless (from Loop, the UI turns them round: C_PROT) */
            walk_fill();
            if (prog_pos < 0)
                prog_pos = 0, slot_play(0);
        }
        en_prog_pos = (int8_t)prog_pos;
        prog_publish();
        break;
    case P_NCOL: nctl.colour = (float)v * 0.02f; break;   /* the noise's controls, -1..+1 (render_crackle) */
    case P_NGRAIN: nctl.grain = (float)v * 0.02f; break;
    case P_NROOM: nctl.room = (float)v * 0.02f; break;
    case P_NDRIFT: nctl.drift = (float)v * 0.02f; break;
    case P_NCHAR:                                /* (BERLIN: the Atmosphere's character; the dub's machine as Tape) */
        atm_char = (uint8_t)((unsigned)v < EN_NNCHAR ? v : 0);
        nctl.character = 1;
        break;
    case P_PHASER: ph_depth = (float)v * 0.01f; break;
    case P_PEDAL: dr_pedal = (float)v * 0.01f; break;
    case P_DRREV: dr_rev = (float)v * 0.02f; break;  /* (the sends: x1 at 50, x2 at 100) */
    case P_DRECHO: dr_echo = (float)v * 0.02f; break;
    case P_SQREV: sq_rev[0] = (float)v * 0.02f; break;
    case P_SQECHO: sq_echo[0] = (float)v * 0.02f; break;
    case P_S2REV: sq_rev[1] = (float)v * 0.02f; break;
    case P_S2ECHO: sq_echo[1] = (float)v * 0.02f; break;
    case P_LDREV: ld_rev = (float)v * 0.02f; break;
    case P_LDECHO: ld_echo = (float)v * 0.02f; break;
    case P_LTONE: ld_tone = fm_exp2f((float)(v - 50) * 0.04f); break;   /* (the Lead's filter, two octaves either way) */
    case P_LATT: ld_att = 1.0f - fm_expf(-1.0f / (0.005f * fm_exp2f((float)v * 0.0645f) * EN_SR)); break;   /* 5 .. 400 ms */
    case P_S2CUT: sq2_cut = 110.0f * fm_exp2f((float)v * 0.055f); break;
    case P_S2DEC: {
        const float t = 0.08f * fm_exp2f((float)v * 0.0164f);
        sq2_edec = fm_expf(-6.9f / (t * EN_SR));
        sq2_adec = fm_expf(-6.9f / (1.5f * t * EN_SR));
        break;
    }
    case P_CATT: dr_up = 1.0f / (0.5f * fm_exp2f((float)v * 0.0458f) * EN_SR); break;    /* a swell of 0.5 .. 12 s */
    case P_DREL: dr_down = 1.0f / (fm_exp2f((float)v * 0.039f) * EN_SR); break;         /* dying in 1 .. 15 s */
    case P_EBLUR:                                /* the diffusers' gain: 0.15 (clean copies) .. 0.6 (50) .. 0.75 */
        ec_g = v <= 50 ? 0.15f + 0.009f * (float)v : 0.6f + 0.003f * (float)(v - 50);
        break;
    case P_ETONE:                                /* what enters the tape: a low-pass ~2 kHz at 50, an octave a quarter */
        ec_lpc = 1.0f - fm_expf(-2.0f * FM_PI * fm_clampf(2000.0f * fm_exp2f((float)(v - 50) * 0.04f), 400.0f, 5000.0f) / TAPE_SR);
        break;
    case P_EWIDTH: ec_wide = (float)v * 0.02f; break;   /* the heads' spread: 0 mono, 1 (50), 2 wide */
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_TEMPO:
        tempo_set();
        echo_time();
        break;
    case P_ROOT:
        if (nsteps)
            voice_chord();
        ld_cut();                                /* (the Lead's phrase in flight was in the old key) */
        break;
    case P_MIDI:                                 /* off: every channel's notes off (the chord, pings, bass, drums) */
        if (!v) {
            int c;
            for (c = 0; c < 4; c++)
                en_midi_out(0xB0 | (uint32_t)c, 123, 0);
            en_midi_out(0xB9, 123, 0);
        }
        break;
    default: break;
    }
}

static int deg_dim(int d)                       /* the scale's chord on degree d has a diminished fifth */
{
    int t[3], n = stack(pool, 0, d, t, 3);
    return n >= 3 && (t[2] - t[0] + 12) % 12 == 6;
}
static void pool_set(uint32_t p)
{
    int k, prev = 0, moved = 0;
    pool = (p | 1u) & 0xFFFu;
    scale_set();
    ld_cut();                                    /* (the Lead's phrase in flight was in the old scale) */
    /* the walk's slots follow the scale, as degrees; one the new scale has no room for, or that lands on a
     * diminished chord (which the walk never plays), or repeats the slot before, is walked again from that one */
    for (k = 0; k < EN_NPROG && nsteps; k++)
        if (slot_walk(k)) {
            int raw = par[P_CR0 + k] - 1, d = raw % nsteps;
            if (raw >= nsteps || deg_dim(d) || (k > 0 && slot_walk(k - 1) && d == prev)) {
                d = walk_from(prev);
                slot_walk_set(k, d);
                moved = 1;
            }
            prev = walk_last = d;
            if (k == prog_pos)
                deg = d;
        }
    if (moved)
        prog_publish();
    if (deg >= nsteps)
        deg = 0;
    voice_chord();
}

static void regrow(void)                        /* a new piece: the home chord, fresh patterns */
{
    int k;
    chord_slot = -1;
    deg = 0;
    shape = cvoice == CV_STRINGS || cvoice == CV_SAWS || cvoice == CV_GLASS || cvoice == CV_FM0 ||
            cvoice == CV_FM0 + 1;               /* these like the 9th (Warm Air and FM Strings as Strings); the rest the
                                                 * open 7th (Soft Organ as Organ) */
    walk_shape = shape;
    if (!PLAY_LOOP)                              /* Endless: a fresh piece's eight, from the walk */
        walk_fill();
    /* the slots: Endless from slot 1, Loop from the first that isn't a gap (none: home) */
    for (prog_pos = 0; prog_pos < EN_NPROG && slot_gap(prog_pos); prog_pos++)
        ;
    if (prog_pos >= EN_NPROG || !PLAY_LOOP)
        prog_pos = prog_pos >= EN_NPROG ? -1 : 0;
    en_prog_pos = (int8_t)prog_pos;
    for (walk_last = 0, k = 0; k < EN_NPROG; k++)   /* (the walk goes on from its last) */
        if (slot_walk(k))
            walk_last = (par[P_CR0 + k] - 1) % (nsteps ? nsteps : 1);
    if (prog_pos >= 0)
        slot_play(prog_pos);
    if (chord_slot < 0 && (prog_pos < 0 || !slot_walk(prog_pos)))
        voice_chord();
    prog_publish();
    seq_regrow(0);
    seq_regrow(1);
    chord_bars = 0;
    change_pending = 0;
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < 4; i++)
        en_midi_out(0xB0 | (uint32_t)i, 123, 0);
    en_midi_out(0xB9, 123, 0);
}

static void quiet(void)
{
    int i;
    drone_off(0), drone_off(1);
    dr_lvl[0] = dr_lvl[1] = 0.0f;
    lead_off();
    lead.env = 0.0f;
    ld_key = -1;                                 /* (BERLIN) a key held on the Lead let go too: its phrases again */
    if (ld_cap & CP_ANS)                         /* (the keys' answer not begun: dropped) */
        ld_due = 0;
    cp_n = cp_q = ld_cap = 0;                    /* (the figure on the keys let go, a theme of theirs not answered now:
                                                  * the theme kept, the Lead's next pair on it in its own time) */
    for (i = 0; i < NPLUCK; i++)
        if (pluck[i].on) {
            pluck[i].on = 0;
            midi(pluck[i].layer ? 0x83u : 0x81u, pluck[i].note, 0);
        }
    notes_off();
}

static void ch_gt_init(void);
void en_init(void)
{
    int i;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    for (i = 0; i <= SN; i++)
        orgt[i] = sint[i % SN] + 0.5f * sint[2 * i % SN] + 0.3f * sint[3 * i % SN] + 0.2f * sint[4 * i % SN];
    ch_gt_init();                                /* (the Choir's level steps here, not on the audio thread's first
                                                  * Choir chunk: ~230 trig calls in one block) */
    rng = 0x9E3779B9u;                           /* (BERLIN) the dice from the top: a reset plays the same again */
    cl_rng = sq_rng = 0x2545F491u;
    dr_rng = 0x6A09E667u;
    ld_rng = 0x3C6EF372u;
    ks_rng = 0x6C078965u;                        /* (Pluck's bursts) */
    atm_rng = 0xBB67AE85u;
    atm_reset();                                 /* (the voices batch: its wanderings and filters too) */
    ar_rng = 0x510E527Fu;
    s2o_rng = 0x9B05688Cu;
    en_s2oct_shift = 0;
    for (i = 0; i < NFO; i++) {                  /* (the filter motion: its dice and its offsets from the top) */
        fo_rng[i] = FO_SEED[i];
        fo[i] = fo_v[i] = fo_a[i] = fo_b[i] = fo_next[i] = fo_bs[i] = 0.0f;
        fo_p[i] = 1.0f;
        en_fo[i] = 0.0f;
        en_fo_dir[i] = 0;
    }
    fo_be = fo_ib = 0.0f;
    root_note = 55;                              /* (the voicing from its first, the walk with no past) */
    walk_prev = 0;
    memset(pluck, 0, sizeof pluck);              /* (BERLIN's voices from rest: none sounding, none to let go) */
    memset(&lead, 0, sizeof lead);
    ld_key = -1;
    ld_reseed();
    dr_on[0] = dr_on[1] = dr_midi[0] = dr_midi[1] = 0;
    dr_lvl[0] = dr_lvl[1] = 0.0f;
    ch_live = 0;                                 /* (Choir from rest) */
    plate_init();
    for (i = 0; i < TAPE_N; i++)
        tape[i] = 0;
    shm_stop();                                  /* (Shimmer out, its line from the top) */
    memset(pl_buf, 0, sizeof pl_buf);            /* (BERLIN) the plate and the diffusers quiet too: a reset rings with
                                                  * nothing that came before (the short lines clear themselves) */
    memset(eap, 0, sizeof eap);
    pl_bw = pl_damp_l = pl_damp_r = pl_acc = pl_ol = pl_or = pl_back = 0.0f;
    memset(phz, 0, sizeof phz);                  /* (and the Drone's phaser from the top) */
    ph_last[0] = ph_last[1] = ph_lfo = 0.0f;
    ar_cr_env = 0.0f;                            /* (no Crash ringing on) */
    for (i = 0; i < CL_LEN; i++)
        cl_buf[i] = 0;
    for (i = 0; i < CL_NG; i++)
        cl_g[i].len = 0;
    cl_live = 0;
    tape_w = tape_ph = 0;
    echo_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    for (i = 0; i < EN_NLOOP * EN_LOOP_MAX; i++)   /* (DUB-1's ping loops: none) */
        en_loop_note[i / EN_LOOP_MAX][i % EN_LOOP_MAX] = -1;
    for (i = 0; i < EN_NMAC; i++)                /* (the master actions at 0, the hand off them) */
        mac[i] = en_mac[i] = 0;
    am_held = am_stuck = 0;
    am_ph[0] = am_ph[1] = 0.0f;
    for (i = 0; i < AM_N; i++)
        am_off[i] = 0.0f;
    en_mac_got = mac_sent;
    for (i = 0; i < P_NPARAMS; i++)
        pofs[i] = 0;
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    regrow();
    pl_decay = pl_decay_t;
}

/* ------------------------------------------------------------ transport --- */
static int32_t tick_wait;                       /* Swing: samples until an off sixteenth plays (0: none waiting) */
static void clock_pulse(void)
{
    if (pulse == 0) {                           /* an off sixteenth: everything on it late together (the clock straight) */
        int32_t w = (sixteenth & 1u) ? (int32_t)(swing * step_len) : 0;
        if (w > 0)
            tick_wait = w;
        else
            tick();
    }
    if (!en_ext)
        midi(0xF8, 0, 0);
    fo_pulses++;                                 /* (the filter motion's clock) */
    if (++pulse >= 6)
        pulse = 0;
}

static void play_start(int from_top)
{
    int i;
    if (from_top) {
        pulse = 0;
        sixteenth = 0;
        en_bars = 0;
        bar = 0;
        fo_pulses = fo_pp = 0;                   /* (the filter motion's clock from the top) */
        fo_f = 1.0f;
        ar_start();                              /* (the arc from its start; the first bar line already counted) */
        ar_first = 1;
        for (i = 0; i < 2; i++) {                /* the sequences from their first step */
            sq_pos[i] = 0;
            sq_bars[i] = sq_wrap[i] = 0;
            seq_hist_clear(&sq_hist[i]);
        }
        nz_drawn = 0;
        noise_pattern();
    }
    pulse_left = 0.0f;
    tick_wait = 0;
    en_playing = 1;
    drone_chord();
    if (ld_key < 0)                              /* (a Start while playing: the note let go, not sounding on through
                                                  * the intro, where the Lead is silent; a key held stays) */
        lead_off();
    ld_n = ld_i = 0;                             /* (the Lead: a breath before its first phrase) */
    ld_rest = 2;
    ld_due = 0;
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    tick_wait = 0;
    en_playing = 0;
    drone_off(dr_bank);
    lead_off();
    cp_n = cp_q = 0;                             /* (BERLIN) a figure on the keys cut by the stop let go (no grid to
                                                  * count it on; an answer already due still comes) */
    ld_cap &= (uint8_t)~CP_HELD;
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

/* a master action turned: the parameters it moves, applied again as heard (Filter: read by render_chunk) */
static void macro(int i, int v)
{
    /* each: the parameters it moves and how far at full turn (+50), in the parameter's own units */
    static const struct { uint8_t p; int8_t at50; } MOVE[] = {
        {P_SQECHO, 40}, {P_LDECHO, 40}, {P_ECHO, 30}, {P_S2ECHO, 40}, {P_DRECHO, 40},   /* Wet: the whole music dry, or
                                                          * drowned in the echo, every layer's send (BERLIN: Sequence 2's
                                                          * and the Drone's too) (the
                                                          * chord's and the pings' Wet, the echo's send; the noise too:
                                                          * render_crackle) */
        {P_DENSITY, 85}, {P_SEQ, 30}, {P_SEQ2, 30},       /* Energy (BERLIN): the sequences' notes and levels, */
        {P_SWEEP, 40}, {P_DECAY, 30}, {P_TAPB, 40}, {P_CUTOFF, 20},   /* their sweep, length, second repeat,
                                                                       * cutoff */
    };
    static const uint8_t OF[] = {EN_MAC_DUB, EN_MAC_DUB, EN_MAC_DUB, EN_MAC_DUB, EN_MAC_DUB, EN_MAC_ENERGY, EN_MAC_ENERGY, EN_MAC_ENERGY,
                                 EN_MAC_ENERGY, EN_MAC_ENERGY, EN_MAC_ENERGY, EN_MAC_ENERGY};
    unsigned k;
    mac[i] = (int8_t)v;
    en_mac[i] = (int8_t)v;
    for (k = 0; k < sizeof OF; k++)
        if (OF[k] == i) {
            int o = MOVE[k].at50 * v / 50, before = pe(MOVE[k].p);
            pofs[MOVE[k].p] = (int8_t)o;
            if (pe(MOVE[k].p) != before)
                apply(MOVE[k].p, par[MOVE[k].p]);
        }
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_SET:
            if (a < P_NPARAMS)
                apply(a, b);
            break;
        case C_POOL: pool_set((uint32_t)b & 0xFFFu); break;
        case C_PLAY:
            if (a && !en_playing)
                play_start(1);
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NEXT:
            if (en_playing)
                change_pending = 1;
            else
                chord_change();
            break;
        case C_FREEZE: en_frozen = (uint8_t)(a != 0); break;
        case C_SEED:
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_CLOCK:
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                if (en_playing)
                    clock_pulse();
            } else if (a == EN_CLK_START) {
                play_start(1);
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start(0);
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC: quiet(); break;
        case C_LKEY:                             /* (the white keys: the Lead, in the scale) */
            if (b) {
                const int held = ld_key >= 0;
                ld_n = 0;                        /* (its own phrase let go, its pair too; the keys' answer too) */
                ld_due = 0;
                ld_cap &= (uint8_t)~(CP_ANS | CP_NEXT);
                ld_key = (int8_t)a;
                lead_note(ld_deg_note(a));
                en_lead_deg = lead.on ? (int8_t)a : -1;
                cp_key(a, held);                 /* (written down once it sounds: the keys play as they did) */
            } else if (a == ld_key) {
                ld_key = -1;
                lead_off();
                ld_rest = 2;                     /* (its own phrases back after two bars) */
                cp_off();
            }
            break;
        case C_SEQSET:                           /* a pattern written: played as written, from its first step */
            sq[a & 1] = sq_pend[a & 1];
            sq_written[a & 1] = 1;
            sq_pos[a & 1] = 0;
            seq_hist_clear(&sq_hist[a & 1]);
            break;
        case C_REGROW:
            for (a = 0; a < EN_NMAC; a++)        /* (the user, 2026-10-28) a piece from the master actions at 0: none
                                                  * of the last one's push carried over */
                macro(a, 0);
            am_held = 0;
            for (a = 0; a < AM_N; a++)
                am_off[a] = 0.0f;
            regrow();
            if (seq_have)                        /* (a piece's own patterns, kept) */
                seq_take();
            ar_start();                          /* (BERLIN) a piece (changed, or back to its recipe) from its arc's
                                                  * start: its intros (Emerge), or Flow at once */
            ld_reseed();                         /* (the Lead's ideas new: a new piece; its phrase let go) */
            lead_off();
            break;
        case C_SEQ:
            seq_have = (uint8_t)a;
            if (a)
                seq_take();
            break;
        case C_MUTE:
            if (a & ~mutes & EN_M_BASS)          /* (BERLIN) the Lead muted: its note let go now (a key's too) */
                lead_off();
            if (a & ~mutes & EN_M_CHORD)         /* the Drone muted: its notes off on MIDI (its voices ring on, silent) */
                dr_midi_off(0), dr_midi_off(1);
            mutes = (uint32_t)a;
            break;
        case C_PERF: perf = (uint32_t)a; break;
        case C_FX: fxm = c & 0xFFFFu; break;
        case C_MOVE: mvm = c & 0xFFu; break;
        case C_LFORGET:                          /* (the user's design: PERFORM's top key) the Lead forgets everything it
                                                  * remembers: its ideas and its theme (the keys' too), a figure on the
                                                  * keys, an answer due, the offspring; its note let go (not a key's: the
                                                  * keys hold it); then new ideas as at a piece's start, its first block
                                                  * after the theme's 4 draws, a bar each (no breath left over) */
            if (ld_key < 0)
                lead_off();
            ld_reseed();
            ld_rest = ld_gap = 0;
            ld_cb = 1;                           /* (its first block, the new theme, at the bar line after its draws,
                                                  * not on the roll: the Lead back within a few bars) */
            break;
        case C_REWALK: {                         /* the walk's slots walked again, all but the one playing, each from
                                                  * the slot before (yours and the gaps kept) */
            int k, prev = deg;
            for (k = 0; k < EN_NPROG && nsteps; k++) {
                if (k != prog_pos && slot_walk(k)) {
                    slot_walk_set(k, walk_from(prev));
                    prev = par[P_CR0 + k] - 1;
                } else if (slot_walk(k)) {
                    prev = (par[P_CR0 + k] - 1) % nsteps;
                } else if (par[P_CQ0 + k]) {
                    prev = en_deg_of(pool, (par[P_CR0 + k] - par[P_ROOT] + 12) % 12);
                }
            }
            prog_publish();
            break;
        }
        case C_LOCK: lock = (uint8_t)a; break;
        case C_PROT:                             /* Loop to Endless: the one playing up to slot 1, all turned round */
            if (prog_pos > 0) {
                int16_t t[3][EN_NPROG];
                int j, k;
                for (k = 0; k < EN_NPROG; k++)
                    for (j = 0; j < 3; j++)
                        t[j][k] = par[P_CR0 + 8 * j + (k + prog_pos) % EN_NPROG];
                for (k = 0; k < EN_NPROG; k++)
                    for (j = 0; j < 3; j++)
                        par[P_CR0 + 8 * j + k] = t[j][k];
                prog_pos = 0;
                en_prog_pos = 0;
                prog_publish();
            }
            break;
        case C_MACRO:                            /* (a: the action, and the UI's count of its turns above it) */
            en_mac_got = (uint8_t)(a >> 2);
            a &= 3;
            if (a < EN_NMAC && (unsigned)b <= 100u) {
                if (a < AM_N)
                    am_held |= (uint8_t)(1u << a);   /* (the hand holds it till the next section change) */
                macro(a, b - 50);
            }
            break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK], crl[BLK], crr[BLK];

/* (the classics pass) the chord's and the pings' dry, wider: each side a little of the other's, 5 and 8 ms later and
 * drifting slowly (a chorus crossed), so the two sides differ without the voices panned further; Width (0 none,
 * 50 0.4, 100 0.8) */
#define WD_N 448                                /* (to 10 ms: the pool's headroom) */
static int16_t wd_buf[2 * WD_N] EN_POOL;
static int wd_w;
static float wd_ph;
static inline float wd_read(const int16_t *b, float d)
{
    float r = (float)wd_w - d, a;
    int i0, i1;
    if (r < 0.0f)
        r += (float)WD_N;
    i0 = (int)r;
    i1 = i0 + 1 < WD_N ? i0 + 1 : 0;
    a = r - (float)i0;
    return ((float)b[i0] + a * (float)(b[i1] - b[i0])) * (1.0f / 12000.0f);
}
static float wd_c;                             /* (its amount, eased: off while shedding) */
static void widen(uint32_t n)
{
    float c, tl, tr, dl, dr;
    uint32_t j;
    wd_c += ((SHED ? 0.0f : 0.4f * fm_minf(ec_wide, 2.0f)) - wd_c) * 0.25f;
    c = wd_c;
    if (c < 0.004f)
        return;
    wd_ph += (float)n * (0.21f / EN_SR);
    if (wd_ph >= 1.0f)
        wd_ph -= 1.0f;
    tl = (0.005f + 0.001f * fm_sinf(6.2831853f * wd_ph)) * EN_SR;
    tr = (0.008f + 0.001f * fm_cosf(6.2831853f * wd_ph + 1.3f)) * EN_SR;
    for (j = 0; j < n; j++) {
        float l = mixl[j], r = mixr[j];
        wd_buf[wd_w] = s16(l), wd_buf[WD_N + wd_w] = s16(r);
        dl = wd_read(wd_buf + WD_N, tl);         /* (the left: the right's, 5 ms on) */
        dr = wd_read(wd_buf, tr);                /* (the right: the left's, 8 ms on) */
        if (++wd_w >= WD_N)
            wd_w = 0;
        mixl[j] = l + c * dl;
        mixr[j] = r + c * dr;
    }
}


/* Crackle: a small palette of noises, each mono, each drifting on its own over minutes (its level, and the
 * hiss its colour), so the bed never settles into one static noise (1.3.0; it was a garnish of hiss and clicks):
 *   the hiss: the tape's floor, flat over 2-8 kHz and dark above, its band moving slowly (an auto-filter on a
 *     random LFO);
 *   the vinyl: clicks at random, mostly small, now and then a pop, each its own colour (dull to sharp) and its
 *     own side; how many drifts, so the transients slow down and space out now and then;
 *   the dust: soft grains of band-passed noise (1.5-6 kHz, 10-50 ms), a few a beat at random, partly into the
 *     echo: a texture between the hits;
 *   the rumble: a room's, a deck's, under 60 Hz and mono, very quiet;
 *   the wind: a breath of noise through a narrow band (~250 Hz-2 kHz), swept slowly by the drift and Motion's arc.
 * The Crackle knob is the level of all of it; turned up, more of the palette comes in (the hiss and the vinyl
 * from the start, the dust from ~20, the rumble from ~25, the wind from ~35). In a Break or Thin, the hiss and
 * the dust come forward (+2.5 dB), to carry the groove. It plays while the music plays, fading in and out over
 * a second; a little of it into the plate.
 * Four controls shape it, each -1..+1 around the piece's own (0), and a character (a preset of the mix): ready
 * on ARP's Noise page (P_NCOL .. P_NCHAR, into nctl) */
const char *const EN_NCHAR_NAME[EN_NNCHAR] = {"None", "Wind", "Surf", "Space", "Air", "Tape", "Rain"};   /* (BERLIN: the
                                                  * Atmosphere's; the voices batch: Rain appended, so every one before it
                                                  * keeps its number in the pieces and the saves;
                                                  * NCHAR below is the dub's noise machine, heard as Tape) */
#define NZCH 5                                  /* (the dub's noise machine's characters: Tape is its 1) */
/* 1.3.0's noise machine (build/research/dubtechno/REPORT4-noise.md, after Bahadirhan Kocer's "Using Resonators in
 * Live"): not a broadband bed but a voice. A crackle groove, hits on a 2-bar sixteenth pattern (Density; redrawn by
 * phrase, now and then kept: a motif that comes back), each a click and a short noise burst, rings three combs tuned
 * to the chord's root, third and fifth in octave 1 (Decay: how long), through a band-pass (Tone, ~250 Hz-2 kHz:
 * the harmonics as a chord, nothing up top); into the plate, a little into the echo (more on a throw). Under it the
 * Air, a quiet hiss that breathes with the duck, and now and then a dry vinyl click. The characters (ALGORITHM) are
 * the same engine set otherwise. The combs and the band-pass at a quarter rate (all of it under 2.5 kHz) */
static const struct {
    float res, cont, clicks, dens, dec, tone, air;  /* resonance (0 dry), the hiss into the combs (a pad), the dry
                                                     * clicks, and offsets: hits, Decay, Tone, Air */
    float loose;                                    /* the dry clicks off the grid (the rest on its sixteenths) */
    uint8_t key, inh;                               /* on the key's triad (not the chord's); inharmonic ratios */
} NCHAR[NZCH] = {
    {1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0, 0},     /* Vinyl: the resonated crackle groove, on the chord */
    {1.0f, 1.0f, 0.4f, -0.6f, 0.6f, -0.2f, -0.3f, 0.3f, 0, 0},  /* Tape: the hiss through the combs, a pad that swells between
                                                           * the beats (the duck), few hits */
    {0.0f, 0.0f, 2.0f, 0.3f, 0.0f, 0.3f, 0.0f, 0.5f, 0, 0},     /* Dust: dry clicks and bursts, no resonance */
    {1.0f, 0.3f, 0.7f, -0.3f, 0.5f, -0.3f, 0.2f, 0.25f, 1, 0},   /* Room: on the key's tonic triad, long, more of the plate */
    {1.0f, 0.0f, 1.2f, 0.2f, 0.2f, 0.15f, -0.2f, 0.0f, 0, 1},   /* Machine: inharmonic (1 : 1.41 : 2.76), metallic */
};
#define CR_ND 4                                 /* the drifts: the air's level, the tone, the clicks' density, the decay */
static float cr_lp, cr_lp2, cr_hp, cr_cl, cr_cr, cr_fl, cr_fr, cr_cc = 0.3f, cr_gate;
static float cr_dv[CR_ND] = {1.0f, 1.0f, 1.0f, 1.0f}, cr_dt[CR_ND] = {1.0f, 1.0f, 1.0f, 1.0f};
static const float CR_DRANGE[CR_ND] = {0.66f, 0.7f, 2.0f, 0.5f};   /* (octaves of each drift's span) */
#define NZ_L 280                                /* a comb's line, at the quarter rate (E1: 269 samples) */
static int16_t nz_buf[3 * NZ_L];
static int nz_w, nz_ph;
static float nz_D[3] = {200.0f, 160.0f, 133.0f}, nz_lp[3], nz_acc, nz_b[4], nz_p[2], nz_c[2];
static uint32_t nz_pat;                         /* this phrase's hits, 32 sixteenths (two bars) */
static uint8_t nz_vel[32];
static float nz_ex, nz_exd = 0.99f, nz_click;   /* a hit: its burst's level and fall, its click */
/* (a hit struck early: the combs speak a period after the burst and swell after, so the next step's hit is struck
 * that much before its tick, its ring heard on it; REPORT6 9.1) */
static int32_t nz_wait = -1;                    /* samples till the next hit strikes (-1: none) */
static float nz_wv, nz_wd, nz_wD[3];            /* its burst's level, fall, the combs' tuning */
static uint8_t nz_due;                          /* (the hit struck for this tick: its screen event on it) */
static float nz_gck;                            /* a dry click on this tick (its level; 0: none) */
static float cr_u(void)                          /* the noises' own randomness: the music's choices left be */
{
    cnoise = cnoise * 1664525u + 1013904223u;
    return (float)(cnoise >> 8) * (1.0f / 16777216.0f);
}
static unsigned nz_char(void) { return nctl.character < NZCH ? nctl.character : 0u; }
/* the phrase's groove: Density's hits (2..16 in two bars), Euclidean, turned, a couple nudged a step off; now and then
 * the last one kept (more often as Evolve falls): a motif that comes back */
static void noise_pattern(void)
{
    const unsigned c = nz_char();
    float d = fm_clampf(nctl.grain + 0.5f * NCHAR[c].dens, -1.0f, 1.0f);
    int k = (int)(9.0f + 7.0f * d + 0.5f), i, m;
    uint32_t p;
    if (nz_drawn && cr_u() > 0.35f + 0.5f * evolve + 0.3f * nctl.drift)   /* (Change: more often new) */
        return;
    nz_drawn = 1;
    k = k < 0 ? 0 : k > 16 ? 16 : k;
    p = en_euclid(k, 32, (int)(cr_u() * 32.0f));
    for (m = 0; m < 2; m++) {                   /* (a hit a step later or earlier, where it's free) */
        int s = (int)(cr_u() * 32.0f) & 31, t = (s + (cr_u() < 0.5f ? 1 : 31)) & 31;
        if (p >> s & 1u && !(p >> t & 1u))
            p = (p & ~(1u << s)) | 1u << t;
    }
    nz_pat = p;
    for (i = 0; i < 32; i++)
        nz_vel[i] = (uint8_t)(cr_u() < 0.15f ? 64 : 180 + (int)(cr_u() * 75.0f));   /* (a ghost now and then) */
}
static float nz_note(int pc) { return mtof((float)(28 + (pc + 8) % 12)); }   /* octave 1: E1 .. Eb2 */
/* a sixteenth (tick): the hit struck for it told; the next step's, if the groove has one, struck ahead of its tick by
 * the combs' latency (the shortest one's period, and a little of the swell), the combs retuned as it strikes (never
 * mid-ring); a dry click on the grid now and then (the character's loose ones at random: render_crackle) */
static void noise_step(int s)
{
    const unsigned c = nz_char();
    const int nx = (s + 1) & 31;
    float f[3], v, gap, lead = 0.0f;
    int k;
    if (nz_due) {
        nz_due = 0;
        v = (float)nz_vel[s & 31] * (1.0f / 255.0f);
        nev(v > 0.5f ? 1 : 0, v * crackle_vol * 9.0f, 0.5f);
    }
    if ((mutes & EN_M_NOISE) || crackle_vol <= 0.0f)
        return;
    {   /* the grid's dry clicks: of the clicks a second, those not loose, a sixteenth at a time */
        const float rate = (3.0f + 12.0f * crackle_vol) * cr_dv[2] * fm_exp2f(fm_clampf(nctl.grain, -1.0f, 1.0f)) *
                           (1.0f - NCHAR[c].loose);
        if (cr_u() < fm_minf(rate * step_len * (1.0f / EN_SR), 0.9f)) {
            float a = cr_u();
            nz_gck = (0.03f + 0.4f * a * a * a);
        }
    }
    if (!(nz_pat >> nx & 1u))
        return;
    if (NCHAR[c].key) {                          /* (Room: the key's tonic triad) */
        int r = par[P_ROOT];
        f[0] = nz_note(r), f[1] = nz_note(r + (pool >> 3 & 1u ? 3 : 4)), f[2] = nz_note(r + 7);
    } else {
        f[0] = nz_note(chord_pcs[0]), f[1] = nz_note(chord_pcs[1]), f[2] = nz_note(chord_pcs[chord_nt >= 3 ? 2 : 0]);
    }
    if (NCHAR[c].inh)                            /* (Machine: 1 : 1.41 : 2.76) */
        f[1] = f[0] * 1.41f, f[2] = f[0] * 2.76f;
    for (k = 0; k < 3; k++) {
        float d = TAPE_SR / f[k];
        while (d > (float)(NZ_L - 2))
            d *= 0.5f;
        nz_wD[k] = d;
    }
    if (NCHAR[c].res > 0.0f)                     /* (the shortest comb's period, at the full rate, and 6 ms) */
        lead = 4.0f * fm_minf(fm_minf(nz_wD[0], nz_wD[1]), nz_wD[2]) + 0.006f * EN_SR;
    gap = step_len * (1.0f + (s & 1 ? -swing : swing));   /* (to the next tick: the odd ones late by Swing) */
    v = (float)nz_vel[nx] * (1.0f / 255.0f);
    nz_wv = v;
    nz_wd = fm_expf(-6.9f / ((0.005f + 0.008f * cr_u()) * EN_SR));   /* (a 5-13 ms burst: struck, staccato) */
    if (SHED)                                    /* (shedding: no new hits; the rings die, and the combs rest) */
        return;
    nz_wait = (int32_t)fm_maxf(gap - lead, 0.0f);
    nz_due = 1;
}
/* (BERLIN) the Atmosphere: the Synthi's weather (Schulze's "water, wind, twittering, thunder"). Wind: pink noise through
 * a resonant band-pass wandering ~250 Hz .. 2 kHz, louder as it climbs (gusts); Surf: brown noise swelling and falling
 * every 6 .. 12 s, left and right apart, brighter as a wave breaks; Space: sample-and-hold bleeps, short sines on
 * random notes of the scale high up, at random, into the echo; Air: a soft high hiss, breathing; Tape: the dub's noise
 * machine (render_crackle); Rain: soft drops, each a short ring, over a quiet hush. Colour: darker .. brighter;
 * Density: calmer .. busier; Drift: slower .. quicker. While playing, as the dub's was.
 * (The voices batch, item 4: Wind's pink, Surf's brown, Wind's level following its band and Rain's drops after Skiff's
 * NOISE, eng_noise.c, Leo Kuroshita, Hügelton Instruments, GPL-3.0-only: its COLR tilt, white -> pink (three leaky
 * poles, Paul Kellet's "economy" filter) -> brown (a leaky integrator), its DRFT (the cutoff wandering, the level
 * following it) and its DUST (sparse impulses through a resonance); written again in float. The user: the FM-1's
 * speaker is bright, the noise was hissy; each level is matched by its A-weighted loudness to the one he tuned.) */
enum { ATM_NONE, ATM_WIND, ATM_SURF, ATM_SPACE, ATM_AIR, ATM_TAPE, ATM_RAIN };
static inline float svf_lp(const float *c, float *s, float x, float *bp);
static float atm_gate, atm_w[2] = {0.3f, 0.6f}, atm_wt[2] = {0.5f, 0.5f}, atm_gu = 0.5f, atm_gt = 0.5f, atm_s[2][2],
             atm_c[2][4], atm_sw[2] = {0.0f, 0.4f}, atm_swr[2] = {0.11f, 0.13f}, atm_lp[2], bl_ph, bl_dt, bl_env,
             bl_gl, bl_gr, bl_wait, atm_hl, atm_hr, bl_dt1, bl_glk, bl_idx, bl_dk = 0.999f, bl_step;
static int bl_burst;
/* (the voices batch) Wind's pink poles and its level now (each side's), Surf's top low-pass; Rain's drops: RN_N rings,
 * each a sine (its phase, its step now and the step it rises to) under two decays, a slow one (the ring) less a quick
 * one (its 3 ms onset: soft, not a click), panned; the wait to the next drop (samples) */
#define RN_N 4
static float atm_pk[2][3], atm_lv[2], atm_lq[2], rn_ph[RN_N], rn_dt[RN_N], rn_dt1[RN_N], rn_es[RN_N], rn_ef[RN_N],
             rn_ds[RN_N], rn_gl[RN_N], rn_gr[RN_N], rn_wait;
/* the constants, by ear and by measure (each level: its A-weighted loudness the shipped one's, in the piece the user
 * tuned it in: Timewind's Wind at 7, Sunrain's Surf at 5; Rain as loud as Wind at the same level) */
#define WD_G 0.20f                               /* Wind's level (pink through the band) */
#define WD_LV0 0.3f                              /* Wind's level at the bottom of its wander (x1 at the top: -10 dB) */
#define SF_G 16.8f                               /* Surf's level (brown, swelling) */
#define SF_BR 110.0f                             /* Surf's brown corner (Hz): flat below, -6 dB an octave above */
#define SF_TOP 1200.0f                           /* Surf's top low-pass at Colour 0 (Hz), x0.5 .. x1.5 as a wave
                                                  * swells to its crest: -12 dB an octave above */
#define RN_G 3.2f                                /* Rain's drops' level */
#define RN_BED 0.04f                             /* its hush, under the drops */
#define RN_LO 700.0f                             /* its drops' lowest pitch at Colour 0 (Hz; two octaves up from it) */
#define RN_RATE 8.0f                             /* its drops a second at Density 0, the shower at its middle */
#define RN_DF 0.9492f                            /* (a drop's onset: the quick decay, -60 dB in 3 ms) */
#define RN_GLK 0.004f                            /* (a drop's rise onto its pitch, from 3 semitones under: ~6 ms) */
/* (the voices batch) the Atmosphere from the top, as en_init's dice (a reset plays the same again): its wanderings,
 * waves and filters as the program starts them (so a piece played alone sounds as it did); its gate, the transport's,
 * kept. Wind's level now follows its wander: without this its loudness in the first minutes hung on where the piece
 * before left the wander */
static void atm_reset(void)
{
    int ch, k;
    for (ch = 0; ch < 2; ch++) {
        atm_w[ch] = ch ? 0.6f : 0.3f, atm_wt[ch] = 0.5f, atm_sw[ch] = ch ? 0.4f : 0.0f, atm_swr[ch] = ch ? 0.13f : 0.11f;
        atm_s[ch][0] = atm_s[ch][1] = atm_lp[ch] = atm_lq[ch] = atm_lv[ch] = 0.0f;
        atm_pk[ch][0] = atm_pk[ch][1] = atm_pk[ch][2] = 0.0f;
    }
    atm_gu = atm_gt = 0.5f;
    atm_hl = atm_hr = 0.0f;
    bl_ph = bl_dt = bl_env = bl_gl = bl_gr = bl_wait = bl_dt1 = bl_glk = bl_idx = bl_step = 0.0f;
    bl_dk = 0.999f, bl_burst = 0;
    for (k = 0; k < RN_N; k++)
        rn_ph[k] = rn_dt[k] = rn_dt1[k] = rn_es[k] = rn_ef[k] = rn_ds[k] = rn_gl[k] = rn_gr[k] = 0.0f;
    rn_wait = 0.0f;
}
static float atm_u(void) { return (float)(seq_rand(&atm_rng) >> 8) * (1.0f / 16777216.0f); }
static float atm_noise(void) { return (float)(int32_t)seq_rand(&atm_rng) * (1.0f / 2147483648.0f); }
/* (the voices batch) a new drop on the quietest ring: a pitch at random over two octaves from RN_LO (Colour moves
 * them), rising onto it from 3 semitones under (a bubble's chirp), loud now and then but mostly soft, its ring Ring's
 * (0.3 .. 0.9 of it, each its own), here or there; the next one at random (a Poisson rain: rate a second) */
static void rain_drop(float col, float ring, float rate)
{
    const float u = atm_u(), p = atm_u() * (FM_PI / 2.0f), v = atm_u(), a = atm_u(), w = atm_u();
    int k = 0, i;
    for (i = 1; i < RN_N; i++)                   /* (the quietest ring: a free one, or the one nearly done) */
        if (rn_es[i] < rn_es[k])
            k = i;
    rn_dt1[k] = RN_LO * fm_exp2f(2.0f * u) * col * (1.0f / EN_SR);
    rn_dt[k] = 0.84f * rn_dt1[k];
    rn_ph[k] = 0.0f;
    rn_es[k] = rn_ef[k] = 0.25f + 0.75f * a * a;
    rn_ds[k] = fm_expf(-6.9f / (ring * (0.3f + 0.6f * v) * EN_SR));
    rn_gl[k] = fm_cosf(p), rn_gr[k] = fm_sinf(p);
    rn_wait += -fm_logf(fm_maxf(w, 1e-6f)) * EN_SR / rate;
}
static void render_atmos(uint32_t n)
{
    const int c = atm_char;
    const float ring = 0.025f + 0.0023f * (float)par[P_NTUNE],
                air = (float)(par[P_NROOM] + 50) * 0.01f, wid = (float)par[P_NWIDTH] * 0.02f,
                rv = (float)par[P_NPLATE] * 0.02f, ec = (float)par[P_NECHO] * 0.02f,
                col = fm_exp2f(fm_clampf(nctl.colour + fo[FO_ATM], -1.0f, 1.0f)), busy = fm_exp2f(fm_clampf(nctl.grain, -1.0f, 1.0f)),
                spd = fm_exp2f(fm_clampf(nctl.drift, -1.0f, 1.0f)), sec_n = (float)n * (1.0f / EN_SR);
    float g, sa[2] = {0.0f, 0.0f}, sd[2] = {0.0f, 0.0f}, kb = 0.0f, kt = 0.0f, bed = 0.0f, rate = 1.0f;
    uint32_t j;
    int ch, k;
    atm_gate += ((en_playing ? 1.0f : 0.0f) - atm_gate) * fm_minf((float)n / (0.5f * EN_SR), 1.0f);
    g = mutes & EN_M_NOISE || c == ATM_NONE ? 0.0f : crackle_vol * atm_gate;   /* (its level: nothing more) */
    atm_g_out = g;
    en_atm_gain = g;
    if (g < 1e-5f) {
        for (j = 0; j < n; j++)
            crl[j] = crr[j] = 0.0f;
        return;
    }
    for (ch = 0; ch < 2; ch++) {                 /* the wanderings: a new aim now and then (Wind's: more often with
                                                  * Density, its gusts), eased to */
        if (atm_u() < sec_n * 0.35f * spd * (c == ATM_WIND ? busy : 1.0f))
            atm_wt[ch] = atm_u();
        atm_w[ch] += (atm_wt[ch] - atm_w[ch]) * fm_minf(sec_n * 0.25f * spd, 1.0f);
        svf_coefs(atm_c[ch], 250.0f * fm_exp2f(3.0f * atm_w[ch]) * col, 0.32f - 0.28f * (float)par[P_NTUNE] * 0.01f);
        atm_lv[ch] = WD_G * (WD_LV0 + (1.0f - WD_LV0) * atm_w[ch]);   /* (Wind: its level follows the band, as
                                                  * Skiff's DRFT: the gusts climb and swell together) */
    }
    if (atm_u() < sec_n * 0.5f * spd * busy)     /* (the gusts; Air's breathing, Rain's showers) */
        atm_gt = atm_u();
    atm_gu += (atm_gt - atm_gu) * fm_minf(sec_n * 0.6f * spd, 1.0f);
    if (c == ATM_SURF) {                         /* (its swell: the block's ends, a straight line between: the
                                                  * waves are seconds long) */
        kb = SF_BR * (FM_TWO_PI / EN_SR);
        kt = fm_minf(SF_TOP * col * (FM_TWO_PI / EN_SR), 0.6f);
        for (ch = 0; ch < 2; ch++) {
            const float s0 = 0.5f - 0.5f * sine(atm_sw[ch] + 0.75f),
                        s1 = 0.5f - 0.5f * sine(atm_sw[ch] + 0.75f + (float)n * atm_swr[ch] * spd * (1.0f / EN_SR));
            sa[ch] = s0 * s0;
            sd[ch] = (s1 * s1 - s0 * s0) / (float)n;
        }
    } else if (c == ATM_RAIN) {                  /* (the shower: Density's rate, the gusts' swell; its hush with it) */
        kb = fm_minf(800.0f * col * (FM_TWO_PI / EN_SR), 1.0f);
        rate = RN_RATE * busy * (0.3f + 1.4f * atm_gu);
        bed = RN_BED * (0.5f + atm_gu);
    }
    for (j = 0; j < n; j++) {
        float l = 0.0f, r = 0.0f, bp, xl = 0.0f, xr = 0.0f;
        switch (c) {
        case ATM_WIND: {                         /* (one draw, two 16-bit noises: one a side, the hiss's too) */
            const uint32_t rr = seq_rand(&atm_rng);
            xl = (float)(int32_t)(rr << 16) * (1.0f / 2147483648.0f);
            xr = (float)(int32_t)(rr & 0xFFFF0000u) * (1.0f / 2147483648.0f);
            for (ch = 0; ch < 2; ch++) {         /* pink: three leaky poles and the noise itself (Kellet) */
                float *p = atm_pk[ch];
                const float x = ch ? xr : xl;
                p[0] = 0.99765f * p[0] + 0.0990460f * x;
                p[1] = 0.96300f * p[1] + 0.2965164f * x;
                p[2] = 0.57000f * p[2] + 1.0526913f * x;
                svf_lp(atm_c[ch], atm_s[ch], p[0] + p[1] + p[2] + 0.1848f * x, &bp);
                if (ch)
                    r = bp * atm_lv[1];
                else
                    l = bp * atm_lv[0];
            }
            break;
        }
        case ATM_SURF:                           /* (each side's draws as before: the noise, a new wave's length) */
            for (ch = 0; ch < 2; ch++) {
                const float a = sa[ch];
                sa[ch] += sd[ch];
                atm_lp[ch] += kb * (atm_noise() - atm_lp[ch]);   /* brown, from SF_BR up */
                atm_lq[ch] += kt * (0.5f + a) * (atm_lp[ch] - atm_lq[ch]);   /* (a wave breaking: brighter) */
                atm_sw[ch] += atm_swr[ch] * spd * (1.0f / EN_SR);
                if (atm_sw[ch] >= 1.0f) {        /* (a wave done: the next one 6 .. 12 s) */
                    atm_sw[ch] -= 1.0f;
                    atm_swr[ch] = 1.0f / (6.0f + 6.0f * atm_u());
                }
                if (ch)
                    r = SF_G * atm_lq[ch] * a;
                else
                    l = SF_G * atm_lq[ch] * a;
            }
            break;
        case ATM_SPACE:                          /* (the Synthi's sample-and-hold, twittering: no two bleeps alike) */
            if ((bl_wait -= 1.0f) <= 0.0f) {     /* a bleep: a note of the scale an octave or two up, here or there,
                                                  * sliding onto it from up to 7 semitones off; now and then a burst of
                                                  * 3 .. 8 at a sample-and-hold clock (7 .. 16 a second) */
                const int k = nsteps ? (int)(seq_rand(&atm_rng) % (uint32_t)nsteps) : 0, o = 1 + (int)(seq_rand(&atm_rng) % 2u);
                const float p = atm_u() * (FM_PI / 2.0f), u = atm_u();
                bl_dt1 = mtof((float)(60 + par[P_ROOT] % 12 + 12 * o + (nsteps ? steps[k] : 0))) * (1.0f / EN_SR);
                bl_dt = bl_dt1 * fm_exp2f((atm_u() - 0.5f) * 1.2f);
                bl_glk = 0.0004f + 0.004f * atm_u();
                bl_idx = 1.6f * u * u;           /* (mostly round, now and then bright) */
                bl_dk = fm_expf(-6.9f / (ring * (0.4f + 1.6f * atm_u()) * EN_SR));
                bl_env = 0.55f + 0.45f * atm_u();
                bl_gl = fm_cosf(p), bl_gr = fm_sinf(p);
                if (bl_burst > 0) {
                    bl_burst--;
                    bl_wait = bl_step;
                } else if (atm_u() < 0.35f) {
                    bl_burst = 2 + (int)(seq_rand(&atm_rng) % 6u);
                    bl_step = EN_SR / (7.0f + 9.0f * atm_u());
                    bl_wait = bl_step;
                } else
                    bl_wait = (0.25f + 1.2f * atm_u()) * EN_SR / busy;
            }
            if (bl_env > 1e-4f) {
                const float x = 1.25f * sine(bl_ph + 2.0f + bl_idx * bl_env * sine(2.0f * bl_ph)) * bl_env;
                l = x * bl_gl, r = x * bl_gr;
                bl_env *= bl_dk;                 /* (Ring: the bleeps' length, each its own) */
            }
            bl_dt += (bl_dt1 - bl_dt) * bl_glk;
            bl_ph += bl_dt;
            if (bl_ph >= 1.0f)
                bl_ph -= 1.0f;
            esend[j] += 0.9f * ec * (l + r) * g;  /* (into the echo, twittering) */
            break;
        case ATM_RAIN: {                         /* (one draw, two noises, as Wind's) */
            const uint32_t rr = seq_rand(&atm_rng);
            xl = (float)(int32_t)(rr << 16) * (1.0f / 2147483648.0f);
            xr = (float)(int32_t)(rr & 0xFFFF0000u) * (1.0f / 2147483648.0f);
            atm_lp[0] += kb * (xl - atm_lp[0]);   /* the hush: the shower far off */
            atm_lp[1] += kb * (xr - atm_lp[1]);
            l = bed * atm_lp[0], r = bed * atm_lp[1];
            if ((rn_wait -= 1.0f) <= 0.0f) {
                if (SHED)                        /* (shedding: no new drop; the rings die) */
                    rn_wait = 0.0f;
                else
                    rain_drop(col, ring, rate);
            }
            for (k = 0; k < RN_N; k++)
                if (rn_es[k] > 0.0f) {
                    const float x = RN_G * sine(rn_ph[k]) * (rn_es[k] - rn_ef[k]);
                    l += x * rn_gl[k], r += x * rn_gr[k];
                    rn_es[k] *= rn_ds[k];
                    rn_ef[k] *= RN_DF;
                    if (rn_es[k] < 1e-4f)        /* (done: the ring free, never into denormals) */
                        rn_es[k] = rn_ef[k] = 0.0f;
                    rn_dt[k] += (rn_dt1[k] - rn_dt[k]) * RN_GLK;
                    rn_ph[k] += rn_dt[k];
                    if (rn_ph[k] >= 1.0f)
                        rn_ph[k] -= 1.0f;
                }
            break;
        }
        default:                                 /* Air: what a low-pass leaves, breathing with the gusts */
            for (ch = 0; ch < 2; ch++) {
                const float x = atm_noise();
                atm_lp[ch] += fm_minf(3500.0f * col * (1.0f / EN_SR) * FM_TWO_PI, 1.0f) * (x - atm_lp[ch]);
                if (ch)
                    r = 0.7f * (x - atm_lp[ch]) * (0.4f + 0.6f * atm_gu);
                else
                    l = 0.7f * (x - atm_lp[ch]) * (0.4f + 0.6f * atm_gu);
            }
            break;
        }
        if (c != ATM_AIR) {                      /* Air: a hiss under the others (Wind's and Rain's from their own
                                                  * noise: a draw saved) */
            const float hl = c == ATM_WIND || c == ATM_RAIN ? xl : atm_noise(),
                        hr = c == ATM_WIND || c == ATM_RAIN ? xr : atm_noise();
            atm_hl += 0.35f * (hl - atm_hl), atm_hr += 0.35f * (hr - atm_hr);
            l += 0.12f * air * (hl - atm_hl), r += 0.12f * air * (hr - atm_hr);
        }
        {                                        /* Width: 0 one, 50 as it is, 100 wider */
            const float m = 0.5f * (l + r), sd = 0.5f * (l - r) * wid;
            l = m + sd, r = m - sd;
        }
        crl[j] = l * g;
        crr[j] = r * g;
        rsend[j] += 0.5f * rv * (l + r) * g;
        if (c != ATM_SPACE)
            esend[j] += 0.15f * ec * (l + r) * g;   /* (a little into the echo; the bleeps, more: above) */
    }
    for (ch = 0; ch < 2; ch++)                   /* (the poles' and the onsets' tails: never into denormals; a block
                                                  * takes a quick onset down by 1e-6 at most) */
        atm_lq[ch] = fm_flush(atm_lq[ch]);
    for (k = 0; k < RN_N; k++)
        rn_ef[k] = fm_flush(rn_ef[k]);
}
#ifdef EN_HOST
static int atm_fine(float x) { return x == x && fm_fabsf(x) < 1e30f && (x == 0.0f || fm_fabsf(x) > 1.2e-38f); }
int en_atm_ok(void)                              /* (host tests) the Atmosphere's float state finite, no denormals */
{
    int ok = atm_fine(atm_gate) && atm_fine(atm_gu) && atm_fine(atm_hl) && atm_fine(atm_hr) && atm_fine(bl_env) &&
             atm_fine(bl_ph) && atm_fine(bl_dt) && atm_fine(rn_wait), ch, k;
    for (ch = 0; ch < 2; ch++)
        ok &= atm_fine(atm_w[ch]) && atm_fine(atm_s[ch][0]) && atm_fine(atm_s[ch][1]) && atm_fine(atm_lp[ch]) &&
              atm_fine(atm_sw[ch]) && atm_fine(atm_lq[ch]) && atm_fine(atm_pk[ch][0]) && atm_fine(atm_pk[ch][1]) &&
              atm_fine(atm_pk[ch][2]);
    for (k = 0; k < RN_N; k++)
        ok &= atm_fine(rn_es[k]) && atm_fine(rn_ef[k]) && atm_fine(rn_ph[k]) && atm_fine(rn_dt[k]);
    return ok;
}
#endif

static void render_crackle(uint32_t n)
{
    uint32_t j, thr;
    int i;
    float g, k, nc, ng, nr, nd, gh, lpc, vg, gres, cg, lpk, bf, cont, dry;
    const unsigned c = nz_char();
    cr_gate += ((en_playing ? 1.0f : 0.0f) - cr_gate) * fm_minf((float)n / (0.5f * EN_SR), 1.0f);
    if (cr_gate < 1e-4f)
        cr_gate = 0.0f;
    g = mutes & EN_M_NOISE ? 0.0f : crackle_vol * cr_gate;   /* (muted: its level kept) */
    atm_g_out = g;
    en_atm_gain = g;
    if (g <= 0.0f) {
        for (j = 0; j < n; j++)
            crl[j] = crr[j] = 0.0f;
        return;
    }
    nc = fm_clampf(nctl.colour + fo[FO_ATM] + NCHAR[c].tone, -1.0f, 1.0f);   /* Tone (and the filter motion) */
    ng = fm_clampf(nctl.grain, -1.0f, 1.0f);                     /* Density (the clicks with it) */
    nr = fm_clampf(nctl.room + NCHAR[c].air, -1.0f, 1.0f);       /* Air */
    nd = fm_clampf(nctl.drift, -1.0f, 1.0f);                     /* the drifts' speed (the recipes') */
    if (cr_dbars <= 0) {                         /* the drifts: new targets on a barline, every 1-3 bars, eased over
                                                  * a bar (REPORT6 9.3: they crossed the barlines at random) */
        float sp = fm_exp2f(nd);
        for (i = 0; i < CR_ND; i++)
            cr_dt[i] = fm_exp2f((cr_u() - 0.5f) * CR_DRANGE[i] * sp);
        cr_dbars = (int)fm_maxf((1.0f + 3.0f * cr_u()) / sp, 1.0f);
    }
    k = fm_minf((float)n * fm_exp2f(nd) / (16.0f * step_len), 1.0f);
    for (i = 0; i < CR_ND; i++)
        cr_dv[i] += (cr_dt[i] - cr_dv[i]) * k;
    {   /* Decay: the combs' gain, 0.75 .. 0.97 (-60 dB in ~70 ms .. ~1.2 s); Tone: the band-pass ~250 Hz-2 kHz, the
         * damping with it; both moved by the drifts */
        float dec = fm_clampf((float)par[P_NTUNE] * 0.01f + 0.3f * NCHAR[c].dec + 0.3f * (cr_dv[3] - 1.0f), 0.0f, 1.0f), t;
        cg = 0.8f + 0.18f * dec;
        t = fm_clampf(nc + 0.7f * (cr_dv[1] - 1.0f) + lfo_n, -1.0f, 1.0f);   /* (and its LFO) */
        bf = 2.0f * fm_sinf(FM_PI * fm_minf(700.0f * fm_exp2f(1.5f * t), 2400.0f) / TAPE_SR);
        lpk = fm_clampf(0.72f + 0.22f * t, 0.3f, 0.97f);
    }
    gres = (20.0f * NCHAR[c].res + 5.0f * (1.0f - NCHAR[c].res)) * g;   /* (the band's level; the knob's folded in) */
    dry = 1.0f - NCHAR[c].res;                   /* (Dust: the bursts dry, into the band-pass) */
    cont = NCHAR[c].cont;
    gh = 0.03f * g * cr_dv[0] * fm_exp2f(1.2f * nr);   /* the Air: quiet (it breathes with the duck: render_chunk) */
    lpc = fm_clampf(0.4f * fm_exp2f(0.5f * nc), 0.15f, 0.8f);   /* (its band: ~2-4 kHz at the top) */
    vg = 0.5f * g * NCHAR[c].clicks * fm_exp2f(0.7f * ng);   /* (the dry clicks, quiet) */
    thr = (uint32_t)((3.0f + 12.0f * g) * cr_dv[2] * fm_exp2f(ng) * NCHAR[c].loose * (4294967296.0f / EN_SR));   /* dry
                                                  * clicks a second, the loose ones (the rest on the grid: noise_step) */
    {   /* (the state in locals: the compiler keeps it in registers, past the sends' stores) */
        uint32_t cn = cnoise;
        float lp1 = cr_lp, lp2 = cr_lp2, hp1 = cr_hp, cl = cr_cl, cr = cr_cr, fl = cr_fl, fr = cr_fr, cc = cr_cc;
        float ex = nz_ex, ck = nz_click, acc = nz_acc, b0 = nz_b[0], b1 = nz_b[1], b2 = nz_b[2], b3 = nz_b[3];
        float pl = nz_p[0], pr = nz_p[1], ql = nz_c[0], qr = nz_c[1];
        float exd = nz_exd;
        const float sw = 0.05f * cont;          /* (Tape: the hiss in) */
        const int32_t sj = nz_wait >= 0 && nz_wait < (int32_t)n ? nz_wait : -1;   /* (a hit strikes in this chunk) */
        const int rest = SHED && ex < 1e-4f && cont <= 0.0f &&
                         fm_fabsf(nz_lp[0]) + fm_fabsf(nz_lp[1]) + fm_fabsf(nz_lp[2]) < 2e-4f;
        /* (the second page: Width, Plate, Echo; PERFORM's Wet, -50..+50: the dry x2 .. x0.5, the sends x0.5 .. x2) */
        const float wv = (float)par[P_NWIDTH] * 0.02f, wa = 0.6f + 0.25f * wv, wb = 0.6f - 0.25f * wv,
                    wm = (float)mac[EN_MAC_DUB] * (1.0f / 50.0f), nw = fm_exp2f(-wm),
                    np = (float)par[P_NPLATE] * 0.02f * fm_exp2f(wm), ne = (float)par[P_NECHO] * 0.02f * fm_exp2f(wm);
        int ph = nz_ph, w = nz_w;
#define CRU() (cn = cn * 1664525u + 1013904223u, (float)(cn >> 8) * (1.0f / 16777216.0f))
        for (j = 0; j < n; j++) {
            float wn, h, x, l, r, f;
            cn = cn * 1664525u + 1013904223u;
            wn = (float)(int32_t)cn * (1.0f / 2147483648.0f);
            lp1 += lpc * (wn - lp1);             /* the Air */
            lp2 += lpc * (lp1 - lp2);
            hp1 += 0.15f * (lp2 - hp1);
            h = lp2 - hp1;
            if ((int32_t)j == sj) {              /* the next step's hit strikes, ahead of its tick */
                ex = nz_wv, exd = nz_wd;
                nz_D[0] = nz_wD[0], nz_D[1] = nz_wD[1], nz_D[2] = nz_wD[2];
            }
            if (j == 0 && nz_gck > 0.0f) {       /* a dry click on the tick */
                float s1 = CRU() < 0.5f ? -1.0f : 1.0f, pn = 0.3f + 0.4f * CRU(), a = nz_gck;
                cc = 0.05f + 0.3f * CRU();
                a *= 1.8f - 0.8f * cc;
                cl += a * s1 * (1.0f - pn);
                cr += a * s1 * pn;
                nev(0, a * vg * 3.0f, pn);
                nz_gck = 0.0f;
            }
            if (cn < thr) {                      /* a dry vinyl click, now and then (dull, near the middle) */
                float a = CRU(), s1 = CRU() < 0.5f ? -1.0f : 1.0f, pn = 0.3f + 0.4f * CRU();
                cc = 0.05f + 0.3f * CRU();
                a = (0.03f + 0.4f * a * a * a) * (1.8f - 0.8f * cc);
                cl += a * s1 * (1.0f - pn);
                cr += a * s1 * pn;
                nev(0, a * vg * 3.0f, pn);
            }
            fl += cc * (cl - fl);
            fr += cc * (cr - fr);
            cl *= 0.55f;
            cr *= 0.55f;
            x = wn * ex + h * sw + 0.3f * (fl + fr);   /* what rings the combs: the hit's burst (no crack), the hiss
                                                         * (Tape), the clicks */
            ck = 0.0f;
            ex *= exd;
            acc += x;
            if (++ph >= 4) {                     /* the combs and the band-pass, a quarter of the time */
                float y[3], o[3], m, sl, sr2;
                int q;
                ph = 0;
                m = acc * 0.25f;
                acc = 0.0f;
                if (dry >= 1.0f || rest)         /* (Dust: no resonance, the combs not run; shedding, rings quiet) */
                    o[0] = o[1] = o[2] = 0.0f;
                else
                for (q = 0; q < 3; q++) {        /* y = x + g LP(y[n - D]), read between two samples */
                    float d = nz_D[q], rd = (float)w - d, a, fb;
                    int i0, i1;
                    if (rd < 0.0f)
                        rd += (float)NZ_L;
                    i0 = (int)rd;
                    i1 = i0 + 1 < NZ_L ? i0 + 1 : 0;
                    a = rd - (float)i0;
                    fb = ((float)nz_buf[q * NZ_L + i0] + a * (float)(nz_buf[q * NZ_L + i1] - nz_buf[q * NZ_L + i0])) * (1.0f / 12000.0f);
                    nz_lp[q] += lpk * (fb - nz_lp[q]);
                    o[q] = cg * nz_lp[q];        /* (out: the ring alone, not the burst that struck it) */
                    y[q] = m + o[q];
                    nz_buf[q * NZ_L + w] = s16(y[q]);
                }
                if (++w >= NZ_L)
                    w = 0;
                sl = o[0] + wa * o[1] + wb * o[2] + dry * m;   /* (the root in the middle, the third left, the fifth
                                                                  * right: Width) */
                sr2 = o[0] + wb * o[1] + wa * o[2] + dry * m;
                b1 += bf * (sl - b0 - 0.9f * b1);    /* (Chamberlin band-passes, Q ~1.1: b1 the band, b0 the low) */
                b0 += bf * b1;
                b3 += bf * (sr2 - b2 - 0.9f * b3);
                b2 += bf * b3;
                pl = ql, pr = qr;
                ql = b1 * gres, qr = b3 * gres;
            }
            f = (float)ph * 0.25f;               /* (back to the full rate: a line from the last to the next) */
            l = pl + (ql - pl) * f;
            r = pr + (qr - pr) * f;
            esend[j] += (l + r) * (0.15f * ne);   /* (a little into the echo, Echo) */
            rsend[j] += (l + r) * 0.3f * np + h * gh * 0.4f;   /* (the plate, Plate) */
            l *= 0.75f * nw;                     /* (the dry -2.5 dB, less into the plate than before: further back,
                                                  * its breathing heard, not smoothed into the plate's wash; PERFORM's
                                                  * Wet: the dry down as it turns up) */
            r *= 0.75f * nw;
            l += h * gh + vg * fl;
            r += 0.9f * h * gh + vg * fr;
            crl[j] = l;
            crr[j] = r;
        }
#undef CRU
        cnoise = cn;
        cr_lp = fm_flush(lp1), cr_lp2 = fm_flush(lp2), cr_hp = fm_flush(hp1);
        cr_cl = fm_flush(cl), cr_cr = fm_flush(cr), cr_fl = fm_flush(fl), cr_fr = fm_flush(fr), cr_cc = cc;
        nz_wait = sj >= 0 ? -1 : nz_wait >= 0 ? nz_wait - (int32_t)n : -1;
        nz_exd = exd;
        nz_ex = fm_flush(ex), nz_click = ck, nz_acc = acc, nz_ph = ph, nz_w = w;
        nz_b[0] = fm_flush(b0), nz_b[1] = fm_flush(b1), nz_b[2] = fm_flush(b2), nz_b[3] = fm_flush(b3);
        nz_p[0] = pl, nz_p[1] = pr, nz_c[0] = ql, nz_c[1] = qr;
        for (i = 0; i < 3; i++)
            nz_lp[i] = fm_flush(nz_lp[i]);
    }
}

/* -------------------------------------------------------- the effects grid --- */
/* (BERLIN) REC held, the keys: each effect eased in while its key is held and out when let go, costing nothing until
 * then. The echo's (Repeat: the Sequence thrown into it; Time, Tape stop; Supernova: its feedback past full);
 * a master filter on everything (Low-pass, High-pass); Sweep the Sequence's own filter, swept; Phaser on all the dry
 * music; Reverb (the dry into the plate), Take-off (the Crash ending's noise falling into the echo), Freeze (the
 * cloud), Wash; Shimmer (the plate climbing in octaves: see the plate); Octave and Return in the sequences; Twitter, a
 * burst of bleeps (Space's) into the echo */
static float mf_lp = 14.2f, mf_hp = 4.3f;        /* the master filter: log2 of its cutoffs */
static float mf_c[2][4], mf_s[8];
static float fx_vt, fx_wash, fx_cf, fx_rep, fx_sw, fx_swl, fx_swm = 1.0f, fx_ph, fx_phl, fx_phz[2][6], fx_phlast[2];
static float tw_amp, tw_env, tw_f, tw_glide, tw_ph, tw_pan;
static int32_t tw_wait;
static uint32_t tw_rng = 0x1F83D9ABu;

static void fx_ctl(uint32_t n)                   /* once a chunk: the effects' easing, the filters' coefficients */
{
    const float t = (float)n / EN_SR;
    uint32_t on = fxm, rise = fxm & ~fxm_was;
    float lpt, hpt, ec;
    fxm_was = fxm;
    /* the echo: Time doubles it (halves it, if the tape is too short), Tape stop slows the tape to a quarter */
    fx_ts += ((on >> EN_FX_TAPESTOP & 1u ? 4.0f : 1.0f) - fx_ts) * fm_minf(t / (on >> EN_FX_TAPESTOP & 1u ? 0.7f : 0.25f), 1.0f);
    ec = on >> EN_FX_TIME & 1u ? (echo_len_t * 2.0f <= (float)(TAPE_N - 8) ? 2.0f : 0.5f) : 1.0f;
    fx_em = ec * fx_ts;
    fx_ek = fx_ts > 1.01f ? 0.004f : 0.001f;
    fx_rep += ((on >> EN_FX_REPEAT & 1u ? 1.0f : 0.0f) - fx_rep) * fm_minf(t / 0.03f, 1.0f);
    fx_nova += ((on >> EN_FX_SUPERNOVA & 1u ? 1.0f : 0.0f) - fx_nova) * fm_minf(t / (on >> EN_FX_SUPERNOVA & 1u ? 0.5f : 0.2f), 1.0f);
    /* the master filter's targets: Low-pass down to 350 Hz, High-pass up to 1.3 kHz */
    lpt = on >> EN_FX_LOWPASS & 1u ? 8.45f : 14.2f;
    hpt = on >> EN_FX_HIGHPASS & 1u ? 10.35f : 4.3f;
    mf_lp += (lpt - mf_lp) * fm_minf(t / (lpt < mf_lp ? 0.5f : 0.3f), 1.0f);
    mf_hp += (hpt - mf_hp) * fm_minf(t / (hpt > mf_hp ? 0.5f : 0.3f), 1.0f);
    if (mf_lp < 14.15f || mf_hp > 4.35f) {
        svf_coefs(mf_c[0], fm_exp2f(mf_lp), 1.0f);
        svf_coefs(mf_c[1], fm_exp2f(mf_hp), 1.2f);
    }
    /* Sweep: the Sequence's filter swept two octaves either way, a turn in ~3 s (from where it is: down first) */
    fx_sw += ((on >> EN_FX_SWEEP & 1u ? 1.0f : 0.0f) - fx_sw) * fm_minf(t / 0.2f, 1.0f);
    if (fx_sw > 0.001f) {
        fx_swl += t * 0.33f;
        if (fx_swl >= 1.0f)
            fx_swl -= 1.0f;
    } else {
        fx_swl = 0.5f;
    }
    fx_swm = fm_exp2f(2.0f * fx_sw * sine(fx_swl));
    fx_ph += ((on >> EN_FX_PHASER & 1u ? 1.0f : 0.0f) - fx_ph) * fm_minf(t / 0.1f, 1.0f);
    fx_vt += ((on >> EN_FX_REVERB & 1u ? 1.0f : 0.0f) - fx_vt) * fm_minf(t / 0.03f, 1.0f);
    fx_cf += ((on >> EN_FX_FREEZE & 1u ? 1.0f : 0.0f) - fx_cf) * fm_minf(t / 0.1f, 1.0f);
    fx_wash += ((on >> EN_FX_WASH & 1u ? 1.0f : 0.0f) - fx_wash) * fm_minf(t / (on >> EN_FX_WASH & 1u ? 0.4f : 0.8f), 1.0f);
    if (rise >> EN_FX_TAKEOFF & 1u)              /* Take-off: the arc's Crash, now (-9 dB: over any music, not only
                                                  * the ending's full) */
        ar_cr_env = 1.0f, ar_cr_f = 5000.0f, ar_cr_g = 0.35f;
    tw_amp += ((on >> EN_FX_TWITTER & 1u ? 1.0f : 0.0f) - tw_amp) * fm_minf(t / (on >> EN_FX_TWITTER & 1u ? 0.02f : 0.3f), 1.0f);
    /* Shimmer: its feedback in over ~50 ms, out over ~0.15 s; once out (~1 s after it is let go), the shifter stops */
    if (on >> EN_FX_SHIMMER & 1u || fx_shm > 0.0f) {
        fx_shm += ((on >> EN_FX_SHIMMER & 1u ? 1.0f : 0.0f) - fx_shm) * fm_minf(t / (on >> EN_FX_SHIMMER & 1u ? 0.05f : 0.15f), 1.0f);
        if (!(on >> EN_FX_SHIMMER & 1u) && fx_shm < 0.001f)
            shm_stop();
        else
            shm_level(t);
    }
}

static void phase6(float *bus, uint32_t n, float *z, float *lastp, float ca, float fb, float wet)
{                                                /* six all-passes (ca), fed back (fb), mixed in (wet) */
    float last = *lastp;
    uint32_t j;
    int st;
    for (j = 0; j < n; j++) {
        float y = bus[j] + fb * last;
        for (st = 0; st < 6; st++) {
            const float o = ca * y + z[st];
            z[st] = y - ca * o;
            y = o;
        }
        last = y;
        bus[j] = bus[j] * (1.0f - wet) + y * wet;
    }
    *lastp = last;
}
static void render_fx(uint32_t n)                /* Twitter (bleeps into the echo) and the Phaser (on the dry music) */
{
    uint32_t j;
    if (fxm >> EN_FX_TWITTER & 1u || tw_env > 1e-4f)
        for (j = 0; j < n; j++) {               /* a bleep every 30 .. 110 ms while held: a sine from 1.2 .. 4.8 kHz,
                                                  * gliding up to three octaves either way, gone in ~30 ms */
            float x;
            if (--tw_wait <= 0 && fxm >> EN_FX_TWITTER & 1u) {
                tw_rng = tw_rng * 1664525u + 1013904223u;
                tw_f = 1200.0f * fm_exp2f(2.0f * (float)(tw_rng >> 8) * (1.0f / 16777216.0f));
                tw_rng = tw_rng * 1664525u + 1013904223u;
                tw_glide = fm_exp2f((6.0f * (float)(tw_rng >> 8) * (1.0f / 16777216.0f) - 3.0f) / (0.05f * EN_SR));
                tw_rng = tw_rng * 1664525u + 1013904223u;
                tw_wait = (int32_t)(EN_SR * (0.03f + 0.08f * (float)(tw_rng >> 8) * (1.0f / 16777216.0f)));
                tw_pan = (tw_rng & 1u) ? 0.3f : 0.7f;
                tw_env = 1.0f;
            }
            if (tw_env > 1e-4f) {
                tw_ph += fm_clampf(tw_f, 100.0f, 15000.0f) * (1.0f / EN_SR);
                if (tw_ph >= 1.0f)
                    tw_ph -= 1.0f;
                tw_f *= tw_glide;
                x = sine(tw_ph) * tw_env * 0.07f * tw_amp;
                tw_env *= 0.99925f;
                mixl[j] += x * (1.0f - tw_pan);
                mixr[j] += x * tw_pan;
                esend[j] += x * 1.5f;
                rsend[j] += x * 0.5f;
            }
        }
    if (fx_ph > 0.005f) {                        /* the Phaser, deep: six all-passes a side swept ~150 Hz .. 3 kHz, a
                                                  * turn in ~5 s, the right a quarter on */
        int ch;
        for (ch = 0; ch < 2; ch++) {
            const float sw = 0.5f + 0.5f * sine(fx_phl + (ch ? 0.25f : 0.0f)),
                        tn = fm_tanf(FM_PI * 150.0f * fm_exp2f(4.3f * sw) * (1.0f / EN_SR));
            phase6(ch ? mixr : mixl, n, fx_phz[ch], &fx_phlast[ch], (1.0f - tn) / (1.0f + tn), 0.7f * fx_ph,
                   0.5f * fx_ph);
        }
        fx_phl += (float)n * (0.2f / EN_SR);
        if (fx_phl >= 1.0f)
            fx_phl -= 1.0f;
    }
}

static inline float svf_lp(const float *c, float *s, float x, float *bp)   /* one TPT SVF step: low-pass (bp: its band) */
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    s[0] = 2.0f * v1 - s[0];
    s[1] = 2.0f * v2 - s[1];
    *bp = v1;
    return v2;
}

/* the master filter on one side (s: its four states): the low-pass, then the high-pass */
static inline float mfx(float x, float *s)
{
    float bp, lp;
    x = svf_lp(mf_c[0], s, x, &bp);
    lp = svf_lp(mf_c[1], s + 2, x, &bp);
    return x - mf_c[1][3] * bp - lp;
}

static float ms_lp1, ms_lp2;                     /* the side's lows (mono below ~150 Hz) */
/* (BERLIN) the plucks: a saw (2600), two a little apart (Moog), a square (Synthi), a narrow pulse (Pulse), an FM
 * bell (Glass) or a sine and its octave under (Sub), through two low-passes (24 dB, the second resonant) swept down
 * by the envelope from three and a half octaves over the cutoff */
static inline float frac1(float x) { return x - (float)(int)x; }
/* (Pluck) a string's n samples (ks_strike): read a period back between two samples, averaged with the last read, the
 * loop's gain, written in again; out at the velocity's level (att: the first ~2 ms eased in), into the mix and the
 * sends as the other voices; amp follows Decay as theirs does (the string's fundamental with it, the partials over it
 * sooner), so the quietest is taken for a new note, the meter moves and the note ends as theirs do */
static void render_ks(pluck_t *v, int16_t *line, uint32_t n, float g, float es, float rv)
{
    const float adec = v->adec, fr = v->ks_fr, fb = v->ks_fb, kg = v->ks_g * g, gl = v->gl, gr = v->gr;
    float amp = v->amp, att = v->att, a1 = v->ks_a1, x1 = v->ks_x1, y = 0.0f;
    uint32_t w = v->ks_w, r = v->ks_r, j;
    for (j = 0; j < n; j++) {
        const float a = (float)line[r], x = a + fr * (a1 - a), s = 0.5f * (x + x1);
        line[w] = (int16_t)(s * fb);             /* (toward 0: a string's last whisper is 0, never a denormal) */
        a1 = a, x1 = x;
        if (++w >= KS_N)
            w = 0;
        if (++r >= KS_N)
            r = 0;
        y = s * kg * att;
        mixl[j] += y * gl;
        mixr[j] += y * gr;
        rsend[j] += y * rv;
        esend[j] += y * es;
        amp *= adec;
        att += (1.0f - att) * 0.03f;
    }
    v->amp = amp, v->att = att, v->ks_a1 = a1, v->ks_x1 = x1, v->last = y;
    v->ks_w = (uint16_t)w, v->ks_r = (uint16_t)r;
    if (amp < 0.0008f) {
        v->on = 0;
        midi(v->layer ? 0x83u : 0x81u, v->note, 0);
    }
}

static void render_plucks(uint32_t n)           /* (and Pluck's strings: render_ks) */
{
    int i;
    const float g0 = seq_vol * 1.2f, g1 = seq2_vol * 1.2f;
    for (i = 0; i < NPLUCK; i++) {
        pluck_t *v = &pluck[i];
        float amp = v->amp, env = v->env, att = v->att, ph = v->ph, ph2 = v->ph2;
        const float g = v->layer ? g1 : g0, dt = v->dt, dt2 = v->dt2, adec = v->adec, edec = v->edec,
                    /* (x2 since the echo report: the Sequences' repeats were barely heard against their dry) */
                    es = 1.2f * (sq_echo[v->layer] + fx_rep * (2.5f - sq_echo[v->layer]));   /* (Repeat: thrown in) */
        uint32_t j;
        float y = 0.0f;
        if (!v->on)
            continue;
        if (v->voice == SV_PLUCK) {              /* (a string: no oscillator, no filter) */
            render_ks(v, ks_line[i], n, g, es, 0.35f * sq_rev[v->layer]);
            continue;
        }
        for (j = 0; j < n; j++) {
            float x, bp;
            if (!(j & 15u)) {
                const float f = v->cut * fm_exp2f(3.5f * env) * fx_swm;   /* (the grid's Sweep) */
                svf_coefs(v->c1, f, 1.4f);
                svf_coefs(v->c2, f, 0.6f);
                if (v->voice >= SV_FM0)          /* (an FM voice's envelopes: held, the layer ends the note) */
                    fm_ctrl(&v->fm, 1, 16, 16);
            }
            switch (v->voice) {
            case 0: x = blep_saw(ph, dt); break;
            case 1: x = 0.6f * (blep_saw(ph, dt) + blep_saw(ph2, dt2)); break;
            case 2: x = blep_saw(ph, dt) - blep_saw(frac1(ph + 0.5f), dt); break;
            case 3: x = blep_saw(ph, dt) - blep_saw(frac1(ph + 0.18f), dt); break;
            case 4: x = sine(ph + 2.0f + 1.8f * env * sine(ph2)); break;
            case 5: x = sine(ph) + 0.6f * sine(ph2); break;
            default: x = SV_FM_G * fm_tick(&v->fm, dt); break;   /* (the FM voices: fm.c) */
            }
            ph += dt;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph2 += dt2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            x = svf_lp(v->c1, v->s1, x, &bp);
            x = svf_lp(v->c2, v->s2, x, &bp);
            y = x *= amp * att * g;
            mixl[j] += x * v->gl;
            mixr[j] += x * v->gr;
            rsend[j] += x * 0.35f * sq_rev[v->layer];
            esend[j] += x * es;
            amp *= adec;
            env *= edec;
            att += (1.0f - att) * 0.03f;
        }
        v->amp = amp, v->env = env, v->att = att, v->ph = ph, v->ph2 = ph2, v->last = y;
        if (amp < 0.0008f) {
            v->on = 0;
            midi(v->layer ? 0x83u : 0x81u, v->note, 0);
        }
    }
}

/* (BERLIN) the Drone's sound: its two banks, each voice low-passed (Colour), into a bus; Strings through the
 * ensemble (three taps of a short line, swaying 120 degrees apart, and a faster shimmer); the bus through the phaser
 * (six all-passes a side, swept slowly between ~200 Hz and ~2 kHz, the right a quarter turn on; Phaser: its depth and
 * feedback); then into the mix, the plate and a little echo */
#define ENS_N 1024
static int16_t ens_buf[ENS_N] EN_POOL;           /* (int16 at 1/8192: +-4) */
static uint32_t ens_w;
static float ens_ph, ens_ph2, drl[BLK], drr[BLK];
static float ens_tap(float d)                    /* d samples back, between two */
{
    const int i = (int)d;
    const float f = d - (float)i, a = (float)ens_buf[(ens_w - (uint32_t)i) & (ENS_N - 1u)],
                b = (float)ens_buf[(ens_w - (uint32_t)i - 1u) & (ENS_N - 1u)];
    return (a + (b - a) * f) * (1.0f / 8192.0f);
}

/* (the voices batch, item 3) Choir, the Drone's formant choir. Each note is two saws a little apart (a section's
 * voices), their pitches drifting slowly apart and back (a few cents, each note at its own rate, set once a chunk),
 * panned into the bus as the other voices are. Then, on each side, the one filter every note shares: a one-pole tilt
 * (a voice's source falls about 12 dB an octave, a saw's 6), three formant resonators in cascade, Colour's low-pass
 * (as the other voices have it), and then the Strings' ensemble. The filters are linear and every note's are the
 * same, so filtering the sum is the same as filtering each note; the cost doesn't grow with the notes (two banks in a
 * crossfade cost what one does, but for the saws).
 * - Resonators (Klatt 1980): y = a x + b y1 + c y2, r = exp(-pi B / fs), b = 2 r cos(2 pi F / fs), c = -r^2,
 *   a = 1 - b - c (unity at DC: the fundamentals under F1 pass, the formants stand over them, nothing over F3).
 *   Coefficients once a chunk.
 * - The vowel: on a path from dark to bright, 'oo' 'oh' 'ah' 'eh' 'ee' (F1..F3: Felucca's table, after Hillenbrand et
 *   al. 1995, men's means; bandwidths a little wider than one speaker's, for a section), between two of them by
 *   straight lines. Two vowels
 *   per octave of the Drone's brightness: Colour (its middle 'ah', -25 'oo', +25 'ee') and the filter motion (the
 *   Move page, Intro, Bridge, Rise and Close) move it, eased over about 0.1 s.
 * - Resonance narrows the formants (bandwidth x1.41 .. x0.71, eased as the vowel is) as well as raising the low-pass's
 *   peak, as for the others.
 * - Its level held to Strings' whatever the vowel, Resonance and Colour (review round 1: narrow formants and a dark
 *   Colour left it up to 7 dB louder): once a chunk, the power the formants and the tilt add to a saw's (its power
 *   falling as 1/f^2, summed over 1/6-octave steps from 80 Hz to 6.4 kHz), both under Colour's low-pass (exact, the
 *   TPT's own magnitude), from the coefficients as set; the gain the square root of the ratio, ramped over the chunk.
 * After Felucca's VOICE (eng_formant.c, Leo Kuroshita, Hügelton Instruments, GPL-3.0), written again in floating
 * point; its vowel table (F1..F3) is Felucca's VOWEL_F as it stands, itself Hillenbrand's means */
#define CV_CHOIR 5
#define CH_TILT 0.0950f                          /* (the tilt's one-pole: 1 - exp(-2 pi 700 / fs), its corner 700 Hz) */
#define CH_GAIN 0.57f                            /* (its level: about Strings' at the same Colour and Resonance) */
#define CH_NG 39                                 /* (the level's estimate: 80 Hz x 2^(i/6), i < 39) */
static float ch_gt[CH_NG][6];                    /* (each step: cos w, sin w, cos 2w, sin 2w, tan(w / 2), its weight) */
static float ch_tw[CH_NG];                       /* (the weight with the tilt's power) */
static uint8_t ch_gt_ok;
static float ch_g, ch_bw;                        /* the gain now (ramped to its next), Resonance's factor (eased) */
static const float CH_F[5][3] = {                /* 'oo' who'd, 'oh' hoed, 'ah' hod, 'eh' head, 'ee' heed (Hz): Felucca's
                                                  * VOWEL_F (after Hillenbrand et al. 1995), reordered dark to bright */
    {378.0f, 997.0f, 2343.0f}, {497.0f, 910.0f, 2459.0f}, {768.0f, 1333.0f, 2522.0f}, {580.0f, 1799.0f, 2605.0f},
    {342.0f, 2322.0f, 3000.0f}};
static const float CH_B[5][3] = {                /* their bandwidths (Hz) */
    {90.0f, 110.0f, 180.0f}, {100.0f, 110.0f, 180.0f}, {120.0f, 130.0f, 200.0f}, {100.0f, 140.0f, 200.0f},
    {80.0f, 160.0f, 220.0f}};
static float ch_p, ch_s[2][9];                   /* the vowel heard (0 'oo' .. 4 'ee'); each side's tilt, the three
                                                  * resonators' y1 y2, the low-pass */
#ifdef EN_HOST
volatile float en_ch_vowel;
volatile uint32_t en_ch_work;
static float ch_rc[3][3];                        /* (the resonators' coefficients as last set) */
float en_ch_gain(float hz)                       /* (host tests) the three resonators' gain at hz, as last set */
{
    const float w = FM_TWO_PI * hz * (1.0f / EN_SR), cw = fm_cosf(w), sw = fm_sinf(w), c2 = fm_cosf(2.0f * w),
                s2 = fm_sinf(2.0f * w);
    float g = 1.0f;
    int f;
    for (f = 0; f < 3; f++) {
        const float re = 1.0f - ch_rc[f][1] * cw - ch_rc[f][2] * c2, im = ch_rc[f][1] * sw + ch_rc[f][2] * s2;
        g *= ch_rc[f][0] * fm_rsqrtf(re * re + im * im + 1e-30f);
    }
    return g;
}
int en_ch_ok(void)                               /* (host tests) Choir's float state: finite, no denormals */
{
    int i, k, ok = ks_fine(ch_p) & ks_fine(ch_g) & ks_fine(ch_bw);
    for (i = 0; i < 2; i++)
        for (k = 0; k < 9; k++)
            ok &= ks_fine(ch_s[i][k]);
    return ok;
}
#endif
static void ch_gt_init(void)                     /* (once) the level estimate's steps */
{
    int i;
    for (i = 0; i < CH_NG; i++) {
        const float f = 80.0f * fm_exp2f((float)i * (1.0f / 6.0f)), w = FM_TWO_PI * f * (1.0f / EN_SR),
                    q = 1.0f - CH_TILT;
        ch_gt[i][0] = fm_cosf(w), ch_gt[i][1] = fm_sinf(w), ch_gt[i][2] = fm_cosf(2.0f * w);
        ch_gt[i][3] = fm_sinf(2.0f * w), ch_gt[i][4] = fm_tanf(0.5f * w);
        ch_gt[i][5] = 1.0f / f;                  /* (a saw's power a hertz, 1/f^2, times the step's width, ~f) */
        ch_tw[i] = ch_gt[i][5] * CH_TILT * CH_TILT / (1.0f - 2.0f * q * ch_gt[i][0] + q * q);
    }
    ch_gt_ok = 1;
}
/* the gain that holds the Choir at a saw's level under the same low-pass (c: its coefficients), rc its resonators */
static float ch_level(const float rc[3][3], const float *c)
{
    const float lg = c[1] / c[0], ilg = 1.0f / lg, k2 = c[3] * c[3];   /* (the low-pass's g = tan(pi fc / fs), k) */
    float ps = 0.0f, pc = 0.0f;
    int i, f;
    for (i = 0; i < CH_NG; i++) {
        const float *t = ch_gt[i], x = t[4] * ilg, x2 = x * x, dl = (1.0f - x2) * (1.0f - x2) + k2 * x2;
        float num = 1.0f, den = 1.0f;
        for (f = 0; f < 3; f++) {
            const float re = 1.0f - rc[f][1] * t[0] - rc[f][2] * t[2], im = rc[f][1] * t[1] + rc[f][2] * t[3];
            num *= rc[f][0] * rc[f][0];
            den *= re * re + im * im;
        }
        ps += t[5] / dl;
        pc += ch_tw[i] * num / (den * dl + 1e-30f);
    }
    return CH_GAIN * fm_sqrtf(ps / (pc + 1e-30f));
}
static void choir(uint32_t n, float g, const float *c)   /* both banks into drl/drr (cleared), then the shared filter */
{
    const float want = fm_clampf(2.0f + 2.0f * (2.0f * rel(P_CCOL) + fo[FO_DRONE]), 0.0f, 4.0f),
                bws = fm_exp2f(-0.5f * rel(P_CRES)), ease = fm_minf((float)n * (1.0f / 4410.0f), 1.0f);
    float rc[3][3], gt, dg;
    uint32_t j;
    int b, i, f;
    if (!ch_gt_ok)
        ch_gt_init();
    if (!ch_live) {
        memset(ch_s, 0, sizeof ch_s);
        ch_p = want;
        ch_bw = bws;
    } else {
        ch_p += (want - ch_p) * ease;
        ch_bw += (bws - ch_bw) * ease;
    }
    {
        const int v0 = ch_p >= 3.0f ? 3 : (int)ch_p;
        const float fr = ch_p - (float)v0;
        for (f = 0; f < 3; f++) {
            const float hz = CH_F[v0][f] + fr * (CH_F[v0 + 1][f] - CH_F[v0][f]),
                        bw = (CH_B[v0][f] + fr * (CH_B[v0 + 1][f] - CH_B[v0][f])) * ch_bw,
                        r = fm_expf(-FM_PI * bw * (1.0f / EN_SR));
            rc[f][1] = 2.0f * r * fm_cosf(FM_TWO_PI * hz * (1.0f / EN_SR));
            rc[f][2] = -r * r;
            rc[f][0] = 1.0f - rc[f][1] - rc[f][2];
#ifdef EN_HOST
            ch_rc[f][0] = rc[f][0], ch_rc[f][1] = rc[f][1], ch_rc[f][2] = rc[f][2];
#endif
        }
    }
    gt = ch_level(rc, c);
    if (!ch_live) {
        ch_g = gt;
        ch_live = 1;
    }
    dg = (gt - ch_g) / (float)n;
    for (b = 0; b < 2; b++) {
        float lvl = dr_lvl[b], d1[NDR], d2[NDR], gl[NDR], gr[NDR];
        const float to = dr_on[b] ? 1.0f : 0.0f;
        if (!dr_on[b] && lvl <= 0.0f)
            continue;
        for (i = 0; i < NDR; i++) {              /* (once a chunk: the two saws' rates, drifting; the pans, the pedal's
                                                  * level in them) */
            drv_t *v = &drv[b][i];
            const float w = 0.0025f * sine(v->vp), pg = i == EN_NPAD ? dr_pedal : 1.0f;
            d1[i] = v->dt * (1.0f + w);
            d2[i] = v->dt * (1.0045f - w);
            v->vp += (float)n * ((0.11f + 0.037f * (float)i) * (1.0f / EN_SR));
            if (v->vp >= 1.0f)
                v->vp -= 1.0f;
            gl[i] = v->gl * pg;
            gr[i] = v->gr * pg;
        }
        for (j = 0; j < n; j++) {
            float a, l = 0.0f, r = 0.0f;
            lvl = to > lvl ? fm_minf(lvl + dr_up, 1.0f) : fm_maxf(lvl - dr_down, 0.0f);
            a = lvl * lvl * g;
            for (i = 0; i < NDR; i++) {
                drv_t *v = &drv[b][i];
                const float x = blep_saw(v->ph, d1[i]) + blep_saw(v->ph2, d2[i]);
                v->ph += d1[i];
                if (v->ph >= 1.0f)
                    v->ph -= 1.0f;
                v->ph2 += d2[i];
                if (v->ph2 >= 1.0f)
                    v->ph2 -= 1.0f;
                l += x * gl[i];
                r += x * gr[i];
            }
            drl[j] += a * l;
            drr[j] += a * r;
        }
        dr_lvl[b] = lvl;
    }
    for (b = 0; b < 2; b++) {                    /* each side: the tilt, F1 -> F2 -> F3, the low-pass */
        float *s = ch_s[b], *d = b ? drr : drl, gj = ch_g;
        for (j = 0; j < n; j++) {
            float y, x, bp;
            gj += dg;
            s[0] += CH_TILT * (d[j] - s[0]);
            y = rc[0][0] * s[0] + rc[0][1] * s[1] + rc[0][2] * s[2];
            s[2] = s[1], s[1] = y;
            x = rc[1][0] * y + rc[1][1] * s[3] + rc[1][2] * s[4];
            s[4] = s[3], s[3] = x;
            y = rc[2][0] * x + rc[2][1] * s[5] + rc[2][2] * s[6];
            s[6] = s[5], s[5] = y;
            d[j] = gj * svf_lp(c, s + 7, y, &bp);
        }
        for (i = 0; i < 9; i++)                  /* (at level 0 the states die away: never into denormals) */
            s[i] = fm_flush(s[i]);
    }
    ch_g = gt;
#ifdef EN_HOST
    en_ch_vowel = ch_p;
    en_ch_work += n;
#endif
}

static void render_drone(uint32_t n)
{
    const int cv = cvoice < EN_NCV ? cvoice : 0;
    const float g = mutes & EN_M_CHORD ? 0.0f : chord_vol * 0.072f, fc = 1800.0f * fm_exp2f(2.0f * rel(P_CCOL)) * fo_mul(FO_DRONE), k = fm_clampf(1.2f - 0.9f * rel(P_CRES), 0.2f, 2.0f);
    float c[4], cf1[4], cf2[4];
    uint32_t j;
    int b, i;
    if (!dr_on[0] && !dr_on[1] && dr_lvl[0] <= 0.0f && dr_lvl[1] <= 0.0f) {
        ch_live = 0;                             /* (Choir from rest when it next sounds) */
        return;
    }
    svf_coefs(c, fc, k);
    svf_coefs(cf1, 650.0f, 0.3f);                /* (Horn: two formants, an 'ah') */
    svf_coefs(cf2, 1080.0f, 0.35f);
    for (j = 0; j < n; j++)
        drl[j] = drr[j] = 0.0f;
    if (cv == CV_CHOIR)
        choir(n, g, c);
    else
        ch_live = 0;
    for (b = 0; b < 2 && cv != CV_CHOIR; b++) {
        float lvl = dr_lvl[b];
        const float to = dr_on[b] ? 1.0f : 0.0f;
        if (!dr_on[b] && lvl <= 0.0f)
            continue;
        if (cv >= CV_FM0)                        /* (the FM pads: each voice on this pad's recipe, from rest if it was
                                                  * on another (turned onto it mid-chord); its envelopes a chunk on) */
            for (i = 0; i < NDR; i++) {
                drv_t *v = &drv[b][i];
                if (v->fm.rec != CV_FM_REC[cv - CV_FM0])
                    fm_start(&v->fm, CV_FM_REC[cv - CV_FM0], v->note), v->fy0 = v->fy1 = 0.0f;
                fm_ctrl(&v->fm, 1, n, (n + 1u - dr_fmpar) / 2u);   /* (n samples on; ticked on half of them) */
            }
        for (j = 0; j < n; j++) {
            float a;
            lvl = to > lvl ? fm_minf(lvl + dr_up, 1.0f) : fm_maxf(lvl - dr_down, 0.0f);
            a = lvl * lvl * g;
            for (i = 0; i < NDR; i++) {
                drv_t *v = &drv[b][i];
                float x, bp;
                switch (cv) {
                case 0: x = organ(v->ph); break;
                case 1: x = blep_saw(v->ph, v->dt); break;
                case 2: {
                    float f1, f2, s0 = blep_saw(v->ph, v->dt);
                    svf_lp(cf1, v->f1, s0, &f1);
                    svf_lp(cf2, v->f2, s0, &f2);
                    x = 1.6f * f1 + 1.2f * f2;
                    break;
                }
                case 3: x = 0.6f * (blep_saw(v->ph, v->dt) + blep_saw(v->ph2, v->dt * 1.006f)); break;
                case 4: x = sine(v->ph + 2.0f + 0.7f * sine(v->ph2)); break;
                default:                         /* (the FM pads: at half the rate, every other sample between two
                                                  * (the cost: a pad six sines a voice, ten voices in a crossfade);
                                                  * the Drone's notes are low, Colour's low-pass after) */
                    if (!((j + dr_fmpar) & 1u)) {
                        v->fy0 = v->fy1;
                        v->fy1 = CV_FM_G * fm_tick(&v->fm, 2.0f * v->dt);
                        x = v->fy0;
                    } else
                        x = 0.5f * (v->fy0 + v->fy1);
                    break;
                }
                v->ph += v->dt;
                if (v->ph >= 1.0f)
                    v->ph -= 1.0f;
                v->ph2 += cv == 4 ? 3.5f * v->dt : 1.006f * v->dt;
                if (v->ph2 >= 1.0f)
                    v->ph2 -= (float)(int)v->ph2;
                x = svf_lp(c, v->s, x, &bp) * a * (i == EN_NPAD ? dr_pedal : 1.0f);
                drl[j] += x * v->gl;
                drr[j] += x * v->gr;
            }
        }
        dr_lvl[b] = lvl;
    }
    dr_fmpar = (uint8_t)((dr_fmpar + n) & 1u);
    if (cv == 1 || cv == CV_CHOIR) {             /* Strings and Choir: the ensemble */
        const float r = 0.6f * (1.0f / EN_SR), r2 = 6.0f * (1.0f / EN_SR);
        for (j = 0; j < n; j++) {
            float m = 0.5f * (drl[j] + drr[j]), t0, t1, t2;
            ens_buf[ens_w & (ENS_N - 1u)] = (int16_t)(fm_clampf(m, -3.99f, 3.99f) * 8192.0f);
            t0 = ens_tap(441.0f + 130.0f * sine(ens_ph) + 6.0f * sine(ens_ph2));
            t1 = ens_tap(441.0f + 130.0f * sine(ens_ph + 0.3333f) + 6.0f * sine(ens_ph2 + 0.3333f));
            t2 = ens_tap(441.0f + 130.0f * sine(ens_ph + 0.6667f) + 6.0f * sine(ens_ph2 + 0.6667f));
            drl[j] = 0.3f * drl[j] + 0.5f * (t0 + 0.5f * t1);
            drr[j] = 0.3f * drr[j] + 0.5f * (t2 + 0.5f * t1);
            ens_w++;
            ens_ph += r;
            if (ens_ph >= 1.0f)
                ens_ph -= 1.0f;
            ens_ph2 += r2;
            if (ens_ph2 >= 1.0f)
                ens_ph2 -= 1.0f;
        }
    }
    if (ph_depth > 0.005f) {                     /* the phaser */
        int ch;
        for (ch = 0; ch < 2; ch++) {
            const float sw = 0.5f + 0.5f * sine(ph_lfo + (ch ? 0.25f : 0.0f)),
                        t = fm_tanf(FM_PI * 200.0f * fm_exp2f(3.3f * sw) * (1.0f / EN_SR));
            phase6(ch ? drr : drl, n, phz[ch], &ph_last[ch], (1.0f - t) / (1.0f + t), 0.55f * ph_depth, 0.5f * ph_depth);
        }
        ph_lfo += (float)n * (0.07f / EN_SR);     /* (a turn in ~14 s) */
        if (ph_lfo >= 1.0f)
            ph_lfo -= 1.0f;
    }
    for (j = 0; j < n; j++) {
        mixl[j] += drl[j];
        mixr[j] += drr[j];
        rsend[j] += 0.5f * dr_rev * (drl[j] + drr[j]);
        esend[j] += 0.12f * dr_echo * (drl[j] + drr[j]);
    }
}

static void render_lead(uint32_t n)
{
    const int lv = (unsigned)par[P_LVOICE] < EN_NBV ? par[P_LVOICE] : 0;
    const float g = lead_vol * 0.45f, vib = (float)par[P_VIB] * 0.004f + (lv == 3 ? 0.25f : 0.0f), gl = 0.84f, gr = 1.08f,
                tone = ld_tone * fo_mul(FO_LEAD);   /* (Tone, moved by the filter motion) */
    uint32_t j;
    float dt = 0.0f;
    if (!lead.on && lead.env < 1e-5f) {
        en_bass_level = 0.0f;                    /* (the meter, at rest) */
        return;
    }
    if (lv >= BV_FM0 && lead.fm.rec != BV_FM_REC[lv - BV_FM0])   /* (turned onto an FM voice as it sounds: from rest) */
        fm_start(&lead.fm, BV_FM_REC[lv - BV_FM0], lead.note);
    for (j = 0; j < n; j++) {
        float x, bp, a;
        if (!(j & 15u)) {                        /* the glide, the vibrato, the filter: every 16 */
            const float vd = vib * fm_clampf((lead.vt - 0.4f) * 2.5f, 0.0f, 1.0f), f = mtof(lead.cur + vd * sine(lead.vph));
            dt = f * (1.0f / EN_SR);
            svf_coefs(lead.c1, f * (3.0f + 6.0f * lead.fenv) * tone, 0.9f);
            svf_coefs(lead.c2, f * (3.0f + 6.0f * lead.fenv) * tone, 0.7f);
            lead.vt += 16.0f / EN_SR;
            lead.vph += 16.0f * 5.5f / EN_SR;
            if (lead.vph >= 1.0f)
                lead.vph -= 1.0f;
            lead.fenv *= 0.985f;                 /* (~0.4 s) */
            if (lv >= BV_FM0)                    /* (an FM voice's envelopes: let go with the Lead) */
                fm_ctrl(&lead.fm, lead.on, 16, 16);
        }
        lead.cur += ((float)lead.note - lead.cur) * ld_gk;
        switch (lv) {
        case 0: x = 0.6f * blep_saw(lead.ph, dt) + 0.4f * (blep_saw(lead.ph, dt) - blep_saw(frac1(lead.ph + 0.35f), dt)); break;
        case 1: x = 0.45f * (blep_saw(lead.ph, dt) + blep_saw(lead.ph2, dt * 1.004f) + blep_saw(lead.ph3, dt * 0.996f)); break;
        case 2: x = sine(lead.ph) + 0.08f * lead.env * (frand() - 0.5f); break;
        case 3: x = sine(lead.ph); break;
        case 4: x = blep_saw(lead.ph, dt) - blep_saw(frac1(lead.ph + 0.5f), dt); break;
        default: x = BV_FM_G * fm_tick(&lead.fm, dt); break;   /* (the FM voices) */
        }
        lead.ph += dt;
        if (lead.ph >= 1.0f)
            lead.ph -= 1.0f;
        lead.ph2 += dt * 1.004f;
        if (lead.ph2 >= 1.0f)
            lead.ph2 -= 1.0f;
        lead.ph3 += dt * 0.996f;
        if (lead.ph3 >= 1.0f)
            lead.ph3 -= 1.0f;
        if (lv != 2 && lv != 3) {
            x = svf_lp(lead.c1, lead.s1, x, &bp);
            x = svf_lp(lead.c2, lead.s2, x, &bp);
        }
        lead.env += ((lead.on ? 1.0f : 0.0f) - lead.env) * (lead.on ? ld_att : 0.00008f);   /* (Attack in, ~300 ms out) */
        a = x * lead.env * g;
        mixl[j] += a * gl;
        mixr[j] += a * gr;
        rsend[j] += 0.45f * ld_rev * a;
        esend[j] += 0.6f * ld_echo * a;
    }
    en_bass_level = lead.env;
}

/* (BERLIN) the arc's Crash: a take-off, noise through a band-pass falling five octaves from ~5 kHz in 4 s, dying in
 * ~4 s, mostly into the echo */
static float ar_cr_s[2];
static void render_crash(uint32_t n)
{
    float c[4], bp;
    uint32_t j;
    if (ar_cr_env < 1e-4f)
        return;
    svf_coefs(c, ar_cr_f, 0.25f);
    for (j = 0; j < n; j++) {
        float x;
        svf_lp(c, ar_cr_s, atm_noise(), &bp);
        x = 0.5f * ar_cr_g * bp * ar_cr_env;
        mixl[j] += x;
        mixr[j] += x;
        esend[j] += 1.2f * x;
        rsend[j] += 0.5f * x;
        ar_cr_env *= 0.99996f;
    }
    ar_cr_f = fm_maxf(ar_cr_f * fm_exp2f(-(float)n * (5.0f / (4.0f * EN_SR))), 120.0f);
}

/* (BERLIN) the meters: each layer's activity, 0..1, at its level (a mute: 0), up at once and down over ~half a second */
static void layer_act(void)
{
    float a[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int i;
    if (!(mutes & EN_M_CHORD))
        a[0] = fm_maxf(dr_lvl[0], dr_lvl[1]) * (float)par[P_CHORD] * 0.01f;
    for (i = 0; i < NPLUCK; i++)
        if (pluck[i].on)
            a[pluck[i].layer ? 3 : 1] = fm_maxf(a[pluck[i].layer ? 3 : 1],
                                                pluck[i].amp * (float)par[pluck[i].layer ? P_SEQ2 : P_SEQ] * 0.01f);
    if (ld_audible())
        a[2] = lead.env * (float)par[P_LEAD] * 0.01f;
    a[4] = atm_g_out > 0.0f ? fm_sqrtf(atm_g_out) : 0.0f;
    for (i = 0; i < 5; i++)
        en_layer_act[i] = fm_maxf(fm_clampf(a[i], 0.0f, 1.0f), en_layer_act[i] * 0.97f);
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    float epk;
    const float rg = rev_send * rev_in_gain;
    /* (the cloud listens as if Reverb were at 63 at least, so Cloud works with little reverb too) */
    const float cl_in = rev_send > 0.0001f ? fm_maxf(rev_send, 0.4f) / rev_send : 0.0f;
    const float push = fm_maxf(perf_echo, ar_push);   /* (and the arc's swell) */
    float eg = (echo_send + (1.0f - echo_send) * push) * ein_gain, ca = cloud_amt;
    const int shm_on = fx_shm > 0.0f;            /* (Shimmer: held, or its feedback easing out) */
    const float shm_g = shm_k * fx_shm * rev_in_gain;   /* (bounded by the music; closed with the plate's input in
                                                  * dub hold) */
    const float dry_pl = fx_vt + fx_wash + fx_shm > 0.001f ? 0.35f * fx_vt + 0.3f * fx_wash + SHM_DRY * fx_shm : 0.0f;
    /* PERFORM's Open and Shut on the music, the kick left out on top: Shut a low-pass down to ~230 Hz, Open a
     * resonant high-pass up to ~400 Hz (thin and bright), both swept with perf_sw */
    const float shut = fm_maxf(-perf_sw, 0.0f), open = fm_maxf(perf_sw, 0.0f);
    if (shut > 0.01f)
        svf_coefs(mb_c, 14000.0f * fm_exp2f(-1.9f * shut), 0.55f);
    else if (open > 0.01f)
        svf_coefs(mb_c, 25.0f * fm_exp2f(1.33f * open), 0.5f);
    else
        mb_s[0] = mb_s[1] = mb_s[2] = mb_s[3] = 0.0f;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
    lfo_step();
    render_plucks(n);
    render_drone(n);
    render_lead(n);
    render_crash(n);
    widen(n);                                    /* (the chord's and the pings' dry, wider) */
    if (atm_char == ATM_TAPE)
        render_crackle(n);
    else
        render_atmos(n);
    layer_act();
    if (fxm || tw_env > 1e-4f || fx_ph > 0.005f)
        render_fx(n);
    eg += (1.0f - eg) * fx_wash;                 /* Wash: everything into the echo */
    epk = en_echo_pk;
    for (j = 0; j < n; j++) {
        float wl, wr, l, r, el, er, rin, cl, cr, sd, dry = 1.0f - 0.75f * fx_wash;
        echoes(esend[j] * eg, &el, &er);
        epk = fm_maxf(epk, fm_fabsf(el + er));   /* (the screen's ripples: the echo as it comes out) */
        rin = (rsend[j] * rg + (el + er) * 0.12f * rev_send) * 0.8f;   /* (the classics pass: less of the echo into
                                                  * the plate, 0.25 before: the repeats stay distinct) */
        if (dry_pl > 0.0f)                       /* Reverb, Wash, Shimmer: the dry into the plate */
            rin += (mixl[j] + mixr[j]) * dry_pl;
        cloud(rin * ca * 2.5f * cl_in + pl_back * cloud_amt * 0.22f, &cl, &cr);   /* the grains, into the plate: they bloom;
                                                 * the plate's tail back into the line: the wash grows on itself */
        {   /* the plate, every other sample (its input the pair's mean, its output a line between) */
            const float pin = rin * (1.0f - 0.5f * cloud_amt) + (cl + cr) * 0.8f;
            if ((pl_ph ^= 1u) != 0u) {
                pl_acc = pin;
                if (shm_on) {                    /* (Shimmer: the music's level into the plate, every other sample;
                                                  * the cloud's return left out: it carries the plate's own tail) */
                    shm_em += rin * rin;
                    shm_nm++;
                }
                wl = pl_ol, wr = pl_or;
            } else {
                float nl, nr, x = 0.5f * (pl_acc + pin);
                if (shm_on)                      /* (Shimmer: the plate's last out, an octave up, back in) */
                    x += shimmer(0.5f * (pl_ol + pl_or), shm_g);
                reverb(x, &nl, &nr);
                wl = 0.5f * (pl_ol + nl), wr = 0.5f * (pl_or + nr);
                pl_ol = nl, pl_or = nr;
            }
        }
        pl_back = 0.5f * (wl + wr);
        cl *= 1.0f + 9.0f * fx_cf;               /* (Freeze: the held cloud brought forward) */
        cr *= 1.0f + 9.0f * fx_cf;
        l = mixl[j] * dry + 1.2f * el + 1.1f * wl + 1.4f * cl;   /* (1.3.0: the Echoplex's taps out) */
        r = mixr[j] * dry + 1.2f * er + 1.1f * wr + 1.4f * cr;
        sd = 0.5f * (l - r);                     /* mono below ~80 Hz (the classics pass; was ~150): the side's lows
                                                  * taken out (two poles), so the
                                                  * stabs' and pings' low partials, the echo's and the plate's, sit in
                                                  * the middle with the bass */
        ms_lp1 += 0.0113f * (sd - ms_lp1);
        ms_lp2 += 0.0113f * (sd - ms_lp1 - ms_lp2);
        sd = ms_lp1 + ms_lp2;
        l = l - sd;
        r = r + sd;
        if (shut > 0.01f) {
            float bp;
            l = svf_lp(mb_c, mb_s, l, &bp);
            r = svf_lp(mb_c, mb_s + 2, r, &bp);
        } else if (open > 0.01f) {               /* (the high-pass: what the low-pass and its band leave) */
            float bp, lp = svf_lp(mb_c, mb_s, l, &bp);
            l -= lp + mb_c[3] * bp;
            lp = svf_lp(mb_c, mb_s + 2, r, &bp);
            r -= lp + mb_c[3] * bp;
        }
        l = (l + crl[j]) * 0.75f * gain;   /* (1.3.0: +1.5 dB, what the
                                                                                  * re-balance took, from the headroom;
                                                                                  * the classics pass: +2 dB more, the
                                                                                  * user's ear; the loop held, the
                                                                                  * level steadier) */
        r = (r + crr[j]) * 0.75f * gain;
        if (mf_lp < 14.15f || mf_hp > 4.35f) {   /* the master filter */
            l = mfx(l, mf_s);
            r = mfx(r, mf_s + 4);
        }

        l = soft(l);                             /* (1.3.0: a rational curve, not tanh: the same below the clip,
                                                  * a fraction of the time) */
        r = soft(r);
        out[2 * j] = (int32_t)(l * 8300000.0f);
        out[2 * j + 1] = (int32_t)(r * 8300000.0f);
        scope_acc += l + r;                      /* (the ripples' outlines: the output) */
        if (++scope_ph >= 8u) {                  /* (an eighth of the rate, each the mean of its eight) */
            scope_ph = 0;
            en_scope[en_scope_w & (EN_SCOPE_N - 1u)] = (int16_t)(fm_clampf(scope_acc, -16.0f, 16.0f) * 2000.0f);
            scope_acc = 0.0f;
            en_scope_w++;
        }
    }
    en_echo_pk = epk;
    ms_lp1 = fm_flush(ms_lp1);
    ms_lp2 = fm_flush(ms_lp2);
}

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    mods_step(n);
    fx_ctl(n);
    heads_step();
    fo_step(n);
    pl_decay += ((en_frozen ? 0.9995f : pl_decay_t) - pl_decay) * 0.002f;
    pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
    rev_in_gain += ((en_frozen ? 0.0f : 1.0f) - rev_in_gain) * 0.01f;
    ein_gain += ((en_frozen ? 0.0f : 1.0f) - ein_gain) * 0.05f;
    fb_cap += ((en_playing ? 2.0f : 0.9f) - fb_cap) * fm_minf((float)n / (2.0f * EN_SR), 1.0f);
    {   /* the performance's pushes, eased in and out (the filter swept over ~400 ms, three octaves; the echo ~40 ms) */
        /* (the Filter knob: the same sweep, quicker to follow a hand; the keys on top of it, to the same ends) */
        /* (the knob reaches further than the keys: Shut down to ~120 Hz, Open up to ~1.2 kHz) */
        float t = fm_clampf((float)mac[EN_MAC_FILTER] * (mac[EN_MAC_FILTER] < 0 ? 0.072f : 0.084f) +
                            (perf & EN_PF_OPEN ? 3.0f : 0.0f) - (perf & EN_PF_CLOSE ? 3.0f : 0.0f), -3.6f, 4.2f);
        perf_sw += (t - perf_sw) * fm_minf((float)n / ((perf & (EN_PF_OPEN | EN_PF_CLOSE) ? 0.4f : 0.12f) * EN_SR), 1.0f);
        perf_echo += ((perf & EN_PF_ECHO ? 1.0f : 0.0f) - perf_echo) * fm_minf((float)n / (0.04f * EN_SR), 1.0f);
    }
    if (echo_len > 0.0f)                        /* for the screen's ripples */
        en_echo_ms = echo_len * (1000.0f / TAPE_SR);
    en_echo_g = en_frozen ? 1.0f : fm_minf(echo_fb + 0.12f * perf_echo, fb_cap) * 0.82f;
    en_echo_in = echo_send + (1.0f - echo_send) * perf_echo;
    cl_dens = cloud_amt > 0.0f ? fm_clampf(0.25f + 0.5f * cloud_amt, 0.05f, 1.0f) : 0.0f;
    if (fx_cf > 0.01f)                          /* (Freeze: the grains thick, even at Cloud 0) */
        cl_dens = fm_maxf(cl_dens, fx_cf);
    for (i = 0; i < EN_NLOOP; i++)
        en_loop_glow[i] *= 0.985f;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    if (en_load > 75u)                           /* (the last block ran heavy: shed for half a second; the classics
                                                  * pass: from 75, was 80, and shedding more) */
        shed_left = (int32_t)(0.5f * EN_SR);
    else if (shed_left > 0)
        shed_left -= (int32_t)n;
    drain();
    if (en_ext && ++ext_age > 100u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
        }
        if (k > n)
            k = n;
        if (tick_wait > 0 && (uint32_t)tick_wait < k)   /* (a swung sixteenth) */
            k = (uint32_t)tick_wait;
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        if (tick_wait > 0 && (tick_wait -= (int32_t)k) <= 0) {
            tick_wait = 0;
            if (en_playing)
                tick();
        }
        out += 2 * k;
        n -= k;
    }
}
