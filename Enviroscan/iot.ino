#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

ESP8266WebServer server(80);

const char* ssid = "GalaxyM30s1308";      // Your Wi-Fi SSID
const char* password = "qwertyipn";       // Your Wi-Fi password
const int MQ135_PIN = A0;                   // Analog pin for MQ-135 sensor
const float R0 = 10.0;                      // Sensor resistance in clean air (kΩ)
const float RL = 10.0;                      // Load resistor value (kΩ)

// HTML content (minified for ESP8266 memory constraints)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ENVIROSCAN Dashboard</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { font-family: 'Inter', sans-serif; background: #1a202c; color: #e2e8f0; }
        .container { max-width: 1200px; margin: 0 auto; padding: 2rem; }
        .card { background: #2d3748; border-radius: 0.5rem; padding: 1.5rem; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); }
        .gauge { position: relative; width: 200px; height: 100px; margin: 0 auto; }
        .gauge-svg { width: 100%; height: 100%; }
        .biohazard { font-weight: bold; padding: 0.5rem; border-radius: 0.25rem; }
    </style>
</head>
<body>
    <div class="container mx-auto">
        <h1 class="text-3xl font-bold text-center mb-6">ENVIROSCAN Air Quality Dashboard</h1>
        <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            <!-- PPM Chart -->
            <div class="card col-span-2">
                <h2 class="text-xl font-semibold mb-4">Gas Concentration (PPM)</h2>
                <canvas id="ppmChart"></canvas>
            </div>
            <!-- AQI Gauge -->
            <div class="card">
                <h2 class="text-xl font-semibold mb-4">Air Quality Index (AQI)</h2>
                <div class="gauge" id="aqiGauge"></div>
                <p id="aqiLabel" class="text-center mt-2"></p>
            </div>
            <!-- Gas Percentage Chart -->
            <div class="card col-span-2">
                <h2 class="text-xl font-semibold mb-4">Gas Percentage</h2>
                <canvas id="gasChart"></canvas>
            </div>
            <!-- Biohazard Level -->
            <div class="card">
                <h2 class="text-xl font-semibold mb-4">Biohazard Level</h2>
                <p id="biohazard" class="biohazard text-center"></p>
                <p id="stats" class="text-sm mt-4"></p>
            </div>
        </div>
    </div>

    <script>
        const ppmChart = new Chart(document.getElementById('ppmChart'), {
            type: 'line', data: { labels: [], datasets: [{ label: 'Raw PPM', data: [], borderColor: '#63b3ed', fill: false }, { label: 'ML PPM', data: [], borderColor: '#3182ce', fill: false }] },
            options: { responsive: true, scales: { x: { title: { display: true, text: 'Time (s)' } }, y: { title: { display: true, text: 'PPM' } } } }
        });
        const gasChart = new Chart(document.getElementById('gasChart'), {
            type: 'line', data: { labels: [], datasets: [{ label: 'Gas %', data: [], borderColor: '#ed8936', fill: false }] },
            options: { responsive: true, scales: { x: { title: { display: true, text: 'Time (s)' } }, y: { title: { display: true, text: 'Percentage (%)' } } } }
        });

        const aqiLevels = [
            { range: [0, 50], label: 'Good', color: '#48bb78' },
            { range: [51, 100], label: 'Moderate', color: '#ecc94b' },
            { range: [101, 150], label: 'Unhealthy for Sensitive', color: '#ed8936' },
            { range: [151, 200], label: 'Unhealthy', color: '#f56565' },
            { range: [201, 300], label: 'Very Unhealthy', color: '#9f7aea' },
            { range: [301, 500], label: 'Hazardous', color: '#9b2c2c' }
        ];

        function drawGauge(value) {
            const gauge = document.getElementById('aqiGauge');
            gauge.innerHTML = '';
            const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
            svg.setAttribute('class', 'gauge-svg');
            const arc = (start, end, color) => {
                const r = 80, cx = 100, cy = 90;
                const startRad = Math.PI * (start / 500), endRad = Math.PI * (end / 500);
                const x1 = cx + r * Math.cos(startRad), y1 = cy - r * Math.sin(startRad);
                const x2 = cx + r * Math.cos(endRad), y2 = cy - r * Math.sin(endRad);
                const path = `M ${x1} ${y1} A ${r} ${r} 0 ${end - start > 250 ? 1 : 0} 1 ${x2} ${y2}`;
                const p = document.createElementNS('http://www.w3.org/2000/svg', 'path');
                p.setAttribute('d', path); p.setAttribute('stroke', color); p.setAttribute('stroke-width', '20'); p.setAttribute('fill', 'none');
                svg.appendChild(p);
            };
            aqiLevels.forEach(l => arc(l.range[0], l.range[1], l.color));
            const needleRad = Math.PI * (Math.min(value, 500) / 500);
            const needle = document.createElementNS('http://www.w3.org/2000/svg', 'line');
            needle.setAttribute('x1', '100'); needle.setAttribute('y1', '90'); needle.setAttribute('x2', 100 + 70 * Math.cos(needleRad)); needle.setAttribute('y2', 90 - 70 * Math.sin(needleRad));
            needle.setAttribute('stroke', 'white'); needle.setAttribute('stroke-width', '3');
            svg.appendChild(needle);
            gauge.appendChild(svg);
            const level = aqiLevels.find(l => value >= l.range[0] && value <= l.range[1]) || aqiLevels[aqiLevels.length - 1];
            document.getElementById('aqiLabel').innerText = `AQI: ${Math.round(value)} - ${level.label}`;
            document.getElementById('aqiLabel').style.color = level.color;
        }

        function updateBiohazard(ppm) {
            const bio = document.getElementById('biohazard');
            if (ppm < 50) { bio.innerText = 'Safe'; bio.style.backgroundColor = '#48bb78'; }
            else if (ppm < 100) { bio.innerText = 'Low'; bio.style.backgroundColor = '#ecc94b'; }
            else if (ppm < 200) { bio.innerText = 'Moderate'; bio.style.backgroundColor = '#ed8936'; }
            else if (ppm < 500) { bio.innerText = 'High'; bio.style.backgroundColor = '#f56565'; }
            else { bio.innerText = 'Critical'; bio.style.backgroundColor = '#9b2c2c'; }
        }

        let time = 0;
        function fetchData() {
            fetch('/data').then(res => res.json()).then(data => {
                time += 1;
                ppmChart.data.labels.push(time);
                ppmChart.data.datasets[0].data.push(data.raw_ppm);
                ppmChart.data.datasets[1].data.push(data.ml_ppm);
                gasChart.data.labels.push(time);
                gasChart.data.datasets[0].data.push(data.gas_percentage);
                if (ppmChart.data.labels.length > 100) {
                    ppmChart.data.labels.shift();
                    ppmChart.data.datasets.forEach(d => d.data.shift());
                    gasChart.data.labels.shift();
                    gasChart.data.datasets[0].data.shift();
                }
                ppmChart.update();
                gasChart.update();
                drawGauge(data.aqi);
                updateBiohazard(data.ml_ppm);
                document.getElementById('stats').innerText = `Raw PPM: ${data.raw_ppm}\nML PPM: ${data.ml_ppm}\nIP: ${data.ip}`;
            }).catch(err => console.error('Fetch error:', err));
        }

        setInterval(fetchData, 1000);
        fetchData();
    </script>
</body>
</html>
)rawliteral";

// WiFi reconnection function
void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting to reconnect...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to reconnect to WiFi");
    }
  }
}

// Calculate sensor resistance
float calculateResistance(int raw_adc) {
  float voltage = raw_adc * (3.3 / 1023.0);
  if (voltage == 0) return RL; // Avoid division by zero
  float rs = RL * (3.3 - voltage) / voltage;
  return rs;
}

// Calculate PPM
float calculatePPM(float rs) {
  float ratio = rs / R0;
  float a = 100.0; // Calibration coefficient
  float b = -1.53; // Calibration exponent
  return a * pow(ratio, b);
}

// Calculate AQI (simplified for ESP8266)
float calculateAQI(float ppm) {
  if (ppm < 50) return (50 / 50) * ppm;
  else if (ppm < 100) return 50 + (50 / 50) * (ppm - 50);
  else if (ppm < 150) return 100 + (50 / 50) * (ppm - 100);
  else if (ppm < 200) return 150 + (50 / 50) * (ppm - 150);
  else if (ppm < 300) return 200 + (100 / 100) * (ppm - 200);
  else return 300 + (200 / 200) * (ppm - 300);
}

// Calculate gas percentage
float calculateGasPercentage(float ppm) {
  const float max_ppm = 10000.0; // Ensure consistent float type
  return min(100.0f, (ppm / max_ppm) * 100.0f); // Use float literals with 'f'
}

// Send sensor data function
void sendSensorData() {
  int readings = 0;
  for (int i = 0; i < 5; i++) {
    readings += analogRead(MQ135_PIN);
    delay(10);
  }
  int rawValue = readings / 5;

  float rs = calculateResistance(rawValue);
  float ppm = calculatePPM(rs);
  float aqi = calculateAQI(ppm);
  float gas_percentage = calculateGasPercentage(ppm);

  String json = "{";
  json += "\"raw_ppm\":" + String(rawValue) + ",";
  json += "\"ml_ppm\":" + String(ppm, 1) + ",";
  json += "\"aqi\":" + String(aqi, 1) + ",";
  json += "\"gas_percentage\":" + String(gas_percentage, 2) + ",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(MQ135_PIN, INPUT);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Start mDNS
  if (MDNS.begin("enviroscan")) {
    Serial.println("mDNS started: http://enviroscan.local");
  } else {
    Serial.println("Error starting mDNS");
  }

  // Server routes
  server.on("/", []() {
    server.send_P(200, "text/html", index_html);
  });
  server.on("/data", sendSensorData);
  server.begin();
  Serial.println("Server started");
}

void loop() {
  reconnectWiFi();
  server.handleClient();
  MDNS.update();
  delay(10);
}
