# g3-ble-telemetry — (5862 C.1) BLE gesture event publishing

Extends `g2-gesture-inference/` with a **BLE peripheral** that
publishes one byte per **stable detected gesture** over a `GestureEvent`
notify characteristic. The raw IMU stream **never leaves the device** —
only the inference result does. This is the canonical AIoT pattern from
Lecture 11 in miniature.

## Before this sketch will run

1. Complete `g2-gesture-inference/` first; this sketch is a delta on
   top of it. Copy your working `#include` (the EI inferencing header)
   and your test-and-trace filter constants into this sketch.

2. Install the **`ArduinoBLE`** library via Library Manager.

3. Replace the placeholder include in the `.ino` file with your EI
   project's inferencing header, as in `g2-gesture-inference/`.

## What you'll see

After flashing this sketch:

- The on-board RGB LED **slow-pulses blue** while the board is
  advertising but no central is connected.
- Open the provided
  [`tools/web-ble-receiver.html`](../../tools/web-ble-receiver.html) —
  a self-hosted Web Bluetooth page that scans, connects, and logs
  incoming notifications. The setup recipe (one `python3 -m
  http.server` line) is in [`tools/README.md`](../../tools/README.md).
  iPhone-only students should use **Bluefruit LE Connect** (open
  source — see the tools README for the fallback path).
- The blue pulse goes solid once a central connects.
- Perform each gesture in front of the board. Each time the
  test-and-trace filter emits a stable detection, a **single byte**
  arrives at the receiver — your encoded gesture index (e.g.
  `0x01 = punch`, `0x02 = flex`, `0x03 = circle`).
- The RGB LED still blinks the matching color (the local act stage is
  unchanged from g2).

## Two paragraphs for your lab report

The handout (§C.1) asks you to compare what's transmitted vs. what's
locally inferred (the AIoT bandwidth argument: bytes-per-event vs.
kilobits-per-second of raw IMU), and to name one specific privacy /
responsibility risk the gesture stream still creates even though the
raw IMU never leaves. Both should reference HW 4 Q8 (AIoT) and HW 4
Q12 (responsible AI at the edge) explicitly.
