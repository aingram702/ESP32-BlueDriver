# 🛰️ ESP32-BlueDriver

A **Bluetooth LE wardriver** for the **ESP32-S3-DevKitC-1 (N16R8)**.

It continuously scans for Bluetooth Low Energy advertisements, keeps a clean,
**de-duplicated** list of every unique device it has ever heard, optionally
geotags them with GPS, and gives you a **captive-portal web interface** that
controls everything from your phone. It also bundles a small **BLE pentest
toolkit** for authorized testing.

> **Why "BLE" and not "Bluetooth"?**
> The ESP32-S3's radio supports **Bluetooth LE 5.0 only — it has *no* Bluetooth
> Classic (BR/EDR)**. So this firmware wardrives BLE, which is what virtually
> all modern trackers, wearables, beacons, headphones, phones (while
> advertising), TVs, and IoT gear use. If you need Bluetooth Classic discovery
> you'd need an original ESP32 (not S3); the wardriving UX here is built for BLE.

---

## ✨ Features

- **Continuous BLE wardriving** — active scan captures names, RSSI, TX power,
  manufacturer (resolved to vendor names), service UUIDs, appearance and
  connectability.
- **No duplicates** — devices are keyed by MAC address. A device seen 5,000
  times is **one** row; its hit-count, signal range, last-seen time and best-fix
  GPS coordinate update in place. The big device table lives in **PSRAM** (8 MB),
  so it scales to tens of thousands of unique devices.
- **Super-easy install** — flash straight from your browser with one click (no
  toolchain). See below.
- **Intuitive web UI** — a self-hosted, offline single-page control panel:
  live sortable/searchable device list, start/stop, clear, device detail view,
  and one-click exports. Auto-refreshes.
- **GPS geotagging (optional)** — plug in any NMEA serial GPS (NEO-6M/M8N…) and
  every device is tagged with the coordinate of its strongest reception.
- **WiGLE-ready export** — download a `WigleWifi-1.6` CSV (Type = `BLE`) you can
  upload directly to [wigle.net](https://wigle.net), plus a raw JSON export.
- **BLE pentest toolkit** (authorized use):
  - **GATT enumeration** — connect to a target and map its services,
    characteristics, properties, and readable values.
  - **Beacon transmitter** — broadcast iBeacon / Eddystone-URL / custom frames
    to test how nearby apps and scanners react.
  - **Name spoof advertiser** — re-advertise a device name to validate
    proximity/asset systems you control.

---

## 🚀 Installation

### Option A — One-click web installer (easiest)

1. Open the installer page in **Chrome** or **Edge** on a desktop:
   **`https://aingram702.github.io/ESP32-BlueDriver/`**
   *(published automatically by CI once this repo is pushed to `main`/tagged).*
2. Plug the ESP32-S3 into USB.
3. Click **Install**, choose the serial port, confirm. Done.

### Option B — PlatformIO

```bash
# Requires PlatformIO (pip install platformio)
git clone https://github.com/aingram702/ESP32-BlueDriver.git
cd ESP32-BlueDriver
pio run -t upload          # build + flash
pio device monitor         # optional serial log
```

### Option C — Pre-merged binary with esptool

Download `bluedriver-merged.bin` from the latest CI run / release, then:

```bash
esptool.py --chip esp32s3 write_flash 0x0 bluedriver-merged.bin
```

---

## 📱 First run

1. After flashing, the board starts a Wi-Fi access point:
   - **SSID:** `BlueDriver`
   - **Password:** `wardrive`
2. Connect your phone or laptop. A **captive portal** opens the control panel
   automatically (or browse to **`http://192.168.4.1/`**).
3. Scanning starts on boot — devices begin populating immediately.

The onboard RGB LED pulses **green** while scanning, **red** when idle.

---

## 🖥️ Web interface

| Tab | What it does |
|-----|--------------|
| **Devices** | Live, de-duplicated table. Search by name/MAC/vendor, sort by signal/hits/recency/name, tap a row for full detail (and a GATT-enumerate button). Start/Stop scan, Clear, Export CSV/JSON. |
| **Pentest Tools** | GATT enumeration of a target; Beacon transmitter (iBeacon / Eddystone / custom). |
| **Settings** | Change AP SSID/password, toggle active scan, view device info. |

Everything is served from flash and works fully **offline** on the AP — no
internet or CDN needed.

---

## 🛰️ Optional GPS wiring

| GPS pin | ESP32-S3 pin |
|---------|--------------|
| TX      | GPIO **18** (RX) |
| RX      | GPIO **17** (TX, usually unused) |
| VCC     | 3V3 |
| GND     | GND |

Defaults are in [`src/config.h`](src/config.h) (`GPS_RX_PIN`, `GPS_BAUD`, …).
No GPS? It just runs without coordinates — the UI shows `GPS: none`.

---

## ⚙️ Configuration

Compile-time defaults live in [`src/config.h`](src/config.h): AP credentials,
GPS pins, scan timing, device cap, autosave interval. SSID/password and the
active-scan toggle are also editable at runtime from the **Settings** tab
(persisted to flash).

**Hardware target:** `board = esp32-s3-devkitc-1`, 16 MB flash, 8 MB **octal**
PSRAM (`qio_opi`), custom [`partitions.csv`](partitions.csv) with dual OTA app
slots and a large LittleFS log area.

---

## 🔌 REST API

The UI is a thin client over a small HTTP API you can script against:

| Method | Endpoint | Purpose |
|--------|----------|---------|
| GET  | `/api/status` | live counters, GPS, beacon state |
| GET  | `/api/devices?limit=&q=&sort=` | de-duplicated device list (JSON) |
| POST | `/api/scan?toggle=1` (or `on=1/0`) | start/stop scanning |
| POST | `/api/clear` | wipe the device list |
| GET  | `/export.csv` | WiGLE CSV download |
| GET  | `/export.json` | full JSON download |
| GET/POST | `/api/config` | read/update settings |
| POST | `/api/pentest/gatt?mac=&type=` | enumerate a target's GATT |
| POST | `/api/pentest/beacon?action=start&type=&...` | beacon transmitter |

---

## 🗂️ Project layout

```
platformio.ini        Build config (board, PSRAM, partitions, libs)
partitions.csv        16 MB partition table (2× OTA + LittleFS log)
src/
  main.cpp            Boot + main loop (scan, web, GPS, autosave, LED)
  config.h            Compile-time configuration
  settings.{h,cpp}    Runtime settings persisted to flash
  ble_scanner.{h,cpp} Continuous NimBLE observer
  device_store.{h,cpp}De-duplicated PSRAM device store + CSV export
  gps.{h,cpp}         Optional NMEA GPS (TinyGPS++)
  web.{h,cpp}         Captive-portal WebServer + REST API
  web_index.h         Embedded single-page UI (HTML/CSS/JS)
  pentest.{h,cpp}     GATT enumeration + beacon transmitter
docs/                 One-click web installer (GitHub Pages)
.github/workflows/    CI: build, merge bin, publish installer
```

---

## ⚖️ Legal & ethical use

This project is for **authorized security testing, research, and education**.

- **Passive BLE scanning/wardriving is legal** in most jurisdictions (it only
  receives publicly broadcast advertisements) but **laws vary** — know yours.
- The **pentest tools** (GATT enumeration, beacon/spoof transmitters) must only
  be used against devices **you own or have explicit written permission** to
  test, or in CTF/lab environments.
- There is deliberately **no indiscriminate "BLE spam / pairing-popup flood"
  DoS tool** here, as that only disrupts uninvolved bystanders.

You are responsible for complying with all applicable laws and regulations.

## 📄 License

[MIT](LICENSE)
