/*
  esp32-s3-8ch-relayboard-RGB.ino — Waveshare ESP32-S3-PoE-ETH-8DI-8RO
  Relays via TCA9554 + single WS2812 status LED using FastLED-backed RgbLed class.
  Adds WiFi + mDNS + ArduinoOTA + a Web UI / JSON API with radio buttons for digital inputs and relay buttons.
  Digital inputs are independent from relays.

  Toggle the relay self-test by defining DEBUG_RELAYS.

  Maintenance notes (2025-10-05 14:36:41):
  - Digital Inputs are ACTIVE-LOW at the hardware pins. To avoid confusion, the Web UI shows a GREEN dot when the line is ACTIVE (pin reads LOW).
  - JSON /api/state keeps `di[]` and `di_mask` as RAW-HIGH (1 means the pin reads HIGH). The UI inverts that for display.
  - Helper functions for DI/relay masks live in StateHelpers.h (inline, header-only) to prep for RS485/MQTT integration.
  - `g_mask` is the authoritative relay state (1 bit per relay, 1=ON). Use getRelayMask()/getRelay() to read it.
*/

// #define DEBUG_RELAYS   // uncomment to auto-walk relays in loop()
// #define DEBUG_RGB      // extra serial logs for LED/mask

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include "StateHelpers.h"

#include <Ethernet_Generic.h>
#include "esp_mac.h"
#include <Wire.h>
#include <TCA9554.h>
#include <FastLED.h>
#include "BoardPins.h"
#include "RgbLed_WS2812.h"
#include <WiFiUdp.h>

// ---------------- WiFi / Host Config ----------------
#ifndef WIFI_SSID
  #define WIFI_SSID   "YourSSID"
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS   "YourPassword"
#endif
#ifndef HOSTNAME
  #define HOSTNAME    "relayboard"
#endif

// ---------------- TCA9554 Address -------------------
#ifndef TCA9554_ADDR
  #define TCA9554_ADDR 0x20
#endif

// ---------------- Globals ---------------------------

volatile bool g_ota_ready = false;
static uint16_t g_ota_port = 3232;  // can change if needed

WebServer server(80);
EthernetServer ethServer(80);

RgbLed rgb;
TCA9554 tca(TCA9554_ADDR);


// Define the param bag BEFORE Arduino generates prototypes:
struct QueryParams { String idx; String on; String value; };

// Forward decls so the preprocessor won't invent mismatched ones:
String buildStateJson();
void parseQuery_(const String& path, String& route, QueryParams& q);
uint8_t g_mask = 0;         // current relay ON/OFF bitmask (authoritative: bit=1 means relay is ON)
static uint32_t g_nextWalkMs = 0;  // for DEBUG_RELAYS

// --------------- Forward Declarations ---------------
static void applyMaskToRelays(uint8_t mask);
static void setRelay(uint8_t idx, bool on);

// -------------------- Web UI ------------------------
const char HTML_INDEX[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32-S3 Relay Board</title>
  <style>
    :root {
      --card-w: 980px;
      --gap: 12px;
      --radius: 14px;
      --shadow: 0 4px 14px rgba(0,0,0,.08);
      --bg: #f6f7fb;
      --card: #fff;
      --text: #111;
      --muted: #6b7280;
      --border: #e5e7eb;
      --brand: #2563eb;
      --brand-weak: #dbeafe;
      --ok: #10b981;
      --ok-weak: #d1fae5;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0; padding: 24px;
      font-family: system-ui, -apple-system, Segoe UI, Roboto, "Helvetica Neue", Arial, "Noto Sans", "Liberation Sans", sans-serif;
      color: var(--text);
      background: var(--bg);
      display: flex; justify-content: center;
    }
    .container { width: 100%; max-width: var(--card-w); }
    .card {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      box-shadow: var(--shadow);
      padding: 16px 16px 18px;
      margin-bottom: 16px;
    }
    h1 {
      font-size: 1.4rem; margin: 0 0 6px 0;
    }
    .sub {
      font-size: .95rem; color: var(--muted);
      display:flex; gap: 10px; flex-wrap: wrap;
    }
    .label { font-weight:600; color: #374151; }
    .mask { font-variant-numeric: tabular-nums; }
    .section-title {
      font-weight: 700; font-size: 1.02rem;
      margin: 2px 0 12px 0; color:#374151;
      border-bottom: 1px solid var(--border); padding-bottom: 6px;
    }
    .inputs-grid {
      display:grid; grid-template-columns: repeat(8,1fr);
      gap: var(--gap);
    }
    @media (max-width: 680px) {
      .inputs-grid { grid-template-columns: repeat(4,1fr); }
    }
    .di-pill {
      border: 1px solid var(--border);
      border-radius: 999px;
      padding: 8px 10px;
      display:flex; align-items:center; gap:10px;
      background:#fff;
    }
    .dot {
      width: 14px; height:14px; border-radius:50%;
      background:#cbd5e1; border:1px solid #94a3b8;
      box-shadow: inset 0 1px 2px rgba(0,0,0,.08);
    }
    .dot.high { background: var(--ok); border-color:#059669; box-shadow: 0 0 0 3px var(--ok-weak); }
    .relay-grid {
      display:grid; gap: var(--gap);
      grid-template-columns: repeat(4, minmax(140px, 1fr));
    }
    @media (max-width: 680px) {
      .relay-grid { grid-template-columns: repeat(2, 1fr); }
    }
    button.relay {
      appearance:none; outline:none;
      border:1px solid var(--border);
      background: #fff;
      border-radius: 12px;
      padding: 12px 10px;
      font-weight: 600; color:#374151;
      cursor:pointer;
      transition: transform .05s ease, border-color .15s ease, box-shadow .15s ease;
    }
    button.relay:hover { border-color:#c7cad0; }
    button.relay:active { transform: translateY(1px); }
    button.relay.active {
      border-color:#1f2937;
      box-shadow: inset 0 0 0 2px var(--brand-weak);
      background: #f8fafc;
      color:#111827;
    }
    .toolbar { display:flex; gap:10px; }
    .btn {
      appearance:none; border:1px solid var(--border);
      background:#fff; color:#111; border-radius:10px;
      padding:10px 12px; cursor:pointer;
      transition: background .2s ease, border-color .2s ease;
    }
    .btn:hover { background:#f3f4f6; border-color:#d1d5db; }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <h1>ESP32-S3 8-Relay + RGB</h1>
      <div class="sub">
        <div><span class="label">IP:</span> <span id="ip">0.0.0.0</span></div>
        <div><span class="label">SSID:</span> <span id="ssid"></span></div>
        <div><span class="label">Mask:</span> <span class="mask" id="mask">0x00</span></div>
      </div>
    </div>
    <div class="card">
      <div class="section-title">Digital Inputs</div>
      <div class="inputs-grid" id="inputs"></div>
    </div>
    <div class="card">
      <div class="section-title">Relays</div>
      <div id="buttons" class="relay-grid"></div>
    </div>
    <div class="card toolbar">
      <button id="rebootBtn" class="btn">Reboot</button>
    </div>
  </div>
  <script>
    // Page state (filled by /api/state)
    var S = { mask:0, di_mask:0, relays:[], di:[], count:8, wifi:{ip:'0.0.0.0', ssid:''} };
    function drawRelays(){
      var wrap = document.getElementById('buttons');
      var html = '';
      for (var i=0; i<S.count; i++){
        var on = ((S.mask >> i) & 1) === 1;
        html += '<button class="relay '+(on?'active':'')+'" id="relayBtn-'+i+'" data-idx="'+i+'" data-state="'+(on?1:0)+'">R'+(i+1)+': '+(on?'ON':'OFF')+'</button>';
      }
      wrap.innerHTML = html;
      for (let i=0; i<S.count; i++){
        let b = document.getElementById('relayBtn-'+i);
        b.addEventListener('click', function(){
          var cur = (b.getAttribute('data-state') === '1');
          var next = cur ? 0 : 1;
          var body = new URLSearchParams({ index: String(i), state: String(next) });
          fetch('/api/relay', { method:'POST', headers: {'Content-Type':'application/x-www-form-urlencoded'}, body })
            .then(function(r){
              if (!r.ok) throw new Error('relay post failed');
              b.setAttribute('data-state', String(next));
              b.classList.toggle('active', !!next);
              b.textContent = 'R'+(i+1)+': '+(next? 'ON':'OFF');
            })
            .catch(console.error);
        });
      }
    }
    function drawInputs(){
      var wrap = document.getElementById('inputs');
      var html = '';
      for (var i=0; i<S.di.length; i++){
        var active = (S.di[i] === 0);  // active-low => LOW means ACTIVE
        html += '<div class="di-pill">'
              + '<div class="dot ' + (active ? 'high' : '') + '" id="di-dot-' + i + '"></div>'
              + '<div>DI' + (i+1) + '</div>'
              + '</div>';
      }
      wrap.innerHTML = html;
    }
    function paintInputs(){
      for (var i=0; i<S.di.length; i++){
        var dot = document.getElementById('di-dot-'+i);
        if (dot) dot.classList.toggle('high', (S.di[i] === 0));  // active-low invert
      }
    }
    function paintNet(){
      var ip = document.getElementById('ip');
      var ssid = document.getElementById('ssid');
      var m = document.getElementById('mask');
      if (ip) ip.textContent = S.wifi.ip;
      if (ssid) ssid.textContent = S.wifi.ssid || '';
      if (m) m.textContent = '0x' + S.mask.toString(16).toUpperCase().padStart(2,'0');
    }
    function refresh(){
      fetch('/api/state').then(r => r.json()).then(j => {
        S = j;
        paintNet();
        if (!document.getElementById('relayBtn-0')) drawRelays();
        if (!document.getElementById('di-dot-0')) drawInputs();
        paintInputs();
      }).catch(console.error);
    }
    document.getElementById('rebootBtn').addEventListener('click', function(){
      fetch('/reboot').then(function(){});
    });
    setInterval(refresh, 500);
    window.addEventListener('load', refresh);
  </script>
</body>
</html>
)rawliteral";

// ----------------- Helpers (I2C / TCA) --------------
static void tcaInit() {
  WiFi.onEvent([](arduino_event_t *event) {
    if (event->event_id == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      int r = event->event_info.wifi_sta_disconnected.reason;
      Serial.printf("[WiFi] Disconnect reason: %d\n", r);
    }
  });

  Wire.begin(BoardPins::I2C_SDA, BoardPins::I2C_SCL);

  if (!tca.isConnected()) {
    Serial.println(F("[ERR] TCA9554 not connected"));
    while (true) delay(1000);
  }

  // Initialize TCA9554 for relays: set output register to 0x00 (off) while pins are inputs,
  // then configure all pins as outputs.
  tca.write8(0x00);  // Set output register to all-off (no effect yet, as pins are inputs).
  tca.pinMode8(0x00);  // Configure all pins as outputs (now drives low, no glitch).

  g_mask = 0;
  rgb.setForMask(g_mask);
}

static void applyMaskToRelays(uint8_t mask) {
  mask &= (uint8_t)((1u << BoardPins::RELAY_COUNT) - 1u);

  for (uint8_t i = 0; i < BoardPins::RELAY_COUNT; ++i) {
    const bool on = (mask >> i) & 0x1;
    tca.write1(i, on ? HIGH : LOW);    // Use write1() as in original code
  }

  g_mask = mask;
  rgb.setForMask(g_mask);
}

static void setRelay(uint8_t idx, bool on) {
  if (idx >= BoardPins::RELAY_COUNT) return;

  tca.write1(idx, on ? HIGH : LOW);    // Use write1() as in original code
  if (on) g_mask |= (1u << idx);
  else g_mask &= ~(1u << idx);

  rgb.setForMask(g_mask);
}

// ----------------- Web Handlers ---------------------
static String ipStr(const IPAddress& ip) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return String(buf);
}

static void handleIndex() {
  server.send(200, "text/html", HTML_INDEX);
}

static void handleState() {
  server.send(200, "application/json", buildStateJson());
}
static void handleRelay() {
  if (!server.hasArg("index") || !server.hasArg("state")) {
    server.send(400, "text/plain", "Missing index or state"); return;
  }
  uint8_t idx = (uint8_t) server.arg("index").toInt();
  String s = server.arg("state");
  bool on = (s == "on" || s == "1" || s == "true");
  if (idx >= BoardPins::RELAY_COUNT) {
    server.send(400, "text/plain", "index out of range"); return;
  }
  setRelay(idx, on);
  handleState();
}

static void handleMask() {
  if (!server.hasArg("value")) {
    server.send(400, "text/plain", "Missing value"); return;
  }
  uint8_t v = (uint8_t) server.arg("value").toInt();
  applyMaskToRelays(v);
  handleState();
}

// ----------------- WiFi / mDNS / OTA ----------------
static void startAPFallback() {
  String apName = String(HOSTNAME) + "-AP";
  const char *pass = "esp32s3rgb";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str(), pass);
  Serial.printf("[WiFi] AP mode: SSID=%s  PASS=%s  IP=%s\n",
                apName.c_str(), pass, ipStr(WiFi.softAPIP()).c_str());
}

static void wifiBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WiFi] Connecting to \"%s\" ...\n", WIFI_SSID);

  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected: IP=%s RSSI=%d dBm\n",
                  ipStr(WiFi.localIP()).c_str(), WiFi.RSSI());
  } else {
    Serial.println("[WiFi] STA connect failed -> starting AP fallback");
    startAPFallback();
  }
}

static void mdnsBegin() {
  if (MDNS.begin(HOSTNAME)) {
    Serial.printf("[mDNS] http://%s.local/\n", HOSTNAME);
  } else {
    Serial.println("[mDNS] start failed");
  }
}

static void otaBegin() {
  ArduinoOTA.setPort(g_ota_port);
  ArduinoOTA.setHostname(HOSTNAME);

  ArduinoOTA
    .onStart([]() {
      Serial.println("[OTA] onStart()");
    })
    .onEnd([]() {
      Serial.println("[OTA] onEnd()");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      const uint32_t pct = total ? (progress * 100U) / total : 0U;
      Serial.printf("[OTA] %u%%\n", (unsigned)pct);
    })
    .onError([](ota_error_t error) {
      Serial.printf("[OTA] onError %u\n", (unsigned)error);
      if      (error == OTA_AUTH_ERROR)    Serial.println("[OTA] Auth Failed");
      else if (error == OTA_BEGIN_ERROR)   Serial.println("[OTA] Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("[OTA] Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("[OTA] Receive Failed");
      else if (error == OTA_END_ERROR)     Serial.println("[OTA] End Failed");
    });

  ArduinoOTA.begin();
  g_ota_ready = true;

  IPAddress ip = WiFi.localIP();
  Serial.printf("[OTA] Ready on %s:%u (SSID:\"%s\" RSSI:%d)\n",
                ip.toString().c_str(),
                (unsigned)g_ota_port,
                WiFi.SSID().c_str(),
                WiFi.RSSI());
}

// -------------------- Setup / Loop ------------------
// ======== Ethernet (W5500) support (minimal HTTP) ========
static const uint32_t ETH_SETUP_TIMEOUT = 5000; // ms

String urlDecode_(String v) {
  v.replace("+"," "); return v;
}


void parseQuery_(const String& path, String& route, QueryParams& q) {
  int qm = path.indexOf('?');
  route = (qm>=0) ? path.substring(0,qm) : path;
  String qs = (qm>=0) ? path.substring(qm+1) : String();
  while (qs.length()) {
    int amp = qs.indexOf('&');
    String pair = (amp>=0)? qs.substring(0,amp) : qs;
    int eq = pair.indexOf('=');
    String key = (eq>=0)? pair.substring(0,eq) : pair;
    String val = (eq>=0)? pair.substring(eq+1) : String();
    key = urlDecode_(key); val = urlDecode_(val);
    if (key == "idx") q.idx = val;
    else if (key == "on") q.on = val;
    else if (key == "value") q.value = val;
    if (amp<0) break; else qs.remove(0, amp+1);
  }
}

void ethSend_(EthernetClient &c, const String& body, const char* ctype="text/plain") {
  c.print(String("HTTP/1.1 200 OK\r\nContent-Type: ") + ctype + "\r\nConnection: close\r\n\r\n" + body);
}

bool bringupEthernet_() {
  Serial.println(F("\n[ETH] Starting W5500…"));

  // Optional reset pulse if you have one:
  #if defined(BOARDPINS_HAS_W5500_RST) && defined(W5500_RST_PIN)
    pinMode(W5500_RST_PIN, OUTPUT);
    digitalWrite(W5500_RST_PIN, LOW);  delay(10);
    digitalWrite(W5500_RST_PIN, HIGH); delay(50);
  #endif

  // SPI + WIZ init
  Serial.printf("[ETH] SPI: CS=%d SCLK=%d MISO=%d MOSI=%d\n",
                BoardPins::W5500_CS, BoardPins::SPI_SCLK, BoardPins::SPI_MISO, BoardPins::SPI_MOSI);
  SPI.begin(BoardPins::SPI_SCLK, BoardPins::SPI_MISO, BoardPins::SPI_MOSI, BoardPins::W5500_CS);
  Ethernet.init(BoardPins::W5500_CS);

  // Solid, silicon-based MAC (set locally-administered bit)
  uint8_t base[6] = {0}; 
  esp_read_mac(base, ESP_MAC_WIFI_STA);
  uint8_t mac[6]  = { uint8_t(base[0] | 0x02), base[1], base[2], base[3], base[4], base[5] };
  Serial.printf("[ETH] MAC=%02X:%02X:%02X:%02X:%02X:%02X\n", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

  // --- QUICK LINK PROBE: skip DHCP if no cable ---
  uint32_t probeStart = millis();
  bool hasLink = false;

  // Give the PHY a brief moment to settle (tune 1500–3000 ms if you want)
  while (millis() - probeStart < 1500) {
    auto ls = Ethernet.linkStatus();  // LinkON, LinkOFF, Unknown
    if (ls == LinkON) { hasLink = true; break; }
    delay(100);
  }

  if (!hasLink) {
    Serial.println("[ETH] No cable detected; skipping DHCP and going to Wi-Fi.");
    return false;  // your code will try Wi-Fi next immediately
  }

  // Optional: make W5500 a bit snappier (if supported by your Ethernet_Generic)
  #if defined(ETHERNET_GENERIC_VERSION)
    // Guarded to avoid issues on older versions
    // Ethernet.setRetransmissionTimeout(1000); // 1s
    // Ethernet.setRetransmissionCount(2);      // 2 tries
  #endif

  // --- Only run DHCP when link is present ---
  #ifdef USE_STATIC_ETH_IP
    Ethernet.begin(mac, ETH_IP, ETH_DNS, ETH_GW, ETH_MASK);
  #else
    Ethernet.begin(mac);
  #endif

  Serial.println(F("[ETH] Waiting for link + DHCP…"));
  const uint32_t DHCP_WAIT = min<uint32_t>(ETH_SETUP_TIMEOUT, 4000); // keep it short now that link is up
  uint32_t t0 = millis();

  while (millis() - t0 < DHCP_WAIT) {
    if (Ethernet.linkStatus() == LinkON) {
      IPAddress ip = Ethernet.localIP();
      if (ip != INADDR_NONE && ip[0] != 0) {
        Serial.printf("[ETH] LINK UP, IP=%u.%u.%u.%u\n", ip[0],ip[1],ip[2],ip[3]);
        ethServer.begin();
        Serial.println("[ETH] HTTP server started on 80");
        return true;
      }
    }
    delay(150);
    Serial.print('.');
  }

  Serial.println(F("\n[ETH] DHCP timed out; falling through to Wi-Fi."));
  return false;
}


void ethHandleClient_() {
  EthernetClient client = ethServer.available();
  if (!client) return;
  String line; uint32_t t0=millis();
  while (client.connected() && (millis()-t0<1000)) { if (client.available()) { line=client.readStringUntil('\\n'); break; } }
  line.trim();
  if (!line.length()) { client.stop(); return; }

  bool isGET = line.startsWith("GET ");
  bool isPOST= line.startsWith("POST ");
  int s=line.indexOf(' '), e=line.indexOf(' ', s+1);
  String path = (s>=0 && e>s)? line.substring(s+1,e) : "/";

  String route; QueryParams q; parseQuery_(path, route, q);

  if (isGET && route=="/") {
    extern const char HTML_INDEX[] PROGMEM;
    ethSend_(client, String(HTML_INDEX), "text/html");
  } else if (isGET && route=="/api/state") {
    ethSend_(client, buildStateJson(), "application/json");
  } else if ((isGET||isPOST) && route=="/api/relay") {
    if (q.idx.length() && q.on.length()) {
      uint8_t idx = (uint8_t) q.idx.toInt();
      bool on = (q.on == "1" || q.on == "true");
      setRelay(idx, on);
      ethSend_(client, buildStateJson(), "application/json");
    } else {
      client.print("HTTP/1.1 400 Bad Request\\r\\nConnection: close\\r\\n\\r\\nMissing idx/on");
    }
  } else if ((isGET||isPOST) && route=="/api/mask") {
    if (q.value.length()) {
      uint8_t v = (uint8_t) q.value.toInt();
      applyMaskToRelays(v);
      ethSend_(client, buildStateJson(), "application/json");
    } else {
      client.print("HTTP/1.1 400 Bad Request\\r\\nConnection: close\\r\\n\\r\\nMissing value");
    }
  } else if (isGET && route=="/reboot") {
    client.print("HTTP/1.1 200 OK\\r\\nContent-Type: text/plain\\r\\nConnection: close\\r\\n\\r\\nrebooting");
    client.flush(); delay(200); ESP.restart();
  } else {
    client.print("HTTP/1.1 404 Not Found\\r\\nConnection: close\\r\\n\\r\\nNot Found");
  }
  delay(1);
  client.stop();
}


void setup() {
  // Try Ethernet first
  bringupEthernet_();
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32-S3 8-Relay + RGB + WiFi/OTA/Web ===");

  // I2C + TCA9554 + RGB
  tcaInit();
  rgb.begin();
  rgb.setHeartbeatEnabled(true);   // pulsing LED when idle

  // Configure digital inputs
  BoardPins::configInputs();  // No pull-ups for opto-isolated inputs

  // Network stack
  wifiBegin();
  mdnsBegin();
  otaBegin();

  // Routes
  server.on("/",            HTTP_GET,  handleIndex);
  server.on("/api/state",   HTTP_GET,  handleState);
  server.on("/api/relay",   HTTP_POST, handleRelay);
  server.on("/api/mask",    HTTP_POST, handleMask);
  server.on("/reboot",      HTTP_GET,  [](){ server.send(200, "text/plain", "rebooting"); delay(200); ESP.restart(); });

  
  server.on("/api/diag", HTTP_GET, [](){
    String j = "{";
    j += "\"ota_ready\":";  j += (g_ota_ready ? "true":"false"); j += ",";
    j += "\"ota_port\":";   j += String(g_ota_port);             j += ",";
    j += "\"heap\":";       j += String(ESP.getFreeHeap());      j += "}";
    server.send(200, "application/json", j);
  });
  server.begin();
    Serial.println("[Web] Server started on port 80");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  ethHandleClient_();
  rgb.tick();

#ifdef DEBUG_RELAYS
  const uint32_t now = millis();
  if ((int32_t)(now - g_nextWalkMs) >= 0) {
    static uint8_t idx = 0;
    applyMaskToRelays(1u << idx);
    idx = (uint8_t)((idx + 1) % BoardPins::RELAY_COUNT);
    g_nextWalkMs = now + 1000;
  }
#endif
}

// -------------------- Digital Inputs (8x) ------------------------
// NOTE: This returns a RAW-HIGH mask: bit=1 means the pin reads HIGH on the MCU.
// Hardware is ACTIVE-LOW (pulling the line to GND indicates 'active'), so use
// diActiveMask()/diActive() helpers from StateHelpers.h when you want logical activity.
inline uint8_t readDI_mask() {
  uint8_t m = 0;
  for (uint8_t i = 0; i < BoardPins::DI_COUNT; ++i) {
    const uint8_t pin = INPUT_PINS[i];  // global aliases from BoardPins.h
    if (digitalRead(pin) == HIGH) m |= (1u << i);
  }
  return m;
}


String buildStateJson() {
  const IPAddress ip = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP() : WiFi.localIP();
  uint8_t di_mask = readDI_mask();
  String json = "{";
  json += "\"mask\":" + String(g_mask) + ",";
  json += "\"di_mask\":" + String(di_mask) + ",";
  json += "\"di\":[";
  for (uint8_t i = 0; i < BoardPins::DI_COUNT; ++i) {
    json += String((di_mask >> i) & 1);
    if (i < BoardPins::DI_COUNT - 1) json += ",";
  }
  json += "],";
  json += "\"count\":" + String(BoardPins::RELAY_COUNT) + ",";
  json += "\"wifi\":{";
  json += "\"ip\":\"" + ipStr(ip) + "\",";
  json += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + "}";
  json += "}";
  return json;
}
