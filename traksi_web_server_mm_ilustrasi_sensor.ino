/*
   ESP32 WiFi Access Point Traction Monitor - ENHANCED VERSION
   - Added mm conversion for position display
   - Fixed motor pinggang position interpretation (always positive)
   - Motor orientations handled correctly
   - Improved web interface with mm units
*/

#include <WiFi.h>
#include <WebServer.h>
#include <AccelStepper.h>
#include "ACS712.h"
#include "HX711.h"
#include <ArduinoJson.h>

// --- WIFI ACCESS POINT CONFIGURATION ---
const char* ssid = "TraksiMonitor";
const char* password = "12345678";

// --- MOTOR CALIBRATION CONSTANTS ---
// Konfigurasi motor - sesuaikan dengan spesifikasi mekanik Anda
const float STEPS_PER_MM_KEPALA = 84.8;     // Steps per mm untuk motor kepala
const float STEPS_PER_MM_PINGGANG = 84.8;   // Steps per mm untuk motor pinggang
const float MAX_TRAVEL_MM_KEPALA = 200.0;   // Max travel dalam mm untuk kepala
const float MAX_TRAVEL_MM_PINGGANG = 150.0; // Max travel dalam mm untuk pinggang

// --- PIN DEFINITIONS ---
const int LED_PIN = 2;

// Stepper Motor Kepala (HBS57)
const int dirPin_Kepala = 33;
const int stepPin_Kepala = 32;
const int enablePin_Kepala = 25;

// Stepper Motor Pinggang (HBS57)
const int dirSpPin = 27;
const int stepSpPin = 26;
const int enableSpPin = 14;

// Limit Switch
const int LS_K = 13;
const int LS_P = 15;

// Remote Control Pins
const int REMOTE_LEFT = 4;
const int REMOTE_RIGHT = 21;

// HX711 Load Cell Sensors
const int HX711_DT_1 = 18;
const int HX711_SCK_1 = 19;
const int HX711_DT_2 = 22;
const int HX711_SCK_2 = 23;

// ACS712 Current Sensors
const int pinACS712 = 35;
const int pinACS712_2 = 34;
const float VCC = 3.3;
const float ACS712_MV_PER_AMPERE = 100;
float ZERO_POINT_1 = 1.65;
float ZERO_POINT_2 = 1.65;

// --- OBJECT INITIALIZATION ---
WebServer server(80);
AccelStepper stepperKepala(AccelStepper::DRIVER, stepPin_Kepala, dirPin_Kepala);
AccelStepper stepperPinggang(AccelStepper::DRIVER, stepSpPin, dirSpPin);
HX711 scale1;
HX711 scale2;

// --- CALIBRATION FACTORS ---
float calibration_factor = 2280.0;
float calibration_factor2 = -2280.0;
const float LOAD_CELL_1_TO_KG = 1.0;
const float LOAD_CELL_2_TO_KG = 1.0;

// --- SYSTEM VARIABLES ---
bool systemReady = false;
bool homingKepalaDone = false;
bool homingPinggangDone = false;
bool autoHomingDone = false;
bool currentSensorsCalibrated = false;
bool loadCellsCalibrated = false;
bool hx711InitSuccess = false;

// --- POSITION TRACKING VARIABLES ---
long homePositionKepala = 0;      // Posisi home untuk kepala
long homePositionPinggang = 0;    // Posisi home untuk pinggang

// --- REMOTE CONTROL VARIABLES ---
enum MotorTarget { MOTOR_PINGGANG, MOTOR_KEPALA };
MotorTarget currentMotorTarget = MOTOR_PINGGANG;
bool remoteControlEnabled = true;
const int REMOTE_STEP_SIZE = 10;  // dalam steps
const int REMOTE_SPEED = 800;
unsigned long lastRemoteLeftPress = 0;
unsigned long lastRemoteRightPress = 0;
const unsigned long DEBOUNCE_DELAY = 200;

// --- MOTOR STATUS ---
bool isMotorKepalaBusy = false;
bool isMotorPinggangBusy = false;

// --- OPTIMIZED WEB SERVER VARIABLES ---
unsigned long lastDataUpdate = 0;
const unsigned long DATA_UPDATE_INTERVAL = 50;
TaskHandle_t webServerTask;
TaskHandle_t sensorTask;

// --- SENSOR DATA CACHE ---
struct SensorData {
  float current1;
  float current2;
  float load1;
  float load2;
  long posK;
  long posP;
  bool lsK;
  bool lsP;
  unsigned long timestamp;
};

// Variabel untuk simulasi sensor
struct SensorSimulation {
  float baseCurrent1 = 0.1;      // Base current saat idle (A)
  float baseCurrent2 = 0.1;      // Base current saat idle (A)
  float baseLoad1 = 50.0;        // Base load saat posisi 0 (gram)
  float baseLoad2 = 30.0;        // Base load saat posisi 0 (gram)
  float currentMultiplier = 0.02; // Multiplier untuk arus berdasarkan kecepatan motor
  float loadMultiplier = 137.7;   // Multiplier untuk beban berdasarkan posisi (gram per mm)
  float noiseAmplitude = 0.05;   // Amplitudo noise untuk realisme
  unsigned long lastNoiseUpdate = 0;
  float noiseOffset1 = 0;
  float noiseOffset2 = 0;
} sensorSim;

SensorData cachedData = {0};
SemaphoreHandle_t dataMutex;

// === POSITION CONVERSION FUNCTIONS ===

float convertKepaladStepsToMM(long steps) {
  // Motor kepala: posisi positif = menjauhi home/limit switch
  return (float)steps / STEPS_PER_MM_KEPALA;
}

long convertKepalaMMToSteps(float mm) {
  return (long)(mm * STEPS_PER_MM_KEPALA);
}

float convertPinggangStepsToMM(long steps) {
  // Motor pinggang: orientasi terbalik, jadi kita perlu membalik interpretasi
  // Limit switch = posisi 0mm, bergerak menjauh dari limit switch = positif
  // Karena motor terbalik, steps negatif sebenarnya adalah gerakan menjauhi limit switch
  return (float)(-steps) / STEPS_PER_MM_PINGGANG;
}

long convertPinggangMMToSteps(float mm) {
  // Konversi balik: mm positif menjadi steps negatif karena orientasi motor
  return (long)(-mm * STEPS_PER_MM_PINGGANG);
}

float getKepalaPositionMM() {
  return convertKepaladStepsToMM(stepperKepala.currentPosition() - homePositionKepala);
}

float getPinggangPositionMM() {
  return convertPinggangStepsToMM(stepperPinggang.currentPosition() - homePositionPinggang);
}

float getKepalaTargetMM() {
  return convertKepaladStepsToMM(stepperKepala.targetPosition() - homePositionKepala);
}

float getPinggangTargetMM() {
  return convertPinggangStepsToMM(stepperPinggang.targetPosition() - homePositionPinggang);
}

// === ENHANCED WEB INTERFACE ===

String getOptimizedDashboardHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Medical Traction Monitor - Enhanced</title>
    <style>
        :root {
            --primary-blue: #1e3a8a;
            --secondary-blue: #3b82f6;
            --success-green: #10b981;
            --warning-amber: #f59e0b;
            --danger-red: #ef4444;
            --medical-teal: #0891b2;
            --light-gray: #f8fafc;
            --medium-gray: #e2e8f0;
            --dark-gray: #475569;
            --white: #ffffff;
            --shadow: 0 2px 4px -1px rgba(0, 0, 0, 0.1);
            --shadow-lg: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Arial', sans-serif;
            background: linear-gradient(135deg, var(--primary-blue) 0%, var(--medical-teal) 100%);
            width: 100vw;
            height: 100vh;
            overflow: hidden;
            font-size: clamp(10px, 1.5vw, 14px);
            color: var(--dark-gray);
        }

        .main-container {
            width: 100%;
            height: 100%;
            background: var(--white);
            display: grid;
            grid-template-rows: auto 1fr;
            box-shadow: var(--shadow-lg);
        }

        .header {
            background: linear-gradient(90deg, var(--primary-blue), var(--secondary-blue));
            color: white;
            padding: clamp(4px, 1vh, 8px) clamp(8px, 2vw, 16px);
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 2px solid var(--medical-teal);
            min-height: clamp(35px, 8vh, 50px);
        }

        .header-left h1 {
            font-size: clamp(12px, 2.5vw, 18px);
            font-weight: 700;
            margin-bottom: 1px;
        }

        .header-left p {
            font-size: clamp(8px, 1.5vw, 12px);
            opacity: 0.9;
        }

        .connection-status {
            display: flex;
            align-items: center;
            gap: 6px;
            background: rgba(255,255,255,0.15);
            padding: 4px 8px;
            border-radius: 12px;
            font-size: clamp(8px, 1.2vw, 12px);
            font-weight: 600;
        }

        .status-dot {
            width: clamp(6px, 1vw, 10px);
            height: clamp(6px, 1vw, 10px);
            border-radius: 50%;
            animation: pulse 2s infinite;
        }

        .status-dot.connected { background: var(--success-green); }
        .status-dot.disconnected { background: var(--danger-red); }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        .content {
            display: grid;
            grid-template-columns: 1.3fr 1fr;
            grid-template-rows: 1fr auto;
            gap: clamp(3px, 0.5vw, 8px);
            padding: clamp(3px, 0.5vw, 8px);
            height: calc(100vh - clamp(35px, 8vh, 50px));
            overflow: hidden;
        }

        .left-panel {
            display: grid;
            grid-template-rows: 1fr 1fr;
            gap: clamp(3px, 0.5vw, 8px);
        }

        .chart-card {
            background: var(--white);
            border-radius: clamp(4px, 0.8vw, 8px);
            padding: clamp(6px, 1vw, 12px);
            border: 1px solid var(--medium-gray);
            box-shadow: var(--shadow);
            display: flex;
            flex-direction: column;
            min-height: 0;
        }

        .chart-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: clamp(4px, 0.8vw, 8px);
            flex-shrink: 0;
        }

        .chart-title {
            font-size: clamp(10px, 1.8vw, 14px);
            font-weight: 600;
            color: var(--primary-blue);
            display: flex;
            align-items: center;
            gap: 6px;
        }

        .chart-icon {
            width: clamp(14px, 2.5vw, 20px);
            height: clamp(14px, 2.5vw, 20px);
            background: linear-gradient(45deg, var(--secondary-blue), var(--medical-teal));
            border-radius: 3px;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-size: clamp(8px, 1.5vw, 12px);
        }

        .chart-container {
            flex: 1;
            position: relative;
            min-height: 0;
            height: 100%;
        }

        .simple-chart {
            width: 100%;
            height: 100%;
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 4px;
            position: relative;
            overflow: hidden;
        }

        .right-panel {
            display: grid;
            grid-template-rows: auto 1fr;
            gap: clamp(3px, 0.5vw, 8px);
        }

        .motor-status-section {
            display: grid;
            grid-template-rows: 1fr 1fr;
            gap: clamp(3px, 0.5vw, 6px);
        }

        .motor-card {
            background: var(--white);
            border-radius: clamp(4px, 0.8vw, 8px);
            padding: clamp(4px, 0.8vw, 8px);
            border-left: 3px solid var(--secondary-blue);
            box-shadow: var(--shadow);
        }

        .motor-card.head { border-left-color: var(--danger-red); }
        .motor-card.waist { border-left-color: var(--success-green); }

        .motor-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: clamp(3px, 0.6vw, 6px);
        }

        .motor-title {
            font-size: clamp(9px, 1.6vw, 12px);
            font-weight: 600;
            color: var(--primary-blue);
            display: flex;
            align-items: center;
            gap: 4px;
        }

        .motor-icon {
            width: clamp(8px, 1.5vw, 12px);
            height: clamp(8px, 1.5vw, 12px);
            border-radius: 50%;
        }

        .motor-icon.head { background: var(--danger-red); }
        .motor-icon.waist { background: var(--success-green); }

        .motor-status-grid {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: clamp(2px, 0.5vw, 4px);
        }

        .status-item {
            background: var(--light-gray);
            padding: clamp(3px, 0.6vw, 6px);
            border-radius: 4px;
            text-align: center;
        }

        .status-label {
            font-size: clamp(7px, 1.2vw, 10px);
            color: var(--dark-gray);
            margin-bottom: 2px;
            text-transform: uppercase;
        }

        .status-value {
            font-size: clamp(8px, 1.4vw, 11px);
            font-weight: 700;
            color: var(--primary-blue);
        }

        .status-indicator {
            display: inline-flex;
            align-items: center;
            gap: 3px;
            font-size: clamp(7px, 1.3vw, 10px);
            font-weight: 600;
        }

        .indicator-dot {
            width: clamp(4px, 0.8vw, 6px);
            height: clamp(4px, 0.8vw, 6px);
            border-radius: 50%;
        }

        .indicator-dot.active { background: var(--success-green); }
        .indicator-dot.inactive { background: var(--danger-red); }
        .indicator-dot.warning { background: var(--warning-amber); }

        .control-section {
            display: grid;
            grid-template-rows: auto 1fr;
            gap: clamp(3px, 0.6vw, 6px);
        }

        .control-panel {
            background: var(--white);
            border-radius: clamp(4px, 0.8vw, 8px);
            padding: clamp(4px, 0.8vw, 8px);
            box-shadow: var(--shadow);
            border: 1px solid var(--medium-gray);
        }

        .control-title {
            font-size: clamp(8px, 1.5vw, 11px);
            font-weight: 600;
            color: var(--primary-blue);
            margin-bottom: clamp(3px, 0.6vw, 6px);
        }

        .target-selector {
            background: var(--light-gray);
            border-radius: 4px;
            padding: clamp(3px, 0.6vw, 6px);
            margin-bottom: clamp(3px, 0.6vw, 6px);
        }

        .target-label {
            font-size: clamp(7px, 1.3vw, 10px);
            color: var(--dark-gray);
            margin-bottom: 3px;
            font-weight: 600;
        }

        .radio-group {
            display: flex;
            gap: clamp(4px, 1vw, 8px);
        }

        .radio-item {
            display: flex;
            align-items: center;
            gap: 3px;
            cursor: pointer;
            font-size: clamp(7px, 1.3vw, 10px);
            font-weight: 500;
        }

        .radio-item input[type="radio"] {
            width: clamp(8px, 1.5vw, 12px);
            height: clamp(8px, 1.5vw, 12px);
            accent-color: var(--secondary-blue);
        }

        .control-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: clamp(2px, 0.5vw, 4px);
        }

        .move-controls {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: clamp(2px, 0.4vw, 3px);
            margin: clamp(3px, 0.6vw, 6px) 0;
        }

        .btn {
            padding: clamp(3px, 0.8vw, 6px) clamp(4px, 1vw, 8px);
            border: none;
            border-radius: 4px;
            font-size: clamp(7px, 1.3vw, 10px);
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s ease;
            text-transform: uppercase;
            min-height: clamp(24px, 4vh, 32px);
        }

        .btn:hover:not(:disabled) {
            transform: translateY(-1px);
            box-shadow: var(--shadow);
        }

        .btn:active:not(:disabled) {
            transform: translateY(0);
        }

        .btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }

        .btn-primary { background: var(--secondary-blue); color: white; }
        .btn-success { background: var(--success-green); color: white; }
        .btn-warning { background: var(--warning-amber); color: white; }
        .btn-danger { background: var(--danger-red); color: white; }
        .btn-move { background: var(--medical-teal); color: white; font-size: clamp(6px, 1.1vw, 9px); }

        .position-display {
            background: var(--light-gray);
            border-radius: 4px;
            padding: clamp(3px, 0.6vw, 6px);
            margin: clamp(3px, 0.6vw, 6px) 0;
            text-align: center;
        }

        .position-label {
            font-size: clamp(7px, 1.2vw, 10px);
            color: var(--dark-gray);
            margin-bottom: 2px;
        }

        .position-value {
            font-size: clamp(10px, 1.8vw, 14px);
            font-weight: 700;
            color: var(--primary-blue);
        }

        .position-unit {
            font-size: clamp(8px, 1.4vw, 11px);
            color: var(--medical-teal);
            font-weight: 600;
        }

        .sensor-panel {
            display: grid;
            grid-template-rows: auto 1fr;
        }

        .sensor-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            grid-template-rows: 1fr 1fr;
            gap: clamp(2px, 0.5vw, 4px);
        }

        .sensor-card {
            background: var(--light-gray);
            border-radius: 4px;
            padding: clamp(3px, 0.6vw, 6px);
            text-align: center;
            border: 1px solid transparent;
            transition: all 0.2s ease;
        }

        .sensor-card.updating {
            border-color: var(--success-green);
            background: rgba(16, 185, 129, 0.1);
        }

        .sensor-value {
            font-size: clamp(8px, 1.4vw, 11px);
            font-weight: 700;
            color: var(--primary-blue);
            margin-bottom: 1px;
        }

        .sensor-label {
            font-size: clamp(6px, 1.1vw, 9px);
            color: var(--dark-gray);
            text-transform: uppercase;
        }

        .sensor-unit {
            font-size: clamp(6px, 1.1vw, 9px);
            color: var(--medical-teal);
            font-weight: 600;
        }

        .log-section {
            grid-column: 1 / -1;
            background: var(--white);
            border-radius: clamp(4px, 0.8vw, 8px);
            padding: clamp(4px, 0.8vw, 8px);
            box-shadow: var(--shadow);
            border: 1px solid var(--medium-gray);
            display: flex;
            flex-direction: column;
            max-height: clamp(60px, 15vh, 100px);
        }

        .log-title {
            font-size: clamp(8px, 1.5vw, 11px);
            font-weight: 600;
            color: var(--primary-blue);
            margin-bottom: clamp(3px, 0.6vw, 6px);
        }

        .log-container {
            background: #0f172a;
            color: #00ff88;
            padding: clamp(3px, 0.6vw, 6px);
            border-radius: 4px;
            font-family: 'Courier New', monospace;
            font-size: clamp(6px, 1.2vw, 9px);
            flex: 1;
            overflow-y: auto;
            line-height: 1.2;
        }

        .log-container::-webkit-scrollbar { width: 3px; }
        .log-container::-webkit-scrollbar-track { background: rgba(255,255,255,0.1); }
        .log-container::-webkit-scrollbar-thumb { background: var(--medical-teal); }

        .log-entry {
            margin-bottom: 1px;
            word-wrap: break-word;
        }

        .log-timestamp { color: #64748b; }

        .debug-info {
            position: fixed;
            top: 10px;
            right: 10px;
            background: rgba(0,0,0,0.8);
            color: white;
            padding: 5px;
            border-radius: 3px;
            font-size: clamp(7px, 1.2vw, 9px);
            z-index: 1000;
            display: none;
        }

        @media (max-width: 900px) {
            .content {
                grid-template-columns: 1fr;
                grid-template-rows: 1fr 1fr auto;
            }
            
            .left-panel {
                grid-template-columns: 1fr 1fr;
                grid-template-rows: 1fr;
            }
        }

        @media (max-height: 600px) {
            .log-section {
                max-height: clamp(40px, 12vh, 60px);
            }
        }

        .error-state {
            color: var(--danger-red);
            font-weight: 600;
        }

        .loading-state {
            color: var(--warning-amber);
            font-style: italic;
        }

        .fps-counter {
            position: fixed;
            top: 10px;
            left: 10px;
            background: rgba(0,0,0,0.7);
            color: white;
            padding: 2px 6px;
            border-radius: 3px;
            font-size: 10px;
            z-index: 1000;
        }

        .mm-indicator {
            background: var(--success-green);
            color: white;
            padding: 1px 4px;
            border-radius: 2px;
            font-size: clamp(6px, 1vw, 8px);
            font-weight: 600;
            margin-left: 3px;
        }
    </style>
</head>
<body>
    <div class="fps-counter" id="fps-counter">FPS: 0</div>
    
    <div class="debug-info" id="debug-info">
        Connection: <span id="debug-connection">Unknown</span><br>
        Last Update: <span id="debug-update">Never</span><br>
        Data Status: <span id="debug-data">No Data</span><br>
        Error Count: <span id="debug-errors">0</span><br>
        Update Rate: <span id="debug-rate">0</span> Hz
    </div>

    <div class="main-container">
        <header class="header">
            <div class="header-left">
                <h1>Medical Traction Monitor</h1>
                <p>Enhanced with mm Position Display</p>
            </div>
            <div class="connection-status">
                <span id="connection-dot" class="status-dot connected"></span>
                <span id="connection-text">Connecting...</span>
            </div>
        </header>

        <main class="content">
            <div class="left-panel">
                <div class="chart-card">
                    <div class="chart-header">
                        <h2 class="chart-title"><span class="chart-icon">📊</span> Force (Kg)</h2>
                    </div>
                    <div class="chart-container">
                        <div id="forceChart" class="simple-chart"></div>
                    </div>
                </div>
                <div class="chart-card">
                    <div class="chart-header">
                        <h2 class="chart-title"><span class="chart-icon">⚡</span> Current (A)</h2>
                    </div>
                    <div class="chart-container">
                        <div id="currentChart" class="simple-chart"></div>
                    </div>
                </div>
            </div>

            <div class="right-panel">
                <div class="motor-status-section">
                    <div class="motor-card head">
                        <div class="motor-header">
                            <h3 class="motor-title"><span class="motor-icon head"></span> Motor Kepala</h3>
                            <span id="motor-kepala-status" class="status-indicator">
                                <span class="indicator-dot inactive"></span> Idle
                            </span>
                        </div>
                        <div class="motor-status-grid">
                            <div class="status-item">
                                <div class="status-label">Posisi</div>
                                <div id="pos-kepala" class="status-value">- <span class="mm-indicator">mm</span></div>
                            </div>
                            <div class="status-item">
                                <div class="status-label">Target</div>
                                <div id="target-kepala" class="status-value">- <span class="mm-indicator">mm</span></div>
                            </div>
                            <div class="status-item">
                                <div class="status-label">Limit SW</div>
                                <div id="ls-kepala" class="status-value">-</div>
                            </div>
                        </div>
                    </div>
                    <div class="motor-card waist">
                        <div class="motor-header">
                            <h3 class="motor-title"><span class="motor-icon waist"></span> Motor Pinggang</h3>
                            <span id="motor-pinggang-status" class="status-indicator">
                                <span class="indicator-dot inactive"></span> Idle
                            </span>
                        </div>
                        <div class="motor-status-grid">
                            <div class="status-item">
                                <div class="status-label">Posisi</div>
                                <div id="pos-pinggang" class="status-value">- <span class="mm-indicator">mm</span></div>
                            </div>
                            <div class="status-item">
                                <div class="status-label">Target</div>
                                <div id="target-pinggang" class="status-value">- <span class="mm-indicator">mm</span></div>
                            </div>
                            <div class="status-item">
                                <div class="status-label">Limit SW</div>
                                <div id="ls-pinggang" class="status-value">-</div>
                            </div>
                        </div>
                    </div>
                </div>

                <div class="control-section">
                    <div class="control-panel">
                        <h3 class="control-title">Motor Control</h3>
                        <div class="target-selector">
                            <div class="target-label">Pilih Motor:</div>
                            <div class="radio-group">
                                <label class="radio-item">
                                    <input type="radio" name="motorTarget" value="KEPALA"> Kepala
                                </label>
                                <label class="radio-item">
                                    <input type="radio" name="motorTarget" value="PINGGANG" checked> Pinggang
                                </label>
                            </div>
                        </div>
                        
                        <div class="position-display">
                            <div class="position-label">Posisi Saat Ini</div>
                            <div id="current-position-display" class="position-value">0.0 <span class="position-unit">mm</span></div>
                        </div>
                        
                        <div class="move-controls">
                            <button class="btn btn-move" onclick="sendMoveCommand(-10)" id="btn-move-back">◄ -10mm</button>
                            <button class="btn btn-move" onclick="sendMoveCommand(-1)" id="btn-move-back-small">◄ -1mm</button>
                            <button class="btn btn-move" onclick="sendMoveCommand(1)" id="btn-move-fwd-small">+1mm ►</button>
                            <button class="btn btn-move" onclick="sendMoveCommand(10)" id="btn-move-fwd">+10mm ►</button>
                        </div>
                        
                        <div class="control-buttons">
                            <button class="btn btn-primary" onclick="sendCommand('HOME')" id="btn-home">HOME</button>
                            <button class="btn btn-success" onclick="sendCommand('ENABLE')" id="btn-enable">ENABLE</button>
                            <button class="btn btn-warning" onclick="sendCommand('CALIBRATE')" id="btn-calibrate">CALIB</button>
                            <button class="btn btn-danger" onclick="sendCommand('STOP')" id="btn-stop">STOP</button>
                        </div>
                    </div>
                    <div class="control-panel sensor-panel">
                        <h3 class="control-title">Sensor Readings</h3>
                        <div class="sensor-grid">
                            <div class="sensor-card" id="force1-card">
                                <div id="force1" class="sensor-value loading-state">Loading... <span class="sensor-unit">Kg</span></div>
                                <div class="sensor-label">Force 1</div>
                            </div>
                            <div class="sensor-card" id="force2-card">
                                <div id="force2" class="sensor-value loading-state">Loading... <span class="sensor-unit">Kg</span></div>
                                <div class="sensor-label">Force 2</div>
                            </div>
                            <div class="sensor-card" id="current1-card">
                                <div id="current1" class="sensor-value loading-state">Loading... <span class="sensor-unit">A</span></div>
                                <div class="sensor-label">Current 1</div>
                            </div>
                            <div class="sensor-card" id="current2-card">
                                <div id="current2" class="sensor-value loading-state">Loading... <span class="sensor-unit">A</span></div>
                                <div class="sensor-label">Current 2</div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <div class="log-section">
                <h3 class="log-title">System Log - Enhanced with mm Position</h3>
                <div id="log-container" class="log-container"></div>
            </div>
        </main>
    </div>

    <script>
        // ENHANCED JAVASCRIPT WITH MM CONVERSION
        
        // Global variables
        let isConnected = false;
        let lastDataTime = 0;
        let dataUpdateCount = 0;
        let errorCount = 0;
        let connectionAttempts = 0;
        let updateRate = 0;
        let lastUpdateTime = 0;
        let currentMotorSelection = 'PINGGANG';
        const MAX_CONNECTION_ATTEMPTS = 3;

        // Chart data arrays
        const forceData1 = [];
        const forceData2 = [];
        const currentData1 = [];
        const currentData2 = [];
        const MAX_CHART_POINTS = 50;

        // FPS counter
        let frameCount = 0;
        let lastFpsTime = 0;

        // DOM elements cache
        const DOM = {
            connectionDot: document.getElementById('connection-dot'),
            connectionText: document.getElementById('connection-text'),
            logContainer: document.getElementById('log-container'),
            posKepala: document.getElementById('pos-kepala'),
            targetKepala: document.getElementById('target-kepala'),
            lsKepala: document.getElementById('ls-kepala'),
            motorKepalaStatus: document.getElementById('motor-kepala-status'),
            posPinggang: document.getElementById('pos-pinggang'),
            targetPinggang: document.getElementById('target-pinggang'),
            lsPinggang: document.getElementById('ls-pinggang'),
            motorPinggangStatus: document.getElementById('motor-pinggang-status'),
            force1Display: document.getElementById('force1'),
            force2Display: document.getElementById('force2'),
            current1Display: document.getElementById('current1'),
            current2Display: document.getElementById('current2'),
            force1Card: document.getElementById('force1-card'),
            force2Card: document.getElementById('force2-card'),
            current1Card: document.getElementById('current1-card'),
            current2Card: document.getElementById('current2-card'),
            forceChart: document.getElementById('forceChart'),
            currentChart: document.getElementById('currentChart'),
            currentPositionDisplay: document.getElementById('current-position-display'),
            debugInfo: document.getElementById('debug-info'),
            debugConnection: document.getElementById('debug-connection'),
            debugUpdate: document.getElementById('debug-update'),
            debugData: document.getElementById('debug-data'),
            debugErrors: document.getElementById('debug-errors'),
            debugRate: document.getElementById('debug-rate'),
            fpsCounter: document.getElementById('fps-counter'),
            btnHome: document.getElementById('btn-home'),
            btnEnable: document.getElementById('btn-enable'),
            btnCalibrate: document.getElementById('btn-calibrate'),
            btnStop: document.getElementById('btn-stop')
        };

        // Utility functions
        function safeNumber(value, decimals = 2) {
            if (value === undefined || value === null || isNaN(value)) {
                return '0.00';
            }
            return parseFloat(value).toFixed(decimals);
        }

        function timestamp() {
            return new Date().toLocaleTimeString();
        }

        // Enhanced logging function
        function appendLog(message, type = 'info') {
            const timestampStr = timestamp();
            const logEntry = document.createElement('div');
            logEntry.classList.add('log-entry');
            
            let icon = '';
            let color = '#00ff88';
            switch(type) {
                case 'success': icon = '✓'; color = '#10b981'; break;
                case 'error': icon = '✗'; color = '#ef4444'; errorCount++; break;
                case 'warning': icon = '⚠'; color = '#f59e0b'; break;
                case 'info': default: icon = '•'; color = '#64748b'; break;
            }
            
            logEntry.innerHTML = `<span class="log-timestamp">[${timestampStr}]</span> <span style="color:${color}">${icon} ${message}</span>`;
            
            if (DOM.logContainer) {
                DOM.logContainer.appendChild(logEntry);
                DOM.logContainer.scrollTop = DOM.logContainer.scrollHeight;
                
                // Limit log entries
                while (DOM.logContainer.children.length > 8) {
                    DOM.logContainer.removeChild(DOM.logContainer.firstChild);
                }
            }
            
            // Update debug info
            if (DOM.debugErrors) {
                DOM.debugErrors.textContent = errorCount;
            }
            
            console.log(`[${timestampStr}] ${icon} ${message}`);
        }

        // Enhanced connection status
        function updateConnectionStatus(connected, reason = '') {
            isConnected = connected;
            
            if (DOM.connectionDot && DOM.connectionText) {
                if (connected) {
                    DOM.connectionDot.className = 'status-dot connected';
                    DOM.connectionText.textContent = 'Connected';
                    if (DOM.debugConnection) DOM.debugConnection.textContent = 'OK';
                    connectionAttempts = 0;
                } else {
                    DOM.connectionDot.className = 'status-dot disconnected';
                    DOM.connectionText.textContent = 'Disconnected';
                    if (DOM.debugConnection) DOM.debugConnection.textContent = `ERROR: ${reason}`;
                    connectionAttempts++;
                }
            }
        }

        // Enhanced sensor data update with mm conversion
        function updateSensorData(data) {
            try {
                if (!data) {
                    appendLog('No data received from ESP32', 'warning');
                    return;
                }

                // Motor Kepala - positions in mm
                const posKepalaMM = safeNumber(data.posK_mm, 1);
                const targetKepalaMM = safeNumber(data.targetK_mm, 1);
                if (DOM.posKepala) DOM.posKepala.innerHTML = `${posKepalaMM} <span class="mm-indicator">mm</span>`;
                if (DOM.targetKepala) DOM.targetKepala.innerHTML = `${targetKepalaMM} <span class="mm-indicator">mm</span>`;
                if (DOM.lsKepala) DOM.lsKepala.textContent = data.lsK !== undefined ? (data.lsK === 1 ? 'ON' : 'OFF') : '-';
                updateMotorStatus(DOM.motorKepalaStatus, data.busyK, data.lsK);

                // Motor Pinggang - positions in mm (now always positive)
                const posPinggangMM = safeNumber(data.posP_mm, 1);
                const targetPinggangMM = safeNumber(data.targetP_mm, 1);
                if (DOM.posPinggang) DOM.posPinggang.innerHTML = `${posPinggangMM} <span class="mm-indicator">mm</span>`;
                if (DOM.targetPinggang) DOM.targetPinggang.innerHTML = `${targetPinggangMM} <span class="mm-indicator">mm</span>`;
                if (DOM.lsPinggang) DOM.lsPinggang.textContent = data.lsP !== undefined ? (data.lsP === 1 ? 'ON' : 'OFF') : '-';
                updateMotorStatus(DOM.motorPinggangStatus, data.busyP, data.lsP);

                // Update current position display based on selected motor
                if (DOM.currentPositionDisplay) {
                    const currentPos = currentMotorSelection === 'KEPALA' ? posKepalaMM : posPinggangMM;
                    DOM.currentPositionDisplay.innerHTML = `${currentPos} <span class="position-unit">mm</span>`;
                }

                // Sensor readings with visual feedback
                const force1 = safeNumber(data.load1_kg);
                const force2 = safeNumber(data.load2_kg);
                const current1 = safeNumber(data.current1);
                const current2 = safeNumber(data.current2);

                updateSensorDisplay(DOM.force1Display, DOM.force1Card, force1, 'Kg');
                updateSensorDisplay(DOM.force2Display, DOM.force2Card, force2, 'Kg');
                updateSensorDisplay(DOM.current1Display, DOM.current1Card, current1, 'A');
                updateSensorDisplay(DOM.current2Display, DOM.current2Card, current2, 'A');

                // Update chart data
                updateChartData(parseFloat(force1), parseFloat(force2), parseFloat(current1), parseFloat(current2));

                // Update debug info
                if (DOM.debugData) {
                    DOM.debugData.textContent = `KepMM:${posKepalaMM} PingMM:${posPinggangMM} F1:${force1} F2:${force2}`;
                }
                
            } catch (error) {
                console.error('Error updating sensor data:', error);
                appendLog('Display update error: ' + error.message, 'error');
            }
        }

        // Update sensor display with animation
        function updateSensorDisplay(element, card, value, unit) {
            if (!element || !card) return;
            
            element.innerHTML = `${value} <span class="sensor-unit">${unit}</span>`;
            element.className = 'sensor-value';
            
            // Add updating animation
            card.classList.add('updating');
            setTimeout(() => card.classList.remove('updating'), 200);
        }

        // Enhanced motor status update
        function updateMotorStatus(element, isBusy, limitSwitchStatus) {
            if (!element) return;
            
            const dot = element.querySelector('.indicator-dot');
            let textContent = ' Idle';
            let dotClass = 'indicator-dot inactive';
            
            if (isBusy) {
                dotClass = 'indicator-dot active';
                textContent = ' Moving';
            } else if (limitSwitchStatus === 1) {
                dotClass = 'indicator-dot warning';
                textContent = ' Limit Hit';
            }
            
            if (dot) dot.className = dotClass;
            
            // Update text content safely
            const textNodes = Array.from(element.childNodes).filter(node => 
                node.nodeType === Node.TEXT_NODE && node.textContent.trim()
            );
            
            if (textNodes.length > 0) {
                textNodes[0].textContent = textContent;
            } else {
                element.appendChild(document.createTextNode(textContent));
            }
        }

        // Update chart data arrays and render
        function updateChartData(force1, force2, current1, current2) {
            // Add new data
            forceData1.push(force1);
            forceData2.push(force2);
            currentData1.push(current1);
            currentData2.push(current2);
            
            // Trim old data
            if (forceData1.length > MAX_CHART_POINTS) {
                forceData1.shift();
                forceData2.shift();
                currentData1.shift();
                currentData2.shift();
            }
            
            // Update charts with smooth rendering
            updateSmoothChart(DOM.forceChart, forceData1, forceData2, 'Force 1', 'Force 2', '#10b981', '#ef4444', 50);
            updateSmoothChart(DOM.currentChart, currentData1, currentData2, 'Current 1', 'Current 2', '#3b82f6', '#f59e0b', 10);
        }

        // Smooth SVG chart renderer (same as before)
        function updateSmoothChart(container, data1, data2, label1, label2, color1, color2, maxValue = 100) {
            if (!container || !data1.length) return;
            
            const width = container.offsetWidth;
            const height = container.offsetHeight;
            
            if (width === 0 || height === 0) return;
            
            const points = Math.min(data1.length, MAX_CHART_POINTS);
            const stepX = width / Math.max(points - 1, 1);
            
            // Find max value for scaling
            const allValues = [...data1.slice(-points), ...data2.slice(-points)];
            const maxVal = Math.max(...allValues, maxValue * 0.1);
            const scale = (height - 40) / maxVal;
            
            // Create SVG
            const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
            svg.setAttribute('width', width);
            svg.setAttribute('height', height);
            svg.style.position = 'absolute';
            svg.style.top = '0';
            svg.style.left = '0';
            
            // Create smooth path for data1
            if (data1.length > 1) {
                let path1 = 'M';
                for (let i = 0; i < points; i++) {
                    const x = i * stepX;
                    const y = height - (data1[data1.length - points + i] * scale) - 20;
                    path1 += (i === 0 ? '' : ' L') + x + ',' + y;
                }
                
                const pathElement1 = document.createElementNS('http://www.w3.org/2000/svg', 'path');
                pathElement1.setAttribute('d', path1);
                pathElement1.setAttribute('stroke', color1);
                pathElement1.setAttribute('stroke-width', '2');
                pathElement1.setAttribute('fill', 'none');
                pathElement1.setAttribute('stroke-linecap', 'round');
                pathElement1.setAttribute('stroke-linejoin', 'round');
                svg.appendChild(pathElement1);
            }
            
            // Create smooth path for data2
            if (data2.length > 1) {
                let path2 = 'M';
                for (let i = 0; i < points; i++) {
                    const x = i * stepX;
                    const y = height - (data2[data2.length - points + i] * scale) - 20;
                    path2 += (i === 0 ? '' : ' L') + x + ',' + y;
                }
                
                const pathElement2 = document.createElementNS('http://www.w3.org/2000/svg', 'path');
                pathElement2.setAttribute('d', path2);
                pathElement2.setAttribute('stroke', color2);
                pathElement2.setAttribute('stroke-width', '2');
                pathElement2.setAttribute('fill', 'none');
                pathElement2.setAttribute('stroke-linecap', 'round');
                pathElement2.setAttribute('stroke-linejoin', 'round');
                svg.appendChild(pathElement2);
            }
            
            // Grid lines
            const gridGroup = document.createElementNS('http://www.w3.org/2000/svg', 'g');
            gridGroup.setAttribute('stroke', '#e5e7eb');
            gridGroup.setAttribute('stroke-width', '0.5');
            gridGroup.setAttribute('opacity', '0.5');
            
            // Horizontal grid lines
            for (let i = 0; i <= 4; i++) {
                const y = (height - 40) * i / 4 + 20;
                const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
                line.setAttribute('x1', 0);
                line.setAttribute('y1', y);
                line.setAttribute('x2', width);
                line.setAttribute('y2', y);
                gridGroup.appendChild(line);
            }
            
            svg.appendChild(gridGroup);
            
            // Clear container and add SVG
            container.innerHTML = '';
            container.appendChild(svg);
            
            // Add labels
            const label = document.createElement('div');
            label.style.position = 'absolute';
            label.style.top = '5px';
            label.style.left = '5px';
            label.style.fontSize = '10px';
            label.style.background = 'rgba(255,255,255,0.9)';
            label.style.padding = '3px 6px';
            label.style.borderRadius = '4px';
            label.style.boxShadow = '0 1px 3px rgba(0,0,0,0.1)';
            label.style.zIndex = '10';
            label.innerHTML = `<span style="color:${color1}">●</span> ${label1} <span style="color:${color2}">●</span> ${label2}`;
            container.appendChild(label);
            
            // Add current values
            if (data1.length > 0 && data2.length > 0) {
                const values = document.createElement('div');
                values.style.position = 'absolute';
                values.style.bottom = '5px';
                values.style.right = '5px';
                values.style.fontSize = '9px';
                values.style.background = 'rgba(0,0,0,0.7)';
                values.style.color = 'white';
                values.style.padding = '2px 4px';
                values.style.borderRadius = '3px';
                values.style.zIndex = '10';
                values.innerHTML = `${label1}: ${data1[data1.length-1].toFixed(2)} | ${label2}: ${data2[data2.length-1].toFixed(2)}`;
                container.appendChild(values);
            }
        }

        // Enhanced command sending with mm support
        function sendCommand(cmdType) {
            try {
                const motorTarget = document.querySelector('input[name="motorTarget"]:checked');
                if (!motorTarget) {
                    appendLog('No motor target selected', 'error');
                    return;
                }

                const targetValue = motorTarget.value;
                let commandToSend = '';

                // Show which motor is selected
                appendLog(`Selected motor: ${targetValue}`, 'info');

                // Disable buttons during command
                disableButtons(true);

                switch (cmdType) {
                    case 'HOME':
                        commandToSend = `HOME_${targetValue.charAt(0)}`;
                        break;
                    case 'ENABLE':
                        commandToSend = `ENABLE_${targetValue.charAt(0)}`;
                        break;
                    case 'STOP':
                        commandToSend = `STOP_${targetValue.charAt(0)}`;
                        break;
                    case 'CALIBRATE':
                        commandToSend = 'CALIBRATE';
                        break;
                    default:
                        appendLog(`Unknown command: ${cmdType}`, 'error');
                        disableButtons(false);
                        return;
                }

                appendLog(`Sending: ${commandToSend} to ${targetValue}`, 'info');

                // Create proper form data
                const formData = new FormData();
                formData.append('command', commandToSend);

                fetch('/control', {
                    method: 'POST',
                    body: formData,
                    mode: 'cors'
                })
                .then(response => {
                    console.log('Response status:', response.status);
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                    }
                    return response.text().then(text => {
                        try {
                            return JSON.parse(text);
                        } catch (e) {
                            console.log('Response text:', text);
                            throw new Error('Invalid JSON response: ' + text.substring(0, 100));
                        }
                    });
                })
                .then(data => {
                    disableButtons(false);
                    console.log('Command response:', data);
                    if (data && data.success) {
                        appendLog(`✓ Command executed: ${commandToSend}`, 'success');
                    } else {
                        appendLog(`✗ Command failed: ${data ? data.message || 'Unknown error' : 'No response'}`, 'error');
                    }
                })
                .catch(error => {
                    disableButtons(false);
                    console.error('Command error:', error);
                    appendLog(`✗ Command error: ${error.message}`, 'error');
                    updateConnectionStatus(false, error.message);
                });

            } catch (error) {
                disableButtons(false);
                console.error('Send command error:', error);
                appendLog(`✗ Command preparation error: ${error.message}`, 'error');
            }
        }

        // Enhanced move command with mm support
        function sendMoveCommand(deltaMMValue) {
            try {
                const motorTarget = document.querySelector('input[name="motorTarget"]:checked');
                if (!motorTarget) {
                    appendLog('No motor target selected', 'error');
                    return;
                }

                const targetValue = motorTarget.value;
                appendLog(`Moving ${targetValue} by ${deltaMMValue > 0 ? '+' : ''}${deltaMMValue}mm`, 'info');

                // Disable buttons during command
                disableButtons(true);

                const commandToSend = `MOVE_MM_${targetValue.charAt(0)} ${deltaMMValue}`;
                
                // Create proper form data
                const formData = new FormData();
                formData.append('command', commandToSend);

                fetch('/control', {
                    method: 'POST',
                    body: formData,
                    mode: 'cors'
                })
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                    }
                    return response.json();
                })
                .then(data => {
                    disableButtons(false);
                    if (data && data.success) {
                        appendLog(`✓ Moved ${targetValue} by ${deltaMMValue}mm`, 'success');
                    } else {
                        appendLog(`✗ Move failed: ${data ? data.message || 'Unknown error' : 'No response'}`, 'error');
                    }
                })
                .catch(error => {
                    disableButtons(false);
                    console.error('Move command error:', error);
                    appendLog(`✗ Move error: ${error.message}`, 'error');
                    updateConnectionStatus(false, error.message);
                });

            } catch (error) {
                disableButtons(false);
                console.error('Send move command error:', error);
                appendLog(`✗ Move preparation error: ${error.message}`, 'error');
            }
        }

        // Button state management
        function disableButtons(disabled) {
            const buttons = [DOM.btnHome, DOM.btnEnable, DOM.btnCalibrate, DOM.btnStop];
            const moveButtons = document.querySelectorAll('.btn-move');
            
            buttons.forEach(btn => {
                if (btn) btn.disabled = disabled;
            });
            
            moveButtons.forEach(btn => {
                btn.disabled = disabled;
            });
        }

        // Optimized data fetching
        function fetchData() {
            const startTime = Date.now();
            
            fetch('/data', {
                method: 'GET',
                headers: {
                    'Cache-Control': 'no-cache',
                    'Pragma': 'no-cache'
                }
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                }
                return response.json();
            })
            .then(data => {
                const responseTime = Date.now() - startTime;
                lastDataTime = Date.now();
                dataUpdateCount++;
                
                // Calculate update rate
                const now = Date.now();
                if (lastUpdateTime > 0) {
                    const deltaTime = (now - lastUpdateTime) / 1000;
                    updateRate = Math.round(1 / deltaTime * 10) / 10;
                }
                lastUpdateTime = now;
                
                // Update connection status
                updateConnectionStatus(true);
                
                // Update displays
                updateSensorData(data);
                
                // Update debug info
                if (DOM.debugUpdate) {
                    DOM.debugUpdate.textContent = timestamp() + ` (${responseTime}ms)`;
                }
                if (DOM.debugRate) {
                    DOM.debugRate.textContent = updateRate;
                }
                
                // Log successful updates occasionally
                if (dataUpdateCount === 1 || dataUpdateCount % 50 === 0) {
                    appendLog(`Data update #${dataUpdateCount} - ${responseTime}ms - ${updateRate}Hz`, 'success');
                }
            })
            .catch(error => {
                console.error('Data fetch error:', error);
                updateConnectionStatus(false, error.message);
                
                // Log error occasionally to avoid spam
                if (dataUpdateCount % 20 === 0 || connectionAttempts <= MAX_CONNECTION_ATTEMPTS) {
                    appendLog(`Data fetch failed: ${error.message}`, 'error');
                }
                
                // Show loading state in sensors
                showLoadingState();
            });
        }

        // Show loading state for sensors
        function showLoadingState() {
            const sensors = [DOM.force1Display, DOM.force2Display, DOM.current1Display, DOM.current2Display];
            sensors.forEach(sensor => {
                if (sensor && sensor.className !== 'sensor-value loading-state') {
                    sensor.className = 'sensor-value loading-state';
                    sensor.innerHTML = sensor.innerHTML.replace(/[\d.-]+/, 'Error');
                }
            });
        }

        // Connection monitoring with automatic retry
        function monitorConnection() {
            const timeSinceLastData = Date.now() - lastDataTime;
            
            if (timeSinceLastData > 3000 && isConnected) {
                updateConnectionStatus(false, 'Timeout');
                appendLog('Connection timeout detected', 'warning');
            }
            
            // Auto-retry connection
            if (timeSinceLastData > 5000 && connectionAttempts < MAX_CONNECTION_ATTEMPTS) {
                appendLog(`Reconnection attempt ${connectionAttempts + 1}/${MAX_CONNECTION_ATTEMPTS}`, 'info');
                fetchData();
            }
        }

        // FPS counter
        function updateFPS() {
            frameCount++;
            const now = Date.now();
            if (now - lastFpsTime >= 1000) {
                const fps = Math.round(frameCount * 1000 / (now - lastFpsTime));
                if (DOM.fpsCounter) {
                    DOM.fpsCounter.textContent = `FPS: ${fps}`;
                }
                frameCount = 0;
                lastFpsTime = now;
            }
        }

        // Emergency stop with visual feedback
        function emergencyStop() {
            appendLog('EMERGENCY STOP ACTIVATED!', 'warning');
            
            // Visual feedback
            document.body.style.backgroundColor = '#dc2626';
            setTimeout(() => {
                document.body.style.backgroundColor = '';
            }, 500);
            
            // Send stop commands to both motors
            const commands = ['STOP_K', 'STOP_P'];
            commands.forEach(cmd => {
                const params = new URLSearchParams();
                params.append('command', cmd);
                fetch('/control', {
                    method: 'POST',
                    body: params,
                    headers: {
                        'Content-Type': 'application/x-www-form-urlencoded'
                    }
                }).catch(error => console.error('Emergency stop error:', error));
            });
        }

        // Enhanced keyboard shortcuts
        document.addEventListener('keydown', function(event) {
            switch(event.key.toLowerCase()) {
                case 'escape':
                    event.preventDefault();
                    emergencyStop();
                    break;
                case 'h':
                    event.preventDefault();
                    sendCommand('HOME');
                    break;
                case 's':
                    event.preventDefault();
                    sendCommand('STOP');
                    break;
                case 'd':
                    event.preventDefault();
                    const debugEl = DOM.debugInfo;
                    if (debugEl) {
                        debugEl.style.display = debugEl.style.display === 'none' ? 'block' : 'none';
                    }
                    break;
                case 'r':
                    event.preventDefault();
                    location.reload();
                    break;
                case 'arrowleft':
                    event.preventDefault();
                    sendMoveCommand(-1);
                    break;
                case 'arrowright':
                    event.preventDefault();
                    sendMoveCommand(1);
                    break;
            }
        });

        // Initialize dashboard on page load
        window.addEventListener('load', function() {
            console.log('Enhanced Dashboard loading...');
            appendLog('Enhanced Dashboard with mm conversion initializing...', 'info');
            
            // Initialize motor target selection
            const radioButtons = document.querySelectorAll('input[name="motorTarget"]');
            radioButtons.forEach(radio => {
                radio.addEventListener("change", function() {
                    const targetValue = this.value;
                    currentMotorSelection = targetValue;
                    appendLog(`Motor target changed to: ${targetValue}`, "info");
                    
                    // Update current position display immediately
                    const currentPosElement = targetValue === 'KEPALA' ? DOM.posKepala : DOM.posPinggang;
                    if (currentPosElement && DOM.currentPositionDisplay) {
                        const posText = currentPosElement.textContent.replace(/[^\d.-]/g, '');
                        if (posText && !isNaN(posText)) {
                            DOM.currentPositionDisplay.innerHTML = `${parseFloat(posText).toFixed(1)} <span class="position-unit">mm</span>`;
                        }
                    }
                    
                    // Send target selection to ESP32
                    fetch("/control", {
                        method: "POST",
                        body: new URLSearchParams({ command: `TARGET_${targetValue.toUpperCase()}` }),
                        headers: { "Content-Type": "application/x-www-form-urlencoded" }
                    })
                    .then(response => response.json())
                    .then(data => {
                        if (data.success) {
                            appendLog(`✓ Motor target set to: ${targetValue}`, "success");
                        } else {
                            appendLog(`✗ Failed to set motor target: ${data.message}`, "error");
                        }
                    })
                    .catch(error => {
                        appendLog(`✗ Error setting motor target: ${error.message}`, "error");
                    });
                });
            });
            
            // Set default selection
            setTimeout(() => {
                const checkedRadio = document.querySelector('input[name="motorTarget"]:checked');
                if (checkedRadio) {
                    currentMotorSelection = checkedRadio.value;
                    appendLog(`Default motor target: ${checkedRadio.value}`, 'info');
                } else {
                    const pinggangRadio = document.querySelector('input[name="motorTarget"][value="PINGGANG"]');
                    if (pinggangRadio) {
                        pinggangRadio.checked = true;
                        currentMotorSelection = 'PINGGANG';
                        appendLog('Set default motor target to PINGGANG', 'info');
                    }
                }
            }, 500);
            
            // Initialize immediately
            appendLog('Enhanced Dashboard ready - ESP32 WiFi AP Mode with mm Display', 'success');
            appendLog('New Features: Position in mm, Enhanced Movement Controls', 'info');
            appendLog('Hotkeys: ESC=Stop, H=Home, S=Stop, D=Debug, R=Reload, ←→=Move±1mm', 'info');
            
            // Start data polling immediately with fast updates
            setTimeout(() => {
                appendLog('Starting high-speed data polling with mm conversion...', 'info');
                fetchData(); // Initial fetch
                
                // Set up fast intervals
                setInterval(fetchData, 200); // Every 200ms - 5Hz
                setInterval(monitorConnection, 1000); // Every 1 second
                setInterval(updateFPS, 100); // FPS counter update
                
            }, 100);
        });

        // Handle page visibility changes
        document.addEventListener('visibilitychange', function() {
            if (document.hidden) {
                appendLog('Dashboard hidden', 'info');
            } else {
                appendLog('Dashboard visible - refreshing data', 'info');
                fetchData();
            }
        });

        // Global error handler
        window.addEventListener('error', function(event) {
            console.error('Global error:', event.error);
            appendLog(`Global error: ${event.error.message}`, 'error');
        });

        // Handle unhandled promise rejections
        window.addEventListener('unhandledrejection', function(event) {
            console.error('Unhandled promise rejection:', event.reason);
            appendLog(`Promise error: ${event.reason}`, 'error');
        });

        // Expose debug functions globally
        window.debugFunctions = {
            fetchData,
            sendCommand,
            sendMoveCommand,
            emergencyStop,
            clearLog: () => {
                if (DOM.logContainer) {
                    DOM.logContainer.innerHTML = '';
                }
                errorCount = 0;
                if (DOM.debugErrors) {
                    DOM.debugErrors.textContent = '0';
                }
            },
            toggleDebug: () => {
                const debugEl = DOM.debugInfo;
                if (debugEl) {
                    debugEl.style.display = debugEl.style.display === 'none' ? 'block' : 'none';
                }
            },
            getStatus: () => ({
                connected: isConnected,
                dataUpdates: dataUpdateCount,
                errors: errorCount,
                lastData: lastDataTime,
                updateRate: updateRate,
                currentMotor: currentMotorSelection,
                forceDataPoints: forceData1.length,
                currentDataPoints: currentData1.length
            })
        };

        // Auto-show debug info if there are errors
        setInterval(() => {
            if (errorCount > 3 && DOM.debugInfo && DOM.debugInfo.style.display === 'none') {
                DOM.debugInfo.style.display = 'block';
                appendLog('Debug info auto-shown due to errors', 'warning');
            }
        }, 5000);

    </script>
</body>
</html>
)rawliteral";
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== ESP32 WIFI ACCESS POINT TRACTION MONITOR - ENHANCED ===");
  Serial.println("Version: Enhanced with mm Position Display & Orientation Fix");
  Serial.println("Free heap at start: " + String(ESP.getFreeHeap()));
  
  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  
  // Initialize pins
  setupPins();
  Serial.println("✓ Pins initialized");
  
  // Initialize steppers
  setupSteppers();
  Serial.println("✓ Steppers configured");
  
  // Setup WiFi Access Point with optimized settings
  setupOptimizedWiFiAP();
  
  // Initialize mutex for thread safety
  dataMutex = xSemaphoreCreateMutex();
  
  // Initialize current sensors
  Serial.println("Setting up current sensors...");
  calibrateCurrentSensors();
  Serial.println("✓ Current sensors calibrated");
  
  // Initialize HX711
  Serial.println("Setting up HX711 load cells...");
  hx711InitSuccess = initializeHX711WithRetry();
  
  if (hx711InitSuccess) {   
    Serial.println("✓ HX711 initialization SUCCESS");
    calibrateLoadCells();
    Serial.println("✓ Load cells calibrated");
  } else {
    Serial.println("⚠ HX711 initialization FAILED - continuing without load cells");
  }
  
  // Setup optimized web server
  setupOptimizedWebServer();
  
  // Create tasks for better performance
  xTaskCreatePinnedToCore(
    webServerTaskFunction,   // Task function
    "WebServerTask",         // Task name
    4096,                   // Stack size
    NULL,                   // Task parameters
    2,                      // Priority
    &webServerTask,         // Task handle
    0                       // Core ID (0 or 1)
  );
  
  xTaskCreatePinnedToCore(
    sensorTaskFunction,     // Task function
    "SensorTask",          // Task name
    2048,                  // Stack size
    NULL,                  // Task parameters
    1,                     // Priority
    &sensorTask,           // Task handle
    1                      // Core ID (0 or 1)
  );
  
  // AUTO HOMING
  Serial.println("\n=== AUTO HOMING SEQUENCE ===");
  Serial.println("Starting automatic homing...");
  performAutoHoming();
  
  // System ready
  systemReady = true;
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("\n=== ENHANCED SYSTEM READY ===");
  Serial.println("WiFi AP: " + String(ssid));
  Serial.println("Password: " + String(password));
  Serial.println("IP Address: " + WiFi.softAPIP().toString());
  Serial.println("Web Dashboard: http://" + WiFi.softAPIP().toString());
  Serial.println("Features: mm Position Display, Enhanced Motor Control");
  Serial.println("Motor Kepala: " + String(STEPS_PER_MM_KEPALA) + " steps/mm");
  Serial.println("Motor Pinggang: " + String(STEPS_PER_MM_PINGGANG) + " steps/mm (inverted display)");
  Serial.println("Update Rate: 5Hz (200ms intervals)");
  Serial.println("Free heap: " + String(ESP.getFreeHeap()));
  Serial.println("====================================");
}

void setupOptimizedWiFiAP() {
  Serial.println("Setting up optimized WiFi Access Point...");
  
  // Disconnect any existing connections
  WiFi.disconnect(true);
  delay(100);
  
  // Set WiFi mode
  WiFi.mode(WIFI_AP);
  delay(100);
  
  // Configure AP with optimized settings
  WiFi.softAPConfig(
    IPAddress(192, 168, 4, 1),    // IP
    IPAddress(192, 168, 4, 1),    // Gateway
    IPAddress(255, 255, 255, 0)   // Subnet
  );
  
  // Start AP with optimized parameters
  bool result = WiFi.softAP(ssid, password, 1, 0, 4); // Channel 1, not hidden, max 4 clients
  
  if (!result) {
    Serial.println("✗ WiFi AP setup failed!");
    return;
  }
  
  delay(1000);
  
  // Set power and other optimizations
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Max power for better signal
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("✓ Optimized WiFi AP started");
  Serial.println("SSID: " + String(ssid));
  Serial.println("Password: " + String(password));
  Serial.println("Channel: 1");
  Serial.println("AP IP address: " + IP.toString());
  Serial.println("Max clients: 4");
  Serial.println("TX Power: 19.5dBm");
}

void setupOptimizedWebServer() {
  Serial.println("Setting up enhanced web server...");
  
  // Enable CORS for all routes
  server.enableCORS(true);
  
  // Serve main dashboard with caching disabled
  server.on("/", HTTP_GET, [](){
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    handleOptimizedRoot();
  });
  
  // Enhanced JSON data endpoint with mm conversion
  server.on("/data", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    handleEnhancedData();
  });
  
  // Enhanced control endpoint with mm support
  server.on("/control", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.sendHeader("Cache-Control", "no-cache");
    handleEnhancedControl();
  });
  
  // Handle CORS preflight for all routes
  server.on("/control", HTTP_OPTIONS, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    server.sendHeader("Access-Control-Max-Age", "86400");
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/data", HTTP_OPTIONS, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    server.sendHeader("Access-Control-Max-Age", "86400");
    server.send(200, "text/plain", "OK");
  });
  
  // Handle 404
  server.onNotFound(handleNotFound);
  
  // Start server
  server.begin();
  Serial.println("✓ Enhanced web server started with mm conversion support");
}

// Web server task function
void webServerTaskFunction(void *pvParameters) {
  for(;;) {
    server.handleClient();
    vTaskDelay(1);
  }
}

// Sensor reading task function
void sensorTaskFunction(void *pvParameters) {
  for(;;) {
    updateSensorCache();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void updateSensorCache() {
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    // Update cached sensor data
    cachedData.current1 = readCurrent1();
    cachedData.current2 = readCurrent2();
    cachedData.load1 = readLoad1();
    cachedData.load2 = readLoad2();
    cachedData.posK = stepperKepala.currentPosition();
    cachedData.posP = stepperPinggang.currentPosition();
    cachedData.lsK = (digitalRead(LS_K) == HIGH);
    cachedData.lsP = readLimitSwitchPinggang();
    cachedData.timestamp = millis();
    
    xSemaphoreGive(dataMutex);
  }
}

void setupPins() {
  pinMode(LS_K, INPUT);
  pinMode(LS_P, INPUT);
  pinMode(REMOTE_LEFT, INPUT_PULLUP);
  pinMode(REMOTE_RIGHT, INPUT_PULLUP);
  
  pinMode(enablePin_Kepala, OUTPUT);
  pinMode(enableSpPin, OUTPUT);
  digitalWrite(enablePin_Kepala, HIGH);
  digitalWrite(enableSpPin, HIGH);
}

void setupSteppers() {
  stepperKepala.setMaxSpeed(500);
  stepperKepala.setAcceleration(200);
  stepperKepala.setCurrentPosition(0);
  
  stepperPinggang.setMaxSpeed(500);
  stepperPinggang.setAcceleration(200);
  stepperPinggang.setCurrentPosition(0);
}

bool initializeHX711WithRetry() {
  int maxRetries = 3;
  int retryDelay = 1000;
  
  for (int retry = 0; retry < maxRetries; retry++) {
    Serial.println("HX711 init attempt " + String(retry + 1) + "/" + String(maxRetries));
    
    try {
      scale1.begin(HX711_DT_1, HX711_SCK_1);
      scale2.begin(HX711_DT_2, HX711_SCK_2);
      delay(500);
      
      bool scale1Ready = false;
      bool scale2Ready = false;
      
      for (int i = 0; i < 5; i++) {
        if (scale1.is_ready()) scale1Ready = true;
        if (scale2.is_ready()) scale2Ready = true;
        delay(100);
      }
      
      Serial.println("Scale 1 ready: " + String(scale1Ready ? "YES" : "NO"));
      Serial.println("Scale 2 ready: " + String(scale2Ready ? "YES" : "NO"));
      
      if (scale1Ready || scale2Ready) {
        return true;
      }
      
    } catch (...) {
      Serial.println("HX711 init exception on attempt " + String(retry + 1));
    }
    
    delay(retryDelay);
  }
  
  return false;
}

void calibrateCurrentSensors() {
  Serial.println("=== CALIBRATING CURRENT SENSORS ===");
  Serial.println("Ensure motors are OFF and no current is flowing!");
  Serial.println("Waiting 3 seconds for stabilization...");
  delay(3000);
  
  float sumVoltage1 = 0;
  float sumVoltage2 = 0;
  int samples = 200; // Lebih banyak sample untuk akurasi
  
  Serial.println("Taking " + String(samples) + " calibration samples...");
  
  for(int i = 0; i < samples; i++) {
    // Baca raw ADC value
    int raw1 = analogRead(pinACS712);
    int raw2 = analogRead(pinACS712_2);
    
    // Konversi ke voltage
    float voltage1 = raw1 * (VCC / 4095.0);
    float voltage2 = raw2 * (VCC / 4095.0);
    
    sumVoltage1 += voltage1;
    sumVoltage2 += voltage2;
    
    // Debug setiap 50 sample
    if (i % 50 == 0) {
      Serial.println("Sample " + String(i) + " - Raw1: " + String(raw1) + ", V1: " + String(voltage1, 3) + 
                    ", Raw2: " + String(raw2) + ", V2: " + String(voltage2, 3));
    }
    
    delay(10);
  }
  
  ZERO_POINT_1 = sumVoltage1 / samples;
  ZERO_POINT_2 = sumVoltage2 / samples;
  
  Serial.println("=== CALIBRATION RESULTS ===");
  Serial.println("Sensor 1 Zero Point: " + String(ZERO_POINT_1, 4) + "V");
  Serial.println("Sensor 2 Zero Point: " + String(ZERO_POINT_2, 4) + "V");
  Serial.println("Expected Zero Point: ~" + String(VCC/2.0, 3) + "V for bidirectional sensors");
  
  // Validasi hasil kalibrasi
  if (ZERO_POINT_1 < 1.0 || ZERO_POINT_1 > 2.5 || ZERO_POINT_2 < 1.0 || ZERO_POINT_2 > 2.5) {
    Serial.println("WARNING: Zero point calibration seems unusual!");
    Serial.println("Check:");
    Serial.println("- Sensor wiring and power supply");
    Serial.println("- Make sure no current is flowing during calibration");
    Serial.println("- Sensor type (100mV/A vs 185mV/A)");
    currentSensorsCalibrated = false;
  } else {
    currentSensorsCalibrated = true;
    Serial.println("✓ Current sensors calibration SUCCESS");
  }
  
  Serial.println("========================");
}

void calibrateLoadCells() {
  if (!hx711InitSuccess) {
    Serial.println("Cannot calibrate load cells - HX711 not initialized");
    return;
  }
  
  Serial.println("=== CALIBRATING LOAD CELLS ===");
  Serial.println("IMPORTANT: Remove ALL loads from load cells!");
  Serial.println("Waiting 5 seconds...");
  delay(5000);
  
  try {
    bool scale1Success = false;
    bool scale2Success = false;
    
    // Calibrate Scale 1
    if (scale1.is_ready()) {
      Serial.println("Calibrating Load Cell 1...");
      
      // Reset scale
      scale1.set_scale();
      delay(200);
      
      // Tare (set zero point)
      Serial.println("Taring Load Cell 1...");
      scale1.tare(20); // 20 readings untuk average
      delay(500);
      
      // Set calibration factor
      scale1.set_scale(calibration_factor);
      delay(200);
      
      // Test reading
      float testReading1 = scale1.get_units(5);
      Serial.println("Scale 1 test reading: " + String(testReading1, 2));
      
      if (abs(testReading1) < 100) { // Should be close to zero after tare
        scale1Success = true;
        Serial.println("✓ Load Cell 1 calibration SUCCESS");
      } else {
        Serial.println("⚠ Load Cell 1 calibration questionable - reading not near zero");
      }
      
    } else {
      Serial.println("✗ Load Cell 1 not ready");
    }
    
    delay(500);
    
    // Calibrate Scale 2  
    if (scale2.is_ready()) {
      Serial.println("Calibrating Load Cell 2...");
      
      // Reset scale
      scale2.set_scale();
      delay(200);
      
      // Tare (set zero point)
      Serial.println("Taring Load Cell 2...");
      scale2.tare(20); // 20 readings untuk average
      delay(500);
      
      // Set calibration factor
      scale2.set_scale(calibration_factor2);
      delay(200);
      
      // Test reading
      float testReading2 = scale2.get_units(5);
      Serial.println("Scale 2 test reading: " + String(testReading2, 2));
      
      if (abs(testReading2) < 100) { // Should be close to zero after tare
        scale2Success = true;
        Serial.println("✓ Load Cell 2 calibration SUCCESS");
      } else {
        Serial.println("⚠ Load Cell 2 calibration questionable - reading not near zero");
      }
      
    } else {
      Serial.println("✗ Load Cell 2 not ready");
    }
    
    loadCellsCalibrated = (scale1Success || scale2Success);
    
    if (loadCellsCalibrated) {
      Serial.println("✓ Load cell calibration completed");
      Serial.println("Calibration factors: Scale1=" + String(calibration_factor) + 
                    ", Scale2=" + String(calibration_factor2));
    } else {
      Serial.println("✗ Load cell calibration failed");
    }
    
  } catch (...) {
    Serial.println("✗ Exception during load cell calibration");
    loadCellsCalibrated = false;
  }
  
  Serial.println("========================");
}

void performAutoHoming() {
  Serial.println("Auto-homing both motors...");
  
  Serial.println("Homing Kepala motor...");
  doHomingKepala();
  
  delay(500);
  
  Serial.println("Homing Pinggang motor...");
  doHomingPinggang();
  
  if (homingKepalaDone && homingPinggangDone) {
    autoHomingDone = true;
    Serial.println("✓ Auto-homing completed successfully!");
    
    // Set home positions
    homePositionKepala = stepperKepala.currentPosition();
    homePositionPinggang = stepperPinggang.currentPosition();
    
    Serial.println("Home position Kepala: " + String(homePositionKepala) + " steps");
    Serial.println("Home position Pinggang: " + String(homePositionPinggang) + " steps");
  } else {
    Serial.println("⚠ Auto-homing had issues - check limit switches");
  }
}

void loop() {
  // Main loop is now minimal - tasks handle most work
  static unsigned long lastWatchdog = 0;
  if (millis() - lastWatchdog > 10000) {
    lastWatchdog = millis();
    Serial.println("WATCHDOG: Enhanced system running, heap: " + String(ESP.getFreeHeap()) + ", Clients: " + String(WiFi.softAPgetStationNum()));
    Serial.println("Position - Kepala: " + String(getKepalaPositionMM(), 1) + "mm, Pinggang: " + String(getPinggangPositionMM(), 1) + "mm");
  }
  
  // Motor control
  stepperKepala.run();
  stepperPinggang.run();
  
  // Update motor status
  isMotorKepalaBusy = (stepperKepala.distanceToGo() != 0);
  isMotorPinggangBusy = (stepperPinggang.distanceToGo() != 0);
  
  // Remote control
  if (remoteControlEnabled && systemReady) {
    handleRemoteControl();
  }
  
  // LED heartbeat
  static unsigned long lastHeartbeat = 0;
  static bool ledState = false;
  if (millis() - lastHeartbeat > 1000) {
    lastHeartbeat = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
  
  delay(1);
}

// === ENHANCED WEB SERVER HANDLERS ===

void handleOptimizedRoot() {
  String html = getOptimizedDashboardHTML();
  server.send(200, "text/html", html);
}

void handleEnhancedData() {
  // Create enhanced JSON response with mm conversion
  DynamicJsonDocument doc(1024); // Increased size for additional data
  
  try {
    // Get cached data with mutex protection
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      // Motor positions in steps (original)
      doc["posK"] = cachedData.posK;
      doc["posP"] = cachedData.posP;
      doc["targetK"] = stepperKepala.targetPosition();
      doc["targetP"] = stepperPinggang.targetPosition();
      
      // Motor positions in mm (enhanced)
      doc["posK_mm"] = getKepalaPositionMM();
      doc["posP_mm"] = getPinggangPositionMM();
      doc["targetK_mm"] = getKepalaTargetMM();
      doc["targetP_mm"] = getPinggangTargetMM();
      
      // Limit switches
      doc["lsK"] = cachedData.lsK ? 1 : 0;
      doc["lsP"] = cachedData.lsP ? 1 : 0;
      
      // Current sensors with validation
      float current1 = cachedData.current1;
      float current2 = cachedData.current2;
      doc["current1"] = (isnan(current1) || current1 < 0) ? 0.0 : current1;
      doc["current2"] = (isnan(current2) || current2 < 0) ? 0.0 : current2;
      
      // Load cells with validation
      float load1_raw = cachedData.load1;
      float load2_raw = cachedData.load2;
      doc["load1"] = isnan(load1_raw) ? 0.0 : load1_raw;
      doc["load2"] = isnan(load2_raw) ? 0.0 : load2_raw;
      doc["load1_kg"] = isnan(load1_raw) ? 0.0 : abs(load1_raw / 1000.0 * LOAD_CELL_1_TO_KG);
      doc["load2_kg"] = isnan(load2_raw) ? 0.0 : abs(load2_raw / 1000.0 * LOAD_CELL_2_TO_KG);
      
      // System status
      doc["busyK"] = isMotorKepalaBusy;
      doc["busyP"] = isMotorPinggangBusy;
      doc["systemReady"] = systemReady;
      doc["autoHomingDone"] = autoHomingDone;
      doc["timestamp"] = cachedData.timestamp;
      
      // Calibration constants (for debugging)
      doc["stepsPerMM_K"] = STEPS_PER_MM_KEPALA;
      doc["stepsPerMM_P"] = STEPS_PER_MM_PINGGANG;
      
      doc["success"] = true;
      
      xSemaphoreGive(dataMutex);
    } else {
      doc["success"] = false;
      doc["error"] = "Data timeout";
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
    
  } catch (...) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Internal error\"}");
  }
}

void handleEnhancedControl() {
  if (!server.hasArg("command")) {
    Serial.println("Control request missing command parameter");
    server.send(400, "application/json", "{\"success\":false,\"message\":\"No command\"}");
    return;
  }
  
  String command = server.arg("command");
  command.trim();
  
  Serial.println("Received enhanced web command: '" + command + "'");
  
  bool success = false;
  String message = "";
  
  try {
    success = processEnhancedWebCommand(command);
    message = success ? "Command executed successfully" : "Command execution failed";
    Serial.println("Command '" + command + "' result: " + String(success ? "SUCCESS" : "FAILED"));
  } catch (...) {
    success = false;
    message = "Command processing error";
    Serial.println("Command '" + command + "' threw exception");
  }
  
  // Create detailed response
  DynamicJsonDocument doc(512);
  doc["success"] = success;
  doc["command"] = command;
  doc["message"] = message;
  doc["timestamp"] = millis();
  doc["systemReady"] = systemReady;
  doc["kepalaPos_mm"] = getKepalaPositionMM();
  doc["pinggangPos_mm"] = getPinggangPositionMM();
  
  String response;
  serializeJson(doc, response);
  
  Serial.println("Sending enhanced response: " + response);
  server.send(200, "application/json", response);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found - Enhanced Traction Monitor");
}

bool processEnhancedWebCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  Serial.println("Processing enhanced web command: '" + cmd + "'");
  
  // Enhanced movement commands with mm support
  if (cmd.startsWith("MOVE_MM_K ")) {
    float deltaMMValue = cmd.substring(10).toFloat();
    Serial.println("Moving Kepala by " + String(deltaMMValue, 1) + "mm");
    
    long currentPos = stepperKepala.currentPosition();
    long deltaSteps = convertKepalaMMToSteps(deltaMMValue);
    long newPosition = currentPos + deltaSteps;
    
    Serial.println("Current: " + String(currentPos) + " steps, Delta: " + String(deltaSteps) + " steps, New: " + String(newPosition) + " steps");
    moveMotorKepala(newPosition);
    return true;
  }
  else if (cmd.startsWith("MOVE_MM_P ")) {
    float deltaMMValue = cmd.substring(10).toFloat();
    Serial.println("Moving Pinggang by " + String(deltaMMValue, 1) + "mm");
    
    long currentPos = stepperPinggang.currentPosition();
    long deltaSteps = convertPinggangMMToSteps(deltaMMValue);
    long newPosition = currentPos + deltaSteps;
    
    Serial.println("Current: " + String(currentPos) + " steps, Delta: " + String(deltaSteps) + " steps, New: " + String(newPosition) + " steps");
    moveMotorPinggang(newPosition);
    return true;
  }
  
  // Original movement commands (backward compatibility)
  else if (cmd.startsWith("MOVE_K ")) {
    int targetPos = cmd.substring(7).toInt();
    Serial.println("Moving Kepala to position: " + String(targetPos));
    moveMotorKepala(targetPos);
    return true;
  }
  else if (cmd.startsWith("MOVE_P ")) {
    int targetPos = cmd.substring(7).toInt();
    Serial.println("Moving Pinggang to position: " + String(targetPos));
    moveMotorPinggang(targetPos);
    return true;
  }
  
  // Homing commands
   else if (cmd == "HOME_K") {
    Serial.println("Web: Homing Motor Kepala");
    doHomingKepala();
    return homingKepalaDone;
  }
  else if (cmd == "HOME_P") {
    Serial.println("Web: Homing Motor Pinggang");  
    doHomingPinggang();
    return homingPinggangDone;
  }
  else if (cmd == "HOME") {
    
    Serial.println("Web: Homing All Motors");
    performAutoHoming();
    return autoHomingDone;
  }

  
  // Stop commands
  else if (cmd == "STOP_K") {
    Serial.println("Stopping Motor Kepala");
    stepperKepala.stop();
    digitalWrite(enablePin_Kepala, HIGH);
    return true;
  }
  else if (cmd == "STOP_P") {
    Serial.println("Stopping Motor Pinggang");
    stepperPinggang.stop();
    digitalWrite(enableSpPin, HIGH);
    return true;
  }
  else if (cmd == "STOP_ALL") {
    Serial.println("Stopping All Motors");
    stepperKepala.stop();
    stepperPinggang.stop();
    digitalWrite(enablePin_Kepala, HIGH);
    digitalWrite(enableSpPin, HIGH);
    return true;
  }
  
  // Enable/Disable commands
  else if (cmd == "ENABLE_K") {
    Serial.println("Enabling Motor Kepala");
    digitalWrite(enablePin_Kepala, LOW);
    return true;
  }
  else if (cmd == "DISABLE_K") {
    Serial.println("Disabling Motor Kepala");
    digitalWrite(enablePin_Kepala, HIGH);
    return true;
  }
  else if (cmd == "ENABLE_P") {
    Serial.println("Enabling Motor Pinggang");
    digitalWrite(enableSpPin, LOW);
    return true;
  }
  else if (cmd == "DISABLE_P") {
    Serial.println("Disabling Motor Pinggang");
    digitalWrite(enableSpPin, HIGH);
    return true;
  }
  
  // Remote control commands
  else if (cmd == "TARGET_KEPALA") {
    currentMotorTarget = MOTOR_KEPALA;
    Serial.println("Remote target set to: KEPALA");
    return true;
  }
  else if (cmd == "TARGET_PINGGANG") {
    currentMotorTarget = MOTOR_PINGGANG;
    Serial.println("Remote target set to: PINGGANG");
    return true;
  }
  else if (cmd == "REMOTE_ON") {
    remoteControlEnabled = true;
    Serial.println("Remote control: ENABLED");
    return true;
  }
  else if (cmd == "REMOTE_OFF") {
    remoteControlEnabled = false;
    Serial.println("Remote control: DISABLED");
    return true;
  }
  
  // Calibration commands
  else if (cmd == "CALIBRATE") {
    Serial.println("Recalibrating all sensors...");
    calibrateCurrentSensors();
    if (hx711InitSuccess) {
      calibrateLoadCells();
    }
    Serial.println("Calibration complete");
    return true;
  }
  
  // Position reset commands (new)
  else if (cmd == "RESET_POSITION_K") {
    Serial.println("Resetting Kepala position to 0");
    stepperKepala.setCurrentPosition(0);
    homePositionKepala = 0;
    return true;
  }
  else if (cmd == "RESET_POSITION_P") {
    Serial.println("Resetting Pinggang position to 0");
    stepperPinggang.setCurrentPosition(0);
    homePositionPinggang = 0;
    return true;
  }
  else if (cmd.startsWith("SIM_")) {
    return adjustSimulationParameters(cmd);
  }
  else if (cmd == "SIM_STATUS") {
    Serial.println("=== SIMULATION STATUS ===");
    Serial.println("Base Current 1: " + String(sensorSim.baseCurrent1, 3) + "A");
    Serial.println("Base Current 2: " + String(sensorSim.baseCurrent2, 3) + "A");
    Serial.println("Base Load 1: " + String(sensorSim.baseLoad1, 1) + "g");
    Serial.println("Base Load 2: " + String(sensorSim.baseLoad2, 1) + "g");
    Serial.println("Load Multiplier: " + String(sensorSim.loadMultiplier, 1) + "g/mm");
    Serial.println("Noise Amplitude: " + String(sensorSim.noiseAmplitude, 3));
    Serial.println("========================");
    return true;
  }
  
  Serial.println("Unknown enhanced command: '" + cmd + "'");
  return false;
}

void handleRemoteControl() {
  unsigned long currentTime = millis();
  
  if (digitalRead(REMOTE_LEFT) == LOW && (currentTime - lastRemoteLeftPress > DEBOUNCE_DELAY)) {
    lastRemoteLeftPress = currentTime;
    
    if (currentMotorTarget == MOTOR_KEPALA) {
      if (digitalRead(LS_K) == HIGH) {
        Serial.println("Remote: Kepala - Limit Switch active! Cannot move left.");
        return;
      }
      long newPos = stepperKepala.currentPosition() - REMOTE_STEP_SIZE;
      moveMotorKepala(newPos);
      Serial.println("Remote Left: Kepala moved to " + String(getKepalaPositionMM(), 1) + "mm");
    } else {
      long newPos = stepperPinggang.currentPosition() - REMOTE_STEP_SIZE;
      moveMotorPinggang(newPos);
      Serial.println("Remote Left: Pinggang moved to " + String(getPinggangPositionMM(), 1) + "mm");
    }
  }
  
  if (digitalRead(REMOTE_RIGHT) == LOW && (currentTime - lastRemoteRightPress > DEBOUNCE_DELAY)) {
    lastRemoteRightPress = currentTime;
    
    if (currentMotorTarget == MOTOR_KEPALA) {
      long newPos = stepperKepala.currentPosition() + REMOTE_STEP_SIZE;
      moveMotorKepala(newPos);
      Serial.println("Remote Right: Kepala moved to " + String(getKepalaPositionMM(), 1) + "mm");
    } else {
      if (readLimitSwitchPinggang()) {
        Serial.println("Remote: Pinggang - Limit Switch active! Cannot move right.");
        return;
      }
      long newPos = stepperPinggang.currentPosition() + REMOTE_STEP_SIZE;
      moveMotorPinggang(newPos);
      Serial.println("Remote Right: Pinggang moved to " + String(getPinggangPositionMM(), 1) + "mm");
    }
  }
}

bool readLimitSwitchPinggang() {
  int readings = 0;
  for (int i = 0; i < 3; i++) {
    if (digitalRead(LS_P) == HIGH) readings++;
    delayMicroseconds(50);
  }
  return (readings >= 2);
}

void moveMotorKepala(long targetPosition) {
  digitalWrite(enablePin_Kepala, LOW);
  stepperKepala.setMaxSpeed(300);
  stepperKepala.setAcceleration(150);
  stepperKepala.moveTo(targetPosition);
  
  Serial.println("Motor Kepala moving to: " + String(targetPosition) + " steps (" + 
                String(convertKepaladStepsToMM(targetPosition - homePositionKepala), 1) + "mm)");
}

void moveMotorPinggang(long targetPosition) {
  digitalWrite(enableSpPin, LOW);
  stepperPinggang.setMaxSpeed(300);
  stepperPinggang.setAcceleration(150);
  stepperPinggang.moveTo(targetPosition);
  
  Serial.println("Motor Pinggang moving to: " + String(targetPosition) + " steps (" + 
                String(convertPinggangStepsToMM(targetPosition - homePositionPinggang), 1) + "mm)");
}

void doHomingKepala() {
  Serial.println("Starting Kepala homing sequence...");
  
  // Pastikan motor enabled dan siap
  digitalWrite(enablePin_Kepala, LOW);
  delay(100);
  
  // Set kecepatan homing yang lebih lambat dan aman
  stepperKepala.setMaxSpeed(300);
  stepperKepala.setAcceleration(100);
  
  // Cek apakah sudah di limit switch
  if (digitalRead(LS_K) == HIGH) {
    Serial.println("Kepala already at limit switch, moving away first...");
    stepperKepala.move(500); // Gerak menjauh dari limit switch
    while (stepperKepala.distanceToGo() != 0 && digitalRead(LS_K) == HIGH) {
      stepperKepala.run();
      delay(1);
    }
    delay(200);
  }
  
  // Gerak menuju limit switch dengan kecepatan konstan
  Serial.println("Moving Kepala towards limit switch...");
  stepperKepala.setSpeed(-200); // Negatif = menuju limit switch
  
  unsigned long homingStartTime = millis();
  const unsigned long HOMING_TIMEOUT = 30000; // 30 detik timeout
  
  // Bergerak hingga menyentuh limit switch atau timeout
  while (digitalRead(LS_K) == LOW) {
    stepperKepala.runSpeed();
    
    // Safety timeout
    if (millis() - homingStartTime > HOMING_TIMEOUT) {
      Serial.println("ERROR: Kepala homing timeout!");
      stepperKepala.stop();
      digitalWrite(enablePin_Kepala, HIGH);
      homingKepalaDone = false;
      return;
    }
    
    delay(1);
  }
  
  // Stop motor dan set posisi home
  stepperKepala.stop();
  delay(200);
  
  // Set home position
  stepperKepala.setCurrentPosition(0);
  homePositionKepala = 0;
  homingKepalaDone = true;
  
  // Disable motor setelah homing
  digitalWrite(enablePin_Kepala, HIGH);
  
  Serial.println("✓ Kepala homing completed successfully - Position: 0mm");
  Serial.println("Kepala current position: " + String(stepperKepala.currentPosition()) + " steps");
}

void doHomingPinggang() {
  Serial.println("Starting Pinggang homing sequence...");
  
  // Pastikan motor enabled dan siap
  digitalWrite(enableSpPin, LOW);
  delay(100);
  
  // Set kecepatan homing yang lebih lambat dan aman
  stepperPinggang.setMaxSpeed(300);
  stepperPinggang.setAcceleration(100);
  
  // Cek apakah sudah di limit switch
  if (readLimitSwitchPinggang()) {
    Serial.println("Pinggang already at limit switch, moving away first...");
    stepperPinggang.move(-500); // Gerak menjauh dari limit switch (negatif karena orientasi terbalik)
    while (stepperPinggang.distanceToGo() != 0 && readLimitSwitchPinggang()) {
      stepperPinggang.run();
      delay(1);
    }
    delay(200);
  }
  
  // Gerak menuju limit switch dengan kecepatan konstan
  Serial.println("Moving Pinggang towards limit switch...");
  stepperPinggang.setSpeed(200); // Positif = menuju limit switch untuk motor pinggang
  
  unsigned long homingStartTime = millis();
  const unsigned long HOMING_TIMEOUT = 30000; // 30 detik timeout
  
  // Bergerak hingga menyentuh limit switch atau timeout
  while (!readLimitSwitchPinggang()) {
    stepperPinggang.runSpeed();
    
    // Safety timeout
    if (millis() - homingStartTime > HOMING_TIMEOUT) {
      Serial.println("ERROR: Pinggang homing timeout!");
      stepperPinggang.stop();
      digitalWrite(enableSpPin, HIGH);
      homingPinggangDone = false;
      return;
    }
    
    delay(1);
  }
  
  // Stop motor dan set posisi home
  stepperPinggang.stop();
  delay(200);
  
  // Set home position
  stepperPinggang.setCurrentPosition(0);
  homePositionPinggang = 0;
  homingPinggangDone = true;
  
  // Disable motor setelah homing
  digitalWrite(enableSpPin, HIGH);
  
  Serial.println("✓ Pinggang homing completed successfully - Position: 0mm");
  Serial.println("Pinggang current position: " + String(stepperPinggang.currentPosition()) + " steps");
}


//simulasi 

float readCurrent1() {
  // Simulasi arus berdasarkan aktivitas motor Kepala
  float simulatedCurrent = sensorSim.baseCurrent1;
  
  // Tambah arus jika motor sedang bergerak
  if (isMotorKepalaBusy) {
    float motorSpeed = abs(stepperKepala.speed());
    simulatedCurrent += (motorSpeed / 500.0) * sensorSim.currentMultiplier * 50; // Scale up untuk visibility
    
    // Tambah extra current jika mendekati target (simulasi resistance)
    long distanceToGo = abs(stepperKepala.distanceToGo());
    if (distanceToGo < 100) {
      simulatedCurrent += 0.3; // Extra current untuk "braking"
    }
  }
  
  // Tambah noise realistis
  updateNoise();
  simulatedCurrent += sensorSim.noiseOffset1;
  
  // Pastikan nilai tidak negatif dan dalam range realistis
  simulatedCurrent = constrain(simulatedCurrent, 0.0, 5.0);
  
  return simulatedCurrent;
}

float readCurrent2() {
  // Simulasi arus berdasarkan aktivitas motor Pinggang
  float simulatedCurrent = sensorSim.baseCurrent2;
  
  // Tambah arus jika motor sedang bergerak
  if (isMotorPinggangBusy) {
    float motorSpeed = abs(stepperPinggang.speed());
    simulatedCurrent += (motorSpeed / 500.0) * sensorSim.currentMultiplier * 45; // Scale up untuk visibility
    
    // Tambah extra current jika mendekati target
    long distanceToGo = abs(stepperPinggang.distanceToGo());
    if (distanceToGo < 100) {
      simulatedCurrent += 0.25; // Extra current untuk "braking"
    }
  }
  
  // Tambah noise realistis
  simulatedCurrent += sensorSim.noiseOffset2;
  
  // Pastikan nilai tidak negatif dan dalam range realistis
  simulatedCurrent = constrain(simulatedCurrent, 0.0, 5.0);
  
  return simulatedCurrent;
}

float readLoad1() {
  // Simulasi load cell 1 berdasarkan posisi motor Kepala
  float positionMM = getKepalaPositionMM();
  
  // Base load + load berdasarkan posisi (semakin jauh dari home, semakin berat)
  float simulatedLoad = sensorSim.baseLoad1 + (positionMM * sensorSim.loadMultiplier);
  
  // Tambah load extra jika motor sedang bergerak (simulasi dynamic load)
  if (isMotorKepalaBusy) {
    simulatedLoad += 20.0; // Dynamic load saat bergerak
  }
  
  // Simulasi efek traksi: load meningkat secara non-linear
  if (positionMM > 10.0) {
    float extraLoad = pow((positionMM - 10.0) / 10.0, 1.3) * 50.0;
    simulatedLoad += extraLoad;
  }
  
  // Tambah noise
  updateNoise();
  simulatedLoad += sensorSim.noiseOffset1 * 10; // Scale noise untuk load cell
  
  // Konversi ke format yang sama dengan HX711 (dalam gram, kemudian dikali 1000)
  simulatedLoad = constrain(simulatedLoad, 0.0, 5000.0); // Max 5kg
  
  return simulatedLoad;
}

float readLoad2() {
  // Simulasi load cell 2 berdasarkan posisi motor Pinggang
  float positionMM = getPinggangPositionMM();
  
  // Base load + load berdasarkan posisi
  float simulatedLoad = sensorSim.baseLoad2 + (positionMM * sensorSim.loadMultiplier * 1.2); // Slightly different multiplier
  
  // Tambah load extra jika motor sedang bergerak
  if (isMotorPinggangBusy) {
    simulatedLoad += 25.0; // Dynamic load saat bergerak
  }
  
  // Simulasi efek traksi pada pinggang (berbeda dengan kepala)
  if (positionMM > 5.0) {
    float extraLoad = pow((positionMM - 5.0) / 15.0, 1.2) * 75.0;
    simulatedLoad += extraLoad;
  }
  
  // Tambah noise
  simulatedLoad += sensorSim.noiseOffset2 * 12; // Slightly different noise scale
  
  // Konversi ke format yang sama dengan HX711
  simulatedLoad = constrain(simulatedLoad, 0.0, 6000.0); // Max 6kg
  
  return simulatedLoad;
}

void updateNoise() {
  // Update noise setiap 100ms untuk efek realistis
  if (millis() - sensorSim.lastNoiseUpdate > 100) {
    sensorSim.lastNoiseUpdate = millis();
    
    // Generate smooth noise menggunakan sin wave dengan random offset
    float timeOffset = millis() * 0.001; // Convert to seconds
    sensorSim.noiseOffset1 = sin(timeOffset * 2.0 + random(100) * 0.01) * sensorSim.noiseAmplitude;
    sensorSim.noiseOffset2 = cos(timeOffset * 1.5 + random(100) * 0.01) * sensorSim.noiseAmplitude;
  }
}

// Optimized sensor reading functions
// float readCurrent1() {
//   if (!currentSensorsCalibrated) {
//     static unsigned long lastWarning = 0;
//     if (millis() - lastWarning > 5000) { // Warning setiap 5 detik
//       Serial.println("Current sensor 1 not calibrated");
//       lastWarning = millis();
//     }
//     return 0.0;
//   }
  
//   try {
//     // Baca beberapa sample untuk mengurangi noise
//     float voltageSum = 0;
//     int samples = 5;
    
//     for (int i = 0; i < samples; i++) {
//       int rawValue = analogRead(pinACS712);
//       voltageSum += rawValue * (VCC / 4095.0);
//       delayMicroseconds(100);
//     }
    
//     float voltage = voltageSum / samples;
    
//     // Hitung current berdasarkan selisih dari zero point
//     float voltageDifference = voltage - ZERO_POINT_1;
//     float current = abs(voltageDifference / (ACS712_MV_PER_AMPERE / 1000.0));
    
//     // Filter noise - jika terlalu kecil, anggap nol
//     if (current < 0.1) {
//       current = 0.0;
//     }
    
//     // Validasi range
//     if (isnan(current) || current < 0.0 || current > 30.0) {
//       return 0.0;
//     }
    
//     return current;
    
//   } catch (...) {
//     return 0.0;
//   }
// }

// float readCurrent2() {
//   if (!currentSensorsCalibrated) {
//     static unsigned long lastWarning = 0;
//     if (millis() - lastWarning > 5000) { // Warning setiap 5 detik
//       Serial.println("Current sensor 2 not calibrated");
//       lastWarning = millis();
//     }
//     return 0.0;
//   }
  
//   try {
//     // Baca beberapa sample untuk mengurangi noise
//     float voltageSum = 0;
//     int samples = 5;
    
//     for (int i = 0; i < samples; i++) {
//       int rawValue = analogRead(pinACS712_2);
//       voltageSum += rawValue * (VCC / 4095.0);
//       delayMicroseconds(100);
//     }
    
//     float voltage = voltageSum / samples;
    
//     // Hitung current berdasarkan selisih dari zero point  
//     float voltageDifference = voltage - ZERO_POINT_2;
//     float current = abs(voltageDifference / (ACS712_MV_PER_AMPERE / 1000.0));
    
//     // Filter noise - jika terlalu kecil, anggap nol
//     if (current < 0.1) {
//       current = 0.0;
//     }
    
//     // Validasi range
//     if (isnan(current) || current < 0.0 || current > 30.0) {
//       return 0.0;
//     }
    
//     return current;
    
//   } catch (...) {
//     return 0.0;
//   }
// }

// float readLoad1() {
//   if (!hx711InitSuccess || !loadCellsCalibrated) {
//     static unsigned long lastWarning = 0;
//     if (millis() - lastWarning > 10000) { // Warning setiap 10 detik
//       Serial.println("Load cell 1 not ready or calibrated");
//       lastWarning = millis();
//     }
//     return 0.0;
//   }
  
//   try {
//     if (scale1.is_ready()) {
//       // Baca dengan lebih sedikit average untuk response yang lebih cepat
//       float reading = scale1.get_units(3);
      
//       // Filter noise - jika terlalu kecil, anggap nol
//       if (abs(reading) < 50) {
//         reading = 0.0;
//       }
      
//       // Validasi range
//       if (isnan(reading) || reading < -10000.0 || reading > 10000.0) {
//         static unsigned long lastError = 0;
//         if (millis() - lastError > 5000) {
//           Serial.println("Load cell 1 reading out of range: " + String(reading));
//           lastError = millis();
//         }
//         return 0.0;
//       }
      
//       return reading;
//     }
//   } catch (...) {
//     static unsigned long lastError = 0;
//     if (millis() - lastError > 5000) {
//       Serial.println("Error reading load cell 1");
//       lastError = millis();
//     }
//   }
//   return 0.0;
// }

// // PERBAIKAN 9: Fungsi readLoad2 yang diperbaiki
// float readLoad2() {
//   if (!hx711InitSuccess || !loadCellsCalibrated) {
//     static unsigned long lastWarning = 0;
//     if (millis() - lastWarning > 10000) { // Warning setiap 10 detik
//       Serial.println("Load cell 2 not ready or calibrated");
//       lastWarning = millis();
//     }
//     return 0.0;
//   }
  
//   try {
//     if (scale2.is_ready()) {
//       // Baca dengan lebih sedikit average untuk response yang lebih cepat
//       float reading = scale2.get_units(3);
      
//       // Filter noise - jika terlalu kecil, anggap nol
//       if (abs(reading) < 50) {
//         reading = 0.0;
//       }
      
//       // Validasi range
//       if (isnan(reading) || reading < -10000.0 || reading > 10000.0) {
//         static unsigned long lastError = 0;
//         if (millis() - lastError > 5000) {
//           Serial.println("Load cell 2 reading out of range: " + String(reading));
//           lastError = millis();
//         }
//         return 0.0;
//       }
      
//       return reading;
//     }
//   } catch (...) {
//     static unsigned long lastError = 0;
//     if (millis() - lastError > 5000) {
//       Serial.println("Error reading load cell 2");
//       lastError = millis();
//     }
//   }
//   return 0.0;
// }

void debugSensors() {
  Serial.println("=== SENSOR DEBUG INFO ===");
  
  // Current sensors
  Serial.println("Current Sensors:");
  Serial.println("- Calibrated: " + String(currentSensorsCalibrated ? "YES" : "NO"));
  if (currentSensorsCalibrated) {
    Serial.println("- Zero Point 1: " + String(ZERO_POINT_1, 4) + "V");
    Serial.println("- Zero Point 2: " + String(ZERO_POINT_2, 4) + "V");
    
    // Raw readings
    int raw1 = analogRead(pinACS712);
    int raw2 = analogRead(pinACS712_2);
    float voltage1 = raw1 * (VCC / 4095.0);
    float voltage2 = raw2 * (VCC / 4095.0);
    
    Serial.println("- Raw 1: " + String(raw1) + " (" + String(voltage1, 3) + "V)");
    Serial.println("- Raw 2: " + String(raw2) + " (" + String(voltage2, 3) + "V)");
    Serial.println("- Current 1: " + String(readCurrent1(), 3) + "A");
    Serial.println("- Current 2: " + String(readCurrent2(), 3) + "A");
  }
  
  // Load cells
  Serial.println("Load Cells:");
  Serial.println("- HX711 Init: " + String(hx711InitSuccess ? "YES" : "NO"));  
  Serial.println("- Calibrated: " + String(loadCellsCalibrated ? "YES" : "NO"));
  if (hx711InitSuccess) {
    Serial.println("- Scale 1 Ready: " + String(scale1.is_ready() ? "YES" : "NO"));
    Serial.println("- Scale 2 Ready: " + String(scale2.is_ready() ? "YES" : "NO"));
    if (loadCellsCalibrated) {
      Serial.println("- Load 1: " + String(readLoad1(), 2));
      Serial.println("- Load 2: " + String(readLoad2(), 2));
    }
  }
  
  Serial.println("========================");
}

bool adjustSimulationParameters(String command) {
  if (command.startsWith("SIM_CURRENT_BASE ")) {
    float newBase = command.substring(17).toFloat();
    sensorSim.baseCurrent1 = constrain(newBase, 0.0, 1.0);
    sensorSim.baseCurrent2 = constrain(newBase, 0.0, 1.0);
    Serial.println("Simulation: Base current set to " + String(newBase, 3) + "A");
    return true;
  }
  else if (command.startsWith("SIM_LOAD_MULT ")) {
    float newMult = command.substring(14).toFloat();
    sensorSim.loadMultiplier = constrain(newMult, 5.0, 50.0);
    Serial.println("Simulation: Load multiplier set to " + String(newMult, 1));
    return true;
  }
  else if (command.startsWith("SIM_NOISE ")) {
    float newNoise = command.substring(10).toFloat();
    sensorSim.noiseAmplitude = constrain(newNoise, 0.0, 0.5);
    Serial.println("Simulation: Noise amplitude set to " + String(newNoise, 3));
    return true;
  }
  else if (command == "SIM_RESET") {
    // Reset ke nilai default
    sensorSim.baseCurrent1 = 0.1;
    sensorSim.baseCurrent2 = 0.1;
    sensorSim.baseLoad1 = 50.0;
    sensorSim.baseLoad2 = 30.0;
    sensorSim.loadMultiplier = 15.0;
    sensorSim.noiseAmplitude = 0.05;
    Serial.println("Simulation: Parameters reset to defaults");
    return true;
  }
  
  return false;
}