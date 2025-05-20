
# EnviroScan

## 🌍 Overview

**EnviroScan** is an IoT-based environmental monitoring system that collects gas sensor data and predicts the Air Quality Index (AQI) using a machine learning model. It integrates Arduino-based hardware with a Python Flask web server to provide real-time AQI insights.

## 📁 Project Structure

```
Enviroscan/
├── aqi_model.joblib        # Trained ML model for AQI prediction
├── scaler.joblib           # Feature scaler used in model training
├── enviroscan.py           # Python Flask app to receive and display data
├── iot.ino                 # Arduino sketch for ESP8266 and gas sensors
└── templates/
    └── index.html          # Web interface for AQI display
```

## 🔧 Hardware Requirements

- ESP8266 (e.g., NodeMCU)
- MQ-series gas sensors (e.g., MQ-2, MQ-7, MQ-135)
- USB cable for programming
- Jumper wires, breadboard

## 💻 Software Setup

### Arduino Side

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Add ESP8266 board via **Board Manager**.
3. Open `iot.ino` and upload it to the ESP8266.
4. Connect your sensors as defined in the sketch.

### Python Side

1. Install Python 3.7 or above.
2. Navigate to the `Enviroscan` folder in your terminal.
3. Create a virtual environment (optional but recommended):
   ```bash
   python -m venv venv
   source venv/bin/activate  # On Windows: venv\Scripts\activate
   ```
4. Install dependencies:
   ```bash
   pip install flask joblib scikit-learn pyserial
   ```

## 🚀 Running the Project

1. Connect your Arduino device via USB.
2. Run the Flask app:
   ```bash
   python enviroscan.py
   ```
3. Open your browser and go to `http://127.0.0.1:5000` to view the dashboard.

## 🌐 Web Interface

The dashboard (served by Flask) shows:
- Real-time sensor readings
- Predicted AQI value
- AQI categories (Good, Moderate, Hazardous, etc.)

## 🛠️ Troubleshooting

- Ensure the correct COM port is set in `enviroscan.py` (`serial.Serial(...)`).
- Verify the Arduino serial baud rate matches the Python script.
- Check sensor wiring if readings seem incorrect.

---

© 2025 EnviroScan Team — Smart Monitoring for a Healthier Future 🌱
