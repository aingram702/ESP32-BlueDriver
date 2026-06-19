// ============================================================================
//  ble_scanner.cpp
// ============================================================================
#include "ble_scanner.h"
#include <NimBLEDevice.h>
#include "config.h"
#include "device_store.h"
#include "gps.h"
#include "settings.h"

BleScanner g_scanner;

static NimBLEScan* s_scan = nullptr;

// Parse "aa:bb:cc:dd:ee:ff" -> mac[6]
static bool parseMac(const std::string& s, uint8_t mac[6]) {
  return sscanf(s.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6;
}

// ---------------------------------------------------------------------------
//  Advertisement callback (runs on the NimBLE host task)
// ---------------------------------------------------------------------------
class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override {
    AdvObservation o{};

    if (!parseMac(dev->getAddress().toString(), o.mac)) return;
    o.addrType = dev->getAddress().getType();
    o.rssi     = (int8_t)dev->getRSSI();

    // Adv PDU types: 0=ADV_IND, 1=ADV_DIRECT_IND(HD), 4=ADV_DIRECT_IND(LD)
    // are connectable; 2=SCAN_IND and 3=NONCONN_IND are not.
    uint8_t at = dev->getAdvType();
    o.connectable = (at == 0 || at == 1 || at == 4);

    if (dev->haveName()) {
      std::string n = dev->getName();
      strncpy(o.name, n.c_str(), NAME_MAX_LEN);
    }
    if (dev->haveServiceUUID()) {
      std::string u = dev->getServiceUUID().toString();
      strncpy(o.serviceUuid, u.c_str(), UUID_MAX_LEN);
    }
    if (dev->haveTXPower()) {
      o.hasTxPower = true;
      o.txPower    = dev->getTXPower();
    }
    if (dev->haveAppearance()) {
      o.appearance = dev->getAppearance();
    }
    if (dev->haveManufacturerData()) {
      std::string md = dev->getManufacturerData();
      if (md.size() >= 2) {
        o.hasCompanyId = true;
        o.companyId = (uint8_t)md[0] | ((uint8_t)md[1] << 8);  // little-endian
      }
    }

    const GpsState gs = g_gps.state();
    uint32_t nowSec = (uint32_t)(millis() / 1000);   // monotonic uptime seconds
    g_store.observe(o, nowSec, gs.valid, gs.lat, gs.lon, gs.alt);
  }
};

static ScanCallbacks s_cb;

// ---------------------------------------------------------------------------
void BleScanner::begin() {
  NimBLEDevice::init(BD_FW_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);   // max RX sensitivity / TX power

  s_scan = NimBLEDevice::getScan();
  // wantDuplicates=true: hear every advertisement so RSSI/counts keep updating.
  s_scan->setAdvertisedDeviceCallbacks(&s_cb, true);
  s_scan->setActiveScan(g_settings.activeScan);
  s_scan->setInterval(BLE_SCAN_INTERVAL);
  s_scan->setWindow(BLE_SCAN_WINDOW);
  s_scan->setMaxResults(0);                 // don't buffer; callback-only
}

void BleScanner::start() {
  if (!s_scan) return;
  s_scan->start(BLE_SCAN_DURATION, nullptr, false);
  scanning_ = true;
}

void BleScanner::stop() {
  if (!s_scan) return;
  s_scan->stop();
  scanning_ = false;
}

void BleScanner::tick() {
  // NimBLE stops scanning while a central connection (GATT enumeration) is
  // active and after each duration window; restart it if we should be running.
  if (scanning_ && s_scan && !s_scan->isScanning()) {
    s_scan->start(BLE_SCAN_DURATION, nullptr, false);
  }
}

void BleScanner::setActiveScan(bool on) {
  if (!s_scan) return;
  s_scan->setActiveScan(on);
  // Restart the scan so the new mode takes effect immediately.
  if (scanning_) {
    s_scan->stop();
    delay(10);
    s_scan->start(BLE_SCAN_DURATION, nullptr, false);
  }
}
