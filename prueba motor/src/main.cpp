#include <WiFi.h>
#include "time.h"

const char* ssid = "CESJT";
const char* password = "itisjtsmg";

const int STEP_PIN = 14;
const int DIR_PIN = 12;
const int horaObjetivo = 15;
const int minutoObjetivo = 30;

void setup() {
  Serial.begin(115200);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);

  // Conexión WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado al WiFi");

  // Zona horaria GMT-3
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // Esperar sincronización
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nHora sincronizada");
}

void moverMotor(int pasos, bool direccion) {
  digitalWrite(DIR_PIN, direccion ? HIGH : LOW);
  for (int i = 0; i < pasos; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(800);  // velocidad
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(800);
  }
}

bool yaEjecutado = false;

void loop() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.printf("Hora: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    if (timeinfo.tm_hour == horaObjetivo &&
        timeinfo.tm_min == minutoObjetivo &&
        !yaEjecutado) {
      Serial.println("Moviendo motor...");
      moverMotor(200, true); // 200 pasos en dirección "true"
      yaEjecutado = true;
    }

    if (timeinfo.tm_min != minutoObjetivo) {
      yaEjecutado = false; // Permite volver a ejecutar al día siguiente
    }
  }

  delay(1000);
}