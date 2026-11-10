/* SPDX-License-Identifier: GPL-3.0-only */
/* NACHTFAHRT on the SMK-37: the controls the FM-1 does not have.
 *
 * The owner's decisions are in smk37-port.yaml; this is the app-side policy that turns the
 * HAL's readings into the firmware's own calls, so the screen, the buttons and these controls
 * always show one state. The FM-1 build never compiles it (main.c includes it under BOARD_SMK37).
 *
 *   pads     the five layers' mute (top row) and solo (bottom row), as the FM-1's SEL keys do
 *   faders   four banks, soft takeover
 *   wheels   pitch bend +-2 semitones, mod wheel vibrato
 *   keys     the ten outer keys extend the keybed by notes
 *
 * The mutes and solos go through ui.mutes / ui.solo and ui_perf_mutes(), the same state the
 * FM-1's PERFORM page uses: a pad cannot disagree with a key.
 *
 * What is NOT here, and is engine work rather than panel work: playing the wheels as bend and
 * vibrato needs the sampler's read head (the Felucca layer's smk_wheels_block in seq.c). Until
 * that lands with bring-up, smk_wheel_bend() / smk_wheel_mod() carry the calibrated values and
 * nothing consumes them yet. */

/* The app's mute/solo state and the layers' engine bits live in ui.c (ui.mutes, ui.solo,
 * LAYER_MUTE, perf_mutes), which main.c includes after this file. Rather than guess at them,
 * the two hooks below are the whole contract: ui.c defines them, this file calls them. */
static void smk_ui_mute_toggle(uint32_t layer);      /* ui.c: the FM-1's SEL white key 1-5 */
static void smk_ui_solo_toggle(uint32_t layer);      /* ui.c: the FM-1's SEL black key 1-5 */

/* which pads are down now, so a mute toggles on the press and not on every frame while held */
static uint16_t smk_pad_down;

/* The fader banks. A parameter the bank cannot reach on this firmware is 0xFF and the fader
 * ignores it, rather than writing something arbitrary. */
enum { SMK_BANK_LEVEL, SMK_BANK_SOUND, SMK_BANK_SPACE, SMK_BANK_MOVE, SMK_NBANK };
static const uint8_t smk_fbank_par[SMK_NBANK][4] = {
    {P_CHORD, P_SEQ, P_LEAD, P_SEQ2},              /* A: the layer levels */
    {P_CRACKLE, P_CUTOFF, P_SWEEP, P_MOTION},      /* B: the Atmosphere's level, the stab's filter, its tone, the arrangement */
    {P_ECHO, P_REVERB, P_FEEDBACK, P_SIZE},        /* C: the space */
    {P_EVOLVE, P_MOTION, P_DRIFT, P_DENSITY},      /* D: the NACHTFAHRT page's motion */
};
static uint8_t smk_fbank;

static void smk_pads_tick(void)
{
    uint32_t p;
    for (p = 0; p < SMK_NPAD; p++) {
        uint16_t bit = (uint16_t)(1u << p);
        uint8_t down = (uint8_t)((fm1_in.pnotes >> SMK_WHITE_NOTE[p]) & 1u);
        if (down == ((smk_pad_down & bit) != 0))
            continue;
        if (down)
            smk_pad_down |= bit;
        else
            smk_pad_down &= (uint16_t)~bit;
        if (!down)                                 /* a pad acts on the press; the release does nothing */
            continue;
        if (p < 5u)                                /* top row, pads 1-5: mute layer p */
            smk_ui_mute_toggle(p);
        else if (p >= 8u && p < 13u)               /* bottom row, pads 9-13: solo layer p-8 */
            smk_ui_solo_toggle(p - 8u);
        /* pads 6, 7, 8, 14, 15 and 16 (the levels page, SWEEP, CHAOS, the Lead's forget, both
         * sequences out, LOCK) are the firmware's own gestures: they call its handlers, which
         * live in ui.c's PERFORM page. Wired at bring-up, once that page's key map is settled
         * (an open item in smk37-port.yaml). */
    }
}

/* Soft takeover: a fader acts only once it has moved, and the first reading after a bank change
 * arms it instead of writing, so nothing jumps. */
static uint8_t smk_fad_last[4], smk_fad_armed;

static void smk_faders_tick(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        uint8_t v = fm1_in.fader[i], bit = (uint8_t)(1u << i), par = smk_fbank_par[smk_fbank][i];
        if (!(smk_fad_armed & bit) || v == smk_fad_last[i]) {
            smk_fad_last[i] = v;
            smk_fad_armed |= bit;
            continue;
        }
        smk_fad_last[i] = v;
        if (par != 0xFF)
            en_set(par, v);
    }
}

/* The wheels self-calibrate: the pitch wheel's rest is its centre, each wheel's range grows to
 * what it has been seen to reach, and a dead band around the centre keeps a resting wheel quiet. */
static int32_t smk_bend, smk_mod;
static int32_t smk_wheel_bend(void) { return smk_bend; }
static int32_t smk_wheel_mod(void) { return smk_mod; }

static void smk_wheels_tick(void)
{
    enum { DEAD = 12, SPAN0 = 80, MOD_LO = 485, MOD_HI = 615 };
    static int32_t pc, plo, phi, mlo, mhi;
    int32_t p = fm1_in.wheel[0], m = fm1_in.wheel[1];
    if (fm1_in.frames < 300u)                      /* (the readings settle) */
        return;
    if (!pc) {
        pc = p;
        plo = p - SPAN0;
        phi = p + SPAN0;
        mlo = m < MOD_LO ? m : MOD_LO;
        mhi = m > MOD_HI ? m : MOD_HI;
    }
    if (p < plo) plo = p;
    if (p > phi) phi = p;
    if (m < mlo) mlo = m;
    if (m > mhi) mhi = m;
    {
        int32_t d = p - pc, neg = pc - plo, pos = phi - pc, b = 0;
        if (d > DEAD && pos > DEAD) b = (d - DEAD) * 8192 / (pos - DEAD);
        else if (d < -DEAD && neg > DEAD) b = (d + DEAD) * 8192 / (neg - DEAD);
        smk_bend = b < -8192 ? -8192 : b > 8192 ? 8192 : b;
    }
    {
        int32_t d = mhi - mlo, t = d > 0 ? (mhi - m) * 127 / d : 0;
        smk_mod = t < 0 ? 0 : t > 127 ? 127 : t;
    }
}

static void smk_controls_tick(void)
{
    smk_pads_tick();
    smk_faders_tick();
    smk_wheels_tick();
}
