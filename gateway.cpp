#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// -------------------- LoRa Pins --------------------
#define LORA_SS 13
#define LORA_RST 14
#define LORA_DIO0 27

// -------------------- WiFi Credentials --------------------
const char *ssid = "YOUR_WIFI_NAME";
const char *password = "YOUR_WIFI_PASSWORD";

// -------------------- API Endpoint --------------------
const char *serverName = "https://lumina-6kxk.onrender.com/api/data";

// Secure client
WiFiClientSecure client;

// -------------------- Setup --------------------
void setup()
{
    Serial.begin(115200);
    delay(1000);

    // 🔹 Connect to WiFi
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // 🔹 Setup LoRa
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(433E6))
    {
        Serial.println("LoRa Failed. Retrying...");
        while (!LoRa.begin(433E6))
        {
            Serial.println("Retrying LoRa...");
            delay(2000);
        }
    }

    Serial.println("Gateway Ready 🚀");
}

// -------------------- Loop --------------------
void loop()
{
    // 🔹 Reconnect WiFi if needed
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi Lost! Reconnecting...");
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
        }
        Serial.println("\nWiFi Reconnected");
    }

    // 🔹 Check for LoRa packet
    int packetSize = LoRa.parsePacket();
    if (packetSize)
    {
        String payload = "";

        while (LoRa.available())
        {
            payload += (char)LoRa.read();
        }

        int rssi = LoRa.packetRssi();

        Serial.println("\nReceived LoRa JSON:");
        Serial.println(payload);
        Serial.println("RSSI: " + String(rssi));

        // 🔹 Send to API
        sendToAPI(payload, rssi);
    }
}

// -------------------- Send Data to API --------------------
void sendToAPI(String jsonPayload, int rssi)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        client.setInsecure(); // ✅ bypass SSL verification

        HTTPClient http;
        http.begin(client, serverName);
        http.addHeader("Content-Type", "application/json");

        // Wrap payload
        String wrappedPayload = "{\"gateway_rssi\":" + String(rssi) + ",\"data\":" + jsonPayload + "}";

        Serial.println("\nSending to API:");
        Serial.println(wrappedPayload);

        int httpResponseCode = http.POST(wrappedPayload);

        if (httpResponseCode > 0)
        {
            Serial.print("HTTP Response Code: ");
            Serial.println(httpResponseCode);

            String response = http.getString();
            Serial.println("Response: " + response);
        }
        else
        {
            Serial.print("Error sending request: ");
            Serial.println(httpResponseCode);
        }

        http.end();
    }
    else
    {
        Serial.println("WiFi not connected. Cannot send data.");
    }
}