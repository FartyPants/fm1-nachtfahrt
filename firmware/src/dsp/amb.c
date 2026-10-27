/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (amb.h). One compilation unit, -O2. */
#include "amb.h"
#include "fastmath.h"
#include "seq.c"

#define EN_DUB 0                                /* (BERLIN: the dub's drums and ping loops, silent; their code goes
                                                 * once the arrangement no longer leans on it) */

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
const char *const EN_SEC_NAME[EN_NSEC] = {"Emerge", "Flow", "Solo", "Drift"};   /* (BERLIN's: the arc) */

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Chord", 0, 100, 60},    {"Sequence", 0, 100, 70},    {"Lead", 0, 100, 40},     {"Sequence 2", 0, 100, 60},
    {"Density", 0, 100, 60},  {"Evolve", 0, 100, 40},   {"Drift", 0, 6, 4},       {"Repeat A", 0, 100, 100},
    {"Echo", 0, 100, 55},     {"Echo time", 0, 5, 1},   {"Reverb", 0, 100, 35},   {"Feedback", 0, 100, 60},
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
    {"Ending", 0, 2, 0}, {"Drone reverb", 0, 100, 50}, {"Drone echo", 0, 100, 50}, {"Seq2 echo", 0, 100, 50},
    {"Chord alg", 0, 9, 0},   {"Chord ratio", -50, 50, 0}, {"Chord fb", 0, 100, 0}, {"Chord LFO", 0, 100, 0},
    {"Decay 2", 0, 100, 35},    {"Ping ratio", -50, 50, 0}, {"Ping fb", 0, 100, 0}, {"Ping LFO", 0, 100, 0},
    {"Bass alg", 0, 9, 0},    {"Lead attack", 0, 100, 10}, {"Bass fb", 0, 100, 0}, {"Bass LFO", 0, 100, 0},
    {"Root 1", 0, 11, 0}, {"Root 2", 0, 11, 0}, {"Root 3", 0, 11, 0}, {"Root 4", 0, 11, 0},
    {"Root 5", 0, 11, 0}, {"Root 6", 0, 11, 0}, {"Root 7", 0, 11, 0}, {"Root 8", 0, 11, 0},
    {"Kind 1", 0, EN_NCQ - 1, 0}, {"Kind 2", 0, EN_NCQ - 1, 0}, {"Kind 3", 0, EN_NCQ - 1, 0}, {"Kind 4", 0, EN_NCQ - 1, 0},
    {"Kind 5", 0, EN_NCQ - 1, 0}, {"Kind 6", 0, EN_NCQ - 1, 0}, {"Kind 7", 0, EN_NCQ - 1, 0}, {"Kind 8", 0, EN_NCQ - 1, 0},
    {"Type 1", 0, EN_NCX - 1, 0}, {"Type 2", 0, EN_NCX - 1, 0}, {"Type 3", 0, EN_NCX - 1, 0}, {"Type 4", 0, EN_NCX - 1, 0},
    {"Type 5", 0, EN_NCX - 1, 0}, {"Type 6", 0, EN_NCX - 1, 0}, {"Type 7", 0, EN_NCX - 1, 0}, {"Type 8", 0, EN_NCX - 1, 0},
    {"Play", 0, 6, 1},
    {"Steps", -32768, 32767, 0}, {"Steps 2", -32768, 32767, 0}, {"Octaves", -32768, 32767, 0},
    {"Octaves 2", -32768, 32767, 0},  {"Skips", -32768, 32767, 0},
    {"Accents", -32768, 32767, 0},   {"Accents 2", -32768, 32767, 0},   {"Seq2 in", 0, 64, 8},
    {"Rim acc", -32768, 32767, 0},    {"Clap acc", -32768, 32767, 0},
    {"Chance", 0, 100, 100}, {"Chance 2", 0, 100, 100}, {"Lead in", 0, 64, 16}, {"Rim chance", 0, 100, 100},
    {"Clap chance", 0, 100, 5},
    {"Accent", 0, 100, 50}, {"Seq play", 0, 1, 0},
    {"Noise tone", -50, 50, 0}, {"Noise density", -50, 50, 0}, {"Air", -50, 50, 0}, {"Noise drift", -50, 50, 0},
    {"Noise", 0, EN_NNCHAR - 1, 0},
    {"Phaser", 0, 100, 40},   {"Cutoff 2", 0, 100, 40},  {"Phrases", 0, 100, 50},   {"Seq2 reverb", 0, 100, 50},
    {"Lead tone", 0, 100, 50}, {"Length 2", 3, 16, 7}, {"Blur", 0, 100, 50}, {"Echo tone", 0, 100, 50},
    {"Width", 0, 100, 50},    {"Ring", 0, 100, 50},
    {"Noise width", 0, 100, 50}, {"Noise plate", 0, 100, 50}, {"Noise echo", 0, 100, 50}, {"Emerge", 0, 100, 0},
    {"Noise LFO", 0, 100, 0}, {"Arc", 0, 100, 0},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

/* (BERLIN) the pieces: eight recipes for the whole machine (the FM-1's PRESETS, the web app's cards), each what it
 * sets over the defaults (PARAMS), in pairs, to -1. Tune and MIDI out are never set here: they belong to the player.
 * Keys and tempos are starting points (spec 6: from analysis sites, to tune by ear); the Lead is dreamy in all of them
 * (its reverb and echo 90, the defaults: the user); an Atmosphere only where it suits, of the four the user likes.
 * (Voices: the Sequence's 2600 Moog Synthi Pulse Glass, Sequence 2's and Sub; the Drone's Organ Strings Choir Saws
 *  Glass; the Lead's Odyssey Mini Flute Theremin Synthi. Modes: 0 Ionian, 1 Dorian, 2 Phrygian, 5 Aeolian. Repeat
 *  times: 1/8 1/8. 1/4 1/4. 1/2 1 bar. Drift: 2 4 8 16 32 64 bars, Hold. Play: the walk Home, Wander, Free. Grid:
 *  8ths, 16ths. Atmosphere: None Wind Surf Space Air Tape. Ending: Crash Supernova Fade) */
static const int16_t PC_TIMEWIND[] = {        /* Schulze, Bayreuth Return: one sequence, an organ through the phaser */
    P_TEMPO, 96, P_ROOT, 2, P_MODE, MODE_AEOLIAN,
    P_SVOICE, 0, P_SEQLEN, 16, P_SEQGRID, 1, P_DENSITY, 65, P_SEQ2, 0,
    P_TAPB, 50, P_TAPBT, 2,
    P_CVOICE, 0, P_PHASER, 60, P_DRIFT, 3, P_PMODE, EN_WALK_HOME,
    P_LVOICE, 0, P_LDIN, 32,
    P_NCHAR, 1, P_CRACKLE, 30,
    P_EMERGE, 10, P_ARC, 25, P_ENDING, 0, P_MOTION, 45, -1};
static const int16_t PC_WAHNFRIED[] = {       /* Schulze, Wahnfried 1883: beatless, roots walking with no home */
    P_TEMPO, 80, P_ROOT, 10, P_MODE, MODE_DORIAN,
    P_SEQ, 0, P_SEQ2, 0,
    P_CHORD, 60, P_CVOICE, 2, P_PEDAL, 85, P_PHASER, 50, P_CRES, 25, P_DRIFT, 2, P_PMODE, EN_WALK_FREE,
    P_LEAD, 45, P_LVOICE, 4, P_LTONE, 75, P_VIB, 55, P_LDENS, 60, P_LDIN, 8,
    P_FEEDBACK, 75, P_REVERB, 45,
    P_ARC, 20, P_ENDING, 1, P_MOTION, 55, -1};
static const int16_t PC_PHAEDRA[] = {         /* Tangerine Dream, Phaedra: a Moog bass line, Mellotron flutes */
    P_TEMPO, 149, P_ROOT, 2, P_MODE, MODE_AEOLIAN,
    P_SVOICE, 1, P_SEQOCT, -1, P_SEQLEN, 16, P_DENSITY, 70, P_SWEEP, 85,
    P_SEQ2, 40, P_S2VOICE, 1, P_SEQ2OCT, 0, P_SEQ2LEN, 15, P_S2IN, 0,
    P_CVOICE, 2, P_DRIFT, 4,
    P_LVOICE, 2, P_LDIN, 24,
    P_NCHAR, 5, P_CRACKLE, 25,
    P_EMERGE, 5, P_ARC, 30, P_ENDING, 1, P_MOTION, 55, -1};
static const int16_t PC_RUBYCON[] = {         /* Tangerine Dream, Rubycon: dark, two sequences out of the drone */
    P_TEMPO, 144, P_ROOT, 5, P_MODE, MODE_AEOLIAN,
    P_SVOICE, 2, P_SEQOCT, -1, P_SEQLEN, 8, P_CUTOFF, 32, P_SWEEP, 60,
    P_SEQ2, 55, P_S2VOICE, 5, P_SEQ2LEN, 7, P_S2IN, 16,
    P_TAPB, 40, P_TAPBT, 4,
    P_CHORD, 65, P_CVOICE, 0, P_CCOL, -20, P_PEDAL, 85, P_DRIFT, 4,
    P_LVOICE, 1, P_LDIN, 32,
    P_EMERGE, 15, P_ARC, 40, P_ENDING, 2, P_MOTION, 65, -1};
static const int16_t PC_STRATOSFEAR[] = {     /* Tangerine Dream, Stratosfear: brighter, driving, the Lead forward */
    P_TEMPO, 138, P_ROOT, 4, P_MODE, MODE_AEOLIAN,
    P_SVOICE, 3, P_SEQLEN, 16, P_DENSITY, 70, P_CUTOFF, 55,
    P_SEQ2, 60, P_S2VOICE, 0, P_SEQ2LEN, 15, P_S2IN, 0,
    P_CVOICE, 1, P_DRIFT, 3,
    P_LEAD, 52, P_LVOICE, 1, P_LDENS, 65, P_LDIN, 12,
    P_ARC, 20, P_ENDING, 0, P_MOTION, 50, -1};
static const int16_t PC_SUNRAIN[] = {         /* Ashra, New Age of Earth: major, the strongest repeats, bright strings */
    P_TEMPO, 130, P_ROOT, 2, P_MODE, MODE_IONIAN,
    P_SVOICE, 3, P_SEQLEN, 8, P_CUTOFF, 55,
    P_SEQ2, 50, P_S2VOICE, 4, P_SEQ2OCT, 0, P_SEQ2LEN, 6, P_S2IN, 8,
    P_TAPA, 100, P_TAPB, 100, P_TAPBT, 3, P_ECHO, 65, P_FEEDBACK, 65,
    P_CVOICE, 1, P_DRIFT, 3,
    P_LVOICE, 0, P_LDIN, 16,
    P_NCHAR, 2, P_CRACKLE, 25,
    P_ARC, 25, P_ENDING, 1, P_MOTION, 45, -1};
static const int16_t PC_MIRAGE[] = {          /* Schulze, Mirage: icy, a glass sequence, sparse, a long reverb */
    P_TEMPO, 95, P_ROOT, 6, P_MODE, MODE_AEOLIAN,
    P_SVOICE, 4, P_SEQLEN, 12, P_SEQGRID, 1, P_DENSITY, 40, P_SWEEP, 30,
    P_SEQ2, 45, P_S2VOICE, 4, P_SEQ2OCT, 0, P_SEQ2LEN, 9, P_S2IN, 24,
    P_TAPB, 60, P_TAPBT, 4,
    P_CVOICE, 4, P_PHASER, 85, P_DRIFT, 4,
    P_LVOICE, 3, P_LDIN, 24,
    P_REVERB, 55, P_SIZE, 90,
    P_NCHAR, 3, P_CRACKLE, 25,
    P_EMERGE, 5, P_ARC, 20, P_ENDING, 2, P_MOTION, 40, -1};
static const int16_t PC_CYBORG[] = {          /* Schulze, Cyborg: early, raw, deep organ drones beating, no sequence */
    P_TEMPO, 70, P_ROOT, 0, P_MODE, MODE_PHRYGIAN,
    P_SEQ, 0, P_SEQ2, 0,
    P_CHORD, 70, P_CVOICE, 0, P_CCOL, 25, P_PEDAL, 90, P_PHASER, 20, P_DRIFT, 4, P_PMODE, EN_WALK_WANDER,
    P_LVOICE, 3, P_LDENS, 30, P_LOCT, -1, P_LDIN, 8,
    P_ARC, 15, P_ENDING, 2, P_MOTION, 35, -1};
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

const char *const EN_CV_NAME[EN_NCV] = {"Organ", "Strings", "Choir", "Saws", "Glass"};   /* (BERLIN: the Drone's) */
const char *const EN_PV_NAME[EN_NPV] = {"Blip", "Marimba", "Glass", "Wood", "Drop"};
const char *const EN_BV_NAME[EN_NBV] = {"Odyssey", "Mini", "Flute", "Theremin", "Synthi"};   /* (BERLIN: the Lead's) */
const char *const EN_KIT_NAME[EN_NKIT] = {"Tight", "Deep", "Punch", "Soft", "Dusty"};
const char *const EN_CQ_NAME[EN_NCQ] = {"Empty", "Major", "Minor", "Sus2", "Sus4", "Dim", "Aug"};
const char *const EN_CX_NAME[EN_NCX] = {"Triad", "6", "7", "Maj7", "9", "11", "Add9"};
static const char *const ETIME_NAME[6] = {"1/8", "1/8.", "1/4", "1/4.", "1/2", "1 bar"};
static const float ETIME_BEATS[6] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f};
/* the Groove knob (P_SEQGRID): the five patterns from sparse to busy (GROOVE_PAT: the engine's GROOVE below); Notes
 * (P_LDENS) how many of its notes play (the rest left out at random, bar by bar); Energy (PERFORM) thins it further,
 * or fills a thin one */
#define NGROOVE 5
static const uint8_t GROOVE_PAT[NGROOVE] = {0, 1, 2, 4, 3};
/* the bass's grooves (bit i: a note on sixteenth i) and how long each note holds, in sixteenths (Offbeat's
 * held past the next kick, and Roller's nearly to its next note: the kick lands on a bass the duck shapes, not
 * in silence) */
static const uint16_t GROOVE[5] = {0x0001u, 0x4444u, 0x0909u, 0x4949u, 0x0481u};
static const uint8_t DRIFT_BARS[7] = {2, 4, 8, 16, 32, 64, 0};

uint32_t en_groove(int g) { return GROOVE[(unsigned)g < 5u ? g : 0]; }
int en_groove_pat(int knob) { return GROOVE_PAT[(unsigned)knob < NGROOVE ? knob : 0]; }
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
        s = EN_SV_NAME[(unsigned)v < EN_NSV ? v : 0];
    else if (i == P_LVOICE)
        s = EN_BV_NAME[(unsigned)v < EN_NBV ? v : 0];
    else if (i == P_S2VOICE)
        s = EN_SV_NAME[(unsigned)v < EN_NS2V ? v : 0];
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
    else if (i == P_ENDING)
        s = v == 2 ? "Fade" : v ? "Supernova" : "Crash";
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
    if ((i == P_CALG || i == P_BALG)) {
        if (!v) {
            b[0] = 'V', b[1] = 'o', b[2] = 'i', b[3] = 'c', b[4] = 'e', b[5] = 0;   /* the voice's own */
        } else if (v == 9) {
            b[0] = '2', b[1] = '-', b[2] = 'o', b[3] = 'p', b[4] = 0;
        } else {
            itoa_s(v, b);
        }
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
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_MUTE, C_PERF, C_FX, C_SEQ, C_MACRO, C_PROT, C_REWALK, C_PNUDGE, C_LOCK, C_SEQSET, C_LKEY };
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
void en_macro(int i, int v) { post(C_MACRO, i, v + 50); }   /* (sent as 0..100) */
void en_lock(int on) { post(C_LOCK, on != 0, 0); }
void en_prog_rotate(void) { post(C_PROT, 0, 0); }
void en_prog_rewalk(void) { post(C_REWALK, 0, 0); }
void en_ping_nudge(void) { post(C_PNUDGE, 0, 0); }
const char *const EN_FX_NAME[EN_NFX] = {"Repeat", "Time", "Tape stop", "Swell", "Low-pass", "High-pass", "Sweep",
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
volatile float en_arc;
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
static float throw_mul = 1.0f, ping_dry = 0.5f, hats_lvl = 1.0f;   /* Throws, Ghost, Hats level (apply) */
static float chord_dry = 0.5f, chord_snd = 1.5f;   /* the chord's Ghost: its dry, its sends (apply) */
static int ec_pp;                               /* (BERLIN) the echo ping-pongs (twice its time fits the tape) */
static float ec_g = 0.6f, ec_lpc = 0.68f, ec_wide = 1.0f;   /* the echo's Blur, Tone, Width (and the chord's and the
                                                             * pings': widen) */
static int gpat(void) { return GROOVE_PAT[1]; }   /* (BERLIN: the bass's Offbeat, until the Lead replaces it) */   /* the pattern */
static int8_t mac[EN_NMAC];                     /* the master actions, -50..50 */
static int8_t pofs[P_NPARAMS];
static float dub_up;                            /* Dub turned up, 0..1: the echo darker, feeding back past its knob */                  /* what they add to the piece's parameters */
static int pe(int p)                            /* a parameter as heard: the piece's, moved by the master actions */
{
    int v = par[p] + pofs[p];
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

/* ---------------------------------------------------------- the FM core --- */
/* Four sine operators, as the 4-op FM synths have them (the DX21 / TX81Z family's eight algorithms; op 4 can
 * feed back on itself), and a ninth "algorithm", two operators only (2 into 1), for the light voices. Each
 * operator: a ratio to the note, a level (a modulator's: its index, in turns; a carrier's: its share of the
 * sound) and a decay to a sustain (a share of its level). The levels are worked out at the control rate
 * (every 32 samples), the operators every sample. The algorithms, 4 -> 3 meaning 4 modulates 3:
 *   1: 4 -> 3 -> 2 -> 1            2: (3 + 4) -> 2 -> 1         3: (3 -> 2) + 4 -> 1
 *   4: (4 -> 3) + 2 -> 1           5: 4 -> 3, 2 -> 1 (two pairs) 6: 4 -> 1, 2 and 3
 *   7: 4 -> 3, and 2 and 1         8: all four, side by side     9: 2 -> 1 (two operators) */
#define FM_NALG 9
#define FM_ALG2 8                                /* the two-operator one */
static const uint8_t FM_CARR[FM_NALG] = {1, 1, 1, 1, 5, 7, 7, 15, 1};   /* the carriers (bit k: op k + 1) */
typedef struct {
    float ratio, level, dec, sus;               /* the ratio, the level, the decay (s) to the sustain (0..1 of the level) */
} fmop_t;
typedef struct {
    uint8_t alg;                                /* 0..8 */
    float fb;                                   /* op 4's feedback (turns) */
    fmop_t op[4];
} fmpatch_t;
typedef struct {
    float ph[4], inc[4], e[4], kd[4], lv[4], fb1, fb2;
} fmvoice_t;

/* a note: the operators' rates (rmul: op 2's ratio moved), their envelopes from the top (dmul: the modulators'
 * decays stretched) */
static void fm_note(fmvoice_t *v, const fmpatch_t *p, float f, float rmul, float qmul, float dmul)
{
    int k;
    for (k = 0; k < 4; k++) {
        float r = p->op[k].ratio * (k == 1 ? rmul : k >= 2 ? qmul : 1.0f), d = p->op[k].dec * (k ? dmul : 1.0f);
        v->inc[k] = f * r * (1.0f / EN_SR);
        v->e[k] = 1.0f;
        v->kd[k] = d > 0.0f ? fm_expf(-32.0f / (d * EN_SR)) : 1.0f;
    }
}

/* the control rate: the envelopes a step on, the levels (ms: the modulators' scaled, as Colour and Tone have it) */
static void fm_ctl(fmvoice_t *v, const fmpatch_t *p, int alg, float ms)
{
    int k;
    for (k = 0; k < 4; k++) {
        float s = p->op[k].sus;
        v->e[k] = s + (v->e[k] - s) * v->kd[k];
        v->lv[k] = p->op[k].level * v->e[k] * ((FM_CARR[alg] >> k & 1u) ? 1.0f : ms);
    }
}

/* one sample (dk: the pitch now, a multiple of the note's; fb: op 4's feedback) */
static inline float fm_sample(fmvoice_t *v, int alg, float fb, float dk)
{
    float o2, o3, o4, out, *ph = v->ph, *l = v->lv;
    int k;
    if (alg == FM_ALG2) {
        o2 = sine(ph[1] + 8.0f + fb * (v->fb1 + v->fb2)) * l[1];
        v->fb2 = v->fb1;
        v->fb1 = o2;
        out = sine(ph[0] + o2 + 8.0f) * l[0];
        ph[0] += v->inc[0] * dk;
        ph[1] += v->inc[1] * dk;
        for (k = 0; k < 2; k++)
            if (ph[k] >= 1.0f)
                ph[k] -= 1.0f;
        return out;
    }
    o4 = sine(ph[3] + 8.0f + fb * (v->fb1 + v->fb2)) * l[3];   /* (the feedback: the last two, averaged) */
    v->fb2 = v->fb1;
    v->fb1 = o4;
    switch (alg) {
    case 0: o3 = sine(ph[2] + o4 + 8.0f) * l[2]; o2 = sine(ph[1] + o3 + 8.0f) * l[1]; out = sine(ph[0] + o2 + 8.0f) * l[0]; break;
    case 1: o3 = sine(ph[2] + 8.0f) * l[2]; o2 = sine(ph[1] + o3 + o4 + 8.0f) * l[1]; out = sine(ph[0] + o2 + 8.0f) * l[0]; break;
    case 2: o3 = sine(ph[2] + 8.0f) * l[2]; o2 = sine(ph[1] + o3 + 8.0f) * l[1]; out = sine(ph[0] + o2 + o4 + 8.0f) * l[0]; break;
    case 3: o3 = sine(ph[2] + o4 + 8.0f) * l[2]; o2 = sine(ph[1] + 8.0f) * l[1]; out = sine(ph[0] + o2 + o3 + 8.0f) * l[0]; break;
    case 4: o3 = sine(ph[2] + o4 + 8.0f) * l[2]; o2 = sine(ph[1] + 8.0f) * l[1]; out = sine(ph[0] + o2 + 8.0f) * l[0] + o3; break;
    case 5: o3 = sine(ph[2] + o4 + 8.0f) * l[2]; o2 = sine(ph[1] + o4 + 8.0f) * l[1]; out = sine(ph[0] + o4 + 8.0f) * l[0] + o2 + o3; break;
    case 6: o3 = sine(ph[2] + o4 + 8.0f) * l[2]; o2 = sine(ph[1] + 8.0f) * l[1]; out = sine(ph[0] + 8.0f) * l[0] + o2 + o3; break;
    default: o3 = sine(ph[2] + 8.0f) * l[2]; o2 = sine(ph[1] + 8.0f) * l[1]; out = sine(ph[0] + 8.0f) * l[0] + o2 + o3 + o4; break;
    }
    for (k = 0; k < 4; k++) {
        ph[k] += v->inc[k] * dk;
        if (ph[k] >= 1.0f)
            ph[k] -= 1.0f;
    }
    return out;
}

/* the FM voices' patches: the chord's Keys (an electric piano: a body pair, and a tine pair at 14) and Bell
 * (3.5 against 1, 7 against 2); the basses: Punch (a stack, a thump in the first 90 ms), Rubber (two pairs, an
 * octave under, op 4 feeding back), Pluck (2 ops at 3, gone in 50 ms), Growl (2 ops at 1, a bite) */
static const fmpatch_t FM_KEYS = {4, 0.0f, {{1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.3f, 0.35f, 0.15f}, {1.0f, 0.4f, 0.0f, 1.0f},
                                            {14.0f, 0.13f, 0.05f, 0.0f}}};
static const fmpatch_t FM_BELL = {4, 0.0f, {{1.0f, 1.0f, 0.0f, 1.0f}, {3.5f, 0.38f, 0.5f, 0.12f}, {2.0f, 0.45f, 0.0f, 1.0f},
                                            {7.07f, 0.16f, 0.18f, 0.0f}}};
static const fmpatch_t FM_PUNCH = {0, 0.12f, {{1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.5f, 0.06f, 0.08f}, {2.0f, 0.22f, 0.12f, 0.1f},
                                             {1.0f, 0.12f, 0.2f, 0.25f}}};
static const fmpatch_t FM_RUBBER = {4, 0.3f, {{1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.3f, 0.25f, 0.3f}, {0.5f, 0.6f, 0.0f, 1.0f},
                                              {1.0f, 0.18f, 0.3f, 0.4f}}};
static const fmpatch_t FM_PLUCK = {FM_ALG2, 0.0f, {{1.0f, 1.0f, 0.0f, 1.0f}, {3.0f, 0.55f, 0.035f, 0.0f}, {2.0f, 0.2f, 0.05f, 0.0f},
                                                   {1.0f, 0.1f, 0.1f, 0.0f}}};
static const fmpatch_t FM_GROWL = {FM_ALG2, 0.0f, {{1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.44f, 0.07f, 0.09f}, {2.0f, 0.2f, 0.1f, 0.1f},
                                                   {1.0f, 0.1f, 0.1f, 0.0f}}};
static float fm_q(int p) { return fm_exp2f((float)par[p] * 0.02f); }           /* FM page: the upper ratios, x0.5 .. x2 */
static float fm_d(int p) { (void)p; return 1.0f; }   /* the modulators' decay: the patch's own (1.3.0: the knob is the LFO) */
static float fm_fb(int p, const fmpatch_t *pt) { return fm_clampf(pt->fb + 0.004f * (float)par[p], 0.0f, 0.7f); }   /* (1.3.0: more only) */
static int fm_alg_of(int p, const fmpatch_t *pt) { return par[p] ? par[p] - 1 : pt->alg; }

/* ------------------------------------------------- slow random motion --- */
#define NMOD 9                                  /* 0..4 the stab's, 5..8 the echo's */
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
static float loose(void) { return fm_minf(evolve * 2.5f, 1.0f); }   /* (1.3.0: how loose the playing is; 0 at Evolve 0) */
static float arcm;                              /* the sweep, -1 dark .. +1 open, times Motion */

/* -------------------------------------------------------------- chord --- */
/* The stab: four voices struck together, one envelope, one filter. Saws: two a voice, 7 cents apart;
 * Keys: an FM electric piano, brighter as it's struck. Then a resonant low-pass a side that opens with
 * each stab (its own, quicker envelope) and with the sweep, and a soft clip. */
typedef struct {
    float ph[2], inc[2];
    uint8_t note, midi_note;
} stab_t;
static stab_t stab[EN_NPAD];
static float chord_vol, cut_base = 800.0f, st_dec = 0.9995f, st_fdec = 0.99f, st_t60 = 0.3f;
static float st_env, st_amp, st_fenv, st_throw, throw_dec, st_cofs;
static float ec_dk, ec_dd;                      /* (the classics pass) a strike ducks the echo's returns: its level,
                                                 * eased; the repeats heard in the gaps, not over the stab */
static float st_s[4], st_c[4], st_h[2];
static int32_t st_midi_left;
static int shape, root_note = 55;               /* the voicing's shape, the chord's root as voiced (MIDI) */
static const float STAB_PAN[EN_NPAD] = {-0.3f, -0.09f, 0.12f, 0.33f};   /* (1.3.0: 60% of the spread) */
static float stab_gl[EN_NPAD], stab_gr[EN_NPAD];
static int32_t gate_ctr;                        /* samples since the sixteenth began (the grid's Gate) */
/* the chord's voices: how it sounds (CV_*), how quickly it strikes (the amplitude's smoothing a sample:
 * 0.035 ~0.6 ms .. 0.00012 ~190 ms), its length (times Decay's), how far its own envelope opens the filter
 * (octaves), its modulator's ratio and index (FM voices, in turns: the index at the strike, and at rest),
 * and a level to match the others */
enum { CV_SAWS, CV_KEYS, CV_ORGAN, CV_PAD, CV_BELL, CV_WHEEL, CV_NOISE, NSTABV };   /* (the dub's stabs, silent) */
static const struct {
    float att, dec, fenv, ratio, idx_hi, idx_lo, gain;
} CV[NSTABV] = {
    /* (1.3.0: each strike opens further, and closes sooner) */
    {0.035f, 1.0f, 2.5f, 0.0f, 0.0f, 0.0f, 1.0f},        /* Saws: two saws a voice, 7 cents apart */
    {0.035f, 1.3f, 1.4f, 1.0f, 0.41f, 0.06f, 1.23f},     /* Keys: FM at the carrier's pitch, an electric piano */
    {0.02f, 2.5f, 0.7f, 0.0f, 0.0f, 0.0f, 0.75f},        /* Organ: four drawbars (1, 2, 3, 4) */
    {0.00012f, 4.0f, 0.6f, 0.0f, 0.0f, 0.0f, 0.72f},     /* Pad: the saws, swelling in, long */
    {0.05f, 1.8f, 1.1f, 3.5f, 0.35f, 0.05f, 1.17f},      /* Bell: FM at 3.5, the clang dying first */
    {0.03f, 3.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.67f},        /* Tonewheel: drawbars, a percussion and a click at the
                                                          * strike, a rotary speaker turning (left against right) */
    {0.05f, 0.6f, 2.6f, 0.0f, 0.0f, 0.0f, 3.6f},         /* Noise: noise and a little saw, the filter's pitch */
};
static int cvoice;                              /* the chord's voice now */
static fmvoice_t cfm[EN_NPAD];                  /* its FM voices' operators */
static const fmpatch_t *cv_patch(int cv) { return cv == 1 ? &FM_KEYS : cv == 4 ? &FM_BELL : 0; }
static float rotor, click;                      /* Tonewheel: the rotor's turn, the key click */

/* ------------------------------------------------------------------ bass --- */
static const fmpatch_t *const BV_PATCH[EN_NBV] = {0, &FM_PUNCH, &FM_RUBBER, &FM_PLUCK, &FM_GROWL};

/* ----------------------------------------------------------------- pings --- */
#define NBELL 6
typedef struct {
    float amp, adec, att, gl, gr, drop;
    fmvoice_t fm;
    fmpatch_t pt;                               /* its own patch: the ping's voice, as its Tone and the loop have it */
    int32_t wait;
    uint8_t on, note, loop;
} bell_t;
static bell_t bell[NBELL];
static float bell_vol, btone;
static int8_t loop_note[EN_NLOOP][EN_LOOP_MAX];
static uint8_t loop_vel[EN_NLOOP][EN_LOOP_MAX], loop_pos[EN_NLOOP];
static const uint8_t LOOP_BASE[EN_NLOOP] = {72, 67, 79, 62};     /* each loop's register */
static const float LOOP_PAN[EN_NLOOP] = {-0.36f, 0.21f, 0.42f, -0.15f};   /* (1.3.0: 60% of the spread) */
static const float RATIO[5] = {1.0f, 2.0f, 3.0f, 3.5f, 4.77f};

/* ----------------------------------------------------------------- drums --- */
typedef struct {
    float ph, inc, inc_end, amp, adec, x1, x2, att, mph[4], pend, clk, lp[2];   /* (lp: a hat's low-pass) */
    int32_t wait;                               /* swing: samples until a hit that waits (pend: its velocity) */
    uint8_t on, note;
} drum_t;
static drum_t drum[EN_NDRUM];
static uint32_t drum_pat[EN_NDRUM];             /* 0 the kick's (four on the floor), 1 the shaker's, 2 the open hat's */
static float pulse_vol, swing, hat_f = 0.9f, hat_open = 0.25f;
static float step_len, pulse_left;               /* a sixteenth, in samples; to the next MIDI clock pulse */
static uint32_t noise = 22222u, cnoise = 4321u;
static const uint8_t DRUM_GM[EN_NDRUM] = {36, 42, 46, 37, 39};  /* kick, closed hat, open hat, side stick, hand clap */
static const float HAT_INC[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};
/* the shaker's steps, the most wanted first (the off sixteenths before the downbeats; never the open
 * hat's off beats) */
static uint8_t hat_order[12];
static const uint8_t HAT_ORDER[12] = {3, 11, 7, 15, 1, 9, 5, 13, 0, 8, 4, 12};
static int hat_k, hat_kb;
static float duck_x, duck_d, duck_depth, duck_rel;
/* the kits: the kick (its pitch at the strike, times its note; how quickly it falls; its length; its click;
 * its drive) and the hats (their high-pass, times Hats'; how much noise against the metal; their length) */
static const struct {
    float start, fall, t60, click, drive, hat_f, noise, hat_t, gain;
} KIT[EN_NKIT] = {
    /* (1.3.0's sound pass: each kick from lower, longer, with half the click; a soft, rolling pulse, the
     * thump down from 100-200 Hz into the sub) */
    {2.75f, 0.0016f, 0.63f, 0.25f, 1.5f, 1.0f, 0.7f, 1.0f, 1.0f},  /* Tight */
    {2.0f, 0.0008f, 0.7f, 0.1f, 1.3f, 0.92f, 0.7f, 1.2f, 0.6f},    /* Deep: long, 808-ish (shorter and softer than
                                                                    * it was: over a bass that holds now) */
    {4.0f, 0.003f, 0.38f, 0.45f, 2.5f, 1.05f, 0.6f, 0.8f, 1.35f},  /* Punch: short and hard */
    {1.8f, 0.0012f, 0.75f, 0.0f, 1.0f, 0.82f, 0.85f, 1.3f, 0.86f}, /* Soft: a muffled thump */
    {2.6f, 0.0016f, 0.6f, 0.18f, 1.8f, 0.75f, 1.0f, 1.1f, 1.0f},   /* Dusty: darker, noisier hats */
};
static int kit;

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
static uint8_t sec, sec_next, fill;
static uint8_t lay_kick = 1, lay_hats = 1, lay_open = 1, lay_bass = 1, lay_stabs = 1;
/* the kick's shape, a phrase at a time (1.3.0, REPORT2's proposal 2): none, the downbeat, 1 and 3, four with 2 and 4
 * ghosted (-10 dB), four on the floor. The Drums page's Density is where they centre; Evolve how far they wander */
enum { KS_NONE, KS_ONE, KS_HALF, KS_GHOST, KS_FOUR, KS_N };
static const uint16_t KSHAPE[KS_N] = {0x0000u, 0x0001u, 0x0101u, 0x1111u, 0x1111u};
static uint8_t kick_shape = KS_FOUR;
static uint8_t lock;                             /* (PERFORM's Lock: the phrases keep what they have) */
static uint8_t kick_next = KS_FOUR, ph_new_next; /* (the next phrase's kick and plan, decided a bar before it) */
volatile uint8_t en_change_next;                 /* the next phrase brings a change: 1 the plan, 2 the kick, 4 the section */
static int kick_draw(int next);
static uint32_t dpat(int d);
/* (the drums' and the bass's phrase plan: phrase_plan, ph_bar) */
static struct {
    uint32_t seed;
    uint8_t busy, drawn;
    int8_t oct;                                  /* (the bass's octave note: its place among the bar's, -1 none) */
    uint8_t vel[16], late[16];                   /* (the hats' feel: each step's accent, its lateness) */
} ph;
static uint32_t ph_rng = 0x2545F491u;
static uint32_t bar_k, bar_sh, bar_op, bar_bass;   /* this bar's: the kick's, the shaker's, the open hat's, the bass's */
static int ph_st;                                /* (the step a drum hit is on: its lateness) */
static void noise_pattern(void);
static int cr_dbars;                            /* bars till the drifts' next targets (on a barline) */
static void noise_step(int s);
static uint8_t nz_drawn;
static float arc, arc_tgt, arc_tau = 1.0f;     /* the sweep, its target, its time (samples) */
static uint8_t change_pending;

/* -------------------------------------------------------------- harmony --- */
static void scale_set(void)
{
    nsteps = en_scale(pool, steps);
}

static int bell_note(int l, int off)            /* a loop slot's note, from the chord's degree */
{
    int d = deg + off, oct = d / nsteps, s = steps[d % nsteps], base, root = par[P_ROOT], n;
    base = LOOP_BASE[l] - ((LOOP_BASE[l] - root) % 12 + 12) % 12;   /* the root at or below the register */
    n = base + s + 12 * oct;
    while (n > 103)
        n -= 12;
    while (n < 36)
        n += 12;
    return n;
}

/* the chord to the four voices: the voicing on the degree, its root moved the least from the last one,
 * in C3..B3 (as a chord memory: the shape kept, transposed whole; 1.3.0 up from F#2..F3, out of the low-mids) */
static int chord_slot = -1;                     /* the slot whose chord sounds (-1: one of the scale) */
/* ------------------------------------------------------------------ the drone --- */
/* (BERLIN) the Drone: the chord held, its four notes and the root an octave under (a pedal), swelling in over Attack
 * and away over Release; a new chord swells in on the other bank while the old one dies. Its voices: Organ
 * (drawbars), Strings (a saw, into the ensemble), Choir (a saw through two formants), Saws (two, apart), Glass (FM);
 * the bus through the phaser. MIDI channel 1: a chord's notes on as it starts, off as it goes */
#define NDR 5
typedef struct {
    uint8_t note;
    float ph, ph2, dt, gl, gr, s[2], f1[2], f2[2];
} drv_t;
static drv_t drv[2][NDR];
static uint8_t dr_bank, dr_on[2];
static float dr_lvl[2], dr_up = 0.0001f, dr_down = 0.00005f, ph_depth, ph_lfo, phz[2][6], ph_last[2];
static uint32_t dr_rng = 0x6A09E667u;
static float dr_pedal = 0.7f, dr_rev = 1.0f, dr_echo = 1.0f;   /* (the pedal's level; the sends, x1 at 50) */          /* (its own: the voices' phases leave the music's draws alone) */

static void drone_off(int b)                     /* a bank dies away: its notes off */
{
    int i;
    if (!dr_on[b])
        return;
    dr_on[b] = 0;
    for (i = 0; i < NDR; i++)
        midi(0x80, drv[b][i].note, 0);
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
        v->ph = (float)(seq_rand(&dr_rng) >> 8) * (1.0f / 16777216.0f);
        v->ph2 = (float)(seq_rand(&dr_rng) >> 8) * (1.0f / 16777216.0f);
        v->gl = fm_cosf(p);
        v->gr = fm_sinf(p);
        midi(0x90, (uint32_t)n, 70);
    }
    dr_on[nb] = 1;
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

/* ------------------------------------------------------------- the loops --- */
static int rand_offset(void)                    /* scale steps above the chord's degree */
{
    if (frand() < 0.75f) {                      /* a chord tone, sometimes an octave up */
        int o = 2 * irand(chord_nt);
        return o + (frand() < 0.25f ? nsteps : 0);
    }
    return 1 + 2 * irand(3);                    /* a passing note: the 2nd, 4th or 6th */
}

static int loop_count(int l)
{
    int i, c = 0;
    for (i = 0; i < EN_LOOP_LEN[l]; i++)
        c += loop_note[l][i] >= 0;
    return c;
}

static int loop_target(void)                    /* 0..5 notes: Density, thinned in the dark */
{
    float d = (float)pe(P_DENSITY) * 0.05f * (1.0f + 0.5f * arcm);
    return (int)(fm_clampf(d, 0.0f, 5.0f) + frand());
}
static uint8_t loop_rest[EN_NLOOP];             /* this pass: silent (Evolve) */

static void loop_add(int l)
{
    int len = EN_LOOP_LEN[l], i, s;
    for (i = 0; i < 8; i++) {
        s = irand(len);
        if (loop_note[l][s] < 0) {
            loop_note[l][s] = (int8_t)rand_offset();
            loop_vel[l][s] = (uint8_t)(50 + irand(50));
            return;
        }
    }
}

static int loop_pick(int l)                     /* a random sounding slot, -1 none */
{
    int len = EN_LOOP_LEN[l], i, s = irand(len);
    for (i = 0; i < len; i++, s = (s + 1) % len)
        if (loop_note[l][s] >= 0)
            return s;
    return -1;
}

static void loop_publish(int l)
{
    int i;
    for (i = 0; i < EN_LOOP_MAX; i++)
        en_loop_note[l][i] = i < EN_LOOP_LEN[l] ? loop_note[l][i] : -1;
}

static void loop_regrow(int l)
{
    int i, n = loop_target();
    for (i = 0; i < EN_LOOP_MAX; i++)
        loop_note[l][i] = -1;
    for (i = 0; i < n; i++)
        loop_add(l);
    loop_publish(l);
}

static void loop_pass(int l)                    /* the end of a pass: grow, prune, evolve */
{
    int c = loop_count(l), t = loop_target(), s;
    if (evolve < 0.005f) {                      /* Evolve 0: the loops as they are (Density changed: to it) */
        t = (int)(fm_clampf((float)pe(P_DENSITY) * 0.05f, 0.0f, 5.0f) + 0.5f);
        for (; c < t; c++)
            loop_add(l);
        for (; c > t && (s = loop_pick(l)) >= 0; c--)
            loop_note[l][s] = -1;
        loop_rest[l] = frand() < 0.2f * fm_maxf(-arcm, 0.0f);   /* (Motion's dark: a pass sat out, the notes kept) */
        loop_publish(l);
        return;
    }
    for (; c < t; c++)
        loop_add(l);
    for (; c > t && (s = loop_pick(l)) >= 0; c--)
        loop_note[l][s] = -1;
    loop_rest[l] = frand() < evolve * 0.2f + 0.2f * fm_maxf(-arcm, 0.0f);
    if (frand() < evolve * 0.7f && (s = loop_pick(l)) >= 0) {
        if (frand() < 0.5f) {
            loop_note[l][s] = (int8_t)rand_offset();             /* another note */
        } else {
            int d = irand(EN_LOOP_LEN[l]);                       /* the same note, elsewhere */
            if (loop_note[l][d] < 0) {
                loop_note[l][d] = loop_note[l][s];
                loop_vel[l][d] = loop_vel[l][s];
                loop_note[l][s] = -1;
            }
        }
    }
    loop_publish(l);
}

/* each loop its own ping: index, decay and ratio a little apart */
static const float LOOP_IDX[EN_NLOOP] = {1.0f, 0.55f, 1.5f, 0.8f}, LOOP_DEC[EN_NLOOP] = {1.0f, 1.3f, 0.7f, 1.5f};
static const int8_t LOOP_RAT[EN_NLOOP] = {0, 0, 1, -1};
/* the pings' voices: the modulator's ratio (0: by Ping tone, RATIO), the index, its decay and the ping's
 * length (times the Blip's), a falling pitch (Drop: from this many times its note), a level */
static const struct {
    float ratio, idx, idec, t60, drop, gain;
} PV[EN_NPV] = {
    {0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},               /* Blip */
    {4.0f, 1.2f, 0.4f, 0.6f, 0.0f, 1.45f},              /* Marimba: a wooden bar's fourth partial */
    {3.5f, 0.8f, 2.0f, 1.7f, 0.0f, 0.79f},              /* Glass: a long, bright ring */
    {1.41f, 2.5f, 0.2f, 0.25f, 0.0f, 2.4f},             /* Wood: a knock */
    {1.0f, 0.6f, 1.0f, 0.8f, 1.0f, 1.29f},              /* Drop: the pitch falling from an octave up */
};

static void bell_voice(int l, int n, float vel, int32_t wait)
{
    int i, best = 0, ri, pv;
    float lo = 9.0f, t60, p;
    bell_t *v;
    for (i = 0; i < NBELL; i++) {               /* a free voice, or the quietest */
        float a = bell[i].on ? bell[i].amp : -1.0f;
        if (a < lo) {
            lo = a;
            best = i;
        }
    }
    v = &bell[best];
    if (v->on)
        midi(0x81, v->note, 0);
    v->on = 1;
    v->note = (uint8_t)n;
    v->loop = (uint8_t)l;
    pv = 0;
    ri = (int)(btone * 100.0f + 0.5f) * 5 / 101 + LOOP_RAT[l];
    v->drop = fm_clampf(PV[pv].drop + 0.015f * 0.0f, 0.0f, 3.0f);   /* Bend: from up to four times its note */
    v->amp = vel * PV[pv].gain;
    t60 = fm_clampf(0.9f - (float)(n - 60) * 0.02f, 0.25f, 1.1f) * LOOP_DEC[l] * PV[pv].t60   /* short: a ping */
          * fm_exp2f(1.5f * 0.0f);
    v->adec = fm_expf(-6.9f / (t60 * EN_SR));
    /* its patch: a carrier, the modulator (Ping tone: 0 a round blip, 100 a metallic clank: its index, in radians
     * as turns, and its decay; Ratio, the sound page's: its ratio), and two more operators for the 4-op algorithms
     * (Glass's own: two pairs) */
    {
        float idx = (0.2f + 5.0f * btone * btone) * (1.0f + 0.4f * fm_maxf(perf_sw, 0.0f)) * LOOP_IDX[l] * (0.6f + 0.4f * vel) * PV[pv].idx * (1.0f / FM_TWO_PI),
              td = (0.08f + 0.5f * btone) * PV[pv].idec * (1.0f / 6.9f);
        fmpatch_t *pt = &v->pt;
        pt->alg = pv == 2 ? 4 : FM_ALG2;
        pt->fb = 0.0f;
        pt->op[0] = (fmop_t){1.0f, 1.0f, 0.0f, 1.0f};
        pt->op[1] = (fmop_t){(PV[pv].ratio > 0.0f ? PV[pv].ratio : RATIO[ri < 0 ? 0 : ri > 4 ? 4 : ri]) * fm_exp2f(0.8f * 0.0f),
                             idx, td, 0.0f};
        pt->op[2] = (fmop_t){2.0f, 0.45f, 0.0f, 1.0f};
        pt->op[3] = (fmop_t){pv == 2 ? 5.0f : 3.0f, idx * 0.3f, td * 0.6f, 0.0f};
        fm_note(&v->fm, pt, mtof((float)n), 1.0f, fm_q(P_PFRAT), fm_d(P_PFDEC));
        v->fm.ph[0] = v->fm.ph[1] = v->fm.ph[2] = v->fm.ph[3] = 0.0f;
        fm_ctl(&v->fm, pt, pt->alg, 1.0f);
    }
    v->att = 0.0f;
    p = (LOOP_PAN[l] + 1.0f) * (FM_PI / 4.0f);
    v->gl = fm_cosf(p);
    v->gr = fm_sinf(p);
    v->wait = wait;
    en_loop_glow[l] = 1.0f;
    hit(1, vel);
    midi(0x91, (uint32_t)n, (uint32_t)(vel * 110.0f));
}

static void bell_ring(int l, int slot)
{
    int n = bell_note(l, loop_note[l][slot]);
    float vel = (float)loop_vel[l][slot] * 0.01f;
    bell_voice(l, n, vel, irand(220));           /* a hand on the grid: up to 5 ms late */
}

/* ---------------------------------------------------------------- the arc --- */
/* (BERLIN, spec 5) how a piece builds: Emerge (the Drone and the Atmosphere; the Sequence fading in through a closed
 * filter over its last third), then Flow (the sequences; the Lead now and then), Solo (the Lead in full), Drift (the
 * sequences out), 16 .. 64 bars each (Motion: shorter; at 0, Flow for ever), a swell into Flow from Drift (the
 * Sequence back through its filter, the echo up, two bars). Over an Arc of bars the Sequence's filter rises to a peak
 * at 80%, the Atmosphere louder just before it; then an ending (Crash: a take-off into the echo; Supernova: the echo
 * past full; Fade), 8 bars, and the arc again (Seq2 in and Lead in counted again from the Sequence fully in) */
static uint8_t ar_sec = EN_SEC_FLOW, ar_end, ar_first;
static int ar_left = 64, ar_bar, ar_in;
static float ar_seq = 1.0f, ar_cut = 1.0f, ar_atm = 1.0f, ar_push, ar_nova, ar_cr_env, ar_cr_f = 5000.0f, ar_cr_g = 1.0f;
static uint32_t ar_rng;
volatile uint8_t en_swell;
volatile uint32_t en_endings;
static int ar_emerge(void) { return par[P_EMERGE] ? 32 + 160 * par[P_EMERGE] / 100 : 0; }
static int ar_len(void) { return par[P_ARC] ? 128 + 640 * par[P_ARC] / 100 : 0; }
static int ar_sec_len(void) { return 16 + (int)(48.0f * (1.0f - motion)); }
static void ar_shape(void)                       /* the Sequence's level and filter, the Atmosphere, the echo, as it stands */
{
    const int e = ar_emerge(), L = ar_len();
    float rise = 0.5f;
    if (ar_end)
        ar_seq = (float)ar_end / 8.0f;
    else if (ar_sec == EN_SEC_EMERGE)
        ar_seq = fm_clampf(((float)ar_bar - (float)(2 * e) / 3.0f) / ((float)e / 3.0f), 0.0f, 1.0f);
    else if (ar_sec == EN_SEC_DRIFT)
        ar_seq = en_swell ? (float)(3 - ar_left) / 3.0f : 0.0f;
    else
        ar_seq = 1.0f;
    if (L) {
        const float pos = (float)ar_bar / (float)L;
        rise = pos < 0.8f ? pos / 0.8f : 1.0f - 2.5f * (pos - 0.8f);
    }
    ar_cut = (L ? fm_exp2f(2.0f * rise - 1.0f) : 1.0f) * (0.25f + 0.75f * ar_seq);
    ar_atm = L && !ar_end && (float)ar_bar >= 0.7f * (float)L && (float)ar_bar < 0.8f * (float)L ? 2.0f : 1.0f;
    ar_push = en_swell ? 0.5f : 0.0f;
}
static void ar_start(void)                       /* the arc from its start: Emerge, or Flow at once */
{
    ar_bar = 0;
    ar_end = 0;
    en_swell = 0;
    ar_nova = 0.0f;
    if (ar_emerge()) {
        ar_sec = EN_SEC_EMERGE;
        ar_left = ar_emerge();
        ar_in = -1;
    } else {
        ar_sec = EN_SEC_FLOW;
        ar_left = ar_sec_len();
        ar_in = 0;
    }
    en_section = ar_sec;
    ar_shape();
}
static void ar_tick_bar(void)                    /* a bar line (the first, at the start, already counted) */
{
    if (ar_first) {
        ar_first = 0;
        return;
    }
    ar_bar++;
    if (ar_end) {                                /* the ending: then the arc again */
        if (--ar_end == 0) {
            en_endings++;
            ar_start();
            return;
        }
        ar_shape();
        return;
    }
    if (ar_len() && ar_bar >= ar_len()) {        /* the arc done: its ending */
        ar_end = 8;
        en_swell = 0;
        if (par[P_ENDING] == 0)
            ar_cr_env = 1.0f, ar_cr_f = 5000.0f, ar_cr_g = 1.0f;
        ar_nova = par[P_ENDING] == 1 ? 1.0f : 0.0f;
        ar_shape();
        return;
    }
    if (--ar_left <= 0) {
        const float r = (float)(seq_rand(&ar_rng) >> 8) * (1.0f / 16777216.0f);
        switch (ar_sec) {
        case EN_SEC_EMERGE: ar_sec = EN_SEC_FLOW, ar_in = ar_bar; break;
        case EN_SEC_FLOW:
            if (motion > 0.005f)
                ar_sec = r < 0.6f ? EN_SEC_SOLO : EN_SEC_DRIFT;
            break;
        case EN_SEC_SOLO: ar_sec = r < 0.7f ? EN_SEC_FLOW : EN_SEC_DRIFT; break;
        default: ar_sec = EN_SEC_FLOW; break;
        }
        ar_left = ar_sec_len();
    }
    en_swell = ar_sec == EN_SEC_DRIFT && ar_left <= 2;
    en_section = ar_sec;
    ar_shape();
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
} pluck_t;
static pluck_t pluck[NPLUCK];
static seq_t sq[2], sq_pend[2];
static seq_hist_t sq_hist[2];
static int sq_pos[2];
static uint8_t sq_wrap[2], sq_bars[2], sq_written[2], sq_prio[2][SEQ_MAX];
static uint32_t sq_rng = 0x2545F491u;
static float seq_vol, seq2_vol, sq_cut = 600.0f, sq_edec = 0.9995f, sq_adec = 0.9997f;
static float sq2_cut = 450.0f, sq2_edec = 0.9995f, sq2_adec = 0.9997f, sq_rev[2] = {1.0f, 1.0f}, sq_echo[2] = {1.0f, 1.0f};
static float s2_gain = 1.0f;                    /* (Sequence 2 coming in: 0 .. 1 over Seq2 in bars) */
static float sw_v, sw_t, sw_mul = 1.0f;          /* the Sweep: where its wander is, where it goes; the cutoff's factor */
static int sw_bars = 16, sw_len = 16;            /* (a first aim on the first bar) */
volatile float en_seq_cut;                       /* (the Sweep's wander, 0..1, for the screen and the tests) */
volatile uint16_t en_seq_lane[5], en_seq_acc[2];   /* (the Pattern page's lanes as Auto plays them: each sequence's
                                                  * steps (as Density thins them), octaves; the Sequence's skips; accents) */
const char *const EN_SV_NAME[EN_NS2V] = {"2600", "Moog", "Synthi", "Pulse", "Glass", "Sub"};

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
    if (v->on)
        midi(v->layer ? 0x83u : 0x81u, v->note, 0);
    v->on = 1;
    v->note = (uint8_t)n;
    v->layer = (uint8_t)l;
    v->voice = (uint8_t)(l ? ((unsigned)par[P_S2VOICE] < EN_NS2V ? par[P_S2VOICE] : 0)
                           : ((unsigned)par[P_SVOICE] < EN_NSV ? par[P_SVOICE] : 0));
    v->dt = mtof((float)n) * (1.0f / EN_SR);
    v->dt2 = v->dt * (v->voice == 1 ? 1.004f : v->voice == 4 ? 3.5f : 0.5f);   /* Moog's second saw, Glass's
                                                                                * modulator, Sub's octave under */
    v->ph = v->ph2 = 0.0f;
    v->amp = (float)vel * (1.0f / 127.0f);
    v->att = 0.0f;
    v->env = 1.0f;
    v->adec = l ? sq2_adec : sq_adec;
    v->edec = l ? sq2_edec : sq_edec;
    v->cut = l ? sq2_cut * fm_exp2f(-2.0f * (1.0f - s2_gain)) : sq_cut * sw_mul * ar_cut * (perf & EN_PF_SWEEP ? 4.0f : 1.0f);   /* (PERFORM's
                                                  * Sweep held: the Sequence's filter two octaves open) */   /* (its filter opening as it comes in) */
    v->s1[0] = v->s1[1] = v->s2[0] = v->s2[1] = 0.0f;
    p = ((l ? 0.0f : rep ? 0.35f : -0.25f) + 1.0f) * (FM_PI / 4.0f);
    v->gl = fm_cosf(p);
    v->gr = fm_sinf(p);
    en_loop_glow[l] = 1.0f;
    hit(1, v->amp);
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
    const int root = par[P_ROOT] % 12, tr = par[P_TRANS],
              off = tr == 2 ? 0 : ((int)en_chord_root % 12 - root + 12) % 12,
              base = 48 + root + 12 * (l ? par[P_SEQ2OCT] : par[P_SEQOCT]),
              n = seq_pitch(q, s, steps, nsteps, base, tr == 0, off);
    return n < 24 ? 24 : n > 108 ? 108 : n;
}

static void seq_tick(void)                       /* a sixteenth: the sequences' steps, the repeats */
{
    static const uint8_t DT[6] = {2, 3, 4, 6, 8, 16};   /* the repeats' times (the echo's), in sixteenths */
    const int on_grid = par[P_SEQGRID] || !(sixteenth & 1u);
    int l;
    s2_gain = ar_in < 0 ? 0.0f : (par[P_S2IN] ? fm_minf((float)(ar_bar - ar_in) / (float)par[P_S2IN], 1.0f) : 1.0f) * ar_seq;   /* (the
                                                  * bars since the Sequence came fully in; the arc's level too) */
    if (!(sixteenth & 15u)) {                    /* the Sweep: the Sequence's cutoff wandering, a new aim every 8 .. 64
                                                  * bars (~20 s .. 2.5 min at 96), up to 1.5 octaves either way */
        if (++sw_bars >= sw_len) {
            sw_t = 2.0f * (float)(seq_rand(&sq_rng) >> 8) * (1.0f / 16777216.0f) - 1.0f;
            sw_len = 8 + (int)(seq_rand(&sq_rng) % 57u);
            sw_bars = 0;
        }
        sw_v += fm_clampf((sw_t - sw_v) * (1.0f / (float)sw_len), -0.08f, 0.08f);
        sw_mul = fm_exp2f(1.5f * (float)pe(P_SWEEP) * 0.01f * sw_v);
        en_seq_cut = 0.5f + 0.5f * (float)pe(P_SWEEP) * 0.01f * sw_v;
    }
    if (!(sixteenth & 15u)) {
        ar_tick_bar();
        seq_publish();
    }
    if (!(sixteenth & 15u))                      /* a bar line: the performer, when a pass ended on it (at most every
                                                  * 4 bars), or every 16 */
        for (l = 0; l < 2; l++)
            if (++sq_bars[l] >= 16u || (sq_wrap[l] && sq_bars[l] >= 4u)) {
                if (!lock && !sq_written[l] && !par[P_SMODE])
                    seq_vary(&sq[l], &sq_rng, nsteps ? nsteps : 7, pe(P_MOTION), pe(P_EVOLVE));
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
                                                                               * as Synth Seeker's 121 and 77) */
                if (l)
                    vel = (int)((float)vel * s2_gain * s2_gain);   /* (Sequence 2 coming in: silent at first) */
                else
                    vel = (int)((float)vel * ar_seq);   /* (the arc: Emerge's fade in, Drift out, the ending) */
                pluck_voice(l, played, vel, 0);
            }
            sq_pos[l] = seq_next(&q, st);
            if (fxm >> EN_FX_RETURN & 1u && sq_pos[l] >= 4)   /* (the grid's Return: round its first four steps) */
                sq_pos[l] = 0;
            sq_wrap[l] = (uint8_t)(sq_pos[l] <= st);
            en_loop_pos[l] = (uint8_t)st;
        }
        if (!l) {                                /* the repeats: A always, B only into a rest */
            if (par[P_TAPA] && (n = seq_tap(&sq_hist[0], DT[par[P_TAPAT] % 6], &v)) >= 0)
                pluck_voice(0, n, v * par[P_TAPA] / 100, 1);
            if (par[P_TAPB] && (!on_grid || played < 0) && (n = seq_tap(&sq_hist[0], DT[par[P_TAPBT] % 6], &v)) >= 0)
                pluck_voice(0, n, v * par[P_TAPB] / 100, 1);
        }
        seq_hist_push(&sq_hist[l], played, vel);
    }
}

/* ------------------------------------------------------------------ the lead --- */
/* (BERLIN) the Lead: one voice, gliding, its vibrato coming in late; it plays phrases (long notes, or a slow run of
 * scale steps ending long), starting on a chord tone, a breath of 2 to 8 bars between (Phrases: how often). Voices:
 * Odyssey (a saw and a pulse), Mini (three saws), Flute (a sine and breath), Theremin (a sine, always wavering),
 * Synthi (a square). MIDI channel 3 */
static struct {
    uint8_t on, note;                            /* sounding (its gate); the note it glides to */
    float cur, ph, ph2, ph3, vph, env, fenv, vt, s1[2], s2[2], c1[4], c2[4];
} lead;
static float lead_vol, ld_gk = 0.004f, ld_tone = 1.0f, ld_att = 0.0009f, ld_rev = 1.0f, ld_echo = 1.0f;
static uint8_t ld_note[8], ld_dur[8], ld_n, ld_i, ld_left, ld_rest;
static int8_t ld_dg[8], ld_key = -1;             /* (the phrase's degrees; the degree held on the keys, -1: none) */
volatile int8_t en_lead_deg = -1;                /* (the degree the Lead sounds, for the keys' light; -1: none) */
static uint32_t ld_rng = 0x3C6EF372u;

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

static int ld_deg_note(int d)                    /* scale degree d (any) as a note, in the Lead's register */
{
    const int ns = nsteps ? nsteps : 7, o = (d + 64 * ns) / ns - 64, k = (d + 64 * ns) % ns,
              n = 60 + par[P_ROOT] % 12 + 12 * par[P_LOCT] + 12 * o + (nsteps ? steps[k] : 2 * k);
    return n < 36 ? 36 : n > 100 ? 100 : n;
}

static void lead_phrase(void)                    /* a new phrase: its notes and their lengths, in sixteenths */
{
    const int ns = nsteps ? nsteps : 7, run = seq_rand(&ld_rng) % 100u < 40u;
    int d = en_deg_of(pool, (chord_pcs[seq_rand(&ld_rng) % (uint32_t)(chord_nt ? chord_nt : 1)] - par[P_ROOT] + 24) % 12), i;
    if (seq_rand(&ld_rng) & 1u)
        d += ns;                                 /* (now and then from the octave above) */
    if (run) {                                   /* a slow run: 3 to 6 steps one way, the last held */
        const int dir = seq_rand(&ld_rng) & 1u ? 1 : -1;
        ld_n = (uint8_t)(3 + seq_rand(&ld_rng) % 4u);
        for (i = 0; i < ld_n; i++) {
            ld_dg[i] = (int8_t)(d + dir * i);
            ld_note[i] = (uint8_t)ld_deg_note(d + dir * i);
            ld_dur[i] = (uint8_t)(i + 1 < ld_n ? 2 + seq_rand(&ld_rng) % 3u : 8 + seq_rand(&ld_rng) % 9u);
        }
    } else {                                     /* long notes: 1 to 3, a beat to a bar each, on chord tones */
        ld_n = (uint8_t)(1 + seq_rand(&ld_rng) % 3u);
        for (i = 0; i < ld_n; i++) {
            ld_dg[i] = (int8_t)d;
            ld_note[i] = (uint8_t)ld_deg_note(d);
            ld_dur[i] = (uint8_t)(4 + seq_rand(&ld_rng) % 13u);
            d += (seq_rand(&ld_rng) & 1u ? 2 : -2) * (1 + (int)(seq_rand(&ld_rng) % 2u));   /* (a third or a fifth away) */
        }
    }
    ld_i = 0;
    ld_left = 0;
}

static void lead_note(int n)                     /* the next note: legato (it glides there), or from silence */
{
    const int legato = lead.on;
    lead_off();
    if (!ld_audible())
        return;
    if (!legato && lead.env < 0.05f)
        lead.cur = (float)n;                     /* (from silence: no glide) */
    lead.note = (uint8_t)n;
    lead.on = 1;
    lead.vt = 0.0f;
    lead.fenv = 1.0f;
    hit(2, 0.8f);
    midi(0x92, (uint32_t)n, 90);
}

static void lead_tick(void)                      /* a sixteenth: the phrase goes on, or the breath */
{
    if (ld_key >= 0)                             /* (the keys play it: its own phrases wait) */
        return;
    if (!(sixteenth & 15u) && !ld_n) {           /* a bar line, between phrases */
        if (ld_rest)
            ld_rest--;
        else if (ar_in >= 0 && ar_bar - ar_in >= par[P_LDIN] && ar_sec != EN_SEC_EMERGE && !ar_end &&
                 seq_rand(&ld_rng) % 100u < (uint32_t)(20 + 6 * pe(P_LDENS) / 10) / (ar_sec == EN_SEC_FLOW ? 3u : 1u))
            lead_phrase();                       /* (not before Lead in bars after the Sequence is in: established first;
                                                  * Solo and Drift its own, Flow now and then) */
    }
    if (!ld_n)
        return;
    if (ld_left) {
        ld_left--;
        if (ld_left)
            return;
    }
    if (ld_i < ld_n) {
        lead_note(ld_note[ld_i]);
        en_lead_deg = lead.on ? ld_dg[ld_i] : -1;
        ld_left = ld_dur[ld_i++];
        return;
    }
    lead_off();                                  /* the phrase done: a breath of 2 .. 8 bars (fewer as Phrases rises) */
    ld_n = 0;
    ld_rest = (uint8_t)(2 + seq_rand(&ld_rng) % (uint32_t)(1 + 6 * (100 - pe(P_LDENS)) / 100));
}

/* ------------------------------------------------------------- the stabs --- */
static int stab_k, stab_kb, stab_rot;
static uint32_t stab_home;                      /* the pattern, before this bar's gaps */

static void stabs_home(void)                    /* Stabs: one a bar .. six, off the beat */
{
    stab_kb = stab_k = 1 + (30 * 5 + 50) / 100;
    stab_rot = 2;
    stab_home = en_euclid(stab_k, 16, stab_rot);
}

static void stabs_evolve(void)                  /* the pattern turns a step, or gains or loses a stab */
{
    if (frand() < 0.6f) {
        stab_rot = (stab_rot + (frand() < 0.5f ? 1 : 15)) % 16;
    } else {
        int k = stab_k + (frand() < 0.5f ? 1 : -1);
        if (k >= stab_kb - 1 && k <= stab_kb + 1 && k >= 1 && k <= 8)
            stab_k = k;
    }
    stab_home = en_euclid(stab_k, 16, stab_rot);
}

static void stabs_bar(void)                     /* this bar's stabs: the pattern, with gaps */
{
    uint32_t p = stab_home, out = 0;
    int i, first = 1;
    if (!lay_stabs)
        p = 0;
    else if (sec == SEC_GROOVE)                 /* a Groove: every other stab */
        for (i = 0; i < 16; i++)
            if (p >> i & 1u) {
                if (!first)
                    p &= ~(1u << i);
                first = !first;
            }
    first = 1;
    for (i = 0; i < 16; i++)
        if (p >> i & 1u) {
            if (first || frand() >= 0.3f * evolve)
                out |= 1u << i;
            first = 0;
        }
    if (sec != SEC_BREAK && frand() < 0.5f * evolve)   /* (1.3.0: now and then a bar with none: the echo carries it;
                                                      * ~20% at Evolve 40) */
        out = 0;
    en_stab_pat = out;
}

static void stab_fire(float vel, int throw_it)
{
    int i;
    if (st_midi_left > 0)
        for (i = 0; i < EN_NPAD; i++)
            midi(0x80, stab[i].midi_note, 0);
    for (i = 0; i < EN_NPAD; i++) {
        float f = mtof((float)stab[i].note);
        float dt = 0.00202f * fm_exp2f(2.0f * rel(P_CCOL));   /* +-3.5 cents (Colour: +-0.9 .. +-14) */
        stab[i].inc[0] = f * (1.0f - dt) * (1.0f / EN_SR);
        stab[i].inc[1] = CV[cvoice].ratio > 0.0f ? f * CV[cvoice].ratio * (1.0f / EN_SR)   /* FM: the modulator */
                                                 : f * (1.0f + dt) * (1.0f / EN_SR);
        if (cv_patch(cvoice))
            fm_note(&cfm[i], cv_patch(cvoice), f, 1.0f, fm_q(P_CFRAT), fm_d(P_CFDEC));
        stab[i].midi_note = stab[i].note;
        midi(0x90, stab[i].note, (uint32_t)(40.0f + 80.0f * vel));
    }
    st_env = vel;
    ec_dk = fm_maxf(ec_dk, vel);
    st_fenv = 1.0f;
    st_cofs = (frand() - 0.5f) * 0.8f * loose();   /* (1.3.0: each stab its own cutoff, +-0.4 octave) */
    click = cvoice == CV_WHEEL ? 0.12f * vel : 0.0f;
    en_stab_vel = vel;
    en_stab_thrown = (uint8_t)(throw_it != 0);
    en_stabs++;
    hit(0, vel);
    st_midi_left = (int32_t)(fm_maxf(st_t60 * 0.5f, 0.06f) * EN_SR);
    if (throw_it) {
        st_throw = 1.0f;
        en_throws++;
    }
}

/* ---------------------------------------------------------------- drums --- */
static void hats_publish(void)
{
    uint32_t c = 0, o = 0;
    int i, k = 50, ko = k < 15 ? 0 : k < 40 ? 2 : 4;
    for (i = 0; i < hat_k && i < 12; i++)
        c |= 1u << hat_order[i];
    if (ko >= 2)
        o |= 1u << 2 | 1u << 10;
    if (ko >= 4)
        o |= 1u << 6 | 1u << 14;
    drum_pat[1] = c;
    drum_pat[2] = o;
    drum_pat[0] = KSHAPE[kick_shape];
    drum_pat[3] = k >= 70 ? 0x1010u : 0u;       /* the rim, with many hits: on two and four */
    drum_pat[4] = 0x1010u;                       /* the clap: two and four, now and then (clap_auto) */
    en_drum_pat[0] = drum_pat[0];
    en_drum_pat[1] = c;
    en_drum_pat[2] = o;
    en_drum_pat[3] = drum_pat[3];
    en_drum_pat[4] = drum_pat[4];
}

static void hats_home(void)                     /* Hits: the shaker's count, its steps home */
{
    int i;
    for (i = 0; i < 12; i++)
        hat_order[i] = HAT_ORDER[i];
    hat_kb = hat_k = (50 * 12 + 50) / 100;
    hats_publish();
}

static void hats_evolve(void)                   /* a shaker step trades places, or one more or less */
{
    if (frand() < 0.6f) {
        int a = irand(11), t = hat_order[a];
        hat_order[a] = hat_order[a + 1];
        hat_order[a + 1] = (uint8_t)t;
    } else if (hat_kb) {
        int k = hat_k + (frand() < 0.5f ? 1 : -1);
        if (k >= hat_kb - 2 && k <= hat_kb + 2 && k >= 1 && k <= 12)
            hat_k = k;
    }
    hats_publish();
}

/* the kick's note: the key's root in F1..B1 (44..62 Hz); below F, its fifth (G1..B1), so the kick and the
 * sub agree. One note a piece: it doesn't follow the chords */
int en_kick_note(int root)
{
    int pc = (root % 12 + 12) % 12;
    return pc >= 5 ? 24 + pc : 24 + pc + 7;
}
#define kick_note en_kick_note

static void drum_fire(int d, float vel)
{
    drum_t *v = &drum[d];
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    switch (d) {
    case 0: {                                   /* kick: a sine falling fast from ~5x to its note */
        float f = mtof((float)kick_note(par[P_ROOT])), pu = 0.0f;
        v->note = DRUM_GM[0];
        v->inc = f * KIT[kit].start * fm_exp2f(0.8f * pu) * (1.0f / EN_SR);
        v->inc_end = f * (1.0f / EN_SR);
        v->adec = fm_expf(-6.9f / (KIT[kit].t60 * fm_exp2f(1.2f * 0.0f) * EN_SR));
        v->clk = KIT[kit].click * 2.0f * fm_maxf(1.0f + pu, 0.0f);   /* (Tight: 1, as before) */
        duck_x = 1.0f;
        break;
    }
    case 1:                                     /* the shaker: short; it chokes the open hat */
        v->note = DRUM_GM[1];
        v->adec = fm_expf(-6.9f / (0.07f * KIT[kit].hat_t * 1.0f * EN_SR));   /* (1.3.0: 45 ms to 70: a
                                                                                             * shaker; the hats' length) */
        drum[2].adec = fm_expf(-6.9f / (0.03f * EN_SR));
        break;
    case 2:                                     /* open hat: the "tss" off the beat */
        v->note = DRUM_GM[2];
        v->adec = fm_expf(-6.9f / (hat_open * KIT[kit].hat_t * 1.0f * EN_SR));
        break;
    case 3:                                     /* the rim: a short wooden tok, thrown into the echo */
        v->note = DRUM_GM[3];
        v->adec = fm_expf(-6.9f / (0.06f * EN_SR));
        v->clk = 1.0f;
        break;
    default:                                    /* the clap: three quick bursts and a tail, into the plate */
        v->note = DRUM_GM[4];
        v->ph = 0.0f;                            /* (its time, in samples) */
        v->adec = fm_expf(-6.9f / (0.22f * EN_SR));
        break;
    }
    en_drum_glow[d] = 1.0f;
    hit(3, d == 0 ? vel : vel * 0.6f);           /* (the kick the biggest ring) */
    midi(0x99, v->note, (uint32_t)(vel * 100.0f));
    midi(0x89, v->note, 0);
}

static void drum_hit(int d, float vel, int off)  /* (Swing delays the whole off sixteenth now: clock_pulse) */
{
    int32_t w = 0;
    (void)off;
    if ((d == 1 || d == 2) && !par[P_SMODE])     /* (1.3.0: the hats a few ms late, the phrase's feel) */
        w = (int32_t)((float)ph.late[ph_st & 15] * (1.0f / 255.0f) * 0.006f * EN_SR * loose());
    if (w > 0) {
        drum[d].wait = w;
        drum[d].pend = vel;
    } else {
        drum_fire(d, vel);
    }
}

static int32_t drums_wait(void)                 /* samples to the next hit that waits; 0: none */
{
    int32_t m = 0;
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (!m || drum[d].wait < m))
            m = drum[d].wait;
    return m;
}

static void drums_wait_step(int32_t k)          /* k samples went by: hits whose wait is over */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (drum[d].wait -= k) <= 0) {
            drum[d].wait = 0;
            drum_fire(d, drum[d].pend);
        }
}

/* ------------------------------------------------------- the arrangement --- */
static void section_set(int s)
{
    float bl = 16.0f * step_len;
    sec = (uint8_t)s;
    (void)0;                                     /* (BERLIN: en_section is the arc's) */
    lay_kick = lay_hats = lay_open = lay_bass = lay_stabs = 1;
    switch (s) {
    case SEC_BREAK:                             /* the kick and the bass out; dark, then building */
        lay_kick = lay_bass = lay_hats = 0;
        lay_open = motion < 0.5f;
        arc_tgt = -1.0f;
        arc_tau = 0.7f * bl;
        break;
    case SEC_GROOVE:                            /* fewer stabs, the drums to the front */
        arc_tgt = -0.2f + 0.5f * frand();
        arc_tau = 3.0f * bl;
        break;
    case SEC_THIN:                              /* no bass, no shaker */
        lay_bass = lay_hats = 0;
        arc_tgt = -0.5f + 0.6f * frand();
        arc_tau = 3.0f * bl;
        break;
    default:
        arc_tgt = -0.1f + 1.1f * frand();
        arc_tau = 4.0f * bl;
        break;
    }
    en_pulse_rest = (uint8_t)!lay_kick;
}

/* the next phrase (the classics pass, REPORT9: the classics hold one loop for minutes, the kick never leaving). Motion
 * in three regions: under 40 the loop holds (Full only: the change is the filter's, the echo's, the texture's); 40-70
 * a variation now and then (Groove or Thin, a phrase or two; the kick half-time at most); from 70 Breaks too (the
 * kick and the bass out, a phrase). Within a region, more often as Motion rises; at least four phrases (32 bars) in
 * Full between departures */
static uint8_t sec_age;                         /* phrases in Full since the last departure */
static uint8_t ph_age, ph_reset;                /* phrases since the kick's shape or the plan last changed */
static int section_pick(void)
{
    float r = frand(), m;
    if (motion < 0.4f)
        return SEC_FULL;
    if (sec != SEC_FULL)                         /* back, mostly; from a Break, a Thin now and then */
        return r < 0.75f ? SEC_FULL : sec == SEC_BREAK ? SEC_THIN : motion >= 0.7f && frand() < 0.5f ? SEC_BREAK : sec;
    if (sec_age < 4)
        return SEC_FULL;
    m = motion < 0.7f ? (motion - 0.4f) * (1.0f / 0.3f) : (motion - 0.7f) * (1.0f / 0.3f);
    if (r >= 0.15f + 0.35f * m)
        return SEC_FULL;
    if (motion >= 0.7f && frand() < 0.4f + 0.3f * m)
        return SEC_BREAK;
    return frand() < 0.5f ? SEC_GROOVE : SEC_THIN;
}

/* (1.3.0) the drums' and the bass's phrase plan (the user: randomness by phrase and bar, not by note): a phrase is
 * A A A B A A A B. A is the main bar, repeated exactly; B, on bars 4 and 8, a variant: sparse (breathing) or busy (a
 * fill), drawn once a phrase. Which of the hats' and the bass's notes rest (Density, Evolve's looseness), the hats'
 * feel (each step's accent and lateness) and the bass's octave note are drawn once a phrase too, from its seed, and
 * the same every bar. A new phrase keeps the plan or draws another (more often as Evolve rises; never at 0) */
static float ph_u(void)
{
    ph_rng = ph_rng * 1664525u + 1013904223u;
    return (float)(ph_rng >> 8) * (1.0f / 16777216.0f);
}
static uint32_t ph_hash(uint32_t a)
{
    a ^= a >> 16, a *= 0x7FEB352Du, a ^= a >> 15, a *= 0x846CA68Bu, a ^= a >> 16;
    return a;
}
/* of a pattern's steps (but those always kept), keep the first `keep` in an order drawn from the phrase's seed */
static uint32_t ph_thin(uint32_t pat, int keep, uint32_t always, uint32_t salt)
{
    uint32_t out = pat & always, rest = pat & ~always;
    while (keep-- > 0 && rest) {
        uint32_t best = 0, bh = 0;
        int i;
        for (i = 0; i < 16; i++)
            if (rest >> i & 1u) {
                uint32_t h = ph_hash(ph.seed ^ salt * 0x9E3779B9u ^ (uint32_t)i);
                if (!best || h < bh)
                    best = 1u << i, bh = h;
            }
        out |= best, rest &= ~best;
    }
    return out;
}
static void phrase_plan(int first)                /* (1: the first; 2: a new one, decided a bar before; 0: kept) */
{
    int i;
    if (!first && ph.drawn)
        return;                                  /* (the last phrase's plan again: a motif that comes back) */
    ph.drawn = 1;
    ph.seed = ph_rng = ph_rng * 1664525u + 1013904223u;
    ph.busy = (uint8_t)(ph_u() < 0.5f);
    ph.oct = (int8_t)(ph_u() < evolve * 0.6f ? (int)(ph_u() * 8.0f) : -1);
    for (i = 0; i < 16; i++) {
        ph.vel[i] = (uint8_t)(255.0f * fm_minf(0.3f + 0.4f * ph_u() + (i & 1 ? 0.0f : 0.15f), 1.0f));
        ph.late[i] = (uint8_t)(255.0f * ph_u());
    }
}
static int pcount(uint32_t m) { return __builtin_popcount(m); }
/* this bar's patterns, from the plan: A, or (bars 4 and 8) B */
static void ph_bar(int pb)
{
    const int b = pb == 3 || pb == 7;
    const float lo = loose(), e = (float)mac[EN_MAC_ENERGY] * (1.0f / 50.0f), d0 = 1.0f;
    const float d = fm_clampf(e < 0.0f ? d0 * (1.0f + 0.6f * e) : d0 + (1.0f - d0) * e, 0.0f, 1.0f);
    uint32_t g = en_groove(gpat()), f = g & (0u - g);   /* (the groove; its first note, always kept) */
    uint32_t k = drum_pat[0], sh = drum_pat[1], op = drum_pat[2];
    bar_bass = ph_thin(g, (int)((float)pcount(g) * d + 0.5f) - (f ? 1 : 0), f, 3u);   /* Density: the same notes out */
    if (b && ph.busy && d >= 0.3f)               /* (B, busy: the whole groove and a pickup into the next bar) */
        bar_bass = g | 1u << 14;
    else if (b)                                   /* (B, sparse: every other of A's, the first kept) */
        bar_bass = ph_thin(bar_bass, pcount(bar_bass) / 2 - (f ? 1 : 0), f, 4u);
    if (par[P_SMODE]) {                          /* (Pattern: yours, each step's Chance on the grid) */
        bar_k = dpat(0), bar_sh = dpat(1), bar_op = dpat(2);
        return;
    }
    bar_sh = ph_thin(sh, pcount(sh) - (int)(0.45f * lo * (float)pcount(sh) + 0.5f), 0u, 1u);
    bar_op = ph_thin(op, pcount(op) - (int)(0.6f * lo * (float)pcount(op) + 0.5f), 0u, 2u);
    bar_k = k;
    if (b && ph.busy) {                          /* (B, busy: a ghost kick before the next bar, the hats all in) */
        if (0 && k)
            bar_k = k | 1u << 14;
        bar_sh = sh | 1u << 15;
        bar_op = op;
    } else if (b) {                              /* (B, sparse: the kick's downbeat, the shaker on the eighths) */
        if (0)
            bar_k = k & 1u;
        bar_sh &= 0x5555u;
        bar_op = 0;
    }
}

static void bar_start(void)
{
    int pb = (int)(bar & 7u), db = en_drift_bars(par[P_DRIFT]);
    en_phrase_bar = (uint8_t)pb;
    if (cr_dbars > 0)                            /* (the noise's drifts: their next targets on a barline) */
        cr_dbars--;
    if (pb == 0 && bar > 0) {
        section_set(sec_next);
        sec_age = sec != SEC_FULL ? 0 : sec_age < 255 ? sec_age + 1 : 255;
        ph_age = ph_reset ? 0 : ph_age < 255 ? ph_age + 1 : 255;   /* (phrases since the kick or the plan changed) */
        ph_reset = 0;
        if (!lock) {                             /* (Lock: the kick's shape, the noise's groove, the plan, kept) */
            kick_shape = kick_next;              /* (the phrase's kick and plan, as decided a bar before) */
            hats_publish();
            noise_pattern();                     /* (the noise's groove: new, or the last one again) */
            phrase_plan(ph_new_next ? 2 : 0);    /* (the drums' and the bass's plan: new, or the last one again) */
        }
        en_change_next = 0;
    }
    if (sec == SEC_BREAK && pb == 2)            /* two bars in, the filter starts to open */
        arc_tgt = 1.0f, arc_tau = 2.2f * 16.0f * step_len;
    fill = 0;
    if (pb == 7) {
        sec_next = (uint8_t)section_pick();
        /* the last bar before the kick: a fill (the kick drops out halfway, the last stab is thrown) */
        /* (the classics pass: under Motion 40 none, the kick never leaving; then into or out of a change, or now and
         * then as Motion rises) */
        if (sec_next != SEC_BREAK && (sec == SEC_BREAK || (motion >= 0.4f && (sec_next != sec ||
                                                                             frand() < 0.5f * (motion - 0.4f)))))
            fill = 1;
        /* Energy far down: the next phrase Thin (no bass, no shaker); far up: Full, with a fill into it (after the
         * draws, which stay as they were: at Energy 0 the music is the same) */
        if (mac[EN_MAC_ENERGY] <= -25)
            sec_next = SEC_THIN;
        else if (mac[EN_MAC_ENERGY] >= 25)
            sec_next = SEC_FULL, fill = 1;
        /* (1.3.0) the next phrase's kick and plan, decided now, so the screen can say a change is coming */
        /* (the classics pass: both held for a few phrases, 2 at Evolve 100 to 6 at 0; a change of section draws the
         * kick anew) */
        {
            const int hold = 2 + (int)(4.0f * (1.0f - evolve));
            kick_next = lock ? kick_shape : (ph_age + 1 >= hold || sec_next != sec) ? (uint8_t)kick_draw(sec_next) : kick_shape;
            ph_new_next = (uint8_t)(!lock && evolve > 0.0f && ph_age + 1 >= hold && ph_u() < 0.2f + 0.6f * evolve);
            ph_reset = (uint8_t)(kick_next != kick_shape || ph_new_next);
        }
        en_change_next = (uint8_t)(ph_new_next | (kick_next != kick_shape) << 1 | (sec_next != sec) << 2);
    }
    if (!en_frozen && (change_pending || (db && bar > 0 && chord_bars >= (uint32_t)db))) {
        change_pending = 0;
        chord_change();
    }
    en_drift = db ? fm_clampf((float)chord_bars / (float)db, 0.0f, 1.0f) : 0.0f;
    chord_bars++;
    if ((bar & 1u) == 0u && bar > 0 && frand() < evolve * 0.6f)   /* (1.3.0: every two bars, was four) */
        stabs_evolve();
    if (pb == 0 && !lock && frand() < evolve * 0.8f)   /* (1.3.0: the shaker's steps trade places at a phrase's start, not
                                                  * any bar: the A bars the same) */
        hats_evolve();
    stabs_bar();
    ph_bar(pb);                                  /* (this bar's drums and bass, from the plan) */
    bar++;
}

/* the kick's shape for the next phrase: around Density (0..100 as 0..4 on the ladder; 100: four, always, as before
 * 1.3.0), each shape's weight falling with its distance (less steeply as Evolve rises), none at half; a Thin phrase
 * a step sparser. A piece starts on four (its first two phrases), then wanders down to Density over the next six;
 * and after a Break the kick comes back whole (the user: start on four on the floor, then let it evolve) */
static int kick_draw(int next)
{
    const int v = 100;
    const uint32_t phr = (bar + 1u) >> 3;       /* (the next phrase's number: 0 at the top) */
    float x = (float)v * 0.04f, lam = 1.4f - evolve, w[KS_N], t = 0.0f, r;
    int k;
    if (v >= 100 || phr < 2u || (sec == SEC_BREAK && next != SEC_BREAK && next != SEC_THIN))
        return KS_FOUR;
    if (v >= 90) {                               /* (the classics pass: the dub pieces' density, four in Full and in a
                                                  * Groove; a Thin, ghosted or half) */
        if (next != SEC_THIN)
            return KS_FOUR;
        return frand() < 0.5f ? KS_GHOST : KS_HALF;
    }
    if (phr < 8u)
        x += (4.0f - x) * (float)(8u - phr) * (1.0f / 6.0f);
    if (next == SEC_THIN)
        x -= 1.0f;
    for (k = 0; k < KS_N; k++) {
        w[k] = fm_expf(-fm_fabsf((float)k - x) * lam) * (k == KS_NONE ? 0.5f : 1.0f);
        t += w[k];
    }
    r = frand() * t;
    for (k = 0; k < KS_N - 1; k++)
        if ((r -= w[k]) < 0.0f)
            break;
    if (k == KS_NONE && next == SEC_FULL && motion < 0.7f)   /* (the kick gone only in a Break, or from Motion 70) */
        k = KS_ONE;
    return k;
}

/* a lane's steps as they play: the pattern page's (Play: Pattern), or the generated ones */
static uint32_t dpat(int d) { return par[P_SMODE] ? (uint32_t)(uint16_t)par[P_SGATE0 + d] : drum_pat[d]; }

/* the screen's lanes, the bar as it plays: what is muted or at level 0, gone at once */
static void lanes(void)
{
    const int kick = lay_kick && !(mutes & EN_M_KICK) && !(perf & EN_PF_DROP) && EN_DUB,
              hats = !(mutes & EN_M_HATS) && EN_DUB;
    en_lane[0] = (uint16_t)(mutes & EN_M_CHORD || !par[P_CHORD] ? 0u : en_stab_pat);
    en_lane[1] = (uint16_t)(kick ? bar_k & (fill ? 0x00FFu : 0xFFFFu) : 0u);
    en_lane[2] = (uint16_t)(hats ? (lay_hats ? bar_sh : 0u) | (lay_open || fill ? bar_op : 0u) : 0u);
    en_lane[3] = (uint16_t)(hats && lay_hats ? dpat(3) : 0u);
    en_lane[4] = (uint16_t)(hats && (sec == SEC_FULL || sec == SEC_GROOVE || par[P_SMODE]) ? dpat(4) : 0u);
    en_lane[5] = (uint16_t)(lay_bass && !(mutes & EN_M_BASS) && !(perf & EN_PF_DROP) && par[P_LEAD] > 0 ? bar_bass : 0u);
}

/* a pattern's hit: its chance (Play: Pattern only, so Auto's music stays as it was), its accent */
static int dhit(int d, int st, float *vel)
{
    if (!par[P_SMODE])
        return 1;
    if (par[P_SCH0 + d] < 100 && frand() * 100.0f >= (float)par[P_SCH0 + d])
        return 0;
    if (!((uint32_t)(uint16_t)par[P_SACC0 + d] >> st & 1u))
        *vel *= 1.0f - 0.006f * 50.0f;   /* (the unaccented, quieter as Accent turns up) */
    return 1;
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static void tick(void)
{
    int st = (int)(sixteenth & 15u), l, d;
    float r = frand();
    gate_ctr = 0;
    if (st == 0)
        bar_start();
    const int kick = lay_kick && !(mutes & EN_M_KICK) && !(perf & EN_PF_DROP), hats = !(mutes & EN_M_HATS);
    lanes();
    if (EN_DUB) {
        const float fv = (float)ph.vel[st] * (1.0f / 255.0f);   /* (the phrase's feel: this step's accent) */
        float v0 = st ? 0.88f : 1.0f, v1 = fv, v2 = 0.55f + 0.35f * fv, v3 = 0.75f + 0.15f * r;
        ph_st = st;
        if (bar_k >> st & 1u && st == 14 && !(KSHAPE[kick_shape] >> 14 & 1u))
            v0 *= 0.45f;                         /* (the busy B bar's pickup: a ghost) */
        if (!par[P_SMODE] && kick_shape == KS_GHOST && (st == 4 || st == 12))
            v0 *= 0.32f;                         /* (two and four ghosted, -10 dB) */
        if (kick && bar_k >> st & 1u && !(fill && st >= 8) && dhit(0, st, &v0))
            drum_hit(0, v0, 0);
        else if (!(st & 3) && EN_DUB) {   /* the ghost duck: the music breathes on every beat, the kick
                                                    * or not (lighter without one; lightest in a Break) */
            const float gd = lay_kick ? 0.7f : 0.45f;
            duck_x = fm_maxf(duck_x, gd);
        }
        if (hats && lay_hats && bar_sh >> st & 1u && dhit(1, st, &v1))
            drum_hit(1, v1, st & 1);
        if (hats && (lay_open || fill) && bar_op >> st & 1u && dhit(2, st, &v2))
            drum_hit(2, v2, 0);
        if (hats && lay_hats && dpat(3) >> st & 1u && dhit(3, st, &v3))
            drum_hit(3, v3, st & 1);
        if (en_lane[4] >> st & 1u) {             /* the clap: rare (Auto: 3% + 5% x Evolve, ~5% at 40; Pattern: its Chance) */
            float v4 = 0.8f + 0.2f * r;
            if (par[P_SMODE] ? dhit(4, st, &v4) : frand() < 0.03f + 0.05f * evolve)
                drum_hit(4, v4, 0);
        }
    }
    if (EN_DUB && en_stab_pat >> st & 1u) {               /* (the random draws made muted or not: the music goes on the same) */
        uint32_t later = en_stab_pat >> (st + 1);
        int last = !later, thr = (fill && last) || frand() < fm_minf(0.12f * loose() * (sec == SEC_BREAK ? 1.5f : 1.0f) * throw_mul, 0.6f);
        float vel = st == (int)__builtin_ctz(en_stab_pat) ? 0.9f + 0.1f * frand() : 0.65f + 0.25f * frand();
        if (!(mutes & EN_M_CHORD))
            stab_fire(vel, thr || (perf & EN_PF_SWEEP));
    }
    noise_step((int)(sixteenth & 31u));         /* (the noise's groove: two bars) */
    for (d = 0; d < EN_NDRUM; d++)
        en_drum_pos[d] = (uint8_t)st;
    seq_tick();
    lead_tick();
    if (EN_DUB && (sixteenth & 1u) == 0u) {     /* (the dub's loops stepped in eighths) */
        for (l = 0; l < EN_NLOOP; l++) {
            int s = loop_pos[l];
            if (loop_note[l][s] >= 0 && !loop_rest[l] && !(mutes & EN_M_PINGS))
                bell_ring(l, s);
            en_loop_pos[l] = (uint8_t)s;
            if (++loop_pos[l] >= EN_LOOP_LEN[l]) {
                loop_pos[l] = 0;
                loop_pass(l);
            }
        }
    }
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
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_p2, tape_c1, tape_c2, tape_p3, tape_c3;   /* (3: the right's, a
                                                 * few ms behind the left: Width) */
static const float HEAD2[5] = {0.75f, 0.6667f, 0.5f, 0.375f, 0.25f};    /* the second head's parts */
static float head2 = 0.75f, head2_t = 0.75f, head2_v;   /* its place (of the loop), target, speed */
static float fb_lean;                            /* 0..1: how much of the feedback is the second head's */
static float hg[4] = {1.0f, 0.0f, 0.0f, 1.0f};   /* the heads' pan gains: 1 left, 1 right, 2 left, 2 right */
static uint32_t head_left;                       /* samples until the second head may move */

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

/* once a chunk: where the heads sit, how the feedback leans, the second head's slide, the band-pass */
static void heads_step(uint32_t n)
{
    float p, want, vmax, sw;
    int i;
    /* (1.3.0) the loop's gain from the tail's length: whatever the echo's time, the repeats are gone (-60 dB) in
     * Feedback's seconds; long delays vanish as quickly as short ones (Dub and the throws still push past it) */
    echo_fb = fm_exp2f(-9.966f * (echo_len_t * (1.0f / TAPE_SR)) / echo_t60);
    for (i = 0; i < 2; i++) {
        /* (1.3.0: the first head near the middle, drifting with Motion; the second, on throws, to the right; Width
         * spreads them: 0 mono, 2 as wide as before) */
        p = fm_clampf(ec_wide * ((i ? 0.4f : 0.0f) + (i ? 0.2f : 0.25f) * (0.4f + 0.6f * motion) * mod[6 + i].v), -1.0f, 1.0f);
        p = (p + 1.0f) * (FM_PI / 4.0f);
        hg[2 * i] = fm_cosf(p);
        hg[2 * i + 1] = fm_sinf(p);
    }
    fb_lean = 0.3f * motion * (1.0f + mod[8].v);
    if (head_left <= n) {
        head_left = (uint32_t)((16.0f + 32.0f * frand()) * EN_SR);
        if (en_playing && !en_frozen && frand() < motion * 0.6f)
            head2_t = HEAD2[irand(5)];
    } else {
        head_left -= n;
    }
    vmax = 0.008f / fm_maxf(echo_len, 100.0f);
    want = fm_clampf((head2_t - head2) * (1.0f / (2.0f * TAPE_SR)), -vmax, vmax);
    head2_v += (want - head2_v) * fm_minf((float)n * (1.0f / EN_SR), 1.0f);
    /* the band-pass: ~220..800 Hz below (1.3.0: from ~60..500, so the repeats thin; the classics pass: from ~165..700), ~1.1..2.6 kHz above, moving
     * up together */
    dub_up = 0.0f;                               /* (1.3.0: PERFORM's second knob is Wet now, not Dub) */
    sw = fm_clampf(0.45f + 0.5f * arcm + 0.3f * perf_sw + 0.5f * motion * mod[5].v - 0.3f * dub_up, 0.0f, 1.0f);   /* (Dub up: darker) */
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
        float d, h1, h2, h3, f1, f2, fb, w;
        tape_ph = 0;
        echo_len += (echo_len_t * fx_em - echo_len) * fx_ek;   /* (the grid: the time doubled, the tape slowing) */
        if (echo_len > (float)(TAPE_N - 8))      /* (Tape stop slows a long echo past the tape: it stops at its end;
                                                  * the hold below reads echo_len itself. Reported by genkigenki) */
            echo_len = (float)(TAPE_N - 8);
        head2 += head2_v;
        d = echo_len * (1.0f + 0.002f * motion * mod[5].v);   /* wow */
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
        if (st_throw > 1e-4f) {                  /* (the second head: only while a throw rings) */
            h2 = tape_read(d * head2);
            f2 = tape_read(fm_maxf(d * head2 - (float)EAP_FB, 2.0f));
        } else {
            h2 = f2 = 0.0f;
        }
        if (en_frozen) {                         /* dub hold: the loop, as it is */
            int k = (int)echo_len, i0 = tape_w - k;
            int16_t s = tape[i0 < 0 ? i0 + TAPE_N : i0];
            if (++tape_w >= TAPE_N)
                tape_w = 0;
            tape[tape_w] = s;
        } else {
            const float nova = fm_maxf(ar_nova, fx_nova);   /* (the arc's ending, or the grid's) */
            float g = fm_minf(echo_fb + 0.15f * st_throw + 0.12f * perf_echo + 0.1f * dub_up + 0.35f * nova,
                              fb_cap + 0.3f * nova);   /* (Dub: past the knob; Supernova, past full) */
            if (ec_pp)
                g *= g;                          /* (two repeats a turn round: the feedback squared) */
            tape_lp += tape_lpc * (f1 + fb_lean * st_throw * (f2 - f1) - tape_lp);   /* each time round: thinner, darker */
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
        tape_p2 = tape_c2;
        tape_c1 = h1;
        tape_c2 = h2;
    }
    {   /* between the tape's samples: a line from the last to the next; then to the field */
        float f = (float)tape_ph * 0.25f;
        float h1 = tape_p1 + (tape_c1 - tape_p1) * f, h2 = tape_p2 + (tape_c2 - tape_p2) * f;
        float h3 = tape_p3 + (tape_c3 - tape_p3) * f;
        if (ec_pp) {                             /* (Width: 0 every repeat both sides, from 50 side to side) */
            const float x = 1.0f - fm_minf(ec_wide, 1.0f);
            *el = hg[0] * (h1 + x * h3) + hg[2] * h2 * st_throw;
            *er = hg[1] * (h3 + x * h1) + hg[3] * h2 * st_throw;
        } else {
            *el = hg[0] * h1 + hg[2] * h2 * st_throw;   /* (the second head: only while a throw rings) */
            *er = hg[1] * (h1 + fm_minf(0.6f * ec_wide, 1.0f) * (h3 - h1)) + hg[3] * h2 * st_throw;   /* (the later head
                                                  * blended in: 60% at Width 50, all from 84) */
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

int en_voice_fm(int layer)
{
    if (layer == 0)
        return cv_patch(cvoice) != 0;
    if (layer == 2)
        return BV_PATCH[(unsigned)par[P_LVOICE] < EN_NBV ? par[P_LVOICE] : 0] != 0;
    return layer == 1;
}
int en_fm_alg(int layer)
{
    if (layer == 0)
        return cv_patch(cvoice) ? fm_alg_of(P_CALG, cv_patch(cvoice)) : 0;
    if (layer == 2) {
        const fmpatch_t *p = BV_PATCH[(unsigned)par[P_LVOICE] < EN_NBV ? par[P_LVOICE] : 0];
        return p ? fm_alg_of(P_BALG, p) : 0;
    }
    return FM_ALG2;
}

/* ------------------------------------------------------------ parameters --- */
static int pulse;
/* (1.3.0) the FM pages' fourth knob, an LFO synced to the bar: the chord's filter over two bars (+-1.5 octaves at
 * 100), the pings' FM brightness over one, the bass's bite over four; a sine from the grid's place, eased */
static float lfo_c, lfo_p, lfo_n;      /* (lfo_n: the noise's, its rings' tone over a bar, +-1 octave at 100;
                                                 * its crest on beat 2, the chord's on beat 3: they move apart) */
static void lfo_step(void)
{
    const float pos = (float)(sixteenth & 63u) + (float)pulse * (1.0f / 6.0f);   /* (four bars of sixteenths) */
    const float tc = fm_sinf(6.2831853f * pos * (1.0f / 32.0f)), tp = fm_sinf(6.2831853f * pos * (1.0f / 16.0f));
    lfo_c += (1.5f * (float)par[P_CFDEC] * 0.01f * tc - lfo_c) * 0.2f;
    lfo_p += (0.8f * (float)par[P_PFDEC] * 0.01f * tp - lfo_p) * 0.2f;
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
    case P_CHORD: chord_vol = (float)(v * v) * 0.0001f; lanes(); break;
    case P_SEQ: seq_vol = (float)(v * v) * 0.0001f; break;
    case P_LEAD: lead_vol = (float)(v * v) * 0.0001f; break;
    case P_GLIDE: ld_gk = 1.0f - fm_expf(-1.0f / ((0.001f + 0.0015f * (float)v) * EN_SR)); break;   /* ~1 .. 150 ms (the
                                                  * user: 37 .. 49 ms sounds right, more is a theremin) */
    case P_SEQ2: seq2_vol = (float)(v * v) * 0.0001f; break;
    case P_SMODE: case P_SGATE0: case P_SGATE1: case P_SOCT0: case P_SOCT1: case P_SSKIP0: lanes(); break;
    case P_SEQLEN: seq_resize(0, v); break;
    case P_SEQ2LEN: seq_resize(1, v); break;
    case P_ECHO: echo_send = (float)(v * v) * 0.0001f; break;
    case P_ETIME: echo_time(); break;
    case P_REVERB: rev_send = (float)(v * v) * 0.0001f; break;
    case P_FEEDBACK: echo_t60 = 1.5f * fm_exp2f((float)v * (1.0f / 30.0f)); break;   /* the tail: 1.5 s .. 15 s (heads_step) */
    case P_SIZE: pl_decay_t = 0.5f + 0.0045f * (float)v; break;      /* to 0.95 */
    case P_CUTOFF:                               /* 110 Hz .. 5.4 kHz: the stab's; the plucks' (the sequences) */
        cut_base = sq_cut = 110.0f * fm_exp2f((float)v * 0.055f);
        break;
    case P_DECAY: case P_CVOICE:                 /* 60 ms .. 1.5 s (times the voice's); the filter's envelope, quicker */
        cvoice = (unsigned)par[P_CVOICE] < EN_NCV ? par[P_CVOICE] : 0;
        st_t60 = 0.06f * fm_exp2f((float)pe(P_DECAY) * 0.0464f) * CV[cvoice].dec;
        st_dec = fm_expf(-6.9f / (st_t60 * EN_SR));
        st_fdec = fm_expf(-6.9f * 32.0f / ((0.25f * st_t60 + 0.02f) * EN_SR));   /* (1.3.0: a shorter strike) */
        {   /* the plucks: the filter's envelope 80 .. 250 ms to -60 dB, the sound half as long again */
            const float t = 0.08f * fm_exp2f((float)pe(P_DECAY) * 0.0164f);
            sq_edec = fm_expf(-6.9f / (t * EN_SR));
            sq_adec = fm_expf(-6.9f / (1.5f * t * EN_SR));
        }
        break;
    case P_MOTION: motion = (float)v * 0.01f; break;
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

static void regrow(void)                        /* a new piece: fresh loops, home chord, patterns home */
{
    int l, k;
    for (l = 0; l < EN_NLOOP; l++) {
        loop_regrow(l);
        loop_rest[l] = 0;
    }
    chord_slot = -1;
    deg = 0;
    shape = cvoice == CV_KEYS || cvoice == CV_PAD || cvoice == CV_BELL;   /* these like the 9th; the rest the open 7th */
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
    stabs_home();
    hats_home();
    seq_regrow(0);
    seq_regrow(1);
    section_set(SEC_FULL);
    sec_next = SEC_FULL;
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
    for (i = 0; i < NBELL; i++)
        bell[i].on = 0;
    for (i = 0; i < EN_NDRUM; i++)
        drum[i].on = 0, drum[i].wait = 0;
    drone_off(0), drone_off(1);
    dr_lvl[0] = dr_lvl[1] = 0.0f;
    lead_off();
    lead.env = 0.0f;
    ld_key = -1;                                 /* (BERLIN) a key held on the Lead let go too: its phrases again */
    for (i = 0; i < NPLUCK; i++)
        if (pluck[i].on) {
            pluck[i].on = 0;
            midi(pluck[i].layer ? 0x83u : 0x81u, pluck[i].note, 0);
        }
    st_env = st_amp = 0.0f;
    notes_off();
}

void en_init(void)
{
    int i;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    for (i = 0; i <= SN; i++)
        orgt[i] = sint[i % SN] + 0.5f * sint[2 * i % SN] + 0.3f * sint[3 * i % SN] + 0.2f * sint[4 * i % SN];
    rng = 0x9E3779B9u;                           /* (BERLIN) the dice from the top: a reset plays the same again */
    ph_rng = cl_rng = sq_rng = 0x2545F491u;
    dr_rng = 0x6A09E667u;
    ld_rng = 0x3C6EF372u;
    atm_rng = 0xBB67AE85u;
    ar_rng = 0x510E527Fu;
    root_note = 55;                              /* (the voicing from its first, the walk with no past) */
    walk_prev = 0;
    memset(pluck, 0, sizeof pluck);              /* (BERLIN's voices from rest: none sounding, none to let go) */
    memset(&lead, 0, sizeof lead);
    ld_key = -1;
    dr_on[0] = dr_on[1] = 0;
    dr_lvl[0] = dr_lvl[1] = 0.0f;
    plate_init();
    for (i = 0; i < TAPE_N; i++)
        tape[i] = 0;
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
    head2 = head2_t = 0.75f;
    head2_v = 0.0f;
    echo_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    for (i = 0; i < EN_NPAD; i++) {
        float p = (STAB_PAN[i] + 1.0f) * (FM_PI / 4.0f);
        stab_gl[i] = fm_cosf(p);
        stab_gr[i] = fm_sinf(p);
        stab[i].ph[0] = 0.13f * (float)i;
        stab[i].ph[1] = 0.37f + 0.21f * (float)i;
    }
    duck_rel = fm_expf(-1.0f / (0.13f * EN_SR));
    throw_dec = fm_expf(-1.0f / (0.5f * EN_SR));
    arcm = arc = 0.0f;
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    for (i = 0; i < EN_NLOOP; i++)
        loop_pos[i] = 0;
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
        for (i = 0; i < EN_NLOOP; i++)
            loop_pos[i] = 0;
        ar_start();                              /* (the arc from its start; the first bar line already counted) */
        ar_first = 1;
        for (i = 0; i < 2; i++) {                /* the sequences from their first step */
            sq_pos[i] = 0;
            sq_bars[i] = sq_wrap[i] = 0;
            seq_hist_clear(&sq_hist[i]);
        }
        section_set(SEC_FULL);
        sec_next = SEC_FULL;
        kick_shape = kick_next = (uint8_t)kick_draw(SEC_FULL);
        hats_publish();
        nz_drawn = 0;
        noise_pattern();
        phrase_plan(1);
        en_change_next = 0;
    }
    pulse_left = 0.0f;
    tick_wait = 0;
    en_playing = 1;
    drone_chord();
    ld_n = ld_i = 0;                             /* (the Lead: a breath before its first phrase) */
    ld_rest = 2;
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        drum[d].wait = 0;                        /* a swung hit still waiting stays unplayed */
    tick_wait = 0;
    en_playing = 0;
    drone_off(dr_bank);
    lead_off();
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

/* a master action turned: the parameters it moves, applied again as heard (Filter: read by render_chunk) */
static void macro(int i, int v)
{
    /* each: the parameters it moves and how far at full turn (+50), in the parameter's own units */
    static const struct { uint8_t p; int8_t at50; } MOVE[] = {
        {P_SQECHO, 40}, {P_LDECHO, 40}, {P_ECHO, 30},  /* (1.3.0) Wet: the whole music dry, or drowned in the echo (the
                                                          * chord's and the pings' Wet, the echo's send; the noise too:
                                                          * render_crackle) */
        {P_DENSITY, 85}, {P_SEQ, 30}, {P_SEQ2, 30},       /* Energy (BERLIN): the sequences' notes and levels, */
        {P_SWEEP, 40}, {P_DECAY, 30}, {P_TAPB, 40}, {P_CUTOFF, 20},   /* their sweep, length, second repeat,
                                                                       * cutoff */
    };
    static const uint8_t OF[] = {EN_MAC_DUB, EN_MAC_DUB, EN_MAC_DUB, EN_MAC_ENERGY, EN_MAC_ENERGY, EN_MAC_ENERGY,
                                 EN_MAC_ENERGY, EN_MAC_ENERGY, EN_MAC_ENERGY, EN_MAC_ENERGY};
    unsigned k;
    mac[i] = (int8_t)v;
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
                ld_n = 0;                        /* (its own phrase let go) */
                ld_key = (int8_t)a;
                lead_note(ld_deg_note(a));
                en_lead_deg = lead.on ? (int8_t)a : -1;
            } else if (a == ld_key) {
                ld_key = -1;
                lead_off();
                ld_rest = 2;                     /* (its own phrases back after two bars) */
            }
            break;
        case C_SEQSET:                           /* a pattern written: played as written, from its first step */
            sq[a & 1] = sq_pend[a & 1];
            sq_written[a & 1] = 1;
            sq_pos[a & 1] = 0;
            seq_hist_clear(&sq_hist[a & 1]);
            break;
        case C_REGROW:
            regrow();
            if (seq_have)                        /* (a piece's own patterns, kept) */
                seq_take();
            ar_start();                          /* (BERLIN) a piece (changed, or back to its recipe) from its arc's
                                                  * start: its own Emerge, or Flow at once */
            break;
        case C_SEQ:
            seq_have = (uint8_t)a;
            if (a)
                seq_take();
            break;
        case C_MUTE: mutes = (uint32_t)a; lanes(); break;
        case C_PERF: perf = (uint32_t)a; lanes(); break;
        case C_FX: fxm = c & 0xFFFFu; break;
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
        case C_PNUDGE: {                         /* a nudge on the pings: one note of one loop, another */
            int l = irand(EN_NLOOP), k = loop_pick(l);
            if (k >= 0)
                loop_note[l][k] = (int8_t)rand_offset();
            break;
        }
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
        case C_MACRO:
            if (a < EN_NMAC && (unsigned)b <= 100u)
                macro(a, b - 50);
            break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK], kickb[BLK], crl[BLK], crr[BLK];

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


static void render_stabs(uint32_t n)
{
    uint32_t j;
    int i;
    const int cv = cvoice;
    /* the sound page: Attack (a swell up, a snap down), Colour (as each voice has it: the detune, the FM index, the
     * drawbars, the noise against the saw), Resonance, Env (how far each stab opens the filter) */
    const float xa = (float)par[P_CATT] * 0.01f, xc = rel(P_CCOL), up = fm_clampf(1.0f + xc, 0.0f, 2.0f), fx = fm_exp2f(1.5f * xc);
    const float g = chord_vol * 0.5f * CV[cv].gain,
                k = fm_clampf(1.1f - 0.6f * fm_clampf(0.5f + 0.5f * arcm, 0.0f, 1.0f) - 0.55f * rel(P_CRES) -
                                 0.22f * fm_maxf(perf_sw, 0.0f), 0.12f, 2.0f);   /* (Open: it sings) */
    const float att = fm_clampf(CV[cv].att * fm_exp2f(-(xa > 0.0f ? 9.0f : 3.0f) * xa), 0.00004f, 0.3f),
                fe = CV[cv].fenv * 1.0f;
    float pc = 0.0f, ck = click;
    const float dk = st_dec;
    const fmpatch_t *pt = cv_patch(cv);
    const int alg = pt ? fm_alg_of(P_CALG, pt) : 0;
    const float cfb = pt ? fm_fb(P_CFB, pt) : 0.0f;
    float s0 = st_s[0], s1 = st_s[1], s2 = st_s[2], s3 = st_s[3], env = st_env, amp = st_amp, thr = st_throw;
    float hl = st_h[0], hr = st_h[1];
    if (st_midi_left > 0 && (st_midi_left -= (int32_t)n) <= 0)
        for (i = 0; i < EN_NPAD; i++)
            midi(0x80, stab[i].midi_note, 0);
    if ((env < 3e-4f && amp < 3e-4f) || (SHED && env < 6e-3f && amp < 6e-3f)) {   /* (1.3.0: done at -70 dB, not -100:
                                                  * past it, nothing heard; shedding, at -44 dB, as the pings) */
        st_env = st_amp = 0.0f;
        st_throw = thr * fm_expf(-(float)n / (0.5f * EN_SR));
        en_pad_level = 0.0f;
        for (i = 0; i < EN_NPAD; i++)
            en_pad_amp[i] = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        float l = 0.0f, r = 0.0f, v1, v2, v3, yl, yr, e;
        if (!(j & 31u)) {                        /* the filter: its base, its envelope, the sweep */
            float fc = cut_base * fm_exp2f(fe * st_fenv + 1.6f * arcm + st_cofs + lfo_c + perf_sw + 0.3f * motion * mod[0].v);
            svf_coefs(st_c, fm_clampf(fc, 90.0f, 12000.0f), k);
            st_fenv *= st_fdec;
            pc = st_fenv * st_fenv;              /* (Tonewheel: the percussion, quicker still) */
            if (pt)
                for (i = 0; i < EN_NPAD; i++)
                    fm_ctl(&cfm[i], pt, alg, fx);   /* (Colour: the modulators' levels) */
        }
        env *= dk;
        amp += (env - amp) * att;               /* the strike, without a click (or a swell, for the Pad) */
        if (cv == CV_NOISE) {                   /* one noise for all four: the filter gives it its pitch */
            noise = noise * 1664525u + 1013904223u;
            l = r = (float)(int32_t)noise * (1.0f / 2147483648.0f) * 1.6f * (1.0f - 0.5f * xc);
        }
        for (i = 0; i < EN_NPAD; i++) {
            stab_t *v = &stab[i];
            float x;
            switch (cv) {
            case CV_KEYS: case CV_BELL:          /* the FM core */
                x = fm_sample(&cfm[i], alg, cfb, 1.0f) * 0.9f;
                break;
            case CV_ORGAN: {
                float p = v->ph[0];
                x = (sine(p) + up * (0.5f * sine(2.0f * p) + 0.33f * sine(3.0f * p) + 0.2f * sine(4.0f * p))) * 0.8f;
                break;
            }
            case CV_WHEEL: {                     /* 8', 4', 2 2/3' and 2'; the percussion (the third harmonic) dies fast */
                float p = v->ph[0];
                x = (sine(p) + up * (0.7f * sine(2.0f * p) + 0.3f * sine(4.0f * p)) + (0.35f * up + 0.9f * pc) * sine(3.0f * p)) * 0.7f;
                break;
            }
            case CV_NOISE:
                x = 0.35f * up * blep_saw(v->ph[0], v->inc[0]);
                break;
            default:                             /* Saws, Pad */
                x = blep_saw(v->ph[0], v->inc[0]) + blep_saw(v->ph[1], v->inc[1]);
                break;
            }
            v->ph[0] += v->inc[0];
            if (v->ph[0] >= 1.0f)
                v->ph[0] -= 1.0f;
            v->ph[1] += v->inc[1];
            if (v->ph[1] >= 1.0f)
                v->ph[1] -= 1.0f;
            l += x * stab_gl[i];
            r += x * stab_gr[i];
        }
        if (cv == CV_WHEEL) {                   /* the click, and the rotor: left and right in turn */
            float ro;
            noise = noise * 1664525u + 1013904223u;
            ck *= 0.996f;
            l += ck * (float)(int32_t)noise * (1.0f / 2147483648.0f);
            r += ck * (float)(int32_t)noise * (1.0f / 2147483648.0f);
            rotor += 5.6f / EN_SR;              /* the fast speed, ~5.6 turns a second */
            if (rotor >= 1.0f)
                rotor -= 1.0f;
            ro = 0.3f * sine(rotor);
            l *= 1.0f + ro;
            r *= 1.0f - ro;
        }
        v3 = l - s1;                             /* the low-pass, left */
        v1 = st_c[0] * s0 + st_c[1] * v3;
        v2 = s1 + st_c[1] * s0 + st_c[2] * v3;
        s0 = 2.0f * v1 - s0;
        s1 = 2.0f * v2 - s1;
        yl = v2;
        v3 = r - s3;                             /* and right */
        v1 = st_c[0] * s2 + st_c[1] * v3;
        v2 = s3 + st_c[1] * s2 + st_c[2] * v3;
        s2 = 2.0f * v1 - s2;
        s3 = 2.0f * v2 - s3;
        yr = v2;
        e = amp * g;
        yl = sat(yl * e * 3.0f) * 0.4f;
        yr = sat(yr * e * 3.0f) * 0.4f;
        hl += 0.0225f * (yl - hl);               /* a gentle high-pass, ~160 Hz: the stabs out of the low-mids */
        hr += 0.0225f * (yr - hr);
        yl -= hl;
        yr -= hr;
        mixl[j] += yl * chord_dry * (1.0f - 0.5f * thr);   /* (Ghost: the dry against the sends; a throw's, -6 dB) */
        mixr[j] += yr * chord_dry * (1.0f - 0.5f * thr);
        esend[j] += (yl + yr) * (0.8f + 2.5f * thr) * chord_snd;
        rsend[j] += (yl + yr) * 0.35f;
        thr *= throw_dec;
    }
    gate_ctr += (int32_t)n;
    st_s[0] = fm_flush(s0);
    st_s[1] = fm_flush(s1);
    st_s[2] = fm_flush(s2);
    st_s[3] = fm_flush(s3);
    st_h[0] = fm_flush(hl);
    st_h[1] = fm_flush(hr);
    st_env = env;
    st_amp = amp;
    st_throw = thr;
    click = fm_flush(ck);
    en_pad_level = fm_minf(amp * 1.2f, 1.0f);
    for (i = 0; i < EN_NPAD; i++) {
        en_pad_note[i] = stab[i].note;
        en_pad_amp[i] = amp;
    }
}

static void render_bells(uint32_t n)
{
    int i;
    const float g = bell_vol * 1.4f, arate = 0.004f + 0.02f * fm_minf(btone * 2.0f, 1.0f);
    for (i = 0; i < NBELL; i++) {
        bell_t *v = &bell[i];
        float amp = v->amp, att = v->att;
        const int alg = v->pt.alg;
        const float fb = fm_fb(P_PFB, &v->pt);
        uint32_t j = 0;
        if (!v->on)
            continue;
        if (v->wait > 0) {
            if ((uint32_t)v->wait >= n) {
                v->wait -= (int32_t)n;
                continue;
            }
            j = (uint32_t)v->wait;
            v->wait = 0;
        }
        for (; j < n; j++) {
            float x;
            if (!(j & 31u))
                fm_ctl(&v->fm, &v->pt, alg, fm_exp2f(lfo_p));   /* (the LFO: brighter, darker) */
            x = fm_sample(&v->fm, alg, fb, 1.0f + v->drop) * amp * att * g;
            mixl[j] += x * v->gl * ping_dry;   /* (Ghost: the dry against the sends, which stay; 50: -6 dB) */
            mixr[j] += x * v->gr * ping_dry;
            rsend[j] += x * 0.4f;
            esend[j] += x * 1.1f;
            v->drop *= 0.9993f;                  /* (Drop: ~35 ms to fall) */
            amp *= v->adec;
            att += (1.0f - att) * arate;
        }
        v->amp = amp;
        v->att = att;
        if (amp < (SHED ? 0.006f : 0.0008f)) {   /* (shedding: a tail ends at -44 dB, not -62) */
            v->on = 0;
            midi(0x81, v->note, 0);
        }
    }
}

/* a hat: the metal and noise, high-passed (Chamberlin SVF, ~1.8 to 5 kHz by Hats) and low-passed (~8 kHz):
 * in the references' 2-8 kHz, not the air above (1.3.0); no pitch; into the echo (metal: the square's share,
 * at: the attack's smoothing) */
static void render_hat(drum_t *v, uint32_t n, float g, float pl, float pr, float es, float metal, float at)
{
    uint32_t j;
    int k;
    float x1, x2, amp, att, lp, lp2, hf = fm_minf(hat_f * KIT[kit].hat_f, 1.2f);
    if (!v->on)
        return;
    x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att, lp = v->lp[0], lp2 = v->lp[1];
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp, x;
        for (k = 0; k < 4; k++) {
            v->mph[k] += HAT_INC[k];
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            in += v->mph[k] < 0.5f ? 0.25f : -0.25f;
        }
        noise = noise * 1664525u + 1013904223u;
        in = metal * (1.1f - KIT[kit].noise) * in + KIT[kit].noise * (float)(int32_t)noise * (1.0f / 2147483648.0f);
        hp = in - x2 - 0.8f * x1;
        x1 += hf * hp;
        x2 += hf * x1;
        lp += 0.65f * (hp - lp);
        lp2 += 0.65f * (lp - lp2);
        x = lp2 * amp * att * g;
        mixl[j] += x * pl;
        mixr[j] += x * pr;
        rsend[j] += x * 0.15f;
        esend[j] += x * es;
        amp *= v->adec;
        att += (1.0f - att) * at;
    }
    v->x1 = fm_flush(x1);
    v->x2 = fm_flush(x2);
    v->lp[0] = fm_flush(lp);
    v->lp[1] = fm_flush(lp2);
    v->amp = amp;
    v->att = att;
    v->on = amp > 0.0005f;
}

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = pulse_vol * KIT[kit].gain, fall = KIT[kit].fall, drive = KIT[kit].drive;
    drum_t *v = &drum[0];
    if (v->on) {                                 /* the kick: its own bus, ducked by nothing */
        float ph = v->ph, inc = v->inc, amp = v->amp, att = v->att, clk = v->clk, cl = v->x1;
        for (j = 0; j < n; j++) {
            float x;
            noise = noise * 1664525u + 1013904223u;
            cl += 0.5f * ((float)(int32_t)noise * (1.0f / 2147483648.0f) - cl);   /* the click: noise, softened */
            x = (sine(ph) * att + 0.5f * cl * clk) * amp;
            kickb[j] += sat(x * drive) * g * (0.89f / drive);   /* (-2 dB from 1.2.0: -5 in 1.3.0, the drums as spice; +3 back, the user: hear the drums) */
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc += (v->inc_end - inc) * fall;
            amp *= v->adec;
            att += (1.0f - att) * 0.05f;          /* (in over ~0.5 ms: softer) */
            clk *= 0.985f;
        }
        v->ph = ph;
        v->inc = inc;
        v->amp = amp;
        v->att = att;
        v->clk = clk;
        v->x1 = fm_flush(cl);
        v->on = amp > 0.0005f;
    }
    {   /* (Energy: the hats -6 dB down to +6 dB up) */
        const float hl = fm_exp2f((float)mac[EN_MAC_ENERGY] * (1.0f / 50.0f)) * hats_lvl;   /* (and Hats: +-12 dB) */
        render_hat(&drum[1], n, g * 0.16f * hl, 0.6f, 1.0f, 0.2f, 0.6f, 0.02f);   /* shaker: a little right, soft, into the echo */
        render_hat(&drum[2], n, g * 0.17f * hl, 1.0f, 0.7f, 0.2f, 1.0f, 0.3f);   /* open: a little left, some echo */
    }
    v = &drum[3];                                /* the rim: two tones (~520 Hz, ~1.7 kHz) and a click of noise */
    if (v->on) {
        float amp = v->amp, att = v->att, clk = v->clk;
        for (j = 0; j < n; j++) {
            float x;
            v->ph += 520.0f / EN_SR;
            if (v->ph >= 1.0f)
                v->ph -= 1.0f;
            v->mph[0] += 1710.0f / EN_SR;
            if (v->mph[0] >= 1.0f)
                v->mph[0] -= 1.0f;
            noise = noise * 1664525u + 1013904223u;
            x = (sine(v->ph) * 0.55f + sine(v->mph[0]) * 0.45f + (float)(int32_t)noise * (1.0f / 2147483648.0f) * clk) *
                amp * att * g * 0.3f;
            mixl[j] += x * 0.85f;
            mixr[j] += x;
            esend[j] += x * 1.2f;                /* (mostly into the echo: a dub rim) */
            rsend[j] += x * 0.5f;
            amp *= v->adec;
            att += (1.0f - att) * 0.4f;
            clk *= 0.9f;
        }
        v->amp = amp;
        v->att = att;
        v->clk = clk;
        v->on = amp > 0.0005f;
    }
    v = &drum[4];                                /* the clap: noise through a band-pass (~1.1 kHz), its envelope */
    if (v->on) {                                 /* three bursts 9 ms apart, then the tail */
        float amp = v->amp, t = v->ph, x1 = v->x1, x2 = v->x2;
        for (j = 0; j < n; j++) {
            float w, hp, bp, e, x, tb = t < 1200.0f ? (t - 400.0f * (float)(int)(t * (1.0f / 400.0f))) : 1e9f;
            noise = noise * 1664525u + 1013904223u;
            w = (float)(int32_t)noise * (1.0f / 2147483648.0f);
            hp = w - x2 - 0.7f * x1;            /* (a Chamberlin SVF: the band) */
            x1 += 0.16f * hp;
            x2 += 0.16f * x1;
            bp = x1;
            /* a burst: ~3 ms, in over ~0.3 ms (from nothing at once, the ringing band stepped: a click each burst,
             * which the plate and the echo repeated) */
            e = tb < 1e8f ? (1.0f - fm_expf(-tb * (1.0f / 6.0f))) * fm_expf(-tb * (1.0f / 130.0f)) : 0.0f;
            if (t >= 1200.0f)
                e = amp * 0.9f;
            x = bp * e * g * 1.1f;
            mixl[j] += x;
            mixr[j] += x * 0.8f;
            rsend[j] += x * 1.4f;                /* (into the plate, most of it) */
            esend[j] += x * 0.5f;
            if (t >= 1200.0f)
                amp *= v->adec;
            t += 1.0f;
        }
        v->amp = amp;
        v->ph = t;
        v->x1 = fm_flush(x1);
        v->x2 = fm_flush(x2);
        v->on = amp > 0.0005f;
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
const char *const EN_NCHAR_NAME[EN_NNCHAR] = {"None", "Wind", "Surf", "Space", "Air", "Tape"};   /* (BERLIN: the Atmosphere's;
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
static float cr_lp, cr_lp2, cr_hp, cr_cl, cr_cr, cr_fl, cr_fr, cr_cc = 0.3f, cr_gate, cr_sg = 1.0f;
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
    if (sec == SEC_BREAK || sec == SEC_THIN)
        k = k * 7 / 10;
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
/* (BERLIN) the Atmosphere: the Synthi's weather (Schulze's "water, wind, twittering, thunder"). Wind: noise through a
 * resonant band-pass wandering ~250 Hz .. 2 kHz, gusting; Surf: dark noise swelling and falling every 6 .. 12 s, left
 * and right apart; Space: sample-and-hold bleeps, short sines on random notes of the scale high up, at random, into
 * the echo; Air: a soft high hiss, breathing; Tape: the dub's noise machine (render_crackle). Colour: darker ..
 * brighter; Density: calmer .. busier; Drift: slower .. quicker. While playing, as the dub's was */
enum { ATM_NONE, ATM_WIND, ATM_SURF, ATM_SPACE, ATM_AIR, ATM_TAPE };
static inline float svf_lp(const float *c, float *s, float x, float *bp);
static float atm_gate, atm_w[2] = {0.3f, 0.6f}, atm_wt[2] = {0.5f, 0.5f}, atm_gu = 0.5f, atm_gt = 0.5f, atm_s[2][2],
             atm_c[2][4], atm_sw[2] = {0.0f, 0.4f}, atm_swr[2] = {0.11f, 0.13f}, atm_lp[2], bl_ph, bl_dt, bl_env,
             bl_gl, bl_gr, bl_wait, atm_hl, atm_hr, bl_dt1, bl_glk, bl_idx, bl_dk = 0.999f, bl_step;
static int bl_burst;
static float atm_u(void) { return (float)(seq_rand(&atm_rng) >> 8) * (1.0f / 16777216.0f); }
static float atm_noise(void) { return (float)(int32_t)seq_rand(&atm_rng) * (1.0f / 2147483648.0f); }
static void render_atmos(uint32_t n)
{
    const int c = atm_char;
    const float ring = 0.025f + 0.0023f * (float)par[P_NTUNE],
                air = (float)(par[P_NROOM] + 50) * 0.01f, wid = (float)par[P_NWIDTH] * 0.02f,
                rv = (float)par[P_NPLATE] * 0.02f, ec = (float)par[P_NECHO] * 0.02f,
                col = fm_exp2f(fm_clampf(nctl.colour, -1.0f, 1.0f)), busy = fm_exp2f(fm_clampf(nctl.grain, -1.0f, 1.0f)),
                spd = fm_exp2f(fm_clampf(nctl.drift, -1.0f, 1.0f)), sec_n = (float)n * (1.0f / EN_SR);
    float g;
    uint32_t j;
    int ch;
    atm_gate += ((en_playing ? 1.0f : 0.0f) - atm_gate) * fm_minf((float)n / (0.5f * EN_SR), 1.0f);
    g = mutes & EN_M_NOISE || c == ATM_NONE ? 0.0f : crackle_vol * atm_gate * ar_atm;
    if (g < 1e-5f) {
        for (j = 0; j < n; j++)
            crl[j] = crr[j] = 0.0f;
        return;
    }
    for (ch = 0; ch < 2; ch++) {                 /* the wanderings: a new aim now and then, eased to */
        if (atm_u() < sec_n * 0.35f * spd)
            atm_wt[ch] = atm_u();
        atm_w[ch] += (atm_wt[ch] - atm_w[ch]) * fm_minf(sec_n * 0.25f * spd, 1.0f);
        svf_coefs(atm_c[ch], 250.0f * fm_exp2f(3.0f * atm_w[ch]) * col, 0.32f - 0.28f * (float)par[P_NTUNE] * 0.01f);
    }
    if (atm_u() < sec_n * 0.5f * spd * busy)     /* (the gusts) */
        atm_gt = atm_u();
    atm_gu += (atm_gt - atm_gu) * fm_minf(sec_n * 0.6f * spd, 1.0f);
    for (j = 0; j < n; j++) {
        float l = 0.0f, r = 0.0f, bp;
        switch (c) {
        case ATM_WIND:
            svf_lp(atm_c[0], atm_s[0], atm_noise(), &bp);
            l = bp * (0.25f + 0.75f * atm_gu);
            svf_lp(atm_c[1], atm_s[1], atm_noise(), &bp);
            r = bp * (0.25f + 0.75f * atm_gu);
            break;
        case ATM_SURF:
            for (ch = 0; ch < 2; ch++) {
                const float sw = 0.5f - 0.5f * sine(atm_sw[ch] + 0.75f), a = sw * sw;
                atm_lp[ch] += fm_minf(900.0f * col * (1.0f / EN_SR) * FM_TWO_PI, 1.0f) * (atm_noise() - atm_lp[ch]);
                atm_sw[ch] += atm_swr[ch] * spd * (1.0f / EN_SR);
                if (atm_sw[ch] >= 1.0f) {        /* (a wave done: the next one 6 .. 12 s) */
                    atm_sw[ch] -= 1.0f;
                    atm_swr[ch] = 1.0f / (6.0f + 6.0f * atm_u());
                }
                if (ch)
                    r = 2.2f * atm_lp[ch] * a;
                else
                    l = 2.2f * atm_lp[ch] * a;
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
        if (c != ATM_AIR) {                      /* Air: a hiss under the others */
            const float hl = atm_noise(), hr = atm_noise();
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
}

static void render_crackle(uint32_t n)
{
    uint32_t j, thr;
    int i;
    float g, k, sg, nc, ng, nr, nd, gh, lpc, vg, gres, cg, lpk, bf, cont, dry;
    const unsigned c = nz_char();
    cr_gate += ((en_playing ? 1.0f : 0.0f) - cr_gate) * fm_minf((float)n / (0.5f * EN_SR), 1.0f);
    if (cr_gate < 1e-4f)
        cr_gate = 0.0f;
    g = mutes & EN_M_NOISE ? 0.0f : crackle_vol * cr_gate;   /* (muted: its level kept) */
    if (g <= 0.0f) {
        for (j = 0; j < n; j++)
            crl[j] = crr[j] = 0.0f;
        return;
    }
    nc = fm_clampf(nctl.colour + NCHAR[c].tone, -1.0f, 1.0f);   /* Tone */
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
    cr_sg += ((sec == SEC_BREAK || sec == SEC_THIN ? 1.0f : 0.0f) - cr_sg) * fm_minf((float)n / (2.0f * EN_SR), 1.0f);
    sg = cr_sg;                                  /* (Break and Thin: the combs ring longer, the noise carries it) */
    {   /* Decay: the combs' gain, 0.75 .. 0.97 (-60 dB in ~70 ms .. ~1.2 s); Tone: the band-pass ~250 Hz-2 kHz, the
         * damping with it; both moved by the drifts and Motion's arc */
        float dec = fm_clampf((float)par[P_NTUNE] * 0.01f + 0.3f * NCHAR[c].dec + 0.15f * sg +
                              0.3f * (cr_dv[3] - 1.0f), 0.0f, 1.0f), t;
        cg = 0.8f + 0.18f * dec;
        t = fm_clampf(nc + 0.7f * (cr_dv[1] - 1.0f) + 0.4f * arcm + lfo_n, -1.0f, 1.0f);   /* (and its LFO) */
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
        const float sw = 0.05f * cont * (1.0f - 0.8f * duck_d);   /* (Tape: the hiss in, between beats) */
        const int32_t sj = nz_wait >= 0 && nz_wait < (int32_t)n ? nz_wait : -1;   /* (a hit strikes in this chunk) */
        const int rest = SHED && ex < 1e-4f && cont <= 0.0f &&
                         fm_fabsf(nz_lp[0]) + fm_fabsf(nz_lp[1]) + fm_fabsf(nz_lp[2]) < 2e-4f;
        /* (1.3.0: the noise under the duck, deeper than the music, its sends too: it dips on each beat and swells
         * between, the kick or not; the duck as it is at this chunk's start) */
        const float dn = 1.0f - fm_minf(1.7f * duck_depth, 0.85f) * duck_d;
        /* (the combs at the music's own depth: ducked deeper, a hit on the beat swelled late, REPORT6 9.1) */
        const float dnr = 1.0f - fm_minf(duck_depth, 0.85f) * duck_d;
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
            l = (pl + (ql - pl) * f) * dnr;
            r = (pr + (qr - pr) * f) * dnr;
            esend[j] += (l + r) * (0.15f * ne + 0.8f * st_throw);   /* (a little into the echo, Echo; a throw, more) */
            rsend[j] += (l + r) * 0.3f * np + h * gh * 0.4f;   /* (the plate, Plate) */
            l *= 0.75f * nw;                     /* (the dry -2.5 dB, less into the plate than before: further back,
                                                  * its breathing heard, not smoothed into the plate's wash; PERFORM's
                                                  * Wet: the dry down as it turns up) */
            r *= 0.75f * nw;
            l += (h * gh + vg * fl) * dn;
            r += (0.9f * h * gh + vg * fr) * dn;
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
 * then. The echo's (Repeat: the Sequence thrown into it; Time, Tape stop, Swell; Supernova: its feedback past full);
 * a master filter on everything (Low-pass, High-pass); Sweep the Sequence's own filter, swept; Phaser on all the dry
 * music; Reverb (the dry into the plate), Take-off (the Crash ending's noise falling into the echo), Freeze (the
 * cloud), Wash; Octave and Return in the sequences; Twitter, a burst of bleeps (Space's) into the echo */
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
static void render_plucks(uint32_t n)
{
    int i;
    const float g0 = seq_vol * 1.2f, g1 = seq2_vol * 1.2f;
    for (i = 0; i < NPLUCK; i++) {
        pluck_t *v = &pluck[i];
        float amp = v->amp, env = v->env, att = v->att, ph = v->ph, ph2 = v->ph2;
        const float g = v->layer ? g1 : g0, dt = v->dt, dt2 = v->dt2, adec = v->adec, edec = v->edec,
                    es = 0.6f * (sq_echo[v->layer] + fx_rep * (2.5f - sq_echo[v->layer]));   /* (Repeat: thrown in) */
        uint32_t j;
        if (!v->on)
            continue;
        for (j = 0; j < n; j++) {
            float x, bp;
            if (!(j & 15u)) {
                const float f = v->cut * fm_exp2f(3.5f * env) * fx_swm;   /* (the grid's Sweep) */
                svf_coefs(v->c1, f, 1.4f);
                svf_coefs(v->c2, f, 0.6f);
            }
            switch (v->voice) {
            case 0: x = blep_saw(ph, dt); break;
            case 1: x = 0.6f * (blep_saw(ph, dt) + blep_saw(ph2, dt2)); break;
            case 2: x = blep_saw(ph, dt) - blep_saw(frac1(ph + 0.5f), dt); break;
            case 3: x = blep_saw(ph, dt) - blep_saw(frac1(ph + 0.18f), dt); break;
            case 4: x = sine(ph + 2.0f + 1.8f * env * sine(ph2)); break;
            default: x = sine(ph) + 0.6f * sine(ph2); break;
            }
            ph += dt;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph2 += dt2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            x = svf_lp(v->c1, v->s1, x, &bp);
            x = svf_lp(v->c2, v->s2, x, &bp);
            x *= amp * att * g;
            mixl[j] += x * v->gl;
            mixr[j] += x * v->gr;
            rsend[j] += x * 0.35f * sq_rev[v->layer];
            esend[j] += x * es;
            amp *= adec;
            env *= edec;
            att += (1.0f - att) * 0.03f;
        }
        v->amp = amp, v->env = env, v->att = att, v->ph = ph, v->ph2 = ph2;
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
static void render_drone(uint32_t n)
{
    const int cv = cvoice < EN_NCV ? cvoice : 0;
    const float g = mutes & EN_M_CHORD ? 0.0f : chord_vol * 0.072f, fc = 1800.0f * fm_exp2f(2.0f * rel(P_CCOL)), k = fm_clampf(1.2f - 0.9f * rel(P_CRES), 0.2f, 2.0f);
    float c[4], cf1[4], cf2[4];
    uint32_t j;
    int b, i;
    if (!dr_on[0] && !dr_on[1] && dr_lvl[0] <= 0.0f && dr_lvl[1] <= 0.0f)
        return;
    svf_coefs(c, fc, k);
    svf_coefs(cf1, 650.0f, 0.3f);                /* (Choir: two formants, an 'ah') */
    svf_coefs(cf2, 1080.0f, 0.35f);
    for (j = 0; j < n; j++)
        drl[j] = drr[j] = 0.0f;
    for (b = 0; b < 2; b++) {
        float lvl = dr_lvl[b];
        const float to = dr_on[b] ? 1.0f : 0.0f;
        if (!dr_on[b] && lvl <= 0.0f)
            continue;
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
                default: x = sine(v->ph + 2.0f + 0.7f * sine(v->ph2)); break;
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
    if (cv == 1) {                               /* Strings: the ensemble */
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
    const float g = lead_vol * 0.45f, vib = (float)par[P_VIB] * 0.004f + (lv == 3 ? 0.25f : 0.0f), gl = 0.84f, gr = 1.08f;
    uint32_t j;
    float dt = 0.0f;
    if (!lead.on && lead.env < 1e-5f) {
        en_bass_level = 0.0f;                    /* (the meter, at rest) */
        return;
    }
    for (j = 0; j < n; j++) {
        float x, bp, a;
        if (!(j & 15u)) {                        /* the glide, the vibrato, the filter: every 16 */
            const float vd = vib * fm_clampf((lead.vt - 0.4f) * 2.5f, 0.0f, 1.0f), f = mtof(lead.cur + vd * sine(lead.vph));
            dt = f * (1.0f / EN_SR);
            svf_coefs(lead.c1, f * (3.0f + 6.0f * lead.fenv) * ld_tone, 0.9f);
            svf_coefs(lead.c2, f * (3.0f + 6.0f * lead.fenv) * ld_tone, 0.7f);
            lead.vt += 16.0f / EN_SR;
            lead.vph += 16.0f * 5.5f / EN_SR;
            if (lead.vph >= 1.0f)
                lead.vph -= 1.0f;
            lead.fenv *= 0.985f;                 /* (~0.4 s) */
        }
        lead.cur += ((float)lead.note - lead.cur) * ld_gk;
        switch (lv) {
        case 0: x = 0.6f * blep_saw(lead.ph, dt) + 0.4f * (blep_saw(lead.ph, dt) - blep_saw(frac1(lead.ph + 0.35f), dt)); break;
        case 1: x = 0.45f * (blep_saw(lead.ph, dt) + blep_saw(lead.ph2, dt * 1.004f) + blep_saw(lead.ph3, dt * 0.996f)); break;
        case 2: x = sine(lead.ph) + 0.08f * lead.env * (frand() - 0.5f); break;
        case 3: x = sine(lead.ph); break;
        default: x = blep_saw(lead.ph, dt) - blep_saw(frac1(lead.ph + 0.5f), dt); break;
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

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    float epk;
    const float rg = rev_send * rev_in_gain * (1.0f - 0.2f * arcm);
    /* (the cloud listens as if Reverb were at 63 at least, so Cloud works with little reverb too) */
    const float cl_in = rev_send > 0.0001f ? fm_maxf(rev_send, 0.4f) / rev_send : 0.0f;
    const float push = fm_maxf(perf_echo, ar_push);   /* (and the arc's swell) */
    float eg = (echo_send + (1.0f - echo_send) * push) * ein_gain, ca = cloud_amt * (sec == SEC_BREAK ? 1.4f : 1.0f);   /* more in a Break */
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
        mixl[j] = mixr[j] = rsend[j] = esend[j] = kickb[j] = 0.0f;
    lfo_step();
    render_stabs(n);
    render_bells(n);
    render_plucks(n);
    render_drone(n);
    render_lead(n);
    render_crash(n);
    widen(n);                                    /* (the chord's and the pings' dry, wider) */
    render_drums(n);
    if (atm_char == ATM_TAPE)
        render_crackle(n);
    else
        render_atmos(n);
    if (fxm || tw_env > 1e-4f || fx_ph > 0.005f)
        render_fx(n);
    eg += (1.0f - eg) * fx_wash;                 /* Wash: everything into the echo */
    epk = en_echo_pk;
    for (j = 0; j < n; j++) {
        float wl, wr, l, r, el, er, dg, rin, cl, cr, sd, dry = 1.0f - 0.75f * fx_wash;
        duck_d += (duck_x - duck_d) * 0.08f;
        duck_x *= duck_rel;
        dg = 1.0f - 0.3f * duck_depth * duck_d;  /* the duck: the wash ducked lightly */
        echoes(esend[j] * eg, &el, &er);
        {   /* (a strike: the returns down by up to 6 dB, back over ~150 ms; not a throw's, which rides over) */
            float ge;
            ec_dd += (ec_dk - ec_dd) * 0.05f;
            ec_dk *= 0.99985f;
            ge = 1.0f - 0.5f * fm_minf(ec_dd, 1.0f) * (1.0f - st_throw);
            el *= ge;
            er *= ge;
        }
        epk = fm_maxf(epk, fm_fabsf(el + er));   /* (the screen's ripples: the echo as it comes out) */
        rin = (rsend[j] * rg + (el + er) * 0.12f * rev_send) * 0.8f;   /* (the classics pass: less of the echo into
                                                  * the plate, 0.25 before: the repeats stay distinct) */
        if (fx_vt + fx_wash > 0.001f)            /* Reverb, Wash: the dry into the plate */
            rin += (mixl[j] + mixr[j]) * (0.35f * fx_vt + 0.3f * fx_wash);
        cloud(rin * ca * 2.5f * cl_in + pl_back * cloud_amt * 0.22f, &cl, &cr);   /* the grains, into the plate: they bloom;
                                                 * the plate's tail back into the line: the wash grows on itself */
        {   /* the plate, every other sample (its input the pair's mean, its output a line between) */
            const float pin = rin * (1.0f - 0.5f * cloud_amt) + (cl + cr) * 0.8f;
            if ((pl_ph ^= 1u) != 0u) {
                pl_acc = pin;
                wl = pl_ol, wr = pl_or;
            } else {
                float nl, nr;
                reverb(0.5f * (pl_acc + pin), &nl, &nr);
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
        l = (l - sd) * dg;
        r = (r + sd) * dg;
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
        l = (l + kickb[j] * (1.0f - 0.5f * fx_wash) + crl[j]) * 0.75f * gain;   /* (1.3.0: +1.5 dB, what the
                                                                                  * re-balance took, from the headroom;
                                                                                  * the classics pass: +2 dB more, the
                                                                                  * user's ear; the loop held, the
                                                                                  * level steadier) */
        r = (r + kickb[j] * (1.0f - 0.5f * fx_wash) + crr[j]) * 0.75f * gain;
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
    duck_x = fm_flush(duck_x);
    duck_d = fm_flush(duck_d);
    ms_lp1 = fm_flush(ms_lp1);
    ms_lp2 = fm_flush(ms_lp2);
}

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    mods_step(n);
    fx_ctl(n);
    heads_step(n);
    if (en_playing)
        arc += (arc_tgt - arc) * fm_minf((float)n / arc_tau, 1.0f);
    arcm = arc * motion;
    en_arc = arcm;
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
        perf_echo += ((perf & EN_PF_ECHO || fxm >> EN_FX_SWELL & 1u ? 1.0f : 0.0f) - perf_echo) * fm_minf((float)n / (0.04f * EN_SR), 1.0f);
    }
    if (echo_len > 0.0f)                        /* for the screen's ripples */
        en_echo_ms = echo_len * (1000.0f / TAPE_SR);
    en_echo_g = en_frozen ? 1.0f : fm_minf(echo_fb + 0.12f * perf_echo, fb_cap) * 0.82f;
    en_echo_in = echo_send + (1.0f - echo_send) * perf_echo;
    en_duck = duck_d * (duck_depth > 0.0f ? 1.0f : 0.0f);
    cl_dens = cloud_amt > 0.0f ? fm_clampf(0.25f + 0.5f * cloud_amt + 0.3f * arcm, 0.05f, 1.0f) : 0.0f;
    if (fx_cf > 0.01f)                          /* (Freeze: the grains thick, even at Cloud 0) */
        cl_dens = fm_maxf(cl_dens, fx_cf);
    for (i = 0; i < EN_NLOOP; i++)
        en_loop_glow[i] *= 0.985f;
    for (i = 0; i < EN_NDRUM; i++)
        en_drum_glow[i] *= 0.93f;
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
        {
            int32_t w = drums_wait();            /* a swung hit lands on its sample */
            if (w > 0 && (uint32_t)w < k)
                k = (uint32_t)w;
            if (tick_wait > 0 && (uint32_t)tick_wait < k)   /* (and a swung sixteenth) */
                k = (uint32_t)tick_wait;
        }
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        drums_wait_step((int32_t)k);
        if (tick_wait > 0 && (tick_wait -= (int32_t)k) <= 0) {
            tick_wait = 0;
            if (en_playing)
                tick();
        }
        out += 2 * k;
        n -= k;
    }
}
