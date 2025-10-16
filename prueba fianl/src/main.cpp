#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "time.h"
#define PIN_SENSOR 34   
#define PIN_RELE 25 
#define NIVEL_BAJO 1200   
#define NIVEL_ALTO 2500   

bool valvulaAbierta = false;
unsigned long tiempoAnterior = 0;
const unsigned long intervaloLectura = 500;

// --- CONFIGURACIÓN WIFI ---
const char* ssid = "CESJT";
const char* password = "itisjtsmg";

// --- CONFIGURACIÓN MOTOR ---
const int STEP_PIN = 14;
const int DIR_PIN = 12;

// --- VARIABLES DE CONTROL ---
int gramosObjetivo = 0;
int horaObjetivo = 0;
int minutoObjetivo = 0;
bool yaEjecutado = false;

// --- SERVIDOR ---
AsyncWebServer server(80);

// --- CONFIGURACIÓN HORA ---
void configurarTiempo() {
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  Serial.print("Sincronizando hora...");
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nHora sincronizada correctamente.");
}

// --- FUNCIÓN MOTOR ---
void moverMotor(int pasos, bool direccion) {
  digitalWrite(DIR_PIN, direccion ? HIGH : LOW);
  for (int i = 0; i < pasos; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(800);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(800);
  }
}

// --- PÁGINA HTML ---
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Comedero Automático</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background-color: #ffffff;
      text-align: center;
      padding-top: 50px;
    }
    .seccion {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 30px;
      background: #1565c0;
      padding: 30px 40px;
      border-radius: 20px;
      box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);
      width: fit-content;
      margin: auto;
    }
    button {
      background-color: #78a2dc;
      color: white;
      border: none;
      font-size: 20px;
      padding: 10px 20px;
      border-radius: 10px;
      cursor: pointer;
      transition: background 0.2s;
    }
    button:hover {
      background-color: #45a049;
    }
    input[type="number"], input[type="time"] {
      font-size: 18px;
      padding: 6px 10px;
      border: 2px solid #78a2dc;
      border-radius: 10px;
      outline: none;
      text-align: center;
    }
    h2 { color: #fff; }
  </style>
</head>
<body>
  <div class="seccion">
    <h2>Programar Comida</h2>
    <form action="/set" method="GET">
      <label style="color:white;">Cantidad de comida (gramos):</label><br>
      <input type="number" name="gramos" min="0" step="50" required><br><br>
      <label style="color:white;">Hora de la comida:</label><br>
      <input type="time" name="hora" required><br><br>
      <button type="submit">Enviar</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, LOW); 
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);

  // --- CONEXIÓN WIFI ---
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());

  configurarTiempo();

  // --- RUTA PRINCIPAL ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", htmlPage);
  });

  // --- RUTA DE CONFIGURACIÓN ---
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("gramos") && request->hasParam("hora")) {
      gramosObjetivo = request->getParam("gramos")->value().toInt();

      String horaStr = request->getParam("hora")->value(); // formato "HH:MM"
      horaObjetivo = horaStr.substring(0, 2).toInt();
      minutoObjetivo = horaStr.substring(3, 5).toInt();

      Serial.printf("Configurado: %d g -> %02d:%02d\n", gramosObjetivo, horaObjetivo, minutoObjetivo);
      request->send(200, "text/html", "<h2>Configuración guardada correctamente.</h2><a href='/'>Volver</a>");
    } else {
      request->send(400, "text/plain", "Faltan parámetros.");
    }
  });

  server.begin();
  Serial.println("Servidor web iniciado.");
}

void loop() {
//codigo control de valvula
  unsigned long tiempoActual = millis();

  if (tiempoActual - tiempoAnterior >= intervaloLectura) {
    tiempoAnterior = tiempoActual;

    int nivel = analogRead(PIN_SENSOR);

    if (nivel < NIVEL_BAJO && !valvulaAbierta) {
      digitalWrite(PIN_RELE, HIGH);  
      valvulaAbierta = true;
    }

    if (nivel > NIVEL_ALTO && valvulaAbierta) {
      digitalWrite(PIN_RELE, LOW);  
      valvulaAbierta = false;
    }
  }
//codigo del motor 
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.printf("Hora actual: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    if (timeinfo.tm_hour == horaObjetivo &&
        timeinfo.tm_min == minutoObjetivo &&
        !yaEjecutado &&
        gramosObjetivo > 0) {

      Serial.println("Ejecutando alimentación...");
      int pasos = gramosObjetivo * 4;  // 50g = 200 pasos => 1g = 4 pasos
      moverMotor(pasos, true);
      yaEjecutado = true;
    }

    if (timeinfo.tm_min != minutoObjetivo) {
      yaEjecutado = false; // permitir ejecutar de nuevo al día siguiente
    }
  }
  delay(1000);
}
