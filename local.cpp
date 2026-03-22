#include <Wire.h>
#include <BH1750.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>

/******** PINS ********/
#define RS485_RX 26
#define RS485_TX 25
#define RS485_DE_RE 33
#define PIR_PIN 4
#define RELAY_PIN 5
#define LDR_PIN 34
#define VOLTAGE_PIN 35
#define GPS_RX 16
#define GPS_TX 17

/******** SERIAL ********/
HardwareSerial RS485Serial(1);
HardwareSerial GPSSerial(2);

/******** OBJECTS ********/
TinyGPSPlus gps;
BH1750 lightMeter;

/******** CONFIG ********/
#define JSON_CAP 192
#define LOOP_MS 5000UL

// ── WiFi fallback ──────────────────────────────────────────────────────────────
// 1 = WiFi fallback active (RS485 module damaged)
// 0 = RS485 only, no WiFi code compiled at all
#define USE_WIFI_FALLBACK 1

#if USE_WIFI_FALLBACK
const char *WIFI_SSID = "Prakash's S23";
const char *WIFI_PASS = "Prakash123";
const uint16_t MASTER_PORT = 8080;
const uint16_t DISC_PORT = 4444; // UDP discovery port (same on master)
const char *DISC_MSG = "LUMINA_DISCOVER";
const char *DISC_ACK = "LUMINA_MASTER:";

WiFiUDP udp;
char masterIP[24] = {0};
bool masterFound = false;

// Broadcast a discovery ping; master replies "LUMINA_MASTER:<ip>"
// Retries up to `retries` times before giving up.
void discoverMaster(uint8_t retries = 3)
{
    for (uint8_t attempt = 1; attempt <= retries && !masterFound; attempt++)
    {
        Serial.printf("[Discovery] Attempt %d/%d\n", attempt, retries);
        udp.begin(DISC_PORT + 1);

        udp.beginPacket(IPAddress(255, 255, 255, 255), DISC_PORT);
        udp.print(DISC_MSG);
        udp.endPacket();

        unsigned long t0 = millis();
        while (millis() - t0 < 3000 && !masterFound)
        {
            int len = udp.parsePacket();
            if (len > 0)
            {
                char reply[64] = {0};
                udp.read(reply, sizeof(reply) - 1);
                if (strncmp(reply, DISC_ACK, strlen(DISC_ACK)) == 0)
                {
                    strlcpy(masterIP, reply + strlen(DISC_ACK), sizeof(masterIP));
                    masterFound = true;
                    Serial.printf("[Discovery] Master at %s\n", masterIP);
                }
            }
            delay(50);
        }
        udp.stop();
        if (!masterFound && attempt < retries)
            delay(1000);
    }
    if (!masterFound)
        Serial.println("[Discovery] Master not found — WiFi fallback disabled");
}

// HTTP POST sensor JSON to master. Re-discovers if master IP goes stale.
bool sendWiFi(const char *buf)
{
    if (WiFi.status() != WL_CONNECTED)
        return false;
    if (!masterFound)
    {
        discoverMaster();
        if (!masterFound)
            return false;
    }

    char url[64];
    snprintf(url, sizeof(url), "http://%s:%d/node", masterIP, MASTER_PORT);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(4000);
    int code = http.POST((uint8_t *)buf, strlen(buf));
    http.end();

    if (code == 200)
    {
        Serial.println("[WiFi] Sent OK");
        return true;
    }
    // HTTP failure — master may have gotten a new IP, force re-discovery next cycle
    Serial.printf("[WiFi] Failed: %d — will re-discover\n", code);
    masterFound = false;
    return false;
}
#endif // USE_WIFI_FALLBACK
// ──────────────────────────────────────────────────────────────────────────────

/******** FUNCTIONS ********/
float readVoltage()
{
    return analogRead(VOLTAGE_PIN) * (3.3f / 4095.0f) * 5.0f;
}

float readCurrent(float v)
{
    // Dummy current — original sensor burnt
    float variation = random(0, 21) / 1000.0f;
    float current = (0.50f + variation) * (v / 12.0f);
    return (current < 0) ? 0 : current;
}

void sendRS485(const char *buf, size_t len)
{
    digitalWrite(RS485_DE_RE, HIGH);
    delayMicroseconds(200);
    RS485Serial.write((uint8_t *)buf, len);
    RS485Serial.write('\n');
    RS485Serial.flush();
    digitalWrite(RS485_DE_RE, LOW);
}

/******** SETUP ********/
void setup()
{
    Serial.begin(115200);

    pinMode(PIR_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(RS485_DE_RE, OUTPUT);

    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(RS485_DE_RE, LOW);

    RS485Serial.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
    GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

    Wire.begin(21, 22);
    lightMeter.begin();

#if USE_WIFI_FALLBACK
    Serial.printf("[WiFi] Connecting to %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long wt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wt < 10000)
    {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("\n[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
        discoverMaster(); // find master automatically — no IP config needed
    }
    else
    {
        Serial.println("\n[WiFi] Failed — RS485 only");
    }
#endif

    Serial.println("Local Node Ready");
    delay(2500); // offset from master cycle
}

/******** LOOP ********/
void loop()
{
    unsigned long t0 = millis();

    while (GPSSerial.available())
        gps.encode(GPSSerial.read());

    float lux = lightMeter.readLightLevel();
    int pir = digitalRead(PIR_PIN);
    int ldr = (analogRead(LDR_PIN) > 1000) ? 0 : 1;
    float voltage = readVoltage();
    float current = readCurrent(voltage);

    // Relay: ON when dark (ldr=0), OFF when bright (ldr=1)
    digitalWrite(RELAY_PIN, ldr ? LOW : HIGH);

    // Build JSON payload
    StaticJsonDocument<JSON_CAP> doc;
    static uint16_t id = 0;
    doc["i"] = id++;
    doc["l"] = (int)lux;
    doc["p"] = pir;
    doc["d"] = ldr;
    doc["v"] = serialized(String(voltage, 2));
    doc["c"] = serialized(String(current, 3));

    if (gps.location.isValid() && gps.location.age() < 2000)
    {
        doc["la"] = serialized(String(gps.location.lat(), 6));
        doc["lo"] = serialized(String(gps.location.lng(), 6));
    }
    else
    {
        doc["g"] = 0;
    }

    char buf[JSON_CAP];
    serializeJson(doc, buf);
    Serial.println(buf);

    // Primary: RS485
    sendRS485(buf, strlen(buf));

#if USE_WIFI_FALLBACK
    // Fallback: WiFi HTTP to master (auto-discovered IP, no config needed)
    sendWiFi(buf);
#endif

    // Non-blocking remainder of 5s cycle
    unsigned long elapsed = millis() - t0;
    if (elapsed < LOOP_MS)
        delay(LOOP_MS - elapsed);
}
