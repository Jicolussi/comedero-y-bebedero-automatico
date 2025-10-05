#include <Arduino.h>
#define PIN_SENSOR 34   
#define PIN_RELE 25   


#define NIVEL_BAJO 1200   
#define NIVEL_ALTO 2500   

bool valvulaAbierta = false;
unsigned long tiempoAnterior = 0;
const unsigned long intervaloLectura = 500;  

void setup() {
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, LOW);  
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
}
