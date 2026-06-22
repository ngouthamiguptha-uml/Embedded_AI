// Lab 2 · Track W · B.2 — On-device weather inference   (deliverable: w2-weather-inference.ino)
//
// Board: Arduino Nano 33 BLE Sense Rev2.
// Libraries: Arduino_HS300x (sensor) + Arduino_TensorFlowLite (from Lab 1).
//
// Same TFLite Micro pattern as Lab 1's hello_world, but the input is a
// rolling WINDOW of live temperature/humidity and the output drives an
// "act" stage. To keep this at Lab-1 effort, the scaffold already
// PROVIDES: the interpreter setup, the rolling window buffer, the int8
// quantization helper, and an accelerated demo cadence. You fill only:
//   (1) reading + normalizing the sensor and pushing it into the window,
//   (2) running Invoke() and acting on the result,
//   (3) the B.3 latency instrumentation.
//
// Before this means anything: replace model.cpp with the array your
// training notebook (B.1) exported. The shipped model.cpp is a stub.

#include <Arduino_HS300x.h>

#include <math.h>

#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_log.h>
#include <tensorflow/lite/schema/schema_generated.h>

#include "model.h"

// ---- Window / feature configuration -----------------------------------
// MUST MATCH THE TRAINING NOTEBOOK. The model reasons over a window of the
// last kWindowLen (temperature, humidity) readings. The notebook prints
// these values; copy them here.
constexpr int kWindowLen = 6;        // TODO(student): set to the notebook's window length
constexpr int kFeaturesPerStep = 2;  // temperature + humidity
constexpr int kInputLen = kWindowLen * kFeaturesPerStep;

// Demo cadence: real weather history spans hours, but for the bench demo
// we sample fast so the window fills in ~a minute. Raise for a slower,
// more "real" cadence; the notebook documents the real-world interval.
constexpr unsigned long kSampleIntervalMs = 1000UL;  // 10 s per step (accelerated)

// ---- Feature normalization (from the notebook) -------------------------
// The notebook normalizes inputs before training; the device MUST apply
// the identical transform. TODO(student): paste the constants the
// notebook prints (e.g. per-feature mean/std, or min/max).
constexpr float kMeanT = 8.718309f;
constexpr float kStdT  = 11.692684f;
constexpr float kMeanH = 67.345032f;
constexpr float kStdH  = 16.831001f;

float normTemp(float t) {
  return (t - kMeanT) / kStdT;
}

float normHumidity(float h) {
  return (h - kMeanH) / kStdH;
}

namespace {
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

// PROVIDED: tensor arena. Grow if AllocateTensors() ever fails.
constexpr int kArenaSize = 8 * 1024;
alignas(16) uint8_t tensor_arena[kArenaSize];

// PROVIDED: rolling window of normalized features (oldest -> newest).
float window[kInputLen];
int filled = 0;            // how many feature-steps we've collected so far
unsigned long lastSample = 0;

unsigned long latencyMin = 4294967295UL;
unsigned long latencyMax = 0;
unsigned long latencySum = 0;
int latencyCount = 0;

// PROVIDED: push one (already-normalized) reading, shifting older data out.
void pushReading(float nt, float nh) {
  // shift left by one step
  for (int i = 0; i < (kWindowLen - 1) * kFeaturesPerStep; i++) {
    window[i] = window[i + kFeaturesPerStep];
  }
  window[kInputLen - 2] = nt;
  window[kInputLen - 1] = nh;
  if (filled < kWindowLen) filled++;
}

int tensorElementCount(TfLiteTensor* tensor) {
  int count = 1;
  for (int i = 0; i < tensor->dims->size; i++) {
    count *= tensor->dims->data[i];
  }
  return count;
}


// PROVIDED: quantize a float to the input tensor's int8 scale/zero-point.
int8_t quantize(float x) {
  int32_t q = lroundf(x / input->params.scale) + input->params.zero_point;
  if (q < -128) q = -128;
  if (q >  127) q =  127;
  return (int8_t)q;
}

void copyWindowToInputTensor() {
  int inputCount = tensorElementCount(input);
  int count = min(inputCount, kInputLen);

  for (int i = 0; i < count; i++) {
    if (input->type == kTfLiteInt8) {
      input->data.int8[i] = quantize(window[i]);
    } else if (input->type == kTfLiteFloat32) {
      input->data.f[i] = window[i];
    } else {
      Serial.println("Unsupported input tensor type");
      return;
    }
  }
}

float readSnowProbability() {
  if (output->type == kTfLiteInt8) {
    return (output->data.int8[0] - output->params.zero_point) * output->params.scale;
  }

  if (output->type == kTfLiteFloat32) {
    return output->data.f[0];
  }

  return 0.0f;
}

void printLatencySummary() {
  if (latencyCount > 0 && latencyCount % 100 == 0) {
    Serial.print("[LAT] samples=");
    Serial.print(latencyCount);
    Serial.print(" min_us=");
    Serial.print(latencyMin);
    Serial.print(" mean_us=");
    Serial.print((float)latencySum / (float)latencyCount, 2);
    Serial.print(" max_us=");
    Serial.println(latencyMax);
  }
}

}  // namespace

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  if (!HS300x.begin()) {
    Serial.println("Failed to initialize HS300x sensor!");
    while (1) { ; }
  }
  pinMode(LED_BUILTIN, OUTPUT);

  model = tflite::GetModel(g_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema version mismatch! (is model.cpp your real export?)");
    while (1) { ; }
  }

  // PROVIDED: standard TFLM setup (mirrors Lab 1 hello_world).
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kArenaSize);
  interpreter = &static_interpreter;
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("AllocateTensors() failed — raise kArenaSize.");
    while (1) { ; }
  }
  input  = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("Track W weather inference started");
  Serial.print("kWindowLen=");
  Serial.println(kWindowLen);
  Serial.print("kInputLen=");
  Serial.println(kInputLen);
  Serial.println("Collecting live temperature/humidity readings...");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSample < kSampleIntervalMs) return;
  lastSample = now;

  // TODO(student): read the sensor, normalize with normTemp/normHumidity,
  // and push into the window:
  //   float t = HS300x.readTemperature();
  //   float h = HS300x.readHumidity();
  //   pushReading(normTemp(t), normHumidity(h));

  float tempC = HS300x.readTemperature();
  float humidity = HS300x.readHumidity();

  Serial.print("raw_temp_c=");
  Serial.print(tempC, 2);
  Serial.print(", raw_humidity_pct=");
  Serial.print(humidity, 2);
  Serial.print(", ");

  float nTemp = normTemp(tempC);
  float nHum = normHumidity(humidity);

  pushReading(nTemp, nHum);

  if (filled < kWindowLen) {
    Serial.print("filling window "); Serial.print(filled);
    Serial.print("/"); Serial.println(kWindowLen);
    return;  // not enough history yet for a prediction
  }

  // PROVIDED: copy the (quantized) window into the input tensor.
  //for (int i = 0; i < kInputLen; i++) input->data.int8[i] = quantize(window[i]);

    copyWindowToInputTensor();

  // TODO(student) + B.3: run inference (wrap in micros() for the latency
  // table; print with Serial, not MicroPrintf %lu):
  //   if (interpreter->Invoke() != kTfLiteOk) { Serial.println("Invoke failed"); return; }

  // TODO(student): read output(0), dequantize, threshold, and ACT:
  //   float p = (output->data.int8[0] - output->params.zero_point) * output->params.scale;
  //   bool snow = p > 0.5f;
  //   digitalWrite(LED_BUILTIN, snow);
  //   Serial.println(snow ? "SNOW LIKELY" : "NO SNOW");

  unsigned long startUs = micros();
  TfLiteStatus status = interpreter->Invoke();
  unsigned long elapsedUs = micros() - startUs;

if (elapsedUs < latencyMin) latencyMin = elapsedUs;
if (elapsedUs > latencyMax) latencyMax = elapsedUs;

latencySum += elapsedUs;
latencyCount++;

if (latencyCount % 100 == 0) {
  Serial.print("[LAT] samples=");
  Serial.print(latencyCount);
  Serial.print(" min_us=");
  Serial.print(latencyMin);
  Serial.print(" mean_us=");
  Serial.print((float)latencySum / latencyCount, 2);
  Serial.print(" max_us=");
  Serial.println(latencyMax);
}

  if (status != kTfLiteOk) {
    Serial.println("Invoke failed");
    return;
  }

  if (elapsedUs < latencyMin) latencyMin = elapsedUs;
  if (elapsedUs > latencyMax) latencyMax = elapsedUs;
  latencySum += elapsedUs;
  latencyCount++;

  float snowProb = readSnowProbability();
  bool snowLikely = snowProb > 0.5f;

  digitalWrite(LED_BUILTIN, snowLikely ? HIGH : LOW);

  Serial.print("prediction=");
  Serial.print(snowLikely ? "SNOW LIKELY" : "NO SNOW");
  Serial.print(", snow_prob=");
  Serial.print(snowProb, 4);
  Serial.print(", latency_us=");
  Serial.println(elapsedUs);

  printLatencySummary();
}
