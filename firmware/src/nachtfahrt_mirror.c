/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Patrick F. Kellogg
 *
 * The FM-1 Big Screen mirror in NACHTFAHRT, for players who can't read the 1.54-inch screen: while an app asks
 * (F0 7D 'F' 'S' 00/01 F7, about once a second), every rectangle sent to the LCD (lcd.c lcd_blit / lcd_fill) also
 * goes out over USB MIDI SysEx, so FM-1 Mirror (github.com/pfkellogg/fm1-mirror) and FM-1 Mirror + Spyglass show the
 * screen big. screen_mirror.c makes the messages; this is the glue: the hello from usb.c sysex_byte, sending through
 * usb.c's SysEx ring without ever waiting on it, the main loop's poll, and the piece playing as the sound's name
 * (F0 7D 'F' 'S' 02 <name> F7). With nobody asking it does nothing. USB only.
 */
#define SM_BUFFER 6144u                          /* 6 KB of queued messages */
#include "screen_mirror.c"

static uint16_t mirror_row[240];                 /* one row, back in RGB565 order (the LCD's buffers are byte-swapped) */
static uint8_t mirror_lost;                      /* rows dropped: the whole screen again when there's room */
static uint32_t mirror_full_at;                  /* the last whole-screen redraw asked for (at most twice a second) */

/* One SysEx message into usb.c's ring, or -2 (busy) when it has no room for all of it: never waits */
static int mirror_send(const uint8_t *m, unsigned n)
{
    if (!usb.config)
        return -1;
    if (SXQ - (so_w - so_r) < (n + 2u) / 3u + 1u)
        return -2;
    return ota_wire_send(m, n) ? -1 : 0;
}

/* lcd.c: a rectangle about to go to the LCD; px (byte-swapped, stride pixels a row) or, for a fill, NULL and the
 * colour c */
static void mirror_lcd(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride, uint16_t c)
{
    uint32_t r, i, start = fm1_ms;
    if (!sm_on() || mirror_lost)
        return;
    for (r = 0; r < h; r++) {
        uint32_t t0 = fm1_ms;
        if (fm1_ms - start > 400u) {                   /* the host isn't keeping up: never hold the screen longer */
            mirror_lost = 1;
            return;
        }
        for (i = 0; i < w; i++)
            mirror_row[i] = px ? (uint16_t)((px[r * stride + i] >> 8) | (px[r * stride + i] << 8)) : c;
        while (!sm_rows(mirror_row, x, y + r, w, 1)) {   /* full: send some, briefly; else the whole screen later */
            sm_poll(fm1_ms, mirror_send);
            if (fm1_ms - t0 > 60u) {
                mirror_lost = 1;
                return;
            }
        }
    }
}

/* main loop: send what's queued; a new viewer (or rows dropped) gets the whole screen redrawn; the piece's name to
 * anyone asking (looked at every 100 ms: sm_name sends it only after a hello or a change) */
static void mirror_service(void)
{
    static uint32_t named_at;
    sm_poll(fm1_ms, mirror_send);
    if (fm1_ms - named_at >= 100u) {
        char b[40];
        const char *p = piece_name(proj.piece);
        unsigned i;
        named_at = fm1_ms;
        for (i = 0; p && p[i] && i < sizeof b - 1u; i++)
            b[i] = p[i];
        b[i] = 0;
        sm_name(b, fm1_ms, mirror_send);
    }
    if (sm_take_full_request() || (mirror_lost && sm_on() && fm1_ms - mirror_full_at >= 500u)) {
        mirror_lost = 0;
        mirror_full_at = fm1_ms;
        ui.sig[0] = ui.sig[1] = ui.sig[2] = 0x5343524eu;   /* every band's "what it shows" unknown: all drawn again */
    }
}
