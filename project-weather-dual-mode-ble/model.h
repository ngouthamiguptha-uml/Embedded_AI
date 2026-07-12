// Lab 2 · Track W · model.h  — PLACEHOLDER
//
// Replace the contents of this file with the C array exported from YOUR
// quantized weather model (B.1). The TFLite converter / xxd produces an
// `unsigned char` array plus its length, exactly like the hello-world
// model.cpp/model.h pair you read in Lab 1.
//
// Expected symbols (match what w2-weather-inference.ino references):
//   extern const unsigned char g_model[];
//   extern const int           g_model_len;
//
// Until you drop in your real model, this stub lets the sketch compile
// but it is NOT a working model — inference results are meaningless.

#ifndef LAB2_WEATHER_MODEL_H_
#define LAB2_WEATHER_MODEL_H_

extern const unsigned char g_model[];
extern const int g_model_len;

#endif  // LAB2_WEATHER_MODEL_H_
