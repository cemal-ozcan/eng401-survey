#include <Arduino.h>

#include "build_info.h"

void setup() {
  Serial.begin(115200);
  Serial.printf("\nENG401 survey firmware v%s (%s sensors)\n", build_info::version(),
                build_info::modeName());
}

void loop() {
  // Placeholder: the application will run in FreeRTOS tasks; loop() stays idle.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
