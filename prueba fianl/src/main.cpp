#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "time.h"

#define PIN_SENSOR 34
#define PIN_RELE 25
#define NIVEL_BAJO 1200
#define NIVEL_ALTO 2500
#define STEP_PIN 14
#define DIR_PIN 12


bool valvulaAbierta = false;
unsigned long tiempoAnterior = 0;
const unsigned long intervaloLectura = 500;

int gramosObjetivo = 0;
int horaObjetivo = 0;
int minutoObjetivo = 0;
bool yaEjecutado = false;


const char* ssid = "CESJT";
const char* password = "itisjtsmg";


AsyncWebServer server(80);


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


void moverMotor(int pasos, bool direccion) {
  digitalWrite(DIR_PIN, direccion ? HIGH : LOW);
  for (int i = 0; i < pasos; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(800);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(800);
  }
}


const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Comedero Automático</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(to right, #ffffff, #aee0ff);
      text-align: center;
      padding-top: 40px;
    }
    .seccion {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 20px;
      background: #1565c0;
      padding: 30px 50px;
      border-radius: 20px;
      box-shadow: 0 4px 10px rgba(0, 0, 0, 0.2);
      width: fit-content;
      margin: auto;
      color: white;
    }
    button {
      background-color: #78a2dc;
      color: white;
      border: none;
      font-size: 20px;
      padding: 10px 25px;
      border-radius: 10px;
      cursor: pointer;
      transition: background 0.2s;
    }
    button:hover { background-color: #45a049; }
    input[type="number"], input[type="time"] {
      font-size: 18px;
      padding: 6px 10px;
      border: 2px solid #78a2dc;
      border-radius: 10px;
      outline: none;
      text-align: center;
    }
    h2 { color: #fff; }
    .contador {
      display: flex;
      justify-content: center;
      align-items: center;
      gap: 20px;
      margin-top: 20px;
    }
    #cantidad {
      font-size: 28px;
      font-weight: bold;
      color: #333;
      width: 80px;
      display: inline-block;
    }
  </style>
</head>
<body>
  <div class="seccion">
    <h2>Programar Comida</h2>
    <form id="formComida" action="/set" method="GET">
      <label>Cantidad (gramos):</label><br>
      <div class="contador">
        <button type="button" onclick="cambiarCantidad(-50)">-</button>
        <span id="cantidad">0</span>
        <button type="button" onclick="cambiarCantidad(50)">+</button>
      </div>
      <input type="hidden" name="gramos" id="inputGramos" value="0">
      <br><br>
      <label>Hora programada:</label><br>
      <input type="time" name="hora" required><br><br>
      <button type="submit">Guardar</button>
    </form>

    <button onclick="alimentarAhora()">Alimentar ahora</button>
  </div>

  <script>
    let cantidad = 0;
    const cantidadSpan = document.getElementById("cantidad");
    const inputGramos = document.getElementById("inputGramos");

    function cambiarCantidad(valor) {
      cantidad += valor;
      if (cantidad < 0) cantidad = 0;
      cantidadSpan.textContent = cantidad;
      inputGramos.value = cantidad;
    }

    function alimentarAhora() {
      fetch(`/alimentar?gramos=${cantidad}`)
        .then(r => r.text())
        .then(t => alert(t));
    }
  </script>
</body>
</html>
)rawliteral";


void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, LOW);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.println(WiFi.localIP());

  configurarTiempo();


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", htmlPage);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("gramos") && request->hasParam("hora")) {
      gramosObjetivo = request->getParam("gramos")->value().toInt();
     
      String horaStr = request->getParam("hora")->value();
      horaObjetivo = horaStr.substring(0, 2).toInt();
      minutoObjetivo = horaStr.substring(3, 5).toInt();
      Serial.printf("Configurado: %d g -> %02d:%02d\n", gramosObjetivo, horaObjetivo, minutoObjetivo);
      request->send(200, "text/html", "<h2>Configuración guardada correctamente.</h2><a href='/'>Volver</a>");
    } else {
      request->send(400, "text/plain", "Faltan parámetros.");
    }
  });

  server.on("/alimentar", HTTP_GET, [](AsyncWebServerRequest *request) {
    int gramos = 0;
    if (request->hasParam("gramos")) {
      gramos = request->getParam("gramos")->value().toInt();
    }
    if (gramos > 0) {
      int pasos = gramos * 4; 
      moverMotor(pasos, true);
      request->send(200, "text/plain", "Alimentación manual completada.");
      Serial.printf("Alimentación manual: %d g\n", gramos);
    } else {
      request->send(400, "text/plain", "Cantidad inválida.");
    }
  });

  server.begin();
  Serial.println("Servidor web iniciado.");
}


void loop() {
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


  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_hour == horaObjetivo &&
        timeinfo.tm_min == minutoObjetivo &&
        !yaEjecutado &&
        gramosObjetivo > 0) {

      int pasos = gramosObjetivo * 4;
      moverMotor(pasos, true);
      yaEjecutado = true;
      Serial.printf("Alimentación automática ejecutada: %d g\n", gramosObjetivo);
    }

    if (timeinfo.tm_min != minutoObjetivo) {
      yaEjecutado = false;
    }
  }
}
