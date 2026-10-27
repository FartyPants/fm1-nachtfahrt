/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB-1 app: the project (everything that is saved) and the app's entry points. Included by the unity
 * build after gfx.c (drawing helpers in scope) and by the host simulator. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/amb.h"

#ifndef EN_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION "DEV"
#endif
#define PROJ_MAGIC 0x31425544u           /* "DUB1": AMB-1's projects ("ENO1") give the recipes */
#define PROJ_FORMAT 17u                  /* 17: the kick's rumble (P_RUMBLE; 8-16 read, it from the recipes);
                                         * 16: the noise's LFO (P_NLFO; 8-15 read, it from the recipes);
                                         * 15: the Noise page's second (Width, Plate, Echo) and the hats' length
                                         * (P_NWIDTH..P_HLEN; 8-14 read, the new ones from the recipes);
                                         * 14: the chord's Ghost, the kick's density, the echo's Blur, Tone and
                                         * Width, the shimmer's Tune (P_CGHOST..P_NTUNE; 8-13 read, the new ones from the
                                         * recipes);
                                         * 13: Throws, Ghost, Notes, Hats level (P_CTHROW..P_HATSLVL), the groove five
                                         * patterns again with Notes (12's ladder converted; 8-12 read, the new ones
                                         * from the recipes);
                                         * 12: the groove a ladder (0 Drone .. 8 Roller; 8-11 read, converted);
                                         * 11: the noise's five (P_NCOL..P_NCHAR; 8-10 read, the noise from the recipes);
                                         * 10: the chords' slots as yours, the walk's or gaps (9's and 8's read, an empty
                                         * slot a gap); 9: each piece's ping loops (8, DUB-1 1.0.0's, read and kept;
                                         * older projects give the recipes) */
#define NPIECES EN_NPIECES

typedef struct {
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];           /* its ping loops as they were left (en_pings_get) */
    uint8_t pings_ok;                    /* 0: none kept (a recipe, or a project from 1.0.0): grown afresh */
    uint8_t screen;                      /* its screen view, 0..2: ripples, scope, spectrum (before 1.2.0: 0) */
} slot_t;

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* the piece playing, as it is now (amb.h P_*) */
    uint16_t pool;                       /* its scale, relative to the root (bit 0 = the root) */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t piece;                       /* which piece */
    uint8_t screen;                      /* the Dub page's middle: 0 ripples, 1 scope, 2 spectrum (HOME again) */
    uint8_t rsv0;
    slot_t slot[NPIECES];                /* every piece as it was left (Tune and MIDI out are global) */
    uint8_t rsv[16];                     /* room to grow */
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
void project_apply(void);                /* every value to the engine (boot, load) */
const char *piece_name(int i);
void project_piece(int i);               /* the playing piece kept, piece i taken up as it was left */
void project_piece_reset(void);          /* the playing piece back to its recipe */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);
#define UI_SPLASH_N 26                   /* the launch screen's frames (~1.4 s at 55 ms) */
void ui_splash(int f);                   /* frame f of the launch screen (the last: as it stays) */
