/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 Martin Butigan */
/* The SMK-37's 16 pad LEDs: one RGB LED under each pad, driven by the same 74HC595 chain as the button
 * LEDs, from the FIRST SPI (0x11D00) - the display's, which is idle between frames.
 *
 * Stock reference (smk37_open, GPL-3.0-linking-exception): board/panel.c smk37_rgb_service(). The frame is
 * 420 bytes = 105 words; the 16 LEDs' colour words start at byte 36, one every 24 bytes, in the order of
 * rgb_map; after each 24-bit colour comes a nibble-expanded word (12 nibbles -> 48 bits, 6 words). A colour
 * is sent as GRB, MSB first. The DMA goes out with PC8 (the display's CS) high, IOMAP_CON1 bit 4 set (the
 * LED data route on PC10) and BAUD 8, then everything is put back for the display.
 *
 * Kept apart from smk_input.h on purpose: that header is the scan, which runs from the 10 kHz ISR and must
 * never touch SPI1; this one is called from the main loop, after a frame has been drawn. */
#pragma once
#include <stdint.h>
#include "fm1_lcd_hw.h"

#define SMK_RGB_SPI_CON (*(volatile uint32_t *)0x11D00u)
#define SMK_RGB_SPI_BAUD (*(volatile uint32_t *)0x11D04u)
#define SMK_RGB_SPI_ADR (*(volatile uint32_t *)0x11D0Cu)
#define SMK_RGB_SPI_CNT (*(volatile uint32_t *)0x11D10u)
#define SMK_RGB_IOMAP_CON1 (*(volatile uint32_t *)0x51020u)
#define SMK_RGB_PC_OUT (*(volatile uint32_t *)0x50080u)
#define SMK_RGB_CS (1u << 8)                    /* PC8: the display's chip select, high while the LEDs go out */

static uint32_t smk_rgb_col[16];                /* what the LEDs should show (0xRRGGBB) */
static uint32_t smk_rgb_dirty;                  /* bit i: LED i changed since the last frame */
static uint8_t smk_rgb_buf[420] __attribute__((aligned(4)));

/* the stock's pad order: hardware pad i -> the LED i of the frame */
static const uint8_t SMK_RGB_MAP[16] = {0, 15, 14, 13, 12, 11, 10, 9, 1, 2, 3, 4, 5, 6, 7, 8};

/* 12 nibbles of a word, each doubled into a byte (stock: nibble[]); the colour's 24 bits are sent six
 * nibbles at a time, so one colour = six of these */
static const uint32_t SMK_RGB_NIB[16] = {
    0xc0c0c0c0u, 0xf0c0c0c0u, 0xc0f0c0c0u, 0xf0f0c0c0u, 0xc0c0f0c0u, 0xf0c0f0c0u, 0xc0f0f0c0u, 0xf0f0f0c0u,
    0xc0c0c0f0u, 0xf0c0c0f0u, 0xc0f0c0f0u, 0xf0f0c0f0u, 0xc0c0f0f0u, 0xf0c0f0f0u, 0xc0f0f0f0u, 0xf0f0f0f0u,
};

/* (no libc in the firmware's headers: the buffer starts zeroed and every word is written by service) */
static void smk_rgb_init(void) { smk_rgb_dirty = 0xFFFFu; }

static void smk_rgb_set(uint32_t id, uint32_t rgb)
{
    if (id < 16u && smk_rgb_col[id] != rgb) {
        smk_rgb_col[id] = rgb;
        smk_rgb_dirty |= 1u << id;
    }
}

/* send what changed; call it from the main loop, never from the ISR (the display's SPI1 is shared) */
static void smk_rgb_service(void)
{
    uint32_t i, n, errs;
    if (!smk_rgb_dirty)
        return;
    for (i = 0; i < 16u; i++) {
        uint32_t rgb, grb;
        if (!(smk_rgb_dirty & (1u << i)))
            continue;
        rgb = smk_rgb_col[i];
        grb = ((rgb & 0xFF00u) << 8) | ((rgb >> 8) & 0xFF00u) | (rgb & 0xFFu);   /* the LEDs take GRB */
        for (n = 0; n < 6u; n++) {
            uint32_t w = SMK_RGB_NIB[(grb >> (20u - 4u * n)) & 15u], o = 36u + 24u * SMK_RGB_MAP[i] + 4u * n;
            smk_rgb_buf[o] = (uint8_t)w;
            smk_rgb_buf[o + 1u] = (uint8_t)(w >> 8);
            smk_rgb_buf[o + 2u] = (uint8_t)(w >> 16);
            smk_rgb_buf[o + 3u] = (uint8_t)(w >> 24);
        }
    }
    /* one owner of SPI1: the display's transfer finished, CS high while the LEDs shift out, then back */
    fm1_lcd_wait();
    SMK_RGB_PC_OUT |= SMK_RGB_CS;
    SMK_RGB_IOMAP_CON1 |= 0x10u;                  /* the LED data route (PC10) */
    SMK_RGB_SPI_BAUD = 8u;
    SMK_RGB_SPI_CON |= 0x4000u;
    __asm__ volatile("csync" ::: "memory");
    SMK_RGB_SPI_ADR = (uint32_t)(uintptr_t)smk_rgb_buf;
    SMK_RGB_SPI_CNT = sizeof smk_rgb_buf;
    errs = fm1_lcd_timeouts;
    fm1_lcd_wait();
    SMK_RGB_IOMAP_CON1 &= ~0x10u;                 /* (the display's pins again) */
    SMK_RGB_SPI_BAUD = 4u;
    if (errs == fm1_lcd_timeouts)                 /* (a timeout: sent again next frame) */
        smk_rgb_dirty = 0;
}
