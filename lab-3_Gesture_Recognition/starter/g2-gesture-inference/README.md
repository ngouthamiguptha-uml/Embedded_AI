# g2-gesture-inference — continuous gesture classification + test-and-trace

This is the **deployment-side** scaffold for Lab 3 §B.2. It is a thin
wrapper around the **Arduino library that Edge Impulse generates** when
you export your trained project from §B.1. The EI library provides
continuous IMU inference; your job is:

1. Wire the EI `run_classifier_continuous()` loop to the BMI270 sample
   loop.
2. Implement the **test-and-trace post-processing filter** (Cookbook
   Ch 9) so a single noisy window does not trigger a spurious detection.
3. Implement the **act stage** — blink the on-board RGB LED in a
   per-gesture color when the filter emits a stable detection.

## Before this sketch will run

1. **Train and export your impulse in Edge Impulse** (handout §B.1).
   In *Deployment*, choose **Arduino library**, check **Quantized
   (int8)**, click **Build**, and download the resulting `.zip`.

2. **Install the library:** Arduino IDE → *Sketch* → *Include Library* →
   *Add .ZIP Library…* → select the EI `.zip` you just downloaded.

3. **Fix the `#include` in the `.ino` file** to match your project. The
   EI zip's header is named `<your-project-name>_inferencing.h`. The
   placeholder include in the sketch will fail to resolve until you
   change it.

The Edge Impulse-generated library ships a ready-made example you can
also start from:

```
File → Examples → <your-project-name> → nano_ble33_sense → nano_ble33_sense_accelerometer_continuous
```

Either start from that example and add your test-and-trace filter + act
stage, **or** fill in the `// TODO(student)` blocks in
`g2-gesture-inference.ino`. Both paths get the same credit on the
rubric.

## Why test-and-trace?

The classifier runs once per **window stride** (500 ms by default), not
once per gesture. Two seconds of gesturing produces ~4 classification
windows; per-window argmax flickers — a single noisy IMU sample can
briefly cross a class boundary. The filter is a small history buffer
(e.g. the last 3 classifications) that emits a "stable detected
gesture" only when **at least 2 of the last 3 windows agreed** on the
same non-`idle` class above the confidence threshold. This converts
noisy per-window predictions into clean per-gesture events — about one
LED blink per real gesture, not ten.

The filter is also the right place to **throttle the act stage** —
require a return to `idle` (or, simpler, 1 second of silence) before
re-arming, so holding a gesture doesn't re-blink the LED every 500 ms.
