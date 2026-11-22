#include <ST7735_t3.h>
#include <ST7789_t3.h>
#include <ST77xx_t3_PrintScreen_SdFat.h>

// for hardware read-back the display SDO/MISO must be wired to the Teensy MISO (pin 12).
// many small ST7789 modules (e.g. 240x240 without a CS pin) do not bring out SDO,
// for those use the frame buffer (see below)
// TFT_DC must be a plain GPIO (on Teensy 4.x not 10, 36 or 37)
#define TFT_CS   4
#define TFT_DC   5
#define TFT_RST  6
#define SD_CS    7

ST7789_t3 Display = ST7789_t3(TFT_CS, TFT_DC, TFT_RST);

void setup() {

  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  // change to match your panel: 240x240, 135x240, 240x320, ...
  Display.init(240, 320);
  Display.setRotation(1);

  // optional: with a frame buffer the capture reads RAM instead of the display
  // (then call Display.updateScreen() after drawing)
  // Display.useFrameBuffer(true);

  Display.fillScreen(ST7735_BLACK);
  for (int i = 0; i < 160; i += 4) {
    Display.drawRect(i, i, 160 - i, 160 - i, ST7735_RED);
  }
  Display.fillRect(180, 20, 50, 50, ST7735_GREEN);
  Display.fillRect(250, 20, 50, 50, ST7735_BLUE);
  Display.updateScreen();  // does nothing unless the frame buffer is on

  Serial.print("Saving file...");
  if (SaveBMP24(&Display, TFT_CS, TFT_DC, SD_CS, "ST7789.BMP")) Serial.println("file saved.");
  else Serial.println("save FAILED.");

  delay(1000);
  Display.fillScreen(ST7735_BLACK);
  Display.updateScreen();

  Serial.print("Drawing BMP file...");
  if (DrawBMP24(&Display, SD_CS, "ST7789.BMP")) Serial.println("drawing complete.");
  else Serial.println("draw FAILED.");
  Display.updateScreen();
}

void loop() {
}
