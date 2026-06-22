# Track W · B.2 starter — on-device weather inference

This folder is the deployment scaffold for Track W. It expects:

- **`Arduino_HS300x`** — on-board temperature/humidity sensor.
- **`Arduino_TensorFlowLite`** — the same TFLite Micro library you
  installed in Lab 1 (Add `.ZIP` Library →
  `../../../lab-1/Arduino_TensorFlowLite.zip`).

## What the scaffold already does for you

To keep this at roughly Lab 1's effort level, the sketch **provides**:

- the full TFLM interpreter setup (resolver, interpreter, arena),
- the rolling **feature-window buffer** (`window[]` + `pushReading()`),
- the **int8 `quantize()` helper** (reads the model's own scale /
  zero-point),
- an **accelerated demo cadence** (`kSampleIntervalMs`) so the window
  fills in a minute or two instead of hours.

You fill in three small, focused things: (1) read + **normalize** the
sensor and push it into the window, (2) run `Invoke()` and **act** on
the result, (3) the B.3 latency timing.

## Drop in your model

`model.h` / `model.cpp` ship as **placeholders** (a meaningless 1-byte
array so the sketch links). Replace `model.cpp` with the array your
training notebook exported in B.1, keeping the symbol names `g_model`
and `g_model_len`. This is the same form as Lab 1's
`hello_world/model.cpp`.

## Match the notebook

Two things in the sketch **must match your training notebook**, or the
model will see garbage:

- **`kWindowLen`** — the number of time-steps in the input window.
- **`normTemp()` / `normHumidity()`** — the exact feature normalization
  (e.g. mean/std) the notebook applied before training. The notebook
  prints these constants; paste them in.

## Demo cadence vs. real cadence

`kSampleIntervalMs` is set fast (10 s/step) so you can see a prediction
at your desk without waiting hours. The notebook documents the
real-world sampling interval the model was trained for; note the
difference in your report (B.3 reflection).
