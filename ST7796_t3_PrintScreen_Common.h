/*
The MIT License (MIT)

library writen by Kris Kasprzak

Shared helpers for ST7796_t3_PrintScreen_SdFat.h

WHY THIS FILE EXISTS
ST7735_t3::readPixel() / readRect() do NOT read from the display. They only copy
from the optional frame buffer (_pfbtft). If useFrameBuffer(true) was never called
that pointer is NULL and readPixel returns garbage / 0 (or faults).

So we support two ways of getting pixels:
1. Frame buffer mode   - if Display.useFrameBuffer(true) is active we read the
                         frame buffer via readRect (fast, no extra wiring)
2. Hardware read       - otherwise we send RAMRD (0x2E) to the ST7796 and clock the
                         pixel data back on MISO. This REQUIRES:
                           - the display SDO/MISO pin wired to the MCU MISO pin
                           - TFT DC on a normal GPIO (on Teensy 4.x NOT pin 10/36/37,
                             which ST7735_t3 drives as hardware chip select)
                           - nothing else driving MISO while the TFT CS is low
                             (some modules' touch controllers do not release MISO)
*/

#ifndef ST7796_t3_PRINTSCREEN_COMMON_H
#define ST7796_t3_PRINTSCREEN_COMMON_H

#include <Arduino.h>
#include <SPI.h>
#include <ST7796_t3.h>

// ST7796S read cycle is much slower than write; keep reads at a conservative clock
#ifndef ST7796_t3_PS_READ_SPEED
#define ST7796_t3_PS_READ_SPEED 6000000
#endif

// largest width we will ever see (480 in landscape)
#define ST7796_t3_PS_MAX_WIDTH 480

#define ST7796_t3_PS_CASET 0x2A
#define ST7796_t3_PS_RASET 0x2B
#define ST7796_t3_PS_RAMRD 0x2E

// convert a 565 color to 8-bit components written in BMP order (B, G, R)
inline void ST7796_t3_PS_565toBGR(uint16_t c, uint8_t *bgr) {
	uint8_t r = (c >> 11) & 0x1F;
	uint8_t g = (c >> 5) & 0x3F;
	uint8_t b = c & 0x1F;
	bgr[0] = (b << 3) | (b >> 2);
	bgr[1] = (g << 2) | (g >> 4);
	bgr[2] = (r << 3) | (r >> 2);
}

// read one display row straight from the ST7796 GRAM, output is BMP order (B, G, R)
inline void ST7796_t3_PS_ReadRowHW(SPIClass &spi, uint8_t tftCS, uint8_t tftDC,
                                uint16_t y, uint16_t w, uint8_t *bgr) {

	spi.beginTransaction(SPISettings(ST7796_t3_PS_READ_SPEED, MSBFIRST, SPI_MODE0));
	digitalWrite(tftCS, LOW);

	digitalWrite(tftDC, LOW);  spi.transfer(ST7796_t3_PS_CASET);
	digitalWrite(tftDC, HIGH); spi.transfer16(0); spi.transfer16(w - 1);

	digitalWrite(tftDC, LOW);  spi.transfer(ST7796_t3_PS_RASET);
	digitalWrite(tftDC, HIGH); spi.transfer16(y); spi.transfer16(y);

	digitalWrite(tftDC, LOW);  spi.transfer(ST7796_t3_PS_RAMRD);
	digitalWrite(tftDC, HIGH);

	// first byte after RAMRD is a dummy
	spi.transfer(0x00);

	// unlike the ILI9341 (always 18 bit reads), the ST7796 returns pixels in the
	// interface format set by COLMOD. ST7735_t3 sets COLMOD = 0x55 (16 bit), so each
	// pixel comes back as 2 bytes, RGB565, MSB first (verified against a capture)
	for (uint16_t i = 0; i < w; i++) {
		uint16_t c = spi.transfer16(0x0000);
		ST7796_t3_PS_565toBGR(c, bgr);
		bgr += 3;
	}

	digitalWrite(tftCS, HIGH);
	spi.endTransaction();
}

// read one row from the frame buffer, output is BMP order (B, G, R)
inline void ST7796_t3_PS_ReadRowFB(ST7796_t3 *d, uint16_t y, uint16_t w, uint8_t *bgr) {
	static uint16_t colors[ST7796_t3_PS_MAX_WIDTH];
	d->readRect(0, y, w, 1, colors);
	for (uint16_t i = 0; i < w; i++) {
		ST7796_t3_PS_565toBGR(colors[i], bgr);
		bgr += 3;
	}
}

// fill the 54 byte BMP header for a 24-bit bottom-up image
inline void ST7796_t3_PS_BuildHeader(uint8_t *hdr, int32_t ww, int32_t hh) {
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
