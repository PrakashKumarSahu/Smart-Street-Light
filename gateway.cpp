#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

/******** LoRa Pins ********/
#define LORA_SS 13
#define LORA_RST 14
#define LORA_DIO0 27

/******** CONFIG ********/
#define LORA_BUF 512
#define JSON_CAP 768
#define NO_PKT_MS 10000UL // report "no packet" after this many ms

const char *ssid = "Prakash's S23";
const char *password = "Prakash123";
const char *endpoint = "https://lumina-6kxk.onrender.com/api/data";

WiFiClientSecure client;
unsigned long lastPkt = 0;

/******** HELPERS ********/
void wifiReconnect()
{
    if (WiFi.status() == WL_CONNECTED)
        return;
    Serial.print("WiFi reconnecting");
    WiFi.begin(ssid, password);
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " FAIL");
}

void sendToAPI(const char *loraData, int rssi, bool received)
{
    client.setInsecure();

    HTTPClient http;
    http.begin(client, endpoint);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<JSON_CAP> doc;

    // "s": 1 = received, 0 = not received  (saves bytes vs string "received")
    doc["s"] = received ? 1 : 0;
    doc["rssi"] = rssi;

    if (received && loraData)
    {
        StaticJsonDocument<512> lDoc;
        DeserializationError err = deserializeJson(lDoc, loraData);
        if (!err)
        {
            doc["d"] = lDoc; // parsed LoRa payload
        }
        else
        {
            doc["de"] = 1; // data error: invalid json
        }
    }
    else
    {
        doc["de"] = 2; // data error: no data
    }

    char buf[JSON_CAP];
    size_t len = serializeJson(doc, buf);

    Serial.println(buf);

    int code = http.POST((uint8_t *)buf, len);
    if (code > 0)
    {
        Serial.printf("HTTP %d: %s\n", code, http.getString().c_str());
    }
    else
    {
        Serial.printf("HTTP error: %d\n", code);
    }

    http.end();
}

/******** SETUP ********/
void setup()
{
    Serial.begin(115200);
    delay(500);

    WiFi.begin(ssid, password);
    Serial.print("Connecting WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" OK");

    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    while (!LoRa.begin(433E6))
    {
        Serial.println("LoRa init failed, retrying...");
        delay(2000);
    }

    Serial.println("Gateway Ready");
    lastPkt = millis();
}

/******** LOOP ********/
void loop()
{
    wifiReconnect();

    int packetSize = LoRa.parsePacket();

    if (packetSize)
    {
        char buf[LORA_BUF];
        size_t idx = 0;

        while (LoRa.available() && idx < sizeof(buf) - 1)
            buf[idx++] = (char)LoRa.read();
        buf[idx] = '\0';

        int rssi = LoRa.packetRssi();
        Serial.printf("LoRa RX (RSSI %d): %s\n", rssi, buf);

        sendToAPI(buf, rssi, true);
        lastPkt = millis();
    }
    else if (millis() - lastPkt > NO_PKT_MS)
    {
        Serial.println("No LoRa packet");
        sendToAPI(nullptr, 0, false);
        lastPkt = millis();
    }
}
