// Sophia Franke - 23030
// Laboratorio 3: Contador Manual y Automático con Alarma
// Fecha: 2025-08-08 
#include <Arduino.h>

// ─────── ❥ ⊱ Definición de Pines ⊰ ❥ ───────
#define PB_INC        19  // Botón físico para incrementar el contador manual
#define PB_DEC        18  // Botón físico para decrementar el contador manual
#define CABLE_RESET    2  // Sensor capacitivo para reiniciar el contador automático

#define LED1          13  // LED bit 0 del contador manual
#define LED2          12  // LED bit 1 del contador manual
#define LED3          14  // LED bit 2 del contador manual
#define LED4          27  // LED bit 3 del contador manual

#define LEDA          26  // LED bit 0 del contador automático
#define LEDB          25  // LED bit 1 del contador automático
#define LEDC          33  // LED bit 2 del contador automático
#define LEDD          32  // LED bit 3 del contador automático

#define LED_ALARM      4  // LED indicador de coincidencia (alarma)

// ─────── ❥ ⊱ Prototipado de Funciones ⊰ ❥ ───────
void IRAM_ATTR onIncManual();       // ISR para botón de incremento
void IRAM_ATTR onDecManual();       // ISR para botón de decremento
void IRAM_ATTR onAutoTick();        // ISR para timer automático

void mostrarManual(uint8_t valor);  // Muestra el valor del contador manual en LEDs
void mostrarAuto(uint8_t valor);    // Muestra el valor del contador automático en LEDs

// ─────── ❥ ⊱ Variables Globales ⊰ ❥ ───────
volatile uint8_t contadorManual = 0;     // Contador controlado por botones
volatile uint8_t contadorAuto   = 0;     // Contador que incrementa automáticamente
volatile bool incManual = false;         // Bandera para ISR de incremento
volatile bool decManual = false;         // Bandera para ISR de decremento
volatile bool autoTick  = false;         // Bandera para ISR del timer

bool estadoAlarma = false;               // Estado actual del LED de alarma
bool coincidenciaAnterior = false;       // Para evitar múltiples activaciones por coincidencia

hw_timer_t* timer0_Cfg = nullptr;        // Timer de hardware para el contador automático


// ─────── ❥ ⊱ Configuración Inicial del Sistema ⊰ ❥ ───────
void setup() {
  Serial.begin(115200);

  // Configurar pines de entrada y salida
  pinMode(PB_INC, INPUT_PULLUP);
  pinMode(PB_DEC, INPUT_PULLUP);
  pinMode(CABLE_RESET, INPUT);

  // Configurar pines de LEDs del contador manual
  pinMode(LED1, OUTPUT); 
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT); 
  pinMode(LED4, OUTPUT);

  // Configurar pines de LEDs del contador automático
  pinMode(LEDA, OUTPUT); 
  pinMode(LEDB, OUTPUT);
  pinMode(LEDC, OUTPUT); 
  pinMode(LEDD, OUTPUT);

  // Configurar LED de alarma
  pinMode(LED_ALARM, OUTPUT);
  // Inicializar LED de alarma apagado
  digitalWrite(LED_ALARM, LOW);

  // Configurar interrupciones de botones
  attachInterrupt(PB_INC, onIncManual, FALLING); // Interrupción por flanco descendente. Esto asegura que la interrupción se dispare al presionar el botón.
  attachInterrupt(PB_DEC, onDecManual, FALLING); // Interrupción por flanco descendente. Esto asegura que la interrupción se dispare al presionar el botón.

  // Configurar timer automático para generar ticks cada 250 ms
  timer0_Cfg = timerBegin(0, 80, true); // Prescaler 80 → 1 µs por tick
  timerAttachInterrupt(timer0_Cfg, &onAutoTick, true);
  timerAlarmWrite(timer0_Cfg, 250000, true); // 250 ms
  timerAlarmEnable(timer0_Cfg);
  //El timer se configura para que se dispare cada 250 ms, lo que permite incrementar el contador automático a esa frecuencia.

  // Mostrar valores iniciales en LEDs
  mostrarManual(contadorManual);
  mostrarAuto(contadorAuto);
}

// ─────── ❥ ⊱ Bucle Principal del Programa ⊰ ❥ ───────
void loop() {
  static unsigned long tLastInc = 0; // 1. Variable para controlar el tiempo del último incremento manual
  static unsigned long tLastDec = 0; // 2. Variable para controlar el tiempo del último decremento manual
  unsigned long ahora = millis(); //  // 3. Obtener el tiempo actual en milisegundos

  // ── Manejo del botón de incremento ──
  if (incManual) { // 4. Verificar si se ha activado la bandera de incremento manual
    incManual = false;

    if (ahora - tLastInc > 50) { // 5. Verificar si ha pasado suficiente tiempo desde el último incremento
      // Incrementar el contador manual y mostrarlo en LEDs
      contadorManual = (contadorManual + 1) & 0x0F; // 6. Incrementar el contador manual y aplicar máscara para limitar a 4 bits. Esta máscara asegura que el contador manual no exceda el valor máximo de 15 (0x0F).
      mostrarManual(contadorManual); // 7. Actualizar los LEDs del contador manual
      tLastInc = ahora; // 8. Actualizar el tiempo del último incremento
    }
  }

  // 9. Verificar si se ha activado la bandera de decremento manual y manejar el botón de decremento
  //    de manera similar al incremento, asegurando que no se decremente más allá de 0.
  //    Esto asegura que el contador manual no se vuelva negativo y resete a 15 si se decrece desde 0.
  // ── Manejo del botón de decremento ──
  if (decManual) {
    decManual = false;

    if (ahora - tLastDec > 50) {
      contadorManual = (contadorManual - 1) & 0x0F;
      mostrarManual(contadorManual);
      tLastDec = ahora;
    }
  }

  // ── Incremento automático cada 250 ms ──
  if (autoTick) {   // 10. Verificar si se ha activado la bandera de tick automático
    autoTick = false;
// Incrementar el contador automático y mostrarlo en LEDs
    // Se utiliza una máscara para limitar el contador automático a 4 bits, asegurando que no exceda el valor máximo de 15 (0x0F).
    contadorAuto = (contadorAuto + 1) & 0x0F;
    mostrarAuto(contadorAuto);
  }

  // ── Comparación entre contadores y activación de alarma ──
  if (contadorManual == contadorAuto) { // 11. Verificar si los contadores manual y automático coinciden
    // Si los contadores coinciden, activar el LED de alarma
    if (!coincidenciaAnterior) {
      // Cambiar estado del LED de alarma (ON/OFF alternado)
      estadoAlarma = !estadoAlarma;
      digitalWrite(LED_ALARM, estadoAlarma);

      // Reiniciar contador automático al detectar coincidencia
      contadorAuto = 0;
      mostrarAuto(contadorAuto);

      coincidenciaAnterior = true;
    }
  }
  else { // 12. Si no coinciden, apagar el LED de alarma
    coincidenciaAnterior = false;
  }

  // ── Reinicio del contador automático usando sensor capacitivo ──
  if (touchRead(CABLE_RESET) < 30) {
    contadorAuto = 0; // Reiniciar contador automático al tocar el sensor o sea cuando el cable capacitivo detecta un toque.
    mostrarAuto(contadorAuto);
    delay(200); // Antirrebote para el sensor táctil
  }
}


// ─────── ❥ ⊱ ISR: Botón Incrementar Manual ⊰ ❥ ───────
void IRAM_ATTR onIncManual() {
  incManual = true;
}

// ─────── ❥ ⊱ ISR: Botón Decrementar Manual ⊰ ❥ ───────
void IRAM_ATTR onDecManual() {
  decManual = true;
}

// ─────── ❥ ⊱ ISR: Timer Automático ⊰ ❥ ───────
void IRAM_ATTR onAutoTick() {
  autoTick = true;
}

// ─────── ❥ ⊱ Mostrar LEDs del Contador Manual ⊰ ❥ ───────
void mostrarManual(uint8_t valor) {
  digitalWrite(LED1, valor & 0x01);
  digitalWrite(LED2, (valor >> 1) & 0x01);
  digitalWrite(LED3, (valor >> 2) & 0x01);
  digitalWrite(LED4, (valor >> 3) & 0x01);
}

// ─────── ❥ ⊱ Mostrar LEDs del Contador Automático ⊰ ❥ ───────
void mostrarAuto(uint8_t valor) {
  digitalWrite(LEDA, valor & 0x01);
  digitalWrite(LEDB, (valor >> 1) & 0x01); // La mascara (valor >> 1) & 0x01 asegura que solo se muestre el bit 1 del contador automático. El desplazamiento a la derecha (>> 1) mueve el bit 1 al bit 0, y la máscara & 0x01 asegura que solo se muestre ese bit. Esto se realiza para que cada LED represente un bit del contador automático dado que los LEDs están conectados a los bits del contador automático.
  digitalWrite(LEDC, (valor >> 2) & 0x01);
  digitalWrite(LEDD, (valor >> 3) & 0x01);
}

// ─────── ❥ ⊱ Fin del Código ⊰ ❥ ───────
