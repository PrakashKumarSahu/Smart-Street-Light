# 🌆 Smart Street Light Fault Detection System

An advanced **IoT-based Smart Street Light System** using ESP32 for **automatic control, fault detection, and real-time monitoring**.

The system combines:
- **RS485** for communication between multiple nodes  
- **LoRa (SX1278)** for long-range transmission  
- **Django + React dashboard** for centralized monitoring  

---

## 🚀 Features

- 🌙 **Automatic Operation**
  - Works only at night using LDR 
  - Activates lights only when motion is detected (PIR)

- ⚡ **Fault Detection**
  - Voltage monitoring 
  - Current monitoring  
  - Detects abnormal behavior and failures  

- 📡 **Hybrid Communication**
  - RS485 → Local Nodes to Master Node  
  - LoRa → Master Node to Gateway  

- 📍 **GPS Integration**
  - Location tracking using Neo-6M  

- 📊 **Central Dashboard**
  - Built with **Django + React**  
  - Real-time monitoring and visualization  

- 🔁 **Scalable System**
  - Multiple nodes connected via RS485  

---

## 🧠 System Architecture

```
(Local Nodes A1, A2, ...)
            ↓ RS485
        Master Node (B)
            ↓ LoRa
     LoRa Gateway ESP32
            ↓ USB
         Computer
            ↓ Internet
   Django + React Server
```

---

## 📡 Node Description

### 🔹 Normal Node (Local Node A)
- Reads sensor data  
- Controls street light via relay  
- Sends data to Master Node via RS485  

---

### 🔸 Master Node (Special Node B)
- Receives data from multiple nodes  
- Reads its own sensors + GPS  
- Sends aggregated data via LoRa  

---

### 📡 Gateway
- Receives LoRa data  
- Sends data to computer/server  

---

## 🔌 Pin Configuration

---

## 🔹 NORMAL NODE (Node A)

### UART (GPS)
| Device | GPIO |
|--------|------|
| GPS TX → ESP RX | 16 |
| GPS RX → ESP TX | 17 |

### RS485
| Function | GPIO |
|----------|------|
| RO (RX) | 26 |
| DI (TX) | 25 |
| RE + DE | 33 |

### I2C (BH1750)
| Function | GPIO |
|----------|------|
| SDA | 21 |
| SCL | 22 |

### Sensors
| Sensor | GPIO |
|--------|------|
| PIR | 4 |
| Current Sensor | 32 |
| LDR | 34 |
| Voltage Sensor | 35 |

### Relay
| Function | GPIO |
|----------|------|
| Relay IN | 5 |

---

## 🔸 MASTER NODE (Node B)

### UART (GPS)
| Device | GPIO |
|--------|------|
| GPS TX → ESP RX | 16 |
| GPS RX → ESP TX | 17 |

### RS485
| Function | GPIO |
|----------|------|
| RO (RX) | 26 |
| DI (TX) | 25 |
| RE + DE | 33 |

### LoRa (SX1278)
| Function | GPIO |
|----------|------|
| MISO | 19 |
| MOSI | 23 |
| SCK | 18 |
| NSS (CS) | 13 |
| RST | 14 |
| DIO0 | 27 |

### I2C (BH1750)
| Function | GPIO |
|----------|------|
| SDA | 21 |
| SCL | 22 |

### Sensors
| Sensor | GPIO |
|--------|------|
| PIR | 4 |
| Current Sensor | 32 |
| LDR | 34 |
| Voltage Sensor | 35 |

### Relay
| Function | GPIO |
|----------|------|
| Relay IN | 5 |

---

## 📡 LoRa Gateway ESP32

> Same LoRa configuration as Master Node

| Function | GPIO |
|----------|------|
| MISO | 19 |
| MOSI | 23 |
| SCK | 18 |
| NSS (CS) | 13 |
| RST | 14 |
| DIO0 | 27 |

---

## ⚠️ Important Notes

- Avoid GPIO: **0, 2, 12, 15 (boot pins caution)**  
- Input-only pins: **34, 35, 36, 39**  
- Use **common GND** for all modules  
- Use **120Ω termination resistor** for RS485  
- Always connect **LoRa antenna**  
- You can safely use GPIO 5 for your relay if you   add a 10k pull-up resistor to 3.3V, because this ensures the pin stays HIGH during boot (required for strapping pins), while still allowing the ESP32 to control it normally—when the ESP32 sets the pin LOW, it actively connects it to ground and easily overrides the weak pull-up, so the relay will work as expected; without the pull-up, especially with an active LOW relay module, you risk random boot failures or unstable startup, so the pull-up makes the system reliable without affecting operation.

---

## ⚡ Hardware Design Advice

- Use **relay module with driver transistor**  
- Add **flyback diode** across relay coil  
- Prevent ESP32 reset due to voltage spikes  

---

## 📊 Dashboard (Django + React)

- Real-time monitoring  
- Fault detection alerts  
- Sensor data visualization  

> 📍 Add dashboard screenshots here

---

## 🔌 Circuit Diagram

> 📍 Add circuit diagram here

---

## 🔄 Data Flow Diagram

> 📍 Add architecture diagram here

---

## 🖥️ Output / Results

> 📍 Add output logs / screenshots here

---

## ⚙️ Setup Instructions

1. Upload code to ESP32 nodes  
2. Connect RS485 between nodes  
3. Configure LoRa modules  
4. Connect gateway to computer  
5. Run Django backend  
6. Run React frontend  
7. Monitor system  

---

## 🎯 Applications

- Smart city infrastructure  
- Street light automation  
- Fault detection systems  
- Energy saving systems  

---

## 🔮 Future Improvements

- MQTT/Cloud integration  
- AI-based fault prediction  
- Adaptive brightness control  

---

## 👨‍💻 Author

ESP32-based Smart IoT System

---

## ⭐ Support

If you found this useful, give it a ⭐ and contribute!