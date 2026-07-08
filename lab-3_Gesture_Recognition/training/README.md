# Lab 3 — training in Edge Impulse

Unlike Lab 2 Track W, Lab 3 has **no bundled training notebook or
dataset**. The training pipeline lives entirely in the **Edge Impulse
Studio** — you collect your own IMU samples in the Studio (or via the
data forwarder pointed at the `g1-imu-capture/` sketch), design an
impulse with **Spectral Analysis** features and a **classifier**
learning block, train it in the browser, and export a quantized
Arduino library that the `g2-gesture-inference/` sketch links against.

The handout (§B.1) is the authoritative procedure. This folder is
intentionally empty other than this note — there is nothing in `lab-3/`
to commit as training data. **Your** dataset lives in your Edge
Impulse project; share your dataset with the instructor by including a
**link to your public Edge Impulse project** in your lab report.

## Quick links

- The Edge Impulse end-to-end IMU tutorial (the canonical recipe Lab 3
  follows):
  <https://docs.edgeimpulse.com/docs/tutorials/end-to-end-tutorials/continuous-motion-recognition>
- The Spectral Analysis DSP block reference (what the §A.2 explanation
  is about):
  <https://docs.edgeimpulse.com/docs/edge-impulse-studio/processing-blocks/spectral-features>
- The Edge Impulse data forwarder (for streaming your `g1-imu-capture`
  CSV into the Studio's *Data acquisition* page):
  <https://docs.edgeimpulse.com/docs/tools/edge-impulse-cli/cli-data-forwarder>

## Why no bundled notebook?

Lab 2 Track W deliberately exposed the TensorFlow → TFLite → C-array
pipeline so students could see the full conversion from a host-trained
Keras model to a TFLM-runnable byte blob. Lab 3 is the next step:
**the model behavior is the variable, not the toolchain**. Edge Impulse
handles the training + DSP + quantization + export in a single
workflow; that lets the lab focus on the things that distinguish Lab 3
from Lab 2 — **continuous classification**, **test-and-trace**, the
**optimization study** (Lecture 9), and (5862) the **AIoT pattern**
(Lecture 11).
