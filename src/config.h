// ============================================================================
//  config.h  -  Compile-time configuration for ESP32-BlueDriver
// ============================================================================
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Firmware identity
// ---------------------------------------------------------------------------
#define BD_FW_NAME      "ESP32-BlueDriver"
#define BD_FW_VERSION   "1.0.0"

// ---------------------------------------------------------------------------
//  Wi-Fi access point (the web UI is served here)
// ---------------------------------------------------------------------------
//  The device boots as its own Wi-Fi access point.  Connect a phone/laptop to
//  it and a captive portal opens the control panel automatically.
#define AP_SSID_DEFAULT     "BlueDriver"
#define AP_PASS_DEFAULT     "wardrive"     // >= 8 chars, or "" for an open AP
#define AP_CHANNEL          6
#define WEB_PORT            80
#define DNS_PORT            53

// ---------------------------------------------------------------------------
//  Data uplink (optional) — push discovered BLE devices to another board
// ---------------------------------------------------------------------------
//  When enabled, BlueDriver also joins another Wi-Fi network (e.g. your
//  ESP32-Wardriver's access point) as a station and periodically HTTP-POSTs
//  newly discovered / updated devices as JSON.  It keeps its own AP + web UI
//  running at the same time (AP+STA mode).  All fields are editable at runtime
//  from the web UI's Settings tab.
#define UPLINK_ENABLED_DEFAULT   false
#define UPLINK_SSID_DEFAULT      "Wardriver"   // the receiving board's AP SSID
#define UPLINK_PASS_DEFAULT      ""            // its AP password ("" = open)
#define UPLINK_HOST_DEFAULT      "192.168.4.1" // receiving board's IP on its AP
#define UPLINK_PORT_DEFAULT      80
#define UPLINK_PATH_DEFAULT      "/ingest"     // HTTP endpoint that accepts JSON
#define UPLINK_INTERVAL_DEFAULT  15            // seconds between POST batches
#define UPLINK_BATCH_MAX         40            // devices per POST

// ---------------------------------------------------------------------------
//  Onboard status LED (RGB on the DevKitC-1)
// ---------------------------------------------------------------------------
#define STATUS_LED_PIN      48      // WS2812 "RGB" LED on ESP32-S3-DevKitC-1
#define STATUS_LED_IS_RGB   1

// ---------------------------------------------------------------------------
//  Optional GPS module (any NMEA-0183 serial GPS, e.g. NEO-6M / NEO-M8N)
// ---------------------------------------------------------------------------
//  Wire GPS-TX -> GPS_RX_PIN.  Leave it unconnected to run without GPS; the
//  firmware auto-detects a fix and only then tags devices with coordinates.
#define GPS_ENABLED         1
#define GPS_RX_PIN          18      // ESP32 RX  <- GPS TX
#define GPS_TX_PIN          17      // ESP32 TX  -> GPS RX (usually unused)
#define GPS_BAUD            9600
#define GPS_UART_NUM        1       // use Serial1

// ---------------------------------------------------------------------------
//  BLE scanner
// ---------------------------------------------------------------------------
#define BLE_ACTIVE_SCAN     true    // request scan-response (names) — more data
// IMPORTANT: Wi-Fi (the web UI) and BLE share one 2.4 GHz radio. The scan window
// MUST stay well below the interval, otherwise continuous scanning starves the
// Wi-Fi AP of airtime and the "BlueDriver" SSID never beacons / never appears.
#define BLE_SCAN_INTERVAL   160     // ms between the start of each scan window
#define BLE_SCAN_WINDOW     80      // ms actively listening (50% duty -> WiFi ok)
#define BLE_SCAN_DURATION   0       // 0 = scan forever (restarted automatically)

// ---------------------------------------------------------------------------
//  Device store
// ---------------------------------------------------------------------------
//  With 8 MB PSRAM we can comfortably hold tens of thousands of unique
//  devices.  This cap is a safety valve only.
#define MAX_DEVICES         40000
#define NAME_MAX_LEN        31
#define MFG_MAX_LEN         23
#define UUID_MAX_LEN        39

// How often the in-RAM device log is flushed to flash (LittleFS), in ms.
#define AUTOSAVE_INTERVAL   30000

// File used to persist the log across reboots.
#define LOG_FILE_PATH       "/devices.csv"
#define CONFIG_FILE_PATH    "/config.json"
