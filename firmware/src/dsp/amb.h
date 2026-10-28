/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument: a generative dub techno machine. Four layers draw on one pool of notes (a scale: the
 * root plus up to eleven more pitch classes) and one chord that moves now and then through it:
 *
 *   CHORD   the stab: four voices (two detuned saws each, or an FM keys) struck together on a Euclidean
 *           pattern off the beat, through a resonant low-pass that opens with each stab and sweeps with the
 *           arrangement; most of it goes to the echo. One voicing (an open minor 7, or a 9 without its fifth)
 *           shifted whole to each new root, as a chord memory. The chord moves every DRIFT bars, or holds.
 *   PINGS   short FM blips played by four loops of unequal length (13, 17, 23, 29 eighths), each holding a
 *           few notes of the chord that shift against the others; heavily echoed.
 *   BASS    a sub on the chord's root (now and then its fifth), on one of five grooves.
 *   DRUMS   a four-on-the-floor kick (it ducks the rest: DUCK), a shaker on the sixteenths (HITS sets how
 *           many) and an open hat off the beat.
 *
 * Over it all, the ARRANGEMENT: eight-bar phrases, each Full, a Break (the kick and the bass out, the filter
 * building), a Groove (fewer stabs) or Thin (no bass, no shaker); MOTION sets how often it leaves Full. Before
 * the kick comes back, a fill. EVOLVE turns the patterns and throws a stab into the echo now and then.
 * Then a tape echo (to ~5.6 s, two heads, a band-pass in its feedback that the arrangement sweeps, a
 * saturating feedback that may pass 100%), a short Echoplex, a granular CLOUD (after JIANT's) blooming into
 * Dattorro's plate, and a bed of vinyl crackle.
 * FREEZE (dub hold): the echo and the cloud stop listening and play what they hold for ever; the chord holds.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>
#include "seq.h"

#define EN_SR 44100.0f
#define EN_NLOOP 4                     /* ping loops */
#define EN_LOOP_MAX 29
#define EN_NDRUM 5                     /* kick, shaker, open hat, rim, clap */
#define EN_NPAD 4                      /* chord voices */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const uint8_t EN_LOOP_LEN[EN_NLOOP];
extern const uint8_t EN_DRUM_N[EN_NDRUM];

/* parameters: value ranges in en_param_info() */
enum {
    P_CHORD, P_SEQ, P_LEAD, P_SEQ2,                /* layer levels */
    P_DENSITY, P_EVOLVE, P_DRIFT, P_TAPA,             /* motion: the pings' notes, change, the chord's bars, the shaker */
    P_ECHO, P_ETIME, P_REVERB, P_FEEDBACK,            /* space */
    P_CUTOFF, P_DECAY, P_SWEEP, P_MOTION,             /* the stab's filter and length, the pings' tone, the arrangement */
    P_TUNE, P_MIDI, P_TEMPO, P_ROOT, P_MODE, P_SEQOCT,  /* setup, panel encoders, the pings' octave */
    P_CVOICE,                                         /* the chord's voice (EN_CV_NAME) */
    P_SEQ2OCT, P_SEQGRID, P_TRANS,                     /* bass: its octave, its pattern, how often the fifth */
    P_TAPAT, P_TAPB,                                  /* drums: swing, the hats' colour */
    P_SEQLEN, P_CRACKLE, P_TAPBT, P_SIZE,               /* stabs a bar, the vinyl, the kick's ducking, the plate */
    P_CLOUD,                                          /* the granular reverb */
    P_SVOICE, P_LVOICE, P_S2VOICE,                        /* the pings' voice, the bass's, the drums' kit */
    /* the sound pages: each relative to the voice as it is (0), -50 .. +50 (the kick's tune: semitones) */
    P_CATT, P_CCOL, P_CRES, P_DREL,                   /* the chord: attack, colour, resonance, the filter's envelope */
    P_PEDAL, P_SQREV, P_SQECHO, P_LDREV,                  /* the pings: tone, ratio, length, bend */
    P_LOCT, P_VIB, P_LDECHO, P_GLIDE,              /* the bass: bite, shape, length, slide */
    P_MT4, P_DRREV, P_DRECHO, P_S2ECHO,               /* the Atmosphere's Time (below); the Drone's sends, Sequence 2's echo */
    /* (BERLIN, format 7) the filter motion (spec 3): each layer's Move page, Width (0..100: how far its filter wanders,
     * +-2 octaves at 100, the Lead's +-3/4), Time (0..100: a move's length, 1 .. 32 bars), Rate (0..100: how often a
     * move starts, 0 never .. every ~2 bars) and Intro (0..64: bars its filter takes to open at the piece's start). In
     * DUB-1's FM pages' slots and others freed: the Drone's (MW0 .. MI0), the Sequence's (its Width is Sweep: P_MW1),
     * the Lead's (its attack, P_LATT, among them), Sequence 2's and the Atmosphere's further on; Bridge at the end */
    P_MW0, P_MT0, P_MR0, P_MI0,
    P_S2DEC, P_MT1, P_MR1, P_MI1,
    P_MW2, P_LATT, P_MT2, P_MR2,
    /* the chords (EDIT's page): eight slots. A slot is one of three: a chord written (a kind, EN_CQ_*, not 0: its root
     * 0..11, absolute, and its type, EN_CX_*); the walk's (kind 0, root 1..7: a degree of the scale, plus one, so it
     * follows the key and the mode; type: its voicing, 0 or 1); or a gap (kind 0, root 0: the chord before it held).
     * Play: the walk's character in bits 0-1 (EN_WALK_*: Home, Wander, Free, each more varied; ALGORITHM on the page)
     * and Loop in bit 2 (EN_PLOOP; KNOB 4). Endless (Loop off): the chord playing is slot 1; on, all move up a slot and
     * the walk writes slot 8. Loop: the slots in turn, to the last that isn't a gap; nothing new */
    P_CR0, P_CR1, P_CR2, P_CR3, P_CR4, P_CR5, P_CR6, P_CR7,
    P_CQ0, P_CQ1, P_CQ2, P_CQ3, P_CQ4, P_CQ5, P_CQ6, P_CQ7,
    P_CX0, P_CX1, P_CX2, P_CX3, P_CX4, P_CX5, P_CX6, P_CX7,
    P_PMODE,
    /* the drums' pattern page: each lane's sixteen steps (bit i: step i, as int16) and its accents, its chance (%);
     * the accents' strength; Play: Auto (generated, as ever) or Pattern (these) */
    P_SGATE0, P_SGATE1, P_SOCT0, P_SOCT1, P_SSKIP0,
    P_SACC0, P_SACC1, P_MI3, P_MI2, P_MW3,
    P_SCH0, P_SCH1, P_LDIN, P_MT3, P_MR3,
    P_SACCENT, P_SMODE,
    /* the noise (SAVE's Noise page, 1.3.0): Colour, Grain, Room, Drift, -50..+50 around the piece's own (0), and a
     * character (EN_NCHAR_NAME: Vinyl, Tape, Dust, Room, Machine); Crackle (the Noise page's Level) is its level.
     * (Room is off the page: the recipes and the characters still set it) */
    P_NCOL, P_NGRAIN, P_NROOM, P_NDRIFT, P_NCHAR,
    /* 1.3.0's pages (format 13), 0..100: the stabs thrown into the echo (50: as Evolve and the arrangement throw
     * them; 0 never, 100 about three times as often); the pings' dry level against their sends (50: -6 dB, as the
     * sound pass left it; 0: +6 dB; 100: about -20 dB, almost all echo); the bass's notes in its groove (100: every
     * note; less, left out at random, the bar's first kept); the shaker and open hat against the kick (50: x1,
     * about +-12 dB at the ends) */
    P_PHASER, P_S2CUT, P_LDENS, P_S2REV,
    /* 1.3.0's echo and density (format 14), 0..100: the chord's dry against its sends (as the pings' Ghost; 50: -6 dB,
     * its echo x1.35); the kick's density (its shapes by phrase: 0 none .. 100 four on the floor, always); the echo's
     * blur (0 clean copies .. 50 each repeat a little more dissolved .. 100 a wash), its tone (darker .. brighter),
     * its width (0 mono .. 100 wide); the noise's Decay (how long its hits ring: 0 a knock .. 100 a hum) */
    P_LTONE, P_SEQ2LEN, P_EBLUR, P_ETONE, P_EWIDTH, P_NTUNE,
    /* (format 15) the Noise page's second (SAVE again): its width (0 mono, 50 as it was, 100 wide), how much of it
     * into the plate and into the echo (50 as it was, 0 none, 100 twice); the hats' length (-50..+50 around the kit's) */
    P_NWIDTH, P_NPLATE, P_NECHO, P_MW4,
    P_NLFO,                                           /* (1.3.0, format 16) the noise's LFO: its rings' tone, over a bar */
    P_ARC,                                         /* (BERLIN) the Sequence's long rise, its bars (repeating; 0 none) */
    P_MR4, P_MI4,                                  /* (BERLIN, format 7) the Atmosphere's Rate and Intro */
    P_BRIDGE,                                      /* (format 7) the bridge's move on every filter, 0..100 (+-3 octaves) */
    P_LMEM,                                        /* (format 8) Memory: how much the Lead keeps its ideas, 0..100 (the
                                                    * chance a block's motif is kept; 0 a fresh one every pair) */
    P_NPARAMS
};
#define P_MW1 P_SWEEP                              /* (the Sequence's Width is its Sweep: one parameter) */
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_pool(uint32_t pool);                   /* the scale, relative to the root (bit 0 always set) */
void en_play(int on);                          /* it plays / rests (the tails ring on) */
void en_next(void);                            /* the next chord: on the next bar (now while resting) */
void en_freeze(int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new piece: fresh loops and patterns, the home chord */
/* a piece's two patterns, packed (EN_SEQ_N bytes): a piece keeps them; en_seq gives them back (0: none kept, grown
 * afresh), taken at once and again after each en_regrow */
#define EN_SEQ_N 82
void en_seq_get(uint8_t *b);
void en_seq(const uint8_t *b);
/* performance (SEL's mode): layers muted (EN_M_*, a mask: set = muted), and the moves held now (EN_PF_*) */
enum { EN_M_CHORD = 1, EN_M_PINGS = 2, EN_M_BASS = 4, EN_M_KICK = 8, EN_M_HATS = 16, EN_M_NOISE = 32 };   /* (the noise:
                                                * OCT- + OCT+ on its page; its level kept) */
enum { EN_PF_SWEEP = 1, EN_PF_OPEN = 2, EN_PF_CLOSE = 4, EN_PF_DROP = 8, EN_PF_ECHO = 16 };   /* (KICKDROP: Drop, the kick and the bass) */
void en_mute(uint32_t mask);
void en_perf(uint32_t flags);
/* the master actions (HOME's and PERFORM's knobs), -50..50 each, 0: the piece as it is; for playing, never saved.
 * Filter: down a low-pass on the music (Shut's), up a resonant high-pass (Open's), the kick left out; Dub: the
 * echo's send and feedback; Energy: Density, Hits and Stabs; Space: the reverb, the cloud and the plate's size */
enum { EN_MAC_FILTER, EN_MAC_DUB, EN_MAC_ENERGY, EN_MAC_SPACE, EN_NMAC };
void en_macro(int i, int v);
extern volatile int8_t en_mac[EN_NMAC];        /* the master actions as heard now (a piece change: all 0; Filter,
                                                * Wet and Energy moving by themselves with Motion, the hand holding) */
#define EN_MAC_BUSY (-128)
int en_macro_now(int i);                       /* (the UI) action i as heard, once the engine has taken every turn sent
                                                * (else EN_MAC_BUSY) */
/* (host tests only, between renders) a parameter as heard: the piece's, moved by the master actions */
int en_heard(int p);
void en_lock(int on);                          /* PERFORM's Lock: each phrase keeps the kick's shape, the noise's groove,
                                                * the drums' and the bass's plan, the shaker's pattern */
/* the effects grid (REC held): the effects held now, a bit each (EN_FX_*), in the grid's order */
enum { EN_FX_REPEAT, EN_FX_TIME, EN_FX_TAPESTOP, EN_FX_SWELL, EN_FX_LOWPASS, EN_FX_HIGHPASS, EN_FX_SWEEP, EN_FX_PHASER,
       EN_FX_REVERB, EN_FX_TAKEOFF, EN_FX_FREEZE, EN_FX_WASH, EN_FX_OCTAVE, EN_FX_RETURN, EN_FX_TWITTER, EN_FX_SUPERNOVA,
       EN_NFX };
extern const char *const EN_FX_NAME[EN_NFX];
void en_fx(uint32_t mask);
/* (BERLIN) the black keys' moves, held: every layer's filter opened together (Rise, +2 octaves), closed (Close, -4),
 * both sequences three scale steps up (Transpose) */
enum { EN_MV_RISE = 1, EN_MV_CLOSE = 2, EN_MV_TRANSPOSE = 4 };
void en_move(uint32_t mask);
/* MIDI clock in (as FoMni): ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* the voices: each layer's sounds (ALGORITHM on the layer's page) */
#define EN_NCV 5                         /* (BERLIN) the Drone's voices: Organ, Strings, Choir, Saws, Glass */
#define EN_NBV 5
#define EN_NSV 5                        /* (BERLIN) the Sequence's voices: 2600, Moog, Synthi, Pulse, Glass */
#define EN_NS2V 6                       /* Sequence 2's: the same, and Sub */
extern const char *const EN_SV_NAME[EN_NS2V];
extern volatile uint8_t en_seq_len[2];          /* each sequence's return point as it plays (the performer's moves) */
extern volatile int8_t en_s2oct_shift;          /* Sequence 2's octave away from its knob's (home): -1, 0, 1 (Motion's,
                                                 * at the section changes; the UI shows the knob as home + it) */
extern volatile uint16_t en_seq_lane[5], en_seq_acc[2];   /* the Pattern page's lanes as Auto plays them: the
                                                          * Sequence's and Sequence 2's steps, their octaves, the
                                                          * Sequence's skips; their accents */
void en_lead_key(int degree, int on);           /* the white keys: the Lead plays scale degree `degree` (0: the
                                                 * root) from the root up, in its register; on 0 lets it go */
extern volatile int8_t en_lead_deg;             /* the degree the Lead sounds (played or its own): -1 none */
/* (BERLIN, the Lead's memory) its phrases' roles: a pair's call (an open ending) and answer (closed), a sentence's
 * sequence and fragment, its cadence (the run); the role of the phrase playing, and a count of its phrases so far (for
 * tests: a phrase begins where the count moves) */
enum { LR_CALL, LR_ANSWER, LR_SEQ, LR_FRAG, LR_CADENCE };
extern volatile uint8_t en_lead_role, en_lead_phr;
#ifdef EN_HOST
/* (host tests only) the phrase playing's motif slot (0: the theme), a count of its blocks so far (a block begins where
 * it moves), the intensity it was planned at (0..100) */
extern volatile uint8_t en_lead_slot, en_lead_blk, en_lead_int;
/* (host tests only) the most motifs drawn in one tick of the grid since it was last set to 0 (the device's audio block
 * is near its limit: never more than 1); the notes in the white keys' capture buffer now; the theme (slot 0): its
 * entries (degrees from the chord root's, LD_REST: a rest; lengths in sixteenths) into off and dur, its count */
extern volatile uint8_t en_lead_dmax;
int en_lead_cap(void);
int en_lead_theme(int8_t *off, uint8_t *dur);
int en_lead_played(void);                        /* (the theme the keys played: LD_PLAYED on slot 0, 1 or 0) */
/* (host tests only, the Lead's offspring) the kinship its next fresh idea has with a theme the keys played, 0..100 (0:
 * none, the generator's own); a count of the fresh ideas its blocks took (an idea begins where it moves) and how the
 * last was made (LK_*); the line's idea now: its entries into off and dur, its count (0: none yet) */
int en_lead_kin(void);
extern volatile uint8_t en_lead_ideas, en_lead_kind;
int en_lead_idea(int8_t *off, uint8_t *dur);
#endif
/* (the Lead's offspring) how a fresh idea was made: the generator's own; from a theme the keys played, its head (2-4
 * notes) continued; its head and the head again on another chord tone (a sequence); its contour mirrored; its notes
 * backwards; its tail continued; its rhythm with new notes */
enum { LK_PLAIN, LK_HEAD, LK_SEQ, LK_MIRROR, LK_BACK, LK_TAIL, LK_RHY };
#define LD_REST (-128)                          /* a rest inside a motif */
/* (host tests only, between renders) motif slot `slot` (0..3) written: n entries (1..8), each a scale degree from the
 * chord root's degree (LD_REST: a rest) and its length in sixteenths; the Lead's next pair plays it */
void en_lead_motif(int slot, const int8_t *off, const uint8_t *dur, int n);
void en_seq_set(int layer, const seq_t *s);   /* a pattern written into a sequence (0: the Sequence, 1:
                                               * Sequence 2): played as written, from its first step */
extern const char *const EN_CV_NAME[EN_NCV], *const EN_BV_NAME[EN_NBV];

/* (BERLIN) the arc's sections (en_section): Emerge while an Intro is still opening, then Flow, Solo and Bridge for ever */
enum { EN_SEC_EMERGE, EN_SEC_FLOW, EN_SEC_SOLO, EN_SEC_BRIDGE, EN_NSEC };
extern const char *const EN_SEC_NAME[EN_NSEC];
extern volatile uint8_t en_swell;               /* the arc's swell into Flow (the Bridge's last two bars) */
/* (BERLIN) the filter motion, by layer (the Drone, the Sequence, the Lead, Sequence 2, the Atmosphere): each one's
 * offset from its brightness knob now, in octaves, and which way it moves (-1 darker, 0 still, +1 brighter) */
extern volatile float en_fo[5];
extern volatile int8_t en_fo_dir[5];
extern volatile float en_atm_gain;              /* (for tests) the Atmosphere's gain now: its level's, nothing more */
extern volatile float en_pluck_cut[2];          /* (for tests) each sequence's last note's cutoff, Hz */

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile uint8_t en_chord[EN_NPAD];     /* the chord's notes (MIDI), as the voices play them */
extern volatile uint8_t en_chord_root;         /* the chord's root, MIDI pitch class 0..11 */
extern volatile uint8_t en_chord_n;            /* its distinct tones: 3 or 4 */
extern volatile float en_drift;                /* 0..1: how far to the next chord (0 while it holds) */
extern volatile float en_pad_level, en_bass_level;   /* the stab's and the bass's envelopes, 0..1 */
extern volatile uint8_t en_loop_pos[EN_NLOOP];
extern volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];   /* -1 rest, else the slot's offset */
extern volatile float en_loop_glow[EN_NLOOP];  /* 0..1: a ping just sounded from this loop */
extern volatile uint8_t en_drum_pos[EN_NDRUM];
extern volatile uint32_t en_drum_pat[EN_NDRUM];/* bit i: a hit on step i (all 16 steps) */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile uint8_t en_pulse_rest;         /* the kick is out (a Break) */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice plays (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */
extern volatile uint8_t en_section;            /* EN_SEC_*: the arc's section playing */
extern volatile uint8_t en_phrase_bar;         /* 0..7: the bar of eight */
extern volatile uint32_t en_stab_pat;          /* the stabs' pattern this bar (bit i: step i) */
extern volatile uint32_t en_throws;            /* stabs thrown into the echo so far (for tests) */
/* for the screen's ripples and lanes */
extern volatile uint32_t en_stabs;             /* stabs struck so far */
extern volatile float en_stab_vel;             /* the last one's strength, 0..1 */
extern volatile uint8_t en_stab_thrown;        /* the last one was thrown */
extern volatile float en_echo_ms, en_echo_g;   /* the echo's time; the level a repeat keeps of the last (1: held) */
extern volatile float en_echo_in;              /* how much of the stab goes into the echo, 0..1 */
extern volatile float en_duck;                 /* the ducking now, 0 .. 1 (full depth) */
/* the output, for the ripples' outlines: the last 256 samples (mono, a mean of each eight), with where the next goes */
#define EN_SCOPE_N 256
extern volatile int16_t en_scope[EN_SCOPE_N];
extern volatile uint32_t en_scope_w;
/* each layer's notes struck so far, the last one's strength */
extern volatile uint32_t en_hits[5];
extern volatile float en_hit_vel[5];
extern volatile float en_layer_act[5];           /* (BERLIN) the Drone's, the Sequence's, the Lead's, Sequence 2's and the
                                                  * Atmosphere's activity at its level, 0..1 (the meters) */
extern volatile uint32_t en_seq_hits[2], en_seq_reps[2];   /* (BERLIN) each sequence's notes and repeats, so far */
extern volatile float en_echo_pk;               /* the echo's output, its peak since the screen last took it */
/* the noise's events, for the screen's specks: a ring of the last few (kind: 0 a click, 1 a pop, 2 a dust grain; its
 * strength as heard, 0..255; its side, 0 left .. 255 right), en_nev_w where the next goes */
/* the audio's load, the last block's time against its budget, in % (set by the device's audio handler; 0 elsewhere):
 * over 80, the engine sheds for a moment what is least heard (the quietest tails, the cloud's new grains), so a crowded moment doesn't overrun */
extern volatile uint8_t en_load;
#define EN_NEV_N 16
extern volatile uint8_t en_nev_k[EN_NEV_N], en_nev_s[EN_NEV_N], en_nev_p[EN_NEV_N];
extern volatile uint32_t en_nev_w;
extern volatile uint16_t en_lane[6];           /* this bar as it plays: the stabs, the kick, the hats, the rim, the clap, the bass */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* the stab's voicing on degree deg: four intervals above the chord's root, ascending (shape 0: an open
 * 7th chord, root 5th 7th 10th; shape 1: root 7th 10th 14th, a 9th without its fifth) */
void en_voicing(uint32_t pool, int deg, int shape, int *iv);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */
int en_drift_bars(int v);                      /* Drift's value in bars; 0: the chord holds */
#define EN_NPROG 8
enum { EN_CQ_EMPTY, EN_CQ_MAJOR, EN_CQ_MINOR, EN_CQ_SUS2, EN_CQ_SUS4, EN_CQ_DIM, EN_CQ_AUG, EN_NCQ };
enum { EN_CX_TRIAD, EN_CX_6, EN_CX_7, EN_CX_MAJ7, EN_CX_9, EN_CX_11, EN_CX_ADD9, EN_NCX };
extern const char *const EN_CQ_NAME[EN_NCQ], *const EN_CX_NAME[EN_NCX];
extern volatile int8_t en_prog_pos;            /* the slot the progression is on (0..7; Endless: 0), -1: none */
/* the slots as the engine has them (it writes the walk's, and moves them up in Endless): root, kind, type each, and
 * a count that changes when they do (the UI takes them into the project) */
extern volatile int8_t en_prog_slot[EN_NPROG][3];
extern volatile uint32_t en_prog_gen;
/* a walk slot's chord as written (to edit it): its root (absolute), kind and type, from the scale's degree */
void en_walk_chord(uint32_t pool, int root, int deg, int *r, int *q, int *x);
void en_prog_rotate(void);                     /* Loop to Endless (by hand): the slot playing up to slot 1 */
void en_prog_rewalk(void);                     /* the walk's character turned (by hand): its slots walked again, all but
                                                * the one playing (yours and the gaps kept) */
enum { EN_WALK_HOME, EN_WALK_WANDER, EN_WALK_FREE, EN_NWALK, EN_PLOOP = 4 };
extern const char *const EN_WALK_NAME[EN_NWALK];
#define EN_NNCHAR 6                      /* (BERLIN) the Atmosphere's: None, Wind, Surf, Space, Air, Tape */
extern const char *const EN_NCHAR_NAME[EN_NNCHAR];   /* the noise's characters */
extern volatile int8_t en_chord_slot;          /* the slot whose chord sounds, -1: a chord of the scale */
int en_deg_of(uint32_t pool, int rel);         /* the scale degree for a note above the root (nearest in the pool) */
void en_chord_iv(int kind, int type, int *iv); /* a slot's voicing: four intervals above its root, ascending */
const char *en_chord_label(int root, int kind, int type, char *buf);   /* "Fm7", "Bbsus4", "Eb6" ("-": empty) */

/* the pieces: recipes for every parameter (Tune and MIDI out included, but they are the player's) */
#define EN_NPIECES 8
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par, uint16_t *pool);
