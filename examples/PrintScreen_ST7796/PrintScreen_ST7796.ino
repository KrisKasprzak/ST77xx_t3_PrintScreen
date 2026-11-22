#include <ST7735_t3.h>
#include <ST7796_t3.h>
#include <ST77xx_t3_PrintScreen_SdFat.h>

// display SDO/MISO must be wired to the Teensy MISO (pin 12) for hardware read-back
// TFT_DC must be a plain GPIO (on Teensy 4.x not 10, 36 or 37)
#define TFT_CS   4
#define TFT_DC   5
#define TFT_RST  6
#define SD_CS    7

ST7796_t3 Display = ST7796_t3(TFT_CS, TFT_DC, TFT_RST);

void setup() {

  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  Display.init(320, 480);
  Display.setRotation(1);

  // optional: with a frame buffer the capture reads RAM instead of the display
  // (then call Display.updateScreen() after drawing)
  // Display.useFrameBuffer(true);

  Display.fillScreen(ST7735_BLACK);
  for (int i = 0; i < 200; i += 4) {
    Display.drawRect(i, i, 200 - i, 200 - i, ST7735_RED);
  }
  Display.fillRect(250, 20, 60, 60, ST7735_GREEN);
  Display.fillRect(330, 20, 60, 60, ST7735_BLUE);

  Serial.print("Saving file...");
  if (SaveBMP24(&Display, TFT_CS, TFT_DC, SD_CS, "ST7796.BMP")) Serial.println("file saved.");
  else Serial.println("save FAILED.");

  delay(1000);
  Display.fillScreen(ST7735_BLACK);

  Serial.print("Drawing BMP file...");
  if (DrawBMP24(&Display, SD_CS, "ST7796.BMP")) Serial.println("drawing complete.");
  else Serial.println("draw FAILED.");
}

void loop() {
}
