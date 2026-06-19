// ============================================================================
//  device_store.h  -  De-duplicated store of discovered BLE devices
// ============================================================================
//  Devices are keyed by their 48-bit MAC address, so a device seen a thousand
//  times occupies exactly one entry (its hit-count, RSSI range, last-seen time
//  and best-fix GPS coordinates are updated in place).  Records live in PSRAM.
// ============================================================================
#pragma once
#include <Arduino.h>
#include <unordered_map>
#include "config.h"

// --- PSRAM allocator so the (potentially large) device map lives in PSRAM ----
template <class T>
struct PsramAllocator {
  using value_type = T;
  PsramAllocator() = default;
  template <class U>
  constexpr PsramAllocator(const PsramAllocator<U>&) noexcept {}
  T* allocate(std::size_t n) {
    void* p = ps_malloc(n * sizeof(T));
    if (!p) p = malloc(n * sizeof(T));  // graceful fallback to internal RAM
    return static_cast<T*>(p);
  }
  void deallocate(T* p, std::size_t) noexcept { free(p); }
};
template <class T, class U>
bool operator==(const PsramAllocator<T>&, const PsramAllocator<U>&) { return true; }
template <class T, class U>
bool operator!=(const PsramAllocator<T>&, const PsramAllocator<U>&) { return false; }

// --- One discovered device ---------------------------------------------------
struct BleDevice {
  uint64_t key;                       // MAC packed into the low 48 bits
  uint8_t  mac[6];
  uint8_t  addrType;                  // 0 public, 1 random, ...
  char     name[NAME_MAX_LEN + 1];
  char     manufacturer[MFG_MAX_LEN + 1];
  char     serviceUuid[UUID_MAX_LEN + 1];
  uint16_t companyId;                 // Bluetooth SIG company identifier
  uint16_t appearance;
  int8_t   rssiBest;
  int8_t   rssiLast;
  int8_t   txPower;
  uint8_t  advFlags;
  bool     connectable;
  uint32_t firstSeen;                 // unix seconds (GPS) or uptime seconds
  uint32_t lastSeen;
  uint32_t count;                     // number of advertisements heard
  double   lat;                       // location of strongest reception
  double   lon;
  float    alt;
  bool     hasGps;
};

using DeviceMap =
    std::unordered_map<uint64_t, BleDevice, std::hash<uint64_t>,
                       std::equal_to<uint64_t>,
                       PsramAllocator<std::pair<const uint64_t, BleDevice>>>;

// Data handed to the store for every advertisement heard.
struct AdvObservation {
  uint8_t  mac[6];
  uint8_t  addrType;
  int8_t   rssi;
  int8_t   txPower;
  bool     hasTxPower;
  bool     connectable;
  uint16_t companyId;
  bool     hasCompanyId;
  uint16_t appearance;
  uint8_t  advFlags;
  char     name[NAME_MAX_LEN + 1];
  char     serviceUuid[UUID_MAX_LEN + 1];
};

class DeviceStore {
 public:
  void begin();

  // Insert or update a device from an advertisement. Returns true if NEW.
  bool observe(const AdvObservation& obs, uint32_t now, bool gpsValid,
               double lat, double lon, float alt);

  size_t size();
  uint32_t newDeviceCount();            // total unique ever seen this run
  void clear();

  // Thread-safe snapshot helpers used by the web layer.
  void lock();
  void unlock();
  const DeviceMap& map() const { return devices_; }   // call between lock/unlock

  // Persistence (LittleFS)
  bool saveCsv(const char* path);
  bool loadCsv(const char* path);

 private:
  DeviceMap devices_;
  SemaphoreHandle_t mutex_ = nullptr;
  uint32_t totalNew_ = 0;
};

extern DeviceStore g_store;

// Helpers
uint64_t macToKey(const uint8_t mac[6]);
void macToStr(const uint8_t mac[6], char out[18]);
const char* companyName(uint16_t id);   // best-effort SIG company lookup
