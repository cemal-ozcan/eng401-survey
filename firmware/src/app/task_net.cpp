// Wi-Fi telemetry (requirement R7): sends every record as a UDP JSON datagram,
// a heartbeat every second, and answers PC pings for latency measurement.
//
// PC discovery: data goes to TELEMETRY_DEFAULT_HOST (broadcast) until the first
// ping arrives; afterwards it is sent unicast to the address that pinged.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "build_info.h"
#include "config.h"
#include "shared_state.h"
#include "tasks.h"
#include "telemetry.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

namespace app {

namespace {

constexpr uint32_t kQueueWaitMs = 5;          // keeps ping handling responsive
constexpr uint64_t kHeartbeatUs = 1000000;
constexpr uint64_t kReconnectUs = 5000000;

WiFiUDP udp;
IPAddress target;
bool udpStarted = false;

void sendText(const char* text, size_t len, const IPAddress& to) {
  udp.beginPacket(to, telemetry::kPcPort);
  udp.write(reinterpret_cast<const uint8_t*>(text), len);
  udp.endPacket();
}

void handlePings(hal::Clock& clock) {
  int size;
  while ((size = udp.parsePacket()) > 0) {
    const uint64_t t2 = clock.nowUs();  // as close to reception as we can observe
    char in[160];
    int n = udp.read(in, sizeof(in) - 1);
    if (n <= 0) continue;
    in[n] = '\0';
    telemetry::Ping ping;
    if (!telemetry::parsePing(in, ping)) continue;
    if (target != udp.remoteIP()) {
      target = udp.remoteIP();
      serialPrintf("# NET PC found at %s, switching to unicast", target.toString().c_str());
    }
    char out[160];
    size_t len = telemetry::encodePong(ping, t2, clock.nowUs(), out, sizeof(out));
    if (len > 0) sendText(out, len, udp.remoteIP());
  }
}

}  // namespace

std::atomic<bool> wifiConnected{false};
std::atomic<uint32_t> netDrops{0};

void netTask(void*) {
  hal::Devices& d = hal::devices();
  target.fromString(TELEMETRY_DEFAULT_HOST);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // modem sleep adds 100+ ms latency spikes
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  serialPrintf("# NET connecting to \"%s\"", WIFI_SSID);

  uint64_t lastHb = 0, lastReconnect = d.clock.nowUs();
  record::Record r;
  char json[record::kMaxJsonLen];

  for (;;) {
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected != wifiConnected.load()) {
      wifiConnected = connected;
      if (connected) {
        udpStarted = udp.begin(telemetry::kDevicePort);
        serialPrintf("# NET connected, ip %s rssi %d dBm", WiFi.localIP().toString().c_str(),
                     static_cast<int>(WiFi.RSSI()));
      } else {
        serialPrintf("# NET disconnected");
      }
    }
    const uint64_t now = d.clock.nowUs();
    if (!connected && now - lastReconnect > kReconnectUs) {
      lastReconnect = now;
      WiFi.reconnect();
    }

    if (connected && udpStarted) handlePings(d.clock);

    if (xQueueReceive(netQueue, &r, pdMS_TO_TICKS(kQueueWaitMs)) == pdTRUE) {
      size_t len = record::toJson(r, json, sizeof(json));
      if (connected && udpStarted && len > 0) sendText(json, len, target);
      else ++netDrops;  // the SD card still has the record
    }

    if (connected && udpStarted && now - lastHb >= kHeartbeatUs) {
      lastHb = now;
      shared.lock();
      const uint32_t sdErrors = loggerStats.writeErrors;
      shared.unlock();
      telemetry::Heartbeat hb{now,
                              static_cast<uint32_t>(now / 1000000ULL),
                              ESP.getFreeHeap(),
                              WiFi.RSSI(),
                              static_cast<uint32_t>(d.storage.freeBytes() / (1024ULL * 1024ULL)),
                              queueDrops.load(),
                              sdErrors,
                              netDrops.load(),
                              build_info::version()};
      size_t len = telemetry::encodeHeartbeat(hb, json, sizeof(json));
      if (len > 0) sendText(json, len, target);
    }
  }
}

}  // namespace app
