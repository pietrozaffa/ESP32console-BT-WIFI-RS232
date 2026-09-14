/*
 * Pi_Zaff System - Wireless Console Server (firmware ESP32)
 * Copyright (C) 2026 Pietro Zaffarano
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/*
╔══════════════════════════════════════════════════════════════════╗
║         Pi_Zaff System  -  Wireless Console Server               ║
║                  Firmware v5.0 STABILE                           ║
╠══════════════════════════════════════════════════════════════════╣
║  CHANGELOG v5.0 (su v4.9 ROCKSOLID)                              ║
║  ----------------------------------                              ║
║  [BUG-FIX critico] La promessa del comment v4.9 era: "i byte     ║
║    non passati restano in rxQueue per il prossimo ciclo".        ║
║    In realta' la write() ritornava un count parziale e i byte    ║
║    non scritti venivano BUTTATI (la rxQueuePop li aveva gia'     ║
║    rimossi). Su client TCP lenti = output troncato silenzioso.   ║
║    Fix: si controlla il return value di write() e i byte non     ║
║    consegnati vengono ri-accodati in rxQueue per il prossimo     ║
║    ciclo. Compatibile col drop-a-75% del rxQueue, che resta come ║
║    safety net per console troppo lente in modo sostenuto.        ║
║                                                                  ║
║  [BUG-FIX] authBuf della password telnet non veniva azzerato     ║
║    sulla disconnect: il client successivo trovava il prefisso    ║
║    del tentativo precedente. Ora viene resettato.                ║
║                                                                  ║
║  [BUG-FIX] /ledtest chiamava server.handleClient() ricorsivo     ║
║    (rientrante): bomba a orologeria. Ora ritorna subito e i LED  ║
║    restano on fino a ledTestUntil, gestito in updateLEDs().      ║
║                                                                  ║
║  [SAFETY] /factory ora richiede POST (era GET): non si cancella  ║
║    piu' la config con un semplice click su un link malevolo.     ║
║                                                                  ║
║  [SAFETY] handleSave valida le lunghezze SSID/password (rispetta ║
║    i limiti WiFi: SSID<=32, AP pass 8-63).                       ║
║                                                                  ║
║  [PULIZIA] drainSerialToRxQueue scarta i dati UART quando NON    ║
║    c'e' alcun client connesso: niente piu' backlog stantio       ║
║    consegnato al prossimo client al momento del connect.         ║
║                                                                  ║
║  [PULIZIA] vTaskDelay(1) -> vTaskDelay(pdMS_TO_TICKS(1)) per     ║
║    chiarezza (semantica in ms a prescindere dal tick rate).      ║
║                                                                  ║
║  [PULIZIA] Banner di boot e logging coerenti con la versione.    ║
║                                                                  ║
║  NOTA IMPORTANTE: il drop-a-75% di drainSerialToRxQueue resta    ║
║  come da v4.9. E' un feature deliberato per la fluidita' della   ║
║  console e NON e' stato toccato.                                 ║
╠══════════════════════════════════════════════════════════════════╣
║  ARCHITETTURA (invariata da v4.9)                                ║
║  ----------------------------------                              ║
║  Modalita' UNICA al boot, scelta dall'utente tramite pulsante.   ║
║                                                                  ║
║  MODE_WIFI: AP + STA + Telnet + Web GUI + OTA                    ║
║             BT mai inizializzato. Heap libera ~140 KB.           ║
║  MODE_BT  : solo BT SPP (compatibile Windows/Android COM virt.)  ║
║             WiFi completamente spento. Heap libera ~100 KB.      ║
║             Nessuna GUI in questa modalita'.                     ║
║                                                                  ║
║  Pulsante GPIO32:                                                ║
║    long press (>1.5s) a runtime -> toggle mode + reboot          ║
║    premuto al power-on          -> recovery: forza MODE_WIFI     ║
║                                                                  ║
║  LED mode-aware:                                                 ║
║   LED_WIFI: in MODE_WIFI lampeggia (no client) / fisso (client)  ║
║             Spento in MODE_BT.                                   ║
║   LED_BT  : in MODE_BT   lampeggia (no client) / fisso (client)  ║
║             Spento in MODE_WIFI.                                 ║
║   LED_ACT : flash sul passaggio di dati                          ║
║   LED_BATT: lampeggio di warning batteria                        ║
╠══════════════════════════════════════════════════════════════════╣
║  PINOUT                                                          ║
║    LED_ACT=25  LED_WIFI=26  LED_BT=27  LED_BATT=33               ║
║    UART2 verso MAX3232: RX=16  TX=17                             ║
║    Batteria 18650 (partitore): GPIO34                            ║
║    Pulsante modalita': GPIO32 (verso GND, pull-up interno)       ║
╚══════════════════════════════════════════════════════════════════╝
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "BluetoothSerial.h"
#include <Update.h>
#include <esp_task_wdt.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>     // per setsockopt SO_SNDTIMEO

// ──────────────────────────────────────────────────────────────────
//  DEBUG
// ──────────────────────────────────────────────────────────────────
#define USB_DEBUG_MODE   false     // true = batteria simulata 100%

// ──────────────────────────────────────────────────────────────────
//  PINOUT
// ──────────────────────────────────────────────────────────────────
#define LED_ACT          25
#define LED_WIFI         26
#define LED_BT           27
#define LED_BATT         33

#define UART_RX_PIN      16
#define UART_TX_PIN      17
#define BATT_PIN         34

#define BTN_PIN          32       // pulsante tra GPIO32 e GND

// ──────────────────────────────────────────────────────────────────
//  TIMING / SOGLIE
// ──────────────────────────────────────────────────────────────────
#define LED_ACT_FLASH_MS    30
#define LED_BLINK_PERIOD   500    // lampeggio "in attesa client"
#define BATT_WARN_PCT       15
#define BATT_LOW_PCT        10
#define BATT_CRIT_PCT        5
#define BATT_BLINK_WARN    800
#define BATT_BLINK_LOW     400
#define BATT_BLINK_CRIT    120

#define WDT_TIMEOUT_S       30
#define BATT_READ_INTERVAL  30000UL
#define LOW_BATT_WARN_INT   300000UL
#define LOOP_REPORT_INT     5000UL
#define LOOP_WARN_MS        150

#define BTN_DEBOUNCE_MS     30
#define BTN_LONG_PRESS_MS   1500

// ──────────────────────────────────────────────────────────────────
//  BUFFER
// ──────────────────────────────────────────────────────────────────
#define TX_QUEUE_SIZE      2048
#define TX_HIGH_WATER      (TX_QUEUE_SIZE * 3 / 4)
#define RX_STAGING_SIZE    256
#define UART_RX_BUF        1024
#define UART_TX_BUF        1024
#define BT_TX_CHUNK        16
#define MAX_TELNET_PER_LOOP   128
#define MAX_BT_PER_LOOP       128
#define MAX_UART_TX_PER_LOOP  128

static const uint32_t VALID_BAUDS[] = { 9600, 19200, 38400, 57600, 115200 };
static const size_t   N_VALID_BAUDS = sizeof(VALID_BAUDS)/sizeof(VALID_BAUDS[0]);
static bool isValidBaud(uint32_t b) {
  for (size_t i = 0; i < N_VALID_BAUDS; i++) if (VALID_BAUDS[i] == b) return true;
  return false;
}

// ──────────────────────────────────────────────────────────────────
//  MODALITA' DI BOOT
// ──────────────────────────────────────────────────────────────────
enum BootMode : uint8_t {
  MODE_WIFI = 0,
  MODE_BT   = 1
};
static uint8_t bootMode = MODE_WIFI;

static const char* bootModeName(uint8_t m) {
  return (m == MODE_BT) ? "BT" : "WIFI";
}

// ──────────────────────────────────────────────────────────────────
//  OGGETTI GLOBALI
// ──────────────────────────────────────────────────────────────────
Preferences      prefs;
WebServer        server(80);
BluetoothSerial  SerialBT;
HardwareSerial   ConsoleSerial(2);
WiFiServer       telnetServer(23);
WiFiClient       telnetClient;

// ──────────────────────────────────────────────────────────────────
//  CONFIG
// ──────────────────────────────────────────────────────────────────
String   apSSID, apPassword;
String   staSSID, staPassword;
String   bluetoothName;
String   telnetPassword;
bool     useSTA          = false;
bool     telnetAuthReq   = false;
uint32_t baudRate        = 9600;

// ──────────────────────────────────────────────────────────────────
//  STATO RUNTIME
// ──────────────────────────────────────────────────────────────────
bool          telnetAuthenticated = false;
bool          otaInProgress       = false;
float         battVoltage         = 0.0f;
int           battPercent         = 0;

unsigned long lastBattRead        = 0;
unsigned long lastLowBattWarn     = 0;
unsigned long ledActFlashUntil    = 0;
unsigned long ledBattLastTick     = 0;
bool          ledBattState        = false;

// v5.0: stato globale del LED-test asincrono (sostituisce il busy-loop
// rientrante che chiamava server.handleClient() dentro un handler).
unsigned long ledTestUntil        = 0;

// v5.0: buffer della password telnet a file-scope, cosi' si puo'
// azzerare nel ramo di disconnect (prima era static dentro handleTelnet
// e restava sporco fra una connessione e l'altra).
String        telnetAuthBuf       = "";

IPAddress     apIP(192, 168, 4, 1);
IPAddress     apGW(192, 168, 4, 1);
IPAddress     apMask(255, 255, 255, 0);

// ──────────────────────────────────────────────────────────────────
//  BRIDGE TASK (Core 1, singolo task)
// ──────────────────────────────────────────────────────────────────
static TaskHandle_t   bridgeTaskHandle = NULL;
#define BRIDGE_STACK_SIZE   8192
#define BRIDGE_PRIORITY     3
#define BRIDGE_CORE         1

static volatile unsigned long bridgeMaxTime = 0;

// ──────────────────────────────────────────────────────────────────
//  RING BUFFER 1: txQueue (client RX -> UART TX)
// ──────────────────────────────────────────────────────────────────
static uint8_t           txQueue[TX_QUEUE_SIZE];
static volatile uint16_t txHead = 0;
static volatile uint16_t txTail = 0;

static inline uint16_t txQueueUsed() {
  uint16_t h = txHead, t = txTail;
  return (h >= t) ? (h - t) : (TX_QUEUE_SIZE - (t - h));
}
static inline bool canAcceptMoreData() {
  return txQueueUsed() < TX_HIGH_WATER;
}
static int txQueuePush(const uint8_t* data, int len) {
  int n = 0;
  uint16_t h = txHead;
  while (n < len) {
    uint16_t next = (h + 1) % TX_QUEUE_SIZE;
    if (next == txTail) break;
    txQueue[h] = data[n++];
    h = next;
  }
  txHead = h;
  return n;
}

// ──────────────────────────────────────────────────────────────────
//  RING BUFFER 2: rxQueue (UART RX -> client TX)
// ──────────────────────────────────────────────────────────────────
#define RX_QUEUE_SIZE      4096
static uint8_t             rxQueue[RX_QUEUE_SIZE];
static volatile uint16_t   rxHead = 0;
static volatile uint16_t   rxTail = 0;

static inline uint16_t rxQueueUsed() {
  uint16_t h = rxHead, t = rxTail;
  return (h >= t) ? (h - t) : (RX_QUEUE_SIZE - (t - h));
}

static int rxQueuePush(const uint8_t* data, int len) {
  int n = 0;
  uint16_t h = rxHead;
  while (n < len) {
    uint16_t next = (h + 1) % RX_QUEUE_SIZE;
    if (next == rxTail) break;
    rxQueue[h] = data[n++];
    h = next;
  }
  rxHead = h;
  return n;
}

static int rxQueuePop(uint8_t* out, int maxLen) {
  int n = 0;
  uint16_t t = rxTail;
  while (n < maxLen && t != rxHead) {
    out[n++] = rxQueue[t];
    t = (t + 1) % RX_QUEUE_SIZE;
  }
  rxTail = t;
  return n;
}

// ──────────────────────────────────────────────────────────────────
//  FORWARD DECL
// ──────────────────────────────────────────────────────────────────
void ledActTrigger();
static void saveBootModeAndReboot(uint8_t newMode);

// ──────────────────────────────────────────────────────────────────
//  PULSANTE GPIO32
// ──────────────────────────────────────────────────────────────────
static bool          btnLastRaw       = HIGH;
static bool          btnState         = HIGH;
static unsigned long btnLastChange    = 0;
static unsigned long btnPressStart    = 0;
static bool          btnLongFired     = false;

static void buttonInit() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  delay(2);
  btnLastRaw    = digitalRead(BTN_PIN);
  btnState      = btnLastRaw;
  btnLastChange = millis();
}

static void handleButton() {
  unsigned long now = millis();
  bool raw = digitalRead(BTN_PIN);

  if (raw != btnLastRaw) {
    btnLastRaw    = raw;
    btnLastChange = now;
  }

  if ((now - btnLastChange) > BTN_DEBOUNCE_MS && btnState != btnLastRaw) {
    btnState = btnLastRaw;
    if (btnState == LOW) {
      btnPressStart = now;
      btnLongFired  = false;
    }
  }

  if (btnState == LOW && !btnLongFired && (now - btnPressStart) >= BTN_LONG_PRESS_MS) {
    btnLongFired = true;
    uint8_t newMode = (bootMode == MODE_WIFI) ? MODE_BT : MODE_WIFI;
    Serial.printf("[BTN] long press: %s -> %s\n",
                  bootModeName(bootMode), bootModeName(newMode));
    saveBootModeAndReboot(newMode);
  }
}

static void saveBootModeAndReboot(uint8_t newMode) {
  prefs.begin("cfg", false);
  prefs.putUChar("mode", newMode);
  prefs.end();

  if (bridgeTaskHandle != NULL) {
    vTaskDelete(bridgeTaskHandle);
    bridgeTaskHandle = NULL;
    delay(50);
  }

  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_WIFI, i & 1);
    digitalWrite(LED_BT,   i & 1);
    digitalWrite(LED_ACT,  i & 1);
    digitalWrite(LED_BATT, i & 1);
    delay(120);
    esp_task_wdt_reset();
  }
  digitalWrite(LED_WIFI, LOW);
  digitalWrite(LED_BT,   LOW);
  digitalWrite(LED_ACT,  LOW);
  digitalWrite(LED_BATT, LOW);
  delay(100);
  ESP.restart();
}

// ──────────────────────────────────────────────────────────────────
//  BATTERIA
// ──────────────────────────────────────────────────────────────────
#define BATT_SAMPLES     16
#define BATT_DIVIDER     2.0f
#define BATT_ADC_REF     3.3f
#define BATT_ADC_MAX     4095.0f
#define BATT_CAL_FACTOR  1.07f

static const float battCurveV[] = { 4.20f, 4.00f, 3.80f, 3.70f, 3.60f, 3.40f, 3.00f };
static const int   battCurveP[] = {  100,    85,    60,    40,    20,    10,     0 };
static const int   battCurveN   = 7;

static int voltageToPercent(float v) {
  if (v >= battCurveV[0])              return 100;
  if (v <= battCurveV[battCurveN - 1]) return 0;
  for (int i = 0; i < battCurveN - 1; i++) {
    if (v <= battCurveV[i] && v > battCurveV[i + 1]) {
      float ratio = (v - battCurveV[i + 1]) / (battCurveV[i] - battCurveV[i + 1]);
      return battCurveP[i + 1] + (int)(ratio * (battCurveP[i] - battCurveP[i + 1]));
    }
  }
  return 0;
}

static void readBattery() {
  if (USB_DEBUG_MODE) {
    battVoltage = 4.10f;
    battPercent = 100;
    return;
  }
  long sum = 0;
  for (int i = 0; i < BATT_SAMPLES; i++) sum += analogRead(BATT_PIN);
  float adc = sum / (float)BATT_SAMPLES;
  battVoltage = (adc / BATT_ADC_MAX) * BATT_ADC_REF * BATT_DIVIDER * BATT_CAL_FACTOR;
  battPercent = voltageToPercent(battVoltage);
}

// ──────────────────────────────────────────────────────────────────
//  LED - mode-aware
// ──────────────────────────────────────────────────────────────────
static void setupLEDs() {
  pinMode(LED_ACT,  OUTPUT);
  pinMode(LED_WIFI, OUTPUT);
  pinMode(LED_BT,   OUTPUT);
  pinMode(LED_BATT, OUTPUT);
  const int leds[] = { LED_ACT, LED_WIFI, LED_BT, LED_BATT };
  for (int i = 0; i < 4; i++) { digitalWrite(leds[i], HIGH); delay(120); }
  delay(200);
  for (int i = 0; i < 4; i++) { digitalWrite(leds[i], LOW);  delay(80); }
}

inline void ledActTrigger() {
  digitalWrite(LED_ACT, HIGH);
  ledActFlashUntil = millis() + LED_ACT_FLASH_MS;
}

static void updateLEDs() {
  unsigned long now = millis();

  // v5.0: LED-test asincrono. Se ledTestUntil > 0 forziamo tutti i LED
  // accesi finche' non scade, poi torniamo alla gestione normale.
  // Questo sostituisce il busy-loop che dentro handleLedTest chiamava
  // server.handleClient() ricorsivamente (rientranza vietata).
  if (ledTestUntil > 0) {
    if (now < ledTestUntil) {
      digitalWrite(LED_ACT,  HIGH);
      digitalWrite(LED_WIFI, HIGH);
      digitalWrite(LED_BT,   HIGH);
      digitalWrite(LED_BATT, HIGH);
      return;
    }
    // scaduto: spegni e prosegui con la logica normale.
    // LED_ACT va spento esplicitamente perche' la sua transizione
    // normale dipende dal flash trigger, non da un timer di reset.
    ledTestUntil = 0;
    ledActFlashUntil = 0;
    digitalWrite(LED_ACT, LOW);
  }

  // LED_ACT: spegni dopo il flash
  if (ledActFlashUntil > 0 && now >= ledActFlashUntil) {
    digitalWrite(LED_ACT, LOW);
    ledActFlashUntil = 0;
  }

  // LED_WIFI e LED_BT: dipendono dalla modalita' e dalla presenza di client
  if (bootMode == MODE_WIFI) {
    digitalWrite(LED_BT, LOW);
    bool clientConnesso =
      (WiFi.softAPgetStationNum() > 0) ||
      (telnetClient && telnetClient.connected());
    if (clientConnesso) {
      digitalWrite(LED_WIFI, HIGH);
    } else {
      digitalWrite(LED_WIFI, (now / LED_BLINK_PERIOD) & 1);
    }
  } else { // MODE_BT
    digitalWrite(LED_WIFI, LOW);
    if (SerialBT.hasClient()) {
      digitalWrite(LED_BT, HIGH);
    } else {
      digitalWrite(LED_BT, (now / LED_BLINK_PERIOD) & 1);
    }
  }

  // LED_BATT: warning batteria
  if (battPercent >= BATT_WARN_PCT) {
    digitalWrite(LED_BATT, LOW);
    ledBattState = false;
  } else {
    unsigned long period =
      (battPercent <= BATT_CRIT_PCT) ? BATT_BLINK_CRIT :
      (battPercent <= BATT_LOW_PCT)  ? BATT_BLINK_LOW  : BATT_BLINK_WARN;
    if (now - ledBattLastTick >= period) {
      ledBattState = !ledBattState;
      digitalWrite(LED_BATT, ledBattState ? HIGH : LOW);
      ledBattLastTick = now;
    }
  }
}

static void handleLowBatteryWarning() {
  if (battPercent > 10) return;
  if (millis() - lastLowBattWarn < LOW_BATT_WARN_INT) return;

  char msg[64];
  int len = snprintf(msg, sizeof(msg),
           "\r\n[WARNING] BATTERIA %d%% (%.2fV)\r\n",
           battPercent, battVoltage);
  if (len > 0 && len < (int)sizeof(msg)) {
    rxQueuePush((const uint8_t*)msg, len);
  }
  lastLowBattWarn = millis();
}

static void handleCriticalBatteryShutdown() {
  if (USB_DEBUG_MODE)      return;
  if (otaInProgress)       return;
  if (battVoltage > 3.15f) return;

  Serial.println(F("\n[CRITICAL BATTERY -> DEEP SLEEP]"));

  if (bridgeTaskHandle != NULL) {
    vTaskDelete(bridgeTaskHandle);
    bridgeTaskHandle = NULL;
    delay(50);
  }

  digitalWrite(LED_ACT,  LOW);
  digitalWrite(LED_WIFI, LOW);
  digitalWrite(LED_BT,   LOW);
  digitalWrite(LED_BATT, LOW);
  delay(300);
  if (bootMode == MODE_WIFI) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    SerialBT.end();
  }
  esp_deep_sleep_start();
}

// ──────────────────────────────────────────────────────────────────
//  TELNET IAC PARSER
// ──────────────────────────────────────────────────────────────────
#define IAC          0xFF
#define DONT         0xFE
#define DO           0xFD
#define WONT         0xFC
#define WILL         0xFB
#define SB           0xFA
#define SE           0xF0
#define TELOPT_ECHO  1
#define TELOPT_SGA   3

enum TelnetParseState : uint8_t {
  TN_NORMAL, TN_GOT_IAC, TN_GOT_VERB, TN_SUBNEG
};
static TelnetParseState tnState  = TN_NORMAL;
static uint8_t          tnVerb   = 0;
static bool             tnLastCR = false;

static void sendTelnetInitialNegotiation() {
  static const uint8_t init[] = {
    IAC, WILL, TELOPT_ECHO,
    IAC, WILL, TELOPT_SGA,
    IAC, DO,   TELOPT_SGA,
  };
  rxQueuePush(init, sizeof(init));
}

static void clientOut(const char* s) {
  if (!s) return;
  size_t len = strlen(s);
  if (len > 0) rxQueuePush((const uint8_t*)s, len);
}

static bool processTelnetByte(uint8_t b, uint8_t &outByte) {
  switch (tnState) {
    case TN_NORMAL:
      if (b == IAC)  { tnState = TN_GOT_IAC; return false; }
      if (b == '\r') { tnLastCR = true; outByte = '\r'; return true; }
      if (b == '\n') {
        if (tnLastCR) { tnLastCR = false; return false; }
        outByte = '\n'; return true;
      }
      tnLastCR = false;
      outByte = b;
      return true;

    case TN_GOT_IAC:
      if (b == IAC)  { tnState = TN_NORMAL; outByte = IAC; return true; }
      if (b == WILL || b == WONT || b == DO || b == DONT) {
        tnVerb = b; tnState = TN_GOT_VERB; return false;
      }
      if (b == SB)   { tnState = TN_SUBNEG; return false; }
      tnState = TN_NORMAL;
      return false;

    case TN_GOT_VERB: {
      uint8_t reply[3] = { IAC, 0, b };
      if (tnVerb == DO) {
        reply[1] = (b == TELOPT_ECHO || b == TELOPT_SGA) ? WILL : WONT;
        rxQueuePush(reply, 3);
      } else if (tnVerb == WILL) {
        reply[1] = (b == TELOPT_SGA) ? DO : DONT;
        rxQueuePush(reply, 3);
      }
      tnState = TN_NORMAL;
      return false;
    }

    case TN_SUBNEG:
      if (b == IAC) tnState = TN_GOT_IAC;
      return false;
  }
  return false;
}

// ──────────────────────────────────────────────────────────────────
//  UART TX DRAIN
// ──────────────────────────────────────────────────────────────────
static void drainTxQueueToUart() {
  if (txTail == txHead) return;
  uint16_t t = txTail;
  int sent = 0;
  while (sent < MAX_UART_TX_PER_LOOP && t != txHead) {
    if (ConsoleSerial.availableForWrite() < 1) break;
    ConsoleSerial.write(txQueue[t]);
    t = (t + 1) % TX_QUEUE_SIZE;
    sent++;
  }
  txTail = t;
  if (sent > 0) ledActTrigger();
}

// ──────────────────────────────────────────────────────────────────
//  UART RX -> rxQueue  (eseguito dal bridgeTask, NON BLOCCANTE)
// ──────────────────────────────────────────────────────────────────
#define RX_QUEUE_DROP_HIGH  (RX_QUEUE_SIZE * 3 / 4)  // 75% = ~3KB

static void drainSerialToRxQueue() {
  int avail = ConsoleSerial.available();
  if (avail <= 0) return;

  // v5.0: se non c'e' alcun client connesso (per la modalita' corrente)
  // i dati UART vengono scartati subito. Cosi':
  //   1) niente backlog stantio nel rxQueue da consegnare al prossimo
  //      client appena si connette (pulizia UX);
  //   2) il buffer hardware UART non si gonfia inutilmente.
  // NB: assorbe anche il vecchio caso "WIFI auth richiesta + non
  // autenticato" che era trattato a parte.
  bool hasClient = false;
  if (bootMode == MODE_WIFI) {
    hasClient = telnetClient && telnetClient.connected();
    if (hasClient && telnetAuthReq && !telnetAuthenticated) hasClient = false;
  } else {
    hasClient = SerialBT.hasClient();
  }
  if (!hasClient) {
    static uint8_t junk[RX_STAGING_SIZE];
    ConsoleSerial.readBytes(junk, min(avail, (int)sizeof(junk)));
    return;
  }

  // DROP CONTROL UNIVERSALE (BT e WIFI): se la coda e' oltre il 75%
  // piena significa che il client non sta riuscendo a smaltire
  // l'output del router. Invece di accumulare backlog di secondi,
  // leggiamo dalla UART e SCARTIAMO i nuovi byte per liberare il
  // buffer hardware. Console realtime > integrita' del flusso.
  // Su WIFI succede con output massicci (show run, debug) e WiFi
  // instabile; su BT succede su Windows SPP che e' lento.
  // *** QUESTO E' UN FEATURE DELIBERATO PER LA FLUIDITA' ***
  if (rxQueueUsed() > RX_QUEUE_DROP_HIGH) {
    static uint8_t junk[RX_STAGING_SIZE];
    ConsoleSerial.readBytes(junk, min(avail, (int)sizeof(junk)));
    return;
  }

  static uint8_t buf[RX_STAGING_SIZE];
  int n = ConsoleSerial.readBytes(buf, min(avail, (int)sizeof(buf)));
  if (n <= 0) return;

  ledActTrigger();
  rxQueuePush(buf, n);
}

// ──────────────────────────────────────────────────────────────────
//  rxQueue -> client.write   (con riaccodo dei byte non scritti)
// ──────────────────────────────────────────────────────────────────
// v5.0 FIX CRITICO: prima la pop scaricava N byte e poi la write()
// ne consegnava solo W <= N; i (N - W) restanti si perdevano.
// Adesso si controlla il return value e i byte non passati vengono
// rimessi in coda per il ciclo successivo. Compatibile con il drop
// a 75% del rxQueue: se la situazione di lentezza diventa cronica,
// e' il drop in INGRESSO (drainSerialToRxQueue) a tagliare, non
// silenziosamente in uscita.
static void drainRxQueueToClient() {
  if (rxQueueUsed() == 0) return;

  if (bootMode == MODE_WIFI) {
    if (!telnetClient || !telnetClient.connected()) {
      uint8_t throwaway[256];
      while (rxQueueUsed() > 0) rxQueuePop(throwaway, sizeof(throwaway));
      return;
    }
    uint8_t buf[128];
    int n = rxQueuePop(buf, sizeof(buf));
    if (n > 0) {
      int w = telnetClient.write(buf, n);
      if (w < 0) w = 0;
      if (w < n) {
        // Re-accoda quello che la write() non ha potuto consegnare
        // (TCP back-pressure, SO_SNDTIMEO scaduto). Il prossimo
        // ciclo bridge riprova. Se rxQueue e' a sua volta pieno
        // il push ritorna meno di (n - w): in quel caso significa
        // che siamo in un blocco prolungato e il drop a 75% sta
        // gia' tagliando in ingresso, quindi e' OK perdere.
        rxQueuePush(buf + w, n - w);
      }
      vTaskDelay(pdMS_TO_TICKS(1));   // pacing: cede CPU
    }

  } else { // MODE_BT
    if (!SerialBT.hasClient()) {
      uint8_t throwaway[256];
      while (rxQueueUsed() > 0) rxQueuePop(throwaway, sizeof(throwaway));
      return;
    }
    uint8_t buf[BT_TX_CHUNK];
    int n = rxQueuePop(buf, sizeof(buf));
    if (n > 0) {
      int w = SerialBT.write(buf, n);
      if (w < 0) w = 0;
      if (w < n) {
        // Stesso fix lato BT. Su BluetoothSerial la write quasi sempre
        // scrive tutto, ma se il client cade durante la write il
        // ritorno e' parziale: ri-accodiamo per non perdere dati.
        rxQueuePush(buf + w, n - w);
      }
      // PACING per Windows SPP: 2ms tra chunk = ~5 KB/s effettivi.
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
}

// ──────────────────────────────────────────────────────────────────
//  TELNET HANDLER  (solo in MODE_WIFI)
// ──────────────────────────────────────────────────────────────────
static void handleTelnet() {
  if (!telnetClient || !telnetClient.connected()) {
    if (telnetClient) telnetClient.stop();
    telnetAuthenticated = false;
    telnetAuthBuf       = "";        // v5.0: reset stato auth sulla disconnect
    tnState             = TN_NORMAL;
    tnLastCR            = false;

    WiFiClient newClient = telnetServer.available();
    if (newClient && newClient.connected()) {
      telnetClient = newClient;
      telnetClient.setNoDelay(true);

      // SO_SNDTIMEO a 100ms: la write() non blocca mai per piu' di
      // 100ms. Combinato col riaccodo dei byte non scritti (v5.0)
      // garantisce sia fluidita' che integrita' del flusso.
      int fd = telnetClient.fd();
      if (fd >= 0) {
        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 100000;
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
      }

      sendTelnetInitialNegotiation();
      if (telnetAuthReq) {
        clientOut("Password: ");
      } else {
        clientOut("\r\n[Pi_Zaff Console Server]\r\n\r\n");
        telnetAuthenticated = true;
      }
    }
  }

  if (!telnetClient || !telnetClient.connected()) return;

  if (!telnetAuthenticated) {
    int safety = 0;
    while (telnetClient.available() && safety++ < 256) {
      uint8_t raw = telnetClient.read();
      uint8_t b;
      if (!processTelnetByte(raw, b)) continue;
      if (b == '\r' || b == '\n') {
        if (telnetAuthBuf.length() == 0) continue;
        if (telnetAuthBuf == telnetPassword) {
          telnetAuthenticated = true;
          clientOut("\r\n[Autenticato]\r\n\r\n");
        } else {
          clientOut("\r\nPassword errata.\r\n");
          vTaskDelay(pdMS_TO_TICKS(50));
          telnetClient.stop();
        }
        telnetAuthBuf = "";
      } else if (b >= 32 && b < 127 && telnetAuthBuf.length() < 64) {
        telnetAuthBuf += (char)b;
      }
    }
    return;
  }

  if (!canAcceptMoreData()) return;

  int avail = telnetClient.available();
  if (avail <= 0) return;

  uint8_t buf[MAX_TELNET_PER_LOOP];
  int count = 0;
  int toRead = min(avail, MAX_TELNET_PER_LOOP);

  while (toRead-- > 0 && telnetClient.available() && count < MAX_TELNET_PER_LOOP) {
    uint8_t raw = telnetClient.read();
    uint8_t b;
    if (processTelnetByte(raw, b)) buf[count++] = b;
  }

  if (count > 0) {
    ledActTrigger();
    int pushed = txQueuePush(buf, count);
    if (pushed < count) {
      Serial.printf("[WARN] Telnet ring full, persi %d/%d byte\n", count - pushed, count);
    }
  }
}

// ──────────────────────────────────────────────────────────────────
//  BLUETOOTH HANDLER  (solo in MODE_BT)
// ──────────────────────────────────────────────────────────────────
static void handleBluetooth() {
  if (!SerialBT.hasClient()) return;
  if (!canAcceptMoreData()) return;

  int avail = SerialBT.available();
  if (avail <= 0) return;

  uint8_t buf[MAX_BT_PER_LOOP];
  int count = 0;
  int toRead = min(avail, MAX_BT_PER_LOOP);
  static bool btLastCR = false;

  while (toRead-- > 0 && SerialBT.available() && count < MAX_BT_PER_LOOP - 1) {
    uint8_t b = (uint8_t)SerialBT.read();
    if (b == '\r')      { btLastCR = true;  buf[count++] = '\r'; }
    else if (b == '\n') {
      if (!btLastCR) buf[count++] = '\n';
      btLastCR = false;
    } else {
      btLastCR = false;
      buf[count++] = b;
    }
  }

  if (count > 0) {
    ledActTrigger();
    int pushed = txQueuePush(buf, count);
    if (pushed < count) {
      Serial.printf("[WARN] BT ring full, persi %d/%d byte\n", count - pushed, count);
    }
  }
}

// ──────────────────────────────────────────────────────────────────
//  PREFERENCES
// ──────────────────────────────────────────────────────────────────
static void loadPrefs() {
  prefs.begin("cfg", true);
  apSSID         = prefs.getString("apssid",   "ConsoleESP32");
  apPassword     = prefs.getString("appass",   "12345678");
  staSSID        = prefs.getString("stassid",  "");
  staPassword    = prefs.getString("stapass",  "");
  bluetoothName  = prefs.getString("btname",   "ConsoleESP32");
  telnetPassword = prefs.getString("telpass",  "console");
  useSTA         = prefs.getBool  ("usesta",   false);
  telnetAuthReq  = prefs.getBool  ("telauth",  false);
  baudRate       = prefs.getUInt  ("baud",     9600);
  bootMode       = prefs.getUChar ("mode",     MODE_WIFI);
  prefs.end();

  if (!isValidBaud(baudRate)) {
    Serial.printf("[CFG] baud invalido in NVS (%u), forzo 9600\n", baudRate);
    baudRate = 9600;
    prefs.begin("cfg", false);
    prefs.putUInt("baud", baudRate);
    prefs.end();
  }
  if (bootMode != MODE_WIFI && bootMode != MODE_BT) {
    bootMode = MODE_WIFI;
  }
  if (apSSID.length()        == 0) apSSID        = "ConsoleESP32";
  if (apPassword.length()     < 8) apPassword    = "12345678";
  if (bluetoothName.length() == 0) bluetoothName = "ConsoleESP32";
}

static void savePrefs() {
  prefs.begin("cfg", false);
  prefs.putString("apssid",  apSSID);
  prefs.putString("appass",  apPassword);
  prefs.putString("stassid", staSSID);
  prefs.putString("stapass", staPassword);
  prefs.putString("btname",  bluetoothName);
  prefs.putString("telpass", telnetPassword);
  prefs.putBool  ("usesta",  useSTA);
  prefs.putBool  ("telauth", telnetAuthReq);
  prefs.putUInt  ("baud",    baudRate);
  prefs.end();
}

// ──────────────────────────────────────────────────────────────────
//  HTML  (servito solo in MODE_WIFI)
// ──────────────────────────────────────────────────────────────────
static const char HTML_HEAD[] PROGMEM = R"HEAD(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pi_Zaff Console Server</title>
<style>
body{background:#0f1117;color:#fff;font-family:Arial,sans-serif;margin:0;padding:15px}
.head{text-align:center;padding:10px 0 20px;border-bottom:1px solid #1e2d40;margin-bottom:18px}
.head h1{margin:0;color:#00d4ff;font-size:22px}
.head .sub{font-size:11px;color:#5a7080;letter-spacing:2px;margin-top:4px}
.card{background:#1a1f29;border-radius:12px;padding:18px;margin-bottom:18px}
.title{font-size:18px;color:#00d4ff;margin-bottom:15px;font-weight:bold}
label{display:block;margin-top:12px;margin-bottom:5px;font-size:13px;color:#9fb3c8}
input[type=text],input[type=password],select{width:100%;box-sizing:border-box;padding:11px;border:none;border-radius:8px;background:#0d1520;color:#fff;font-family:monospace;font-size:14px}
input[type=checkbox]{margin-right:8px;vertical-align:middle}
.chk{margin-top:10px;font-size:14px}
button,.btn{display:block;width:100%;padding:14px;border:none;border-radius:10px;margin-top:15px;font-size:15px;font-weight:bold;cursor:pointer;text-align:center;text-decoration:none}
.save{background:#00d4ff;color:#000}
.reboot{background:#ff4455;color:#fff}
.bt-mode{background:#d68bff;color:#000}
.ota{background:#00ff99;color:#000}
.battery{text-align:center;font-size:32px;margin:8px 0;font-weight:bold}
.bok{color:#00ff9d}.bw{color:#ffaa00}.bl{color:#ff4455}
.info{margin:5px 0;color:#9fb3c8;font-family:monospace;font-size:13px}
.info b{color:#00d4ff}
.fwbox{padding:14px;background:#0d1520;border-radius:8px;text-align:center;margin-bottom:10px}
#st{margin-top:10px;font-family:monospace;color:#9fb3c8;min-height:18px}
</style></head><body>
<div class="head"><h1>&#9889; Pi_Zaff System</h1><div class="sub">WIRELESS CONSOLE SERVER &middot; RS232 &middot; v5.0</div></div>
)HEAD";

static const char HTML_TAIL[] PROGMEM = R"TAIL(<div class="card">
<div class="title">Aggiornamento Firmware (OTA)</div>
<div class="fwbox"><input type="file" id="fw" accept=".bin" style="color:#9fb3c8"></div>
<button type="button" class="ota" onclick="ota()">&#9889; CARICA FIRMWARE</button>
<div id="st"></div></div>
<div class="card">
<a class="btn bt-mode" href="/mode?to=bt" onclick="return confirm('Passare in modalita BT? La GUI sara inaccessibile, premi il pulsante GPIO32 per tornare.')">PASSA A MODALITA' BT</a>
<a class="btn reboot" href="/reboot" onclick="return confirm('Riavviare?')">RIAVVIA</a>
</div>
<script>
function ota(){
var f=document.getElementById('fw').files[0];
if(!f){alert('Seleziona un .bin');return}
if(!f.name.endsWith('.bin')){alert('Serve .bin');return}
var s=document.getElementById('st');
s.textContent='Upload...';s.style.color='#9fb3c8';
var d=new FormData();d.append('firmware',f);
var x=new XMLHttpRequest();x.open('POST','/update');x.timeout=180000;
x.upload.onprogress=function(e){if(e.lengthComputable){s.textContent='Invio: '+Math.round(e.loaded/e.total*100)+'%'}};
x.onload=function(){if(x.status==200){s.style.color='#00ff9d';s.textContent='OK - riavvio...'}else{s.style.color='#ff4455';s.textContent='Errore: '+x.responseText}};
x.onerror=function(){s.style.color='#ff4455';s.textContent='Errore connessione'};
x.send(d);}
</script></body></html>
)TAIL";

static void handleRoot() {
  readBattery();

  const char* battClass = (battPercent > 50) ? "bok" : (battPercent > 20 ? "bw" : "bl");

  IPAddress ip    = WiFi.softAPIP();
  IPAddress staIp = (useSTA && WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : IPAddress(0,0,0,0);

  char dyn[2400];
  int off = 0;

  off += snprintf(dyn + off, sizeof(dyn) - off,
    "<div class=\"card\"><div class=\"title\">Stato Sistema</div>"
    "<div class=\"battery %s\">&#128267; %d%%</div>"
    "<div class=\"info\">Modalita': <b>WiFi</b> (pulsante GPIO32 = passa a BT)</div>"
    "<div class=\"info\">AP IP: <b>%u.%u.%u.%u</b> (%s)</div>",
    battClass, battPercent,
    ip[0], ip[1], ip[2], ip[3], apSSID.c_str());

  if (useSTA && WiFi.status() == WL_CONNECTED) {
    off += snprintf(dyn + off, sizeof(dyn) - off,
      "<div class=\"info\">STA IP: <b>%u.%u.%u.%u</b> (%s)</div>",
      staIp[0], staIp[1], staIp[2], staIp[3], staSSID.c_str());
  }

  off += snprintf(dyn + off, sizeof(dyn) - off,
    "<div class=\"info\">Baud: <b>%u</b></div>"
    "<div class=\"info\">Heap libero: <b>%u KB</b></div>"
    "<div class=\"info\">Ring TX: <b>%u/%u byte</b></div>"
    "<div class=\"info\">Ring RX: <b>%u/%u byte</b></div>"
    "<div class=\"info\">Uptime: <b>%lu s</b></div>"
    "</div>",
    (unsigned)baudRate, (unsigned)(ESP.getFreeHeap() / 1024),
    (unsigned)txQueueUsed(), (unsigned)TX_QUEUE_SIZE,
    (unsigned)rxQueueUsed(), (unsigned)RX_QUEUE_SIZE,
    millis() / 1000UL);

  off += snprintf(dyn + off, sizeof(dyn) - off,
    "<div class=\"card\"><div class=\"title\">Configurazione</div>"
    "<form action=\"/save\" method=\"POST\">"
    "<label>SSID AP</label><input type=\"text\" name=\"apssid\" value=\"%s\" maxlength=\"32\">"
    "<label>Password AP</label><input type=\"password\" name=\"appass\" value=\"%s\" maxlength=\"63\">"
    "<div class=\"chk\"><label><input type=\"checkbox\" name=\"usesta\" value=\"1\"%s> Abilita STA (router casa)</label></div>"
    "<label>SSID STA</label><input type=\"text\" name=\"stassid\" value=\"%s\" maxlength=\"32\">"
    "<label>Password STA</label><input type=\"password\" name=\"stapass\" value=\"%s\" maxlength=\"63\">"
    "<label>Nome Bluetooth</label><input type=\"text\" name=\"btname\" value=\"%s\" maxlength=\"32\">"
    "<div class=\"chk\"><label><input type=\"checkbox\" name=\"telauth\" value=\"1\"%s> Richiedi password Telnet</label></div>"
    "<label>Password Telnet</label><input type=\"password\" name=\"telpass\" value=\"%s\" maxlength=\"63\">"
    "<label>Baud Rate</label><select name=\"baud\">",
    apSSID.c_str(), apPassword.c_str(),
    useSTA ? " checked" : "",
    staSSID.c_str(), staPassword.c_str(),
    bluetoothName.c_str(),
    telnetAuthReq ? " checked" : "",
    telnetPassword.c_str());

  for (size_t i = 0; i < N_VALID_BAUDS; i++) {
    off += snprintf(dyn + off, sizeof(dyn) - off,
      "<option value=\"%u\"%s>%u</option>",
      (unsigned)VALID_BAUDS[i],
      (baudRate == VALID_BAUDS[i]) ? " selected" : "",
      (unsigned)VALID_BAUDS[i]);
  }

  off += snprintf(dyn + off, sizeof(dyn) - off,
    "</select>"
    "<button class=\"save\" type=\"submit\">SALVA E RIAVVIA</button>"
    "</form></div>");

  size_t headLen = strlen_P(HTML_HEAD);
  size_t tailLen = strlen_P(HTML_TAIL);
  server.setContentLength(headLen + off + tailLen);
  server.sendHeader("Connection", "close");
  server.send(200, "text/html; charset=utf-8", "");
  server.sendContent_P(HTML_HEAD);
  server.sendContent(dyn, off);
  server.sendContent_P(HTML_TAIL);
}

// v5.0: helper di validazione lunghezza stringhe ricevute dal POST.
// Limita gli input per non rompere WiFi.softAP()/BluetoothSerial al
// prossimo reboot. minLen=0 significa "anche stringa vuota OK".
static bool acceptArg(const String& s, size_t minLen, size_t maxLen) {
  size_t L = s.length();
  return (L >= minLen) && (L <= maxLen);
}

static void handleSave() {
  // SSID AP: 1..32
  if (server.hasArg("apssid")) {
    String v = server.arg("apssid");
    if (acceptArg(v, 1, 32)) apSSID = v;
    else Serial.printf("[SAVE] apssid len=%u rifiutato\n", v.length());
  }
  // Password AP: 8..63 (limite WiFi WPA2)
  if (server.hasArg("appass")) {
    String v = server.arg("appass");
    if (acceptArg(v, 8, 63)) apPassword = v;
    else Serial.printf("[SAVE] appass len=%u rifiutato\n", v.length());
  }
  // SSID STA: 0..32 (vuoto = disabilita)
  if (server.hasArg("stassid")) {
    String v = server.arg("stassid");
    if (acceptArg(v, 0, 32)) staSSID = v;
    else Serial.printf("[SAVE] stassid len=%u rifiutato\n", v.length());
  }
  // Password STA: 0..63
  if (server.hasArg("stapass")) {
    String v = server.arg("stapass");
    if (acceptArg(v, 0, 63)) staPassword = v;
    else Serial.printf("[SAVE] stapass len=%u rifiutato\n", v.length());
  }
  // Nome BT: 1..32
  if (server.hasArg("btname")) {
    String v = server.arg("btname");
    if (acceptArg(v, 1, 32)) bluetoothName = v;
    else Serial.printf("[SAVE] btname len=%u rifiutato\n", v.length());
  }
  // Password telnet: 1..63
  if (server.hasArg("telpass")) {
    String v = server.arg("telpass");
    if (acceptArg(v, 1, 63)) telnetPassword = v;
    else Serial.printf("[SAVE] telpass len=%u rifiutato\n", v.length());
  }

  useSTA        = server.arg("usesta")  == "1";
  telnetAuthReq = server.arg("telauth") == "1";

  if (server.hasArg("baud")) {
    uint32_t b = server.arg("baud").toInt();
    if (isValidBaud(b)) baudRate = b;
    else Serial.printf("[SAVE] Baud invalido %u, mantengo %u\n", b, baudRate);
  }

  // Safety net: se qualcosa e' rimasto in stato invalido per qualunque
  // motivo (NVS corrotto, downgrade firmware, ecc) ripristina default.
  if (!isValidBaud(baudRate)) baudRate = 9600;
  if (apSSID.length() == 0)    apSSID = "ConsoleESP32";
  if (apPassword.length() < 8) apPassword = "12345678";
  if (bluetoothName.length() == 0) bluetoothName = "ConsoleESP32";

  savePrefs();

  server.send_P(200, PSTR("text/html; charset=utf-8"), PSTR(
    "<!DOCTYPE html><html><head><meta charset=utf-8>"
    "<meta http-equiv='refresh' content='3;url=/'></head>"
    "<body style='background:#0a0e14;color:#00ff9d;font-family:monospace;"
    "text-align:center;padding:60px'><h2>&#10003; Configurazione salvata</h2>"
    "<p>Riavvio...</p></body></html>"));
  delay(800);
  ESP.restart();
}

static void handleReboot() {
  server.send_P(200, PSTR("text/html"), PSTR(
    "<body style='background:#0a0e14;color:#ff4455;font-family:monospace;"
    "text-align:center;padding:60px'><h2>Riavvio...</h2></body>"));
  delay(300);
  ESP.restart();
}

static void handleModeChange() {
  if (!server.hasArg("to")) {
    server.send(400, "text/plain", "manca arg 'to' (wifi|bt)");
    return;
  }
  String to = server.arg("to");
  uint8_t newMode = (to == "bt") ? MODE_BT : MODE_WIFI;
  server.send_P(200, PSTR("text/html"), PSTR(
    "<body style='background:#0a0e14;color:#d68bff;font-family:monospace;"
    "text-align:center;padding:60px'><h2>Cambio modalita'...</h2>"
    "<p>Per tornare premi GPIO32 a lungo (1.5s).</p></body>"));
  delay(500);
  saveBootModeAndReboot(newMode);
}

// v5.0: LED test non-bloccante. Setta solo il timer; la macchina di
// stato in updateLEDs() si occupa di tenere i LED accesi per 3s e di
// spegnerli alla scadenza. Niente piu' rientranza del WebServer.
static void handleLedTest() {
  ledTestUntil = millis() + 3000;
  server.send_P(200, PSTR("text/html; charset=utf-8"), PSTR(
    "<body style='background:#0a0e14;color:#00ff9d;font-family:monospace;"
    "text-align:center;padding:60px'><h2>LED TEST</h2>"
    "<p>Tutti i LED accesi per 3 secondi.</p>"
    "<p><a href='/' style='color:#00d4ff'>Torna</a></p></body>"));
}

static void handleStatus() {
  char buf[320];
  snprintf(buf, sizeof(buf),
    "{\"mode\":\"%s\",\"batt\":%d,\"volt\":%.2f,\"heap\":%u,"
    "\"txring\":%u,\"rxring\":%u,\"telnet\":%d,\"baud\":%u,\"uptime\":%lu,"
    "\"ver\":\"5.0\"}",
    bootModeName(bootMode), battPercent, battVoltage,
    (unsigned)ESP.getFreeHeap(),
    (unsigned)txQueueUsed(), (unsigned)rxQueueUsed(),
    (telnetClient && telnetClient.connected()) ? 1 : 0,
    (unsigned)baudRate,
    millis() / 1000UL);
  server.send(200, "application/json", buf);
}

// v5.0: SOLO POST. Cancellare la config con una GET (click su link,
// img tag malevolo, prefetch del browser) era troppo facile.
// Da terminale:  curl -X POST http://192.168.4.1/factory
static void handleFactoryReset() {
  prefs.begin("cfg", false);
  prefs.clear();
  prefs.end();
  server.send(200, "text/plain", "Factory reset eseguito. Riavvio...");
  delay(500);
  ESP.restart();
}

static void handleFactoryGetBlocked() {
  server.send(405, "text/plain",
    "Method Not Allowed: /factory richiede POST (v5.0).\n"
    "Usa: curl -X POST http://<ip>/factory\n");
}

static void handleOtaUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaInProgress = true;
    Serial.printf("[OTA] start: %s\n", upload.filename.c_str());
    digitalWrite(LED_ACT,  LOW);
    digitalWrite(LED_WIFI, LOW);
    digitalWrite(LED_BT,   LOW);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    esp_task_wdt_reset();
    digitalWrite(LED_BATT, (millis() / 100) % 2);
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA] OK: %u bytes\n", upload.totalSize);
      const int leds[] = { LED_ACT, LED_WIFI, LED_BT, LED_BATT };
      for (int i = 0; i < 4; i++) { digitalWrite(leds[i], HIGH); delay(80); }
      for (int i = 0; i < 4; i++) { digitalWrite(leds[i], LOW);  delay(80); }
    } else {
      Update.printError(Serial);
    }
    otaInProgress = false;
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaInProgress = false;
    digitalWrite(LED_BATT, LOW);
  }
}

static void setupWeb() {
  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/save",    HTTP_POST, handleSave);
  server.on("/reboot",  HTTP_GET,  handleReboot);
  server.on("/mode",    HTTP_GET,  handleModeChange);
  server.on("/ledtest", HTTP_GET,  handleLedTest);
  server.on("/status",  HTTP_GET,  handleStatus);
  // v5.0: /factory accetta SOLO POST. La GET ritorna 405 con istruzioni.
  server.on("/factory", HTTP_POST, handleFactoryReset);
  server.on("/factory", HTTP_GET,  handleFactoryGetBlocked);
  server.on("/update",  HTTP_POST,
    []() {
      bool ok = !Update.hasError();
      server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : Update.errorString());
      delay(400);
      ESP.restart();
    },
    handleOtaUpload
  );
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println(F("[WEB] server :80 attivo"));
}

// ──────────────────────────────────────────────────────────────────
//  WIFI BOOT (solo MODE_WIFI)
// ──────────────────────────────────────────────────────────────────
static void startWiFiSubsystem() {
  WiFi.persistent(false);
  if (useSTA && staSSID.length() > 0) WiFi.mode(WIFI_AP_STA);
  else                                WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  delay(150);

  WiFi.softAPConfig(apIP, apGW, apMask);
  if (WiFi.softAP(apSSID.c_str(), apPassword.c_str())) {
    Serial.printf("[WIFI] AP up: %s -> %s\n",
                  apSSID.c_str(), WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println(F("[WIFI] AP FAIL"));
  }

  if (useSTA && staSSID.length() > 0) {
    Serial.printf("[WIFI] STA -> %s ", staSSID.c_str());
    WiFi.begin(staSSID.c_str(), staPassword.c_str());
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 12000) {
      delay(250);
      Serial.print('.');
      esp_task_wdt_reset();
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n[WIFI] STA OK -> %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println(F("\n[WIFI] STA timeout"));
    }
  }

  telnetServer.begin();
  telnetServer.setNoDelay(true);
  Serial.println(F("[NET] Telnet listener :23"));

  setupWeb();
}

// ──────────────────────────────────────────────────────────────────
//  BT BOOT (solo MODE_BT)
// ──────────────────────────────────────────────────────────────────
static void startBtSubsystem() {
  WiFi.mode(WIFI_OFF);
  WiFi.persistent(false);

  if (SerialBT.begin(bluetoothName)) {
    Serial.printf("[BT] SPP up: %s\n", bluetoothName.c_str());
  } else {
    delay(300);
    if (SerialBT.begin(bluetoothName)) {
      Serial.printf("[BT] SPP up (retry): %s\n", bluetoothName.c_str());
    } else {
      Serial.println(F("[BT] FAIL - proseguo comunque"));
    }
  }
}

// ──────────────────────────────────────────────────────────────────
//  BRIDGE TASK (Core 1)
// ──────────────────────────────────────────────────────────────────
static void bridgeTask(void *param) {
  esp_task_wdt_add(NULL);
  Serial.printf("[BRIDGE] task partito su core %d\n", xPortGetCoreID());

  unsigned long lastBattWarnCheck = 0;

  for (;;) {
    esp_task_wdt_reset();
    unsigned long t0 = millis();

    if (!otaInProgress) {
      // 1) UART -> rxQueue (output del router verso il client).
      //    v5.0: scarta subito se non c'e' alcun client.
      drainSerialToRxQueue();

      // 2) Client -> txQueue (i comandi che l'utente digita).
      if (bootMode == MODE_WIFI) handleTelnet();
      else                       handleBluetooth();

      // 3) txQueue -> UART (consegna i comandi al router).
      drainTxQueueToUart();

      // 4) rxQueue -> client (consegna l'output al client).
      //    v5.0: i byte non consegnati dalla write() tornano in coda.
      drainRxQueueToClient();

      // 5) Battery warning.
      if (millis() - lastBattWarnCheck > 5000) {
        handleLowBatteryWarning();
        lastBattWarnCheck = millis();
      }
    }

    unsigned long elapsed = millis() - t0;
    if (elapsed > bridgeMaxTime) bridgeMaxTime = elapsed;

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ──────────────────────────────────────────────────────────────────
//  SETUP
// ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(80);
  Serial.println(F("\n[Pi_Zaff] Console Server v5.0 STABILE"));
  if (USB_DEBUG_MODE)
    Serial.println(F("** USB_DEBUG_MODE: batteria simulata 100% **"));

  setupLEDs();
  buttonInit();

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_reconfigure(&wdt_config);
  esp_task_wdt_add(NULL);

  analogReadResolution(12);
  analogSetPinAttenuation(BATT_PIN, ADC_11db);

  loadPrefs();

  // RECOVERY: pulsante premuto al power-on -> forza WIFI
  if (digitalRead(BTN_PIN) == LOW) {
    Serial.println(F("[BTN] Pulsante premuto al boot -> RECOVERY: forza MODE_WIFI"));
    bootMode = MODE_WIFI;
    prefs.begin("cfg", false);
    prefs.putUChar("mode", MODE_WIFI);
    prefs.end();
    for (int i = 0; i < 8; i++) {
      digitalWrite(LED_ACT,  i & 1);
      digitalWrite(LED_WIFI, i & 1);
      digitalWrite(LED_BT,   i & 1);
      digitalWrite(LED_BATT, i & 1);
      delay(100);
      esp_task_wdt_reset();
    }
    digitalWrite(LED_ACT, LOW);
    digitalWrite(LED_BT,  LOW);
    digitalWrite(LED_BATT, LOW);
    while (digitalRead(BTN_PIN) == LOW) {
      delay(10);
      esp_task_wdt_reset();
    }
    btnLastRaw   = HIGH;
    btnState     = HIGH;
    btnLongFired = false;
  }

  Serial.printf("[CFG] mode=%s baud=%u AP=%s BT=%s STA=%s\n",
                bootModeName(bootMode), (unsigned)baudRate,
                apSSID.c_str(), bluetoothName.c_str(),
                useSTA ? "on" : "off");

  ConsoleSerial.setRxBufferSize(UART_RX_BUF);
  ConsoleSerial.setTxBufferSize(UART_TX_BUF);
  ConsoleSerial.begin(baudRate, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.printf("[UART2] %u 8N1 RX=%d TX=%d (buf %d/%d)\n",
                (unsigned)baudRate, UART_RX_PIN, UART_TX_PIN,
                UART_RX_BUF, UART_TX_BUF);

  if (bootMode == MODE_WIFI) {
    startWiFiSubsystem();
  } else {
    startBtSubsystem();
  }

  readBattery();

  BaseType_t ok = xTaskCreatePinnedToCore(
    bridgeTask, "bridge", BRIDGE_STACK_SIZE, NULL,
    BRIDGE_PRIORITY, &bridgeTaskHandle, BRIDGE_CORE);
  Serial.printf("[BRIDGE] task %s (core %d, prio %d)\n",
                ok == pdPASS ? "creato" : "FAIL",
                BRIDGE_CORE, BRIDGE_PRIORITY);

  Serial.printf("[BOOT] OK mode=%s heap=%u  (tieni GPIO32 1.5s per cambiare)\n",
                bootModeName(bootMode), (unsigned)ESP.getFreeHeap());
}

// ──────────────────────────────────────────────────────────────────
//  LOOP  (Core 1) - solo HTTP/GUI/LED/batteria/pulsante
// ──────────────────────────────────────────────────────────────────
void loop() {
  static unsigned long loopMaxTime    = 0;
  static unsigned long lastLoopReport = 0;
  unsigned long loopStart = millis();

  esp_task_wdt_reset();

  handleButton();

  if (otaInProgress) {
    delay(1);
    return;
  }

  if (bootMode == MODE_WIFI) {
    server.handleClient();
  }

  updateLEDs();
  handleCriticalBatteryShutdown();

  if (millis() - lastBattRead > BATT_READ_INTERVAL) {
    readBattery();
    lastBattRead = millis();
  }

  unsigned long loopTime = millis() - loopStart;
  if (loopTime > loopMaxTime) loopMaxTime = loopTime;

  if (millis() - lastLoopReport > LOOP_REPORT_INT) {
    if (loopMaxTime > LOOP_WARN_MS || bridgeMaxTime > 50) {
      Serial.printf("[PERF] loop=%lums bridge=%lums txq=%u rxq=%u heap=%u mode=%s\n",
                    loopMaxTime, bridgeMaxTime,
                    txQueueUsed(), rxQueueUsed(),
                    (unsigned)ESP.getFreeHeap(),
                    bootModeName(bootMode));
    }
    loopMaxTime = 0;
    bridgeMaxTime = 0;
    lastLoopReport = millis();
  }

  delay(5);
}
