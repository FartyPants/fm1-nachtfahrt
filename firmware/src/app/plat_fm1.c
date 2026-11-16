/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 platform on the FM-1: plat.h over Felucca's HAL, panel map, USB rings and storage.
 * Part of the unity build (amb.c), after the HAL, usb.c, storage.c and panel.c. */

uint32_t plat_ms(void) { return fm1_ms; }

#if BOARD_SMK37
/* The SMK-37's 14 buttons, by their printed names (smk37-port.yaml map: printed). The FM-1's fourteen
 * are named after its own pages, which the SMK has not got, so the app's B_* keep their jobs and the
 * printed key that means the same thing takes them:
 *   HOME the home page        GLOBE         SEL  PERFORM        PARA
 *   EDIT the Chords page      CHORD         SAVE the Noise page PATCH
 *   ENV  the Drone page       BT            LFO  the Lead page   NOTE REPEAT
 *   ARP  the Sequence page    ARP           SEQ  Sequence 2      SEQ
 *   FX   the Space page       FX            GLO  the Setup page  KNOB BANK
 *   PLAY / REC                SEQ PLAY / SEQ REC                OCT- / OCT+  OCTAVE- / OCTAVE+
 * What the SMK has and NACHTFAHRT has no job for (BT, the page arrows, the fader bank) is left to the
 * controls in smk_controls.h and to the stock firmware's own use. */
static const uint8_t SMK_BTN_APP[NB] = {14, 13, 11, 7, 9, 0, 10, 12, 6, 15, 16, 17, 4, 5};
/* the raw matrix id of a printed button: the SMK's own table, the FM-1's calibrated one (main_fm1.c's
 * power-on combos and the bootguard's PLAY used panel.btn[], which names the wrong key on the SMK) */
#define PLAT_RAW_BTN(i) (SMK_BTN_APP[i])
#else
#define PLAT_RAW_BTN(i) (panel.btn[i])
#endif

uint32_t plat_buttons(void)
{
#if BOARD_SMK37
    /* (the SMK's buttons are fixed: a calibration taught on the FM-1 would name the wrong keys) */
    uint32_t raw = fm1_in.buttons, out = 0, i;
    for (i = 0; i < NB; i++)
        if (raw & (1u << SMK_BTN_APP[i]))
            out |= 1u << i;
    return out;
#else
    uint32_t raw = fm1_in.buttons, out = 0, i;
    for (i = 0; i < NB; i++)
        if (raw & (1u << panel.btn[i]))
            out |= 1u << i;
    return out;
#endif
}


uint32_t plat_keys(void) { return fm1_in.notes & ((1u << NKEYS) - 1u); }

#if BOARD_SMK37
/* The SMK's eight knobs, by the printed K1..K8 (smk37-port.yaml knobs: layout fm1):
 *   K1 MASTER the output (the FM-1's MASTER pot, virtual here: fm1_adc_read reports it)
 *   K2 SELECT  K3 ALGORITHM  K4 PRESETS   the FM-1's own three, in its screen order
 *   K5..K8     the screen's four dials (KNOB 1..4) */
static const uint8_t SMK_ENC_APP[NE] = {1, 2, 3, 4, 5, 6, 7};
#endif

int32_t plat_enc(int role)
{
    if (role < 0 || role >= NE)
        return 0;
#if BOARD_SMK37
    return fm1_enc_take(SMK_ENC_APP[role]) * panel.dir[role];
#else
    return fm1_enc_take(panel.enc[role]) * panel.dir[role];
#endif
}

static volatile uint32_t master_q12 = 2048;
uint32_t plat_master(void) { return master_q12; }

/* LEDs: the picture is built off-line and copied one byte per column (Felucca: clearing
 * and relighting would let the 10 kHz scan catch the dark gap and flicker). The SMK has no
 * per-key LED at all: its own plat_leds is below. */
#if !BOARD_SMK37
static uint8_t led_pos[FM1_NKEY];
static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < FM1_NKEY; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}
/* Glow: every button not lit is lit dim, on one scan frame in GLOW_DIV (the HAL's fm1_led_dim) */
#define GLOW_DIV 4u
static uint8_t glow_on = 1;
void plat_glow(int on) { glow_on = (uint8_t)(on != 0); }
/* PLAY has two LEDs: its red at the button's own position, a green at the keyless matrix position
 * column 8, row PA5 (found on the device). Green, unless recording. */
static uint8_t play_red;
void plat_play_red(int red) { play_red = (uint8_t)(red != 0); }
#endif  /* !BOARD_SMK37 */

#if BOARD_SMK37
/* Glow is the FM-1's: unlit buttons stay dark on this panel (the stock firmware lights nothing either),
 * but the app calls plat_glow whatever the board. */
void plat_glow(int on) { (void)on; }
void plat_play_red(int red) { (void)red; }

/* The SMK's 19 LEDs sit on its buttons (one each, SEQ PLAY's two-colour, SEQ REC's red): there is no
 * per-key LED, so the keys' picture (the step grid, the effects) is the screen's job here, and the
 * pads carry it (smk_controls.h). A button's LED is fm1_led_key(SMK id), the HAL's own table. Glow is
 * the FM-1's: unlit buttons stay dark on this panel (the stock firmware lights nothing either). */
void plat_leds(uint32_t buttons, uint32_t keys)
{
    uint32_t i;
    (void)keys;
    for (i = 0; i < NB; i++)
        fm1_led_key(SMK_BTN_APP[i], (int)((buttons >> i) & 1u));
}
#else
void plat_leds(uint32_t buttons, uint32_t keys)
{
    uint8_t nl[FM1_NCOL] = {0}, dl[FM1_NCOL] = {0};
    uint32_t i, c;
    for (i = 0; i < NB + NKEYS; i++) {
        uint32_t id = i < NB ? panel.btn[i] : 14u + (i - NB);
        int on = i < NB ? (buttons >> i) & 1u : (keys >> (i - NB)) & 1u;
        uint8_t q = led_pos[id];
        if (i == B_PLAY && !play_red)            /* PLAY's green LED instead (keyless position col 8, row PA5) */
            q = (uint8_t)((8u << 3) | 1u);
        if (q == 0xFF)
            continue;
        if (on)
            nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
        else if (i < NB && glow_on)
            dl[q >> 3] |= (uint8_t)(1u << (q & 7u));
    }
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led[c] = nl[c];
        fm1_led_dim[c] = dl[c];
    }
    fm1_led_dim_div = glow_on ? GLOW_DIV : 0u;
}
#endif

/* MIDI: Felucca's rings carry 4-byte USB-MIDI event packets, byte 0 = cable / CIN */
int plat_midi_in(uint32_t *pkt)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = midi_in_q[mi_r % MQ];
    mi_r++;
    return 1;
}
void plat_midi_out(uint32_t pkt) { midi_out_event(pkt); }
/* the engine's MIDI out (from the render): a USB-MIDI packet, cable 0, CIN = the status's high nibble */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

int plat_store_load(uint32_t obj, void *dst, uint32_t max) { return flash_ok ? st_load(obj, dst, max) : -1; }
int plat_store_save(uint32_t obj, const void *src, uint32_t len) { return flash_ok ? st_save(obj, src, len) : -9; }

static volatile uint32_t audio_cpu_pct, audio_xruns, audio_cpu_peak;
uint32_t plat_cpu_pct(void) { return audio_cpu_pct; }
uint32_t plat_cpu_peak(void) { return audio_cpu_peak; }
static int32_t batt_raw = -1;
int32_t plat_battery(void) { return batt_raw; }
int plat_usb_power(void) { return usb.config && !usb.suspended; }
uint32_t plat_xruns(void) { return audio_xruns; }
