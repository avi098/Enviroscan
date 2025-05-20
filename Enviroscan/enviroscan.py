import requests
import json
import time
import pandas as pd
from datetime import datetime
import threading
import socket
import backoff
import os
from flask import Flask, render_template, jsonify
from sklearn.ensemble import RandomForestRegressor
from sklearn.preprocessing import StandardScaler
import joblib

# Flask app setup
app = Flask(__name__)

# Configuration
ESP8266_HOSTNAME = "enviroscan.local"
ESP8266_IP = None
DATA_URL = None
UPDATE_INTERVAL = 1  # Seconds between data fetches
EXPECTED_MAC = "98:F4:AB:F9:34:22"  # ESP8266 MAC address
MODEL_PATH = "aqi_model.joblib"
SCALER_PATH = "scaler.joblib"
data_history = []  # Store data for web UI

# Discover device IP
def get_device_address():
    global ESP8266_IP, DATA_URL
    try:
        ESP8266_IP = socket.gethostbyname(ESP8266_HOSTNAME)
        DATA_URL = f"http://{ESP8266_IP}/data"
        print(f"Resolved mDNS hostname to {ESP8266_IP}")
    except socket.gaierror:
        ESP8266_IP = "192.168.43.240"  # Fallback IP
        DATA_URL = f"http://{ESP8266_IP}/data"
        print(f"Using fallback IP: {ESP8266_IP}")

# Air Quality ML Model
class AirQualityModel:
    def __init__(self):
        self.model = RandomForestRegressor(n_estimators=100, random_state=42)
        self.scaler = StandardScaler()
        self.is_trained = False
        self.training_data = []
        self.training_targets = []
        self.load_model()

    def load_model(self):
        if os.path.exists(MODEL_PATH) and os.path.exists(SCALER_PATH):
            self.model = joblib.load(MODEL_PATH)
            self.scaler = joblib.load(SCALER_PATH)
            self.is_trained = True
            print("Loaded existing ML model and scaler")
        else:
            print("No ML model or scaler found; will train with new data")

    def save_model(self):
        if self.is_trained:
            joblib.dump(self.model, MODEL_PATH)
            joblib.dump(self.scaler, SCALER_PATH)
            print("Saved ML model and scaler")

    def train(self, features, target):
        self.training_data.append([features])
        self.training_targets.append(target)
        if len(self.training_data) >= 20:
            X = self.scaler.fit_transform(self.training_data)
            y = self.training_targets
            self.model.fit(X, y)
            self.is_trained = True
            self.save_model()
            if len(self.training_data) > 1000:
                self.training_data = self.training_data[-1000:]
                self.training_targets = self.training_targets[-1000:]

    def predict(self, raw_ppm):
        if not self.is_trained:
            return raw_ppm
        try:
            X = self.scaler.transform([[raw_ppm]])
            return max(1, self.model.predict(X)[0])
        except ValueError:
            print("Scaler not fitted yet; returning raw PPM")
            return raw_ppm

air_quality_model = AirQualityModel()

# Calculate AQI (matching ESP8266 logic)
def calculate_aqi(ppm):
    if ppm < 50: return (50 / 50) * ppm
    elif ppm < 100: return 50 + (50 / 50) * (ppm - 50)
    elif ppm < 150: return 100 + (50 / 50) * (ppm - 100)
    elif ppm < 200: return 150 + (50 / 50) * (ppm - 150)
    elif ppm < 300: return 200 + (100 / 100) * (ppm - 200)
    else: return 300 + (200 / 200) * (ppm - 300)

# Calculate Gas Percentage (matching ESP8266 logic)
def calculate_gas_percentage(ppm):
    max_ppm = 10000.0
    return min(100.0, (ppm / max_ppm) * 100.0)

# Fetch data from ESP8266
@backoff.on_exception(backoff.expo, requests.exceptions.RequestException, max_tries=5)
def fetch_sensor_data():
    response = requests.get(DATA_URL, timeout=3)
    if response.status_code == 200:
        data = response.json()
        if data.get("mac", "").replace(":", "").lower() == EXPECTED_MAC.replace(":", "").lower():
            print("Raw data from ESP8266:", data)
            return data
    return None

# Collect data in background
def collect_data():
    global data_history
    while True:
        try:
            raw_data = fetch_sensor_data()
            if raw_data:
                # Preserve all fields from ESP8266 and add computed fields
                data = {}
                data["timestamp"] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                data["raw_ppm"] = float(raw_data.get("raw_ppm", raw_data.get("ml_ppm", 0)))
                data["ml_ppm"] = air_quality_model.predict(data["raw_ppm"])
                data["aqi"] = float(raw_data.get("aqi", calculate_aqi(data["ml_ppm"])))
                data["gas_percentage"] = float(raw_data.get("gas_percentage", calculate_gas_percentage(data["ml_ppm"])))
                data["ip"] = raw_data.get("ip", ESP8266_IP)
                data["mac"] = raw_data.get("mac", "")

                air_quality_model.train(data["raw_ppm"], data["ml_ppm"])
                data_history.append(data)
                print(f"Processed data: {data}")
                if len(data_history) > 100:
                    data_history = data_history[-100:]
            else:
                print("Failed to fetch data, retrying...")
                get_device_address()
        except Exception as e:
            print(f"Error in data collection: {e}")
        time.sleep(UPDATE_INTERVAL)

# Flask routes
@app.route('/')
def index():
    return render_template('index.html')

@app.route('/data')
def get_data():
    return jsonify(data_history[-10:])  # Return last 10 data points

def main():
    print("Starting ENVIROSCAN data logger and web server...")
    get_device_address()
    print(f"Fetching data from {DATA_URL}")

    # Start data collection in a separate thread
    data_thread = threading.Thread(target=collect_data, daemon=True)
    data_thread.start()

    # Start Flask web server
    app.run(host='0.0.0.0', port=5000, debug=False)

if __name__ == "__main__":
    main()