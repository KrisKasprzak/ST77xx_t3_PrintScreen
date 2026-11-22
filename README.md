# ST77xx_t3_PrintScreen

Screen capture for **ST7735**, **ST7789** and **ST7796** displays driven by the **ST7735_t3** library on **Teensy**.
Save whatever is on the display to a 24-bit BMP file on an SD card, and draw BMP files back onto the display.

- `SaveBMP24()` saves the screen to a `.bmp` file (any rotation, up to 480 × 320)
- `DrawBMP24()` draws a 24-bit `.bmp` file onto the display, at an optional x, y position
- Works **with or without** a frame buffer
- One library for all three displays in ST7735_t3: `ST7735_t3`, `ST7789_t3`, `ST7796_t3`
- Header-only, just include it

> Formerly **ST7796_t3_PrintScreen**. If you used that library, change the include to
> `ST77xx_t3_PrintScreen_SdFat.h` and the option `ST7796_t3_PS_READ_SPEED` to `ST77xx_t3_PS_READ_SPEED`.
> The function calls themselves haven't changed.

---

## Supported displays

| Display class | Controller | Typical sizes | Frame buffer mode | Hardware read mode |
|---|---|---|---|---|
| `ST7796_t3` | ST7796S | 320 × 480 | Yes | Yes, tested on a Teensy 4.0 with a 4" module |
| `ST7789_t3` | ST7789 | 240 × 240, 135 × 240, 240 × 320 | Yes | Yes, needs a module that brings out **SDO/MISO** (follows the datasheet, not yet tested on hardware) |
| `ST7735_t3` | ST7735 / ST7735S | 128 × 160, 128 × 128, 80 × 160 | Yes (recommended) | Only with extra wiring, see [ST7735 hardware read](#st7735-hardware-read) (not yet tested on hardware) |

Frame buffer mode works the same on all three displays and needs no extra wiring.

---

## Why this library is needed

`ST7735_t3` has a `readPixel()` function, but it does **not** read from the display. Both `readPixel()` and `readRect()` only copy from the optional frame buffer. If you haven't called `useFrameBuffer(true)`, there is no frame buffer and `readPixel()` returns garbage or 0.

This library fixes that by getting pixels one of two ways:

| Mode | When it is used | Requirements |
|---|---|---|
| **Frame buffer** | `Display.useFrameBuffer(true)` is active | RAM for width × height × 2 bytes (ST7735 128 × 160: 40 KB, ST7789 240 × 320: 150 KB, ST7796 320 × 480: 300 KB). No extra wiring |
| **Hardware read** | No frame buffer | Display **SDO/MISO** wired to the Teensy MISO pin |

In hardware read mode the library reads the image straight back from the display controller's memory over SPI (command `RAMRD 0x2E`).

---

## Installation

1. Download this repository (**Code → Download ZIP**) and unzip it.
2. Copy the folder to your Arduino `libraries` folder and name it `ST77xx_t3_PrintScreen`.
3. Restart the Arduino IDE.

If you have the older `ST7796_t3_PrintScreen` library installed, delete it.

### Dependencies

| Library | Notes |
|---|---|
| [ST7735_t3](https://github.com/PaulStoffregen/ST7735_t3) | Display driver (provides `ST7735_t3`, `ST7789_t3` and `ST7796_t3`), ships with Teensyduino |
| SdFat | Ships with Teensyduino. You don't need to install it separately |
| SPI | Ships with Teensyduino |

> **SD library users:** Teensy's `SD` library is built on the copy of SdFat that ships with Teensyduino, so this library works even if your sketch only uses `SD.h`.

---

## Wiring

Display wiring is the same as for normal ST7735_t3 use. For **hardware read mode** you also need MISO.

| Display pin | Teensy 4.0 | Notes |
|---|---|---|
| SCK | 13 | |
| SDI / MOSI | 11 | |
| **SDO / MISO** | **12** | **Required for hardware read mode** (ST7796, ST7789) |
| CS | any GPIO (examples use 4) | |
| DC / RS | any GPIO **except 10, 36, 37** (examples use 5) | See note below |
| RST | any GPIO (examples use 6) | |
| SD card CS | any GPIO (examples use 7) | |

**DC pin note:** on Teensy 4.x, ST7735_t3 drives DC as a hardware SPI chip-select when it is on pin 10, 36 or 37. Hardware read mode needs DC on a normal GPIO pin. This doesn't apply in frame buffer mode.

**MISO note:** on some display modules the touch controller doesn't release MISO, which corrupts the read. If captures come back as noise, check what else is connected to MISO, or use frame buffer mode.

**ST7789 modules:** many small ST7789 boards (for example the 240 × 240 boards with no CS pin) don't bring out SDO. Hardware read needs SDO and a CS pin, so use frame buffer mode on those boards.

### ST7735 hardware read

The ST7735 has no SDO pin. Its SPI data line (SDA) is bidirectional: the controller receives on it, then drives it to send data back. To read it with the Teensy's normal (full-duplex) SPI:

- connect the display **SDA** directly to Teensy **MISO** (12)
- connect Teensy **MOSI** (11) to the display **SDA** through a resistor (about 1 kΩ to 4.7 kΩ), so the ST7735 can override MOSI while it is sending data back

Most ST7735 sketches should just use frame buffer mode. It only needs 40 KB of RAM and no extra wiring.

---

## Quick start

```cpp
#include <ST7735_t3.h>
#include <ST7796_t3.h>          // or ST7789_t3.h, or just ST7735_t3.h
#include <ST77xx_t3_PrintScreen_SdFat.h>

#define TFT_CS   4
#define TFT_DC   5
#define TFT_RST  6
#define SD_CS    7

ST7796_t3 Display = ST7796_t3(TFT_CS, TFT_DC, TFT_RST);
// ST7789_t3 Display = ST7789_t3(TFT_CS, TFT_DC, TFT_RST);
// ST7735_t3 Display = ST7735_t3(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Display.init(320, 480);       // ST7789: init(240, 320)   ST7735: initR(INITR_BLACKTAB)
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

### Examples

| Example | Display | Mode |
|---|---|---|
| [`PrintScreen_ST7796`](examples/PrintScreen_ST7796/PrintScreen_ST7796.ino) | ST7796 320 × 480 | Hardware read (frame buffer optional) |
| [`PrintScreen_ST7789`](examples/PrintScreen_ST7789/PrintScreen_ST7789.ino) | ST7789 240 × 320 | Hardware read (frame buffer optional) |
| [`PrintScreen_ST7735`](examples/PrintScreen_ST7735/PrintScreen_ST7735.ino) | ST7735 128 × 160 | Frame buffer |

---

## API

### `SaveBMP24` (any mode)

```cpp
bool SaveBMP24(ST7796_t3 *d, uint8_t tftCS, uint8_t tftDC, int sdCS,
               const char *FileName, SPIClass &spi = SPI);
bool SaveBMP24(ST7789_t3 *d, uint8_t tftCS, uint8_t tftDC, int sdCS,
               const char *FileName, SPIClass &spi = SPI);
bool SaveBMP24(ST7735_t3 *d, uint8_t tftCS, uint8_t tftDC, int sdCS,
               const char *FileName, SPIClass &spi = SPI);
```

Saves the current screen as a 24-bit, uncompressed BMP. Uses the frame buffer if one is active, otherwise reads the display over MISO. You always call it the same way: `SaveBMP24(&Display, ...)`. The compiler picks the right version from the display type, which tells the library how that controller sends pixels back.

| Parameter | Description |
|---|---|
| `d` | Pointer to your display object |
| `tftCS` | Display chip-select pin |
| `tftDC` | Display DC pin |
| `sdCS` | SD card chip-select pin |
| `FileName` | File to create (an existing file is overwritten) |
| `spi` | SPI bus the display is on (default `SPI`) |

Returns `true` on success, `false` if the file couldn't be created.

> Pass a pointer of the **real** display type (`&Display`, where `Display` is declared as `ST7789_t3` or `ST7796_t3`). An `ST7735_t3*` that actually points to an ST7789 or ST7796 object selects the ST7735 version. That works in frame buffer mode, but hardware read would use the wrong format.

### `SaveBMP24` (frame buffer only)

```cpp
bool SaveBMP24(ST7735_t3 *d, int sdCS, const char *FileName);
```

Shorter form for sketches that use a frame buffer. Works with all three display types. Returns `false` if no frame buffer is active.

### `DrawBMP24`

```cpp
bool DrawBMP24(ST7735_t3 *d, int cs, const char *FileName, uint16_t x = 0, uint16_t y = 0);
```

Draws a 24-bit, uncompressed BMP with its top-left corner at `x, y`. Works with all three display types. Parts of the image beyond the screen edge are clipped. Returns `false` if the file is missing or isn't a 24-bit uncompressed BMP. In frame buffer mode, call `Display.updateScreen()` afterwards to show it.

### Options

Define this before the `#include` to change the SPI clock used for hardware reads. The default is 6 MHz. All three controllers read much slower than they write (150 ns minimum read clock period, about 6.6 MHz).

```cpp
#define ST77xx_t3_PS_READ_SPEED 4000000
#include <ST77xx_t3_PrintScreen_SdFat.h>
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

- **Read format.** The controllers don't send pixels back the same way:
  - **ST7796:** reads use the same format as writes. ST7735_t3 sets that to 16-bit RGB565, so each pixel is 2 bytes, most significant byte first.
  - **ST7735 and ST7789:** the datasheets say reads are "fixed to 18-bit" and that COLMOD should be set to `0x66` for frame memory reads. The library sets COLMOD to `0x66` for each row, reads 3 bytes per pixel (R, G, B, 6 bits each), then restores the driver's 16-bit setting (`0x05` on the ST7735, `0x55` on the ST7789).

  All three send one dummy byte after `RAMRD`.
- **Memory offsets.** Many ST7735 and ST7789 panels are smaller than the controller's memory, so the driver shifts everything it draws by an offset that also changes with rotation. For example, a 240 × 240 ST7789 is offset by 80 rows in rotation 0. Hardware read uses the driver's own offsets, so it reads exactly the area the driver draws to. The offsets are protected members of `ST7735_t3`, so the library reads them through a small derived-class accessor. The driver doesn't need to be changed.
- **Shared SPI bus.** The display and the SD card usually share the SPI bus. The library reads one row from the display, releases the display CS, then writes the row to the SD card.
- **Colors.** `ST7735_t3::Color565()` puts blue in the high bits, which swaps red and blue compared with the library's own color constants (`ST7735_RED = 0xF800`). `DrawBMP24` does its own RGB565 conversion so colors come out right. Captures store the colors your sketch drew, regardless of the panel's RGB/BGR setting.
- **SD card setup.** `SaveBMP24` and `DrawBMP24` set up the SD card themselves each time they're called, so you don't need to call `SD.begin()` / `sd.begin()` first. Don't call them while your sketch has another file open on the card.
- **Capture time.** In hardware read mode, reading a full 480 × 320 ST7796 screen at 6 MHz takes about 0.4 s of SPI time, plus however long the SD card takes to write the 460 KB file. A 240 × 320 ST7789 takes about 0.3 s (3 bytes per pixel).

---

## License

MIT. See [LICENSE](LICENSE).

Written by Kris Kasprzak. The BMP drawing code is adapted from code by Teensy forum user *socalpinplayer*.
