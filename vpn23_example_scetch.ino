/*********************************************************************
 * ESP32-Beispiel mit:
 *  - Webinterface (Port 80) zur Konfiguration von
 *    * WLAN (SSID/Pass)
 *    * API (Base URL, Login, WireGuard-Endpunkt)
 *    * WireGuard-Konfiguration
 *    * GPIOs (Lampe & Sensor)
 *    * MQTT-Parameter (Broker + TLS, User/Pass) + eigene Topics:
 *      - Lampen-Cmd-Topic: Eingehende Kommandos zum Schalten
 *      - Sensor-Status-Topic: Ausgehende Meldungen über Sensorzustand
 *  - TLS-gesicherter HTTPS-Request (WiFiClientSecure + Root-CA)
 *  - NTP-Zeitsynchronisierung (für TLS)
 *  - WireGuard-ESP32 (VPN)
 *  - MQTT (PubSubClient) mit TLS
 *
 *  Bei MQTT-Callback wird geprüft, ob das Topic dem Lampen-Cmd-Topic
 *  entspricht. Dann wird basierend auf Payload "on"/"off" die Lampe geschaltet.
 *  Außerdem wird alle paar Sekunden der Sensorstatus gepublished.
 *
 *  Benötigte Libraries (zusätzlich zu ESP32 Core):
 *    - ArduinoJson
 *    - WireGuard-ESP32 (https://github.com/ciniml/WireGuard-ESP32-Arduino)
 *    - PubSubClient (von Nick O'Leary)
 *********************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WireGuard-ESP32.h>
#include <PubSubClient.h>
#include "time.h" // NTP

// Webserver auf Port 80
WebServer server(80);

// WireGuard
WireGuard wg;

// Preferences (NVS) für Konfiguration
Preferences preferences;

// MQTT: TLS-Client + PubSubClient
WiFiClientSecure mqttNet;
PubSubClient mqttClient(mqttNet);

// -----------------------------------------------------------
// Beispiel-Root-CA (GTS Root R4). Für deinen Broker/Server
// ggf. anderes Zertifikat nötig!
// -----------------------------------------------------------
static const char *rootCACert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX
MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE
CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx
NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT
GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0
MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube
Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e
WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H/MIH8MA4GA1UdDwEB/wQEAwIBhjAd
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

// -----------------------------------------------------------
// Struktur aller Konfigurationswerte
// -----------------------------------------------------------
struct Config {
  // WLAN
  String wifiSSID;
  String wifiPassword;

  // API / WG
  String apiBaseUrl;
  String apiUsername;
  String apiPassword;
  String clientName;
  String loginEndpoint;  

  // WireGuard
  String privateKey;
  String address;
  String dns;
  String endpoint;
  String publicKey;
  String presharedKey;

  // GPIO
  int lampPin;
  int sensorPin;

  // MQTT
  String mqttHost;
  int    mqttPort;
  String mqttUser;
  String mqttPass;

  // Neue Felder: Topics für Lampen-Cmd & Sensor-State
  String mqttLampCmdTopic;      // z.B. "/home/esp32/lamp/cmd"
  String mqttSensorStateTopic;  // z.B. "/home/esp32/sensor/state"
};

Config configData;
bool wireguardRunning = false;
bool mqttConnected    = false;

// Für Sensor-State
int lastSensorState   = -1;  // Merkt sich letzten Status, um nur bei Änderung zu publishen

/************************************************
 * NVS LADEN / SPEICHERN
 ***********************************************/
void loadConfig() {
  preferences.begin("app-config", true);
  configData.wifiSSID             = preferences.getString("wifiSSID", "");
  configData.wifiPassword         = preferences.getString("wifiPass", "");

  configData.apiBaseUrl           = preferences.getString("apiBaseUrl", "https://vpn23.com");
  configData.apiUsername          = preferences.getString("apiUser", "");
  configData.apiPassword          = preferences.getString("apiPass", "");
  configData.clientName           = preferences.getString("clientName", "ESP32_Client");
  configData.loginEndpoint        = preferences.getString("loginEp", "/auth/login");

  configData.privateKey           = preferences.getString("privKey", "");
  configData.address              = preferences.getString("address", "");
  configData.dns                  = preferences.getString("dns", "");
  configData.endpoint             = preferences.getString("endpoint", "");
  configData.publicKey            = preferences.getString("pubKey", "");
  configData.presharedKey         = preferences.getString("psk", "");

  configData.lampPin              = preferences.getInt("lampPin", 2);
  configData.sensorPin            = preferences.getInt("sensorPin", 4);

  configData.mqttHost            = preferences.getString("mqttHost", "");
  configData.mqttPort            = preferences.getInt("mqttPort", 8883);
  configData.mqttUser            = preferences.getString("mqttUser", "");
  configData.mqttPass            = preferences.getString("mqttPass", "");
  configData.mqttLampCmdTopic    = preferences.getString("mqttLampCmdT", "/esp32/lamp/cmd");
  configData.mqttSensorStateTopic= preferences.getString("mqttSensStT", "/esp32/sensor/state");

  preferences.end();
}

void saveConfig() {
  preferences.begin("app-config", false);
  preferences.putString("wifiSSID",      configData.wifiSSID);
  preferences.putString("wifiPass",      configData.wifiPassword);

  preferences.putString("apiBaseUrl",    configData.apiBaseUrl);
  preferences.putString("apiUser",       configData.apiUsername);
  preferences.putString("apiPass",       configData.apiPassword);
  preferences.putString("clientName",    configData.clientName);
  preferences.putString("loginEp",       configData.loginEndpoint);

  preferences.putString("privKey",       configData.privateKey);
  preferences.putString("address",       configData.address);
  preferences.putString("dns",           configData.dns);
  preferences.putString("endpoint",      configData.endpoint);
  preferences.putString("pubKey",        configData.publicKey);
  preferences.putString("psk",           configData.presharedKey);

  preferences.putInt("lampPin",          configData.lampPin);
  preferences.putInt("sensorPin",        configData.sensorPin);

  preferences.putString("mqttHost",      configData.mqttHost);
  preferences.putInt("mqttPort",         configData.mqttPort);
  preferences.putString("mqttUser",      configData.mqttUser);
  preferences.putString("mqttPass",      configData.mqttPass);
  preferences.putString("mqttLampCmdT",  configData.mqttLampCmdTopic);
  preferences.putString("mqttSensStT",   configData.mqttSensorStateTopic);

  preferences.end();
}

/************************************************
 * WLAN
 ***********************************************/
bool connectToWiFi(const String &ssid, const String &pass) {
  if (ssid.isEmpty() || pass.isEmpty()) {
    Serial.println("[WIFI] SSID/Passwort leer");
    return false;
  }
  Serial.println("[WIFI] Verbinde mit " + ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long start = millis();
  const unsigned long timeout = 15000;

  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Verbunden! IP: " + WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("\n[WIFI] Timeout - kein Connect.");
    return false;
  }
}

/************************************************
 * WEBINTERFACE
 ***********************************************/
#include <WebServer.h>

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

// Hauptseite
void handleRoot() {
  String html = "<html><head><title>ESP32 Config (MQTT + TLS)</title></head><body>";
  html += "<h1>ESP32 Config & Status (MQTT + TLS)</h1>";

  // WLAN
  html += "<p><b>WLAN:</b> ";
  if (WiFi.isConnected()) {
    html += "Verbunden mit " + htmlEscape(WiFi.SSID()) + " (IP: " + WiFi.localIP().toString() + ")";
  } else {
    html += "<span style='color:red;'>Nicht verbunden</span>";
  }
  html += "</p>";

  // WireGuard
  html += "<p><b>WireGuard:</b> " + String(wireguardRunning ? "aktiv" : "inaktiv") + "</p>";

  // MQTT
  html += "<p><b>MQTT:</b> " + String(mqttConnected ? "connected" : "disconnected") + "</p>";

  // GPIO
  int lampState = digitalRead(configData.lampPin);
  int sensorState = digitalRead(configData.sensorPin);
  html += "<h3>GPIO</h3>";
  html += "<p>Lampe-Pin: " + String(configData.lampPin) + " (" + (lampState == HIGH ? "AN" : "AUS") + ")</p>";
  html += "<p>Sensor-Pin: " + String(configData.sensorPin) + " (" + (sensorState == HIGH ? "HIGH" : "LOW") + ")</p>";
  
  // Lampe togglen
  html += "<form method='POST' action='/toggleLamp'><button type='submit'>Lampe Umschalten</button></form>";

  // Link zu /config
  html += "<p><a href='/config'>Konfiguration ändern</a></p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Lampe togglen
void handleToggleLamp() {
  int state = digitalRead(configData.lampPin);
  digitalWrite(configData.lampPin, (state == HIGH) ? LOW : HIGH);

  // Redirect
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Redirecting...");
}

// Config-Seite
void handleConfigPage() {
  String html = "<html><head><title>ESP32 Config</title></head><body>";
  html += "<h1>Konfiguration</h1><form method='POST' action='/saveConfig'>";

  // WLAN
  html += "<h3>WLAN</h3>";
  html += "SSID: <input type='text' name='wifiSSID' value='" + htmlEscape(configData.wifiSSID) + "'/><br/>";
  html += "Pass: <input type='password' name='wifiPass' value='" + htmlEscape(configData.wifiPassword) + "'/><br/>";

  // API/WG
  html += "<h3>API/WireGuard</h3>";
  html += "Base URL: <input type='text' name='apiBaseUrl' value='" + htmlEscape(configData.apiBaseUrl) + "'/><br/>";
  html += "Login Endpoint: <input type='text' name='loginEndpoint' value='" + htmlEscape(configData.loginEndpoint) + "'/><br/>";
  html += "API User: <input type='text' name='apiUser' value='" + htmlEscape(configData.apiUsername) + "'/><br/>";
  html += "API Pass: <input type='password' name='apiPass' value='" + htmlEscape(configData.apiPassword) + "'/><br/>";
  html += "Client Name: <input type='text' name='clientName' value='" + htmlEscape(configData.clientName) + "'/><br/>";

  html += "<h4>WireGuard Keys</h4>";
  html += "Private Key: <input type='text' name='privKey' value='" + htmlEscape(configData.privateKey) + "'/><br/>";
  html += "Address: <input type='text' name='address' value='" + htmlEscape(configData.address) + "'/><br/>";
  html += "DNS: <input type='text' name='dns' value='" + htmlEscape(configData.dns) + "'/><br/>";
  html += "Endpoint: <input type='text' name='endpoint' value='" + htmlEscape(configData.endpoint) + "'/><br/>";
  html += "Public Key: <input type='text' name='pubKey' value='" + htmlEscape(configData.publicKey) + "'/><br/>";
  html += "Preshared Key: <input type='text' name='psk' value='" + htmlEscape(configData.presharedKey) + "'/><br/>";

  // GPIO
  html += "<h3>GPIO</h3>";
  html += "Lampe-Pin: <input type='number' name='lampPin' value='" + String(configData.lampPin) + "'/><br/>";
  html += "Sensor-Pin: <input type='number' name='sensorPin' value='" + String(configData.sensorPin) + "'/><br/>";

  // MQTT
  html += "<h3>MQTT</h3>";
  html += "Host: <input type='text' name='mqttHost' value='" + htmlEscape(configData.mqttHost) + "'/><br/>";
  html += "Port: <input type='number' name='mqttPort' value='" + String(configData.mqttPort) + "'/><br/>";
  html += "User: <input type='text' name='mqttUser' value='" + htmlEscape(configData.mqttUser) + "'/><br/>";
  html += "Pass: <input type='password' name='mqttPass' value='" + htmlEscape(configData.mqttPass) + "'/><br/>";

  // Neue Topics
  html += "<h4>MQTT Topics</h4>";
  html += "Lamp-Cmd-Topic: <input type='text' name='mqttLampCmdTopic' value='" + htmlEscape(configData.mqttLampCmdTopic) + "'/><br/>";
  html += "Sensor-State-Topic: <input type='text' name='mqttSensorStateTopic' value='" + htmlEscape(configData.mqttSensorStateTopic) + "'/><br/>";

  // Submit
  html += "<br/><input type='submit' value='Speichern'/></form>";
  html += "<p><a href='/'>Zurück</a></p></body></html>";

  server.send(200, "text/html", html);
}

// POST: Config speichern
void handleSaveConfig() {
  if (server.hasArg("wifiSSID"))           configData.wifiSSID             = server.arg("wifiSSID");
  if (server.hasArg("wifiPass"))           configData.wifiPassword         = server.arg("wifiPass");
  if (server.hasArg("apiBaseUrl"))         configData.apiBaseUrl           = server.arg("apiBaseUrl");
  if (server.hasArg("loginEndpoint"))      configData.loginEndpoint        = server.arg("loginEndpoint");
  if (server.hasArg("apiUser"))            configData.apiUsername          = server.arg("apiUser");
  if (server.hasArg("apiPass"))            configData.apiPassword          = server.arg("apiPass");
  if (server.hasArg("clientName"))         configData.clientName           = server.arg("clientName");
  if (server.hasArg("privKey"))            configData.privateKey           = server.arg("privKey");
  if (server.hasArg("address"))            configData.address              = server.arg("address");
  if (server.hasArg("dns"))                configData.dns                  = server.arg("dns");
  if (server.hasArg("endpoint"))           configData.endpoint             = server.arg("endpoint");
  if (server.hasArg("pubKey"))             configData.publicKey            = server.arg("pubKey");
  if (server.hasArg("psk"))                configData.presharedKey         = server.arg("psk");

  if (server.hasArg("lampPin"))            configData.lampPin              = server.arg("lampPin").toInt();
  if (server.hasArg("sensorPin"))          configData.sensorPin            = server.arg("sensorPin").toInt();

  if (server.hasArg("mqttHost"))           configData.mqttHost             = server.arg("mqttHost");
  if (server.hasArg("mqttPort"))           configData.mqttPort             = server.arg("mqttPort").toInt();
  if (server.hasArg("mqttUser"))           configData.mqttUser             = server.arg("mqttUser");
  if (server.hasArg("mqttPass"))           configData.mqttPass             = server.arg("mqttPass");
  if (server.hasArg("mqttLampCmdTopic"))   configData.mqttLampCmdTopic     = server.arg("mqttLampCmdTopic");
  if (server.hasArg("mqttSensorStateTopic")) configData.mqttSensorStateTopic = server.arg("mqttSensorStateTopic");

  saveConfig();

  String html = "<html><body><h1>Konfiguration gespeichert!</h1>";
  html += "<p><a href='/'>Zurück zur Startseite</a></p></body></html>";
  server.send(200, "text/html", html);

  // Optional: Neustart
  // ESP.restart();
}

/************************************************
 * JWT (falls benötigt)
 ***********************************************/
String getJwtToken() {
  if (!WiFi.isConnected()) return "";
  if (configData.apiBaseUrl.isEmpty() || configData.loginEndpoint.isEmpty()) {
    return "";
  }

  WiFiClientSecure client;
  client.setCACert(rootCACert);

  HTTPClient http;
  String url = configData.apiBaseUrl + configData.loginEndpoint;
  Serial.println("[API] Login: " + url);
  if (!http.begin(client, url)) {
    Serial.println("[API] http.begin fehlgeschlagen");
    return "";
  }

  StaticJsonDocument<256> doc;
  doc["username"] = configData.apiUsername;
  doc["password"] = configData.apiPassword;
  String body;
  serializeJson(doc, body);

  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  if (code == 200 || code == 201) {
    String resp = http.getString();
    http.end();

    StaticJsonDocument<512> respDoc;
    if (!deserializeJson(respDoc, resp)) {
      const char* token = respDoc["token"];
      if (token) {
        Serial.println("[API] JWT erhalten");
        return String(token);
      }
    }
  } else {
    Serial.printf("[API] Login fehlgeschlagen, code=%d\n", code);
    http.end();
  }
  return "";
}

/************************************************
 * WG-KONFIG LADEN
 ***********************************************/
bool fetchWireGuardConfig(const String &jwt) {
  if (!WiFi.isConnected()) return false;

  String url = configData.apiBaseUrl + "/clients/name/" + configData.clientName + "/config";
  Serial.println("[WG] GET " + url);

  WiFiClientSecure client;
  client.setCACert(rootCACert);

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("[WG] http.begin fehlgeschlagen");
    return false;
  }

  if (!jwt.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + jwt);
  }

  int code = http.GET();
  if (code == 200) {
    String resp = http.getString();
    http.end();

    StaticJsonDocument<1024> doc;
    if (!deserializeJson(doc, resp)) {
      configData.privateKey   = doc["private_key"]   | "";
      configData.address      = doc["address"]       | "";
      configData.dns          = doc["dns"]           | "";
      configData.endpoint     = doc["endpoint"]      | "";
      configData.publicKey    = doc["public_key"]    | "";
      configData.presharedKey = doc["preshared_key"] | "";

      saveConfig();
      Serial.println("[WG] WG-Konfig aktualisiert");
      return true;
    }
  } else {
    Serial.printf("[WG] GET code=%d\n", code);
    http.end();
  }
  return false;
}

/************************************************
 * WG START
 ***********************************************/
bool startWireGuard() {
  if (configData.privateKey.isEmpty() || configData.publicKey.isEmpty()) {
    Serial.println("[WG] Keys fehlen");
    return false;
  }

  WGConfig c;
  c.private_key   = configData.privateKey;
  c.address       = configData.address;
  c.dns           = configData.dns;
  c.endpoint      = configData.endpoint;
  c.public_key    = configData.publicKey;
  c.preshared_key = configData.presharedKey;

  // c.allowedIPs.push_back("0.0.0.0/0");
  // c.keep_alive = 25;

  if (wg.begin(c)) {
    Serial.println("[WG] WireGuard gestartet");
    return true;
  }
  Serial.println("[WG] start failed");
  return false;
}

/************************************************
 * MQTT-CALLBACK
 ***********************************************/
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Baue String aus Payload
  String message;
  for (unsigned int i=0; i<length; i++) {
    message += (char)payload[i];
  }
  Serial.printf("[MQTT] Eingehende Msg. Topic=%s, Msg=%s\n", topic, message.c_str());

  // Lampen-Cmd-Topic?
  if (String(topic) == configData.mqttLampCmdTopic) {
    // Bsp: "on" -> Lampe an, "off" -> Lampe aus
    if (message == "on") {
      digitalWrite(configData.lampPin, HIGH);
      Serial.println("[LAMPE] eingeschaltet (per MQTT)");
    }
    else if (message == "off") {
      digitalWrite(configData.lampPin, LOW);
      Serial.println("[LAMPE] ausgeschaltet (per MQTT)");
    }
    else {
      Serial.println("[LAMPE] Unbekannter Befehl: " + message);
    }
  }
}

/************************************************
 * MQTT CONNECT
 ***********************************************/
bool connectToMQTT() {
  if (configData.mqttHost.isEmpty()) {
    Serial.println("[MQTT] Kein Host definiert, skip");
    return false;
  }

  mqttNet.setCACert(rootCACert);
  mqttClient.setServer(configData.mqttHost.c_str(), configData.mqttPort);
  mqttClient.setCallback(mqttCallback);

  Serial.println("[MQTT] Verbinde zu " + configData.mqttHost + ":" + String(configData.mqttPort));

  // Username/Pass
  if (!configData.mqttUser.isEmpty()) {
    if (mqttClient.connect("ESP32Client", configData.mqttUser.c_str(), configData.mqttPass.c_str())) {
      Serial.println("[MQTT] Verbunden (mit User/PW)");
      return true;
    } else {
      Serial.print("[MQTT] Fehler, rc=");
      Serial.println(mqttClient.state());
      return false;
    }
  } else {
    // ohne Auth
    if (mqttClient.connect("ESP32Client")) {
      Serial.println("[MQTT] Verbunden (ohne Auth)");
      return true;
    } else {
      Serial.print("[MQTT] Fehler, rc=");
      Serial.println(mqttClient.state());
      return false;
    }
  }
}

/************************************************
 * SETUP
 ***********************************************/
void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1) Config laden
  loadConfig();

  // 2) GPIO
  pinMode(configData.lampPin, OUTPUT);
  digitalWrite(configData.lampPin, LOW);
  pinMode(configData.sensorPin, INPUT);

  // 3) WLAN
  bool wifiOk = connectToWiFi(configData.wifiSSID, configData.wifiPassword);
  if (!wifiOk) {
    Serial.println("[WIFI] Starte AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_Config", "12345678");
    Serial.println("[WIFI] AP-IP: " + WiFi.softAPIP().toString());
  } else {
    // 4) NTP
    configTime(0, 0, "pool.ntp.org");
    delay(2000);

    // 5) WireGuard: Token (optional), WG-Config, WG starten
    String token = getJwtToken();
    if (fetchWireGuardConfig(token)) {
      wireguardRunning = startWireGuard();
    }

    // 6) MQTT
    mqttConnected = connectToMQTT();
    if (mqttConnected) {
      mqttClient.subscribe(configData.mqttLampCmdTopic.c_str());
      Serial.println("[MQTT] Subscribed: " + configData.mqttLampCmdTopic);
    }
  }

  // 7) Webserver
  server.on("/", HTTP_GET, handleRoot);
  server.on("/toggleLamp", HTTP_POST, handleToggleLamp);
  server.on("/config", HTTP_GET, handleConfigPage);
  server.on("/saveConfig", HTTP_POST, handleSaveConfig);
  server.onNotFound([](){
    server.send(404, "text/plain", "Not Found");
  });
  server.begin();
  Serial.println("[WEB] Webserver gestartet (Port 80)");
}

/************************************************
 * LOOP
 ***********************************************/
void loop() {
  // Webserver bearbeiten
  server.handleClient();

  // MQTT Loop
  if (mqttClient.connected()) {
    mqttClient.loop();
  } else {
    // Reconnect?
    static unsigned long lastAttempt = 0;
    if (millis() - lastAttempt > 10000) {
      lastAttempt = millis();
      mqttConnected = connectToMQTT();
      if (mqttConnected) {
        mqttClient.subscribe(configData.mqttLampCmdTopic.c_str());
        Serial.println("[MQTT] Re-Subscribed: " + configData.mqttLampCmdTopic);
      }
    }
  }

  // Sensorstatus prüfen und publishen?
  int currentSensor = digitalRead(configData.sensorPin);
  if (currentSensor != lastSensorState) {
    lastSensorState = currentSensor;
    // Nur publishen, wenn wir connected sind
    if (mqttConnected && !configData.mqttSensorStateTopic.isEmpty()) {
      String msg = (currentSensor == HIGH) ? "HIGH" : "LOW";
      mqttClient.publish(configData.mqttSensorStateTopic.c_str(), msg.c_str());
      Serial.println("[SENSOR] Status geändert -> MQTT: " + msg);
    }
  }

  // Ggf. Delay oder Freedrum minimal
  delay(10);
}
