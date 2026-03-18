#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <BH1750.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>

/******** PINS ********/
#define RS485_RX 26
#define RS485_TX 25
#define RS485_DE_RE 33

#define PIR_PIN 4
#define RELAY_PIN 5 // must be high during boot to avoid boot issues, changed from 15 to 5

#define CURRENT_SENSOR 32
#define LDR_PIN 34
#define VOLTAGE_SENSOR 35

#define GPS_RX 16
#define GPS_TX 17

#define LORA_SS 13
#define LORA_RST 14
#define LORA_DIO0 27

/******** OBJECTS ********/
HardwareSerial RS485Serial(1);
HardwareSerial GPSSerial(2);

TinyGPSPlus gps;
BH1750 lightMeter;

/******** FUNCTIONS ********/
String receiveRS485()
{
    String data = "";
    digitalWrite(RS485_DE_RE, LOW);

    long start = millis();
    while (millis() - start < 1000)
    {
        while (RS485Serial.available())
        {
            data += char(RS485Serial.read());
        }
    }
    return data;
}

float readVoltage()
{
    int adc = analogRead(VOLTAGE_SENSOR);
    return (adc * 3.3 / 4095.0) * 5;
}

float readCurrent()
{
    int adc = analogRead(CURRENT_SENSOR);
    return (adc * 3.3 / 4095.0);
}

void setup()
{

    Serial.begin(115200);

    pinMode(PIR_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(RS485_DE_RE, OUTPUT);

    RS485Serial.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
    GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

    Wire.begin(21, 22);
    lightMeter.begin();

    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433E6))
    {
        Serial.println("LoRa Failed");
        while (1)
            ;
    }

    Serial.println("Node B Started");
}

void loop()
{

    while (GPSSerial.available())
        gps.encode(GPSSerial.read());

    float lux = lightMeter.readLightLevel();
    int pir = digitalRead(PIR_PIN);
    int ldr = analogRead(LDR_PIN);
    float voltage = readVoltage();
    float current = readCurrent();

    if (lux < 40 || pir == 1)
        digitalWrite(RELAY_PIN, HIGH);
    else
        digitalWrite(RELAY_PIN, LOW);

    StaticJsonDocument<256> myData;
    myData["lux"] = lux;
    myData["pir"] = pir;
    myData["ldr"] = ldr;
    myData["voltage"] = voltage;
    myData["current"] = current;

    if (gps.location.isValid())
    {
        myData["lat"] = gps.location.lat();
        myData["lon"] = gps.location.lng();
    }

    String normalData = receiveRS485();

    StaticJsonDocument<512> finalDoc;
    finalDoc["master"] = myData;
    finalDoc["normal"] = normalData;

    String payload;
    serializeJson(finalDoc, payload);

    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();

    Serial.println("LoRa Sent:");
    Serial.println(payload);

    delay(5000);
}