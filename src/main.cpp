#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "VIsensors.h"
#include "Controls.h"
#include "DisplayPrint.h"

// ====== WIFI CONFIG (edit these!) ======
const char* WIFI_SSID     = "FIERES";
const char* WIFI_PASSWORD = "11381138";

// ===== Pins =====
#define vSensorPinout 33   // Battery / output voltage
#define iSensorOut    39   // Battery / output current
#define vSensorPinin  34   // Solar / input voltage
#define iSensorPinin  35   // Solar / input current

#define relayPin      32   // Charging relay
#define loadPin       25   // Load switch

#define redLedPin     27   // LED for error indication
#define blueLedPin    26   // LED for status / OK

// ===== Objects =====
VIsensors viSensorsInput(vSensorPinin,  iSensorPinin);   // Solar side
VIsensors viSensorsOutput(vSensorPinout, iSensorOut);    // Battery / load side
Controls controls(relayPin, loadPin);
WebServer server(80);

// ===== Measured values =====
float Vsolar   = 0.0f, Isolar = 0.0f;
float Vbat     = 0.0f, Ibat   = 0.0f;
float powerIn  = 0.0f, powerOut = 0.0f;

// ===== Status for display / state =====
bool isCharging      = false;
bool isLoadActive    = false;
bool systemStatus    = true;   // true = OK, false = Error/Warning
int  batteryLevel    = 0;      // 0–100 %
int  loadPercentage  = 0;      // 0–100 %

// ===== Limits (DEFAULT, can be changed from WEB) =====
float ISolarmax = 3.0f;   // Max solar charging current (A)
float ILoadmax  = 3.0f;   // Max load current (A)
float VSolarmax = 18.0f;  // Max solar voltage (V)
float VBatmax   = 15.0f;  // Max battery voltage (V)
float VBatmin   = 10.0f;  // Min battery voltage (V)

// ===== Mode / manual control =====
bool autoMode          = true;  // true = MPPT auto, false = manual
bool manualRelayState  = false;
bool manualLoadState   = false;

// ===== Timing =====
unsigned long previousMillis = 0;
const unsigned long interval = 100;  // 100ms update

// ===== FORWARD DECLARATIONS =====
void setupWebServer();
void errorAndRestartDisplay(const char *errMsg);
void handleLogicAuto();
void handleLogicManual();

// ===== Web UI HTML page (simple SPA) =====
const char MAIN_page[] PROGMEM = R"====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>MPPT Charge Control</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  :root {
    font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    background: #0f172a;
    color: #e5e7eb;
  }
  body {
    margin: 0;
    padding: 1rem;
  }
  h1 {
    text-align: center;
    margin-bottom: 1rem;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
    gap: 1rem;
    max-width: 1100px;
    margin: 0 auto;
  }
  .card {
    background: #020617;
    border-radius: 18px;
    padding: 1rem 1.2rem;
    box-shadow: 0 10px 40px rgba(15,23,42,0.7);
    border: 1px solid #1f2937;
  }
  .card h2 {
    margin-top: 0;
    font-size: 1.1rem;
    margin-bottom: 0.6rem;
  }
  .kv {
    display: flex;
    justify-content: space-between;
    margin: 0.2rem 0;
    font-size: 0.95rem;
  }
  .label {
    color: #9ca3af;
  }
  .value {
    font-weight: 600;
  }
  .status-ok {
    color: #4ade80;
  }
  .status-err {
    color: #f97373;
  }
  .badge {
    display: inline-block;
    padding: 0.15rem 0.5rem;
    border-radius: 999px;
    font-size: 0.75rem;
    border: 1px solid #374151;
    margin-left: 0.4rem;
  }
  .badge.auto {
    background: #0f766e33;
    color: #6ee7b7;
  }
  .badge.manual {
    background: #7c2d1233;
    color: #f97316;
  }
  .inputs {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.5rem 0.75rem;
  }
  .inputs label {
    font-size: 0.8rem;
    color: #9ca3af;
  }
  .inputs input {
    width: 100%;
    box-sizing: border-box;
    padding: 0.3rem 0.45rem;
    border-radius: 8px;
    border: 1px solid #4b5563;
    background: #020617;
    color: #e5e7eb;
    font-size: 0.85rem;
  }
  .btn-row {
    margin-top: 0.6rem;
    display: flex;
    gap: 0.5rem;
    justify-content: flex-end;
  }
  button {
    border-radius: 999px;
    border: none;
    padding: 0.45rem 0.9rem;
    font-size: 0.85rem;
    cursor: pointer;
    background: #4f46e5;
    color: white;
    box-shadow: 0 8px 25px rgba(79,70,229,0.5);
  }
  button.secondary {
    background: #111827;
    color: #e5e7eb;
    box-shadow: none;
    border: 1px solid #374151;
  }
  button:active {
    transform: translateY(1px);
    box-shadow: 0 4px 15px rgba(79,70,229,0.4);
  }
  .toggle-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin: 0.3rem 0;
  }
  .toggle-row span {
    font-size: 0.9rem;
  }
  .toggle-row input[type="checkbox"] {
    width: 42px;
    height: 22px;
  }
  small {
    color: #6b7280;
    font-size: 0.75rem;
  }
</style>
</head>
<body>
  <h1>MPPT Charge Control</h1>
  <div class="grid">
    <!-- Live status card -->
    <div class="card">
      <h2>Live Data <span id="mode-badge" class="badge auto">AUTO</span></h2>
      <div class="kv"><span class="label">Vsolar</span><span class="value" id="vsolar">--</span></div>
      <div class="kv"><span class="label">Isolar</span><span class="value" id="isolar">--</span></div>
      <div class="kv"><span class="label">Vbat</span><span class="value" id="vbat">--</span></div>
      <div class="kv"><span class="label">Ibat / Iload</span><span class="value" id="ibat">--</span></div>
      <div class="kv"><span class="label">Pin</span><span class="value" id="pin">--</span></div>
      <div class="kv"><span class="label">Pout</span><span class="value" id="pout">--</span></div>
      <div class="kv"><span class="label">Battery level</span><span class="value" id="batt">--</span></div>
      <div class="kv"><span class="label">Load %</span><span class="value" id="load">--</span></div>
      <div class="kv"><span class="label">Charging</span><span class="value" id="charging">--</span></div>
      <div class="kv"><span class="label">Load state</span><span class="value" id="loadstate">--</span></div>
      <div class="kv">
        <span class="label">System</span>
        <span class="value" id="status">--</span>
      </div>
    </div>

    <!-- Thresholds card -->
    <div class="card">
      <h2>Thresholds</h2>
      <div class="inputs">
        <div>
          <label for="ISolarmax">ISolarmax (A)</label>
          <input type="number" step="0.01" id="ISolarmax">
        </div>
        <div>
          <label for="ILoadmax">ILoadmax (A)</label>
          <input type="number" step="0.01" id="ILoadmax">
        </div>
        <div>
          <label for="VBatmin">VBatmin (V)</label>
          <input type="number" step="0.01" id="VBatmin">
        </div>
        <div>
          <label for="VBatmax">VBatmax (V)</label>
          <input type="number" step="0.01" id="VBatmax">
        </div>
        <div>
          <label for="VSolarmax">VSolarmax (V)</label>
          <input type="number" step="0.01" id="VSolarmax">
        </div>
      </div>
      <div class="btn-row">
        <button onclick="saveSettings()">Save thresholds</button>
      </div>
      <small>Thresholds are enforced in both AUTO and MANUAL modes.</small>
    </div>

    <!-- Control card -->
    <div class="card">
      <h2>Control</h2>
      <div class="toggle-row">
        <span>Mode</span>
        <button class="secondary" id="mode-btn" onclick="toggleMode()">Switch to MANUAL</button>
      </div>
      <hr style="border-color:#1f2937;">
      <div class="toggle-row">
        <span>Charging relay</span>
        <input type="checkbox" id="relay-toggle" onchange="sendControl()">
      </div>
      <div class="toggle-row">
        <span>Load</span>
        <input type="checkbox" id="load-toggle" onchange="sendControl()">
      </div>
      <small>Manual relay/load only have effect when mode is MANUAL.</small>
    </div>
  </div>

<script>
let state = {};

async function fetchState() {
  try {
    const res = await fetch('/api/state');
    if (!res.ok) return;
    state = await res.json();
    updateUI();
  } catch (e) {
    console.log(e);
  }
}

function updateUI() {
  const num = (v, d=2) => (v !== undefined ? v.toFixed(d) : '--');

  document.getElementById('vsolar').textContent = num(state.Vsolar,2) + ' V';
  document.getElementById('isolar').textContent = num(state.Isolar,2) + ' A';
  document.getElementById('vbat').textContent   = num(state.Vbat,2)   + ' V';
  document.getElementById('ibat').textContent   = num(state.Ibat,2)   + ' A';
  document.getElementById('pin').textContent    = num(state.powerIn,1)  + ' W';
  document.getElementById('pout').textContent   = num(state.powerOut,1) + ' W';
  document.getElementById('batt').textContent   = (state.batteryLevel ?? '--') + ' %';
  document.getElementById('load').textContent   = (state.loadPercentage ?? '--') + ' %';
  document.getElementById('charging').textContent = state.isCharging ? 'ON' : 'OFF';
  document.getElementById('loadstate').textContent = state.isLoadActive ? 'ON' : 'OFF';

  const statusEl = document.getElementById('status');
  if (state.systemStatus) {
    statusEl.textContent = 'OK';
    statusEl.className = 'value status-ok';
  } else {
    statusEl.textContent = 'ERR';
    statusEl.className = 'value status-err';
  }

  // thresholds
  if (state.ISolarmax !== undefined) {
    document.getElementById('ISolarmax').value = state.ISolarmax;
    document.getElementById('ILoadmax').value  = state.ILoadmax;
    document.getElementById('VBatmin').value   = state.VBatmin;
    document.getElementById('VBatmax').value   = state.VBatmax;
    document.getElementById('VSolarmax').value = state.VSolarmax;
  }

  // mode badge + button
  const badge = document.getElementById('mode-badge');
  const modeBtn = document.getElementById('mode-btn');
  if (state.autoMode) {
    badge.textContent = 'AUTO';
    badge.className = 'badge auto';
    modeBtn.textContent = 'Switch to MANUAL';
  } else {
    badge.textContent = 'MANUAL';
    badge.className = 'badge manual';
    modeBtn.textContent = 'Switch to AUTO';
  }

  // manual toggles reflect manualRelayState/manualLoadState from server
  document.getElementById('relay-toggle').checked = !!state.manualRelayState;
  document.getElementById('load-toggle').checked  = !!state.manualLoadState;
}

async function saveSettings() {
  const params = new URLSearchParams();
  params.append('ISolarmax', document.getElementById('ISolarmax').value);
  params.append('ILoadmax',  document.getElementById('ILoadmax').value);
  params.append('VBatmin',   document.getElementById('VBatmin').value);
  params.append('VBatmax',   document.getElementById('VBatmax').value);
  params.append('VSolarmax', document.getElementById('VSolarmax').value);

  try {
    await fetch('/api/settings', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: params.toString()
    });
    fetchState(); // refresh
  } catch (e) {
    console.log(e);
  }
}

async function toggleMode() {
  const newAuto = !(state.autoMode); // flip
  const params = new URLSearchParams();
  params.append('autoMode', newAuto ? '1' : '0');
  params.append('relay', document.getElementById('relay-toggle').checked ? '1' : '0');
  params.append('load',  document.getElementById('load-toggle').checked ? '1' : '0');

  try {
    await fetch('/api/mode', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: params.toString()
    });
    fetchState();
  } catch (e) {
    console.log(e);
  }
}

async function sendControl() {
  // Only meaningful in MANUAL, but we send anyway
  const params = new URLSearchParams();
  params.append('autoMode', state.autoMode ? '1' : '0');
  params.append('relay', document.getElementById('relay-toggle').checked ? '1' : '0');
  params.append('load',  document.getElementById('load-toggle').checked ? '1' : '0');

  try {
    await fetch('/api/mode', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: params.toString()
    });
    fetchState();
  } catch (e) {
    console.log(e);
  }
}

setInterval(fetchState, 1000);
fetchState();
</script>
</body>
</html>
)====";



// ===== Web server setup =====
void setupWebServer() {
  // Root page
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", MAIN_page);
  });

  // State API (JSON)
  server.on("/api/state", HTTP_GET, []() {
    String json = "{";
    json += "\"Vsolar\":" + String(Vsolar, 2) + ",";
    json += "\"Isolar\":" + String(Isolar, 2) + ",";
    json += "\"Vbat\":"   + String(Vbat,   2) + ",";
    json += "\"Ibat\":"   + String(Ibat,   2) + ",";
    json += "\"powerIn\":"  + String(powerIn, 1) + ",";
    json += "\"powerOut\":" + String(powerOut,1) + ",";
    json += "\"isCharging\":"   + String(isCharging ? "true" : "false") + ",";
    json += "\"isLoadActive\":" + String(isLoadActive ? "true" : "false") + ",";
    json += "\"systemStatus\":" + String(systemStatus ? "true" : "false") + ",";
    json += "\"batteryLevel\":" + String(batteryLevel) + ",";
    json += "\"loadPercentage\":" + String(loadPercentage) + ",";
    json += "\"ISolarmax\":" + String(ISolarmax, 3) + ",";
    json += "\"ILoadmax\":"  + String(ILoadmax, 3) + ",";
    json += "\"VBatmin\":"   + String(VBatmin, 3) + ",";
    json += "\"VBatmax\":"   + String(VBatmax, 3) + ",";
    json += "\"VSolarmax\":" + String(VSolarmax, 3) + ",";
    json += "\"autoMode\":"  + String(autoMode ? "true" : "false") + ",";
    json += "\"manualRelayState\":" + String(manualRelayState ? "true" : "false") + ",";
    json += "\"manualLoadState\":"  + String(manualLoadState ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
  });

  // Settings API
  server.on("/api/settings", HTTP_POST, []() {
    if (server.hasArg("ISolarmax")) {
      float v = server.arg("ISolarmax").toFloat();
      if (v > 0.0f) ISolarmax = v;
    }
    if (server.hasArg("ILoadmax")) {
      float v = server.arg("ILoadmax").toFloat();
      if (v > 0.0f) ILoadmax = v;
    }
    if (server.hasArg("VBatmin")) {
      float v = server.arg("VBatmin").toFloat();
      if (v > 0.0f && v < VBatmax) VBatmin = v;
    }
    if (server.hasArg("VBatmax")) {
      float v = server.arg("VBatmax").toFloat();
      if (v > VBatmin) VBatmax = v;
    }
    if (server.hasArg("VSolarmax")) {
      float v = server.arg("VSolarmax").toFloat();
      if (v > 0.0f) VSolarmax = v;
    }
    server.send(200, "text/plain", "OK");
  });

  // Mode + manual control API
  server.on("/api/mode", HTTP_POST, []() {
    if (server.hasArg("autoMode")) {
      String v = server.arg("autoMode");
      autoMode = (v == "1" || v == "true" || v == "TRUE");
    }
    if (server.hasArg("relay")) {
      String v = server.arg("relay");
      manualRelayState = (v == "1" || v == "true" || v == "TRUE");
    }
    if (server.hasArg("load")) {
      String v = server.arg("load");
      manualLoadState = (v == "1" || v == "true" || v == "TRUE");
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println(F("HTTP server started"));
}

// ===== AUTO LOGIC (simple threshold logic, can be replaced by MPPT) =====
void handleLogicAuto() {
  isCharging   = false;
  isLoadActive = false;
  systemStatus = true;

  float Iload = Ibat;   // same shunt used for battery/load

  if (Vsolar > Vbat && Vsolar < VSolarmax) {
    // Suitable for charging
    controls.setRelayActive();
    isCharging = true;

    if (Isolar > ISolarmax) {
      errorAndRestartDisplay("Solar overcurrent");
    }

    if (Vbat > VBatmin && Vbat < VBatmax) {
      controls.activateLoad();
      isLoadActive = true;

      if (Iload > ILoadmax) {
        errorAndRestartDisplay("Load overcurrent");
      }

    } else {
      controls.deactivateLoad();
      isLoadActive = false;

      if (Vbat < VBatmin) {
        errorAndRestartDisplay("Low battery");
      } else {
        systemStatus = false; // high battery, no load
        Serial.println(F("Warning: Vbat >= VBatmax, load disabled (AUTO)."));
      }
    }

  } else {
    // No charging
    controls.setRelayInactive();
    isCharging = false;

    if (Vsolar >= VSolarmax) {
      errorAndRestartDisplay("Solar overvoltage");
    }

    if (Vbat > VBatmin && Vbat < VBatmax) {
      controls.activateLoad();
      isLoadActive = true;

      if (Iload > ILoadmax) {
        errorAndRestartDisplay("Load overcurrent");
      }

    } else {
      controls.deactivateLoad();
      isLoadActive = false;

      if (Vbat < VBatmin) {
        errorAndRestartDisplay("Low battery");
      } else {
        systemStatus = false;
        Serial.println(F("Warning: Vbat >= VBatmax, load disabled (AUTO)."));
      }
    }
  }
}

// ===== MANUAL LOGIC =====
// We still enforce HARD protections (overcurrent / undervoltage / solar overvoltage)
void handleLogicManual() {
  // Apply manual states
  if (manualRelayState) {
    controls.setRelayActive();
    isCharging = true;
  } else {
    controls.setRelayInactive();
    isCharging = false;
  }

  if (manualLoadState) {
    controls.activateLoad();
    isLoadActive = true;
  } else {
    controls.deactivateLoad();
    isLoadActive = false;
  }

  systemStatus = true;

  float Iload = Ibat;

  // HARD protections
  if (Isolar > ISolarmax) {
    errorAndRestartDisplay("Solar overcurrent (MANUAL)");
  }
  if (Iload > ILoadmax) {
    errorAndRestartDisplay("Load overcurrent (MANUAL)");
  }
  if (Vsolar >= VSolarmax) {
    errorAndRestartDisplay("Solar overvoltage (MANUAL)");
  }
  if (Vbat < VBatmin) {
    errorAndRestartDisplay("Low battery (MANUAL)");
  }
}

// ===== SETUP & LOOP =====
void setup() {
  Serial.begin(9600);

  pinMode(redLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);

  digitalWrite(redLedPin, LOW);
  digitalWrite(blueLedPin, LOW);

  initializeDisplay();
  controls.initializePins();

  controls.setRelayInactive();
  controls.deactivateLoad();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("Connecting to WiFi"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print(F("Connected, IP: "));
  Serial.println(WiFi.localIP());

  printInitialDisplay(WiFi.localIP(), WIFI_SSID);
  delay(8000);
  initDisplayLayout();
  setupWebServer();

  // Indicate OK
  digitalWrite(redLedPin, LOW);
  digitalWrite(blueLedPin, HIGH);
}

void loop() {
  server.handleClient();   // handle HTTP

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis < interval) {
    return;
  }
  previousMillis = currentMillis;

  // 1. Read sensors
  Vsolar = viSensorsInput.readVoltage();
  Isolar = viSensorsInput.readCurrent();
  Vbat   = viSensorsOutput.readVoltage();
  Ibat   = viSensorsOutput.readCurrent();


  powerIn  = Vsolar * Isolar;
  powerOut = Vbat   * Ibat;

  // 2. Run logic (auto or manual)
  if (autoMode) {
    handleLogicAuto();
  } else {
    handleLogicManual();
  }

  // 3. Calculate levels for display (0–100%)
  // Battery %
  if (VBatmax > VBatmin) {
    float ratio = (Vbat - VBatmin) / (VBatmax - VBatmin);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    batteryLevel = (int)(ratio * 100.0f + 0.5f);
  } else {
    batteryLevel = 0;
  }

  // Load current %
  if (ILoadmax > 0.0f) {
    float r = fabsf(Ibat) / ILoadmax;
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    loadPercentage = (int)(r * 100.0f + 0.5f);
  } else {
    loadPercentage = 0;
  }

  // 4. Update LCD
  printToDisplay(
    Vsolar, Isolar, powerIn,
    Vbat,   isCharging, batteryLevel,
    powerOut, isLoadActive, systemStatus
  );
}
