# 🛰️ ESP32-BlueDriver

A **headless Bluetooth LE wardriving sensor** for the **ESP32-S3-DevKitC-1
(N16R8)** that feeds your **[ESP32-WarDriver](https://github.com/aingram702/ESP32-Wardriver)**.

It continuously scans for Bluetooth Low Energy advertisements, keeps a
**de-duplicated** list of every unique device it hears (in 8 MB PSRAM), optionally
geotags them with GPS, and **streams them to the WarDriver over Wi-Fi**. The
WarDriver's web UI is the single control surface for both boards — it can start/
stop scanning, clear the list, toggle active scan, and drive the built-in **BLE
pentest tools** remotely. It also bundles those pentest tools for authorized use.

> **v2.0.0 — headless sensor.** Earlier versions hosted their own Wi-Fi AP + web
> UI on the BlueDriver board. As of v2.0.0 the BlueDriver has **no UI of its
> own**: it joins the WarDriver's access point as a Wi-Fi station and is
> controlled entirely from the WarDriver. This removes the Wi-Fi-AP/BLE radio
> contention and puts all your data in one place. (Need the standalone web-UI
> build? Check out a pre-v2.0.0 tag.)

> **Why "BLE" and not "Bluetooth"?** The ESP32-S3 radio supports **Bluetooth LE
> 5.0 only — no Bluetooth Classic (BR/EDR)**. That covers virtually all modern
> trackers, wearables, beacons, headphones, advertising phones, TVs and IoT gear.
> Bluetooth Classic discovery would require an original ESP32 (not S3).

---

## ✨ Features

- **Continuous BLE wardriving** — captures name, RSSI (last + best), TX power,
  manufacturer (resolved to vendor names), service UUID, appearance and
  connectability.
- **No duplicates** — devices are keyed by MAC. A device heard 5,000 times is
  **one** record; its hit-count, signal range, last-seen and best-fix GPS update
  in place. The store lives in **PSRAM**, scaling to tens of thousands of uniques
  (with a low-memory guard so it degrades gracefully instead of crashing).
- **Streams to the WarDriver** — every ~4 s it POSTs newly discovered devices to
  the WarDriver's `/ingest` endpoint; each unique device is sent **once**, and
  only marked sent on an HTTP `2xx`, so nothing is lost or duplicated if the link
  drops. The same response channel delivers remote-control commands.
- **GPS geotagging (optional)** — plug in any NMEA serial GPS (NEO-6M/M8N…) and
  each device is tagged with the coordinate of its strongest reception.
- **WiGLE-ready logging** — keeps a local `WigleWifi-1.6` CSV (Type = `BLE`)
  backup on flash, and the WarDriver-side receiver writes the same format to SD.
- **BLE pentest toolkit** (authorized use), driven remotely from the WarDriver:
  - **GATT enumeration** — connect to a target and map its services,
    characteristics, properties and readable values.
  - **Beacon transmitter** — broadcast iBeacon / Eddystone-URL / custom frames.

---

## 🚀 Install

### 1. Set the link parameters (required)

Edit [`src/config.h`](src/config.h) so they match your WarDriver's access point:

```c
#define LINK_SSID   "Wardriver"      // == WarDriver AP SSID
#define LINK_PASS   "wardrive-me"    // == WarDriver AP password ("" = open)
#define LINK_HOST   "192.168.4.1"    // WarDriver SoftAP gateway IP
#define LINK_PORT   80
#define LINK_PATH   "/ingest"
```

> ⚠️ **Change `LINK_PASS`.** The default is published here, so anyone could
> impersonate the WarDriver AP. Use a strong WPA2 password that matches your
> WarDriver's AP. See [Security](#-security).

### 2. Flash it

```bash
# PlatformIO (pip install platformio)
git clone https://github.com/aingram702/ESP32-BlueDriver.git
cd ESP32-BlueDriver
pio run -t upload          # build + flash
pio device monitor         # serial log (USB CDC, 115200)
```

Or flash the pre-merged CI binary with esptool:

```bash
esptool.py --chip esp32s3 write_flash 0x0 bluedriver-merged.bin
```

### 3. Add the receiver to the WarDriver

Implement the `POST /ingest` endpoint on the WarDriver — there's a drop-in
handler in **[docs/wardriver-ingest.md](docs/wardriver-ingest.md)**.

---

## 📡 How it connects

1. On boot, BlueDriver joins the WarDriver's Wi-Fi (`LINK_SSID`) as a station and
   starts BLE scanning immediately.
2. Every `LINK_INTERVAL_MS` (default 4 s) it makes **one HTTP POST** to
   `http://LINK_HOST:LINK_PORT/LINK_PATH` containing its status, GPS, any
   finished GATT result, and a batch of new devices.
3. The WarDriver's **response** carries back control commands (scan on/off,
   clear, active-scan, beacon, gatt) which BlueDriver executes.

Serial log on boot prints the link target and, once associated, the upload
status. The onboard RGB LED shows state at a glance:

| LED | meaning |
|-----|---------|
| green pulse | scanning **and** linked to the WarDriver |
| amber/red pulse | scanning but **not** linked (joining/retrying) |
| dim red / blue tint | idle (blue tint = linked) |

The full request/response schema and command reference are in
**[docs/wardriver-ingest.md](docs/wardriver-ingest.md)**.

---

## 🛰️ Optional GPS wiring

| GPS pin | ESP32-S3 pin |
|---------|--------------|
| TX      | GPIO **18** (RX) |
| RX      | GPIO **17** (TX, usually unused) |
| VCC     | 3V3 |
| GND     | GND |

Defaults are in [`src/config.h`](src/config.h) (`GPS_RX_PIN`, `GPS_BAUD`, …).
No GPS? It just runs without coordinates.

---

## ⚙️ Configuration & hardware

Compile-time configuration is in [`src/config.h`](src/config.h): the WarDriver
link parameters, GPS pins, BLE scan timing (window kept at 50% duty so the Wi-Fi
station stays responsive), device cap, and autosave interval. The only runtime
setting is the active-scan toggle (set remotely by the WarDriver, persisted to
flash).

**Hardware target:** `board = esp32-s3-devkitc-1`, 16 MB flash, 8 MB **octal**
PSRAM (`qio_opi`), custom [`partitions.csv`](partitions.csv) (dual OTA app slots
+ LittleFS log area).

---

## 🗂️ Project layout

```
platformio.ini        Build config (board, PSRAM, partitions, libs)
partitions.csv        16 MB partition table (2× OTA + LittleFS log)
src/
  main.cpp            Boot + main loop (scan, GPS, uplink, autosave, LED)
  config.h            Compile-time configuration (incl. WarDriver link)
  settings.{h,cpp}    Active-scan toggle persisted to flash
  ble_scanner.{h,cpp} Continuous NimBLE observer
  device_store.{h,cpp}De-dup PSRAM device store + uplink batching + CSV
  gps.{h,cpp}         Optional NMEA GPS (TinyGPS++)
  uplink.{h,cpp}      Wi-Fi station link: POST devices + run commands
  pentest.{h,cpp}     GATT enumeration + beacon transmitter
docs/wardriver-ingest.md  Link protocol + drop-in WarDriver receiver
.github/workflows/    CI: build + merge binary
```

---

## 🔒 Security

- **Use WPA2 on the WarDriver AP and change `LINK_PASS`.** BlueDriver executes
  control commands returned by whatever answers at `LINK_HOST`. A strong, unique
  AP password is what prevents an attacker from impersonating the WarDriver and
  issuing commands (clear, beacon, GATT). The link is plain HTTP — fine on a
  private AP, but the AP password is the trust boundary.
- **Advertised names are attacker-controllable.** BlueDriver sanitizes them for
  CSV (strips `, " CR LF`) and JSON-escapes them for the uplink; the WarDriver
  should likewise escape them when rendering its UI.
- **Memory-exhaustion resistant.** The device store is capped and refuses new
  entries when PSRAM runs low, so a flood of random-MAC advertisements degrades
  gracefully instead of crashing.
- **Pentest tools are dual-use.** GATT enumeration and the beacon/spoof
  transmitter must only target devices you own or are authorized to test. There
  is deliberately **no** indiscriminate "BLE spam / pairing-popup flood" DoS tool.

---

## ⚖️ Legal & ethical use

This project is for **authorized security testing, research, and education**.
Passive BLE scanning only receives publicly broadcast advertisements and is legal
in most jurisdictions, but **laws vary — know yours**. The pentest tools must only
be used against devices you own or have explicit written permission to test (or in
CTF/lab environments). You are responsible for complying with all applicable laws.

## 📄 License

[MIT](LICENSE)
