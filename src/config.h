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
#define BLE_SCAN_INTERVAL   97      // ms
#define BLE_SCAN_WINDOW     97      // ms (== interval => continuous listening)
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
