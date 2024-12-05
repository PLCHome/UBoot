
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <Wire.h>
// #include <Adafruit_SSD1306.h>
#include <Adafruit_MCP4725.h>
#include <Adafruit_SH1106.h>
#include <EEPROM.h>
#include "version.h"

#define LCD_COLUMNS 21
#define LCD_LINES 4

#define ENCODER_CLK 2
#define ENCODER_DT 3

#define ENCODER_BTN 4
#define LENZ_BTN 5

Adafruit_MCP4725 dac_heck;
Adafruit_MCP4725 dac_bug;
#define ADDR_DAC_HECK 0x60
#define ADDR_DAC_BUG 0x61
// #define DAC_HECK 10
// #define DAC_BUG 11

#define mode_normalbetr 0
#define mode_asynchronbetr 2
#define mode_synchronbetr 1
#define mode_max 2

#define moden_fluten 0
#define moden_normalbetr 1
#define moden_lenzen 2
#define moden_max 2

int16_t val_heck = 127;
#define EEPROM_POS_HECK 0
int16_t val_bug = 127;
#define EEPROM_POS_BUG 2

#define MAX_ASYNC 256

uint8_t mode = mode_normalbetr;
uint8_t mode_normal = moden_normalbetr;
uint8_t moden_done = moden_lenzen;
bool lastmode = false;
bool shortmode = false;
bool printmode = true;
bool lastlenz = false;
bool printval = true;
bool setanalog = false;
uint8_t modetime = 0;

unsigned long time = 0;
const unsigned long key_time = 2000;

#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS                                                         \
  0x3c ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Adafruit_SH1106 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SH1106 display(OLED_RESET);
#define DIS_ROW_PIX SCREEN_HEIGHT / LCD_LINES *

uint8_t to8(int16_t val) { return val < 0 ? 0 : val > 255 ? 255 : val; }

void writeChannel(Adafruit_MCP4725 port, int16_t value) {
  int16_t val = to8(value) << 4;
  if (val > 4079) {
    val = 4095;
  }
  port.setVoltage(val, false);
}

void writeVal(int16_t heck, int16_t bug) {
  writeChannel(dac_heck, heck);
  writeChannel(dac_bug, bug);
  // analogWrite(DAC_HECK, to8(heck));
  // analogWrite(DAC_BUG, to8(bug));
}

unsigned long timeEncoder = 0;
void readEncoder() {
  int dtValue = digitalRead(ENCODER_DT);
  switch (mode) {
  case mode_normalbetr:
    if (dtValue == HIGH) {
      if (mode_normal > 0)
        mode_normal--;
    }
    if (dtValue == LOW) {
      if (mode_normal < moden_max)
        mode_normal++;
    }
    printmode = true;
    break;
  case mode_asynchronbetr:
    if (dtValue == HIGH) {
      if ((val_heck - val_bug) < MAX_ASYNC) {
        val_heck++;
        val_bug--;
      }
    }
    if (dtValue == LOW) {
      if ((val_bug - val_heck) < MAX_ASYNC) {
        val_heck--;
        val_bug++;
      }
    }
    setanalog = true;
    printval = true;
    break;
  case mode_synchronbetr:
    uint8_t add = 1;
    if ((millis() - timeEncoder) < 500)
      add = 5;
    if ((millis() - timeEncoder) < 250)
      add = 10;
    if (dtValue == LOW) {
      if (val_heck >= 0 + add || val_bug >= 0 + add) {
        val_heck -= add;
        val_bug -= add;
      }
    }
    if (dtValue == HIGH) {
      if (val_heck <= 255 - add || val_bug <= 255 - add) {
        val_heck += add;
        val_bug += add;
      }
    }
    setanalog = true;
    printval = true;
    timeEncoder = millis();
    break;
  }
}

void setup() {
  // Init
  EEPROM.get(EEPROM_POS_HECK, val_heck);
  EEPROM.get(EEPROM_POS_BUG, val_bug);
  dac_heck.begin(ADDR_DAC_HECK);
  dac_bug.begin(ADDR_DAC_BUG);

  // val_heck = 255;
  // val_bug = 255;
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  lastmode = !digitalRead(ENCODER_BTN);
  pinMode(LENZ_BTN, INPUT_PULLUP);
  lastlenz = digitalRead(LENZ_BTN);
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), readEncoder, FALLING);
  writeVal(0, 0);

  display.begin(SH1106_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.display();
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(WHITE, BLACK); // Draw white text
  display.setCursor(0, 0);            // Start at top-left corner
  display.print(VERSION);
  display.display();
  delay(1000);
  display.clearDisplay();
  display.display();
}

void changemode(bool s) {
  if (s) {
    mode++;
    if (mode > mode_max)
      mode = 1;
  } else {
    mode = 0;
  }
  if (mode == moden_normalbetr) {
    mode_normal = moden_normalbetr;
    moden_done = mode_normal;
  }
  printmode = true;
  printval = true;
}

char *ltrim(char *const s) {
  size_t len;
  char *cur;

  if (s && *s) {
    len = strlen(s);
    cur = s;

    while (*cur && isspace(*cur))
      ++cur, --len;

    if (s != cur)
      memmove(s, cur, len + 1);
  }

  return s;
}

void centerText(char *b, char *text, int fieldWidth) {
  char bufff[LCD_COLUMNS + 1];
  int len = strlen(text);
  int padlen = (fieldWidth - len) / 2 + len;
  sprintf(bufff, "%%%ds%%%ds", padlen, fieldWidth - padlen);
  sprintf(b, bufff, text, " ");
}

void centerNum(char *b, int val, int fieldWidth) {
  char buffv[LCD_COLUMNS + 1];
  sprintf(buffv, "%d", val);
  centerText(b, buffv, fieldWidth);
}

void centerFloat(char *b, float val, int fieldWidth) {
  char buffv[LCD_COLUMNS + 1];
  dtostrf(val, 5, 1, buffv);
  sprintf(buffv, "%s%%", buffv);
  centerText(b, buffv, fieldWidth);
}

void rightFloat(char *b, float val) {
  char buffv[LCD_COLUMNS + 1];
  dtostrf(val, 5, 1, buffv);
  sprintf(b, "%5s%%", buffv);
}

uint16_t getTextLength(char *b) {
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(b, 0, 0, &x, &y, &w, &h); // calc width of new string
  return w;
}

void printm() {
  char buff[LCD_COLUMNS + 1];
  switch (mode) {
  case mode_normalbetr:
    switch (mode_normal) {
    case moden_fluten:
      sprintf(buff, "Tank Fluten");
      break;
    case moden_normalbetr:
      if (moden_done == moden_normalbetr) {
        sprintf(buff, "Normalbetrieb");
      } else {
        sprintf(buff, "Zum Normalbetrieb");
      }
      break;
    case moden_lenzen:
      sprintf(buff, "Tank Lenzen");
      break;
    }
    break;
  case mode_asynchronbetr:
    sprintf(buff, "Asynchronbetrieb");
    break;
  case mode_synchronbetr:
    sprintf(buff, "Synchronbetrieb");
    break;
  }
  display.fillRect(0, 0, SCREEN_WIDTH, 8, BLACK);
  display.setCursor((SCREEN_WIDTH - getTextLength(buff)) / 2, 0);
  display.print(buff);
  display.display();
}

#define UBOOT_RAND 25
#define UBOOT_TOP 12
#define UBOOT_BOTTOM 20
void printv() {
  char buff[LCD_COLUMNS + 1];
  char buff2[LCD_COLUMNS + 1];
  uint8_t heck = 0;
  uint8_t bug = 0;
  int16_t val_diff = 0;
  display.fillRect(0, 9, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
  // display.setTextColor(INVERSE ); // Draw white text

  display.setTextSize(1); // Normal 1:1 pixel scale
  display.setRotation(3);

  sprintf(buff, "HECK");
  display.setCursor((SCREEN_HEIGHT - getTextLength(buff)) / 2, 0);
  display.print(buff);
  display.setRotation(1);
  sprintf(buff, "BUG");
  display.setCursor((SCREEN_HEIGHT - getTextLength(buff)) / 2, 0);
  display.print(buff);
  display.setRotation(0);

  if (mode == mode_normalbetr && moden_done != moden_normalbetr) {
    for (uint8_t i = 0; i < 2; i++) {
      if (moden_done == moden_lenzen) {
        display.setCursor(11 + 64 * i, SCREEN_HEIGHT - 9);
        display.print("gelenzt");
        heck = 0;
        bug = 0;
      } else {
        display.setCursor(8 + 64 * i, SCREEN_HEIGHT - 9);
        display.print("geflutet");
        heck = 255;
        bug = 255;
      }
    }
  } else {

    heck = to8(val_heck);
    bug = to8(val_bug);

    val_diff = heck - bug;
    rightFloat(buff, 100.0 * heck / 255.0);
    rightFloat(buff2, 100.0 * bug / 255.0);
    uint8_t pos = 0;
    for (uint8_t i = 0; i < 6; i++) {
      uint8_t size = 2;
      if (i == 3 || i == 5) {
        size = 1;
      }
      display.drawChar(pos, SCREEN_HEIGHT - size * 7 - 1, buff[i], WHITE, BLACK,
                       size);
      display.drawChar(SCREEN_WIDTH - 56 + pos, SCREEN_HEIGHT - size * 7 - 1,
                       buff2[i], WHITE, BLACK, size);
      pos += size * 5 + 1;
    }
    switch (mode) {
    case mode_asynchronbetr:
      display.drawChar(50, SCREEN_HEIGHT - 16, 0x18, WHITE, BLACK, 1);
      display.drawChar(SCREEN_WIDTH - 6, SCREEN_HEIGHT - 16, 0x19, WHITE, BLACK,
                       1);
      break;
    case mode_synchronbetr:
      display.drawChar(50, SCREEN_HEIGHT - 16, 0x18, WHITE, BLACK, 1);
      display.drawChar(SCREEN_WIDTH - 6, SCREEN_HEIGHT - 16, 0x18, WHITE, BLACK,
                       1);
      break;
    }
  }
  for (uint8_t i = 0; i < 3; i++) {
    display.drawLine(
        UBOOT_RAND - i % 2,
        1.0 * UBOOT_TOP +
            ((SCREEN_HEIGHT - UBOOT_TOP - UBOOT_BOTTOM) * heck / 255) + i,
        SCREEN_WIDTH - UBOOT_RAND + i % 2,
        1.0 * UBOOT_TOP +
            ((SCREEN_HEIGHT - UBOOT_TOP - UBOOT_BOTTOM) * bug / 255) + i,
        WHITE);
  }
  display.setTextSize(1); // Normal 1:1 pixel scale
  if (val_diff != 0) {
    dtostrf(100.0 * val_diff / 255.0, 5, 1, buff);
    ltrim(buff);
    sprintf(buff, "%s%%", buff);
    uint16_t w = getTextLength(buff);
    display.setCursor((SCREEN_WIDTH - w) / 2,
                      1.0 * UBOOT_TOP +
                          ((SCREEN_HEIGHT - UBOOT_TOP - UBOOT_BOTTOM) *
                           (to8(val_bug) + (val_diff / 2)) / 255) -
                          2);
    display.print(buff);
  }
  display.setTextColor(WHITE, BLACK); // Draw white text
  display.display();
  EEPROM.put(EEPROM_POS_HECK, val_heck);
  EEPROM.put(EEPROM_POS_BUG, val_bug);
}

void loop() {
  // last mode
  bool thismode = digitalRead(ENCODER_BTN);
  bool thislenz = digitalRead(LENZ_BTN);
  if (!thislenz && lastlenz) {
    writeVal(0, 0);
    shortmode = false;
    changemode(false);
    moden_done = moden_lenzen;
    mode_normal = moden_normalbetr;
    printmode = true;
    printval = true;
  } else if (!lastmode && thismode) {
    shortmode = true;
    time = millis();
  } else if (lastmode && thismode && time > 0 && (millis() - time) > key_time) {
    shortmode = false;
    changemode(false);
    time = millis();
  } else if (lastmode && !thismode) {
    time = 0;
    if (shortmode) {
      if (mode == mode_normalbetr && mode_normal == moden_normalbetr &&
          moden_done != moden_normalbetr) {
        setanalog = true;
        moden_done = mode_normal;
        printmode = true;
        printval = true;
      } else if (mode == mode_normalbetr && mode_normal == moden_fluten) {
        writeVal(255, 255);
        moden_done = moden_fluten;
        mode_normal = moden_normalbetr;
        printmode = true;
        printval = true;
      } else if (mode == mode_normalbetr && mode_normal == moden_lenzen) {
        writeVal(0, 0);
        moden_done = moden_lenzen;
        mode_normal = moden_normalbetr;
        printmode = true;
        printval = true;
      } else /* if (mode != mode_normalbetr) */ {
        changemode(true);
      }
    }
  }
  lastmode = thismode;

  if (printmode) {
    printmode = false;
    printm();
  }
  if (printval) {
    printval = false;
    printv();
  }
  if (setanalog) {
    setanalog = false;
    writeVal(val_heck, val_bug);
  }

  delay(10);
}
