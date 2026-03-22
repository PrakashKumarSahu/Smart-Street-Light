#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <BH1750.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
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
#define LORA_SS 13
#define LORA_RST 14
#define LORA_DIO0 27

/******** SERIAL ********/
HardwareSerial RS485Serial(1);
HardwareSerial GPSSerial(2);

/******** OBJECTS ********/
TinyGPSPlus gps;
BH1750 lightMeter;

/******** CONFIG ********/
#define RS485_BUF 192
#define JSON_CAP 512
#define LOOP_MS 6000UL

// ── WiFi fallback ──────────────────────────────────────────────────────────────
// 1 = WiFi fallback active (RS485 module damaged)
// 0 = RS485 only, no WiFi code compiled at all
#define USE_WIFI_FALLBACK 1

#if USE_WIFI_FALLBACK
const char *WIFI_SSID = "Prakash's S23";
const char *WIFI_PASS = "Prakash123";
const uint16_t HTTP_PORT = 8080;
const uint16_t DISC_PORT = 4444;
const char *DISC_MSG = "LUMINA_DISCOVER";
const char *DISC_ACK = "LUMINA_MASTER:";

WebServer server(HTTP_PORT);
WiFiUDP udp;

// Shared buffer written by HTTP handler, read by main loop
volatile bool wifiNodeReady = false;
char wifiNodeBuf[RS485_BUF] = {0};
portMUX_TYPE nodeMux = portMUX_INITIALIZER_UNLOCKED;

// HTTP POST /node — local node sends its JSON here
void handleNodePost()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "text/plain", "No body");
        return;
    }
    String body = server.arg("plain");
    portENTER_CRITICAL(&nodeMux);
    body.toCharArray(wifiNodeBuf, sizeof(wifiNodeBuf));
    wifiNodeReady = true;
    portEXIT_CRITICAL(&nodeMux);
    server.send(200, "text/plain", "OK");
    Serial.printf("[WiFi Server] Received: %s\n", wifiNodeBuf);
}

// UDP discovery responder — reply with this node's IP whenever pinged
void handleDiscovery()
{
    int len = udp.parsePacket();
    if (len > 0)
    {
        char msg[32] = {0};
        udp.read(msg, sizeof(msg) - 1);
        if (strcmp(msg, DISC_MSG) == 0)
        {
            String reply = String(DISC_ACK) + WiFi.localIP().toString();
            udp.beginPacket(udp.remoteIP(), udp.remotePort());
            udp.print(reply);
            udp.endPacket();
            Serial.printf("[Discovery] Replied to %s\n", udp.remoteIP().toString().c_str());
        }
    }
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

// RS485 receive — also pumps WiFi server and UDP discovery during the wait
bool receiveRS485(char *buf, size_t size, uint16_t timeout_ms = 6000)
{
    size_t idx = 0;
    unsigned long t0 = millis();
    digitalWrite(RS485_DE_RE, LOW);

    while (millis() - t0 < timeout_ms)
    {
#if USE_WIFI_FALLBACK
        server.handleClient();
        handleDiscovery();
#endif
        while (RS485Serial.available())
        {
            char c = RS485Serial.read();
            if (c == '\n')
            {
                buf[idx] = '\0';
                return (idx > 0);
            }
            if (idx < size - 1)
                buf[idx++] = c;
        }
    }
    buf[0] = '\0';
    return false;
}

/******** SETUP ********/
void setup()
{
    Serial.begin(115200);

    pinMode(PIR_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(RS485_DE_RE, OUTPUT);
    digitalWrite(RS485_DE_RE, LOW);

    RS485Serial.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
    GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

    Wire.begin(21, 22);
    lightMeter.begin();

    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433E6))
    {
        Serial.println("LoRa Init Failed");
        while (1)
            ;
    }

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
        Serial.printf("\n[WiFi] Connected. Master IP: %s\n", WiFi.localIP().toString().c_str());

        // HTTP server for receiving local node data
        server.on("/node", HTTP_POST, handleNodePost);
        server.begin();
        Serial.printf("[WiFi] HTTP server on port %d\n", HTTP_PORT);

        // UDP for auto-discovery (local node finds this IP automatically)
        udp.begin(DISC_PORT);
        Serial.printf("[Discovery] Listening on UDP port %d\n", DISC_PORT);
    }
    else
    {
        Serial.println("\n[WiFi] Failed — RS485 only");
    }
#endif

    Serial.println("Master Node Ready");
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

    // Relay: ON when dark OR motion detected
    digitalWrite(RELAY_PIN, (ldr || pir) ? LOW : HIGH);

    // Build master sub-document
    StaticJsonDocument<256> mDoc;
    mDoc["l"] = (int)lux;
    mDoc["p"] = pir;
    mDoc["d"] = ldr;
    mDoc["v"] = serialized(String(voltage, 2));
    mDoc["c"] = serialized(String(current, 3));

    if (gps.location.isValid() && gps.location.age() < 2000)
    {
        mDoc["la"] = serialized(String(gps.location.lat(), 6));
        mDoc["lo"] = serialized(String(gps.location.lng(), 6));
    }
    else
    {
        mDoc["g"] = 0;
    }

    // ── Primary: RS485 (WiFi server + discovery pumped inside the wait) ──
    char rs485Buf[RS485_BUF];
    bool gotRS485 = receiveRS485(rs485Buf, sizeof(rs485Buf));

    // Feed GPS after the long wait
    while (GPSSerial.available())
        gps.encode(GPSSerial.read());

    // ── Fallback: WiFi data posted by local node during RS485 window ──
    bool gotWiFi = false;
    char wifiBuf[RS485_BUF] = {0};

#if USE_WIFI_FALLBACK
    portENTER_CRITICAL(&nodeMux);
    if (wifiNodeReady)
    {
        strlcpy(wifiBuf, wifiNodeBuf, sizeof(wifiBuf));
        wifiNodeReady = false;
        gotWiFi = true;
    }
    portEXIT_CRITICAL(&nodeMux);
#endif

    // RS485 preferred; WiFi used only if RS485 had nothing
    bool gotNode = gotRS485 || gotWiFi;
    char *nodeBuf = gotRS485 ? rs485Buf : wifiBuf;

    // Build combined LoRa payload
    StaticJsonDocument<JSON_CAP> payload;
    payload["m"] = mDoc;

    if (gotNode)
    {
        StaticJsonDocument<256> nDoc;
        DeserializationError err = deserializeJson(nDoc, nodeBuf);
        if (!err)
        {
            payload["n"] = nDoc;
            payload["via"] = gotRS485 ? 0 : 1; // 0=RS485, 1=WiFi
        }
        else
        {
            payload["ne"] = 1; // invalid JSON from node
        }
    }
    else
    {
        payload["ne"] = 2; // no data from either path
    }

    char buf[JSON_CAP];
    size_t len = serializeJson(payload, buf);
    Serial.println(buf);

    LoRa.beginPacket();
    LoRa.write((uint8_t *)buf, len);
    LoRa.endPacket();

    // Burn remaining cycle time — keep WiFi server and discovery alive
    while (millis() - t0 < LOOP_MS)
    {
#if USE_WIFI_FALLBACK
        server.handleClient();
        handleDiscovery();
#endif
        while (GPSSerial.available())
            gps.encode(GPSSerial.read());
        delay(10);
    }
}
