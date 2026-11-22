/*
The MIT License (MIT)

library writen by Kris Kasprzak

Shared helpers for ST77xx_t3_PrintScreen_SdFat.h

Supports the three displays in the ST7735_t3 driver library:
  ST7735_t3   (ST7735 / ST7735S, 128x160, 128x128, 80x160)
  ST7789_t3   (ST7789,  240x240, 135x240, 240x320, ...)
  ST7796_t3   (ST7796S, 320x480)

WHY THIS FILE EXISTS
ST7735_t3::readPixel() / readRect() do NOT read from the display. They only copy
from the optional frame buffer (_pfbtft). If useFrameBuffer(true) was never called
that pointer is NULL and readPixel returns garbage / 0 (or faults).

So we support two ways of getting pixels:
1. Frame buffer mode   - if Display.useFrameBuffer(true) is active we read the
                         frame buffer via readRect (fast, no extra wiring, works
                         the same on all three displays)
2. Hardware read       - otherwise we send RAMRD (0x2E) to the controller and clock
                         the pixel data back on MISO. This REQUIRES:
                           - the display SDO/MISO pin wired to the MCU MISO pin
                             (the ST7735 has no SDO pin, see README)
                           - TFT DC on a normal GPIO (on Teensy 4.x NOT pin 10/36/37,
                             which ST7735_t3 drives as hardware chip select)
                           - nothing else driving MISO while the TFT CS is low
                             (some modules' touch controllers do not release MISO)

READ FORMATS (from the Sitronix datasheets)
  ST7796S  - pixels are read in the interface format set by COLMOD. ST7735_t3 sets
             16 bit, so each pixel comes back as 2 bytes RGB565
  ST7735 / ST7789 - "the data color coding is fixed to 18-bit in reading function"
             and COLMOD "should be set to 66h when reading pixel data from frame
             memory". Each pixel comes back as 3 bytes (R, G, B), 6 bits each in
             the upper bits. We set COLMOD = 0x66 for the read and restore the
             driver's 16 bit setting afterwards

GRAM OFFSETS
Many ST7735 and ST7789 panels are smaller than the controller's memory, so the
driver adds an offset (_xstart / _ystart, which also change with rotation) to every
address window. The hardware read must use the same offsets or it reads the wrong
part of memory. Those members are protected, so we read them through a derived
class accessor (no driver changes needed).
*/

#ifndef ST77xx_t3_PRINTSCREEN_COMMON_H
#define ST77xx_t3_PRINTSCREEN_COMMON_H

#include <Arduino.h>
#include <SPI.h>
#include <ST7735_t3.h>
#include <ST7789_t3.h>
#include <ST7796_t3.h>

// read cycles are much slower than writes (all three datasheets: 150 ns minimum
// read clock period = 6.6 MHz), keep reads at a conservative clock
#ifndef ST77xx_t3_PS_READ_SPEED
#define ST77xx_t3_PS_READ_SPEED 6000000
#endif

// largest width we will ever see (ST7796 480 in landscape)
#define ST77xx_t3_PS_MAX_WIDTH 480

#define ST77xx_t3_PS_CASET  0x2A
#define ST77xx_t3_PS_RASET  0x2B
#define ST77xx_t3_PS_RAMRD  0x2E
#define ST77xx_t3_PS_COLMOD 0x3A

// how the controller sends pixels back on a RAMRD
#define ST77xx_t3_PS_READ_565 0		// 2 bytes per pixel (ST7796)
#define ST77xx_t3_PS_READ_666 1		// 3 bytes per pixel (ST7735, ST7789)

// COLMOD values the driver uses for normal 16 bit writes (restored after a read)
#define ST77xx_t3_PS_COLMOD_ST7735 0x05
#define ST77xx_t3_PS_COLMOD_ST7789 0x55
#define ST77xx_t3_PS_COLMOD_READ   0x66

// read access to the driver's protected, rotation adjusted window offsets.
// taking the member address through a derived class is legal C++ and gives a
// pointer to the ST7735_t3 member, which works on all three display classes.
// this class is never instantiated
struct ST77xx_t3_PS_Access : public ST7735_t3 {
	static uint16_t xStart(ST7735_t3 *d) { return d->*(&ST77xx_t3_PS_Access::_xstart); }
	static uint16_t yStart(ST7735_t3 *d) { return d->*(&ST77xx_t3_PS_Access::_ystart); }
};

// convert a 565 color to 8-bit components written in BMP order (B, G, R)
inline void ST77xx_t3_PS_565toBGR(uint16_t c, uint8_t *bgr) {
	uint8_t r = (c >> 11) & 0x1F;
	uint8_t g = (c >> 5) & 0x3F;
	uint8_t b = c & 0x1F;
	bgr[0] = (b << 3) | (b >> 2);
	bgr[1] = (g << 2) | (g >> 4);
	bgr[2] = (r << 3) | (r >> 2);
}

// expand a 6 bit component (sent in the upper 6 bits of a byte) to 8 bits
inline uint8_t ST77xx_t3_PS_6to8(uint8_t v) {
	v >>= 2;
	return (v << 2) | (v >> 4);
}

inline void ST77xx_t3_PS_WriteCommand(SPIClass &spi, uint8_t tftDC, uint8_t cmd) {
	digitalWrite(tftDC, LOW);
	spi.transfer(cmd);
	digitalWrite(tftDC, HIGH);
}

// read one display row straight from the controller GRAM, output is BMP order (B, G, R)
// readFormat:  ST77xx_t3_PS_READ_565 or ST77xx_t3_PS_READ_666
// colmod:      COLMOD value to restore after an 18 bit read (ignored for 565)
inline void ST77xx_t3_PS_ReadRowHW(ST7735_t3 *d, SPIClass &spi, uint8_t tftCS, uint8_t tftDC,
                                   uint8_t readFormat, uint8_t colmod,
                                   uint16_t y, uint16_t w, uint8_t *bgr) {

	uint16_t x0 = ST77xx_t3_PS_Access::xStart(d);
	uint16_t y0 = y + ST77xx_t3_PS_Access::yStart(d);

	spi.beginTransaction(SPISettings(ST77xx_t3_PS_READ_SPEED, MSBFIRST, SPI_MODE0));
	digitalWrite(tftCS, LOW);

	if (readFormat == ST77xx_t3_PS_READ_666) {
		ST77xx_t3_PS_WriteCommand(spi, tftDC, ST77xx_t3_PS_COLMOD);
		spi.transfer(ST77xx_t3_PS_COLMOD_READ);
	}

	ST77xx_t3_PS_WriteCommand(spi, tftDC, ST77xx_t3_PS_CASET);
	spi.transfer16(x0); spi.transfer16(x0 + w - 1);

	ST77xx_t3_PS_WriteCommand(spi, tftDC, ST77xx_t3_PS_RASET);
	spi.transfer16(y0); spi.transfer16(y0);

	ST77xx_t3_PS_WriteCommand(spi, tftDC, ST77xx_t3_PS_RAMRD);

	// first byte after RAMRD is a dummy
	spi.transfer(0x00);

	if (readFormat == ST77xx_t3_PS_READ_666) {
		// 3 bytes per pixel: R, G, B (6 bits each, upper bits)
		for (uint16_t i = 0; i < w; i++) {
			uint8_t r = spi.transfer(0x00);
			uint8_t g = spi.transfer(0x00);
			uint8_t b = spi.transfer(0x00);
			bgr[0] = ST77xx_t3_PS_6to8(b);
			bgr[1] = ST77xx_t3_PS_6to8(g);
			bgr[2] = ST77xx_t3_PS_6to8(r);
			bgr += 3;
		}
		// any command ends the frame read; put the driver's 16 bit mode back
		ST77xx_t3_PS_WriteCommand(spi, tftDC, ST77xx_t3_PS_COLMOD);
		spi.transfer(colmod);
	}
	else {
		// 2 bytes per pixel, RGB565, MSB first (verified against an ST7796 capture)
		for (uint16_t i = 0; i < w; i++) {
			uint16_t c = spi.transfer16(0x0000);
			ST77xx_t3_PS_565toBGR(c, bgr);
			bgr += 3;
		}
	}

	digitalWrite(tftCS, HIGH);
	spi.endTransaction();
}

// read one row from the frame buffer, output is BMP order (B, G, R)
inline void ST77xx_t3_PS_ReadRowFB(ST7735_t3 *d, uint16_t y, uint16_t w, uint8_t *bgr) {
	static uint16_t colors[ST77xx_t3_PS_MAX_WIDTH];
	d->readRect(0, y, w, 1, colors);
	for (uint16_t i = 0; i < w; i++) {
		ST77xx_t3_PS_565toBGR(colors[i], bgr);
		bgr += 3;
	}
}

// fill the 54 byte BMP header for a 24-bit bottom-up image
inline void ST77xx_t3_PS_BuildHeader(uint8_t *hdr, int32_t ww, int32_t hh) {
	uint32_t rowSize  = (ww * 3 + 3) & ~3;
	uint32_t imgSize  = rowSize * hh;
	uint32_t fileSize = imgSize + 54;

	memset(hdr, 0, 54);
	hdr[0] = 'B'; hdr[1] = 'M';
	hdr[2] = fileSize; hdr[3] = fileSize >> 8; hdr[4] = fileSize >> 16; hdr[5] = fileSize >> 24;
	hdr[10] = 54;                      // pixel data offset
	hdr[14] = 40;                      // info header size
	hdr[18] = ww; hdr[19] = ww >> 8; hdr[20] = ww >> 16; hdr[21] = ww >> 24;
	hdr[22] = hh; hdr[23] = hh >> 8; hdr[24] = hh >> 16; hdr[25] = hh >> 24;
	hdr[26] = 1;                       // planes
	hdr[28] = 24;                      // bits per pixel
	hdr[34] = imgSize; hdr[35] = imgSize >> 8; hdr[36] = imgSize >> 16; hdr[37] = imgSize >> 24;
}

#endif
