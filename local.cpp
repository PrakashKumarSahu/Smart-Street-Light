#include <Wire.h>
#include <BH1750.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>

/******** PINS ********/
#define RS485_RX 26
#define RS485_TX 25
#define RS485_DE_RE 33

#define PIR_PIN 4
#define RELAY_PIN 5 //must be high during boot to avoid boot issues

#define CURRENT_SENSOR 32
#define LDR_PIN 34
#define VOLTAGE_SENSOR 35

#define GPS_RX 16
#define GPS_TX 17

/******** OBJECTS ********/
HardwareSerial RS485Serial(1);
HardwareSerial GPSSerial(2);

TinyGPSPlus gps;
BH1750 lightMeter;

/******** FUNCTIONS ********/
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

void sendRS485(String data)
{
    digitalWrite(RS485_DE_RE, HIGH);
    RS485Serial.println(data);
    delay(10);
    digitalWrite(RS485_DE_RE, LOW);
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

    Serial.println("Node A Started");
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

    // Relay Logic
    if (lux < 40 || pir == 1)
        digitalWrite(RELAY_PIN, HIGH);
    else
        digitalWrite(RELAY_PIN, LOW);

    StaticJsonDocument<256> doc;
    doc["lux"] = lux;
    doc["pir"] = pir;
    doc["ldr"] = ldr;
    doc["voltage"] = voltage;
    doc["current"] = current;

    if (gps.location.isValid())
    {
        doc["lat"] = gps.location.lat();
        doc["lon"] = gps.location.lng();
    }

    String data;
    serializeJson(doc, data);

    sendRS485(data);

    Serial.println("Sent RS485:");
    Serial.println(data);

    delay(5000);
}