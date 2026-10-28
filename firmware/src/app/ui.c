/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays)
 *   PLAY           plays / rests                   EDIT  tapped: the CHORDS page (eight slots: KNOB 1 the slot, the
 *                  keys its root, KNOB 2 its kind, KNOB 3 its type, KNOB 4 Play: Endless or Loop); held: the next chord
 *   REC            tapped: DUB HOLD (the echo repeats what it holds, the chord holds); held: the EFFECTS grid, played
 *                  on the white keys while they're held
 *   OCT- / OCT+    on the Sequence's, the Lead's and Sequence 2's pages: its octave, a step a press (both held 5 s:
 *                  the update mode)
 *   SEL            PERFORM: the keys mute (white 1-5) and solo (black 1-5) the layers and, held, make the dub moves
 *                  (white 7-11); the knobs the master actions (Filter, Dub, Energy, Space), with white 6 held the mix
 *   SELECT         a layer's pages: its level (elsewhere nothing)   ALGORITHM the voice, the walk
 *                  (Chords), the noise's character                    PRESETS the piece
 *   FX ENV LFO GLO / HOME SAVE ARP SEQ   the pages: SPACE, CHORD, BASS, SETUP / DUB, NOISE, PINGS, DRUMS
 *   KNOB 1-4       the page's four values
 *   GLO held       saves (it also saves by itself a few seconds after a change, while resting);
 *                  held on 1.5 s: the piece back to its recipe */
#include "../dsp/fastmath.h"

/* the pages, one a layer (a button each, NPAGES; the Noise page, V_NSND, SAVE's); the header's dots show them in the
 * panel's order, PAGE_ORDER below (the view numbers stay as they were: the tests use them) */
/* (past NPAGES, on no button of their own: V_MIX, PERFORM's knobs, the four layers' levels; the layers' sound
 * pages, the layer's button pressed again) */
enum { V_DUB, V_CHORD, V_TEXTURE, V_BASS, V_DRUMS, V_SPACE, V_SETUP, NPAGES, V_MIX = NPAGES, V_CSND, V_PSND, V_BSND,
       V_DSND, V_CFM, V_PFM, V_BFM, V_CHORDS, V_PAT, V_PERF, V_KEY, V_NSND, V_ESND, V_NSND2, V_ARC, V_SMOVE, V_S2MOVE, V_AMOVE, NVIEWS };
/* (V_PERF: PERFORM's knobs, the master actions; V_MIX while white 6 is held. V_KEY: HOME again, the key and the
 * tempo, the Dub page's second) */
static const char *const VIEW_NAME[NVIEWS] = {"Nachtfahrt", "Drone", "Sequence", "Lead", "Sequence 2", "Space", "Setup",
                                              "Mix", "Drone sound", "Seq sound", "Lead sound", "Seq2 sound",
                                              "Drone move", "Repeats", "Lead move", "Chords", "Pattern", "Perform", "Key",
                                              "Atmosphere", "Echo", "Atmos sound", "Arc", "Seq move", "Seq2 move",
                                              "Atmos move"};
#define B_NONE 0xFFu
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_ENV, B_ARP, B_LFO, B_SEQ, B_FX, B_GLO, B_NONE, B_NONE, B_NONE, B_NONE,
                                         B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_SAVE, B_NONE, B_NONE,
                                         B_NONE, B_NONE, B_NONE, B_NONE};
static int sound_of(int v) { return v == V_CHORD ? V_CSND : v == V_TEXTURE ? V_PSND : v == V_BASS ? V_BSND : v == V_DRUMS ? V_DSND : -1; }
static int fm_of(int v) { return v == V_TEXTURE ? V_PFM : -1; }   /* (BERLIN: no FM pages; the Sequence's third, its
                                                                   * Repeats, in the Ping FM page's place) */
static int parent_of(int v)
{
    return v == V_CSND || v == V_CFM ? V_CHORD : v == V_PSND || v == V_PFM || v == V_SMOVE ? V_TEXTURE
         : v == V_BSND || v == V_BFM ? V_BASS : v == V_DSND || v == V_PAT || v == V_S2MOVE ? V_DRUMS : -1;
}
static int next_page(int v)                      /* (BERLIN) a layer's button again: its next page, the last back to the
                                                  * first (the Move page, the filter motion's, last) */
{
    switch (v) {
    case V_CSND: return V_CFM;
    case V_PSND: return V_PFM;
    case V_PFM: return V_SMOVE;
    case V_BSND: return V_BFM;
    case V_DSND: return V_PAT;
    case V_PAT: return V_S2MOVE;
    default: return parent_of(v);
    }
}
static int atm_view(int v) { return v == V_NSND || v == V_NSND2 || v == V_AMOVE; }   /* (the Atmosphere's pages) */
static int page_of(int v)                        /* a sound page: its layer's; the Echo page: Space's */
{
    return v == V_KEY ? V_DUB : v == V_ESND ? V_SPACE : v == V_NSND2 || v == V_AMOVE ? V_NSND : v == V_ARC ? V_SETUP
         : parent_of(v) >= 0 ? parent_of(v) : v;
}
/* the header's dots, as the buttons sit on the panel: FX ENV LFO GLO, then HOME SAVE ARP SEQ (SELECT's round once) */
#define NROUND 8
static const uint8_t PAGE_ORDER[NROUND] = {V_SPACE, V_CHORD, V_BASS, V_SETUP, V_DUB, V_NSND, V_TEXTURE, V_DRUMS};
/* the layer a page is (SELECT: its level; OCT- / OCT+: its octave): the Chord, Pings, Bass, Drums and Noise pages
 * and their sound, FM and Pattern pages; -1 elsewhere (the Chords page too) */
#define NLAYER 5
static int oct_layer(int v)
{
    v = parent_of(v) >= 0 ? parent_of(v) : v;
    return v == V_CHORD ? 0 : v == V_TEXTURE ? 1 : v == V_BASS ? 2 : v == V_DRUMS ? 3 : atm_view(v) ? 4 : -1;
}
/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum { K_LEDS = 200, K_CPU, K_NONE, K_SLOT, K_CQ, K_CX,    /* (the Chords page: the slot, and its kind and type) */
       K_LANE, K_LCH,                                        /* (the Pattern page: the lane, and its chance) */
       K_FILTER, K_DUB, K_ENERGY, K_SPACE,                   /* the master actions (EN_MAC_*), in that order */
       K_MODE,                                               /* (the Key page: the mode, as ALGORITHM sets it) */
       K_PLAY,                                               /* (the Chords page: Endless or Loop, P_PMODE's bit 2) */
       K_SAVE };                                             /* (Setup: a turn saves; it shows whether it's saved) */
static const char *const LANE_NAME[EN_NDRUM] = {"Steps", "Steps 2", "Octaves", "Octaves 2", "Skips"};   /* (BERLIN:
                                                  * the Sequence's and Sequence 2's) */
static int lane_seq(int d) { return d == 1 || d == 3; }   /* (a lane's sequence: its accents, its chance) */
_Static_assert(P_NPARAMS <= K_LEDS, "the knobs' own codes must come after the parameters");
static const uint16_t VIEW_KNOB[NVIEWS][4] = {
    {K_FILTER, P_EVOLVE, P_MOTION, P_DRIFT},
    {P_PEDAL, P_CRES, P_PHASER, P_CATT},          /* (BERLIN) the Drone */
    {P_DENSITY, P_SEQOCT, P_SEQLEN, P_SEQGRID},  /* the Sequence */
    {P_LDENS, P_LOCT, P_GLIDE, P_VIB},           /* (the Lead) */
    {P_SCH1, P_SEQ2OCT, P_SEQ2LEN, P_SEQ2},      /* (Sequence 2) */
    {P_ECHO, P_ETIME, P_FEEDBACK, P_REVERB},
    {P_TUNE, P_MIDI, K_LEDS, K_SAVE},
    {P_CHORD, P_SEQ, P_LEAD, P_SEQ2},
    {P_CCOL, P_DREL, P_DRREV, P_DRECHO},        /* the Drone's sound */
    {P_CUTOFF, P_DECAY, P_SQREV, P_SQECHO},      /* the Sequence's sound */
    {P_LTONE, P_LATT, P_LDREV, P_LDECHO},        /* the Lead's sound */
    {P_S2CUT, P_S2DEC, P_S2REV, P_S2ECHO},       /* Sequence 2's sound */
    {P_MW0, P_MT0, P_MR0, P_MI0},                /* the Drone's Move (ENV, a third time) */
    {P_TAPA, P_TAPAT, P_TAPB, P_TAPBT},          /* the Sequence's repeats (its third page) */
    {P_MW2, P_MT2, P_MR2, P_MI2},                /* the Lead's Move (LFO, a third time) */
    {K_SLOT, K_CQ, K_CX, K_PLAY},
    {K_LANE, P_SACCENT, K_LCH, P_SMODE},
    {K_FILTER, K_DUB, K_ENERGY, P_SWEEP},         /* (Wet, Energy: offsets for playing; Sweep the piece's own) */
    {P_TEMPO, P_ROOT, K_MODE, P_TRANS},
    {P_NGRAIN, P_NWIDTH, P_NDRIFT, P_NROOM},       /* the Atmosphere */
    {P_EBLUR, P_ETONE, P_EWIDTH, P_SIZE},
    {P_NCOL, P_NTUNE, P_NPLATE, P_NECHO},      /* its space */
    {P_LDIN, P_BRIDGE, P_ARC, K_NONE},         /* the Arc (GLO again): how the piece builds (the intros are each
                                                  * layer's, on its Move page) */
    {P_MW1, P_MT1, P_MR1, P_MI1},                /* the Sequence's Move (ARP, a fourth time; its Width is Sweep) */
    {P_MW3, P_MT3, P_MR3, P_MI3},                /* Sequence 2's Move (SEQ, a fourth time) */
    {P_MW4, P_MT4, P_MR4, P_MI4},                /* the Atmosphere's Move (SAVE, a third time) */
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Filter", "Evolve", "Motion", "Drift"},
    {"Pedal", "Reso", "Phaser", "Attack"},
    {"Density", "Octave", "Length", "Grid"},
    {"Phrases", "Octave", "Glide", "Vibrato"},
    {"Chance", "Octave", "Length", "Level"},
    {"Echo", "Echo time", "Feedback", "Reverb"},
    {"Tune", "MIDI out", "Lights", "Save"},
    {"Drone", "Seq", "Lead", "Seq 2"},
    {"Colour", "Release", "Reverb", "Echo"},
    {"Cutoff", "Decay", "Reverb", "Echo"},
    {"Tone", "Attack", "Reverb", "Echo"},
    {"Cutoff", "Decay", "Reverb", "Echo"},
    {"Width", "Time", "Rate", "Intro"},
    {"Repeat A", "A time", "Repeat B", "B time"},
    {"Width", "Time", "Rate", "Intro"},
    {"Slot", "Chord", "Type", "Play"},
    {"Lane", "Accent", "Chance", "Play"},
    {"Filter", "Wet", "Energy", "Sweep"},
    {"Tempo", "Root", "Mode", "Transpose"},
    {"Density", "Width", "Drift", "Air"},
    {"Blur", "Tone", "Width", "Size"},
    {"Colour", "Ring", "Reverb", "Echo"},
    {"Lead in", "Bridge", "Arc", ""},
    {"Width", "Time", "Rate", "Intro"},
    {"Width", "Time", "Rate", "Intro"},
    {"Width", "Time", "Rate", "Intro"},
};
/* what each knob does, a line: on the rule above the knobs while it's being turned (in capitals; each fits the
 * rule's 228 px, and every view has its row: the host's expect helpfit) */
_Static_assert(NVIEWS == 26, "a view added: its KNOB_HELP row too");
static const char *const KNOB_HELP[NVIEWS][4] = {   /* (BERLIN's, a row a view, V_DUB .. V_ARC) */
    {"Down shuts, up opens; Off mid", "How often a step changes", "How much, and the sections' pace", "Bars before the chord moves on"},
    {"The root an octave under", "The filter's resonance", "The slow phaser's depth", "How slowly the chord swells in"},
    {"How many of the steps sound", "Two octaves down to two up", "Steps before it returns", "Eighths, or sixteenths"},
    {"How often it plays a phrase", "Two octaves down to one up", "How long it slides, note to note", "How deep the late vibrato"},
    {"How likely each note plays", "Its octave (Motion moves it a while)", "Steps: a step short, it drifts", "Its level"},
    {"How much goes into the echo", "The echo's time: 1/8 to a bar", "How long the repeats go on", "How much goes into the plate"},
    {"Fine tuning, in cents", "Notes and clock out over MIDI", "The keys' and buttons' lights", "A turn saves all eight pieces"},
    {"The Drone's level", "The Sequence's level", "The Lead's level", "Sequence 2's level"},
    {"Darker, or brighter and wider", "How long it fades at a change", "How much goes into the plate", "How much goes into the echo"},
    {"The filter: dark to bright", "How long each note rings", "How much goes into the plate", "How much goes into the echo"},
    {"Dark and soft, or bright", "How slowly a note swells in", "How much goes into the plate", "How much goes into the echo"},
    {"The filter: dark to bright", "How long each note rings", "How much goes into the plate", "How much goes into the echo"},
    {"How far its filter wanders", "How long one move takes", "How often a new move starts", "Bars its filter takes to open"},
    {"Each note again, as loud: 100", "When: a dotted eighth, classic", "A second repeat, into rests", "When the second comes"},
    {"How far its tone wanders: gently", "How long one move takes", "How often a new move starts", "Bars its tone takes to open"},
    {"The slot you're writing", "Its kind (Empty: a gap)", "Triad, 6, 7, Maj7, 9, 11, Add9", "Endless, or the slots in a loop"},
    {"The lane you're writing", "How much quieter off the root", "How likely the lane's steps play", "Auto patterns, or your own"},
    {"Down shuts, up opens; Off mid", "The whole music dry, or drowned", "Sparser, or busier", "How far the filter wanders"},
    {"Beats a minute", "The key's root note", "The scale: Minor, Major, ...", "How the steps follow the chord"},
    {"How busy it is", "Mono, to wide", "How quickly it changes", "The air under it"},
    {"Clean repeats, to a dissolving wash", "The repeats darker, or brighter", "The echo mono, to wide", "How long the plate rings"},
    {"Darker, or brighter", "How much it rings", "How much goes into the plate", "How much goes into the echo"},
    {"Bars before the Lead may start", "How far the bridge moves filters", "The Sequence's long rise: 0 none", ""},
    {"How far its filter wanders", "How long one move takes", "How often a new move starts", "Bars its filter takes to open"},
    {"How far its filter wanders", "How long one move takes", "How often a new move starts", "Bars its filter takes to open"},
    {"How far its filter wanders", "How long one move takes", "How often a new move starts", "Bars its filter takes to open"},
};

/* (BERLIN) the screen's colours: Cosmic, on black (spec 15): lavender text, a cyan accent; the layers in their own
 * colours, the Drone violet, the Sequence pink, the Lead gold, Sequence 2 cyan, the Atmosphere dim lavender; each with a
 * dark tint for the shapes at rest. (The names stay DUB-1's: K_PAD the Drone, K_BELL the Sequence, K_BASS the Lead,
 * K_PULSE Sequence 2, K_NOISE the Atmosphere.) */
#define K_BG RGB(0, 0, 0)
#define K_PANEL RGB(26, 23, 38)
#define K_LINE RGB(46, 41, 66)
#define K_DIM RGB(141, 127, 184)
#define K_TEXT RGB(239, 231, 255)
#define K_ACC RGB(127, 216, 255)         /* the accent: the page, the knob being turned, a message's word */
#define K_BELL RGB(255, 106, 213)
#define K_BELL_T RGB(96, 36, 82)
#define K_PAD RGB(180, 140, 255)
#define K_PAD_T RGB(58, 44, 96)
#define K_PULSE RGB(127, 216, 255)
#define K_PULSE_T RGB(36, 66, 84)
#define K_BASS RGB(255, 210, 127)
#define K_BASS_T RGB(98, 80, 44)
#define K_FROST RGB(232, 92, 84)         /* dub hold */
#define K_FROST_T RGB(128, 56, 52)
#define K_RIM RGB(214, 124, 176)         /* the rim */
#define K_RIM_T RGB(108, 58, 86)
#define K_CLAP RGB(232, 206, 98)         /* the clap */
#define K_CLAP_T RGB(108, 98, 48)
#define K_NOISE RGB(186, 174, 156)       /* the noise (its level ring) */

static struct {
    uint8_t view;
    uint8_t perf, perf_from;           /* PERFORM (SEL): the keys play the mutes and the moves, the knobs
                                        * the mix; the page it was started from */
    uint32_t mutes, moves;             /* the layers muted (EN_M_*), the moves held (EN_PF_*) */
    uint32_t solo;                     /* the layers soloed (EN_M_*): the others muted while any is */
    int8_t mac[EN_NMAC];               /* the master actions (HOME's Filter, PERFORM's knobs): -50..50, never saved */
    uint32_t fx;                       /* the effects held (EN_FX_*), REC's grid */
    uint32_t bfx;                      /* (BERLIN) the grid's effects held on the black keys (EN_FX_*) */
    uint8_t mv;                        /* (BERLIN) the black keys' moves held (EN_MV_*) */
    uint8_t slot, chords_from;         /* the Chords page: the slot edited; the page it was opened from */
    uint8_t noise_from;                /* the Noise page (SAVE): the page it was opened from */
    int8_t oct_home[NLAYER];           /* each layer's octave as its piece was loaded (OCT- / OCT+ lit away from it) */
    uint8_t oct_ring;                  /* the level ring in the middle (SELECT): its layer + 1 (0: none) */
    uint8_t s2_hand;                   /* Sequence 2's Octave set by hand while Motion had it shifted: the shift not
                                        * shown till the engine has dropped it (a block later) */
    uint32_t card_until, card_t0;      /* a nudge's card in the middle: up until, then fading; when it came */
    uint8_t card_shake;                /* the card a big move's (red: a shake, new chords, a reset) or not */
    uint16_t card_col;                 /* its colour */
    uint8_t lock;                      /* Lock (SEL held): each phrase keeps what it has */
    char card_head[16], card_text[24];
    uint32_t oct_until, oct_pulse;     /* the ring: up until, then fading; its last step (a pulse) */
    float oct_v;                       /* the ring's arc, gliding to the level */
    uint32_t prog_gen;                 /* the engine's slots, as last taken (en_prog_gen) */
    uint8_t lane;                      /* the Pattern page: the lane edited */
    uint8_t grid_said;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
} ui;

/* the screen reader (announce.c; speak() below): a message went to it this frame, so speak() leaves what it
 * changed be; and the transport as last said (PLAY says its own: the engine takes it a block later) */
static uint8_t sp_said;
static char sp_buf[ANN_MAX + 1];                 /* (the screen reader's text, built below) */
static uint32_t sp_n;
static void sp_cat(const char *s);
static struct {
    uint8_t playing;
    uint32_t play_t;                   /* when PLAY (or the start) set it: the engine catches up within ms */
} sp_tp;
static void sp_piece(void);
static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader gets it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
/* a message for a moment in the header; flags: how a screen reader gets it (0: the caller tells it) */
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* a long press's card in the middle (a nudge, a shake, the next chord, new chords, saved, a reset): what it did, in
 * big, outlined in its colour; it fades out CARD_SHOW after (draw_card) */
#define CARD_SHOW 1500u
#define CARD_FADE 300u
static int str_begins(const char *s, const char *p)   /* (the firmware's libc has no strcmp, strncmp) */
{
    while (*p && *s == *p)
        s++, p++;
    return !*p;
}
static int card_on(uint32_t now) { return ui.card_until && now < ui.card_until + CARD_FADE; }
static void card_show(const char *head, const char *text, uint16_t col, int big)
{
    int i;
    for (i = 0; head && head[i] && i < (int)sizeof ui.card_head - 1; i++)
        ui.card_head[i] = head[i];
    ui.card_head[i] = 0;
    for (i = 0; text && text[i] && i < (int)sizeof ui.card_text - 1; i++)
        ui.card_text[i] = text[i];
    ui.card_text[i] = 0;
    ui.card_col = col;
    ui.card_shake = (uint8_t)big;
    ui.card_t0 = plat_ms();
    ui.card_until = ui.card_t0 + CARD_SHOW;
}
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)proj.par[P_ROOT];
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)
{
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? EN_MODE_NAME[proj.par[P_MODE]] : "Custom";
}

/* ------------------------------------------------------------ values --- */
/* the modes as they're stepped through (ALGORITHM, the Key page's Mode): the most common first. (The values kept
 * are the engine's, MODE_*) */
static const uint8_t MODE_ORDER[EN_NMODES] = {MODE_AEOLIAN, MODE_IONIAN, MODE_DORIAN, MODE_MIXOLYDIAN, MODE_PENT_MIN,
                                              MODE_PENT_MAJ, MODE_PHRYGIAN, MODE_LYDIAN};
static int mode_pos(int m)
{
    int i;
    for (i = 0; i < EN_NMODES && MODE_ORDER[i] != m; i++)
        ;
    return i < EN_NMODES ? i : 0;
}
/* the Chords page's slots (amb.h): written (a kind), the walk's (no kind, a degree + 1 as the root) or a gap */
static int slot_is_walk(int i) { return !proj.par[P_CQ0 + i] && proj.par[P_CR0 + i]; }
static int slot_is_gap(int i) { return !proj.par[P_CQ0 + i] && !proj.par[P_CR0 + i]; }
static int scale_steps(void)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(proj.pool >> i & 1u);
    return n ? n : 1;
}
static void walk_written(int i, int *r, int *q, int *x)   /* a walk slot's chord, as it would be written */
{
    en_walk_chord(proj.pool, proj.par[P_ROOT], (proj.par[P_CR0 + i] - 1) % scale_steps(), r, q, x);
}
static void knob_set(int k, int v);
static void oct_home_take(void);
static void slot_write(int i)                    /* a walk slot about to be edited: written as it sounds */
{
    int r, q, x;
    if (!slot_is_walk(i))
        return;
    walk_written(i, &r, &q, &x);
    knob_set(P_CR0 + i, r);
    knob_set(P_CX0 + i, x);
    knob_set(P_CQ0 + i, q);
}
static int s2_shift(void) { return ui.s2_hand ? 0 : en_s2oct_shift; }   /* (Motion's move of Sequence 2's octave) */
static int knob_get(int k)
{
    if (k < P_NPARAMS)                           /* (Sequence 2's Octave: its home and Motion's shift, as it plays;
                                                  * the project keeps home) */
        return proj.par[k] + (k == P_SEQ2OCT ? s2_shift() : 0);
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    if (k == K_SLOT)
        return ui.slot;
    if (k == K_CQ || k == K_CX) {
        if (slot_is_walk(ui.slot)) {             /* the walk's: its chord as it would be written */
            int r, q, x;
            walk_written(ui.slot, &r, &q, &x);
            return k == K_CQ ? q : x;
        }
        return proj.par[(k == K_CQ ? P_CQ0 : P_CX0) + ui.slot];
    }
    if (k == K_LANE)
        return ui.lane;
    if (k == K_LCH)
        return proj.par[P_SCH0 + lane_seq(ui.lane)];
    if (k >= K_FILTER && k <= K_SPACE)
        return ui.mac[k - K_FILTER];
    if (k == K_MODE)
        return mode_pos(proj.par[P_MODE]);
    if (k == K_PLAY)
        return proj.par[P_PMODE] & EN_PLOOP ? 1 : 0;
    if (k == K_SAVE)
        return ui.dirty ? 1 : 0;
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else if (k >= K_FILTER && k <= K_SPACE) {
        *lo = -50;
        *hi = 50;
    } else {
        *lo = 0;
        *hi = k == K_PLAY ? 1 : k == K_MODE ? EN_NMODES - 1 : k == K_LEDS ? 2 : k == K_CPU ? 100 : k == K_SLOT ? EN_NPROG - 1 : k == K_CQ ? EN_NCQ - 1 : k == K_CX ? EN_NCX - 1
            : k == K_LANE ? EN_NDRUM - 1 : k == K_LCH ? 100 : 0;
    }
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS)
        return en_param_info(k)->name;
    return k == K_LEDS ? "Lights" : k == K_CPU ? "CPU" : k == K_SLOT ? "Slot" : k == K_CQ ? "Chord" : k == K_CX ? "Type"
         : k == K_LANE ? "Lane" : k == K_LCH ? "Chance" : k == K_FILTER ? "Filter" : k == K_DUB ? "Wet"
         : k == K_ENERGY ? "Energy" : k == K_SPACE ? "Duck" : k == K_MODE ? "Mode" : k == K_PLAY ? "Play" : k == K_SAVE ? "Save" : "";
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else if (k == K_SLOT) {
        itoa_u((uint32_t)v + 1u, b);
    } else if (k == K_CQ || k == K_CX) {
        en_param_text((k == K_CQ ? P_CQ0 : P_CX0) + ui.slot, v, b);
    } else if (k == K_LANE) {
        const char *n = LANE_NAME[(unsigned)v < EN_NDRUM ? v : 0];
        while ((*b++ = *n++))
            ;
    } else if (k == K_LCH) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else if (k == K_FILTER) {                  /* Off in the middle; Shut (the low-pass) down, Open up, 0..100 */
        const char *n = v < 0 ? "Shut " : "Open ";
        if (!v) {
            n = "Off";
            while ((*b++ = *n++))
                ;
            return;
        }
        while (*n)
            *b++ = *n++;
        itoa_u((uint32_t)(v < 0 ? -v : v) * 2u, b);
    } else if (k >= K_DUB && k <= K_SPACE) {     /* 0: the piece as it is; more or less of it, -100..+100 */
        if (v)
            *b++ = v < 0 ? '-' : '+';
        itoa_u((uint32_t)(v < 0 ? -v : v) * 2u, b);
    } else if (k == K_MODE) {
        const char *n = mode_name();
        while ((*b++ = *n++))
            ;
    } else if (k == K_PLAY) {
        const char *n = v ? "Loop" : "Endless";
        while ((*b++ = *n++))
            ;
    } else if (k == K_SAVE) {                    /* changed since the last save: "Save" (turn it); else "Saved" */
        const char *n = v ? "Save" : "Saved";
        while ((*b++ = *n++))
            ;
    } else {
        b[0] = 0;
    }
}

static void set_mode(int m);
static void do_save(void)                       /* GLO held, or Setup's KNOB 4 turned */
{
    if (project_save() == 0) {
        ui.dirty = 0;
        say("SAVED", 0);
    } else {
        warn("NOT SAVED", "FLASH ERROR");
    }
}
static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k) && !(k == P_SEQ2OCT && s2_shift()))   /* (Sequence 2's Octave, shifted: the hand makes what
                                                               * it shows home, even unchanged) */
        return;
    if (k < P_NPARAMS) {
        int was = proj.par[k];
        if (k == P_SEQ2OCT && en_s2oct_shift)
            ui.s2_hand = 1;
        proj.par[k] = (int16_t)v;
        en_set(k, v);
        if (k == P_PMODE && (was & EN_PLOOP) && !(v & EN_PLOOP))
            en_prog_rotate();                    /* (from Loop, by hand: the slot playing up to slot 1) */
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    } else if (k == K_SLOT) {
        ui.slot = (uint8_t)v;
        return;                                  /* (only where the page looks: nothing saved) */
    } else if (k == K_CQ || k == K_CX) {
        slot_write(ui.slot);
        knob_set((k == K_CQ ? P_CQ0 : P_CX0) + ui.slot, v);
        if (k == K_CQ && !v) {                   /* Empty: a gap (the chord before it held) */
            knob_set(P_CR0 + ui.slot, 0);
            knob_set(P_CX0 + ui.slot, 0);
        }
        return;
    } else if (k == K_LANE) {
        ui.lane = (uint8_t)v;
        return;
    } else if (k == K_LCH) {
        knob_set(P_SCH0 + lane_seq(ui.lane), v);
        return;
    } else if (k >= K_FILTER && k <= K_SPACE) {
        ui.mac[k - K_FILTER] = (int8_t)v;
        en_macro(k - K_FILTER, v);
        return;                                  /* (for playing: nothing saved) */
    } else if (k == K_MODE) {                    /* (the knob: the place in MODE_ORDER) */
        set_mode(MODE_ORDER[v]);
        return;
    } else if (k == K_PLAY) {                    /* (the walk's character kept) */
        knob_set(P_PMODE, (proj.par[P_PMODE] & 3) | (v ? EN_PLOOP : 0));
        return;
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    if (k == K_SAVE) {                           /* a turn, either way: saved */
        do_save();
        return;
    }
    knob_range(k, &lo, &hi);
    if (lo < 0 && hi > 0 && hi - lo >= 10) {     /* (1.3.0) a centred knob: a notch at 0, three clicks to leave it */
#define NOTCH 2                                  /* (the clicks out of 0 held back) */
        static int8_t held[16];
        static uint32_t held_t[16];
        int v = knob_get(k), n, r = role & 15;
        if (v == 0) {                            /* (clicks out of 0 held back, NOTCH of them, one way, close together) */
            int32_t a = e > 0 ? e : -e, dir = e > 0 ? 1 : -1;
            if (plat_ms() - held_t[r] > 700u || (held[r] > 0) != (dir > 0))
                held[r] = 0;
            held_t[r] = plat_ms();
            while (a > 0 && (held[r] > 0 ? held[r] : -held[r]) < NOTCH)
                held[r] = (int8_t)(held[r] + dir), a--;
            if (!a)
                return;
            e = dir * a;
            held[r] = 0;
        }
        n = v + accel(role, e, hi - lo);
        knob_set(k, (v > 0 && n < 0) || (v < 0 && n > 0) ? 0 : n);   /* (and it stops at 0 on the way through) */
        return;
    }
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - proj.par[P_ROOT] + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        warn("THE ROOT STAYS", "(HOME, KEY)");
        return;
    }
    if (pool_count(p) < 3) {
        warn("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    proj.pool = EN_MODE_POOL[m];
    en_set(P_MODE, m);
    en_pool(proj.pool);
    say("MODE", EN_MODE_NAME[m]);
    mark_dirty();
}

/* the voices: ALGORITHM on a layer's page steps through that layer's (round: after the last, the first) */
/* (BERLIN) the page's brightness knob (the filter motion's): its knob index, its layer, its units an octave; -1: none */
static int bright_of(int v, int *layer, float *per_oct)
{
    switch (v) {
    case V_CSND: *layer = 0, *per_oct = 25.0f; return 0;   /* the Drone's Colour (2 octaves a 50) */
    case V_PSND: *layer = 1, *per_oct = 18.18f; return 0;  /* the Sequence's Cutoff (0.055 an octave a unit) */
    case V_BSND: *layer = 2, *per_oct = 25.0f; return 0;   /* the Lead's Tone */
    case V_DSND: *layer = 3, *per_oct = 18.18f; return 0;  /* Sequence 2's Cutoff */
    case V_NSND2: *layer = 4, *per_oct = 50.0f; return 0;  /* the Atmosphere's Colour */
    default: return -1;
    }
}
static int knob_dot(int v)                       /* the knob's value where its filter is now (-999: no such knob) */
{
    int layer, p, lo, hi;
    float per;
    const int k = bright_of(v, &layer, &per);
    if (k < 0)
        return -999;
    p = VIEW_KNOB[v][k];
    lo = en_param_info(p)->lo, hi = en_param_info(p)->hi;
    {
        const float x = (float)proj.par[p] + en_fo[layer] * per;
        return x < (float)lo ? lo : x > (float)hi ? hi : (int)(x + (x >= 0.0f ? 0.5f : -0.5f));
    }
}

static int view_voice(int v)
{
    v = page_of(v);
    return v == V_CHORD ? P_CVOICE : v == V_TEXTURE ? P_SVOICE : v == V_BASS ? P_LVOICE : v == V_DRUMS ? P_S2VOICE : -1;
}
static void set_voice(int p, int v)
{
    const en_param_t *pi = en_param_info(p);
    char t[12];
    int n = pi->hi - pi->lo + 1;
    v = ((v - pi->lo) % n + n) % n + pi->lo;
    proj.par[p] = (int16_t)v;
    en_set(p, v);
    en_param_text(p, v, t);
    say("VOICE", t);
    mark_dirty();
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    project_apply();
    oct_home_take();
    en_seed(plat_ms());
    en_regrow();
    say_as("PIECE", piece_name(proj.piece), 0);
    sp_piece();                                  /* a screen reader: the piece with its key and tempo */
    mark_dirty();
}

/* ----------------------------------------------------------- PERFORM --- */
/* SEL's mode, on the keys from F3 up, in three sets, a dark key between:
 *   white 1-5    the layers, a press each: muted and back, their levels kept (chord, pings, bass, kick, hats)
 *   black 1-5    the same layers soloed, a press each (more than one: they play together; the rest muted)
 *   white 7-11   dub moves, held as long as the key is: every stab thrown into the echo, the filter pushed
 *                open, pushed shut, the floor dropped (the kick and the bass out), the echo pushed (all in, feeding back)
 * Each key that does something is lit: a layer while it sounds, a solo (blinking while on), a move (dark
 * while held). The rest do nothing; MIDI notes in still change the scale */
#define PERF_N 15
static const uint8_t PERF_KEY[PERF_N] = {0, 2, 4, 6, 7,          /* white 1-5: the layers */
                                         11, 12, 14, 16, 18,     /* white 7-11: the moves */
                                         1, 3, 5, 8, 10};        /* black 1-5: the solos */
static const char *const PERF_NAME[PERF_N] = {"DRONE", "SEQ", "LEAD", "SEQ 2", "ATMOS", "SWEEP", "OPEN", "SHUT",
                                              "DROP", "ECHO", "DRONE", "SEQ", "LEAD", "SEQ 2", "ATMOS"};
/* each layer's mute (EN_M_*), in the meters' order: Sequence 2 is two of the engine's bits (DUB-1's kick and hats) */
static const uint32_t LAYER_MUTE[NLAYER] = {EN_M_CHORD, EN_M_PINGS, EN_M_BASS, EN_M_KICK | EN_M_HATS, EN_M_NOISE};
#define ALL_MUTE (EN_M_CHORD | EN_M_PINGS | EN_M_BASS | EN_M_KICK | EN_M_HATS | EN_M_NOISE)
static void perf_mutes(void)                     /* to the engine: the mutes, and with a solo, all but it */
{
    en_mute(ui.mutes | (ui.solo ? ALL_MUTE & ~ui.solo : 0u));
}
static int perf_sounds(int i)                    /* layer i (0..4) heard: not muted, not soloed out */
{
    if (ui.solo && !(ui.solo & LAYER_MUTE[i]))
        return 0;
    return (ui.mutes & LAYER_MUTE[i]) != LAYER_MUTE[i];
}
static void set_view(int v);
#define PERF_SHIFT 9u                            /* white 6 (D4), held: the knobs the mix */
static void lock_toggle(void)                    /* SEL held: Lock, on or off: the phrases keep their kick, groove and plan,
                                                  * Evolve's own moves rest (not saved; off at boot) */
{
    ui.lock = (uint8_t)!ui.lock;
    en_lock(ui.lock);
    say("LOCK", ui.lock ? "ON" : "OFF");
    card_show("LOCK", ui.lock ? "the sequences kept" : "the sequences free", K_ACC, 0);
}
static void perf_key(uint32_t key, int down)
{
    int i;
    if (key == PERF_SHIFT) {
        set_view(down ? V_MIX : V_PERF);
        if (down)
            say("MIX", 0);
        return;
    }
    for (i = 0; i < PERF_N && PERF_KEY[i] != key; i++)
        ;
    if (i >= PERF_N)
        return;
    if (i >= 10) {                               /* a solo: on or off, a press each */
        uint32_t m = LAYER_MUTE[i - 10];
        if (!down)
            return;
        ui.solo ^= m;
        perf_mutes();
        if (ui.view == V_MIX) {                  /* (the mix's knob lit, as for a mute) */
            ui.touched = (int8_t)(i - 10 < 3 ? i - 10 : 3);
            ui.touch_until = plat_ms() + 1200u;
        }
        if (ui.solo & m)
            say("SOLO", PERF_NAME[i]);
        else if (ui.solo)                        /* (one let go, others still soloed) */
            say(PERF_NAME[i], "SOLO OFF");
        else
            say("SOLO", "OFF");
        return;
    }
    if (i < 5) {                                 /* the chord, pings, bass, kick, hats: a mute (the levels kept) */
        if (!down)
            return;
        if ((ui.mutes & LAYER_MUTE[i]) == LAYER_MUTE[i])   /* (Sequence 2: both its bits out, back) */
            ui.mutes &= ~LAYER_MUTE[i];
        else
            ui.mutes |= LAYER_MUTE[i];
        perf_mutes();
        if (ui.view == V_MIX) {                  /* the layer's Mix knob lit (the kick and the hats: Drums) */
            ui.touched = (int8_t)(i < 3 ? i : 3);
            ui.touch_until = plat_ms() + 1200u;
        }
        say(PERF_NAME[i], (ui.mutes & LAYER_MUTE[i]) == LAYER_MUTE[i] ? "OFF" : "ON");
        return;
    }
    if (down)
        ui.moves |= 1u << (i - 5);
    else
        ui.moves &= ~(1u << (i - 5));
    en_perf(ui.moves);
    if (down)
        say(PERF_NAME[i], 0);
}

static void set_view(int v);
static void perf_set(int on)                     /* on: the mixer on the knobs; off: back to the page */
{
    ui.perf = (uint8_t)on;
    if (on) {
        if (ui.view != V_MIX && ui.view != V_PERF)
            ui.perf_from = ui.view;
        set_view(V_PERF);
    } else {
        if (ui.moves) {                          /* the moves let go; the mutes stay */
            ui.moves = 0;
            en_perf(0);
        }
        if (ui.view == V_MIX || ui.view == V_PERF)
            set_view(ui.perf_from);
    }
    say("PERFORM", on ? "ON" : "OFF");
}

/* ------------------------------------------------------- the effects grid --- */
/* REC held: the white keys from F3 up are the sixteen effects (four groups of four: the echo, the filter,
 * the space, rhythm and noise), each as long as its key is held, even once REC is let go. A tap on REC alone
 * is still dub hold (REC's release does nothing once a key played) */
static const uint8_t FX_KEY[EN_NFX] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
#define GRID_HOLD 300u                   /* ms of REC held before the grid shows */
static int grid_up(void) { return ui.fx || ((ui.btn >> B_REC & 1u) && plat_ms() - ui.down_t[B_REC] >= GRID_HOLD); }
static int fx_key(uint32_t key, int down)        /* 1: the key was an effect's */
{
    int e;
    for (e = 0; e < EN_NFX && FX_KEY[e] != key; e++)
        ;
    if (e >= EN_NFX)
        return 0;
    if (down) {
        ui.fx |= 1u << e;
        ui.btn_used |= 1u << B_REC;              /* (REC's release: no dub hold now) */
        say(EN_FX_NAME[e], 0);
    } else {
        ui.fx &= ~(1u << e);
    }
    en_fx(ui.fx | ui.bfx);
    return 1;
}

/* ----------------------------------------------------------- the chords --- */
/* EDIT tapped: the Chords page, the eight coming: the walk's (teal: degrees, they follow the key), yours (white)
 * and gaps (the chord before held). KNOB 1 the slot, the keys its root (a walk's chord becomes yours, its kind and
 * type kept; a gap becomes a minor chord), KNOB 2 its kind (Empty: a gap), KNOB 3 its type, KNOB 4 Play:
 * Endless (slot 1 playing; on, all move up and the walk writes slot 8) or Loop (the slots in turn, to the last
 * that isn't a gap). The engine holds them (it writes the walk's): prog_take mirrors them into the project.
 * EDIT again, or any page's button: back. EDIT held: the next chord */
#define NEXT_HOLD 500u
static int prog_count(void)                      /* how far the slots run (Loop): to the last that isn't a gap */
{
    int i, n = 0;
    for (i = 0; i < EN_NPROG; i++)
        if (!slot_is_gap(i))
            n = i + 1;
    return n;
}
static const char *slot_label(int i, char *nm)   /* written: its name; the walk's: the chord the scale gives it */
{
    if (slot_is_walk(i)) {
        int pcs[EN_NPAD + 1], iv[4], t[4], d = (proj.par[P_CR0 + i] - 1) % scale_steps(), k;
        en_chord_tones(proj.pool, proj.par[P_ROOT], d, t);
        en_voicing(proj.pool, d, proj.par[P_CX0 + i] & 1, iv);
        pcs[0] = t[0];
        for (k = 0; k < EN_NPAD; k++)
            pcs[k + 1] = (t[0] + iv[k]) % 12;
        return en_chord_name(pcs, EN_NPAD + 1, nm);
    }
    return en_chord_label(proj.par[P_CR0 + i], proj.par[P_CQ0 + i], proj.par[P_CX0 + i], nm);
}
static void prog_take(void)                      /* the engine's slots (the walk writes them, Endless moves them) */
{
    int i;
    if (en_prog_gen == ui.prog_gen)
        return;
    ui.prog_gen = en_prog_gen;
    for (i = 0; i < EN_NPROG; i++) {
        proj.par[P_CR0 + i] = en_prog_slot[i][0];
        proj.par[P_CQ0 + i] = en_prog_slot[i][1];
        proj.par[P_CX0 + i] = en_prog_slot[i][2];
    }
}
static void set_view(int v);
static void slot_key(uint32_t key)               /* the Chords page: a key, the slot's root */
{
    char nm[16];
    slot_write(ui.slot);                         /* (the walk's: its kind and type kept, the root yours) */
    knob_set(P_CR0 + ui.slot, (int)((5u + key) % 12u));
    if (!proj.par[P_CQ0 + ui.slot])
        knob_set(K_CQ, EN_CQ_MINOR);
    say(slot_label(ui.slot, nm), 0);
}
static uint8_t edit_new;                         /* EDIT held on: the new progression made this hold */
static void edit_released(uint32_t held)
{
    if (edit_new) {                              /* (held on: done already) */
        edit_new = 0;
    } else if (held >= NEXT_HOLD) {              /* held: the next chord */
        en_next();
        say("NEXT CHORD", en_playing ? "ON THE BAR" : 0);
        card_show("NEXT CHORD", en_playing ? "on the bar" : "", K_PULSE, 0);
    } else if (ui.view == V_CHORDS) {            /* tapped: the Chords page (one page: again stays) */
        ;
    } else {
        if (ui.perf) {                           /* (a page's button leaves PERFORM: the keys the scale again) */
            perf_set(0);
            set_view(V_DUB);
        }
        ui.chords_from = (uint8_t)ui.view;
        set_view(V_CHORDS);
    }
}

/* ---------------------------------------------------------- the pattern --- */
/* the Drums button a third time: the Pattern page. KNOB 1 the lane (kick, shaker, open hat, rim), the white keys
 * its sixteen steps (a key, a step: on or off), a black key held with a white one: that step accented; KNOB 2
 * the accents' strength, KNOB 3 the lane's chance, KNOB 4 Play: Auto (generated, as ever) or Pattern. The first
 * edit takes the pattern over from what Auto plays. The arrangement still takes lanes out (a Break, a fill) */
static const uint8_t STEP_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* the white keys */
/* (BERLIN, spec 13) the black keys in normal playing: an effect each while held, eight of the grid's and three moves
 * (EN_MV_*, coded 100 + the move) */
#define NBLACK 11
static const uint8_t BLACK_KEY[NBLACK] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static const uint8_t BLACK_DO[NBLACK] = {EN_FX_TAKEOFF, EN_FX_SUPERNOVA, EN_FX_TWITTER, EN_FX_REPEAT, EN_FX_SWELL,
                                         EN_FX_FREEZE, EN_FX_OCTAVE, EN_FX_RETURN, 100 + EN_MV_RISE, 100 + EN_MV_CLOSE,
                                         100 + EN_MV_TRANSPOSE};
static const char *const BLACK_NAME[NBLACK] = {"Take-off", "Supernova", "Twitter", "Repeat", "Swell", "Freeze",
                                               "Octave", "Return", "Rise", "Close", "Transpose"};
static int black_of(uint32_t key)                /* the black key's index (0..10), or -1 */
{
    int b;
    for (b = 0; b < NBLACK && BLACK_KEY[b] != key; b++)
        ;
    return b < NBLACK ? b : -1;
}
static void black_set(int b, int on)             /* a black key's effect on or off, to the engine */
{
    const int d = BLACK_DO[b];
    if (d >= 100)
        ui.mv = (uint8_t)(on ? ui.mv | (d - 100) : ui.mv & ~(d - 100));
    else
        ui.bfx = on ? ui.bfx | 1u << d : ui.bfx & ~(1u << d);
    en_fx(ui.fx | ui.bfx);
    en_move(ui.mv);
}
static uint32_t lane_steps(int d) { return proj.par[P_SMODE] ? (uint32_t)(uint16_t)proj.par[P_SGATE0 + d] : en_seq_lane[d]; }
static void pattern_key(uint32_t key, uint32_t keys)
{
    int st, d = ui.lane, acc;
    uint32_t m, black = 0;
    char t[24];
    for (st = 0; st < 16 && STEP_KEY[st] != key; st++)
        ;
    if (st >= 16)
        return;                                  /* (a black key: an accent's, with a white one) */
    for (m = 0; m < 16; m++)
        keys &= ~(1u << STEP_KEY[m]);
    black = keys;
    if (!proj.par[P_SMODE]) {                    /* the first edit: Auto's pattern, taken over */
        int k;
        for (k = 0; k < EN_NDRUM; k++)
            knob_set(P_SGATE0 + k, (int)(int16_t)(uint16_t)en_seq_lane[k]);
        knob_set(P_SACC0, (int)(int16_t)en_seq_acc[0]);
        knob_set(P_SACC1, (int)(int16_t)en_seq_acc[1]);
        knob_set(P_SMODE, 1);
    }
    m = (uint32_t)(uint16_t)proj.par[P_SGATE0 + d];
    acc = (int)((uint32_t)(uint16_t)proj.par[P_SACC0 + lane_seq(d)] >> st & 1u);
    if (black) {                                 /* accent it (on, too), or not: the lane's sequence's accents */
        knob_set(P_SACC0 + lane_seq(d), (int)(int16_t)(uint16_t)((uint32_t)(uint16_t)proj.par[P_SACC0 + lane_seq(d)] ^ (1u << st)));
        knob_set(P_SGATE0 + d, (int)(int16_t)(uint16_t)(m | 1u << st));
        acc = !acc;
    } else {
        knob_set(P_SGATE0 + d, (int)(int16_t)(uint16_t)(m ^ 1u << st));
    }
    t[0] = 0;
    itoa_u((uint32_t)st + 1u, t);
    say_as(LANE_NAME[d], 0, 0);
    if (ann_on()) {
        sp_n = 0;
        sp_buf[0] = 0;
        sp_cat("Step ");
        sp_cat(t);
        sp_cat(black ? (acc ? " accented" : " not accented") : (proj.par[P_SGATE0 + d] >> st & 1 ? " on" : " off"));
        ann_say(sp_buf, 0, ANN_NOW);
        sp_said = 1;
    }
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    int v;
    if (ui.perf && (b == B_HOME || b == B_ENV || b == B_ARP || b == B_LFO || b == B_SEQ || b == B_FX || b == B_GLO ||
                    b == B_SAVE || b == B_EDIT)) {   /* (1.3.0) a page's button leaves PERFORM: the keys the scale
                                                      * again, the page its first (SEL brings PERFORM back) */
        perf_set(0);
        set_view(V_DUB);
        if (b == B_HOME)
            return;
    }
    if (sound_of(ui.view) >= 0 && b == VIEW_BTN[ui.view]) {   /* the layer's button again: its sound page */
        set_view(sound_of(ui.view));
        return;
    }
    if (parent_of(ui.view) >= 0 && b == VIEW_BTN[parent_of(ui.view)]) {   /* again: the layer's next page; the last,
                                                                           * back to its first */
        set_view(next_page(ui.view));
        return;
    }
    if (b == B_FX && (ui.view == V_SPACE || ui.view == V_ESND)) {   /* FX again: the Echo page; again: back */
        set_view(ui.view == V_SPACE ? V_ESND : V_SPACE);
        return;
    }
    if (b == B_GLO && (ui.view == V_SETUP || ui.view == V_ARC)) {   /* GLO again: the Arc (how the piece builds);
                                                                       * again: back */
        set_view(ui.view == V_SETUP ? V_ARC : V_SETUP);
        return;
    }
    if (b == B_HOME && ui.view == V_DUB) {       /* HOME again: the Key page (the key and the tempo); again: back */
        set_view(V_KEY);
        return;
    }
    for (v = 0; v < NPAGES; v++)                 /* a page's button: its page */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_SEL:                                  /* PERFORM: one page, so again stays (1.3.0: a page's button
                                                  * leaves it) */
        if (!ui.perf)
            perf_set(1);
        else if (ui.view == V_MIX)
            set_view(V_PERF);
        break;
    case B_EDIT:                                 /* (its release: edit_released, in input) */
        break;
    case B_PLAY:
        sp_tp.playing = !en_playing;
        sp_tp.play_t = plat_ms();
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("PLAYING", 0);
        }
        break;
    case B_REC:
        if (plat_ms() - ui.down_t[B_REC] >= GRID_HOLD)
            break;                               /* held long enough for the grid: only a tap is dub hold */
        say("HOLD", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_SAVE:                                 /* the Noise page (one page: again stays; saving: GLO held, Setup's
                                                  * KNOB 4) */
        if (atm_view(ui.view)) {                 /* (SAVE again steps through: its sound, its Move page, back) */
            set_view(ui.view == V_NSND ? V_NSND2 : ui.view == V_NSND2 ? V_AMOVE : V_NSND);
        } else {
            ui.noise_from = (uint8_t)ui.view;
            set_view(V_NSND);
        }
        break;
    default: break;
    }
}

/* ---------------------------------------------------------- OCT- / OCT+ --- */
/* (the user, 2026-10-28) on the Sequence's, the Lead's and Sequence 2's pages (oct_layer): its octave, a step a tap
 * (on its release; none held on, none when both are down: so the update mode, both held 5 s, main_fm1.c, moves
 * nothing), said; lit while that octave is above (OCT+) or below (OCT-) its home, where the piece was loaded (oct_home).
 * The Drone's and the Atmosphere's pages, and pages of no layer: nothing. (The levels are SELECT's now, with the ring;
 * the mutes PERFORM's) */
#define OCT_SHOW 1200u                           /* the ring up after the last turn, then OCT_FADE fading */
#define OCT_FADE 250u
#define NO_OCT 0xFFu
static const uint8_t LAYER_LEVEL[NLAYER] = {P_CHORD, P_SEQ, P_LEAD, P_SEQ2, P_CRACKLE};
static const uint8_t LAYER_OCT[NLAYER] = {NO_OCT, P_SEQOCT, P_LOCT, P_SEQ2OCT, NO_OCT};
static const char *const LAYER_NAME[NLAYER] = {"DRONE", "SEQ", "LEAD", "SEQ 2", "ATMOS"};
static const char *const OCT_NAME[NLAYER] = {"", "SEQ OCTAVE", "LEAD OCTAVE", "SEQ 2 OCTAVE", ""};
static void oct_home_take(void)                  /* a piece loaded (or reset): its octaves, the lights' home */
{
    int l;
    for (l = 0; l < NLAYER; l++)
        if (LAYER_OCT[l] != NO_OCT)
            ui.oct_home[l] = (int8_t)proj.par[LAYER_OCT[l]];
}
static void oct_show(int l)                      /* the ring up (from where it was, if it was: it glides) */
{
    uint32_t now = plat_ms();
    if (ui.oct_ring != l + 1 || now >= ui.oct_until + OCT_FADE)
        ui.oct_v = (float)proj.par[LAYER_LEVEL[l]] * 0.01f;
    ui.oct_ring = (uint8_t)(l + 1);
    ui.oct_until = now + OCT_SHOW;
    ui.oct_pulse = now;
}
static int oct_ring_on(uint32_t now)             /* the ring's layer + 1, while it shows on this page (else 0) */
{
    if (!ui.oct_ring || now >= ui.oct_until + OCT_FADE || oct_layer(ui.view) != ui.oct_ring - 1)
        return 0;
    return ui.oct_ring;
}
static void level_turn(int l, int32_t e)         /* SELECT on a layer's page: its level, 1 a detent */
{
    char t[8];
    knob_set(LAYER_LEVEL[l], proj.par[LAYER_LEVEL[l]] + (int)e);
    itoa_u((uint32_t)proj.par[LAYER_LEVEL[l]], t);
    say(LAYER_NAME[l], t);
    oct_show(l);
}
static void oct_step(int b)
{
    int l = oct_layer(ui.view), k;
    char t[12];
    if (l < 0 || LAYER_OCT[l] == NO_OCT)
        return;
    k = LAYER_OCT[l];
    knob_set(k, knob_get(k) + (b == B_OCTUP ? 1 : -1));
    en_param_text(k, knob_get(k), t);
    say(OCT_NAME[l], t + (t[0] == '+'));         /* (as the knob shows it: "1", not "+1") */
    if (VIEW_KNOB[ui.view][1] == k) {            /* (the page's Octave knob, lit a moment) */
        ui.touched = 1;
        ui.touch_until = plat_ms() + 1200u;
    }
}
static void oct_press(int b)
{
    const int other = b == B_OCTUP ? B_OCTDN : B_OCTUP;
    if (ui.btn >> other & 1u)                    /* both down: neither steps */
        ui.btn_used |= 1u << B_OCTDN | 1u << B_OCTUP;
}
static void oct_release(int b)
{
    if (!(ui.btn_used >> b & 1u))
        oct_step(b);                             /* a tap */
}

/* MIDI in (USB and the TRS jack): channel 1 notes add or remove their note from the scale; clock and
 * start / stop drive the grid */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if (st == 0x90u && v)
            toggle_note((int)(n % 12u));
    }
}


#define RESET_HOLD 1500u                 /* SAVE held: the piece back to its recipe */

/* NUDGE: a layer's button held (ENV the Drone, ARP the Sequence, LFO the Lead, SEQ Sequence 2): its sound a small step
 * further, a few of its values each a few units, a walk pulled back to the piece's recipe (the further a value has
 * gone, the likelier it steps back), so nudges explore round the piece's sound and never wander off. The biggest
 * change said, and the layer's sound page shown, that knob lit. Evolve nudges by itself: now and then on a phrase's
 * first bar, one layer, a smaller step (nudge_tick) */
#define NUDGE_HOLD 500u
#define SHAKE_HOLD 1500u                  /* held on: a shake (a new voice, every value a big step) */
#define NNUDGE 6
static const uint8_t NUDGE_BTN[NNUDGE] = {B_ENV, B_ARP, B_LFO, B_SEQ, B_FX, B_SAVE};
static const char *const NUDGE_NAME[NNUDGE] = {"DRONE", "SEQ", "LEAD", "SEQ 2", "SPACE", "ATMOS"};
static const uint8_t NUDGE_PAR[NNUDGE][6] = {   /* (BERLIN) each layer its own sound, never its sends, octave or the
                                                  * piece's frame (the Lead's reverb and echo stay where set: the user) */
    {P_CATT, P_CCOL, P_CRES, P_DREL, P_PHASER, P_PEDAL},   /* the Drone */
    {P_CUTOFF, P_DECAY, P_SACCENT, P_CUTOFF, P_DECAY, P_SACCENT},   /* the Sequence */
    {P_LTONE, P_LATT, P_VIB, P_GLIDE, P_LTONE, P_VIB},   /* the Lead */
    {P_S2CUT, P_S2DEC, P_S2CUT, P_S2DEC, P_S2CUT, P_S2DEC},   /* Sequence 2 */
    {P_ECHO, P_FEEDBACK, P_REVERB, P_SIZE, P_CLOUD, P_ECHO},   /* (the space: FX; its shake, a new echo time) */
    {P_NCOL, P_NGRAIN, P_NTUNE, P_NROOM, P_NCOL, P_NGRAIN},   /* (the noise: SAVE; its shake, a new character) */
};
static uint32_t nudge_seed = 0x9E3779B9u;
static int nrand(int n)
{
    nudge_seed = nudge_seed * 1664525u + 1013904223u;
    return n > 0 ? (int)((nudge_seed >> 8) % (uint32_t)n) : 0;
}
enum { NUDGE_EVOLVE, NUDGE_HAND, NUDGE_SHAKE };
/* how a nudge goes: EVOLVE (Evolve's, 1-2 values, half steps, nothing said), HAND (the button held: 3-4 values, steps
 * up to an eighth of the range), SHAKE (held on: a new voice or kit, and every value a big step). Each value walks
 * from where it is, pulled back to the recipe: back as likely as it is far from it, always back from a quarter of
 * the range away */
static uint8_t nudge_quiet;                      /* (HOME held on: the shakes all at once, said once, the page kept) */
static void nudge(int layer, int how)
{
    static const uint8_t SND[NNUDGE] = {V_CSND, V_PSND, V_BSND, V_DSND, V_SPACE, V_NSND};
    static const uint8_t VOICE[NNUDGE] = {P_CVOICE, P_SVOICE, P_LVOICE, P_S2VOICE, P_ETIME, P_NCHAR};
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int j, best = -1, bd = 0, n, np = 0;
    uint8_t pick[8];
    char vn[24] = "";
    if (how == NUDGE_SHAKE && layer == 4) {      /* the space's: another echo time */
        int nt = en_param_info(P_ETIME)->hi + 1, cur = proj.par[P_ETIME], v = (cur + 1 + nrand(nt - 1)) % nt;
        knob_set(P_ETIME, v);
        en_param_text(P_ETIME, v, vn);
    } else if (how == NUDGE_SHAKE) {             /* a new voice (another than this), first: the values then fit it */
        const en_param_t *pi = en_param_info(VOICE[layer]);
        int nv = pi->hi - pi->lo + 1 - (layer == 0), cur = proj.par[VOICE[layer]], v;
        v = (cur + 1 + nrand(nv > 1 ? nv - 1 : 1)) % nv;   /* (the chord's: not Noise, the last, an effect more
                                                             * than a colour; ALGORITHM still has it) */
        if (v == cur)
            v = (v + 1) % nv;
        set_voice(VOICE[layer], v);
        en_param_text(VOICE[layer], proj.par[VOICE[layer]], vn);
    }
    en_piece(proj.piece, rec, &pool);
    for (j = 0; j < 6; j++) {                    /* (the values to choose from, each once) */
        int q, dup = 0;
        for (q = 0; q < np; q++)
            dup |= pick[q] == NUDGE_PAR[layer][j];
        if (!dup)
            pick[np++] = NUDGE_PAR[layer][j];
    }
    n = how == NUDGE_SHAKE ? np : how == NUDGE_HAND ? 3 + nrand(2) : 1 + nrand(2);
    for (j = 0; j < n && np; j++) {
        int w = nrand(np), p = pick[w], lo = en_param_info(p)->lo, hi = en_param_info(p)->hi;
        int st = (hi - lo) / (p == P_DECAY || p == P_CUTOFF || p == P_FEEDBACK ? 16 : 8), v = proj.par[p], d;
        pick[w] = pick[--np];                    /* (not twice in one nudge) */
        if (how == NUDGE_EVOLVE)
            st = (st + 1) / 2;
        else if (how == NUDGE_SHAKE)
            st = st * 3 / 2;
        if (st < 1)
            st = 1;
        d = 1 + nrand(st);                       /* a step of 1..st; back towards the recipe as likely as it is far
                                                  * from it (a quarter of the range away: always back), else either way */
        if (v != rec[p] && nrand(hi - lo) < 4 * (v > rec[p] ? v - rec[p] : rec[p] - v))
            d = v > rec[p] ? -d : d;
        else if (nrand(2))
            d = -d;
        v += d;
        v = v < lo ? lo : v > hi ? hi : v;
        d = v - proj.par[p];
        if (!d)
            continue;
        knob_set(p, v);
        if (d * d > bd * bd)
            best = p, bd = d;
    }
    if (how == NUDGE_EVOLVE)
        return;
    {   /* said: "CHORD NUDGE  Colour +9", "CHORD SHAKE  Keys" */
        char t[24], head[16], v[8];
        const char *nm = how == NUDGE_SHAKE ? vn : best >= 0 ? en_param_info(best)->name : "";
        int k = 0, i;
        while (*nm && k < (how == NUDGE_SHAKE ? 23 : 14))
            t[k++] = *nm++;
        if (how == NUDGE_HAND && best >= 0) {
            t[k++] = ' ';
            t[k++] = bd > 0 ? '+' : '-';
            itoa_u((uint32_t)(bd > 0 ? bd : -bd), v);
            for (i = 0; v[i] && k < 23; i++)
                t[k++] = v[i];
        }
        t[k] = 0;
        for (i = 0; NUDGE_NAME[layer][i]; i++)
            head[i] = NUDGE_NAME[layer][i];
        for (k = 0; (how == NUDGE_SHAKE ? " SHAKE" : " NUDGE")[k] && i < 15; k++)
            head[i++] = (how == NUDGE_SHAKE ? " SHAKE" : " NUDGE")[k];
        head[i] = 0;
        if (!nudge_quiet)
            say(head, t);
        if (how != NUDGE_EVOLVE && !nudge_quiet) /* (yours, not Evolve's: a card in the middle too) */
            card_show(head, t, how == NUDGE_SHAKE ? K_FROST : K_PULSE, how == NUDGE_SHAKE);
    }
    if (!ui.perf && !nudge_quiet) {
        if (SND[layer] == V_NSND && ui.view != V_NSND) {
            ui.noise_from = (uint8_t)ui.view;   /* (SAVE again goes back there) */
        }
        set_view(SND[layer]);
        for (j = 0; j < 4; j++)
            if (VIEW_KNOB[ui.view][j] == best && how == NUDGE_HAND) {
                ui.touched = (int8_t)j;
                ui.touch_until = plat_ms() + 1500u;
            }
    }
}
static void nudge_tick(void)                     /* Evolve's nudges: on a phrase's first bar, now and then, one layer */
{
    static uint8_t last_pb = 0xFF;
    uint8_t pb = en_phrase_bar;
    int ev = proj.par[P_EVOLVE];
    if (pb == last_pb)
        return;
    last_pb = pb;
    if (pb != 0 || !en_playing || en_frozen || ev <= 40 || ui.lock)   /* (Lock: they rest) */
        return;
    if (nrand(100) < (ev - 40) * 2 / 3)          /* (Evolve 100: 40% of the phrases; 70: 20%; 40 and under: none) */
        nudge(nrand(NNUDGE), NUDGE_EVOLVE);
}
static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_GLO)
                ui.reset_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
            if (i == B_OCTDN || i == B_OCTUP)
                oct_press((int)i);
        } else if (i == B_OCTDN || i == B_OCTUP) {
            oct_release((int)i);
        } else if (i == B_EDIT) {               /* EDIT: the Chords page (a tap), the next chord (held) */
            edit_released(plat_ms() - ui.down_t[B_EDIT]);
        } else if (i == B_GLO && !(ui.btn_used & m) && plat_ms() - ui.down_t[B_GLO] >= NUDGE_HOLD) {
            do_save();                           /* GLO held and let go: saved (held on: the reset, below, unsaved) */
            if (!ui.dirty)
                card_show("SAVED", "all eight pieces", K_ACC, 0);
        } else if (i == B_SEL && !(ui.btn_used & m) && plat_ms() - ui.down_t[B_SEL] >= NUDGE_HOLD) {
            lock_toggle();                       /* (SEL held: Lock; a tap, PERFORM) */
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    for (i = 0; i < NNUDGE; i++) {               /* a layer's button held: a nudge; held on: a shake (its release
                                                  * then does nothing) */
        static uint8_t shaken;
        uint32_t m = 1u << NUDGE_BTN[i], held = plat_ms() - ui.down_t[NUDGE_BTN[i]];
        if (!(btn & m)) {
            shaken &= (uint8_t)~(1u << i);
        } else if (!(ui.btn_used & m) && held >= NUDGE_HOLD) {
            ui.btn_used |= m;
            nudge((int)i, NUDGE_HAND);
        } else if ((ui.btn_used & m) && held >= SHAKE_HOLD && !(shaken >> i & 1u) &&
                   (int32_t)(ui.turn_t - ui.down_t[NUDGE_BTN[i]]) <= 0) {
            shaken |= (uint8_t)(1u << i);
            nudge((int)i, NUDGE_SHAKE);
        }
    }
    {   /* (1.3.0) HOME held on (1.5 s, nothing turned meanwhile): a shake of everything that shakes,
         * the chord, the pings, the bass, the drums and the noise, at once; again and again, till something sounds */
        static uint8_t home_shaken;
        const uint32_t m = 1u << B_HOME;
        if (!(btn & m)) {
            home_shaken = 0;
        } else if (!home_shaken && !(ui.btn_used & m) && plat_ms() - ui.down_t[B_HOME] >= SHAKE_HOLD &&
                   (int32_t)(ui.turn_t - ui.down_t[B_HOME]) <= 0) {
            static const uint8_t ALL[5] = {0, 1, 2, 3, 5};
            int j;
            home_shaken = 1;
            ui.btn_used |= m;
            nudge_quiet = 1;
            for (j = 0; j < 5; j++)
                nudge(ALL[j], NUDGE_SHAKE);
            nudge_quiet = 0;
            say("SHAKE", "EVERYTHING");
            card_show("SHAKE ALL", "every layer, and the space", K_FROST, 1);
        }
    }
    if ((btn >> B_EDIT & 1u) && !edit_new && plat_ms() - ui.down_t[B_EDIT] >= SHAKE_HOLD &&
        (int32_t)(ui.turn_t - ui.down_t[B_EDIT]) <= 0) {   /* EDIT held on: a new progression, the walk's chords
                                                  * all walked again (yours, the gaps and the one playing kept) */
        edit_new = 1;
        en_prog_rewalk();
        say("NEW CHORDS", EN_WALK_NAME[(proj.par[P_PMODE] & 3) % EN_NWALK]);
        card_show("NEW CHORDS", EN_WALK_NAME[(proj.par[P_PMODE] & 3) % EN_NWALK], K_FROST, 1);
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if (ch >> i & 1u) {
            int down = (int)(keys >> i & 1u);
            if (!down && black_of(i) >= 0)       /* (BERLIN) a black key let go: its effect ends, whatever took it */
                black_set(black_of(i), 0);
            if (!down) {                         /* (BERLIN) a white key let go: the Lead let go of it first, whatever
                                                  * takes the key now (REC, an effect, PERFORM, a page); the engine
                                                  * ignores a release that isn't its key */
                int st;
                for (st = 0; st < 16 && STEP_KEY[st] != i; st++)
                    ;
                if (st < 16)
                    en_lead_key(st, 0);
            }
            if (((btn >> B_REC & 1u) && down) || (!down && ui.fx))
                if (fx_key(i, down))
                    continue;                    /* REC held: the effects (and their keys let go) */
            if (btn >> B_REC & 1u)
                continue;                        /* (REC held: the other keys do nothing) */
            if (ui.view == V_PAT && !ui.perf) {  /* the Pattern page: the white keys, the lane's steps */
                if (down)
                    pattern_key(i, keys);
                continue;
            }
            if (ui.view == V_CHORDS && !ui.perf) {   /* the Chords page: the keys, the slot's root */
                if (down)
                    slot_key(i);
                continue;
            }
            if (ui.perf)
                perf_key(i, (int)(keys >> i & 1u));
            else {                               /* (BERLIN) the white keys: the Lead, in the scale; the black: nothing */
                int st;
                for (st = 0; st < 16 && STEP_KEY[st] != i; st++)
                    ;
                if (st < 16 && down)
                    en_lead_key(st, 1);          /* (its release sent above) */
                else if (down && black_of(i) >= 0) {   /* the black keys: their effects (spec 13) */
                    black_set(black_of(i), 1);
                    say(BLACK_NAME[black_of(i)], 0);
                }
            }
        }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* (the user, 2026-10-28) a layer's pages: its level; elsewhere
                                                  * nothing (no pages, no tempo: one knob, one job) */
        if (oct_layer(ui.view) >= 0)
            level_turn(oct_layer(ui.view), e);
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {         /* a layer's page: its voice; the Chords page: the walk */
        int vp = view_voice(ui.view);
        if (atm_view(ui.view)) {   /* the Noise pages: its character */
            int c = ((proj.par[P_NCHAR] + (int)(e % EN_NNCHAR)) % EN_NNCHAR + EN_NNCHAR) % EN_NNCHAR;
            knob_set(P_NCHAR, c);
            say("NOISE", EN_NCHAR_NAME[c]);
        } else if (ui.view == V_CHORDS) {        /* the walk's character: its chords walked again, at once */
            int w = (((proj.par[P_PMODE] & 3) + (int)(e % EN_NWALK)) % EN_NWALK + EN_NWALK) % EN_NWALK;
            knob_set(P_PMODE, (proj.par[P_PMODE] & EN_PLOOP) | w);
            en_prog_rewalk();
            {   /* the character, and what it does */
                static const char *const WHAT[EN_NWALK] = {"Home: stays near home", "Wander: roams wider",
                                                         "Free: goes anywhere"};
                say("WALK", WHAT[w]);
            }
        } else if (vp >= 0) {
            set_voice(vp, proj.par[vp] + (int)e);   /* (every detent: a quick turn comes as several at once) */
        }                                        /* (elsewhere: nothing; the scope and the spectrum are gone) */
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (int)e);
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_GLO & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_GLO] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held on: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_GLO;
        project_piece_reset();
        project_apply();
        oct_home_take();
        en_regrow();
        say("RESET", piece_name(proj.piece));
        card_show("RESET", piece_name(proj.piece), K_FROST, 1);
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            if (grid_up())
                continue;                        /* (the grid on the screen: the knobs rest) */
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the main screen (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r (0..6) centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6)
        r = 6;
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc_th(int32_t cx, int32_t cy, int32_t r, float th, float v, uint16_t track, uint16_t fill, uint16_t bg,
                   int bipolar)
{
    const float a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    arc_th(cx, cy, r, 3.4f, v, track, fill, bg, bipolar);
}
/* a mark across a knob's ring at v (0..1 of its sweep), a little past both its edges: where a knob's behaviour changes
 * (Motion's regions, 40 and 70) */
static void arc_mark(int32_t cx, int32_t cy, int32_t r, float v, uint16_t col)
{
    const float a = (0.375f + 0.75f * v) * FM_TWO_PI, c = fm_cosf(a), sn = fm_sinf(a);
    float d;
    for (d = (float)r - 5.2f; d <= (float)r + 1.6f; d += 0.4f)
        px_blend(cx + (int32_t)(c * d + 0.5f), cy + (int32_t)(sn * d + 0.5f), col, 16u);
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void tag(int32_t *x, const char *s, uint16_t bg, uint16_t fg)
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(*x, 6, w, 16, 6, bg);
    cv_text(*x + 5, 7, &FONT_XS, s, fg);
    *x += w + 4;
}

/* the battery (dxsloop's thresholds on the divider's raw reading): three bars, 591 up, 561, 531; on USB
 * (powering and charging it: there is no charger line) the bars fill one, two, three in turn */
static int batt_bars(void)
{
    int32_t b = plat_battery();
    return b >= 591 ? 3 : b >= 561 ? 2 : b >= 531 ? 1 : 0;
}
static int batt_pct(void)                        /* a rough charge, 521 .. 601 */
{
    int32_t p = (plat_battery() - 521) * 100 / 80;
    return p < 0 ? 0 : p > 100 ? 100 : (int)p;
}
static void draw_battery(int32_t bx)
{
    int lvl = plat_usb_power() ? 1 + (int)((plat_ms() / 600u) % 3u) : batt_bars(), k;
    uint16_t c = !plat_usb_power() && lvl <= 1 ? K_FROST : K_DIM, o = !plat_usb_power() && !lvl ? K_FROST : K_DIM;
    cv_rect(bx, 9, 15, 1, o);                    /* (empty: the outline red) */
    cv_rect(bx, 17, 15, 1, o);
    cv_rect(bx, 9, 1, 9, o);
    cv_rect(bx + 14, 9, 1, 9, o);
    cv_rect(bx + 15, 11, 2, 5, o);
    for (k = 0; k < lvl; k++)
        cv_rect(bx + 2 + k * 4, 11, 3, 5, c);
}

/* the header: the piece in a tag, its root and mode; on the right, the tempo with a slow beat light
 * (a play triangle while resting) and a dot while there is something unsaved. Messages take its place
 * for a moment. */
static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)proj.pool << 12),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 8),
                      (uint32_t)proj.par[P_TEMPO] | (uint32_t)(ui.view == V_SETUP ? batt_pct() : 0) << 22 | (uint32_t)batt_bars() << 29 | (uint32_t)plat_usb_power() << 31 |
                      (plat_usb_power() ? (plat_ms() / 600u) % 3u << 7 : 0u) | (ui.view == V_SETUP ? plat_cpu_pct() << 8 | plat_cpu_peak() << 15 |
                                                                           (plat_xruns() & 0x1FFu) << 22 : 0u));
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (en_playing)
        h = hash(h, (en_bars / 4u) & 1u);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (plat_battery() >= 0 || plat_usb_power()) {   /* the battery, at the right edge */
            draw_battery(219);
            x = 213;
        }
        if (ui.view == V_SETUP && (plat_battery() >= 0 || plat_usb_power())) {   /* Setup: the charge, by the icon */
            char c[8];
            if (plat_usb_power()) {
                c[0] = 'U', c[1] = 'S', c[2] = 'B', c[3] = 0;
            } else {
                uint32_t k;
                itoa_u((uint32_t)batt_pct(), c);
                for (k = 0; c[k]; k++)
                    ;
                c[k++] = '%', c[k] = 0;
            }
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_TEXT);
            x -= 8;
        }
        if (ui.dirty) {                            /* unsaved */
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {
            t[0] = 'E';
            t[1] = 'X';
            t[2] = 'T';
            t[3] = 0;
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], t);
        }
        if (ui.view == V_SETUP) {                 /* Setup: the audio load (now/peak), and any dropouts ("!n") */
            char c[24] = "CPU ";
            uint32_t k;
            itoa_u(plat_cpu_pct(), c + 4);
            for (k = 0; c[k]; k++)
                ;
            c[k++] = '/';
            itoa_u(plat_cpu_peak(), c + k);
            if (plat_xruns()) {
                for (; c[k]; k++)
                    ;
                c[k++] = ' ', c[k++] = '!';
                itoa_u(plat_xruns() > 999u ? 999u : plat_xruns(), c + k);
            }
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);   /* (the leftmost on the right: its changing width moves nothing) */
        } else {                                 /* elsewhere: the tempo and the beat light */
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, en_playing ? K_TEXT : K_DIM);
        x -= 16;
        if (en_playing) {                         /* a slow beat light */
            int on = ((en_bars / 4u) & 1u) == 0u;
            cv_round(x, 9, 10, 10, 5, on ? K_PAD : K_PAD_T);
        } else {
            int32_t j;                            /* resting: a play triangle */
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        }
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;   /* the piece, in a coral tag */
        cv_round(4, 4, w, 20, 6, K_TEXT);
        cv_text(10, 6, &FONT_B, piece_name(proj.piece), K_BG);
        if (ui.view != V_SETUP) {                /* (Setup: the room is the CPU's; the root is its first knob) */
            w = cv_text(w + 10, 8, &FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT]], K_TEXT);
            cv_text(w + 3, 8, &FONT_XS, mode_name(), K_DIM);
        }
    }
    cv_blit(0, 0);
}

/* (BERLIN) the main screen, Cosmic: a slow starfield; the chord a glowing body in the middle; each sequence note a
 * particle orbiting it (the Sequence inner and pink, Sequence 2 outer and cyan), a repeat a smaller one, each with a
 * fading trail. Around it: the section and its bars (top left), the page (top right), the five layers' meters (left),
 * the echo's level (right), and the two sequences' lanes (below), each with its own step */
#define CX 120
#define CY 48
/* the echo's output, as measured: its peak each frame, the last few frames (a repeat's level is the peak around
 * when it's due) */
#define EPK_N 16
static struct {
    uint32_t t;
    float pk;
} epk[EPK_N];
static uint8_t epk_w;
static int scr_layer(int v)                      /* a layer's own page (and the Noise page): the screen that layer's */
{
    return v == V_CHORD || v == V_TEXTURE || v == V_BASS || v == V_DRUMS || atm_view(v) || (v >= V_CSND && v <= V_DSND) || v == V_CFM || v == V_BFM || v == V_SMOVE || v == V_S2MOVE
         ? oct_layer(v) : -1;                    /* (1.3.0: the sound pages too, live, not the envelope) */
}

static const char *chord_now(char *nm)          /* the chord's name, as the screen shows it ("Em7") */
{
    int pcs[EN_NPAD + 1], k;
    if (en_chord_slot >= 0 && proj.par[P_CQ0 + en_chord_slot])   /* a slot's: its own name */
        return slot_label(en_chord_slot, nm);
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    return en_chord_name(pcs, EN_NPAD + 1, nm);
}

/* the echo's level this frame (its peak since the last): the ECHO meter */
static void echo_take(uint32_t now)
{
    epk[epk_w].t = now;
    epk[epk_w].pk = en_echo_pk;
    en_echo_pk = 0.0f;
    epk_w = (uint8_t)((epk_w + 1u) % EPK_N);
}

static void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c);

/* (BERLIN) the stars: a sparse field, dim, drifting left a pixel every few frames while it plays */
#define STAR_N 36
static uint8_t star_x[STAR_N], star_y[STAR_N], star_b[STAR_N];
static uint8_t star_init, star_t;
static void draw_stars(void)
{
    int i;
    if (!star_init) {                            /* (the same sky every start) */
        uint32_t r = 0x2545F491u;
        for (i = 0; i < STAR_N; i++) {
            r = r * 1664525u + 1013904223u, star_x[i] = (uint8_t)(r >> 24);
            r = r * 1664525u + 1013904223u, star_y[i] = (uint8_t)((r >> 24) % 92u);
            r = r * 1664525u + 1013904223u, star_b[i] = (uint8_t)(2u + (r >> 24) % 5u);
        }
        star_init = 1;
    }
    if (en_playing && ++star_t >= 4u) {
        star_t = 0;
        for (i = 0; i < STAR_N; i++)
            star_x[i]--;                         /* (past the left edge: back in at the right, 16 px later) */
    }
    for (i = 0; i < STAR_N; i++)
        if (star_x[i] < 240)
            cv_rect(star_x[i], star_y[i], 1, 1, mix(K_BG, K_DIM, star_b[i]));
}

/* (BERLIN) the orbits: each note a particle round the chord, fading; its trail the last few places it was */
#define ORB_N 24
static struct {
    float a, life;
    uint8_t on, layer, rep;
} orb[ORB_N];
static uint32_t orb_hits[2], orb_reps[2];
static void orb_add(int layer, int rep)
{
    int i, k = 0;
    for (i = 0; i < ORB_N; i++) {
        if (!orb[i].on) {
            k = i;
            break;
        }
        if (orb[i].life < orb[k].life)
            k = i;
    }
    orb[k].on = 1;
    orb[k].layer = (uint8_t)layer;
    orb[k].rep = (uint8_t)rep;
    orb[k].life = rep ? 0.55f : 1.0f;
    orb[k].a = 0.39f * (float)(orb_hits[layer] + orb_reps[layer]) + (layer ? 3.0f : 0.0f);
}
static void orbits_take(void)                    /* the sequences' new notes and repeats, a particle each (3 at most) */
{
    int l, k;
    for (l = 0; l < 2; l++) {
        const uint32_t h = en_seq_hits[l], r = en_seq_reps[l];
        const int muted = (ui.mutes & LAYER_MUTE[l ? 3 : 1]) == LAYER_MUTE[l ? 3 : 1];
        for (k = 0; orb_hits[l] != h; k++)
            if (orb_hits[l]++, k < 3 && !muted)
                orb_add(l, 0);
        for (k = 0; orb_reps[l] != r; k++)
            if (orb_reps[l]++, k < 2 && !muted)
                orb_add(l, 1);
    }
}
static int draw_orbits(void)                     /* 1: one still moves */
{
    int i, k, any = 0;
    for (i = 0; i < ORB_N; i++) {
        const float rx = orb[i].layer ? 44.0f : 31.0f, ry = rx * 0.68f;
        const uint16_t c = en_frozen ? K_FROST : orb[i].layer ? K_PULSE : K_BELL;
        if (!orb[i].on)
            continue;
        for (k = 3; k >= 0; k--) {               /* the trail first, the particle on top */
            const float a = orb[i].a - 0.09f * (float)k, f = orb[i].life * (1.0f - 0.22f * (float)k);
            const int32_t x = CX + (int32_t)(rx * fm_cosf(a)), y = CY + (int32_t)(ry * fm_sinf(a)),
                          d = orb[i].rep || k ? 2 : 3;
            if (f > 0.03f)
                cv_round(x - d / 2, y - d / 2, d, d, 1, mix(K_BG, c, (int)(f * 16.0f)));
        }
        orb[i].a += orb[i].layer ? 0.045f : 0.06f;
        orb[i].life -= 0.016f;
        if (orb[i].life <= 0.0f)
            orb[i].on = 0;
        any = 1;
    }
    return any;
}

static void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* Bresenham */
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1,
            sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        int32_t e2 = 2 * e;                      /* (taken before either step: else a short slope can overshoot) */
        cv_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            e += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            e += dx;
            y0 += sy;
        }
    }
}

static void draw_meters(void)                    /* (BERLIN) left: the five layers, each its own activity at its level */
{
    static const uint16_t C[5] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_NOISE},
                          T[5] = {K_PAD_T, K_BELL_T, K_BASS_T, K_PULSE_T, K_PANEL};
    int i;
    for (i = 0; i < 5; i++) {
        int32_t x = 8 + i * 9, h = (int32_t)(fm_clampf(en_layer_act[i], 0.0f, 1.0f) * 52.0f + 0.5f);
        cv_round(x, 32, 6, 56, 2, T[i]);
        if (h > 0)
            cv_round(x, 32 + 56 - h - 2, 6, h + 2, 2, C[i]);
        {   /* the filter: a tick, 6 px an octave from the middle (up: brighter) */
            const int32_t o = (int32_t)(fm_clampf(en_fo[i], -4.5f, 4.5f) * 6.0f);
            cv_rect(x - 1, 59 - o, 8, 1, K_TEXT);
        }
    }
}

static void draw_duck(void)                      /* (BERLIN) right: the echo's level, rising from the bottom (eased: up
                                                  * at once, down over ~half a second) */
{
    static float lv;
    const float pk = epk[(epk_w + EPK_N - 1u) % EPK_N].pk;
    int32_t h;
    lv = fm_maxf(fm_clampf(pk * 3.0f, 0.0f, 1.0f), lv * 0.93f);
    h = (int32_t)(lv * 52.0f + 0.5f);
    cv_round(220, 32, 8, 56, 3, K_PULSE_T);
    if (h > 0)
        cv_round(220, 32 + 54 - h, 8, h + 2, 3, K_PULSE);
    cv_text(216 - text_w(&FONT_XS, "ECHO"), 76, &FONT_XS, "ECHO", K_DIM);
}

/* (BERLIN) below: the two sequences, a lane each, as long as it is, each with its own step (white): the steps that
 * play in its colour, the rests a dot; on a layer's page the other lane greyed */
static void draw_lanes(void)
{
    static const uint16_t C[2] = {K_BELL, K_PULSE}, T[2] = {K_BELL_T, K_PULSE_T};
    const int lay = oct_layer(ui.view);
    int l, i;
    for (l = 0; l < 2; l++) {
        const uint32_t p = lane_steps(l);
        const int len = proj.par[l ? P_SEQ2LEN : P_SEQLEN], pos = en_loop_pos[l],
                  lit = lay < 0 || lay == (l ? 3 : 1);
        const int32_t y = 108 + l * 16;
        for (i = 0; i < len && i < 16; i++) {
            const int32_t x = 32 + i * 12;
            if (en_playing && i == pos)
                cv_round(x - 3, y - 3, 7, 7, 3, lit ? K_TEXT : K_DIM);
            else if (p >> i & 1u)
                cv_round(x - 2, y - 2, 5, 5, 2, lit ? C[l] : T[l]);
            else
                cv_rect(x, y, 1, 1, lit ? K_LINE : K_PANEL);
        }
    }
}

static void grid_band(int32_t oy)                 /* the whole screen, in two bands of 240 x 120 */
{
    static const char *const GROUP[4] = {"ECHO", "FILTER", "SPACE", "MOVES"};   /* (BERLIN's) */
    static const char *const CELL[EN_NFX] = {"Repeat", "Time", "Stop", "Swell", "Low", "High", "Sweep", "Phaser",
                                             "Reverb", "Takeoff", "Freeze", "Wash", "Octave", "Return", "Twitter", "Nova"};
    int r, c;
    cv_begin(240, 120, K_BG);
    text_c(120, 2 - oy, &FONT_S, "EFFECTS", K_ACC);
    text_c(120, 20 - oy, &FONT_XS, "HOLD A WHITE KEY    TAP REC: HOLD", K_DIM);
    for (r = 0; r < 4; r++) {
        int32_t yr = 38 + r * 50;
        cv_text(6, yr - oy, &FONT_XS, GROUP[r], K_DIM);
        for (c = 0; c < 4; c++) {
            int e = r * 4 + c, on = (int)(ui.fx >> e & 1u);
            int32_t x = 4 + c * 59, y = yr + 13 - oy;
            char t[12];
            cv_round(x, y, 55, 33, 5, on ? K_ACC : c == 0 ? K_PANEL : mix(K_BG, K_PANEL, 8));
            if (c == 0 && !on)                   /* (the first of a group: its key is lit) */
                cv_rect(x + 4, y + 29, 47, 2, K_ACC);
            caps(t, CELL[e], sizeof t);
            text_c(x + 28, y + 8, &FONT_S, t, on ? K_BG : K_TEXT);
        }
    }
    cv_blit(0, (uint32_t)oy);
}

static void draw_grid(void)                      /* REC held: the effects, the whole screen */
{
    uint32_t h = hash(0x47524944u, ui.fx);
    if (h == ui.sig[1] && ui.sig[0] == 0x47524944u)
        return;
    ui.sig[0] = ui.sig[2] = 0x47524944u;         /* (the header and the knobs drawn again after) */
    ui.sig[1] = h;
    grid_band(0);
    grid_band(120);
}

/* the Pattern page: the four lanes, sixteen steps each, the lane edited lit, the accents taller, the step
 * playing a column */
static void draw_pattern(void)
{
    static const uint16_t C[EN_NDRUM] = {K_PULSE, K_TEXT, K_DIM, K_RIM, K_CLAP};
    static const char *const SH[EN_NDRUM] = {"SEQ", "SEQ2", "OCT", "OCT2", "SKIP"};   /* (BERLIN's lanes, LANE_NAME) */
    int l, i, pos = en_loop_pos[0];              /* (the Sequence's step, not the bar's sixteenth) */
    text_c(120, -1, &FONT_XS, proj.par[P_SMODE] ? "PLAY: THE PATTERN" : "PLAY: AUTO (A KEY TAKES IT OVER)",
           proj.par[P_SMODE] ? K_ACC : K_DIM);
    if (en_playing)
        cv_round(37 + pos * 12, 15, 11, 86, 2, K_PANEL);   /* (five lanes, 17 px apart) */
    for (l = 0; l < EN_NDRUM; l++) {
        uint32_t p = lane_steps(l), a = proj.par[P_SMODE] ? (uint32_t)(uint16_t)proj.par[P_SACC0 + lane_seq(l)] : 0u;
        int32_t y = 17 + l * 17, sel = l == ui.lane;
        int maybe = proj.par[P_SMODE] && proj.par[P_SCH0 + lane_seq(l)] < 50;   /* (a chance under half: rings) */
        if (sel)
            cv_round(2, y - 2, 236, 17, 4, mix(K_BG, K_PANEL, 10));
        cv_text(5, y + 1, &FONT_XS, SH[l], sel ? K_TEXT : K_DIM);
        for (i = 0; i < 16; i++) {
            int32_t x = 38 + i * 12;
            if (p >> i & 1u) {
                int ac = (int)(a >> i & 1u);
                uint16_t c = en_playing && i == pos ? K_TEXT : sel ? C[l] : mix(K_BG, C[l], 9);
                int32_t yy = ac ? y : y + 3, hh = ac ? 13 : 7;
                if (maybe) {                     /* a step that only sometimes plays: a ring */
                    cv_rect(x + 1, yy, 7, 1, c), cv_rect(x + 1, yy + hh - 1, 7, 1, c);
                    cv_rect(x, yy + 1, 1, hh - 2, c), cv_rect(x + 8, yy + 1, 1, hh - 2, c);
                } else {
                    cv_round(x, yy, 9, hh, 2, c);
                }
            } else {
                cv_rect(x + 4, y + 6, 2, 2, (i & 3) ? K_LINE : K_DIM);
            }
        }
    }
    text_c(120, 104, &FONT_XS, "WHITE: STEPS   BLACK + WHITE: ACCENT", K_DIM);
}

/* the Chords page: the eight slots, the one edited outlined, the one playing lit; an empty slot inside the
 * progression shows that the chord before it goes on */
static void draw_slots(void)
{
    static const char *const HEAD[4] = {"HOME: THE WALK WRITES SLOT 8", "WANDER: THE WALK WRITES SLOT 8",
                                        "FREE: THE WALK WRITES SLOT 8", "LOOP: THE SLOTS IN TURN"};
    int i, loop = (proj.par[P_PMODE] & EN_PLOOP) != 0, n = loop ? prog_count() : EN_NPROG;
    char nm[16];
    text_c(120, -1, &FONT_XS, HEAD[loop ? 3 : (proj.par[P_PMODE] & 3) % EN_NWALK], K_ACC);
    for (i = 0; i < EN_NPROG; i++) {
        int32_t x = 12 + (i & 3) * 55, y = 30 + (i >> 2) * 46;   /* (the two rows in the middle) */
        int filled = !slot_is_gap(i), now = en_playing && i == en_prog_pos && n, sel = i == ui.slot;
        if (sel)
            cv_round(x - 2, y - 2, 54, 38, 5, K_ACC);
        cv_round(x, y, 50, 34, 4, now ? K_PAD : filled ? K_PANEL : mix(K_BG, K_PANEL, 6));
        {
            char d[2];
            d[0] = (char)('1' + i), d[1] = 0;
            cv_text(x + 3, y + 1, &FONT_XS, d, now ? K_BG : K_DIM);
        }
        if (filled) {                            /* (yours in white; the walk's in the scale's teal) */
            const char *t = slot_label(i, nm);
            text_c(x + 25, y + 11, text_w(&FONT_B, t) <= 46 ? &FONT_B : &FONT_XS, t,
                   now ? K_BG : slot_is_walk(i) ? K_PAD : K_TEXT);
        } else if (i < n) {
            text_c(x + 25, y + 13, &FONT_XS, "(HOLDS)", now ? K_BG : K_DIM);
        }
    }
}

/* the level ring (SELECT on a layer's page): over the middle, the layer's level as a big knob in its colour, the
 * number inside, its name under; the arc glides to the level, a step a brief brightening; muted, grey; it fades out
 * OCT_SHOW after the last turn */
static void draw_oct(uint32_t now)
{
    static const uint16_t C[NLAYER] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_NOISE};
    const int32_t r = 30, cy = 66;
    int l = oct_ring_on(now) - 1, f;
    int32_t f2;
    uint16_t c, tr, tx;
    float v;
    char t[8];
    if (l < 0)
        return;
    v = (float)proj.par[LAYER_LEVEL[l]] * 0.01f;
    ui.oct_v += (v - ui.oct_v) * 0.55f;          /* (20 frames a second: there in ~100 ms) */
    if (fm_fabsf(v - ui.oct_v) < 0.004f)
        ui.oct_v = v;
    f = now < ui.oct_until ? 16 : 16 - (int)((now - ui.oct_until) * 16u / OCT_FADE);   /* the fade, 16 .. 0 */
    c = (ui.mutes & LAYER_MUTE[l]) == LAYER_MUTE[l] ? K_DIM : C[l];
    if (now - ui.oct_pulse < 160u)               /* a step: brighter for a moment */
        c = mix(c, K_TEXT, 7 - (int)((now - ui.oct_pulse) * 7u / 160u));
    tr = mix(K_BG, K_LINE, f);
    tx = mix(K_BG, K_TEXT, f);
    c = mix(K_BG, c, f);
    for (f2 = -(r + 6); f2 <= r + 6; f2++) {     /* (a disc behind it, the middle hidden) */
        int32_t w = (int32_t)fm_sqrtf((float)((r + 6) * (r + 6) - f2 * f2));
        cv_rect(CX - w, cy + f2, 2 * w + 1, 1, K_BG);
    }
    arc_th(CX, cy, r, 5.0f, ui.oct_v, tr, c, K_BG, 0);
    if ((ui.mutes & LAYER_MUTE[l]) == LAYER_MUTE[l]) {
        text_c(CX, cy - 6, &FONT_XS, "MUTED", tx);
    } else {
        itoa_u((uint32_t)proj.par[LAYER_LEVEL[l]], t);
        text_c(CX, cy - 11, &FONT_C, t, tx);
    }
    {
        int32_t w = text_w(&FONT_XS, LAYER_NAME[l]);
        cv_round(CX - w / 2 - 4, cy + r - 2, w + 8, 14, 4, K_BG);
        text_c(CX, cy + r - 2, &FONT_XS, LAYER_NAME[l], c);
    }
}

/* a nudge's card: a rounded panel in the middle, outlined amber (a nudge) or red (a shake), the layer and its move
 * and what changed; it fades out CARD_SHOW after */
static void draw_card(uint32_t now)
{
    const int32_t w = 196, h = 50, x = CX - w / 2, y = 34;
    int f;
    uint16_t edge, hc, tc;
    if (!card_on(now))
        return;
    f = now < ui.card_until ? 16 : 16 - (int)((now - ui.card_until) * 16u / CARD_FADE);
    if (now - ui.card_t0 < 120u)                 /* (it comes in brighter, for a moment) */
        f = 16;
    edge = mix(K_BG, ui.card_col, f);
    hc = mix(K_BG, ui.card_col, f);
    tc = mix(K_BG, K_TEXT, f);
    cv_round(x - 2, y - 2, w + 4, h + 4, 6, edge);
    cv_round(x, y, w, h, 5, mix(K_BG, K_PANEL, f / 2));
    text_c(CX, y + 4, &FONT_B, ui.card_head, hc);
    text_c(CX, y + 28, &FONT_XS, ui.card_text, tc);
}

#define BERLIN_BY "CRANKY MAN"                   /* (the user's name, on the main screen and the launch screen) */
static void draw_main(void)
{
    uint32_t now = plat_ms(), h = hash(2166136261u, (uint32_t)en_frozen << 16 | proj.piece << 24 | (uint32_t)en_playing << 17), i;
    static uint8_t moving;
    echo_take(now);                              /* (the ECHO meter: each frame) */
    orbits_take();
    if (ui.frame % 3u)                           /* 20 frames a second is plenty */
        return;
    if (grid_up()) {                             /* REC held: the effects grid (ui_frame draws it, whole;
                                                  * before the Chords and Pattern pages, which would draw over it) */
        if (!ui.grid_said && ann_on()) {         /* the grid in words: the white keys' effects, a group at a time */
            int e;
            sp_n = 0;
            sp_buf[0] = 0;
            sp_cat("EFFECTS GRID. White keys");
            for (e = 0; e < EN_NFX; e++) {
                sp_cat(e == 0 ? ": " : (e & 3) ? ", " : "; ");
                sp_cat(EN_FX_NAME[e]);
            }
            ann_say(sp_buf, 0, ANN_NOW);
            ui.grid_said = 1;
        }
        return;
    }
    ui.grid_said = 0;
    if (ui.view == V_PAT) {                      /* the Pattern page: the steps */
        h = hash(hash(0x50415454u, ui.lane | (uint32_t)proj.par[P_SMODE] << 4 | (uint32_t)en_drum_pos[0] << 8), en_playing);
        for (i = 0; i < EN_NDRUM; i++)
            h = hash(hash(h, lane_steps((int)i)), (uint32_t)(uint16_t)proj.par[P_SACC0 + lane_seq((int)i)]);
        h = hash(h, (uint32_t)oct_ring_on(now));
        if (h != ui.sig[1] || oct_ring_on(now)) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_pattern();
            draw_oct(now);
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.view == V_CHORDS) {                   /* the Chords page: the slots */
        h = hash(hash(0x43484F52u, ui.slot | (uint32_t)proj.par[P_PMODE] << 4), (uint32_t)en_prog_pos + 1u);
        for (i = 0; i < EN_NPROG; i++)
            h = hash(h, (uint32_t)proj.par[P_CR0 + i] | (uint32_t)proj.par[P_CQ0 + i] << 4 | (uint32_t)proj.par[P_CX0 + i] << 8);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_slots();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, en_chord[i]);
    for (i = 0; i < 6; i++)
        h = hash(h, en_lane[i] | (uint32_t)proj.par[i < 4 ? i : 0] << 16);
    h = hash(h, en_drum_pos[0] | (uint32_t)q16(en_duck) << 4 | (uint32_t)q16(en_pad_level) << 9 | (uint32_t)q16(en_bass_level) << 14);
    h = hash(h, (uint32_t)(en_prog_pos + 1) << 12);
    h = hash(h, (uint32_t)(en_drift * 40.0f) | en_section << 8 | (uint32_t)en_phrase_bar << 12 | (uint32_t)ui.perf << 16 | ui.solo << 20 |
                (uint32_t)ui.lock << 28);
    for (i = 0; i < EN_NLOOP; i++)
        h = hash(h, (uint32_t)q16(en_loop_glow[i]) | (uint32_t)en_loop_pos[i] << 8);
    for (i = 0; i < EN_NDRUM; i++)
        h = hash(h, (uint32_t)q16(en_drum_glow[i]));
    h = hash(h, ui.view | (uint32_t)oct_ring_on(now) << 8);
    if (parent_of(ui.view) >= 0) {               /* a sound or FM page: redrawn as its values move */
        for (i = 0; i < 4; i++)
            h = hash(h, (uint32_t)(proj.par[VIEW_KNOB[ui.view][i]] + 64));
        h = hash(h, (uint32_t)proj.par[view_voice(ui.view)] | (uint32_t)proj.par[P_DECAY] << 8 | (uint32_t)proj.par[P_SEQGRID] << 16);
    }
    h = hash(h, (uint32_t)card_on(now));
    if (h == ui.sig[1] && !moving && !oct_ring_on(now) && !card_on(now))
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    draw_stars();
    moving = (uint8_t)(draw_orbits() | en_playing);
    const int named = parent_of(ui.view) < 0 || scr_layer(ui.view) >= 0;   /* (a sound page: as its layer's) */
    static char ch_last[16];
    static uint32_t ch_t;
    char nm[16];
    uint32_t ch_age;
    uint16_t ch_c;
    chord_now(nm);
    if (!str_begins(nm, ch_last) || !str_begins(ch_last, nm)) {                   /* (1.3.0) a new chord: it comes in big and teal, then settles */
        if (ch_last[0])
            ch_t = now | 1u;
        if (card_on(now) && str_begins(ui.card_head, "NEXT CHORD") && now < ui.card_until)
            ui.card_until = now - CARD_FADE;     /* (the next chord's card gone at once, for it) */
        memcpy(ch_last, nm, sizeof ch_last);
    }
    ch_age = ch_t ? now - ch_t : 1000u;
    ch_c = en_frozen ? K_FROST : ch_age < 900u ? mix(K_ACC, K_TEXT, (int)(ch_age * 16u / 900u)) : K_TEXT;   /* (white over
                                                  * the orbits, the user; a new chord: cyan, settling) */
    if (ch_age < 900u)
        moving = 1;
    if (named) {                                 /* the chord, in the middle */
        if ((ch_age < 200u || (ch_age >= 400u && ch_age < 600u)) && text_w(&FONT_L, nm) <= 96) {   /* (big, small, big,
                                                                                         * small: a ping-pong) */
            text_c(CX, CY - 16, &FONT_L, nm, ch_c);
        } else if (text_w(&FONT_C, nm) <= 66) {  /* (the bigger face, the rings behind it; a long name, smaller) */
            text_c(CX, CY - 11, &FONT_C, nm, ch_c);
        } else if (text_w(&FONT_B, nm) <= 72) {
            text_c(CX, CY - 8, &FONT_B, nm, ch_c);
        }
        else
            text_c(CX, CY - 6, &FONT_XS, nm, ch_c);
        if ((proj.par[P_PMODE] & EN_PLOOP) && en_prog_pos >= 0 && prog_count()) {   /* Loop: where it is ("2/4") */
            char t[8];
            t[0] = (char)('1' + en_prog_pos), t[1] = '/', t[2] = (char)('0' + prog_count()), t[3] = 0;
            text_c(CX, CY + 11, &FONT_XS, t, K_DIM);
        }
    }
    {   /* top left: the phrase's section and its eight bars; under it, the mode the keys are in */
        char t[12];
        int b;
        caps(t, EN_SEC_NAME[en_section], sizeof t);
        cv_text(8, 2, &FONT_XS, t, en_section == EN_SEC_FLOW ? K_DIM : K_ACC);
        for (b = 0; b < 8; b++) {                /* (1.3.0: bars 4 and 8, the B bars, whiter) */
            const int bb = (b == 3 || b == 7) && !ui.lock, lit = en_playing && b <= en_phrase_bar;   /* (Lock: all alike) */
            uint16_t c = lit ? (bb ? K_TEXT : K_DIM) : (bb ? K_DIM : K_LINE);   /* (B bars: ahead, as bright as one played) */
            cv_round(9 + b * 5, 22, 3, 3, 1, c);
        }
        if (!en_frozen && en_drift_bars(proj.par[P_DRIFT])) {   /* under them, the way to the next chord (Drift) */
            int32_t f = (int32_t)(en_drift * 38.0f + 0.5f);
            cv_rect(9, 27, 38, 1, K_LINE);
            if (f > 0)
                cv_rect(9, 27, f, 1, K_PAD);
        }
    }
    {   /* (1.3.0) top right: the page's name (PERFORM's too); to its left, LOCK and SOLO while on */
        char pn[16];
        int32_t x;
        int k;
        caps(pn, VIEW_NAME[ui.view], sizeof pn);
        x = 232 - text_w(&FONT_B, pn);
        cv_text(x, 12, &FONT_B, pn, K_ACC);
        for (k = 0; k < NROUND; k++)             /* (under it, the round's dots, in the panel's order: FX ENV LFO GLO
                                                  * HOME SAVE ARP SEQ) */
            cv_round(212 - NROUND * 6 + 2 + k * 6, 31, 4, 4, 2, PAGE_ORDER[k] == page_of(ui.view) ? K_ACC : K_LINE);
        if (ui.solo) {                           /* (a solo on, in PERFORM or kept after it) */
            x -= 6 + text_w(&FONT_XS, "SOLO");
            cv_text(x, 14, &FONT_XS, "SOLO", K_CLAP);
        }
    }
    cv_text(232 - text_w(&FONT_XS, BERLIN_BY), -2, &FONT_XS, BERLIN_BY, K_DIM);   /* (the user's: over the page) */
    {   /* hold and Lock, in red, after the section's name (top left) */
        char t[12];
        int32_t x;
        caps(t, EN_SEC_NAME[en_section], sizeof t);
        x = 8 + text_w(&FONT_XS, t) + 6;
        if (en_frozen) {
            cv_text(x, 2, &FONT_XS, "hold", K_FROST);
            x += text_w(&FONT_XS, "hold") + 6;
        }
        if (ui.lock)
            cv_text(x, 2, &FONT_XS, "lock", K_FROST);
    }
    draw_meters();
    draw_duck();
    draw_lanes();
    {   /* (BERLIN) a sound page: its brightness knob's filter, moved by the filter motion, said under the lanes */
        int layer;
        float per;
        if (bright_of(ui.view, &layer, &per) >= 0 && fm_fabsf(en_fo[layer]) >= 0.1f) {
            char t[32], *q = t;
            const char *w = en_fo[layer] < 0.0f ? "MOVING DARKER, -" : "MOVING BRIGHTER, +";
            const int tenths = (int)(fm_fabsf(en_fo[layer]) * 10.0f + 0.5f);
            while (*w)
                *q++ = *w++;
            *q++ = (char)('0' + tenths / 10 % 10), *q++ = '.', *q++ = (char)('0' + tenths % 10);
            w = " OCT";
            while (*w)
                *q++ = *w++;
            *q = 0;
            text_c(120, 130, &FONT_XS, t, K_ACC);
        }
    }
    draw_oct(now);
    draw_card(now);
    cv_blit(0, MAIN_Y);
}

/* the knob strip (X0X's): a rule with the page's name and five page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a tint. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    if (view_voice(ui.view) >= 0)
        h = hash(h, (uint32_t)proj.par[view_voice(ui.view)]);
    if (ui.view == V_CHORDS)
        h = hash(h, (uint32_t)proj.par[P_PMODE]);
    if (atm_view(ui.view))
        h = hash(h, (uint32_t)proj.par[P_NCHAR]);
    if (ui.view == V_MIX)
        h = hash(h, ui.mutes);
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(h, (uint32_t)now_touch | (uint32_t)(knob_dot(ui.view) + 1000) << 1);   /* (the filter's dot moving) */
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule; at its right end, where you are: the page, then the dots */
        char t[12];
        int k;
        int32_t xd = 237 - NROUND * 6, xt;
        int32_t x0 = 4;
        if (view_voice(ui.view) >= 0 || ui.view == V_CHORDS || atm_view(ui.view)) {   /* a layer's page: its voice, at the rule's start;
                                                                   * the Chords page: the walk's; Noise: its character */
            char vn[12];
            if (ui.view == V_CHORDS || atm_view(ui.view)) {
                const char *w = ui.view != V_CHORDS ? EN_NCHAR_NAME[proj.par[P_NCHAR] % EN_NNCHAR]
                                                  : EN_WALK_NAME[(proj.par[P_PMODE] & 3) % EN_NWALK];
                int j;
                for (j = 0; j < 11 && w[j]; j++)
                    vn[j] = w[j];
                vn[j] = 0;
            } else {
                en_param_text(view_voice(ui.view), proj.par[view_voice(ui.view)], vn);
            }
            caps(t, vn, sizeof t);
            x0 = cv_text(6, -1, &FONT_XS, t, K_DIM) + 5;
        }
        if (now_touch && ui.touched >= 0 && ui.touched < 4) {   /* a knob turning: what it does, the rule's
                                                                          * whole width */
            char hp[48];
            caps(hp, KNOB_HELP[ui.view][ui.touched], sizeof hp);
            cv_rect(0, 0, 240, 12, K_BG);
            xt = cv_text(6, -1, &FONT_XS, hp, K_TEXT) + 5;
            if (xt < 234)
                cv_rect(xt, 5, 234 - xt, 1, K_LINE);
        } else {
            xt = 234;                            /* (1.3.0: the page's name and its dots top right now; here the rule) */
            cv_rect(x0, 5, xt - x0, 1, K_LINE);
            (void)xd, (void)k;
        }
    }
    for (i = 0; i < 4; i++) {
        const int moff = ui.view == V_MIX && (ui.mutes & LAYER_MUTE[i]) == LAYER_MUTE[i];   /* (the mix: a muted
                                                                                       * layer grey, its level kept) */
        int k = VIEW_KNOB[ui.view][i], lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        static const uint16_t MIXC[4] = {K_PAD, K_BELL, K_BASS, K_PULSE};   /* (the Mix page: the layers' colours) */
        uint16_t bg = hot ? K_PANEL : K_BG, fill = moff ? K_LINE : ui.view == V_MIX ? MIXC[i] : hot ? K_ACC : K_PAD;
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, KNOB_LABEL[ui.view][i], sizeof t);
        text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (k == K_SAVE) {                       /* (Save: a dot, lit while there's something to save) */
            cv_disc(cx, 37, 5, v ? K_ACC : K_LINE);
        } else if (k != K_CPU && hi - lo < 10) { /* a switch: its positions as pips (up to ten) */
            int n = hi - lo + 1, j, pw = n > 6 ? 3 : n > 4 ? 6 : 9, gap = n > 6 ? 2 : 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
            if (knob_dot(ui.view) > -999 && (int)i == bright_of(ui.view, &(int){0}, &(float){0.0f}) && hi > lo) {
                /* (BERLIN) the filter motion: where the filter is now, a cyan dot on the ring */
                const float a = (0.375f + 0.75f * (float)(knob_dot(ui.view) - lo) / (float)(hi - lo)) * FM_TWO_PI;
                cv_disc(cx + (int32_t)(fm_cosf(a) * 12.0f + 0.5f), 37 + (int32_t)(fm_sinf(a) * 12.0f + 0.5f), 2, K_ACC);
            }
            if (k == P_MOTION) {                 /* (Motion's three regions: the loop held, variations, Breaks) */
                arc_mark(cx, 37, 12, 0.4f, K_TEXT);
                arc_mark(cx, 37, 12, 0.7f, K_TEXT);
            }
        }
        knob_text(k, t);
        text_c(cx, 50, &FONT_B, t + (t[0] == '+'), moff ? K_DIM : hot ? K_ACC : K_TEXT);   /* (the user: no plus, "1";
                                                  * the font's thin plus read as a minus; the reader still says it) */
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i, ap = pool_abs();
    if (VIEW_BTN[page_of(ui.view)] != B_NONE && ((parent_of(ui.view) < 0 && ui.view != V_KEY && ui.view != V_ESND && ui.view != V_NSND2 && ui.view != V_AMOVE) || (plat_ms() / 300u) & 1u))
        b |= 1u << VIEW_BTN[page_of(ui.view)];   /* (a sound page, or Key: its button blinks) */
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (ui.view == V_PAT && !ui.perf && !grid_up()) {   /* the Pattern page: the lane's steps on the white keys */
        uint32_t p = lane_steps(ui.lane);
        for (i = 0; i < 16; i++)
            if (p >> i & 1u)
                k |= 1u << STEP_KEY[i];
    } else if (ui.view == V_CHORDS && !ui.perf && !grid_up()) {   /* the Chords page: the slot's root (if it has one) */
        if (!slot_is_gap(ui.slot)) {
            int r = proj.par[P_CR0 + ui.slot], q, x;
            if (slot_is_walk(ui.slot))
                walk_written(ui.slot, &r, &q, &x);
            for (i = 0; i < NKEYS; i++)
                if ((5u + i) % 12u == (uint32_t)r)
                    k |= 1u << i;
        }
    } else if (grid_up()) {                      /* the grid: each group's first key, and the effects held */
        for (i = 0; i < EN_NFX; i++)
            if (!(i & 3) || (ui.fx >> i & 1u))
                k |= 1u << FX_KEY[i];
    } else if (ui.perf) {                        /* PERFORM: the layers heard, the solos, the moves (dark while held) */
        const int blink = (plat_ms() / 250u) & 1u;
        b |= 1u << B_SEL;
        if (ui.view != V_MIX && (plat_ms() / 450u) & 1u)   /* white 6: blinking, slower than a solo, to be found;
                                                          * dark while held (the mix on the knobs) */
            k |= 1u << PERF_SHIFT;
        for (i = 0; i < 5; i++) {
            if (perf_sounds((int)i))
                k |= 1u << PERF_KEY[i];
            if (!(ui.moves >> i & 1u))
                k |= 1u << PERF_KEY[5 + i];
            if (!(ui.solo & LAYER_MUTE[i]) || blink)
                k |= 1u << PERF_KEY[10 + i];
        }

    } else if (proj.leds) {                      /* (BERLIN) the white key of the note the Lead sounds */
        const int d = en_lead_deg;
        (void)ap;
        int bk;
        if (d >= 0 && d < 16)
            k |= 1u << STEP_KEY[d];
        for (bk = 0; bk < NBLACK; bk++)          /* (the user: the black keys lit, the effects apart from the Lead's
                                                  * white keys; one dark while its effect is held, as PERFORM's moves) */
            if (!(BLACK_DO[bk] >= 100 ? ui.mv & (BLACK_DO[bk] - 100) : ui.bfx >> BLACK_DO[bk] & 1u))
                k |= 1u << BLACK_KEY[bk];
    }
    {   /* (the user, 2026-10-28) a layer's page: OCT+ lit while its octave is above its home (oct_home), OCT- below */
        const int l = oct_layer(ui.view);
        if (l >= 0 && LAYER_OCT[l] != NO_OCT) {
            const int v = knob_get(LAYER_OCT[l]);
            if (v != ui.oct_home[l])
                b |= 1u << (v > ui.oct_home[l] ? B_OCTUP : B_OCTDN);
        }
    }
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the garden grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns; the chord as it moves (queued: the
 * garden moves it by itself, every 20 s at the quickest); growing and resting from MIDI, and a MIDI clock
 * arriving or going; what a button did (say). WHERE (the reader asks, or it has just started listening) is
 * the whole screen in two: the page, its knobs and the header, then the garden (the chord, the scale).
 * Nothing is built while no reader listens. */
static struct {
    uint8_t view, ext, started;
    int16_t val[4];
    uint32_t chord;                            /* the chord's name, hashed */
} sp;
static char sp_buf[ANN_MAX + 1];
static uint32_t sp_n;
/* the chord names (en_chord_name's, after the root) in words: "m" and "M7" differ only in capitals, which
 * the words file ignores */
static const char *const CHORD_Q[][2] = {
    {"", "major"}, {"m", "minor"}, {"7", "7"}, {"M7", "major 7"}, {"m7", "minor 7"}, {"6", "6"},
    {"m6", "minor 6"}, {"add9", "add 9"}, {"madd9", "minor add 9"}, {"aug", "augmented"}, {"dim", "diminished"},
    {"dim7", "diminished 7"}, {"m7b5", "half diminished"}, {"mM7", "minor major 7"}, {"sus2", "suspended 2"},
    {"sus4", "suspended 4"}, {"7sus4", "7 suspended 4"}, {"5", "5"},
};
#define NCHORD_Q (sizeof CHORD_Q / sizeof CHORD_Q[0])

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_int(uint32_t v)
{
    char t[12];
    itoa_u(v, t);
    sp_cat(t);
}
static int sp_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a value as it is said: as shown */
static void sp_value(int k)
{
    char t[16];
    knob_text(k, t);
    sp_cat(t);
}
static void sp_knob(int view, int i)           /* "Stabs 30": the name the page gives it, then its value */
{
    sp_cat(KNOB_LABEL[view][i]);
    sp_cat(" ");
    sp_value(VIEW_KNOB[view][i]);
}

static void sp_chord_q(int root, const char *q)   /* "E minor 7" */
{
    uint32_t i;
    sp_cat(EN_NOTE_NAME[root % 12]);
    sp_cat(" ");
    for (i = 0; i < NCHORD_Q; i++)
        if (sp_same(q, CHORD_Q[i][0])) {
            sp_cat(CHORD_Q[i][1]);
            return;
        }
    sp_cat(q);
}
static void sp_chord(void)                     /* the chord in the middle of the garden */
{
    char nm[16];
    const char *q = chord_now(nm), *r = EN_NOTE_NAME[en_chord_root % 12u];
    while (*r && *q == *r)
        q++, r++;
    sp_chord_q(en_chord_root % 12u, q);
}

/* the page: its name and its knobs; all: the header too (the piece, its key, the tempo, playing or resting) */
static void sp_page(int all)
{
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(VIEW_NAME[ui.view]);
    sp_cat(" page");
    if (atm_view(ui.view)) {
        sp_cat(", character ");
        sp_cat(EN_NCHAR_NAME[proj.par[P_NCHAR] % EN_NNCHAR]);
    } else if (view_voice(ui.view) >= 0) {
        char vn[12];
        en_param_text(view_voice(ui.view), proj.par[view_voice(ui.view)], vn);
        sp_cat(", voice ");
        sp_cat(vn);
    }
    if (ui.view == V_CHORDS) {
        sp_cat(", walk ");
        sp_cat(EN_WALK_NAME[(proj.par[P_PMODE] & 3) % EN_NWALK]);
    }
    for (i = 0; i < 4; i++) {
        if (VIEW_KNOB[ui.view][i] == K_NONE)
            continue;
        sp_cat(i ? ", " : ": ");
        sp_knob(ui.view, i);
    }
    if (!all)
        return;
    sp_cat(". Piece ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    if (en_ext) {
        sp_cat(", tempo EXT");
    } else if (ui.view != V_KEY) {             /* (the Key page's first knob says it) */
        sp_cat(", tempo ");
        sp_int((uint32_t)proj.par[P_TEMPO]);
    }
    sp_cat(sp_tp.playing ? ", playing" : ", resting");
    if (ui.dirty)
        sp_cat(", not saved yet");
    if (ui.view == V_SETUP) {
        sp_cat(". CPU ");
        sp_int(plat_cpu_pct());
        sp_cat("%, peak ");
        sp_int(plat_cpu_peak());
        sp_cat("%");
        if (plat_usb_power()) {
            sp_cat(", on USB power");
        } else if (plat_battery() >= 0) {
            sp_cat(", battery about ");
            sp_int((uint32_t)batt_pct());
            sp_cat("%");
        }
        if (plat_xruns()) {
            sp_cat(", ");
            sp_int(plat_xruns());
            sp_cat(" dropouts");
        }
    }
}

/* the main screen: the chord and how far to the next, the section, the scale from the root up ("F and F
 * sharp": one letter's two notes joined, so they are heard as two) */
static void sp_garden(void)
{
    uint32_t ap = pool_abs();
    int i, last = -1;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("Chord ");
    sp_chord();
    if (en_frozen) {
        sp_cat(", hold");
    } else if (en_drift_bars(proj.par[P_DRIFT])) {
        sp_cat(", next chord ");
        sp_int((uint32_t)(fm_clampf(en_drift, 0.0f, 1.0f) * 100.0f + 0.5f));
        sp_cat("% of the way");
    } else {
        sp_cat(", held");
    }
    sp_cat(". Section ");
    sp_cat(EN_SEC_NAME[en_section]);
    {   /* (BERLIN) on a sound page: where its brightness knob's filter is moved ("Cutoff moving darker, about -1 octave") */
        int layer;
        float per;
        const int k = bright_of(ui.view, &layer, &per);
        if (k >= 0 && fm_fabsf(en_fo[layer]) >= 0.25f) {
            const int oct = (int)(fm_fabsf(en_fo[layer]) + 0.5f);
            sp_cat(". ");
            sp_cat(KNOB_LABEL[ui.view][k]);
            sp_cat(en_fo[layer] < 0.0f ? " moving darker, about " : " moving brighter, about ");
            if (oct) {
                sp_cat(en_fo[layer] < 0.0f ? "-" : "+");
                sp_int((uint32_t)oct);
                sp_cat(oct == 1 ? " octave" : " octaves");
            } else {
                sp_cat("half an octave");
            }
        }
    }
    sp_cat(". Scale");
    for (i = 0; i < 12; i++) {
        int pc = (proj.par[P_ROOT] + i) % 12;
        if (ap >> pc & 1u) {
            sp_cat(last < 0 ? " " : EN_NOTE_NAME[last][0] == EN_NOTE_NAME[pc][0] ? " and " : ", ");
            sp_cat(EN_NOTE_NAME[pc]);
            last = pc;
        }
    }
}


static void sp_piece_text(void)                /* "PIECE Timewind, D Minor, tempo 96" */
{
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("PIECE ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    sp_cat(", tempo ");
    sp_int((uint32_t)proj.par[P_TEMPO]);
}
static void sp_piece(void)
{
    if (!ann_on())
        return;
    sp_piece_text();
    ann_say(sp_buf, 0, ANN_NOW);
    sp_said = 1;
}

static uint32_t sp_chord_sig(void)
{
    char nm[16];
    const char *s = chord_now(nm);
    uint32_t h = 2166136261u;
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}
static void sp_snap(void)
{
    int i;
    for (i = 0; i < 4; i++)
        sp.val[i] = (int16_t)knob_get(VIEW_KNOB[ui.view][i]);
    sp.view = ui.view;
    sp.ext = en_ext;
    sp.chord = sp_chord_sig();
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {                       /* everything on the screen, in two */
        sp.started = 1;
        sp_snap();
        sp_page(1);
        ann_say(sp_buf, 0, ANN_NOW);
        sp_garden();
        ann_say(sp_buf, 0, 0);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        sp_snap();
        return;
    }
    if (en_playing != sp_tp.playing && now - sp_tp.play_t > 150u) {   /* MIDI start / stop */
        sp_tp.playing = en_playing;
        ann_say(en_playing ? "PLAYING" : "RESTING", 0, 0);
    }
    if (en_ext != sp.ext) {                           /* a MIDI clock arrived, or stopped */
        sp.ext = en_ext;
        sp_n = 0;
        sp_cat("Tempo ");
        if (en_ext)
            sp_cat("EXT");
        else
            sp_int((uint32_t)proj.par[P_TEMPO]);
        ann_say(sp_buf, 0, 0);
    }
    if (ui.view != sp.view) {                         /* a new page: said when it settles */
        sp_snap();
        sp_page(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    for (i = 0; i < 4; i++) {                         /* a knob turning */
        int v = knob_get(VIEW_KNOB[ui.view][i]);
        if (v != sp.val[i]) {
            sp.val[i] = (int16_t)v;
            if (changed < 0)
                changed = i;
        }
    }
    if (changed >= 0) {
        sp_n = 0;
        sp_knob(ui.view, changed);
        ann_later(sp_buf, ANN_NOW, 120u, now);
    }
    c = sp_chord_sig();
    if (c != sp.chord) {                              /* the chord moved: queued, after what is being said */
        sp.chord = c;
        sp_n = 0;
        sp_cat("Chord ");
        sp_chord();
        ann_say(sp_buf, 0, 0);
    }
}

#ifdef EN_HOST
/* every page, knob value, chord and message as a screen reader gets them (the "speakdump"
 * command): KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"},
                                         {"NEXT CHORD", "ON THE BAR"}, {"NEXT CHORD", 0}, {"RESTING", 0},
                                         {"PLAYING", 0}, {"HOLD", "ON"}, {"HOLD", "OFF"}, {"SAVED", 0},
                                         {"PERFORM", "ON"}, {"PERFORM", "OFF"}};
    static const char *const WARN[][2] = {{"THE ROOT STAYS", "(SETUP)"}, {"3 NOTES", "AT LEAST"},
                                          {"NOT SAVED", "FLASH ERROR"}};
    project_t keep = proj;
    uint8_t keep_view = ui.view, keep_dirty = ui.dirty, keep_play = sp_tp.playing;
    uint8_t keep_ext = en_ext, keep_frozen = en_frozen, keep_rest = en_pulse_rest, keep_sec = en_section;
    int v, i, x, lo, hi;
    char t[48];
    for (v = 0; v < NVIEWS; v++) {
        ui.view = (uint8_t)v;
        sp_page(0);
        sp_print("SCREEN", sp_buf);
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[v][i];
            knob_range(k, &lo, &hi);
            for (x = lo; x <= hi; x++) {
                if (k < P_NPARAMS)
                    proj.par[k] = (int16_t)x;
                else if (k == K_SLOT)
                    ui.slot = (uint8_t)x;
                else if (k == K_CQ || k == K_CX)
                    proj.par[(k == K_CQ ? P_CQ0 : P_CX0) + ui.slot] = (int16_t)x;
                else if (k == K_LANE)
                    ui.lane = (uint8_t)x;
                else if (k == K_LCH)
                    proj.par[P_SCH0 + lane_seq(ui.lane)] = (int16_t)x;
                else
                    proj.leds = (uint8_t)x;
                sp_n = 0;
                sp_knob(v, i);
                sp_print("KNOB", sp_buf);
            }
            proj = keep;
        }
    }
    for (x = 0; x < 8; x++) {                         /* WHERE: playing, resting, unsaved, a clock, held, custom */
        ui.view = (uint8_t)(x * 3 % NPAGES);
        sp_tp.playing = (uint8_t)(x & 1);
        ui.dirty = (uint8_t)(x >> 1 & 1);
        en_ext = (uint8_t)(x == 3);
        en_frozen = (uint8_t)(x == 5);
        en_pulse_rest = (uint8_t)(x == 2);
        en_section = (uint8_t)(x % EN_NSEC);
        proj.pool = x == 6 ? 0xFFFu : x == 7 ? 0x91u : keep.pool;
        proj.par[P_ROOT] = (int16_t)(x * 5 % 12);
        sp_page(1);
        sp_print("WHERE", sp_buf);
        sp_garden();
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    ui.dirty = keep_dirty;
    sp_tp.playing = keep_play;
    en_ext = keep_ext;
    en_frozen = keep_frozen;
    en_pulse_rest = keep_rest;
    en_section = keep_sec;
    ui.view = keep_view;
    for (x = 0; x < 12; x++)
        for (i = 0; i < (int)NCHORD_Q; i++) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord_q(x, CHORD_Q[i][0]);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], t);
        sp_print("MSG", t);
    }
    for (i = 0; i < (int)(sizeof WARN / sizeof WARN[0]); i++) {
        msg_text(WARN[i][0], WARN[i][1], t);
        sp_print("WARN", t);
    }
    for (x = 0; x < 12; x++) {
        msg_text("ADDED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
        msg_text("REMOVED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = 10; x < PERF_N; x++) {
        msg_text("SOLO", PERF_NAME[x], t);
        sp_print("MSG", t);
        msg_text(PERF_NAME[x], "SOLO OFF", t);
        sp_print("MSG", t);
    }
    msg_text("SOLO", "OFF", t);
    sp_print("MSG", t);
    for (x = 0; x < 10; x++) {
        msg_text(PERF_NAME[x], x < 5 ? "OFF" : 0, t);
        sp_print("MSG", t);
        if (x < 5) {
            msg_text(PERF_NAME[x], "ON", t);
            sp_print("MSG", t);
        }
    }
    for (x = 0; x < EN_NMODES; x++) {
        msg_text("MODE", EN_MODE_NAME[MODE_ORDER[x]], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NLAYER; x++) {                    /* OCT- / OCT+: a level, a mute */
        msg_text(LAYER_NAME[x], "55", t);
        sp_print("MSG", t);
        msg_text(LAYER_NAME[x], "MUTED", t);
        sp_print("MSG", t);
        msg_text(LAYER_NAME[x], "ON", t);
        sp_print("MSG", t);
    }
    for (x = 90; x <= 140; x += 25) {
        char o[8];
        itoa_u((uint32_t)x, o);
        msg_text("TEMPO", o, t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NPIECES; x++) {
        project_piece(x);
        sp_piece_text();
        sp_print("MSG", sp_buf);
        msg_text("RESET", piece_name(x), t);
        sp_print("MSG", t);
    }
    proj = keep;
    sp_print("MSG", "Tempo EXT");
    sp_print("MSG", "Tempo 122");
}
#endif

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    oct_home_take();
    lcd_fill(0, 0, 240, 240, K_BG);
    if (!en_playing)
        en_play(1);                              /* it plays from the start */
    sp_tp.playing = 1;
    sp_tp.play_t = plat_ms();
}

void ui_frame(void)
{
    if (ui.s2_hand && !en_s2oct_shift)           /* (the engine has the hand's octave: its shifts shown again) */
        ui.s2_hand = 0;
    prog_take();
    nudge_tick();
    input();
    autosave();
    if (grid_up()) {                             /* REC held: the effects, the whole screen */
        if (!(ui.frame % 3u))
            draw_grid();
        draw_main();                             /* (it says the grid to a screen reader) */
    } else {
        draw_header();
        draw_main();
        draw_knobs();
    }
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }

/* (BERLIN) the launch screen, the user's: NACHTFAHRT large (BERLIN's new name, the user's), "by CRANKY MAN" under
 * it, on black among a few stars, fading in; the version small at the foot. Frame f of UI_SPLASH_N, drawn whole (two
 * bands of 240 x 120) */
static void splash_band(int f, int32_t oy)
{
    static const uint8_t SX[14] = {18, 51, 77, 103, 139, 171, 205, 226, 33, 66, 121, 158, 190, 214},
                         SY[14] = {22, 61, 15, 190, 31, 205, 48, 120, 150, 218, 230, 92, 168, 9};
    int k;
    cv_begin(240, 120, K_BG);
    for (k = 0; k < 14; k++)
        cv_rect(SX[k], SY[k] - oy, 1, 1, mix(K_BG, K_DIM, 3 + (k & 3)));
    if (f >= 1)
        text_c(120, 92 - oy, &FONT_L, "NACHTFAHRT", mix(K_BG, K_TEXT, f >= 8 ? 16 : 2 * f));
    if (f >= 8)
        text_c(120, 132 - oy, &FONT_S, "by " BERLIN_BY, mix(K_BG, K_BELL, f >= 16 ? 16 : 2 * (f - 7)));
    if (f >= 16)
        text_c(120, 224 - oy, &FONT_XS, EN_VERSION, K_DIM);
    cv_blit(0, (uint32_t)oy);
}

void ui_splash(int f)
{
    splash_band(f, 0);
    splash_band(f, 120);
}
