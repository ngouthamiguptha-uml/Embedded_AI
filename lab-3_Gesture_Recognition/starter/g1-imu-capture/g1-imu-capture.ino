// Lab 3 · A.1 — IMU capture   (deliverable: g1-imu-capture.ino)
//
// Board: Arduino Nano 33 BLE Sense Rev2 (on-board BMI270 IMU).
// Library: Arduino_BMI270_BMM150 (install via Library Manager).
//
// The BMI270 library uses the SAME `IMU` object and method signatures as
// the Cookbook's Rev1-era Arduino_LSM9DS1 examples, so most of those
// snippets work after just swapping the include. We use the accelerometer
// (3 axes) and gyroscope (3 axes); the magnetometer is not used in this
// lab.
//
// Goal: at the chosen sample rate, read accel + gyro and print ONE CSV
// line per sample at 115200 baud, in the order Edge Impulse's data
// forwarder expects:
//
//   millis,ax,ay,az,gx,gy,gz
//
// Skeleton compiles/uploads as-is. Fill in each TODO(student).
//
// NOTE: USE 115200 BAUD, NOT 9600. At 100 Hz x 6 channels x CSV overhead,
// 9600 baud backs up and the forwarder drops samples — your dataset will
// silently be missing chunks. The Serial Monitor's baud-rate dropdown in
// the lower-right of the IDE must match.

#include <Arduino_BMI270_BMM150.h>

// TODO(student): pick your sample rate. The BMI270 supports 1.5625, 3.125,
// 6.25, 12.5, 25, 50, 100, 200, 400, 800, 1600 Hz on the accelerometer
// (similar on the gyro). The Arduino_BMI270_BMM150 library's default is
// 100 Hz on the accelerometer and 100 Hz on the gyro — use that for A.1.
// You will revisit this number for 5862 Option C.2 (power-vs-throughput).
const float kSampleRateHz = 100.0f;

// One sample period in microseconds. We'll busy-wait on micros() so the
// rate is measured against the sketch, not against IMU.read() blocking.
const unsigned long kSamplePeriodUs = (unsigned long)(1e6f / kSampleRateHz);

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }  // wait for the USB CDC port

  if (!IMU.begin()) {
    //Serial.println("IMU.begin() failed -- check that the");
    //Serial.println("Arduino_BMI270_BMM150 library is installed.");
    while (1) { ; }
  }

  // Print a CSV header so the Serial Monitor reads naturally. The Edge
  // Impulse data forwarder ignores non-numeric leading lines, so this is
  // safe to leave in when collecting EI samples.
  //Serial.println("millis,ax,ay,az,gx,gy,gz");

  // Print the library-reported sample rates for sanity. They should be
  // close to kSampleRateHz; the library coerces to the nearest supported
  // BMI270 step.
  //Serial.print("# accel rate = "); Serial.print(IMU.accelerationSampleRate());
  //Serial.print(" Hz, gyro rate = "); Serial.print(IMU.gyroscopeSampleRate());
  //Serial.println(" Hz");
}

void loop() {
  static unsigned long next_sample_us = 0;
  unsigned long now_us = micros();

  if ((long)(now_us - next_sample_us) < 0) return;   // not time yet
  next_sample_us = now_us + kSamplePeriodUs;

  // Both axes available? IMU.accelerationAvailable() / gyroscopeAvailable()
  // are the right gate; the BMI270 advances on its own clock and the
  // library buffers samples internally.
  if (!IMU.accelerationAvailable() || !IMU.gyroscopeAvailable()) return;

  float ax, ay, az, gx, gy, gz;
  IMU.readAcceleration(ax, ay, az);   // units: g
  IMU.readGyroscope(gx, gy, gz);      // units: deg/sec

  // TODO(student): print ONE CSV line in the exact order:
  //     millis(), ax, ay, az, gx, gy, gz
  // Use Serial.print() with commas (not tabs), and Serial.println() at the
  // end of the line. The handout's measured-sample-rate deliverable comes
  // from counting these lines per second.
  
 // Serial.print(millis()); Serial.print(',');
  Serial.print(ax, 4);    Serial.print(',');
  Serial.print(ay, 4);    Serial.print(',');
  Serial.print(az, 4);    Serial.print(',');
  Serial.print(gx, 4);    Serial.print(',');
  Serial.print(gy, 4);    Serial.print(',');
  Serial.println(gz, 4);

}
