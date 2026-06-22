// Lab 2 · Track W · A.1 — Temperature/Humidity capture   (deliverable: w1-th-capture.ino)
//
// Board: Arduino Nano 33 BLE Sense Rev2 (HS3003 sensor via Arduino_HS300x).
// Install Arduino_HS300x via Library Manager if you haven't (Lab 1 §2).
//
// Goal: once per second, read temperature (degC) and relative humidity (%)
// and print ONE CSV line over Serial so you can capture a dataset.
// Suggested columns:  millis,temp_c,humidity_pct
//
// Skeleton compiles/uploads as-is once the include resolves. Fill in
// each TODO(student). (Handout §A.1: >= 10 streamed lines is a deliverable.)

#include <Arduino_HS300x.h>

const unsigned long SAMPLE_INTERVAL_MS = 1000;  // 1 Hz sampling
unsigned long lastSample = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }  // wait for the Serial Monitor (USB native)

  if (!HS300x.begin()) {
    Serial.println("Failed to initialize HS300x sensor!");
    while (1) { ; }  // halt — check the library install / board selection
  }

  // TODO(student): print a one-line CSV header so your capture file is
  // self-describing, e.g.  Serial.println("millis,temp_c,humidity_pct");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSample < SAMPLE_INTERVAL_MS) return;
  lastSample = now;

  float t = HS300x.readTemperature();  // Celsius
  float h = HS300x.readHumidity();  // Relative humidity %

  Serial.print(now);
  Serial.print(",");
  Serial.print(t, 2);
  Serial.print(",");
  Serial.println(h, 2);

}
