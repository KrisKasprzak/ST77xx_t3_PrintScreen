#include <ST7735_t3.h>
#include <ST77xx_t3_PrintScreen_SdFat.h>

// the ST7735 has no SDO/MISO pin (its data line is bidirectional), so this example
// uses the frame buffer, which needs no extra wiring (128 x 160 x 2 = 40 KB of RAM).
// see the README for hardware read-back on the ST7735
#define TFT_CS   4
#define TFT_DC   5
#define TFT_RST  6
#define SD_CS    7

ST7735_t3 Display = ST7735_t3(TFT_CS, TFT_DC, TFT_RST);

void setup() {

  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  // change to match your panel: INITR_BLACKTAB, INITR_GREENTAB, INITR_144GREENTAB, INITR_MINI160x80 ...
  Display.initR(INITR_BLACKTAB);
  Display.setRotation(1);

  if (!Display.useFrameBuffer(true)) {
    Serial.println("could not allocate the frame buffer");
  }

  Display.fillScreen(ST7735_BLACK);
  for (int i = 0; i < 100; i += 4) {
    Display.drawRect(i, i, 100 - i, 100 - i, ST7735_RED);
  }
  Display.fillRect(110, 10, 40, 40, ST7735_GREEN);
  Display.fillRect(110, 60, 40, 40, ST7735_BLUE);
  Display.updateScreen();

  // frame buffer form, no TFT pins needed
  Serial.print("Saving file...");
  if (SaveBMP24(&Display, SD_CS, "ST7735.BMP")) Serial.println("file saved.");
  else Serial.println("save FAILED.");

  delay(1000);
  Display.fillScreen(ST7735_BLACK);
  Display.updateScreen();

  Serial.print("Drawing BMP file...");
  if (DrawBMP24(&Display, SD_CS, "ST7735.BMP")) Serial.println("drawing complete.");
  else Serial.println("draw FAILED.");
  Display.updateScreen();
}

void loop() {
}
