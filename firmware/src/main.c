/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 firmware for the M-VAVE FM-1: one compilation unit for the platform and the app
 * (Felucca's layout, as X0X: the HAL is header-only, so everything that touches it is here).
 * The instrument (dsp/amb.c) is a separate unit compiled at -O2 by tools/build.py; this one is
 * -Os. Order matters. */
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#if BOARD_SMK37
#include "smk_input.h"        /* the SMK-37 panel: velocity keybed, pads, faders, wheels (same API as fm1_input.h) */
#include "smk_rgb.h"          /* the SMK-37 pad LEDs: SPI1, after a frame (never from the scan ISR) */
#else
#include "fm1_input.h"        /* the FM-1 panel */
#endif
#include "fm1_timer.h"
#include "fm1_audio.h"
#if BOARD_SMK37
/* smk_input.h brings its own: the battery on mux C, MASTER as the virtual K1 (the SMK has no pot) */
#else
#include "fm1_adc.h"
#endif
#include "fm1_lcd_hw.h"
#if BOARD_SMK37
/* BEFORE fm1_flash.h, which defines FL_RANGE_OK under #ifndef: a map included after it could only
 * redefine the macro while the RAM driver's own erase and program kept the FM-1's net. Included
 * first, the driver's net is the SMK's (FL_DATA 0x9D000..0xBB000 and FL_SMP 0xC2000..0xEA000,
 * never BTIF/USRTRIM at 0xC0000..0xC1FFF nor key_mac above 0xFF000), and fm1_flash.h leaves it be. */
#include "fm1_flash_map.h"
#endif
#include "fm1_flash.h"
#include "fm1_xip.h"
#include "fm1_perf.h"

#include "libc.c"
#include "lcd.c"
#include "gfx.c"

static volatile uint32_t fm1_ms;  /* milliseconds since boot (TIMER4-based, TIMER5 ISR) */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

#include "app/plat.h"

#define FELUCCA_OTA 1             /* M-UPGRADE update entry: keeps the web installer working */
#ifndef EN_CDC
#define EN_CDC 0                  /* USB serial console off: one plain MIDI interface */
#endif
#define FELUCCA_CDC EN_CDC
#ifndef EN_UAC
#define EN_UAC 1                  /* USB audio input: the master output, recordable over the cable */
#endif
#define FELUCCA_UAC EN_UAC
#ifndef FELUCCA_ID
#define FELUCCA_ID "FM-1_700"     /* package identity (tools/build.py) */
#endif
#include "usb.c"
#ifndef EN_TRS
#define EN_TRS 1                  /* MIDI IN on the TRS jack */
#endif
#if EN_TRS
#include "midi_uart.c"
#endif

static uint8_t flash_ok;          /* JEDEC id matched at boot */
static void audio_silence(void);
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static int st_erase(uint32_t off)
{
    uint32_t took;
    if (!FL_STORE_OK(off, 0x1000u))
        return -8;
    audio_silence();
    return fl_erase4k(off, &took);
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (!FL_STORE_OK(off, n))
        return -8;
    return fl_write(off, src, n);
}
#include "storage.c"

struct bootguard_s { uint32_t magic, failed, pending; };
extern struct bootguard_s bootguard;
#include "ota.c"
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { fm1_wdt_feed(); }
static int ota_in_area(uint32_t off, uint32_t n) { return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN); }
static int ota_erase(uint32_t off)
{
    uint32_t took;
    if (!ota_in_area(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    audio_silence();
    return fl_erase4k(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!ota_in_area(off, n))
        return -8;
    return fl_write(off, p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }
static void ota_show(uint32_t step, int32_t code)
{
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "RESTART"};
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 92, 240, &FONT_S, "UPDATE", C_WHITE, 1);
    if (step < 9u) {
        draw_text_box(0, 124, 240, &FONT_S, STEP[step < 6u ? step : 0], C_HI, 1);
        return;
    }
    draw_text_box(0, 124, 240, &FONT_S, code == 1 ? "DRY RUN OK" : "FAILED", C_HI, 1);
    fm1_delay_ms(1500);
}
static void ota_commit(const uint8_t *parm)
{
    bootguard.pending = 0;                              /* intentional reset */
    bootguard.failed = 0;                               /* a new install starts with a clean slate */
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);
}

#include "app/app.h"
#define ANN_NAME "NACHTFAHRT " EN_VERSION
#include "ann_words.h"            /* the link to AMB-1's words file (tools/gen_words.py, from firmware/words.tsv) */
#include "announce.c"             /* the screen as text for a screen reader (fm1-reader) */
static int ann_wire_send(const uint8_t *p, uint32_t n)   /* never waits: no room (nobody reading), not sent */
{
    if (!usb.config || usb.suspended || SXQ - (so_w - so_r) < (n + 2u) / 3u)
        return -1;
    return ota_wire_send(p, n);
}
#include "app/panel.c"
#include "app/plat_fm1.c"
#include "app/project.c"
#include "app/ui.c"
#if BOARD_SMK37
#include "app/smk_pads.h"       /* the SMK-37's pads (the FM-1's black keys) and their red/green LEDs */
#include "app/smk_controls.h"   /* the SMK-37's faders, wheels and outer keys, over ui.c's own handlers */
#endif
#include "app/main_fm1.c"
