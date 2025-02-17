# ESP32 WireGuard Minimal Setup for **vpn23.com**

> **English version below**  
> **Deutsche Version weiter unten**

---

## Deutsche Version

### Übersicht

Dieses Projekt demonstriert einen **vereinfachten Sketch** für den ESP32, der mit dem VPN-Dienst **vpn23.com** zusammenarbeitet.  
Nur wenige Daten sind konfigurierbar:

- **WLAN**: SSID und Passwort  
- **API**: Benutzername (Username), Passwort, DeviceName  

Alle anderen Angaben (URLs, Endpunkte) sind **fest** im Sketch hinterlegt:

- **Login**: `https://vpn23.com/login`  
- **WireGuard-Konfiguration**: `https://vpn23.com/clients/name/<DeviceName>/config`

### Funktionsweise

1. **Speicherung** der WLAN-Daten und API-Credentials (Username, Passwort, DeviceName) in **Preferences (NVS)**.  
2. Der ESP32 versucht, sich ins WLAN einzuwählen.  
   - **Fehlschlag**: Startet einen **Access Point** `ESP32_Config` (Passwort `12345678`).  
     - Dann kannst du dich mit dem AP verbinden und über `http://192.168.4.1/` das Webinterface aufrufen, um WLAN und API-Daten einzugeben.  
3. **Erfolgreiche WLAN-Verbindung**  
   - Der Sketch führt einen **HTTPS-Login** bei `https://vpn23.com/login` durch (POST mit `{"username", "password", "deviceName"}`) und erhält ein **JWT**.  
   - Anschließend wird `GET https://vpn23.com/clients/name/<DeviceName>/config` aufgerufen (Header: `Authorization: Bearer <JWT>`), um die **WireGuard-Daten** (Keys, Endpoint etc.) zu erhalten.  
   - Der Sketch speichert diese Daten und startet **WireGuard** per [WireGuard-ESP32-Library](https://github.com/ciniml/WireGuard-ESP32-Arduino).  
4. **TLS-Root-CA**  
   - Ein Beispiel-Root-CA ist im Code hinterlegt (z.B. „GTS Root R4“). In einer echten Umgebung solltest du dein eigenes CA-Zertifikat integrieren oder die Zeit per NTP setzen, damit die Zertifikatsvalidierung korrekt funktioniert.

### Voraussetzungen

- **Arduino IDE** (oder PlatformIO) mit **ESP32-Support**  
- Zusätzliche Libraries:  
  - **ArduinoJson**  
  - **WireGuard-ESP32**  
- Optional: **NTP**, um die Uhrzeit für echte TLS-Prüfungen zu aktualisieren.

### Sketch-Datei

- **`MinimalVPN23.ino`** (Beispielname)  
  Enthält den gesamten Beispielcode. Der Name ist frei wählbar, `.ino` für Arduino IDE empfohlen.

### Nutzung

1. **Installation**  
   - Öffne den Sketch in der Arduino IDE, wähle dein ESP32-Board unter „Werkzeuge“ und lade den Code hoch.

2. **Erster Start**  
   - Kann sich der ESP32 nicht mit den gespeicherten WLAN-Daten verbinden, öffnet er einen Access Point `ESP32_Config` (PW: `12345678`).  
   - Verbinde dich mit dem AP, rufe `http://192.168.4.1/` auf und gib WLAN-SSID/Passwort sowie Username, Passwort, DeviceName für **vpn23.com** an. Klicke „Speichern“.

3. **VPN-Aktivierung**  
   - Nach erfolgreicher WLAN-Verbindung holt der Sketch ein **JWT** von `https://vpn23.com/login`, anschließend die WG-Konfiguration von `https://vpn23.com/clients/name/<DeviceName>/config`.  
   - Startet WireGuard, sofern alle Daten korrekt sind.

4. **Anpassungen**  
   - Wenn du andere URLs oder Zertifikate brauchst, ändere sie direkt im Sketch (hart codierte Konstante).  
   - Für echte TLS-Sicherheit die Zeit per NTP aktualisieren (z.B. `configTime(...)`), damit das Zertifikat von vpn23.com gültig verifiziert wird.

---

## English Version

### Overview

This project demonstrates a **simplified ESP32 sketch** working with the **vpn23.com** VPN service.  
Only a few parameters are configurable:

- **Wi-Fi**: SSID and password  
- **API**: username, password, deviceName  

All other details (URLs/endpoints) are **hardcoded**:

- **Login**: `https://vpn23.com/login`  
- **WireGuard config**: `https://vpn23.com/clients/name/<DeviceName>/config`

### How It Works

1. The ESP32 **stores** Wi-Fi credentials and API credentials (username, password, deviceName) in **Preferences (NVS)**.  
2. It attempts to connect to Wi-Fi:  
   - **If fail**: starts an **Access Point** `ESP32_Config` (password `12345678`).  
     - Connect to that AP and open `http://192.168.4.1/` to enter Wi-Fi and API data, then click “Save.”  
3. **Successful Wi-Fi**  
   - The sketch performs an **HTTPS** login at `https://vpn23.com/login` with the JSON body `{"username", "password", "deviceName"}` to obtain a **JWT**.  
   - Then it calls `GET https://vpn23.com/clients/name/<DeviceName>/config` (with `Authorization: Bearer <JWT>` header) to retrieve the **WireGuard configuration** (keys, endpoint, etc.).  
   - The sketch stores those details and starts **WireGuard** via the [WireGuard-ESP32 Library](https://github.com/ciniml/WireGuard-ESP32-Arduino).  
4. **TLS Root CA**  
   - A sample Root CA (e.g. “GTS Root R4”) is included in the code. For real security, you should provide your own certificate and optionally sync time via NTP to validate certificates properly.

### Requirements

- **Arduino IDE** (or PlatformIO) with **ESP32** support  
- Libraries:  
  - **ArduinoJson**  
  - **WireGuard-ESP32**  
- Optional: **NTP** for accurate time in TLS checks.

### Sketch File

- **`MinimalVPN23.ino`** (example name)  
  Contains all the example code. You can rename it; `.ino` is recommended for Arduino IDE.

### Usage

1. **Installation**  
   - Open the sketch in Arduino IDE, select your ESP32 board under “Tools,” and upload.

2. **Initial Start**  
   - If the ESP32 cannot connect with stored Wi-Fi credentials, it opens an AP named `ESP32_Config` (password `12345678`).  
   - Connect and browse to `http://192.168.4.1/` to configure Wi-Fi SSID/password and username/password/deviceName for **vpn23.com**, then “Save.”

3. **VPN Activation**  
   - Once Wi-Fi is successful, the sketch fetches a **JWT** from `https://vpn23.com/login`, then GETs the WireGuard config from `https://vpn23.com/clients/name/<DeviceName>/config`.  
   - It starts WireGuard if all keys are correct.

4. **Customization**  
   - If you need different URLs or certificates, change the constants in the sketch.  
   - For proper TLS validation, sync time via NTP so that the `vpn23.com` certificate is verified correctly.

---

### License / Disclaimer

- **Demo code**: Use at your own risk.  
- **Libraries**: Respect each library’s license (WireGuard-ESP32, ArduinoJson, etc.).  
- Domain `vpn23.com` is for demonstration.  
