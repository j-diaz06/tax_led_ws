#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

// --- HARDWARE ---
#define PIN_MATRIZ    2
#define PIN_TAXIMETRO 9

// --- UUIDs DEL SERVICIO BLE ---
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, PIN_MATRIZ,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

// --- COLORES ---
uint16_t C_VERDE      = matrix.Color(0, 255, 0);
uint16_t C_ROJO       = matrix.Color(255, 0, 0);
uint16_t C_NEQUI      = matrix.Color(225, 0, 152);
uint16_t C_BLANCO     = matrix.Color(255, 255, 255);
uint16_t C_AMARILLO   = matrix.Color(255, 255, 0);
uint16_t C_NARANJA    = matrix.Color(255, 128, 0);
uint16_t C_CIAN       = matrix.Color(0, 255, 255);
uint16_t C_MORADO     = matrix.Color(155, 89, 182);
uint16_t C_BREB_AZUL  = matrix.Color(77, 195, 232);
uint16_t C_BREB_VERDE = matrix.Color(109, 226, 150);

// --- VARIABLES DE ESTADO ---
volatile bool taxiOcupado = false;
int modoApp = 0;           // 0:Normal, 1:Nequi, 2:Fuera, 3:Pico, 4:Custom, 5:Reportado, 6:Ocupado Manual
int brilloGlobal = 15;
int velScroll = 60;
int xScroll = 32;
int estadoLibre = 0;       // 0:Libre, 1:Nequi, 2:Bre-B

unsigned long tiempoUltimoFrame = 0;
unsigned long tiempoAlternancia = 0;
String textoUsuario = "HOLA";

// Cola simple para procesar comandos BLE de forma segura en el loop()
volatile bool hayComandoBLE = false;
String comandoPendiente = "";

// --- CALLBACKS BLE ---
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("BLE: Celular conectado");
  }
  void onDisconnect(BLEServer* pServer) {
    Serial.println("BLE: Desconectado. Reiniciando publicidad...");
    BLEDevice::startAdvertising(); // Vital para poder reconectar luego
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rx = String(pCharacteristic->getValue().c_str());
    rx.trim();
    if (rx.length() > 0) {
      comandoPendiente = rx;
      hayComandoBLE = true;
    }
  }
};

// --- INTERRUPCIÓN HARDWARE (TAXÍMETRO) ---
void IRAM_ATTR ISR_Taximetro() {
  taxiOcupado = (digitalRead(PIN_TAXIMETRO) == LOW);
}

void procesarComando(String cmd) {
  Serial.print("Comando recibido: ");
  Serial.println(cmd);

  if (cmd.startsWith("M:")) {
    modoApp = cmd.substring(2).toInt();
    xScroll = 32;
    estadoLibre = 0;
    tiempoAlternancia = millis();
  } 
  else if (cmd.startsWith("B:")) {
    brilloGlobal = constrain(cmd.substring(2).toInt(), 5, 100);
    matrix.setBrightness(brilloGlobal);
  } 
  else if (cmd.startsWith("V:")) {
    velScroll = constrain(cmd.substring(2).toInt(), 20, 150);
  } 
  else if (cmd.startsWith("T:")) {
    textoUsuario = cmd.substring(2);
    textoUsuario.toUpperCase();
    modoApp = 4;
    xScroll = 32;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TAXIMETRO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TAXIMETRO), ISR_Taximetro, CHANGE);

  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(brilloGlobal);

  // --- INICIALIZAR BLE ---
  BLEDevice::init("Taxi_LED");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pCharacteristic->setCallbacks(new CommandCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("Sistema listo. Buscame por BLE como: Taxi_LED");
}

void loop() {
  unsigned long ms = millis();

  // 1. Procesar comandos entrantes por BLE
  if (hayComandoBLE) {
    String cmd = comandoPendiente;
    hayComandoBLE = false;
    procesarComando(cmd);
  }

  // 2. Simulador Serial (para pruebas en escritorio)
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'T' || c == 't') { taxiOcupado = true; xScroll = 32; }
    else if (c == 'L' || c == 'l') { taxiOcupado = false; xScroll = 32; }
  }

  // 3. Animación de Matriz
  if (ms - tiempoUltimoFrame > velScroll) {
    matrix.fillScreen(0);

    // Señal física del taxímetro O botón manual "OCUPADO"
    if (taxiOcupado || modoApp == 6) {
      matrix.setTextColor(C_ROJO);
      matrix.setCursor(xScroll, 1);
      matrix.print("OCUPADO");
      xScroll--;
      if (xScroll < -42) xScroll = 32;
    } 
    else {
      if (modoApp == 5) {
        matrix.setTextColor(C_MORADO);
        matrix.setCursor(xScroll, 1);
        matrix.print("REPORTADO");
        xScroll--;
        if (xScroll < -55) xScroll = 32;
      }
      else if (modoApp == 2) {
        matrix.setTextColor(C_AMARILLO);
        matrix.setCursor(xScroll, 1);
        matrix.print("FUERA DE SERVICIO");
        xScroll--;
        if (xScroll < -102) xScroll = 32;
      }
      else if (modoApp == 3) {
        matrix.setTextColor(C_NARANJA);
        matrix.setCursor(xScroll, 1);
        matrix.print("PICO Y PLACA");
        xScroll--;
        if (xScroll < -72) xScroll = 32;
      }
      else if (modoApp == 4) {
        matrix.setTextColor(C_CIAN);
        matrix.setCursor(xScroll, 1);
        matrix.print(textoUsuario);
        xScroll--;
        int anchoTxt = textoUsuario.length() * 6;
        if (xScroll < -anchoTxt) xScroll = 32;
      }
      else if (modoApp == 1) {
        matrix.fillRect(0, 1, 2, 2, C_NEQUI);
        matrix.setTextColor(C_BLANCO);
        matrix.setCursor(3, 1);
        matrix.print("NEQUI");
      }
      else {
        // Modo 0: Alternancia (Libre 5s -> Nequi 2s -> Bre-B 2s)
        if (estadoLibre == 0 && ms - tiempoAlternancia > 5000) {
          estadoLibre = 1;
          tiempoAlternancia = ms;
        } else if (estadoLibre == 1 && ms - tiempoAlternancia > 2000) {
          estadoLibre = 2;
          tiempoAlternancia = ms;
        } else if (estadoLibre == 2 && ms - tiempoAlternancia > 2000) {
          estadoLibre = 0;
          tiempoAlternancia = ms;
        }

        if (estadoLibre == 1) {
          matrix.fillRect(0, 1, 2, 2, C_NEQUI);
          matrix.setTextColor(C_BLANCO);
          matrix.setCursor(3, 1);
          matrix.print("NEQUI");
        }
        else if (estadoLibre == 2) {
          matrix.setCursor(1, 1);
          matrix.setTextColor(C_BREB_AZUL);
          matrix.print("B");
          matrix.setTextColor(C_BREB_VERDE);
          matrix.print("re-B");
        }
        else {
          matrix.setTextColor(C_VERDE);
          matrix.setCursor(1, 1);
          matrix.print("LIBRE");
        }
      }
    }

    matrix.show();
    tiempoUltimoFrame = ms;
  }
}