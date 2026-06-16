// ============================================================================
//  device_store.cpp
// ============================================================================
#include "device_store.h"
#include <LittleFS.h>
#include "gps.h"

DeviceStore g_store;

// ---------------------------------------------------------------------------
//  Small lookup of common Bluetooth SIG company identifiers.  Not exhaustive
//  (the full list has thousands of entries) but covers the manufacturers you
//  actually run into while wardriving.  Unknown IDs are shown as hex.
// ---------------------------------------------------------------------------
struct Company { uint16_t id; const char* name; };
static const Company kCompanies[] = {
    {0x004C, "Apple"},        {0x0006, "Microsoft"},     {0x00E0, "Google"},
    {0x0075, "Samsung"},      {0x0087, "Garmin"},        {0x0157, "Anhui Huami"},
    {0x038F, "Xiaomi"},       {0x0499, "Ruuvi"},         {0x004F, "APT/Qualcomm"},
    {0x0059, "Nordic Semi"},  {0x0001, "Ericsson"},      {0x000F, "Broadcom"},
    {0x0078, "Nike"},         {0x0085, "BlueRadios"},    {0x00D2, "Dialog Semi"},
    {0x0118, "Fitbit"},       {0x0131, "Cypress"},       {0x0171, "Amazon"},
    {0x0A12, "Wireless"},     {0x0C60, "Tile"},          {0x05A7, "Sonos"},
    {0x0030, "ST Micro"},     {0x000D, "Texas Instr"},   {0x0822, "Adafruit"},
    {0x07D7, "Espressif"},    {0x008A, "Jawbone"},       {0x0110, "TomTom"},
    {0x05C3, "Logitech"},     {0x015E, "Continental"},   {0x0103, "Bose"},
    {0x01D1, "Polar"},        {0x0271, "Plantronics"},   {0x0212, "Withings"},
};

const char* companyName(uint16_t id) {
  for (auto& c : kCompanies)
    if (c.id == id) return c.name;
  return nullptr;
}

uint64_t macToKey(const uint8_t mac[6]) {
  uint64_t k = 0;
  for (int i = 0; i < 6; i++) k = (k << 8) | mac[i];
  return k;
}

void macToStr(const uint8_t mac[6], char out[18]) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ---------------------------------------------------------------------------
void DeviceStore::begin() {
  mutex_ = xSemaphoreCreateMutex();
  devices_.reserve(2048);
}

void DeviceStore::lock()   { if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY); }
void DeviceStore::unlock() { if (mutex_) xSemaphoreGive(mutex_); }

size_t DeviceStore::size() {
  lock();
  size_t n = devices_.size();
  unlock();
  return n;
}

uint32_t DeviceStore::newDeviceCount() { return totalNew_; }

void DeviceStore::clear() {
  lock();
  devices_.clear();
  totalNew_ = 0;
  unlock();
}

bool DeviceStore::observe(const AdvObservation& obs, uint32_t now,
                          bool gpsValid, double lat, double lon, float alt) {
  uint64_t key = macToKey(obs.mac);
  bool isNew = false;

  lock();
  auto it = devices_.find(key);
  if (it == devices_.end()) {
    if (devices_.size() >= MAX_DEVICES) { unlock(); return false; }
    BleDevice d{};
    d.key = key;
    memcpy(d.mac, obs.mac, 6);
    d.addrType  = obs.addrType;
    d.firstSeen = now;
    d.rssiBest  = obs.rssi;
    d.count     = 0;
    it = devices_.emplace(key, d).first;
    isNew = true;
    totalNew_++;
  }

  BleDevice& d = it->second;
  d.lastSeen    = now;
  d.rssiLast    = obs.rssi;
  d.count++;
  d.connectable = obs.connectable;
  d.advFlags    = obs.advFlags;
  if (obs.appearance) d.appearance = obs.appearance;
  if (obs.hasTxPower) d.txPower = obs.txPower;
  if (obs.hasCompanyId) {
    d.companyId = obs.companyId;
    const char* cn = companyName(obs.companyId);
    if (cn) strncpy(d.manufacturer, cn, MFG_MAX_LEN);
    else    snprintf(d.manufacturer, MFG_MAX_LEN, "0x%04X", obs.companyId);
  }
  if (obs.name[0]) strncpy(d.name, obs.name, NAME_MAX_LEN);
  if (obs.serviceUuid[0]) strncpy(d.serviceUuid, obs.serviceUuid, UUID_MAX_LEN);

  // Keep the GPS coordinate from the *strongest* reception (best fix point).
  if (obs.rssi >= d.rssiBest) {
    d.rssiBest = obs.rssi;
    if (gpsValid) {
      d.lat = lat; d.lon = lon; d.alt = alt; d.hasGps = true;
    }
  }
  unlock();
  return isNew;
}

// ---------------------------------------------------------------------------
//  CSV persistence.  We use WiGLE's "WigleWifi-1.6" layout so the exported
//  file can be uploaded straight to wigle.net, with Type=BLE.
// ---------------------------------------------------------------------------
static void writeWigleHeader(File& f) {
  f.printf(
      "WigleWifi-1.6,appRelease=%s,model=ESP32-S3,release=%s,"
      "device=ESP32-BlueDriver,display=,board=ESP32-S3-DevKitC1,brand=Espressif\n",
      BD_FW_VERSION, BD_FW_VERSION);
  f.print("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,"
          "CurrentLongitude,AltitudeMeters,AccuracyMeters,Type\n");
}

bool DeviceStore::saveCsv(const char* path) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  writeWigleHeader(f);

  char mac[18];
  uint32_t bootEpoch = g_gps.bootEpoch();   // 0 if GPS time never acquired
  lock();
  for (auto& kv : devices_) {
    const BleDevice& d = kv.second;
    macToStr(d.mac, mac);
    uint32_t firstEpoch = bootEpoch ? (d.firstSeen + bootEpoch) : d.firstSeen;
    // AuthMode carries the human-meaningful descriptor for BLE rows.
    f.printf("%s,%s,%s,%lu,0,%d,%.6f,%.6f,%.1f,0,BLE\n",
             mac,
             d.name[0] ? d.name : "",
             d.manufacturer[0] ? d.manufacturer : "BLE Device",
             (unsigned long)firstEpoch,
             d.rssiLast,
             d.hasGps ? d.lat : 0.0,
             d.hasGps ? d.lon : 0.0,
             d.hasGps ? d.alt : 0.0f);
  }
  unlock();
  f.close();
  return true;
}

bool DeviceStore::loadCsv(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  // Skip the two header lines.
  f.readStringUntil('\n');
  f.readStringUntil('\n');

  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (line.length() < 12) continue;
    // MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,Lat,Lon,Alt,Acc,Type
    int p[10];
    int n = 0, idx = 0;
    for (int i = 0; i < (int)line.length() && n < 10; i++)
      if (line[i] == ',') p[n++] = i;
    if (n < 10) continue;

    auto col = [&](int c) -> String {
      int s = (c == 0) ? 0 : p[c - 1] + 1;
      int e = (c == 10) ? line.length() : p[c];
      return line.substring(s, e);
    };
    (void)idx;

    String macs = col(0);
    uint8_t mac[6];
    if (sscanf(macs.c_str(), "%hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6)
      continue;

    AdvObservation o{};
    memcpy(o.mac, mac, 6);
    o.rssi = (int8_t)col(5).toInt();
    strncpy(o.name, col(1).c_str(), NAME_MAX_LEN);
    String mfg = col(2);
    double lat = col(6).toFloat();
    double lon = col(7).toFloat();
    float  alt = col(8).toFloat();
    uint32_t fseen = (uint32_t)strtoul(col(3).c_str(), nullptr, 10);
    bool gps = (lat != 0.0 || lon != 0.0);
    observe(o, fseen ? fseen : 1, gps, lat, lon, alt);

    // Restore the manufacturer label (not derivable without raw mfg data).
    if (mfg.length() && mfg != "BLE Device") {
      lock();
      auto it = devices_.find(macToKey(mac));
      if (it != devices_.end())
        strncpy(it->second.manufacturer, mfg.c_str(), MFG_MAX_LEN);
      unlock();
    }
  }
  f.close();
  return true;
}
