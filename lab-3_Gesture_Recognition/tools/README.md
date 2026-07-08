# lab-3/tools — open-source receivers for the Lab 3 BLE telemetry stream

These tools are for **5862 Part C.1** (BLE gesture telemetry). All are
self-hosted / open-source — no commercial app install, no account
required.

## `web-ble-receiver.html` — primary path (Chrome / Edge)

A single-file Web Bluetooth page that scans for `Nano33Gesture`,
connects, subscribes to the `GestureEvent` characteristic, and logs
each incoming byte with a timestamp + decoded gesture label. The
source is ~150 lines you can read in one sitting.

### How to use it

Web Bluetooth requires a **secure context** — the page must be served
over `http://localhost`, not opened from a `file://` URL. The simplest
recipe:

```bash
# in this folder
cd lab-3/tools
python3 -m http.server 8080
```

Then in **Chrome** or **Edge** (desktop *or* Android — see browser
note below), open:

<http://localhost:8080/web-ble-receiver.html>

1. Click **Connect to Nano33Gesture**. The browser shows a system
   permission prompt listing nearby BLE peripherals.
2. Select your Nano (look for `Nano33Gesture`) and click **Pair**.
3. The status row turns green and says *"Connected … Subscribed."*
4. Perform a gesture. A row appears in the log per stable detection,
   showing the timestamp, the raw byte (e.g. `0x01`), and the decoded
   label (e.g. `punch`).

### Browser support

| Browser | Web Bluetooth | Works for this page? |
|---|---|---|
| Chrome (desktop) | ✅ | yes |
| Edge (desktop) | ✅ | yes |
| Chrome (Android) | ✅ | yes |
| Firefox (any platform) | ❌ | no — Firefox does not implement Web Bluetooth |
| Safari (macOS) | ❌ | no |
| Safari (iOS) | ❌ | no — see the iOS fallback below |

iOS users: see the next section.

### If your UUIDs differ from the scaffold

The page hard-codes the same service / characteristic UUIDs as
`starter/g3-ble-telemetry/g3-ble-telemetry.ino`. If you changed them in
the sketch, edit the matching `SERVICE_UUID`, `CHAR_UUID`, and
`LABELS` constants at the top of the page's `<script>` block.

## iOS / fallback path — Bluefruit LE Connect

If you only have an iPhone, install **Bluefruit LE Connect** (Adafruit,
free, open source — sources at
<https://github.com/adafruit/Bluefruit_LE_Connect_v2>):

1. Open the app and switch to the **Scanner** tab.
2. Find `Nano33Gesture`, tap **Connect**.
3. Navigate to the **Information** view, find the custom service
   (UUID starts with `19b10000-…`), tap the characteristic
   (`19b10001-…`), and tap the **Notify** button.
4. Each detected gesture appears as a one-byte value in the log.

Bluefruit's display is less polished than the Web Bluetooth page, but
the data is the same and the screenshot satisfies the C.1 deliverable.

## Why open-source matters here

Edge AI products often ship custom BLE protocols with proprietary
companion apps. Building the receiver yourself (or auditing one whose
source you can read) is the same skill, and it sidesteps the privacy
and supply-chain risks of installing a closed-source binary to read
data from a board you own. That's the same `Responsible AI at the
Edge` argument from L12 + HW 4 Q12, one layer up the stack.
