#include "esp_ota_ops.h"
#include <Arduino.h>
#include <U8g2lib.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

void setup() {
  pinMode(2, OUTPUT);
  pinMode(19, INPUT_PULLUP);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_haxrcorp4089_tr);
  u8g2.drawStr(0, 10, "Hello, World!");
  u8g2.drawStr(0, 21, "- hello_world.tar");
  u8g2.sendBuffer();
}

void loop() {
  if (digitalRead(19) == LOW) {
    esp_ota_mark_app_invalid_rollback_and_reboot();
  }
}
