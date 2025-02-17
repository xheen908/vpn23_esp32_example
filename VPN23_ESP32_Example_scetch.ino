/*********************************************************************
 * ESP32 Minimal Sketch:
 *  - Web interface for configuring: WLAN (SSID, password), 
 *    API username/password/deviceName
 *  - Hardcoded URL & Endpoints:
 *      Login: https://vpn23.com/login
 *      WG-Config: https://vpn23.com/clients/name/<deviceName>/config
 *  - Preferences (NVS) to store WLAN & API credentials
 *  - WireGuard-ESP32 to start the VPN connection
 *  - TLS (HTTPS) via WiFiClientSecure
 *
 * IMPORTANT:
 *  - Install the following libraries:
 *    - ArduinoJson
 *    - WireGuard-ESP32 (https://github.com/ciniml/WireGuard-ESP32-Arduino)
 *  - Optionally (for proper certificate validation), get the time via NTP
 *********************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WireGuard-ESP32.h>

// If you need actual TLS validation time checks, include <time.h> and use configTime(...)

// Web server
WebServer server(80);

// WireGuard
WireGuard wg;

// Preferences
Preferences preferences;

// Hardcoded URLs / Endpoints
static const char* LOGIN_URL         = "https://vpn23.com/login";
static const char* WIREGUARD_URL_PT1 = "https://vpn23.com/clients/name/";
static const char* WIREGUARD_URL_PT2 = "/config"; // will become .../clients/name/<deviceName>/config

// Example Root-CA (GTS Root R4). Adjust if needed!
// Full certificate included:
static const char *rootCACert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX
MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE
CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx
NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT
GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0
MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube
Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e
WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H+MIH8MA4GA1UdDwEB/wQEAwIBhjAd
BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd
BgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0jBBgwFoAUYHtmGkUN
l8qJUC99BM00qP/8/UswNgYIKwYBBQUHAQEEKjAoMCYGCCsGAQUFBzAChhpodHRw
Oi8vaS5wa2kuZ29vZy9nc3IxLmNydDAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8v
Yy5wa2kuZ29vZy9yL2dzcjEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqG
SIb3DQEBCwUAA4IBAQAYQrsPBtYDh5bjP2OBDwmkoWhIDDkic574y04tfzHpn+cJ
odI2D4SseesQ6bDrarZ7C30ddLibZatoKiws3UL9xnELz4ct92vID24FfVbiI1hY
+SW6FoVHkNeWIP0GCbaM4C6uVdF5dTUsMVs/ZbzNnIdCp5Gxmx5ejvEau8otR/Cs
kGN+hr/W5GvT1tMBjgWKZ1i4//emhA1JG1BbPzoLJQvyEotc03lXjTaCzv8mEbep
8RqZ7a2CPsgRbuvTPBwcOMBBmuFeU88+FSBX6+7iP0il8b4Z0QFqIwwMHfs/L6K1
vepuoxtGzi4CZ68zJpiq1UvSqTbFJjtbD4seiMHl
-----END CERTIFICATE-----
)EOF";

// Structure for our minimal settings
struct Config {
  String wifiSSID;
  String wifiPass;
  String apiUser;
  String apiPass;
  String deviceName;
  
  // Received from the WG config
  String privateKey;
  String address;
  String dns;
  String endpoint;
  String publicKey;
  String presharedKey;
};

// Global configuration
Config configData;

/************************************************
 * HTML-ESCAPING HELPER
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
 * NVS: LOAD & SAVE
 ***********************************************/
void loadConfig() {
  preferences.begin("myapp-config", true); // read-only
  configData.wifiSSID   = preferences.getString("wifiSSID", "");
  configData.wifiPass   = preferences.getString("wifiPass", "");
  configData.apiUser    = preferences.getString("apiUser", "");
  configData.apiPass    = preferences.getString("apiPass", "");
  configData.deviceName = preferences.getString("devName", "ESP32");
  
  // WG fields are only filled when received from the server.
  // If manually set, you could also store them.
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
  
  // Also store the WG fields in Preferences
  preferences.putString("privKey",    configData.privateKey);
  preferences.putString("address",    configData.address);
  preferences.putString("dns",        configData.dns);
  preferences.putString("endpoint",   configData.endpoint);
  preferences.putString("pubKey",     configData.publicKey);
  preferences.putString("psk",        configData.presharedKey);

  preferences.end();
}

/************************************************
 * WLAN CONNECTION
 ***********************************************/
bool connectToWiFi(const String &ssid, const String &pass) {
  if (ssid.isEmpty() || pass.isEmpty()) {
    Serial.println("[WIFI] SSID or password is empty!");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  Serial.print("[WIFI] Connecting to ");
  Serial.println(ssid);
  unsigned long start = millis();
  const unsigned long timeout = 15000; // 15s
  
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\n[WIFI] Connected! IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n[WIFI] Failed to connect!");
    return false;
  }
}

/************************************************
 * WEB INTERFACE: PAGES
 ***********************************************/
WebServer webServer(80);

void handleRoot() {
  // Minimal page showing WLAN status, if WireGuard is running, etc.
  String html = "<html><head><title>ESP32 Setup</title></head><body>";
  html += "<h1>ESP32 Setup - Hardcoded URLs</h1>";

  // WLAN status
  html += "<p><strong>WLAN:</strong> ";
  if (WiFi.isConnected()) {
    html += "connected to " + htmlEscape(WiFi.SSID()) + ", IP: " + WiFi.localIP().toString();
  } else {
    html += "<span style='color:red'>not connected</span>";
  }
  html += "</p>";

  // Link to the config page
  html += "<p><a href='/config'>Configuration</a></p>";
  
  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

void handleConfigPage() {
  // Form for SSID, WLAN password, API user, API pass, deviceName
  String html = "<html><head><title>ESP32 Configuration</title></head><body>";
  html += "<h1>Configuration</h1>";
  html += "<form method='POST' action='/saveConfig'>";

  // WLAN
  html += "<h3>WLAN</h3>";
  html += "SSID: <input type='text' name='wifiSSID' value='" + htmlEscape(configData.wifiSSID) + "'/><br/>";
  html += "Password: <input type='password' name='wifiPass' value='" + htmlEscape(configData.wifiPass) + "'/><br/>";

  // API
  html += "<h3>API / WireGuard</h3>";
  html += "Username: <input type='text' name='apiUser' value='" + htmlEscape(configData.apiUser) + "'/><br/>";
  html += "Password: <input type='password' name='apiPass' value='" + htmlEscape(configData.apiPass) + "'/><br/>";
  html += "DeviceName: <input type='text' name='devName' value='" + htmlEscape(configData.deviceName) + "'/><br/>";

  // Submit
  html += "<br/><input type='submit' value='Save'/></form>";
  html += "<p><a href='/'>Back</a></p>";
  html += "</body></html>";

  webServer.send(200, "text/html", html);
}

void handleSaveConfig() {
  // Read values
  if (webServer.hasArg("wifiSSID"))   configData.wifiSSID   = webServer.arg("wifiSSID");
  if (webServer.hasArg("wifiPass"))   configData.wifiPass   = webServer.arg("wifiPass");
  if (webServer.hasArg("apiUser"))    configData.apiUser    = webServer.arg("apiUser");
  if (webServer.hasArg("apiPass"))    configData.apiPass    = webServer.arg("apiPass");
  if (webServer.hasArg("devName"))    configData.deviceName = webServer.arg("devName");
  
  // Save
  saveConfig();

  // Confirmation
  String html = "<html><body><h1>Saved!</h1>";
  html += "<p><a href='/'>Back</a></p></body></html>";
  webServer.send(200, "text/html", html);
}

/************************************************
 * LOGIN (HTTPS) -> JWT
 ***********************************************/
String getJwtToken() {
  if (!WiFi.isConnected()) return "";
  
  Serial.println("[API] Fetching JWT from: " + String(LOGIN_URL));
  
  WiFiClientSecure client;
  client.setCACert(rootCACert);

  HTTPClient http;
  if (!http.begin(client, LOGIN_URL)) {
    Serial.println("[API] http.begin() failed!");
    return "";
  }
  
  // JSON body (username, password, deviceName)
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
    Serial.println("[API] Response: " + resp);

    StaticJsonDocument<512> respDoc;
    DeserializationError err = deserializeJson(respDoc, resp);
    if (!err) {
      const char* token = respDoc["token"];
      if (token) {
        Serial.println("[API] JWT received");
        return String(token);
      }
    }
  } else {
    Serial.printf("[API] Login failed, Code=%d\n", httpCode);
    http.end();
  }
  return "";
}

/************************************************
 * WIREGUARD CONFIG -> GET /clients/name/<deviceName>/config
 ***********************************************/
bool fetchWireGuardConfig(const String &jwt) {
  if (!WiFi.isConnected()) return false;
  
  // Hardcoded URL
  String url = String(WIREGUARD_URL_PT1) + configData.deviceName + String(WIREGUARD_URL_PT2);
  Serial.println("[WG] Fetching from: " + url);

  WiFiClientSecure client;
  client.setCACert(rootCACert);

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("[WG] http.begin() failed!");
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

    // Parse JSON
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (!err) {
      // Transfer fields
      configData.privateKey   = doc["private_key"]   | "";
      configData.address      = doc["address"]       | "";
      configData.dns          = doc["dns"]           | "";
      configData.endpoint     = doc["endpoint"]      | "";
      configData.publicKey    = doc["public_key"]    | "";
      configData.presharedKey = doc["preshared_key"] | "";

      // Save to NVS
      saveConfig();
      return true;
    } else {
      Serial.println("[WG] JSON parse error!");
    }
  } else {
    Serial.printf("[WG] GET failed, Code=%d\n", httpCode);
    http.end();
  }
  return false;
}

/************************************************
 * WIREGUARD START
 ***********************************************/
bool startWireGuard() {
  if (configData.privateKey.isEmpty() || configData.publicKey.isEmpty()) {
    Serial.println("[WG] Private/Public Key is missing!");
    return false;
  }

  WGConfig c;
  c.private_key   = configData.privateKey;
  c.address       = configData.address;
  c.dns           = configData.dns;
  c.endpoint      = configData.endpoint;
  c.public_key    = configData.publicKey;
  c.preshared_key = configData.presharedKey;

  // c.allowedIPs.push_back("0.0.0.0/0"); // Optional if all traffic should be routed
  // c.keep_alive = 25;

  if (wg.begin(c)) {
    Serial.println("[WG] WireGuard started!");
    return true;
  } else {
    Serial.println("[WG] Failed to start WireGuard.");
    return false;
  }
}

/************************************************
 * SETUP
 ***********************************************/
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Load configuration
  loadConfig();

  // Connect Wi-Fi
  bool wifiOk = connectToWiFi(configData.wifiSSID, configData.wifiPass);
  if (!wifiOk) {
    // If Wi-Fi fails, start AP
    Serial.println("[WIFI] Starting AP mode...");
    WiFi.softAP("ESP32_Config", "12345678");
    Serial.println("[WIFI] AP-IP: " + WiFi.softAPIP().toString());
  } else {
    // Optionally, get time via NTP for TLS validation
    // e.g. configTime(0, 0, "pool.ntp.org"); delay(2000);

    // Fetch JWT, then WG config, then start WG
    String token = getJwtToken();
    bool ok = fetchWireGuardConfig(token);
    if (ok) {
      startWireGuard();
    }
  }

  // Web server endpoints
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/config", HTTP_GET, handleConfigPage);
  webServer.on("/saveConfig", HTTP_POST, handleSaveConfig);

  webServer.onNotFound([](){
    webServer.send(404, "text/plain", "Not found");
  });

  // Start web server
  webServer.begin();
  Serial.println("[WEB] Server started (Port 80)");
}

/************************************************
 * LOOP
 ***********************************************/
void loop() {
  // Process web server
  webServer.handleClient();
  
  // You could poll WireGuard status here, etc.
  delay(10);
}
