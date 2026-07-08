// Lab 3 · B.2 — Continuous gesture inference + test-and-trace
//                                   (deliverable: g2-gesture-inference.ino)
//
// Board: Arduino Nano 33 BLE Sense Rev2 (on-board BMI270 IMU + RGB LED).
//
// This is a thin wrapper around the Arduino library that EDGE IMPULSE
// generates when you export your trained project (B.1). The EI library
// provides continuous IMU inference; your job is:
//
//   1. Wire run_classifier_continuous() to the BMI270 sample loop.
//   2. Implement the TEST-AND-TRACE post-processing filter (Cookbook Ch 9).
//   3. Implement the ACT stage — blink the on-board RGB LED in a per-gesture
//      color when the filter emits a stable detection.
//
// See README.md in this folder for the install / setup steps and an
// explanation of why test-and-trace is necessary.
//
// Fill in each TODO(student).

// TODO(student): replace with YOUR Edge Impulse project's header.
// The EI library zip's header is named "<your-project-name>_inferencing.h".
// Until you fix this include, the sketch will not compile.
//
//   #include <your-project-name_inferencing.h>

#include <Lab3-Gesture_inferencing.h>
#include <Arduino_BMI270_BMM150.h>
#include <string.h>

// On-board RGB LED (active LOW on the Nano 33 BLE Sense).
const int PIN_R = LEDR;
const int PIN_G = LEDG;
const int PIN_B = LEDB;

// ----------------------------------------------------------------------------
// Test-and-trace filter state.
//
// We remember the last N predictions; emit a "stable detection" only when
// >= M of the last N agreed on the same non-idle class above the threshold.
// Start at N=3, M=2 (Cookbook §9 default); tune up if your detector is
// jumpy, down if it is sluggish. The handout's §B.2(b) "four filter knobs"
// table lists which knob to turn for which symptom — read that before
// changing values here.
// ----------------------------------------------------------------------------
constexpr int   kHistoryN          = 3;     // window history depth
constexpr int   kAgreeMin          = 2;     // votes required for a stable detection

// kConfidenceMin: minimum top-class softmax probability that COUNTS as
// a real prediction. Windows below this threshold are treated as `idle`
// so they don't latch onto an accidental majority. argmax alone is a
// coin flip when the output is near-uniform — this gates that out.
// L10 ROC operating-point framing applies here (HW 4 Q5).
constexpr float kConfidenceMin     = 0.80f;

// kReArmIdleMs: after firing a gesture, the same gesture can't re-fire
// for this many milliseconds (or until an `idle` window resets the
// guard). Without it, holding a gesture re-blinks the LED every window
// (~every 500 ms at the default stride).
constexpr unsigned long kReArmIdleMs = 1000;

// TODO(student): set this constant to the index of your "idle" class in
// the EI label list (look at the order ei_classifier_inferencing_categories
// is generated in your model_metadata.h, or just match by string in code).
// The Edge Impulse Studio shows class labels in the order it assigned them
// — usually alphabetical, so e.g. circle=0, flex=1, idle=2, punch=3.
constexpr int kIdleClassIx = 2;       // <-- replace with your idle index

int   history[kHistoryN] = { kIdleClassIx, kIdleClassIx, kIdleClassIx };
int   history_head       = 0;
int   last_acted_on      = kIdleClassIx;
unsigned long last_act_ms = 0;

static float feature_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// Edge Impulse reads raw samples through this callback.
int raw_feature_get_data(size_t offset, size_t length, float* out_ptr) {
  memcpy(out_ptr, feature_buffer + offset, length * sizeof(float));
  return 0;
}

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
    delay(250);
    rgb_off();
    delay(150);
  }
}

// Tally the most-common index in `history`. Returns -1 if no class clears
// the kAgreeMin threshold.
int majority_class() {
  // Simple O(N^2) since N is tiny.
  for (int i = 0; i < kHistoryN; i++) {
    int candidate = history[i], count = 0;
    for (int j = 0; j < kHistoryN; j++) if (history[j] == candidate) count++;
    if (count >= kAgreeMin) return candidate;
  }
  return -1;
}

void act_on_stable_detection(int class_ix, const char* label) {
  unsigned long now = millis();

  // Prevent repeated blinking for the same held gesture.
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
    rgb_off();                 // idle or unknown
    return;
  }

  Serial.print("ACT ");
  Serial.print(label);
  Serial.print(" @ ");
  Serial.print(now);
  Serial.println(" ms");

  last_acted_on = class_ix;
  last_act_ms = now;
}

// ----------------------------------------------------------------------------
// Act stage — blink the matching color once. Active-low RGB.
// ----------------------------------------------------------------------------
//void blink_color(int pin, int times = 1) {
//  for (int i = 0; i < times; i++) {
//    digitalWrite(pin, LOW);   delay(200);
//    digitalWrite(pin, HIGH);  delay(200);
//  }
//}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  rgb_off();

  if (!IMU.begin()) {
    Serial.println("IMU.begin() failed -- install Arduino_BMI270_BMM150");
    while (1) {}
  }

  // TODO(student): initialize the EI continuous-classifier pipeline.
  // The EI example sketch for IMU continuous inference calls
  // run_classifier_init(); then in the main loop fills a rolling buffer of
  // raw IMU samples and calls run_classifier_continuous() once per stride.
  //
  //run_classifier_init();
  //
  Serial.println("Lab 3 B.2 gesture inference ready");
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
  // Collect one 2-second IMU window at the model sampling frequency.
  // The classifier is then called repeatedly and the test-and-trace filter
  // smooths consecutive window predictions.
  const unsigned long sample_period_us = 1000000UL / EI_CLASSIFIER_FREQUENCY;
  unsigned long next_sample_us = micros();

  for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 6) {
    while ((long)(micros() - next_sample_us) < 0) {
      // wait until next sample time
    }
    next_sample_us += sample_period_us;

    while (!IMU.accelerationAvailable() || !IMU.gyroscopeAvailable()) {
      // wait for both accel and gyro
    }

    float ax, ay, az, gx, gy, gz;
    IMU.readAcceleration(ax, ay, az);   // units: g
    IMU.readGyroscope(gx, gy, gz);      // units: deg/sec

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

unsigned long inference_ms = result.timing.dsp + result.timing.classification;

Serial.print("dsp_ms=");
Serial.print(result.timing.dsp);

Serial.print(" cls_ms=");
Serial.print(result.timing.classification);

Serial.print(" total_ms=");
Serial.println(inference_ms);

  // Debug: print every class confidence so we can verify the model is changing.
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.print(ei_classifier_inferencing_categories[i]);
    Serial.print(": ");
    Serial.print(result.classification[i].value, 3);
    Serial.print("  ");
  }
  Serial.println();

  // Find top class.
  int top_ix = 0;
  float top_conf = result.classification[0].value;
  for (size_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > top_conf) {
      top_conf = result.classification[i].value;
      top_ix = i;
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
  Serial.print(" anomaly_ms=");
  Serial.println(result.timing.anomaly);

  // Confidence gate: weak windows are treated as idle.
  if (top_conf < kConfidenceMin) {
    top_ix = kIdleClassIx;
  }

  // Push into history buffer.
  history[history_head] = top_ix;
  history_head = (history_head + 1) % kHistoryN;

  int stable = majority_class();

  if (stable >= 0 && stable != kIdleClassIx) {
    const char* stable_label = ei_classifier_inferencing_categories[stable];
    act_on_stable_detection(stable, stable_label);
  }
  else if (stable == kIdleClassIx) {
    last_acted_on = kIdleClassIx;
    rgb_off();
  }
}