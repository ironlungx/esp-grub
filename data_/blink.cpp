#include <Arduino.h>
#include "esp_ota_ops.h"

void setup() {
  pinMode(2, OUTPUT);
  pinMode(19, INPUT_PULLUP);
}

void loop() {
  digitalWrite(2, HIGH);
  delay(500);
  digitalWrite(2, LOW);
  delay(500);

  if (digitalRead(19) == LOW) {
    esp_ota_mark_app_invalid_rollback_and_reboot();
  }
}
