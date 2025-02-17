# ESP32: Webinterface + WireGuard + MQTT + TLS

## English Version

### Overview

This project provides an **example Sketch** for the ESP32 combining the following functionalities:

1. **Web interface (Port 80)**  
   - Configures Wi-Fi (SSID/Password)  
   - Configures **WireGuard** parameters (API, Keys, Endpoint)  
   - Sets **GPIO pins** (lamp & sensor)  
   - Configures **MQTT** (broker, user/password, TLS port)  
   - Defines individual **MQTT topics** for lamp commands and sensor status  

2. **Persistent Storage** in **Preferences (NVS)**  
   - All settings survive a restart  

3. **TLS Support**  
   - For connecting to the WireGuard/API server (HTTPS)  
   - For connecting to the MQTT broker (TLS)  
   - A **Root CA certificate** (example: “GTS Root R4”) is included in the source code  

4. **NTP Time Synchronization**  
   - Ensures proper TLS certificate validation within correct time frames  

5. **WireGuard VPN** (using [WireGuard-ESP32-Arduino](https://github.com/ciniml/WireGuard-ESP32-Arduino))

6. **MQTT** (using [PubSubClient](https://github.com/knolleary/pubsubclient))  
   - Receives lamp commands (“on”, “off”) on a configurable **topic**  
   - Sends sensor status (e.g., “HIGH”, “LOW”) on another **topic**

---

### Requirements

- **Arduino IDE** or PlatformIO with **ESP32 board support**  
- Additional libraries (install via Arduino Library Manager):  
  - **ArduinoJson**  
  - **PubSubClient** (by Nick O'Leary)  
  - **WireGuard-ESP32** ([GitHub](https://github.com/ciniml/WireGuard-ESP32-Arduino))  

---

### Project Files

- **`ESP32_VPN_MQTT.ino`**  
  The entire example sketch (C++/Arduino code). You can rename it, but keep the `.ino` extension so it opens in the Arduino IDE.

- **`readme.md`**  
  This README file (in both English and German).

---

### Installation & Usage

1. **Sketch Upload**  
   1. Open the project in the Arduino IDE.  
   2. Under “Tools,” select your ESP32 board (e.g., “ESP32 Dev Module”).  
   3. Compile and upload the sketch to your ESP32.

2. **Initial Start & Access Point**  
   - If the ESP32 cannot connect to the stored Wi-Fi (e.g., empty SSID), it starts in **Access Point mode**:  
     - SSID: `ESP32_Config`  
     - Password: `12345678`  
   - Connect to this Wi-Fi network, then open `http://192.168.4.1/` in your browser.

3. **Configuration via Web Interface**  
   - The main page (`/`) shows current status (Wi-Fi, WireGuard, MQTT, GPIO).  
   - Click “Configure” (`/config`) to open a form for all settings:  
     - **Wi-Fi** (SSID & Password)  
     - **API/WireGuard** (Base URL, Login Endpoint, Username/Password, Client Name, etc.)  
     - **GPIO** (lamp pin, sensor pin)  
     - **MQTT** (broker host, port, user, password, lamp command topic, sensor status topic)  
   - After clicking “Save” (`/saveConfig`), the ESP32 writes all data to **Preferences** (NVS).

4. **Wi-Fi Connection**  
   - After saving, you can restart the ESP32 or let it continue running (depending on your code).  
   - Once Wi-Fi is successful, the device sets the **NTP time** (important for TLS) and attempts to:  
     1. **Fetch** the WireGuard config via HTTPS.  
     2. **Start** WireGuard (if keys are valid).  
     3. **Connect** to the MQTT broker (using TLS on port 8883 or whichever you configured).

5. **MQTT Functionality**  
   - **Lamp Command Topic**: The ESP32 subscribes to incoming commands like `"on"` or `"off"`. It switches the lamp accordingly.  
   - **Sensor Status Topic**: When the sensor pin changes state, the ESP32 publishes its new state (`"HIGH"` or `"LOW"`) to the configured topic.

---

### Customization

1. **Root CA Certificate**  
   - The sketch includes a sample certificate (GTS Root R4). If your server/broker uses a different certificate, replace it with the appropriate PEM.

2. **API Structure**  
   - If your WireGuard server expects different JSON fields or endpoints, adjust `fetchWireGuardConfig()` in the sketch.

3. **MQTT**  
   - You can add more topics at will. Just extend the `Config` structure, the web interface, and the MQTT callback logic.

4. **Security**  
   - This demo uses an **unencrypted** web interface on port 80. For production, consider additional measures (e.g., HTTPS for the web interface, IP filtering).  
   - For TLS connections, the example checks if the server certificate is signed by the provided Root CA and is within a valid time range. You can implement further hostname checks or certificate pinning if needed.

---

### FAQ

**1. Why does the ESP32 need the correct time (NTP)?**  
Without a valid time, the TLS certificate’s “not before” / “not after” fields cannot be correctly verified.

**2. Can I use `client.setInsecure()` instead of a CA certificate?**  
Technically yes, but then you lose authenticity of the connection and are vulnerable to Man-in-the-Middle attacks. Not recommended for production.

**3. Which pins can I use for lamp & sensor?**  
Not all ESP32 pins are created equal. Refer to ESP32 pin documentation for safe usage.

**4. How do I add more sensors/actuators?**  
Add fields to the `Config` structure, place them in the web interface, and handle them in the code. The same principle applies for additional MQTT topics.

---

### License / Disclaimer

- **Demo code**: This is an example. Use at your own risk.  
- **Libraries**: Follow the individual licenses (ArduinoJson, PubSubClient, WireGuard-ESP32, etc.).  
- Any domain or server names (like `vpn23.com`) are placeholders.

---

## Deutsche Version

### Übersicht

Dieses Projekt liefert einen **Beispiel-Sketch** für den ESP32, der folgende Funktionen kombiniert:

1. **Webinterface (Port 80)**  
   - Konfiguration von WLAN (SSID/Passwort)  
   - Konfiguration der **WireGuard**-Parameter (API, Keys, Endpoint)  
   - Einstellung der **GPIO-Pins** (Lampe & Sensor)  
   - Konfiguration von **MQTT** (Broker, Benutzer/Passwort, TLS-Port)  
   - **MQTT-Themen** für Lampen-Befehle und Sensor-Status  

2. **Dauerhafte Speicherung** in **Preferences (NVS)**  
   - Alle Einstellungswerte bleiben auch nach Neustart erhalten

3. **TLS-Unterstützung**  
   - Für die Verbindung zum WireGuard-/API-Server (HTTPS)  
   - Für die Verbindung zum MQTT-Broker (TLS)  
   - Ein **Root-CA-Zertifikat** (Beispiel: „GTS Root R4“) ist im Code enthalten

4. **NTP-Zeitabgleich**  
   - Damit die Gültigkeit der TLS-Zertifikate korrekt geprüft werden kann

5. **WireGuard-VPN** (mit [WireGuard-ESP32-Arduino](https://github.com/ciniml/WireGuard-ESP32-Arduino))

6. **MQTT** (via [PubSubClient](https://github.com/knolleary/pubsubclient))  
   - Empfängt Lampen-Kommandos („on“, „off“) auf einem konfigurierbaren **Topic**  
   - Sendet den Sensor-Status (z.B. „HIGH“, „LOW“) auf einem anderen **Topic**

---

### Voraussetzungen

- **Arduino IDE** oder PlatformIO mit **ESP32-Support**  
- Zusätzliche Libraries (Arduino-Bibliotheksverwalter):  
  - **ArduinoJson**  
  - **PubSubClient** (von Nick O'Leary)  
  - **WireGuard-ESP32** ([GitHub](https://github.com/ciniml/WireGuard-ESP32-Arduino))

---

### Projektdateien

- **`ESP32_VPN_MQTT.ino`**  
  Enthält den gesamten Beispielcode (C++/Arduino). Der Name ist beliebig, die Endung `.ino` sollte jedoch beibehalten werden.

- **`readme.md`**  
  Diese README-Datei (zweisprachig).

---

### Installation & Verwendung

1. **Sketch hochladen**  
   1. Öffne das Projekt in der Arduino IDE.  
   2. Wähle unter „Werkzeuge“ dein ESP32-Board (z.B. „ESP32 Dev Module“) aus.  
   3. Kompiliere und lade den Sketch auf den ESP32 hoch.

2. **Erster Start & Access Point**  
   - Kann sich der ESP32 nicht mit den gespeicherten WLAN-Daten verbinden (leere SSID o.Ä.), startet er im **AP-Modus**:  
     - SSID: `ESP32_Config`  
     - Passwort: `12345678`  
   - Verbinde dich mit diesem WLAN und öffne im Browser `http://192.168.4.1/`.

3. **Konfiguration über das Webinterface**  
   - Auf der Startseite (`/`) siehst du den Status (WLAN, WireGuard, MQTT, GPIO).  
   - Über „Konfiguration ändern“ (`/config`) öffnet sich ein Formular für alle Einstellungen:  
     - **WLAN** (SSID, Passwort)  
     - **API/WireGuard** (Base URL, Login Endpoint, Username/Passwort, Client Name usw.)  
     - **GPIO** (Lampe-Pin, Sensor-Pin)  
     - **MQTT** (Broker-Host, Port, Benutzer, Passwort, Topics)  
   - Nach dem Klick auf „Speichern“ (`/saveConfig`) speichert der ESP32 die Daten in den **Preferences** (NVS).

4. **WLAN-Verbindung**  
   - Nach dem Speichern kannst du den ESP32 neu starten oder (je nach Code) sofort weiterlaufen lassen.  
   - **Funktioniert** das WLAN, holt sich der ESP32 über **NTP** die aktuelle Uhrzeit (wichtig für TLS) und versucht:  
     1. Per **HTTPS** die WireGuard-Konfiguration zu laden.  
     2. **WireGuard** zu starten (sofern Keys gültig sind).  
     3. **MQTT**-Verbindung zum Broker herzustellen (TLS, oft Port 8883).

5. **MQTT-Funktionen**  
   - **Lampencmd-Topic**: Der ESP32 abonniert Kommandos wie `"on"` oder `"off"`. Damit schaltet er die Lampe.  
   - **Sensor-Status-Topic**: Ändert sich der Sensorzustand, veröffentlicht der ESP32 (publish) `“HIGH”` oder `“LOW”` auf dem konfigurierten Topic.

---

### Anpassungen

1. **Root-CA-Zertifikat**  
   - Standardmäßig wird „GTS Root R4“ verwendet. Wenn dein Server/Broker ein anderes Zertifikat nutzt, tausche den PEM-Block im Code aus.

2. **API-Struktur**  
   - Falls dein WireGuard-Server andere JSON-Felder oder Endpunkte hat, passe `fetchWireGuardConfig()` an.

3. **MQTT**  
   - Du kannst weitere Topics hinzufügen, indem du die `Config`-Struktur und den Code (z.B. `mqttCallback()`) erweiterst.

4. **Sicherheit**  
   - Das Webinterface ist unverschlüsselt (Port 80). Für den Produktionseinsatz sind zusätzliche Sicherheitsmaßnahmen (HTTPS, Passwortschutz etc.) empfehlenswert.  
   - Die TLS-Verbindung wird überprüft, indem das Server-Zertifikat gegen das Root-CA validiert wird und die Zeit per NTP gesetzt ist. Optional kannst du noch Hostnamen prüfen oder Zertifikats-Pinning einbauen.

---

### FAQ

**1. Warum braucht der ESP32 die korrekte Uhrzeit?**  
Ohne aktuelle Uhrzeit kann nicht geprüft werden, ob das TLS-Zertifikat abgelaufen oder noch nicht gültig ist.

**2. Kann ich `client.setInsecure()` statt CA-Zertifikat nutzen?**  
Das ist zwar möglich, aber unsicher (kein Schutz vor Man-in-the-Middle-Angriffen). Für den Produktivbetrieb nicht zu empfehlen.

**3. Welche Pins darf ich für Lampe und Sensor verwenden?**  
Informiere dich über die geeigneten Pins auf dem ESP32. Manche Pins sind eingeschränkt oder nur zum Booten geeignet.

**4. Wie füge ich weitere Sensoren/Aktoren hinzu?**  
Ergänze in der `Config`-Struktur neue Felder, binde diese ins Webinterface ein und erweitere den Code. Das gleiche gilt für zusätzliche MQTT-Themen.

---

### Lizenz / Rechtliches

- **Demo-Code**: Dieser Sketch dient als Beispiel, Nutzung auf eigenes Risiko.  
- **Libraries**: Beachte die jeweiligen Lizenzen (ArduinoJson, PubSubClient, WireGuard-ESP32, etc.).  
- Eventuelle Domain- oder Servernamen (z.B. `vpn23.com`) sind nur Platzhalter.
