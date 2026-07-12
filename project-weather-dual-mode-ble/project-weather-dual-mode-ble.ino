// Final Project Extension: TinyML Weather Station
// Extensions on top of Lab 2:
//   1) Live Mode: uses real HS3003 temperature/humidity sensor readings
//   2) Test Mode: uses synthetic weather arrays for snow/no-snow validation
//   3) BLE Telemetry: sends temp, humidity, snow probability, prediction, latency
//
// Board: Arduino Nano 33 BLE Sense Rev2
// Required libraries:
//   - Arduino_HS300x
//   - ArduinoBLE
//   - Arduino_TensorFlowLite
// Required project files in the SAME sketch folder:
//   - model.h
//   - model.cpp

#include <Arduino_HS300x.h>
#include <ArduinoBLE.h>
#include <math.h>

#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>

#include "model.h"

// ---------------- Model/window configuration ----------------
constexpr int kWindowLen = 6;
constexpr int kFeaturesPerStep = 2;
constexpr int kInputLen = kWindowLen * kFeaturesPerStep;
constexpr unsigned long kSampleIntervalMs = 1000UL;

// Normalization constants from Lab 2 training notebook
constexpr float kMeanT = 8.718309f;
constexpr float kStdT  = 11.692684f;
constexpr float kMeanH = 67.345032f;
constexpr float kStdH  = 16.831001f;

float normTemp(float t) { return (t - kMeanT) / kStdT; }
float normHumidity(float h) { return (h - kMeanH) / kStdH; }

// ---------------- Synthetic test scenarios ----------------
struct WeatherSample {
  float tempC;
  float humidity;
};

const WeatherSample indoorWarm[] = {
  {23.5, 42.0}, {24.0, 41.0}, {24.4, 40.5},
  {24.8, 39.5}, {25.1, 39.0}, {25.3, 38.5}
};

const WeatherSample snowLikely[] = {
  {-0.5, 86.0}, {-1.2, 88.0}, {-1.8, 90.0},
  {-2.5, 92.0}, {-3.0, 94.0}, {-3.5, 96.0}
};

const WeatherSample rainLikely[] = {
  {7.0, 91.0}, {7.5, 93.0}, {8.0, 94.0},
  {8.5, 95.0}, {9.0, 96.0}, {9.5, 96.0}
};

const WeatherSample coldDry[] = {
  {-4.0, 35.0}, {-4.5, 34.0}, {-5.0, 32.0},
  {-5.5, 31.0}, {-6.0, 30.0}, {-6.5, 29.0}
};

enum Mode {
  MODE_LIVE,
  MODE_INDOOR_WARM,
  MODE_SNOW_LIKELY,
  MODE_RAIN_LIKELY,
  MODE_COLD_DRY
};

Mode currentMode = MODE_SNOW_LIKELY;  // default starts with professor's synthetic test idea
int syntheticIndex = 0;

// ---------------- BLE telemetry ----------------
BLEService weatherService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEStringCharacteristic telemetryChar(
  "19B10001-E8F2-537E-4F6C-D104768A1214",
  BLERead | BLENotify,
  120
);

// ---------------- TFLite Micro globals ----------------
namespace {
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

constexpr int kArenaSize = 8 * 1024;
alignas(16) uint8_t tensor_arena[kArenaSize];

float window[kInputLen];
int filled = 0;
unsigned long lastSample = 0;

unsigned long latencyMin = 4294967295UL;
unsigned long latencyMax = 0;
unsigned long latencySum = 0;
int latencyCount = 0;
int snowCount = 0;
int noSnowCount = 0;

void resetWindow() {
  for (int i = 0; i < kInputLen; i++) window[i] = 0.0f;
  filled = 0;
  syntheticIndex = 0;
}

void pushReading(float nt, float nh) {
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

int8_t quantize(float x) {
  int32_t q = lroundf(x / input->params.scale) + input->params.zero_point;
  if (q < -128) q = -128;
  if (q > 127) q = 127;
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
    }
  }
}

float dequantOutput(int index) {
  if (output->type == kTfLiteInt8) {
    return (output->data.int8[index] - output->params.zero_point) * output->params.scale;
  }
  if (output->type == kTfLiteFloat32) {
    return output->data.f[index];
  }
  return 0.0f;
}

// Lab 2 code used output[0] as snow probability. If your result looks inverted,
// change this function to return dequantOutput(1) instead.
float readSnowProbability() {
  return dequantOutput(0);
}

const char* modeName() {
  switch (currentMode) {
    case MODE_LIVE: return "LIVE";
    case MODE_INDOOR_WARM: return "TEST_INDOOR_WARM";
    case MODE_SNOW_LIKELY: return "TEST_SNOW_LIKELY";
    case MODE_RAIN_LIKELY: return "TEST_RAIN_LIKELY";
    case MODE_COLD_DRY: return "TEST_COLD_DRY";
  }
  return "UNKNOWN";
}

WeatherSample getSyntheticSample() {
  const WeatherSample* data = snowLikely;
  int len = 6;

  if (currentMode == MODE_INDOOR_WARM) data = indoorWarm;
  else if (currentMode == MODE_SNOW_LIKELY) data = snowLikely;
  else if (currentMode == MODE_RAIN_LIKELY) data = rainLikely;
  else if (currentMode == MODE_COLD_DRY) data = coldDry;

  WeatherSample sample = data[syntheticIndex % len];
  syntheticIndex++;
  return sample;
}

void printHelp() {
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  l = live HS3003 mode");
  Serial.println("  0 = synthetic indoor warm test");
  Serial.println("  1 = synthetic snow likely test");
  Serial.println("  2 = synthetic rain likely test");
  Serial.println("  3 = synthetic cold dry test");
  Serial.println();
}

void handleSerialCommand() {
  if (!Serial.available()) return;

  char c = Serial.read();
  if (c == 'l' || c == 'L') currentMode = MODE_LIVE;
  else if (c == '0') currentMode = MODE_INDOOR_WARM;
  else if (c == '1') currentMode = MODE_SNOW_LIKELY;
  else if (c == '2') currentMode = MODE_RAIN_LIKELY;
  else if (c == '3') currentMode = MODE_COLD_DRY;
  else return;

  resetWindow();
  Serial.print("Mode changed to: ");
  Serial.println(modeName());
}

}  // namespace

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  pinMode(LED_BUILTIN, OUTPUT);

  if (!HS300x.begin()) {
    Serial.println("Failed to initialize HS300x sensor. Live mode will not work.");
  }

  if (!BLE.begin()) {
    Serial.println("BLE initialization failed. Check ArduinoBLE library.");
    while (1) { ; }
  }

  BLE.setLocalName("TinyMLWeather");
  BLE.setAdvertisedService(weatherService);
  weatherService.addCharacteristic(telemetryChar);
  BLE.addService(weatherService);
  telemetryChar.writeValue("TinyML Weather BLE ready");
  BLE.advertise();

  model = tflite::GetModel(g_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema version mismatch. Check model.cpp/model.h.");
    while (1) { ; }
  }

  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
    model, resolver, tensor_arena, kArenaSize
  );
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("AllocateTensors() failed. Increase kArenaSize.");
    while (1) { ; }
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("TinyML Weather Station Project Started");
  Serial.println("Extension: Live/Test Mode + Synthetic Data + BLE Telemetry");
  Serial.print("BLE device name: ");
  Serial.println("TinyMLWeather");
  Serial.print("Default mode: ");
  Serial.println(modeName());
  printHelp();
}

void loop() {
  BLE.poll();
  handleSerialCommand();

  unsigned long now = millis();
  if (now - lastSample < kSampleIntervalMs) return;
  lastSample = now;

  WeatherSample sample;

  if (currentMode == MODE_LIVE) {
    sample.tempC = HS300x.readTemperature();
    sample.humidity = HS300x.readHumidity();
  } else {
    sample = getSyntheticSample();
  }

  pushReading(normTemp(sample.tempC), normHumidity(sample.humidity));

  if (filled < kWindowLen) {
    Serial.print("mode=");
    Serial.print(modeName());
    Serial.print(", temp_c=");
    Serial.print(sample.tempC, 2);
    Serial.print(", humidity_pct=");
    Serial.print(sample.humidity, 2);
    Serial.print(", filling_window=");
    Serial.print(filled);
    Serial.print("/");
    Serial.println(kWindowLen);
    return;
  }

  copyWindowToInputTensor();

  unsigned long startUs = micros();
  TfLiteStatus status = interpreter->Invoke();
  unsigned long elapsedUs = micros() - startUs;

  if (status != kTfLiteOk) {
    Serial.println("Invoke failed");
    return;
  }

  if (elapsedUs < latencyMin) latencyMin = elapsedUs;
  if (elapsedUs > latencyMax) latencyMax = elapsedUs;
  latencySum += elapsedUs;
  latencyCount++;

  float snowProb = readSnowProbability();
  bool snowLikely = snowProb > 0.50f;

  if (snowLikely) snowCount++;
  else noSnowCount++;

  digitalWrite(LED_BUILTIN, snowLikely ? HIGH : LOW);

  String payload = "mode=";
  payload += modeName();
  payload += ",temp_c=";
  payload += String(sample.tempC, 2);
  payload += ",humidity_pct=";
  payload += String(sample.humidity, 2);
  payload += ",snow_prob=";
  payload += String(snowProb, 4);
  payload += ",prediction=";
  payload += (snowLikely ? "SNOW_LIKELY" : "NO_SNOW");
  payload += ",latency_us=";
  payload += String(elapsedUs);
  payload += ",snow_count=";
  payload += String(snowCount);
  payload += ",no_snow_count=";
  payload += String(noSnowCount);

  Serial.println(payload);
  telemetryChar.writeValue(payload);

  if (latencyCount > 0 && latencyCount % 25 == 0) {
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
