/*********************************************************************
 * ESP32 Minimal-Sketch:
 *  - Webinterface zur Konfiguration: WLAN (SSID, Pass), API-Username/Password/DeviceName
 *  - Hardcoded URL & Endpoints:
 *      Login: https://vpn23.com/login
 *      WG-Config: https://vpn23.com/clients/name/<DeviceName>/config
 *  - Preferences (NVS) zum Speichern von WLAN & API-Credentials
 *  - WireGuard-ESP32 zum Starten der VPN-Verbindung
 *  - TLS (HTTPS) via WiFiClientSecure
 *
 * WICHTIG:
 *  - Installiere folgende Libraries:
 *    - ArduinoJson
 *    - WireGuard-ESP32 (https://github.com/ciniml/WireGuard-ESP32-Arduino)
 *    - Ggf. (optionaler) Zeitabgleich via NTP für echte Zertifikatsvalidierung
 *********************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WireGuard-ESP32.h>

// Falls du für echte TLS-Validierung die Zeit brauchst, binde <time.h> ein und nutze configTime(...) etc.

// Webserver
WebServer server(80);

// WireGuard
WireGuard wg;

// Preferences-Speicher
Preferences preferences;

// Hardcodete URLs / Endpoints
static const char* LOGIN_URL        = "https://vpn23.com/login";
static const char* WIREGUARD_URL_PT1 = "https://vpn23.com/clients/name/";
static const char* WIREGUARD_URL_PT2 = "/config"; // Wird zu .../clients/name/<deviceName>/config

// Beispiel-Root-CA (z.B. GTS Root R4) - Bei Bedarf anpassen!
static const char *rootCACert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX
...
-----END CERTIFICATE-----
)EOF";

// Struktur für unsere minimalen Einstellungen
struct Config {
  String wifiSSID;
  String wifiPass;
  String apiUser;
  String apiPass;
  String deviceName;
  
  // Aus WG-Config
  String privateKey;
  String address;
  String dns;
  String endpoint;
  String publicKey;
  String presharedKey;
};

// Globale Konfiguration
Config configData;

/************************************************
 * HILFSFUNKTIONEN ZUM HTML-ESCAPE
 ***********************************************/
String htmlEscape(const String &s) {
  String escaped;
  for (char c : s) {
    switch (c) {
      case '<': escaped += "&lt;"; break;
      case '>': escaped += "&gt;"; break;
      case '&': escaped += "&amp;"; break;
      case '"': escaped += "&quot;"; break;
      default: escaped += c; break;
    }
  }
  return escaped;
}

/************************************************
 * NVS: LADEN & SPEICHERN
 ***********************************************/
void loadConfig() {
  preferences.begin("myapp-config", true); // read-only
  configData.wifiSSID   = preferences.getString("wifiSSID", "");
  configData.wifiPass   = preferences.getString("wifiPass", "");
  configData.apiUser    = preferences.getString("apiUser", "");
  configData.apiPass    = preferences.getString("apiPass", "");
  configData.deviceName = preferences.getString("devName", "ESP32");
  
  // WireGuard Felder werden nur geholt, wenn der Server sie liefert. 
  // Falls man manuell was setzt, könnte man das auch speichern.
  configData.privateKey   = preferences.getString("privKey", "");
  configData.address      = preferences.getString("address", "");
  configData.dns          = preferences.getString("dns", "");
  configData.endpoint     = preferences.getString("endpoint", "");
  configData.publicKey    = preferences.getString("pubKey", "");
  configData.presharedKey = preferences.getString("psk", "");
  
  preferences.end();
}

void saveConfig() {
  preferences.begin("myapp-config", false); // writeable
  preferences.putString("wifiSSID",   configData.wifiSSID);
  preferences.putString("wifiPass",   configData.wifiPass);
  preferences.putString("apiUser",    configData.apiUser);
  preferences.putString("apiPass",    configData.apiPass);
  preferences.putString("devName",    configData.deviceName);
  
  // WireGuard Felder ebenfalls in Preferences sichern
  preferences.putString("privKey",    configData.privateKey);
  preferences.putString("address",    configData.address);
  preferences.putString("dns",        configData.dns);
  preferences.putString("endpoint",   configData.endpoint);
  preferences.putString("pubKey",     configData.publicKey);
  preferences.putString("psk",        configData.presharedKey);

  preferences.end();
}

/************************************************
 * WLAN VERBINDEN
 ***********************************************/
bool connectToWiFi(const String &ssid, const String &pass) {
  if (ssid.isEmpty() || pass.isEmpty()) {
    Serial.println("[WIFI] SSID oder Passwort ist leer!");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  Serial.print("[WIFI] Verbinde mit ");
  Serial.println(ssid);
  unsigned long start = millis();
  const unsigned long timeout = 15000; // 15s
  
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\n[WIFI] Verbunden! IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n[WIFI] Keine Verbindung!");
    return false;
  }
}

/************************************************
 * WEBINTERFACE: SEITEN
 ***********************************************/
WebServer webServer(80);

void handleRoot() {
  // Minimal-Seite mit WLAN-Status, ob WireGuard läuft, etc.
  String html = "<html><head><title>ESP32 Setup</title></head><body>";
  html += "<h1>ESP32 Setup - Hardcoded URLs</h1>";

  // WLAN-Status
  html += "<p><strong>WLAN:</strong> ";
  if (WiFi.isConnected()) {
    html += "verbunden mit " + htmlEscape(WiFi.SSID()) + ", IP: " + WiFi.localIP().toString();
  } else {
    html += "<span style='color:red'>nicht verbunden</span>";
  }
  html += "</p>";

  // Link zur Konfig-Seite
  html += "<p><a href='/config'>Konfiguration</a></p>";
  
  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

void handleConfigPage() {
  // Formular für SSID, WLAN-PW, API-User, API-Pass, DeviceName
  String html = "<html><head><title>ESP32 Konfiguration</title></head><body>";
  html += "<h1>Konfiguration</h1>";
  html += "<form method='POST' action='/saveConfig'>";

  // WLAN
  html += "<h3>WLAN</h3>";
  html += "SSID: <input type='text' name='wifiSSID' value='" + htmlEscape(configData.wifiSSID) + "'/><br/>";
  html += "PW: <input type='password' name='wifiPass' value='" + htmlEscape(configData.wifiPass) + "'/><br/>";

  // API
  html += "<h3>API / WireGuard</h3>";
  html += "Username: <input type='text' name='apiUser' value='" + htmlEscape(configData.apiUser) + "'/><br/>";
  html += "Password: <input type='password' name='apiPass' value='" + htmlEscape(configData.apiPass) + "'/><br/>";
  html += "DeviceName: <input type='text' name='devName' value='" + htmlEscape(configData.deviceName) + "'/><br/>";

  // Abschicken
  html += "<br/><input type='submit' value='Speichern'/></form>";
  html += "<p><a href='/'>Zurück</a></p>";
  html += "</body></html>";

  webServer.send(200, "text/html", html);
}

void handleSaveConfig() {
  // Werte auslesen
  if (webServer.hasArg("wifiSSID"))   configData.wifiSSID   = webServer.arg("wifiSSID");
  if (webServer.hasArg("wifiPass"))   configData.wifiPass   = webServer.arg("wifiPass");
  if (webServer.hasArg("apiUser"))    configData.apiUser    = webServer.arg("apiUser");
  if (webServer.hasArg("apiPass"))    configData.apiPass    = webServer.arg("apiPass");
  if (webServer.hasArg("devName"))    configData.deviceName = webServer.arg("devName");
  
  // Speichern
  saveConfig();

  // Bestätigung
  String html = "<html><body><h1>Gespeichert!</h1>";
  html += "<p><a href='/'>Zurück</a></p></body></html>";
  webServer.send(200, "text/html", html);
}

/************************************************
 * LOGIN (HTTPS) -> JWT
 ***********************************************/
String getJwtToken() {
  if (!WiFi.isConnected()) return "";
  
  Serial.println("[API] Hole JWT von: " + String(LOGIN_URL));
  
  WiFiClientSecure client;
  client.setCACert(rootCACert);

  HTTPClient http;
  if (!http.begin(client, LOGIN_URL)) {
    Serial.println("[API] http.begin() fehlgeschlagen!");
    return "";
  }
  
  // JSON-Body (Username, Password, DeviceName)
  StaticJsonDocument<256> doc;
  doc["username"]   = configData.apiUser;
  doc["password"]   = configData.apiPass;
  doc["deviceName"] = configData.deviceName;

  String body;
  serializeJson(doc, body);

  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(body);
  if (httpCode == 200 || httpCode == 201) {
    String resp = http.getString();
    http.end();
    Serial.println("[API] Antwort: " + resp);

    StaticJsonDocument<512> respDoc;
    DeserializationError err = deserializeJson(respDoc, resp);
    if (!err) {
      const char* token = respDoc["token"];
      if (token) {
        Serial.println("[API] JWT empfangen");
        return String(token);
      }
    }
  } else {
    Serial.printf("[API] Login fehlgeschlagen, Code=%d\n", httpCode);
    http.end();
  }
  return "";
}

/************************************************
 * WIREGUARD-KONFIG -> GET /clients/name/<deviceName>/config
 ***********************************************/
bool fetchWireGuardConfig(const String &jwt) {
  if (!WiFi.isConnected()) return false;
  
  // Hardcoded URL
  String url = String(WIREGUARD_URL_PT1) + configData.deviceName + String(WIREGUARD_URL_PT2);
  Serial.println("[WG] Abruf von: " + url);

  WiFiClientSecure client;
  client.setCACert(rootCACert);

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("[WG] http.begin() fehlgeschlagen!");
    return false;
  }
  if (!jwt.isEmpty()) {
    String bearer = "Bearer " + jwt;
    http.addHeader("Authorization", bearer);
  }

  int httpCode = http.GET();
  if (httpCode == 200) {
    String resp = http.getString();
    http.end();
    Serial.println("[WG] WG-Config: " + resp);

    // JSON parsen
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (!err) {
      // Felder übernehmen
      configData.privateKey   = doc["private_key"]   | "";
      configData.address      = doc["address"]       | "";
      configData.dns          = doc["dns"]           | "";
      configData.endpoint     = doc["endpoint"]      | "";
      configData.publicKey    = doc["public_key"]    | "";
      configData.presharedKey = doc["preshared_key"] | "";

      // Speichern in NVS
      saveConfig();
      return true;
    } else {
      Serial.println("[WG] JSON-Fehler beim Parsen!");
    }
  } else {
    Serial.printf("[WG] GET fehlgeschlagen, Code=%d\n", httpCode);
    http.end();
  }
  return false;
}

/************************************************
 * WIREGUARD STARTEN
 ***********************************************/
bool startWireGuard() {
  if (configData.privateKey.isEmpty() || configData.publicKey.isEmpty()) {
    Serial.println("[WG] Private/Public Key fehlen!");
    return false;
  }

  WGConfig c;
  c.private_key   = configData.privateKey;
  c.address       = configData.address;
  c.dns           = configData.dns;
  c.endpoint      = configData.endpoint;
  c.public_key    = configData.publicKey;
  c.preshared_key = configData.presharedKey;

  // c.allowedIPs.push_back("0.0.0.0/0"); // Optional, falls alles geroutet werden soll
  // c.keep_alive = 25;

  if (wg.begin(c)) {
    Serial.println("[WG] WireGuard gestartet!");
    return true;
  } else {
    Serial.println("[WG] Konnte WireGuard nicht starten.");
    return false;
  }
}

/************************************************
 * SETUP
 ***********************************************/
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Konfiguration laden
  loadConfig();

  // WLAN verbinden
  bool wifiOk = connectToWiFi(configData.wifiSSID, configData.wifiPass);
  if (!wifiOk) {
    // Falls WLAN fehlschlägt, AP starten
    Serial.println("[WIFI] Starte AP-Modus...");
    WiFi.softAP("ESP32_Config", "12345678");
    Serial.println("[WIFI] AP-IP: " + WiFi.softAPIP().toString());
  } else {
    // Optional: Zeit via NTP holen, um TLS-Zert. zu validieren
    // z.B. configTime(0, 0, "pool.ntp.org"); delay(2000);

    // JWT holen, WG-Konfig abrufen, WG starten
    String token = getJwtToken();
    bool ok = fetchWireGuardConfig(token);
    if (ok) {
      startWireGuard();
    }
  }

  // Webserver-Endpunkte
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/config", HTTP_GET, handleConfigPage);
  webServer.on("/saveConfig", HTTP_POST, handleSaveConfig);

  webServer.onNotFound([](){
    webServer.send(404, "text/plain", "Not found");
  });

  // Webserver starten
  webServer.begin();
  Serial.println("[WEB] Server gestartet (Port 80)");
}

/************************************************
 * LOOP
 ***********************************************/
void loop() {
  // Webserver abarbeiten
  webServer.handleClient();
  
  // Hier könntest du z.B. WireGuard-Status pollen, ...
  delay(10);
}
