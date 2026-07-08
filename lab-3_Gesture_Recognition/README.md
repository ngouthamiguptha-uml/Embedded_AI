# Lab 3 — Gesture Recognition on the Nano

**EECE.4862 / 5862 — Embedded Artificial Intelligence · Summer 2026**

| | |
|---|---|
| **Released** | Monday, June 22, 2026 |
| **Due** | Sunday, July 12, 2026, 11:59 PM ET |
| **Points** | 20 (4862) / 18 (5862) |
| **Anchors** | L8, L9, L10, L11 |
| **Reading** | *TinyML Cookbook* Ch 9 |
| **Late policy** | 4 % per day; 20 % per week |

---

## 1. Objectives

Lab 2 deployed a model that classifies **one window at a time** — you
spoke a word, the model decided, the LED blinked, then it waited for the
next window. Lab 3 is the capstone: you build a model that classifies
**continuously**, on a sliding window of live IMU data, while
**concurrent sensor sampling, inference, and post-processing** all run
on the Nano at the same time. You also do an **optimization study**
(Lecture 9) and report the **real-time performance metrics** (Lecture 10)
that decide whether your model is shippable. **5862 students** extend
this into a small **AIoT system** (Lecture 11) by streaming inference
results — *not raw data* — over **BLE**, or by characterizing the
**power-vs-throughput** tradeoff at multiple sampling rates.

By the end of this lab you will be able to:

1. Capture 6-axis **IMU** data (accelerometer + gyroscope) from the
   on-board **BMI270** and stream it over Serial for off-device training.
2. Train and tune a small gesture classifier in **Edge Impulse** on
   **spectral analysis** features and **quantize it to int8** for the
   microcontroller.
3. Deploy the model to the Nano and run **continuous classification**
   with a **test-and-trace post-processing filter** that smooths
   spurious single-window predictions into stable gesture events.
4. Measure and report the deployed system's **end-to-end latency**,
   **classifications per second**, **flash and RAM usage**, and apply
   **one optimization** from Lecture 9 (model variant, feature window,
   or quantization choice) and compare before/after.
5. *(5862 only)* Either turn the device into a small **AIoT endpoint**
   that publishes confident gesture events over **BLE**, or characterize
   the **power-vs-throughput** tradeoff at three sampling rates.

---

## 2. Required hardware and software

### Hardware

- Arduino Nano 33 BLE Sense Rev2 (SKU **ABX00070**) with headers
- USB-A to micro-USB **data** cable (not power-only)
- *(5862 C.1 only)* A laptop or Android phone running **Chrome or
  Edge** for the provided **Web Bluetooth receiver page**
  (`tools/web-ble-receiver.html` in this lab — fully open source, no
  app install needed). iOS Safari does **not** support Web Bluetooth;
  iPhone-only students should use **Bluefruit LE Connect** (Adafruit;
  iOS/Android; sources at
  <https://github.com/adafruit/Bluefruit_LE_Connect_v2>) as the
  open-source fallback.

Lab 3 runs entirely on **on-board** peripherals — no breadboard wiring
or extra instruments are required. C.2 uses datasheet lookups instead
of a power meter (see §6).

### Software (common)

- **Arduino IDE 2.x** with the **Arduino Mbed OS Nano Boards** package
  (same as Lab 1 / Lab 2).
- **`Arduino_BMI270_BMM150`** library — install via Library Manager.
  The Rev2 ships with the **BMI270** 6-axis IMU and the **BMM150**
  magnetometer. (The Cookbook's IMU recipes target the older Rev1's
  `Arduino_LSM9DS1`; the BMI270 library exposes the same `IMU` object
  and method signatures, so most Cookbook snippets work after the one
  include swap. We use **accelerometer + gyroscope** for this lab; the
  magnetometer is not used.)
- **Edge Impulse** account (free) — <https://studio.edgeimpulse.com/>.
  You already have one from Lab 2 Track K; reuse it.
- **Edge Impulse CLI** (`npm install -g edge-impulse-cli`) — for the
  data forwarder. The Edge Impulse phone app also works for IMU samples.
- The Edge Impulse–generated **Arduino library** (you export this from
  your project and add it via **Sketch → Include Library → Add .ZIP
  Library…**, exactly as in Lab 2 Track K).

### Software — 5862 C.1 only

- **`ArduinoBLE`** library — install via Library Manager. Provides the
  BLE peripheral API used in `starter/g3-ble-telemetry/`.

---

## 3. Background

Read **Chapter 9 of the *TinyML Cookbook*** (2nd Ed., Iodice, Packt
2023) — *"Recognizing music genres with TensorFlow and the Arduino Nano
33 BLE Sense Rev2"* and the gesture-recognition recipes alongside it —
before starting. Chapter 9 walks through reading the on-board IMU,
acquiring labeled samples via Edge Impulse, designing the impulse
(spectral features + small classifier), training, deploying with
continuous classification, and applying a **test-and-trace** filter to
turn noisy per-window predictions into stable detected events.

For the lecture context, Lab 3 is where Lectures 8–11 land on real
hardware:

- **Lecture 8 — Model Deployment on Edge Devices** → **Part B.2**:
  the model runs on the Nano; the deployment cliff (host → MCU input
  scale and quantization) is the same one you crossed in Lab 2.
- **Lecture 9 — Model Optimization** → **Part B.3**: you apply one
  optimization technique (quantization variant, feature-window size, or
  the EON Tuner's architecture search) and report what it cost and what
  it bought.
- **Lecture 10 — Performance Metrics & Timing Analysis** → **Part B.3**:
  the real-time metrics — end-to-end latency, classifications per
  second, RAM / flash budget, *(5862 C.2)* current draw — are what
  decide whether the deployed system is good enough.
- **Lecture 11 — AIoT Applications** → **Part C.1**: the canonical AIoT
  pattern is on-device inference followed by *low-rate radio
  transmission of inference results, not raw data* (HW 4 Q8). C.1 is a
  miniature instance of exactly that pattern.

---

## 4. Part A — Sense: capture IMU gesture data (6 points)

### A.1 — Read the IMU and stream it over Serial (3 pts)

Start from [`starter/g1-imu-capture/`](starter/g1-imu-capture/)
(structure + `// TODO(student)` hints). Write a sketch that initializes
the BMI270 via `Arduino_BMI270_BMM150` and, at a configurable
**sampling rate** (start with **100 Hz** — the BMI270's standard
accelerometer rate), reads the 3-axis accelerometer **and** 3-axis
gyroscope, then prints them to Serial as one **CSV line** per sample at
115200 baud:

```
millis,ax,ay,az,gx,gy,gz
```

Use **115200 baud** (not 9600) — at 100 Hz × 6 channels × CSV overhead,
the 9600 stream backs up and the data forwarder drops samples. The
sketch's `Serial.begin(115200);` is the line to match.

This is also the sketch you will point the **Edge Impulse data
forwarder** at to collect labeled gesture samples in B.1.

**Deliverable for A.1:** the sketch (`g1-imu-capture.ino`), a Serial
Monitor screenshot showing ≥ 20 lines of streaming data while you wave
the board, and the **measured sample rate** (count lines per second
from a one-second capture and report the number — it should be close
to 100 but won't be exact).

### A.2 — From raw IMU samples to model features (3 pts)

You will not feed raw IMU samples straight into the classifier — at
100 Hz over a 2-second gesture window, that's 1,200 raw scalars per
window, which is both too many features for a small dense net to learn
from and too noisy to generalize across people. Edge Impulse's
**Spectral Analysis DSP block** does the feature reduction for you.
Explain, in your lab report:

(a) **What the Spectral Analysis block computes**, in plain terms. For
each channel (the 3 axes of the accelerometer and 3 of the gyroscope),
the block extracts a small number of **time-domain statistics** (RMS,
peak, etc.) and **frequency-domain features** (the FFT magnitudes at a
few binned frequencies, plus spectral power summaries). Roughly how
many features per channel does the default configuration produce, and
therefore how many total features does the model see per window?

(b) **Why a frequency-domain representation is useful here.** Tie it
explicitly to a concept from **Lecture 6** (sampling, Nyquist, the
affine sensor model) or **Lecture 7** (feature representations that
generalize across subjects, dimensionality reduction). For example: a
shake at 4 Hz looks the same in frequency-domain regardless of when
within the window it happened, so frequency features are
*translation-invariant in time* — a property the raw waveform lacks.

(c) **Sketch (in pseudocode or with a small figure)** how a 2-second
window of 6-channel IMU samples (≈ 1,248 scalars) becomes the ~50–60
features the model actually receives. Identify which step you would
have to write yourself if you were not using Edge Impulse.

**Deliverable for A.2:** the written explanation (½–1 page) with the
sketch / diagram.

---

## 5. Part B — Infer + Act: train, deploy, run continuously (12 points)

### B.1 — Train the model in Edge Impulse (4 pts)

Build a **Spectral Analysis + dense (or small CNN) impulse** in **Edge
Impulse** to recognize **three gestures** of your choice **plus an
`idle` background class** (four classes total). Suggested gesture set:
**`punch`**, **`flex`**, **`circle`** — but any three motions that are
visibly distinct on the IMU work. The fourth class, `idle`, is the
board sitting still (or moving slowly with the user walking around); it
prevents every small wobble from triggering a spurious gesture
prediction at deployment time. Work through the Studio:

1. **Create the project.** Sign in at <https://studio.edgeimpulse.com/>,
   create a new project, and set the device type to **Arduino Nano 33
   BLE Sense**.
2. **Collect data.** Record **at least ~30 examples per class** (so
   ≥ 120 samples total), each **~2 seconds long**. Either point the
   **`edge-impulse-data-forwarder`** CLI at your A.1 sketch (configured
   for 6 channels at your sample rate) on the *Data acquisition* page,
   or use the Edge Impulse phone app's IMU collection. The data
   forwarder will detect your CSV format from the A.1 sketch
   automatically; label `axis_0..axis_5` as `accX, accY, accZ, gyrX,
   gyrY, gyrZ` in the Studio.
3. **Design the impulse** (*Create impulse*): a **Time-series** input
   block with **window size 2000 ms**, **window increase (stride)
   500 ms**, at your A.1 sample rate (100 Hz typical); a **Processing
   block → Spectral Analysis** (default parameters); and a **Learning
   block → Classification** (the default small dense or 1-D CNN).
4. **Generate features** and inspect the **Feature explorer** — your
   four classes should form visibly distinct clusters in the 2-D
   projection. If `idle` overlaps badly with the gestures, your
   gestures are too gentle — re-collect with sharper motions.
5. **Train** the classifier (~100 epochs) and record the **validation
   accuracy** and **confusion matrix** the Studio reports. For the
   deployment build (B.2), check **Quantized (int8)** in *Deployment*
   and record the on-device flash / RAM / latency **estimates** the
   Studio prints; you will check these against the real device in B.3.

**Deliverable for B.1:** screenshots of the **impulse design** page,
the **Feature explorer**, and the **Model** page (validation accuracy +
confusion matrix); plus the per-class **precision / recall / F1** from
the *Model testing* page. State the four classes you used.

### B.2 — Deploy with continuous classification + test-and-trace (5 pts)

Export the Edge Impulse **Arduino library** for your project
(*Deployment* → *Arduino library* → **Quantized (int8)** → *Build* →
download `.zip`). Install via **Sketch → Include Library → Add .ZIP
Library…** in the Arduino IDE.

Start from
[`starter/g2-gesture-inference/`](starter/g2-gesture-inference/). The
scaffold sets up the IMU sample loop and the EI continuous-classifier
call; the parts you fill in are small and focused:

(a) **Continuous classification.** Run inference once per **window
stride** (500 ms by default), not once per 2-second window. The EI
runtime exposes `run_classifier_continuous()`, which maintains the
rolling buffer for you. Each call returns probabilities for all four
classes.

(b) **Test-and-trace post-processing filter.** A raw per-window argmax
will flicker — a single noisy reading can briefly cross into a wrong
class. Implement the **test-and-trace** filter from Cookbook Ch 9:
maintain a small history (e.g. **last 3 classifications**) and emit a
**stable detected gesture** only when **at least 2 of the last 3**
windows agreed on the same non-`idle` class **above your confidence
threshold** (start at 0.7). This turns ~10 noisy per-window predictions
per second into a few clean detection events per gesture.

The starter scaffold declares the filter as **four tunable knobs** at
the top of the sketch. Read the table below before you tweak anything —
the *direction* you turn each one matters more than the absolute value:

| Constant | What it controls | Tune **up** if… | Tune **down** if… |
|---|---|---|---|
| `kHistoryN` | how many recent windows the filter remembers | filter still flickers at the default | filter is sluggish (slow to react) |
| `kAgreeMin` | how many of `kHistoryN` must agree to emit a stable detection | false fires on a single noisy window | a real, sharp gesture is being missed |
| `kConfidenceMin` | minimum top-class probability that counts as a "real" prediction (otherwise treated as `idle`) | false fires from background motion | clean, deliberate gestures are getting discarded as "idle" |
| `kReArmIdleMs` | minimum gap between two same-class detections (prevents a held gesture from re-firing every window) | the LED re-blinks during a single held gesture | rapid repeats of the same gesture are getting dropped |

`kConfidenceMin` is the most consequential and the least obvious — it
exists because **argmax alone is a coin flip** when the softmax output
is roughly uniform (e.g. `[0.30, 0.25, 0.25, 0.20]` argmaxes to class 0,
but no human watching would call that a real detection). The threshold
gates the history buffer so weak windows don't latch onto an accidental
agreement; this is a direct application of the **ROC operating-point
framing** from L10 — you are picking where on the
*precision-vs-recall* curve the deployed detector lives.

You also need to tell the filter **which class index is `idle`**. The
Edge Impulse Studio assigns indices to your labels alphabetically when
it generates `model_metadata.h`, so for the suggested 4-class set
(`circle`, `flex`, `idle`, `punch`) the value is **2**. **Verify in your
own generated `model_metadata.h`** by reading the
`ei_classifier_inferencing_categories[]` array — if you used different
class names, your `idle` index will be different and the filter will
behave incorrectly until you fix it.

(c) **Act on a stable detection.** When the filter emits a stable
gesture, **blink the on-board RGB LED** in a per-gesture color (e.g.
`punch` → red, `flex` → green, `circle` → blue, `idle` → off). Do
**nothing** for `idle` or low-confidence windows. Throttle the act
stage so the same continuous gesture doesn't re-blink every 500 ms —
require a return to `idle` (or 1 second of silence) before re-arming.

**Concurrency note.** The Edge Impulse `run_classifier_continuous()`
runtime drives the IMU sample loop and the classifier from a single
Mbed OS thread for you — you do **not** need to spawn threads
explicitly. (If you choose to, the Cookbook's `rtos::Thread` pattern
from Ch 9 will work; the rubric does not require it.)

**Deliverable for B.2:** the completed sketch
(`g2-gesture-inference.ino`), and a **video or stitched screenshots**
showing the board responding to live gestures — each of your three
gestures triggering its matching LED color, and `idle` producing no
output.

### B.3 — Measure + an optimization study (3 pts)

Report, in a small table in your lab report, your **baseline** numbers
on the Nano:

- **Per-class accuracy** — from the EI *Model testing* page.
- **End-to-end latency** — the EI runtime prints DSP + classification
  timing in `result.timing` per call. Report **min / mean / max** over
  ≥ 100 inferences. **Note:** `result.timing` measures the **inference
  cost only** (DSP + model forward pass). Your user-perceived
  detection latency is *that plus the filter latency* — roughly
  `window_stride × kAgreeMin`, since the filter has to see `kAgreeMin`
  agreeing windows before it emits a stable detection. At 500 ms stride
  and `kAgreeMin = 2`, the filter adds ~1 second. Report this as a
  separate row so the table reflects the full L10 *real-time
  performance* picture.
- **Classifications per second** — for continuous classification at
  500 ms stride, this is ~2/s; verify by counting `Serial.print` lines
  per second of stable measurement.
- **Flash and RAM usage** — from the Arduino IDE compile output.

> **Carry over the Lab 1 measurement caveats.** Print `unsigned long`
> timing with `Serial.print()`, not `MicroPrintf %lu`. The nRF52840's
> `micros()` has ~4 µs granularity, so reporting the **mean** over many
> windows is the meaningful number.

Then apply **one optimization** from Lecture 9 and re-measure. Pick
**one**:

- **(i) Quantization variant.** Re-export EI's deployment as
  **unoptimized (float32)** and re-measure. Compare to the int8
  baseline. Expected direction: int8 is smaller and faster, with a
  small accuracy hit. Report whether your numbers match the L9
  expectation.
- **(ii) Window size.** Halve the window (2000 ms → 1000 ms) in the
  impulse design, retrain, re-deploy, re-measure. Expected direction:
  smaller window → faster inference, smaller arena, possibly worse
  accuracy on slower gestures.
- **(iii) EON Tuner.** Run the **EON Tuner** in Edge Impulse to search
  for a smaller/faster architecture at similar accuracy, deploy the
  Studio's recommended winner, re-measure. (This is the closest L9
  technique to NAS for MCUs.)

Then write **one short paragraph** (3–5 sentences) tying the result to
a specific L9 / L10 idea — name the technique (operator fusion, int8
PTQ, NAS, window-as-bandwidth-knob) by name.

**Deliverable for B.3:** the **baseline + after-optimization
measurement table** (4 columns: accuracy, latency mean, flash, RAM —
two rows), and the analysis paragraph.

---

## 6. Part C — 5862-only deepening (4 points)

**5862 students must complete one** of the options below. 4862 students
may attempt one for bonus credit (up to +2 pts toward the final lab
grade, capped at 20).

### Option C.1 — BLE telemetry (the AIoT pattern)

Turn your gesture detector into a small **AIoT endpoint** that
publishes a one-byte gesture event over **BLE** every time the
test-and-trace filter emits a stable detection. The raw IMU stream
**never leaves the device** — only the inference *result* does
(per HW 4 Q8 and the L11 AIoT framing).

Start from [`starter/g3-ble-telemetry/`](starter/g3-ble-telemetry/),
which extends `g2-gesture-inference/` with a `Gesture` BLE service
(`ArduinoBLE` library). The scaffold defines:

- A **single characteristic** (`GestureEvent`, `BLENotify`) carrying
  one byte per event: `0x00 = idle, 0x01 = punch, 0x02 = flex,
  0x03 = circle` (or your encoding).
- A connection-state LED indicator (slow pulse when advertising, solid
  when a central is connected).

**Receive the stream** with the provided
[`tools/web-ble-receiver.html`](tools/web-ble-receiver.html) — a
single-file **Web Bluetooth** page that scans, connects, subscribes,
and shows each incoming byte (with the gesture label). It uses no app
install, requires no account, and you can read its ~150 lines of
source. Setup is in [`tools/README.md`](tools/README.md); the short
version is `python3 -m http.server 8080` then open
`http://localhost:8080/web-ble-receiver.html` in **Chrome or Edge**
(desktop *or* Android — iOS Safari does not support Web Bluetooth).

(iPhone-only fallback: **Bluefruit LE Connect** —
<https://github.com/adafruit/Bluefruit_LE_Connect_v2> — open-source
Adafruit app on the iOS App Store. Connect, subscribe to the
characteristic, watch the byte stream.)

Two short paragraphs in your lab report:

1. **What the radio actually transmits.** Quantify the data-rate
   savings vs. streaming raw IMU. Streaming 6 channels of float32 at
   100 Hz over BLE would be ~19 Kbit/s of payload; your telemetry is a
   handful of bytes per *gesture event* — orders of magnitude less.
   Tie this to the L11 AIoT power-budget argument: the radio is the
   most expensive component (HW 4 Q7), and on-device inference exists
   in large part to keep the radio off most of the time.
2. **One specific privacy risk** that the system still has *even
   though raw IMU never leaves the device*. (Hint: the gesture stream
   is itself behavioral data; tie to L12 "Responsible AI at the edge"
   and HW 4 Q12.) Describe one mitigation you would add for a
   shippable product.

**Deliverable for C.1:** the completed sketch
(`g3-ble-telemetry.ino`), a **screenshot of the Web Bluetooth receiver
page** (or the Bluefruit LE Connect log on iOS) showing notifications
arriving as you perform each gesture, and the two analysis paragraphs.

### Option C.2 — Throughput + energy budget at three sample rates

Characterize the **throughput-vs-accuracy** tradeoff for your gesture
detector at **three different IMU sampling rates** — for example
**50 Hz**, **100 Hz** (the A.1 baseline), and **200 Hz** (the BMI270
supports these standard rates; the
[`Arduino_BMI270_BMM150` library reference](https://github.com/arduino-libraries/Arduino_BMI270_BMM150)
lists the available options). This option uses **only on-board
measurements plus datasheet lookups** — no extra hardware is required.

For each sample rate, re-deploy and measure:

- **Test-set accuracy** — you'll need to **retrain** in Edge Impulse
  with samples collected at that rate (or use the Studio's *Impulse
  design → Time-series* resample option). Be explicit about which path
  you used.
- **Classifications per second** delivered at the deployed cadence
  (count `Serial.print` lines per second of stable measurement).
- **Inference latency** — mean over ≥ 100 windows from the EI
  `result.timing` field.

Then **compute (do not measure) an analytical energy estimate** from
the relevant datasheets. The point of this part is to apply the L10
energy framing — and HW 4 Q7's `Energy = I × V × t` — to a real
system using only published current numbers.

- BMI270 IMU active current at each ODR — from the
  [BMI270 datasheet, Section 1.2 "Typical performance"](https://www.bosch-sensortec.com/products/motion-sensors/imus/bmi270/)
  (accel-only is on the order of **hundreds of µA**; gyro adds about
  **~700–800 µA** on top — and is what dominates if you enable both,
  which you are).
- nRF52840 (the Nano's MCU) active current at 64 MHz — from the
  [nRF52840 product specification, Section 21.3 "Electrical
  characteristics"](https://infocenter.nordicsemi.com/topic/ps_nrf52840/chips/nrf52840.html)
  (on the order of **~7–8 mA** running flash + cache, no radio).
- **Compute, per sample rate:** energy per inference (mJ) =
  *(I_imu + I_mcu) × 3.3 V × inference_time*. Also compute energy *per
  second of continuous operation* using your measured throughput.

Tabulate (accuracy, throughput, latency, **computed mJ per
inference**) across the three sample rates. Two short paragraphs:

1. **Where is the knee of the curve?** Which sample rate maximizes
   accuracy per mJ, and is it the highest, lowest, or somewhere in the
   middle? Tie to a Lecture 10 concept (the "operating point" framing
   from HW 4 Q5 — pick the point that minimizes *expected cost*, not
   the one that "looks best").
2. **One specific deployment scenario** where the *low-rate* point on
   your curve would be the right choice even though it loses
   accuracy (hint: a coin-cell battery budget, like HW 4 Q14). Cite
   the specific BMI270 or nRF52840 number that drives your choice.

> **Why analytical?** A USB inline power meter is not in the course kit
> and reading sub-milliamp current accurately needs a bench supply most
> students don't have. The datasheet numbers are the same ones a
> product engineer would consult during architecture review *before*
> ordering hardware — making this exercise faithful to the L10 + HW 4
> Q7 framing rather than a measurement-instrument lab.

**Deliverable for C.2:** the measurement-and-computation table, the
cited datasheet pages (a screenshot or page-number reference is fine),
and the two analysis paragraphs.

---

## 7. Deliverables and submission

> **About the `starter/` skeletons.** As in Labs 1 and 2, the starters
> are *scaffolding* — file structure plus `// TODO(student)` hints,
> with the graded logic left for you to write. They compile and upload
> as-is so you can confirm the toolchain, but a starter submitted with
> its TODOs unfilled earns no credit for that part.

Submit a **single PDF** lab report following the
[lab report template](../templates/lab-report-template.docx) (also on
Canvas). The report should contain:

- Title page (your name, lab title, **the four classes you trained**,
  section, date).
- **Objectives** — restate in your own words.
- **Method and results** — one section per A.1, A.2, B.1, B.2, B.3,
  and (5862) Part C. Include the screenshots, the feature diagram, the
  EI Model page screenshots, your **two measurement tables** (B.3
  baseline + after-optimization), serial / video evidence of live
  detection, and the analysis paragraphs.
- **AI tool disclosure** — per the syllabus AI Use Policy: declare
  whether you used any AI tool, and if so list the tool, the prompts,
  the output, and your modifications.
- **References** — at minimum the *TinyML Cookbook* Ch 9, the
  `Arduino_BMI270_BMM150` library reference, and the Edge Impulse docs
  you used.

Also include in your Canvas submission, alongside the PDF:

- A `.zip` of your Arduino sketches (`g1-imu-capture/`,
  `g2-gesture-inference/`, and — 5862 — `g3-ble-telemetry/` or
  `partC2-*/`).
- A **link to your public Edge Impulse project** (or, if private, the
  exported library `.zip`).
- A `README.md` inside the zip listing each artifact and what it does.

---

## 8. Grading rubric

| Part | Points 4862 | Points 5862 | Criteria |
|---|---|---|---|
| A.1 IMU capture | 3 | 3 | Sketch reads BMI270 at the chosen rate and streams CSV over Serial; screenshot + measured rate |
| A.2 Features | 3 | 3 | Correct written explanation of Spectral Analysis + diagram of raw → feature transform |
| B.1 Train in EI | 4 | 4 | Impulse trained on 4 classes; EI Model screenshots + per-class P/R/F1 |
| B.2 Deploy + continuous + test-and-trace + act | 5 | 5 | Sketch runs continuous classification with the filter; LED act stage verified on live gestures (video/stills) |
| B.3 Measure + optimization | 3 | 3 | Baseline measurement table + after-optimization table + analysis paragraph |
| C (5862 only) | (+2 bonus, capped at 20) | 4 | One option completed; analysis paragraphs included |
| Report quality + AI disclosure | 2 | 2 | Follows template; classes stated; AI policy honored |
| **Total** | **20** | **24 raw → 18 (scaled)** | |

> **5862 scaling.** As in Labs 1 and 2, Part C is required and worth
> 4 raw points, making the raw 5862 total 24. Per the syllabus, 5862
> lab grades are out of 18, so your raw score is multiplied by
> `18 / 24 = 0.75` before being entered.

**Late policy.** 4 % per day or 20 % per week, accepted up to one week
past the due date. After one week, the lab earns zero.

**Partial credit.** You can earn up to 5 points on a single lab even if
your program does not work, as long as your report describes in detail
your efforts and what you learned. See the syllabus.

> **End-of-term reminder.** Lab 3 is due the **last day of class**
> (Sun Jul 12, 11:59 PM ET). The one-week late window crosses the end
> of the term and is **at the instructor's discretion** — start the
> data-collection step early.

---

## 9. Help and discussion

- **IMU library, compile, or deployment questions** — post in the
  **General Discussion** forum on Canvas. Others have the same
  questions.
- **Edge Impulse data collection and impulse-design questions** — the
  **Week 7 Zoom session** is the right place. (Lab 3 is released at the
  start of Week 6, so Weeks 6 and 7 are both fair game for live debug.)
- **BLE pairing (5862 C.1) or power-meter setup (5862 C.2)** — drop a
  screenshot or a wiring photo in the forum; we can debug pairing
  failures faster than the Studio can.

---

## 10. References

- Iodice, G.M. *TinyML Cookbook* (2nd Edition). Packt Publishing, 2023.
  Chapter 9 — IMU gesture recognition with Edge Impulse, Spectral
  Analysis features, continuous classification, and the test-and-trace
  post-processing filter.
- TinyML Cookbook 2E companion code repository.
  <https://github.com/PacktPublishing/TinyML-Cookbook_2E>
- Arduino. *Nano 33 BLE Sense Rev2 Product Page*.
  <https://store-usa.arduino.cc/products/nano-33-ble-sense-rev2>
- Arduino. *Arduino_BMI270_BMM150* library (on-board IMU + magnetometer).
  <https://github.com/arduino-libraries/Arduino_BMI270_BMM150>
- Arduino. *ArduinoBLE* library (for 5862 C.1).
  <https://docs.arduino.cc/libraries/arduinoble/>
- Edge Impulse — *Continuous motion recognition* tutorial.
  <https://docs.edgeimpulse.com/docs/tutorials/end-to-end-tutorials/continuous-motion-recognition>
- Edge Impulse — *Spectral features* DSP block reference.
  <https://docs.edgeimpulse.com/docs/edge-impulse-studio/processing-blocks/spectral-features>
- MDN. *Web Bluetooth API* — the browser API the
  `tools/web-ble-receiver.html` page is built on.
  <https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API>
- Adafruit. *Bluefruit LE Connect* (open-source iOS fallback for C.1).
  <https://github.com/adafruit/Bluefruit_LE_Connect_v2>
- Online companion notes for this course.
  <https://acanets.github.io/learn-eAI/>

---

*Lab 3 handout · Last updated: 2026-06-15.*
