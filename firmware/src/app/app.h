/* SPDX-License-Identifier: GPL-3.0-only */
/* BERLIN app: the project (everything that is saved) and the app's entry points. Included by the unity
 * build after gfx.c (drawing helpers in scope) and by the host simulator. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/amb.h"

#ifndef EN_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION "DEV"
#endif
#define PROJ_MAGIC 0x314C5242u           /* "BRL1": DUB-1's ("DUB1") and AMB-1's ("ENO1") projects give the recipes */
#define PROJ_FORMAT 9u                   /* (9: a ninth piece, Sakura Rain (first Umeda, then Ryokuu): its slot appended, saves reset (the user: "saves -
                                          * reset", 2026-10-30); 8: the Lead's Memory (P_LMEM) appended; 7: the filter motion: the Move pages' and Bridge's slots, no Emerge, Seq2 in, Ending; 6: BERLIN's eight pieces; 5: the arc: Emerge, Arc, Ending; 4: an Atmosphere in half the pieces; 3: Sequence 2 heard by default; 2: the pages' new parameters, the Lead's sends; while BERLIN is built, a
                                         * slot that changes meaning moves the format on, so a save from before gives the
                                         * recipes) */
#define NPIECES EN_NPIECES

typedef struct {
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t pings[EN_SEQ_N];             /* its patterns as they were left (en_seq_get) */
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
