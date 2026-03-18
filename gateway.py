import serial
import requests

ser = serial.Serial('COM3', 115200)

while True:
    data = ser.readline().decode().strip()
    print(data)

    try:
        requests.post("https://yourserver.com/api", json={"data": data})
    except:
        print("Error sending")