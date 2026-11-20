# ST7796_t3_PrintScreen

Screen capture for **ST7796** displays driven by the **ST7735_t3** library on **Teensy**.
Save whatever is on the display to a 24-bit BMP file on an SD card, and draw BMP files back onto the display.

- `SaveBMP24()` saves the screen to a `.bmp` file (any rotation, up to 480 × 320)
- `DrawBMP24()` draws a 24-bit `.bmp` file onto the display, at an optional x, y position
- Works **with or without** a frame buffer
- Header-only, just include it

Tested on a Teensy 4.0 with a 4" 320 × 480 SPI ST7796 module.

---

## Why this library is needed

`ST7735_t3` has a `readPixel()` function, but it does **not** read from the display. Both `readPixel()` and `readRect()` only copy from the optional frame buffer. If you haven't called `useFrameBuffer(true)`, there is no frame buffer and `readPixel()` returns garbage or 0.

This library fixes that by getting pixels one of two ways:

| Mode | When it is used | Requirements |
|---|---|---|
| **Frame buffer** | `Display.useFrameBuffer(true)` is active | ~300 KB RAM (320 × 480 × 2), no extra wiring |
| **Hardware read** | No frame buffer | Display **SDO/MISO** wired to the Teensy MISO pin |

In hardware read mode the library reads the image straight back from the ST7796's memory over SPI (command `RAMRD 0x2E`).

---

## Installation

1. Download this repository (**Code → Download ZIP**) and unzip it.
2. Copy the folder to your Arduino `libraries` folder and name it `ST7796_t3_PrintScreen`.
3. Restart the Arduino IDE.

### Dependencies

| Library | Notes |
|---|---|
| [ST7735_t3](https://github.com/PaulStoffregen/ST7735_t3) | Display driver (provides `ST7796_t3`), ships with Teensyduino |
| SdFat | Ships with Teensyduino. You don't need to install it separately |
| SPI | Ships with Teensyduino |

> **SD library users:** Teensy's `SD` library is built on the copy of SdFat that ships with Teensyduino, so this library works even if your sketch only uses `SD.h`.

---

## Wiring

Display wiring is the same as for normal `ST7796_t3` use. For **hardware read mode** you also need MISO.

| Display pin | Teensy 4.0 | Notes |
|---|---|---|
| SCK | 13 | |
| SDI / MOSI | 11 | |
| **SDO / MISO** | **12** | **Required for hardware read mode** |
| CS | any GPIO (example uses 4) | |
| DC / RS | any GPIO **except 10, 36, 37** (example uses 5) | See note below |
| RST | any GPIO (example uses 6) | |
| SD card CS | any GPIO (example uses 7) | |

**DC pin note:** on Teensy 4.x, ST7735_t3 drives DC as a hardware SPI chip-select when it is on pin 10, 36 or 37. Hardware read mode needs DC on a normal GPIO pin. This doesn't apply in frame buffer mode.

**MISO note:** on some display modules the touch controller doesn't release MISO, which corrupts the read. If captures come back as noise, check what else is connected to MISO, or use frame buffer mode.

---

## Quick start

```cpp
#include <ST7735_t3.h>
#include <ST7796_t3.h>
#include <ST7796_t3_PrintScreen_SdFat.h>

#define TFT_CS   4
#define TFT_DC   5
#define TFT_RST  6
#define SD_CS    7

ST7796_t3 Display = ST7796_t3(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Display.init(320, 480);
  Display.setRotation(1);
  Display.fillScreen(ST7735_BLACK);
  Display.fillRect(50, 50, 100, 100, ST7735_RED);

  // save the screen
  SaveBMP24(&Display, TFT_CS, TFT_DC, SD_CS, "Screen.bmp");

  // ...later, draw it back
  DrawBMP24(&Display, SD_CS, "Screen.bmp");
}

void loop() {}
```

A complete sketch is in [`examples/PrintScreen_SdFat`](examples/PrintScreen_SdFat/PrintScreen_SdFat.ino).

---

## API

### `SaveBMP24` (any mode)

```cpp
bool SaveBMP24(ST7796_t3 *d, uint8_t tftCS, uint8_t tftDC, int sdCS,
               const char *FileName, SPIClass &spi = SPI);
```

Saves the current screen as a 24-bit, uncompressed BMP. Uses the frame buffer if one is active, otherwise reads the display over MISO.

| Parameter | Description |
|---|---|
| `d` | Pointer to your display object |
| `tftCS` | Display chip-select pin |
| `tftDC` | Display DC pin |
| `sdCS` | SD card chip-select pin |
| `FileName` | File to create (an existing file is overwritten) |
| `spi` | SPI bus the display is on (default `SPI`) |

Returns `true` on success, `false` if the file couldn't be created.

### `SaveBMP24` (frame buffer only)

```cpp
bool SaveBMP24(ST7796_t3 *d, int sdCS, const char *FileName);
```

Shorter form for sketches that use a frame buffer. Returns `false` if no frame buffer is active.

### `DrawBMP24`

```cpp
bool DrawBMP24(ST7796_t3 *d, int cs, const char *FileName, uint16_t x = 0, uint16_t y = 0);
```

Draws a 24-bit, uncompressed BMP with its top-left corner at `x, y`. Parts of the image beyond the screen edge are clipped. Returns `false` if the file is missing or isn't a 24-bit uncompressed BMP.

### Options

Define this before the `#include` to change the SPI clock used for hardware reads. The default is 6 MHz, because the ST7796 reads much slower than it writes:

```cpp
#define ST7796_t3_PS_READ_SPEED 8000000
#include <ST7796_t3_PrintScreen_SdFat.h>
```

---

## Using a frame buffer

```cpp
Display.useFrameBuffer(true);
// ...draw...
Display.updateScreen();

SaveBMP24(&Display, SD_CS, "Screen.bmp");
```

In frame buffer mode the capture is taken from RAM, so MISO doesn't need to be wired.

---

## Technical notes

- **Read format.** The ILI9341 always sends reads as 18-bit (3 bytes per pixel). The ST7796 sends reads in whatever format writes use, and ST7735_t3 sets that to 16-bit RGB565. So each pixel is read as 2 bytes, most significant byte first, after one dummy byte.
- **Shared SPI bus.** The display and the SD card usually share the SPI bus. The library reads one row from the display, releases the display CS, then writes the row to the SD card.
- **Colors.** `ST7735_t3::Color565()` puts blue in the high bits, which swaps red and blue compared with the library's own color constants (`ST7735_RED = 0xF800`). `DrawBMP24` does its own RGB565 conversion so colors come out right.
- **SD card setup.** `SaveBMP24` and `DrawBMP24` set up the SD card themselves each time they're called, so you don't need to call `SD.begin()` / `sd.begin()` first. Don't call them while your sketch has another file open on the card.
- **Capture time.** In hardware read mode, reading a full 480 × 320 screen at 6 MHz takes about 0.4 s of SPI time, plus however long the SD card takes to write the 460 KB file.

---

## License

MIT. See [LICENSE](LICENSE).

Written by Kris Kasprzak. The BMP drawing code is adapted from code by Teensy forum user *socalpinplayer*.
