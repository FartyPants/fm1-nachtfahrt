/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* LCD wiring, per board (fm1_cc.h BOARD_SMK37). The panel protocol is src/lcd.c and is the same
 * for both: only the pins and the backlight differ.
 *
 *   FM-1 (0)     SPI1 on PC9 CLK, PC10 DO, PC7 CS, PC8 D/C; backlight PA2, active low
 *   SMK-37 (1)   SPI1 on PB6 CLK, PB7 DO, PC8 CS, PB5 D/C; no backlight control at all
 *                (ST7789V, 240x240 IPS, as the stock firmware drives it)
 *
 *   fm1_lcd_hw_init()          pins, backlight on, SPI1 master at BAUD 4
 *   fm1_lcd_baud(b)            SPI1 clock = lsb / (b + 1)
 *   fm1_lcd_send_cmd(c)        D/C low, CS low, one byte (then fm1_lcd_wait)
 *   fm1_lcd_send_data(p, n)    D/C high, CS low, DMA n bytes from RAM p
 *   fm1_lcd_wait()             SPI done (or timeout, counted), pending cleared
 *   fm1_lcd_deselect()         CS high
 */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#define FM1_LCD_PC_OUT (*(volatile uint32_t *)0x50080u)
#define FM1_LCD_PC_DIR (*(volatile uint32_t *)0x50088u)
#define FM1_LCD_IOMAP_CON1 (*(volatile uint32_t *)0x51020u)
#if BOARD_SMK37
/* The SMK-37's display runs on the controller's SECOND SPI (0x11E00), not the FM-1's first one. Read
 * out of the image the stock firmware's own init writes, and out of smk37_open's lcd.inc. */
#define FM1_LCD_SPI_CON (*(volatile uint32_t *)0x11E00u)
#define FM1_LCD_SPI_BAUD (*(volatile uint32_t *)0x11E04u)
#define FM1_LCD_SPI_BUF (*(volatile uint32_t *)0x11E08u)
#define FM1_LCD_SPI_ADR (*(volatile uint32_t *)0x11E0Cu)
#define FM1_LCD_SPI_CNT (*(volatile uint32_t *)0x11E10u)
#else
#define FM1_LCD_SPI_CON (*(volatile uint32_t *)0x11D00u)
#define FM1_LCD_SPI_BAUD (*(volatile uint32_t *)0x11D04u)
#define FM1_LCD_SPI_BUF (*(volatile uint32_t *)0x11D08u)
#define FM1_LCD_SPI_ADR (*(volatile uint32_t *)0x11D0Cu)
#define FM1_LCD_SPI_CNT (*(volatile uint32_t *)0x11D10u)
#endif
#define FM1_LCD_PA_OUT (*(volatile uint32_t *)0x50000u)
#define FM1_LCD_PA_DIR (*(volatile uint32_t *)0x50008u)
#if BOARD_SMK37
/* PC8 is the chip select on both boards (first verified on a unit, 2026-10-09, as the stock firmware
 * drives it); the data and the clock moved to port B, and the panel has no backlight line. */
#define FM1_LCD_PB_OUT (*(volatile uint32_t *)0x50040u)     /* (fm1_gpio.h: port B is 0x50040) */
#define FM1_LCD_PB_DIR (*(volatile uint32_t *)0x50048u)
#define FM1_LCD_CS (1u << 8)
#define FM1_LCD_DC (1u << 5)
#define FM1_LCD_CLK (1u << 6)
#define FM1_LCD_DO (1u << 7)
#else
#define FM1_LCD_BL (1u << 2)
#define FM1_LCD_CS (1u << 7)
#define FM1_LCD_DC (1u << 8)
#define FM1_LCD_CLK (1u << 9)
#define FM1_LCD_DO (1u << 10)
#endif

static uint32_t fm1_lcd_timeouts;


FM1_INLINE void fm1_lcd_hw_init(void)
{
#if BOARD_SMK37
    /* IOMAP_CON1 bit 4 CLEARED: the SPI pins belong to the controller, and the same bit is what the
     * RGB pad LEDs' data route (PC10, SPI1) is switched by (smk37_hw.h in the stock firmware). */
    FM1_LCD_IOMAP_CON1 &= ~0x10u;
    FM1_LCD_PC_OUT |= FM1_LCD_CS;
    FM1_LCD_PC_DIR &= ~FM1_LCD_CS;
    FM1_LCD_PB_OUT &= ~(FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
    FM1_LCD_PB_DIR &= ~(FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
#else
    FM1_LCD_PA_OUT &= ~FM1_LCD_BL;
    FM1_LCD_PA_DIR &= ~FM1_LCD_BL;
    FM1_LCD_IOMAP_CON1 |= 0x10u;
    FM1_LCD_PC_OUT |= FM1_LCD_CS;
    FM1_LCD_PC_OUT &= ~(FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
    FM1_LCD_PC_DIR &= ~(FM1_LCD_CS | FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
#endif
    FM1_LCD_SPI_CON = 0x4021u;
    FM1_LCD_SPI_BAUD = 4u;
}

FM1_INLINE void fm1_lcd_baud(uint32_t b) { FM1_LCD_SPI_BAUD = b; }

static void fm1_lcd_wait(void)
{
    uint32_t n;
    for (n = 0; n < 4000000u && !(FM1_LCD_SPI_CON & 0x8000u); n++)
        ;
    if (n == 4000000u)
        fm1_lcd_timeouts++;
    FM1_LCD_SPI_CON |= 0x4000u;
}

FM1_INLINE void fm1_lcd_deselect(void) { FM1_LCD_PC_OUT |= FM1_LCD_CS; }

#if BOARD_SMK37
#define FM1_LCD_DC_LOW() (FM1_LCD_PB_OUT &= ~FM1_LCD_DC)
#define FM1_LCD_DC_HIGH() (FM1_LCD_PB_OUT |= FM1_LCD_DC)
#else
#define FM1_LCD_DC_LOW() (FM1_LCD_PC_OUT &= ~FM1_LCD_DC)
#define FM1_LCD_DC_HIGH() (FM1_LCD_PC_OUT |= FM1_LCD_DC)
#endif

FM1_INLINE void fm1_lcd_send_cmd(uint8_t c)
{
    FM1_LCD_DC_LOW();
    FM1_LCD_PC_OUT &= ~FM1_LCD_CS;
    FM1_LCD_SPI_CON |= 0x4000u;
    FM1_LCD_SPI_BUF = c;
}

FM1_INLINE void fm1_lcd_send_data(const void *p, uint32_t n)
{
    FM1_LCD_DC_HIGH();
    FM1_LCD_PC_OUT &= ~FM1_LCD_CS;
    FM1_LCD_SPI_CON |= 0x4000u;
    FM1_LCD_SPI_ADR = (uint32_t)(uintptr_t)p;
    FM1_LCD_SPI_CNT = n;
}
