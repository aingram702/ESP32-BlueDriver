// ============================================================================
//  config.h  -  Compile-time configuration for ESP32-BlueDriver
//
//  v2.0.0: BlueDriver is now a HEADLESS BLE SENSOR. It no longer hosts its own
//  Wi-Fi access point or web UI. It joins the ESP32-WarDriver's access point as
//  a Wi-Fi station, POSTs the BLE devices it discovers to the WarDriver, and is
//  controlled remotely from the WarDriver's web UI (start/stop/clear scan,
//  active-scan toggle, beacon transmitter, GATT enumeration).
// ============================================================================
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Firmware identity
// ---------------------------------------------------------------------------
#define BD_FW_NAME      "ESP32-BlueDriver"
#define BD_FW_VERSION   "2.0.0"

// ---------------------------------------------------------------------------
//  Link to the ESP32-WarDriver  (REQUIRED — set these before flashing)
// ---------------------------------------------------------------------------
//  Because BlueDriver no longer has a configuration UI, the link parameters are
//  compile-time constants. They MUST match the WarDriver's SoftAP:
//      LINK_SSID == WarDriver  AP_SSID
//      LINK_PASS == WarDriver  AP_PASSWORD
//  LINK_HOST is the WarDriver's SoftAP gateway IP (fixed at 192.168.4.1).
#define LINK_SSID            "Wardriver"        // == WarDriver AP_SSID
#define LINK_PASS            "wardrive-me"      // == WarDriver AP_PASSWORD ("" = open)
#define LINK_HOST            "192.168.4.1"      // WarDriver SoftAP gateway IP
#define LINK_PORT            80
#define LINK_PATH            "/ingest"          // WarDriver ingest endpoint

//  How often BlueDriver POSTs to the WarDriver. Each POST uploads any new/
//  updated devices AND pulls any pending control commands, so this doubles as
//  the remote-control latency. 3–5 s is a good balance.
#define LINK_INTERVAL_MS     4000
#define LINK_RETRY_MS        10000             // Wi-Fi reconnect retry cadence
#define LINK_CONNECT_TMO_MS  3000              // per-POST TCP connect timeout
#define LINK_RW_TMO_MS       4000              // per-POST read timeout
#define UPLINK_BATCH_MAX     40                // devices per POST

// ---------------------------------------------------------------------------
//  Onboard status LED (RGB on the DevKitC-1)
// ---------------------------------------------------------------------------
#define STATUS_LED_PIN      48      // WS2812 "RGB" LED on ESP32-S3-DevKitC-1
#define STATUS_LED_IS_RGB   1

// ---------------------------------------------------------------------------
//  Optional GPS module (any NMEA-0183 serial GPS, e.g. NEO-6M / NEO-M8N)
// ---------------------------------------------------------------------------
#define GPS_ENABLED         1
#define GPS_RX_PIN          18      // ESP32 RX  <- GPS TX
#define GPS_TX_PIN          17      // ESP32 TX  -> GPS RX (usually unused)
#define GPS_BAUD            9600
#define GPS_UART_NUM        1       // use Serial1

// ---------------------------------------------------------------------------
//  BLE scanner
// ---------------------------------------------------------------------------
#define BLE_ACTIVE_SCAN     true    // request scan-response (names) — more data
// Wi-Fi (the STA link) and BLE share one 2.4 GHz radio. Keeping the scan window
// well below the interval leaves the Wi-Fi station enough airtime to stay
// associated and to push uploads reliably.
#define BLE_SCAN_INTERVAL   160     // ms between the start of each scan window
#define BLE_SCAN_WINDOW     80      // ms actively listening (50% duty)
#define BLE_SCAN_DURATION   0       // 0 = scan forever (restarted automatically)

// ---------------------------------------------------------------------------
//  Device store
// ---------------------------------------------------------------------------
#define MAX_DEVICES         40000
#define NAME_MAX_LEN        31
#define MFG_MAX_LEN         23
#define UUID_MAX_LEN        39

// How often the in-RAM device log is flushed to flash (LittleFS), in ms.
#define AUTOSAVE_INTERVAL   30000

// Files used to persist data across reboots.
#define LOG_FILE_PATH       "/devices.csv"
#define CONFIG_FILE_PATH    "/config.json"
