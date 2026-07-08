// Lab 3 · C.1 (5862) — BLE gesture telemetry
//                                       (deliverable: g3-ble-telemetry.ino)
//
// Board: Arduino Nano 33 BLE Sense Rev2 (on-board BMI270 IMU + RGB LED + BLE).
//
// Extends g2-gesture-inference/ with a BLE peripheral that publishes
// ONE BYTE per stable gesture detection over a Notify characteristic.
// The raw IMU stream never leaves the device — only inference RESULTS
// do. This is HW 4 Q8 in firmware: AIoT = on-device inference + low-rate
// radio transmission of results, not raw data.
//
// See README.md in this folder for pairing instructions (the self-hosted
// Web Bluetooth receiver in lab-3/tools/, or Bluefruit LE Connect on iOS)
// and the report deliverables.
//
// Fill in each TODO(student). Most of the gesture pipeline is unchanged
// from g2; the new code is the BLE setup + the notify call.

// TODO(student): replace with YOUR Edge Impulse project's header
// (same as g2-gesture-inference). The sketch won't compile until you do.
//
//   #include <your-project-name_inferencing.h>

#include <Lab3-Gesture_inferencing.h>
#include <Arduino_BMI270_BMM150.h>
#include <ArduinoBLE.h>
#include <string.h>

// ----------------------------------------------------------------------------
// BLE service + characteristic.
//
// A custom 128-bit UUID is the right choice for a project-specific service.
// (The Bluetooth SIG only assigns short 16-bit UUIDs to standardized services
// like Heart Rate; for a custom payload we generate one once and ship it.)
// Replace the random UUID below with one of your own if you ship multiple
// students' boards in the same room.
//
// GestureEvent payload: 1 byte
//   0x00 = idle (we don't notify on idle — quiet bus)
//   0x01 = punch
//   0x02 = flex
//   0x03 = circle
// ----------------------------------------------------------------------------
const char* kServiceUuid        = "19b10000-e8f2-537e-4f6c-d104768a1214";
const char* kCharacteristicUuid = "19b10001-e8f2-537e-4f6c-d104768a1214";
const char* kLocalName          = "Nano33Gesture";

BLEService        gestureService(kServiceUuid);
BLEByteCharacteristic gestureChar(kCharacteristicUuid, BLERead | BLENotify);

// ---- Re-use the test-and-trace filter from g2-gesture-inference. ----
// Copy your tuned constants over. The handout's §B.2(b) "four filter
// knobs" table documents each one; the brief reminders are inline below.
constexpr int   kHistoryN          = 3;     // window history depth
constexpr int   kAgreeMin          = 2;     // votes required for a stable detection

// kConfidenceMin: gate weak (near-uniform softmax) windows out of the
// history; without this, an accidental majority of low-confidence
// argmaxes can latch the filter onto a wrong class. L10 ROC framing.
constexpr float kConfidenceMin     = 0.70f;

// kReArmIdleMs: same-class re-fire suppression so a held gesture
// doesn't notify every window stride (and over BLE, doesn't spam the
// receiver — important for the AIoT bandwidth argument in §C.1).
constexpr unsigned long kReArmIdleMs = 1000;

// kIdleClassIx: array index of the `idle` class in EI's
// ei_classifier_inferencing_categories[]. Studio sorts alphabetically,
// so for the suggested 4-class set (circle, flex, idle, punch) this
// is 2. Verify in your model_metadata.h and edit if your labels differ.
constexpr int kIdleClassIx       = 2;     // <-- match g2

int   history[kHistoryN] = { kIdleClassIx, kIdleClassIx, kIdleClassIx };
int   history_head       = 0;
int   last_acted_on      = kIdleClassIx;
unsigned long last_act_ms = 0;

static float feature_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

int raw_feature_get_data(size_t offset, size_t length, float* out_ptr) {
  memcpy(out_ptr, feature_buffer + offset, length * sizeof(float));
  return 0;
}

// On-board RGB LED is active LOW.
const int PIN_R = LEDR;
const int PIN_G = LEDG;
const int PIN_B = LEDB;

void rgb_off() {
  digitalWrite(PIN_R, HIGH);
  digitalWrite(PIN_G, HIGH);
  digitalWrite(PIN_B, HIGH);
}

void blink_rgb(int r, int g, int b, int times = 1) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_R, r ? LOW : HIGH);
    digitalWrite(PIN_G, g ? LOW : HIGH);
    digitalWrite(PIN_B, b ? LOW : HIGH);
    delay(220);
    rgb_off();
    delay(120);
  }
}

int majority_class() {
  for (int i = 0; i < kHistoryN; i++) {
    int candidate = history[i], count = 0;
    for (int j = 0; j < kHistoryN; j++) if (history[j] == candidate) count++;
    if (count >= kAgreeMin) return candidate;
  }
  return -1;
}


void blink_color(int pin, int times = 1) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, LOW);   delay(200);
    digitalWrite(pin, HIGH);  delay(200);
  }
}

void act_on_stable_detection(int class_ix, const char* label) {
  unsigned long now = millis();

  if (class_ix == last_acted_on && (now - last_act_ms) < kReArmIdleMs) {
    return;
  }

  if (strcmp(label, "punch") == 0) {
    blink_rgb(1, 0, 0);        // red
  }
  else if (strcmp(label, "flex") == 0) {
    blink_rgb(0, 1, 0);        // green
  }
  else if (strcmp(label, "circle") == 0) {
    blink_rgb(0, 0, 1);        // blue
  }
  else {
    rgb_off();
    return;
  }

  // BLE telemetry: one byte per stable gesture event.
  // Mapping matches web-ble-receiver.html: 0=circle, 1=flex, 2=idle, 3=punch.
  uint8_t payload = (uint8_t)class_ix;
  gestureChar.writeValue(payload);

  Serial.print("ACT ");
  Serial.print(label);
  Serial.print(" -> BLE notify 0x");
  if (payload < 16) Serial.print("0");
  Serial.print(payload, HEX);
  Serial.print(" @ ");
  Serial.print(now);
  Serial.println(" ms");

  last_acted_on = class_ix;
  last_act_ms = now;

}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  pinMode(PIN_R, OUTPUT); 
  pinMode(PIN_G, OUTPUT); 
  pinMode(PIN_B, OUTPUT);
  rgb_off();


  if (!IMU.begin()) {
    Serial.println("IMU.begin() failed -- install Arduino_BMI270_BMM150");
    while (1) {}
  }

  if (!BLE.begin()) {
    Serial.println("BLE.begin() failed");
    while (1) {}
  }

  BLE.setLocalName(kLocalName);
  BLE.setDeviceName(kLocalName);   // GAP Device Name (what macOS/Chrome
                                    // pickers display; without this they
                                    // fall back to the default "Arduino")
  BLE.setAdvertisedService(gestureService);
  gestureService.addCharacteristic(gestureChar);
  BLE.addService(gestureService);
  gestureChar.writeValue((uint8_t)kIdleClassIx);   // initial value
  BLE.advertise();

  Serial.print("BLE peripheral '");
  Serial.print(kLocalName);
  Serial.println("' advertising");
  Serial.print("Labels: ");
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.print(i);
    Serial.print("=");
    Serial.print(ei_classifier_inferencing_categories[i]);
    if (i + 1 < EI_CLASSIFIER_LABEL_COUNT) Serial.print(", ");
  }
  Serial.println();
  Serial.print("Input frame size: ");
  Serial.println(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  Serial.print("Frequency: ");
  Serial.println(EI_CLASSIFIER_FREQUENCY);
  Serial.print("Idle index: ");
  Serial.println(kIdleClassIx);
}

void loop() {
  BLE.poll();

  // Connection-state LED indicator: slow blue pulse while disconnected,
  // solid blue while connected.
  static unsigned long last_pulse_ms = 0;
  if (BLE.connected()) {
    digitalWrite(PIN_B, LOW);
  } else if (millis() - last_pulse_ms > 1000) {
    digitalWrite(PIN_B, digitalRead(PIN_B) == HIGH ? LOW : HIGH);
    last_pulse_ms = millis();
  }

  const unsigned long sample_period_us = 1000000UL / EI_CLASSIFIER_FREQUENCY;
  unsigned long next_sample_us = micros();

  for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 6) {
    BLE.poll();

    while ((long)(micros() - next_sample_us) < 0) {
      BLE.poll();
    }
    next_sample_us += sample_period_us;

    while (!IMU.accelerationAvailable() || !IMU.gyroscopeAvailable()) {
      BLE.poll();
    }

    float ax, ay, az, gx, gy, gz;
    IMU.readAcceleration(ax, ay, az);
    IMU.readGyroscope(gx, gy, gz);

    feature_buffer[ix + 0] = ax;
    feature_buffer[ix + 1] = ay;
    feature_buffer[ix + 2] = az;
    feature_buffer[ix + 3] = gx;
    feature_buffer[ix + 4] = gy;
    feature_buffer[ix + 5] = gz;
  }

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  signal.get_data = &raw_feature_get_data;

  ei_impulse_result_t result = { 0 };
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

  if (res != EI_IMPULSE_OK) {
    Serial.print("ERR run_classifier failed: ");
    Serial.println((int)res);
    return;
  }

  int top_ix = 0;
  float top_conf = result.classification[0].value;
  for (size_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > top_conf) {
      top_conf = result.classification[i].value;
      top_ix = (int)i;
    }
  }

  const char* top_label = ei_classifier_inferencing_categories[top_ix];

  Serial.print("top=");
  Serial.print(top_label);
  Serial.print(" conf=");
  Serial.print(top_conf, 3);
  Serial.print(" dsp_ms=");
  Serial.print(result.timing.dsp);
  Serial.print(" cls_ms=");
  Serial.print(result.timing.classification);
  Serial.print(" total_ms=");
  Serial.println(result.timing.dsp + result.timing.classification);

  if (top_conf < kConfidenceMin) {
    top_ix = kIdleClassIx;
  }

  history[history_head] = top_ix;
  history_head = (history_head + 1) % kHistoryN;

  int stable = majority_class();

  if (stable >= 0 && stable != kIdleClassIx) {
    const char* stable_label = ei_classifier_inferencing_categories[stable];
    act_on_stable_detection(stable, stable_label);
  }
  else if (stable == kIdleClassIx) {
    last_acted_on = kIdleClassIx;
    if (BLE.connected()) {
      digitalWrite(PIN_B, LOW);
    }
  }
}
