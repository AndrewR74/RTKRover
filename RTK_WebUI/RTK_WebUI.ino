#include <WiFi.h>
#include <HardwareSerial.h>
#include <WebServer.h>
#include <Preferences.h>
#include "BleSerial.h"
#include "web_ui.h"

// ===================== PINS =====================
// ESP32 GPIO8 -> LG290P RXD  (ESP32 TX2)
// ESP32 GPIO9 -> LG290P TXD  (ESP32 RX2)
#define RXD2 D0
#define TXD2 D1

HardwareSerial gpsSerial(2);
WiFiClient     ntripClient;
BleSerial      ble;

WebServer   server(80);
Preferences prefs;

// ===================== AP (always on) =====================
static const char* AP_SSID = "RTK-Rover-Setup";
static const char* AP_PASS = "configureme";

// -------- DEFAULT CONFIG --------
const char* DEFAULT_WIFI_SSID  = "RINNE";     // empty = AP-only until user config
const char* DEFAULT_WIFI_PASS  = "";

const char* DEFAULT_NTRIP_HOST = "rtn.dot.ny.gov";
const int   DEFAULT_NTRIP_PORT = 8080;
const char* DEFAULT_MOUNT      = "net_msm_vrs";
const char* DEFAULT_NTRIP_USER = "AndrewRinneNTRIP";
const char* DEFAULT_NTRIP_PASS = "";

// ===================== CONFIG (persisted) =====================
struct Config {
  String wifiSsid;
  String wifiPass;

  String ntripHost;
  int    ntripPort;
  String mountpoint;
  String ntripUser;
  String ntripPass;
} cfg;

// ===================== GPS STATUS =====================
struct GPSData {
  bool  fix;
  int   satellites;
  float hdop;
  float latitude;
  float longitude;
  float speed;    // knots
  float heading;  // degrees
  int   fixType;  // 0=no fix, 1=SPS, 2=DGPS, 4=RTK FIX, 5=RTK FLOAT
} gps;

String partsGGA[15];
String partsRMC[12];
String nmeaSentence = "";
bool   hasGGA = false, hasRMC = false;
bool isConfigured = false;

String lastGGA = "";
unsigned long lastGGASend = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastRTCM = 0;

bool staConnecting = false;
unsigned long staStartTime = 0;

const unsigned long STA_CONNECT_TIMEOUT = 15000; // 15s max
const unsigned long STA_RETRY_BACKOFF   = 30000; // 30s wait
unsigned long lastStaAttempt = 0;

// ===================== SKY PLOT (optional) =====================
// If LG290P outputs GSA + GSV, we can show a sky plot.
static const int MAX_SATS = 32;

struct SatInfo {
  int prn = 0;
  int elev = -1;
  int az = -1;
  int snr = -1;
  bool used = false;
  bool valid = false;
};

SatInfo sats[MAX_SATS];
int usedPrns[16];
int usedPrnCount = 0;

void clearSatUsedFlags() {
  for (int i = 0; i < MAX_SATS; i++) sats[i].used = false;
}

void markUsedPRNs() {
  clearSatUsedFlags();
  for (int u = 0; u < usedPrnCount; u++) {
    int prn = usedPrns[u];
    for (int i = 0; i < MAX_SATS; i++) {
      if (sats[i].valid && sats[i].prn == prn) {
        sats[i].used = true;
        break;
      }
    }
  }
}

int upsertSat(int prn) {
  // find existing
  for (int i = 0; i < MAX_SATS; i++) {
    if (sats[i].valid && sats[i].prn == prn) return i;
  }
  // find empty
  for (int i = 0; i < MAX_SATS; i++) {
    if (!sats[i].valid) {
      sats[i].valid = true;
      sats[i].prn = prn;
      return i;
    }
  }
  // overwrite oldest slot if full
  sats[0].valid = true;
  sats[0].prn = prn;
  return 0;
}

// ===================== BASE64 (for Basic Auth) =====================
String base64Encode(const String &data) {
  const char* base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String ret;
  int val = 0, valb = -6;
  for (int i = 0; i < data.length(); i++) {
    uint8_t c = data.charAt(i);
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      ret += base64_chars[(val >> valb) & 0x3F];
      valb -= 6;
    }
  }
  if (valb > -6) ret += base64_chars[((val << 8) >> (valb + 8)) & 0x3F];
  while (ret.length() % 4) ret += '=';
  return ret;
}

// ===================== NMEA HELPERS =====================
void splitNMEA(const String &sentence, String *parts, int maxParts) {
  int startIdx = 0;
  int partIdx  = 0;
  while (partIdx < maxParts) {
    int commaIdx = sentence.indexOf(',', startIdx);
    if (commaIdx == -1) {
      parts[partIdx++] = sentence.substring(startIdx);
      break;
    }
    parts[partIdx++] = sentence.substring(startIdx, commaIdx);
    startIdx = commaIdx + 1;
  }
}

float convertToDecimalDegree(const String &val, const String &direction) {
  if (val.length() < 6) return 0.0;

  float deg = 0.0;
  float min = 0.0;

  if (direction == "N" || direction == "S") {
    deg = val.substring(0, 2).toFloat();
    min = val.substring(2).toFloat();
  } else if (direction == "E" || direction == "W") {
    deg = val.substring(0, 3).toFloat();
    min = val.substring(3).toFloat();
  } else {
    return 0.0;
  }

  float decDeg = deg + min / 60.0;
  if (direction == "S" || direction == "W") decDeg = -decDeg;
  return decDeg;
}

void parseGNGGA(const String &sentence) {
  splitNMEA(sentence, partsGGA, 15);

  gps.fixType    = partsGGA[6].toInt();
  gps.fix        = (gps.fixType > 0);
  gps.satellites = partsGGA[7].toInt();
  gps.hdop       = partsGGA[8].toFloat();
  gps.latitude   = convertToDecimalDegree(partsGGA[2], partsGGA[3]);
  gps.longitude  = convertToDecimalDegree(partsGGA[4], partsGGA[5]);

  lastGGA = sentence;
}

void parseGNRMC(const String &sentence) {
  splitNMEA(sentence, partsRMC, 12);
  if (partsRMC[2] == "A") {
    gps.speed   = partsRMC[7].toFloat();
    gps.heading = partsRMC[8].toFloat();
  } else {
    gps.speed   = 0;
    gps.heading = 0;
  }
}

// GSA: captures "used in fix" PRNs
void parseGSA(const String& sentence) {
  // $GPGSA,<mode1>,<mode2>,<sat1>,...,<sat12>,<pdop>,<hdop>,<vdop>*CS
  // We'll grab PRNs fields 3..14 (12 sats)
  String parts[20];
  splitNMEA(sentence, parts, 20);

  usedPrnCount = 0;
  for (int i = 3; i <= 14 && usedPrnCount < 16; i++) {
    if (parts[i].length() == 0) continue;
    usedPrns[usedPrnCount++] = parts[i].toInt();
  }
  markUsedPRNs();
}

// GSV: updates az/el/snr
void parseGSV(const String& sentence) {
  // $GPGSV,totalMsgs,msgNum,totalSats, PRN, elev, az, snr, PRN, elev, az, snr, ...
  String parts[25];
  splitNMEA(sentence, parts, 25);

  // starting at field 4, groups of 4
  for (int i = 4; i + 3 < 25; i += 4) {
    if (parts[i].length() == 0) break;
    int prn = parts[i].toInt();
    int el  = parts[i + 1].toInt();
    int az  = parts[i + 2].toInt();
    int snr = parts[i + 3].toInt(); // may have *CS, toInt handles until non-digit

    int idx = upsertSat(prn);
    sats[idx].elev = el;
    sats[idx].az   = az;
    sats[idx].snr  = snr;
  }
  markUsedPRNs();
}

// ===================== PRINT GPS DATA =====================
void printGPSData() {
  Serial.println("-------------");
  Serial.print("Positioning Status: ");
  Serial.println(gps.fix ? "Yes" : "No");

  Serial.print("RTK Status: ");
  switch (gps.fixType) {
    case 0: Serial.println("No Fix"); break;
    case 1: Serial.println("GPS SPS Mode"); break;
    case 2: Serial.println("DGNSS (SBAS/DGPS)"); break;
    case 3: Serial.println("GPS PPS Fix"); break;
    case 4: Serial.println("Fixed RTK"); break;
    case 5: Serial.println("Float RTK"); break;
    default:
      Serial.print("Unknown (");
      Serial.print(gps.fixType);
      Serial.println(")");
      break;
  }

  Serial.print("Satellites: ");
  Serial.println(gps.satellites);
  Serial.print("HDOP: ");
  Serial.println(gps.hdop, 2);
  Serial.print("Latitude: ");
  Serial.println(gps.latitude, 6);
  Serial.print("Longitude: ");
  Serial.println(gps.longitude, 6);
  Serial.print("Speed (knots): ");
  Serial.println(gps.speed, 3);
  Serial.print("Heading (degrees): ");
  Serial.println(gps.heading, 2);

  Serial.print("NTRIP: ");
  Serial.println(ntripClient.connected() ? "Connected" : "Disconnected");
  Serial.println("-------------");
}

// ===================== PREFERENCES (NVS) =====================
void loadConfig() {
  prefs.begin("rtk", true);

  isConfigured = prefs.getBool("configured", false);

  cfg.wifiSsid   = prefs.getString("ssid", "");
  cfg.wifiPass   = prefs.getString("wpass", "");

  cfg.ntripHost  = prefs.getString("nhost", "rtn.dot.ny.gov");
  cfg.ntripPort  = prefs.getInt("nport", 8080);
  cfg.mountpoint = prefs.getString("nmount", "net_msm_vrs");
  cfg.ntripUser  = prefs.getString("nuser", "");
  cfg.ntripPass  = prefs.getString("npass", "");
  prefs.end();
}

void saveConfig() {
  prefs.begin("rtk", false);
  
  prefs.putBool("configured", true);

  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("wpass", cfg.wifiPass);

  prefs.putString("nhost", cfg.ntripHost);
  prefs.putInt("nport", cfg.ntripPort);
  prefs.putString("nmount", cfg.mountpoint);
  prefs.putString("nuser", cfg.ntripUser);
  prefs.putString("npass", cfg.ntripPass);
  prefs.end();
}

void saveDefaultConfig() {
  prefs.begin("rtk", false);

  prefs.putBool("configured", false);

  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("wpass", cfg.wifiPass);

  prefs.putString("nhost", cfg.ntripHost);
  prefs.putInt("nport", cfg.ntripPort);
  prefs.putString("nmount", cfg.mountpoint);
  prefs.putString("nuser", cfg.ntripUser);
  prefs.putString("npass", cfg.ntripPass);

  prefs.end();
}

void applyDefaultConfig() {
  Serial.println("No config found — applying defaults");

  cfg.wifiSsid   = DEFAULT_WIFI_SSID;
  cfg.wifiPass   = DEFAULT_WIFI_PASS;

  cfg.ntripHost  = DEFAULT_NTRIP_HOST;
  cfg.ntripPort  = DEFAULT_NTRIP_PORT;
  cfg.mountpoint = DEFAULT_MOUNT;
  cfg.ntripUser  = DEFAULT_NTRIP_USER;
  cfg.ntripPass  = DEFAULT_NTRIP_PASS;
}

void factoryReset() {
  prefs.begin("rtk", false);
  prefs.clear();
  prefs.end();
}

// ===================== WIFI / NTRIP =====================
void connectWiFi() {
  if (cfg.wifiSsid.length() == 0) return;

  Serial.print("Connecting to WiFi: ");
  Serial.println(cfg.wifiSsid);

  WiFi.disconnect(true);        // ensure clean state
  WiFi.setAutoReconnect(false); // IMPORTANT
  WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
  staConnecting = true;
  staStartTime = millis();

  // unsigned long t0 = millis();
  // while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
  //   delay(300);
  //   Serial.print(".");
  // }
  // Serial.println();

  // if (WiFi.status() == WL_CONNECTED) {
  //   Serial.print("WiFi connected, IP: ");
  //   Serial.println(WiFi.localIP());
  // } else {
  //   Serial.println("WiFi connect failed.");
  // }
}

void handleSTA() {
  wl_status_t st = WiFi.status();

  // ---- Connected ----
  if (st == WL_CONNECTED) {
    if (staConnecting) {
      Serial.print("STA connected, IP: ");
      Serial.println(WiFi.localIP());
    }
    staConnecting = false;
    return;
  }

  // ---- Actively connecting ----
  if (staConnecting) {
    if (millis() - staStartTime > STA_CONNECT_TIMEOUT) {
      Serial.println("STA connect timed out");

      WiFi.disconnect(true);
      staConnecting = false;
      lastStaAttempt = millis();
    }
    return;
  }

  // ---- Retry later ----
  if (cfg.wifiSsid.length() == 0) return;

  if (millis() - lastStaAttempt > STA_RETRY_BACKOFF) {
    Serial.println("Retrying STA connection...");
    WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
    staConnecting = true;
    staStartTime = millis();
  }
}

void connectToNTRIP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skip NTRIP.");
    return;
  }
  if (cfg.ntripHost.length() == 0 || cfg.mountpoint.length() == 0) {
    Serial.println("NTRIP not configured, skip.");
    return;
  }

  Serial.printf("Connecting to NTRIP %s:%d\n", cfg.ntripHost.c_str(), cfg.ntripPort);
  if (!ntripClient.connect(cfg.ntripHost.c_str(), cfg.ntripPort)) {
    Serial.println("NTRIP connect failed!");
    return;
  }

  String auth       = cfg.ntripUser + ":" + cfg.ntripPass;
  String authBase64 = base64Encode(auth);

  String request =
    String("GET /") + cfg.mountpoint + " HTTP/1.1\r\n" +
    "User-Agent: NTRIP client\r\n" +
    "Accept: */*\r\n" +
    "Connection: keep-alive\r\n" +
    "Authorization: Basic " + authBase64 + "\r\n" +
    "\r\n";

  ntripClient.print(request);
  Serial.println("NTRIP request sent.");

  String header = "";
  unsigned long t0 = millis();
  while (millis() - t0 < 3000 && ntripClient.connected()) {
    while (ntripClient.available()) {
      char c = ntripClient.read();
      header += c;
      if (header.endsWith("\r\n\r\n")) break;
    }
    if (header.endsWith("\r\n\r\n")) break;
  }

  Serial.println("NTRIP response header:");
  Serial.println("-----");
  Serial.println(header);
  Serial.println("-----");

  if (header.indexOf("200") < 0 && header.indexOf("ICY 200") < 0) {
    Serial.println("NTRIP handshake failed (no 200), closing connection.");
    ntripClient.stop();
    return;
  }

  Serial.println("NTRIP OK - Sending last GGA (if available)");
  if (lastGGA.length() > 0) {
    ntripClient.print(lastGGA);
    ntripClient.print("\r\n");
    ntripClient.flush();
  }

  Serial.println("NTRIP connected, waiting for RTCM...");
}

// ===================== WEB UI =====================

// Bootstrap + Leaflet (OpenStreetMap) page with Map + Compass + Skyplot.
// No API key required.

String fixLabel(int fixType) {
  switch (fixType) {
    case 0: return "NO FIX";
    case 1: return "GPS";
    case 2: return "DGPS";
    case 4: return "RTK FIX";
    case 5: return "RTK FLOAT";
    default: return String("FIX ") + fixType;
  }
}

String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\\' || c == '\"') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else out += c;
  }
  return out;
}

void handleRoot() {
  server.send_P(200, "text/html", WEB_UI_HTML);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    json += "\"" + jsonEscape(WiFi.SSID(i)) + "\"";
    if (i < n - 1) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleConfig() {
  String json =
    "{"
    "\"ssid\":\""   + jsonEscape(cfg.wifiSsid)  + "\","
    "\"wpass\":\""  + jsonEscape(cfg.wifiPass)  + "\","
    "\"nhost\":\""  + jsonEscape(cfg.ntripHost) + "\","
    "\"nport\":"    + String(cfg.ntripPort)     + ","
    "\"nmount\":\"" + jsonEscape(cfg.mountpoint)+ "\","
    "\"nuser\":\""  + jsonEscape(cfg.ntripUser) + "\","
    "\"npass\":\""  + jsonEscape(cfg.ntripPass) + "\""
    "}";
  server.send(200, "application/json", json);
}

void handleStatus() {
  String satsJson = "[]";
  // Build sats_detail array only if we have some GSV content
  // Keep this small to avoid heap churn.
  {
    String arr = "[";
    bool first = true;
    for (int i = 0; i < MAX_SATS; i++) {
      if (!sats[i].valid) continue;
      if (sats[i].elev < 0 || sats[i].az < 0) continue;

      if (!first) arr += ",";
      first = false;

      arr += "{";
      arr += "\"id\":" + String(sats[i].prn) + ",";
      arr += "\"az\":" + String(sats[i].az) + ",";
      arr += "\"el\":" + String(sats[i].elev) + ",";
      arr += "\"used\":" + String(sats[i].used ? "true" : "false");
      arr += "}";
    }
    arr += "]";
    satsJson = arr;
  }

  String fix = fixLabel(gps.fixType);

  String json =
    "{"
    "\"fix\":\"" + fix + "\","
    "\"sats\":" + String(gps.satellites) + ","
    "\"hdop\":" + String(gps.hdop, 2) + ","
    "\"lat\":"  + String(gps.latitude, 6) + ","
    "\"lon\":"  + String(gps.longitude, 6) + ","
    "\"speed_kn\":" + String(gps.speed, 3) + ","
    "\"heading_deg\":" + String(gps.heading, 2) + ","
    "\"wifi\":\"" + jsonEscape(WiFi.SSID()) + "\","
    "\"ip\":\"" + WiFi.localIP().toString() + "\","
    "\"rssi\":" + String(WiFi.RSSI()) + ","
    "\"ntrip\":\"" + String(ntripClient.connected() ? "Connected" : "Disconnected") + "\","
    "\"rtcmtime\":" + String((millis() - lastRTCM) / 1000) + ","
    "\"sats_detail\":" + satsJson +
    "}";
  server.send(200, "application/json", json);
}

void handleSave() {
  cfg.wifiSsid   = server.arg("ssid");
  cfg.wifiPass   = server.arg("wpass");

  cfg.ntripHost  = server.arg("nhost");
  cfg.ntripPort  = server.arg("nport").toInt();
  cfg.mountpoint = server.arg("nmount");
  cfg.ntripUser  = server.arg("nuser");
  cfg.ntripPass  = server.arg("npass");

  if (cfg.ntripPort <= 0) cfg.ntripPort = 2101;

  saveConfig();

  server.send(200, "text/plain", "Saved. Rebooting...");
  delay(700);
  ESP.restart();
}

void handleReboot() {
  server.send(200, "text/plain", "Rebooting...");
  delay(300);
  ESP.restart();
}

void handleReset() {
  server.send(200, "text/plain", "Resetting...");
  delay(200);
  factoryReset();
  delay(200);
  ESP.restart();
}

void startWeb() {
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/config", handleConfig);
  server.on("/status", handleStatus);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/reset", HTTP_POST, handleReset);

  server.begin();
}

// ===================== SETUP / LOOP =====================
void setup() {
  Serial.begin(460800);
  delay(300);

  Serial.println("ESP32-S3 LG290P RTK Rover starting...");

  // GNSS UART
  gpsSerial.begin(460800, SERIAL_8N1, RXD2, TXD2);

  // BLE (your existing library)
  ble.begin("ESP32_BleSerial");
  Serial.println("BLE Serial started.");

  // Load config from NVS
  loadConfig();

  if (!isConfigured) {
    applyDefaultConfig();
    saveDefaultConfig();
  }

  // Always enable AP + STA so UI is reachable in all cases
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Try STA connect if configured
  if (cfg.wifiSsid.length() > 0) {
    connectWiFi();
  }

  // Start web UI
  startWeb();

  // NTRIP will connect after we have a GGA (so we can send it immediately)
  Serial.println("Waiting for first GGA before connecting NTRIP...");
}

void loop() {
  server.handleClient();

  // 1) Receive RTCM from NTRIP and forward to LG290P
  if (ntripClient.connected()) {
    while (ntripClient.available()) {
      uint8_t c = ntripClient.read();
      gpsSerial.write(c);

      if (c == 0xD3) {
        lastRTCM = millis();
        // Serial.print("[RTCM]"); // noisy; enable if needed
      }
    }
  }

  // 2) Read NMEA from LG290P
  while (gpsSerial.available()) {
    char c = gpsSerial.read();

    if (c == '\n') {
      nmeaSentence.trim();

      // Forward raw NMEA to BLE if connected (unchanged)
      if (ble.connected()) {
        ble.print(nmeaSentence + "\r\n");
      }

      if (nmeaSentence.startsWith("$GNGGA") || nmeaSentence.startsWith("$GPGGA")) {
        // First time we get a location, connect STA (if not already) and NTRIP
        bool firstGga = (lastGGA.length() == 0);

        parseGNGGA(nmeaSentence);
        hasGGA = true;

        if (firstGga) {
          //if (WiFi.status() != WL_CONNECTED)
          //  connectWiFi();
            connectToNTRIP();
        }

      } else if (nmeaSentence.startsWith("$GNRMC") || nmeaSentence.startsWith("$GPRMC")) {
        parseGNRMC(nmeaSentence);
        hasRMC = true;

      } else if (nmeaSentence.startsWith("$GNGSA") || nmeaSentence.startsWith("$GPGSA")) {
        parseGSA(nmeaSentence);

      } else if (nmeaSentence.startsWith("$GNGSV") || nmeaSentence.startsWith("$GPGSV")) {
        parseGSV(nmeaSentence);
      }

      nmeaSentence = "";

    } else if (c != '\r') {
      nmeaSentence += c;
    }
  }

  // 3) When both GGA and RMC are updated, print GPS info
  if (hasGGA && hasRMC) {
    //printGPSData();
    hasGGA = hasRMC = false;
  }

  // 4) Send GGA to NTRIP every 5 seconds
  if (ntripClient.connected() && lastGGA.length() > 0 && millis() - lastGGASend > 5000) {
    ntripClient.print(lastGGA);
    ntripClient.print("\r\n");
    lastGGASend = millis();
    Serial.println(String("[GGA sent to NTRIP] ") + lastGGA);
  }

  // 5) Check Wi-Fi periodically if we have config
  handleSTA();
  // if (cfg.wifiSsid.length() > 0 &&
  //     WiFi.status() != WL_CONNECTED &&
  //     millis() - lastWiFiAttempt > 10000) {
  //   connectWiFi();
  //   lastWiFiAttempt = millis();
  // }

  // 6) Reconnect NTRIP if disconnected (once we have GGA)
  if (lastGGA.length() > 0 && WiFi.status() == WL_CONNECTED && !ntripClient.connected()) {
    // backoff a little to avoid hammering
    static unsigned long lastNtripTry = 0;
    if (millis() - lastNtripTry > 5000) {
      Serial.println("NTRIP disconnected, reconnecting...");
      ntripClient.stop();
      connectToNTRIP();
      lastNtripTry = millis();
    }
  }

  delay(1);
}
