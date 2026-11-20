/*
The MIT License (MIT)

library writen by Kris Kasprzak

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

On a personal note, if you develop an application or product using this library
and make millions of dollars, I'm happy for you!

*/

#ifndef ST7796_t3_PRINTSCREEN_SDFAT_H
#define ST7796_t3_PRINTSCREEN_SDFAT_H

#include <Arduino.h>
#include <SPI.h>
#include <ST7796_t3.h>

#include <SdFat.h>

#include "ST7796_t3_PrintScreen_Common.h"

#define PS_FILE_T            SdFile
#define PS_BEGIN(cs)         SdFat sd; sd.begin(cs, SD_SCK_MHZ(20))
#define PS_OPEN_WRITE(f, n)  f.open(n, O_WRONLY | O_CREAT | O_TRUNC)
#define PS_OPEN_READ(f, n)   f.open(n, O_RDONLY)
#define PS_IS_OPEN(f)        f.isOpen()
#define PS_POSITION(f)       f.curPosition()
#define PS_SEEK(f, p)        f.seekSet(p)

/*
code to capture the screen and save it to a 24 bit BMP file

you will need to pass in:
1. your display object (ST7796_t3)
2. the TFT chip select pin
3. the TFT DC pin
4. the SD chip select pin
5. filename
6. optional SPI bus the display is on (default SPI)

If the display is in frame buffer mode (Display.useFrameBuffer(true)) the frame
buffer is saved. Otherwise the pixels are read back from the ST7796 over MISO
(see ST7796_t3_PrintScreen_Common.h for the wiring requirements).

Note: ST7735_t3::readPixel() only reads the frame buffer, that is why this
library does not call it when no frame buffer is in use.
*/

bool inline SaveBMP24(ST7796_t3 *d, uint8_t tftCS, uint8_t tftDC, int sdCS,
                      const char *FileName, SPIClass &spi = SPI) {

	static uint8_t rowData[ST7796_t3_PS_MAX_WIDTH * 3 + 3];	// +3 for row padding (stays 0)
	uint8_t  hdr[54];
	PS_FILE_T dataFile;
	int16_t  ww = d->width();
	int16_t  hh = d->height();
	uint32_t rowSize = (ww * 3 + 3) & ~3;
	bool     useFB = (d->getFrameBuffer() != NULL);

	if (ww > ST7796_t3_PS_MAX_WIDTH) {
		return false;
	}

	// it may look odd not error trapping SD begin
	// but if the user already has a created instance this call will fail
	// if SD really does fail, we'll catch it in the open statement
	PS_BEGIN(sdCS);

	PS_OPEN_WRITE(dataFile, FileName);
	if (!PS_IS_OPEN(dataFile)) {
		return false;
	}

	ST7796_t3_PS_BuildHeader(hdr, ww, hh);
	dataFile.write(hdr, sizeof(hdr));

	// BMP is stored bottom row first
	// read a full row from the display, then write it to the card. The display
	// CS is released before the SD card is touched (they usually share the bus)
	for (int16_t y = hh - 1; y >= 0; y--) {
		if (useFB) {
			ST7796_t3_PS_ReadRowFB(d, y, ww, rowData);
		}
		else {
			ST7796_t3_PS_ReadRowHW(spi, tftCS, tftDC, y, ww, rowData);
		}
		dataFile.write(rowData, rowSize);
	}

	dataFile.close();
	delay(10);
	return true;
}

// convenience version for frame buffer users (no TFT pins needed)
bool inline SaveBMP24(ST7796_t3 *d, int sdCS, const char *FileName) {
	if (d->getFrameBuffer() == NULL) {
		Serial.println("SaveBMP24: no frame buffer, call SaveBMP24(&Display, TFT_CS, TFT_DC, SD_CS, name)");
		return false;
	}
	return SaveBMP24(d, 0, 0, sdCS, FileName);
}


/*

code adapted from Teensy forum user: socalpinplayer
code has been combined in to a single inline function call

you will need to pass in:
1. your display object (ST7796_t3)
2. chip select pin
3. filename
4. and optional x and y for image location

*/

bool inline DrawBMP24(ST7796_t3 *d, int cs, const char *FileName, uint16_t x = 0, uint16_t y = 0) {

	PS_FILE_T bmpFile;
	uint8_t  hdr[54];
	int32_t  bmpWidth, bmpHeight;
	int      w, h, row, col;
	uint32_t bmpImageoffset;
	uint32_t rowSize;
	uint8_t  sdbuffer[3 * 16];				// pixel buffer (B+G+R per pixel)
	uint8_t  buffidx = sizeof(sdbuffer);
	bool     flip = true;					// BMP is stored bottom-to-top
	uint32_t pos;
	static uint16_t awColors[ST7796_t3_PS_MAX_WIDTH];	// one row (480 in landscape)

	if ((x >= d->width()) || (y >= d->height())) {
		return false;
	}

	PS_BEGIN(cs);

	PS_OPEN_READ(bmpFile, FileName);
	if (!PS_IS_OPEN(bmpFile)) {
		return false;
	}

	if (bmpFile.read(hdr, sizeof(hdr)) != (int)sizeof(hdr) || hdr[0] != 'B' || hdr[1] != 'M') {
		bmpFile.close();
		return false;
	}

	bmpImageoffset = (uint32_t)hdr[10] | ((uint32_t)hdr[11] << 8) | ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
	bmpWidth  = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19] << 8) | ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24));
	bmpHeight = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8) | ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24));
	uint16_t planes      = hdr[26] | (hdr[27] << 8);
	uint16_t depth       = hdr[28] | (hdr[29] << 8);
	uint32_t compression = (uint32_t)hdr[30] | ((uint32_t)hdr[31] << 8) | ((uint32_t)hdr[32] << 16) | ((uint32_t)hdr[33] << 24);

	// only uncompressed 24 bit files are supported
	if (planes != 1 || depth != 24 || compression != 0) {
		bmpFile.close();
		return false;
	}

	rowSize = (bmpWidth * 3 + 3) & ~3;

	if (bmpHeight < 0) {
		bmpHeight = -bmpHeight;
		flip = false;
	}

	w = bmpWidth;
	h = bmpHeight;
	if ((x + w - 1) >= d->width())  w = d->width()  - x;
	if ((y + h - 1) >= d->height()) h = d->height() - y;

	for (row = 0; row < h; row++) {
		if (flip) pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
		else      pos = bmpImageoffset + row * rowSize;

		if (PS_POSITION(bmpFile) != pos) {
			PS_SEEK(bmpFile, pos);
			buffidx = sizeof(sdbuffer);		// force buffer reload
		}

		for (col = 0; col < w; col++) {
			if (buffidx >= sizeof(sdbuffer)) {
				bmpFile.read(sdbuffer, sizeof(sdbuffer));
				buffidx = 0;
			}
			uint8_t b = sdbuffer[buffidx++];
			uint8_t g = sdbuffer[buffidx++];
			uint8_t r = sdbuffer[buffidx++];
			// note: ST7735_t3::Color565() packs blue in the high bits (swaps R/B), so do it here
			awColors[col] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
		}

		d->writeRect(x, y + row, w, 1, awColors);
	}

	bmpFile.close();
	return true;
}

#undef PS_FILE_T
#undef PS_BEGIN
#undef PS_OPEN_WRITE
#undef PS_OPEN_READ
#undef PS_IS_OPEN
#undef PS_POSITION
#undef PS_SEEK

#endif
