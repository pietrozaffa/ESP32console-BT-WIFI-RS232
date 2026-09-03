/*
╔══════════════════════════════════════════════════════════════════╗
║         Pi_Zaff System  -  Wireless Console Server               ║
║                  Firmware v6.0                                   ║
╠══════════════════════════════════════════════════════════════════╣
║  CHANGELOG v6.0 (su v5.0 STABILE, stesso hardware/pin/cavo)      ║
║  ----------------------------------                              ║
║  [BUG-FIX] Client zombie: se telnet/BT risultano ancora           ║
║    "connessi" (stato locale) ma la write() non consegna piu'      ║
║    NEMMENO UN byte per ZOMBIE_WRITE_STREAK cicli di fila (link    ║
║    caduto senza FIN/RST, tipico quando il WiFi/BT sparisce        ║
║    durante un output pesante dal router), la connessione viene    ║
║    chiusa a forza. Prima si restava bloccati sul peer morto in    ║
║    attesa che TCP/BT se ne accorgesse da soli (anche a lungo):    ║
║    era percepito come console "impallata".                        ║
║                                                                  ║
║  [BUG-FIX] bridgeTask non viene piu' ucciso con vTaskDelete()     ║
║    dall'esterno (cambio modalita' via GPIO32, spegnimento per     ║
║    batteria critica): se il task veniva colto a meta' di una      ║
║    write() di rete, poteva restare un lock interno lwIP orfano    ║
║    e la successiva WiFi.disconnect() restava bloccata per         ║
║    sempre. Ora si segnala lo stop e si aspetta l'uscita pulita    ║
║    del task a inizio ciclo (mai dentro una chiamata di rete).     ║
║                                                                  ║
║  [NUOVO] Terminale live nella GUI web (/term): WebSocket sulla    ║
║    porta 81, con storico recente (scrollback in RAM, 8KB) e       ║
║    invio comandi a riga intera. Gira SOLO da loop(), mai da       ║
║    bridgeTask (vedi commento su WEBSOCKETS_TCP_TIMEOUT in testa   ║
║    al file) per non rischiare di rintrodurre blocchi nel ponte.   ║
║                                                                  ║
║  [NUOVO] Storico connessioni telnet/BT in home page: ultime 20,   ║
║    le piu' vecchie si sovrascrivono da sole (memoria fissa).      ║
║                                                                  ║
║  [NUOVO] mDNS (pizaff.local), RSSI del client AP e del link STA,  ║
║    motivo dell'ultimo reset: visibili in home page e in /status.  ║
║                                                                  ║
║  [SAFETY] /reboot e /mode ora richiedono POST, come /factory.     ║
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
║  MODE_WIFI: AP + STA + Telnet + Web GUI + OTA + mDNS +            ║
║             terminale WebSocket (/term, porta 81).                ║
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
#include <esp_system.h>       // per esp_reset_reason()
#include <esp_wifi.h>         // per esp_wifi_ap_get_sta_list() (RSSI client AP)
#include <ESPmDNS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>     // per setsockopt SO_SNDTIMEO

// v6.0: la write() interna di questa libreria (WebSockets.cpp) ha un
// timeout di default di 5000ms prima di rinunciare su un client
// bloccato - troppo lungo per essere accettabile ovunque venga
// chiamata. Va ridefinito PRIMA dell'include (la libreria lo accetta
// via #ifndef). Il terminale WS viene comunque pilotato solo da
// loop(), mai da bridgeTask: vedi handleWsTerminal()/setupWebSocket().
#define WEBSOCKETS_TCP_TIMEOUT 100
#include <WebSocketsServer.h>

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

// v6.0: se la write() verso il client non consegna NEMMENO UN byte per
// N cicli consecutivi, il client e' "connesso" solo secondo lo stato
// TCP/SPP locale ma di fatto morto (link caduto senza FIN/RST, tipico
// col WiFi che sparisce durante output pesante). Dopo la soglia lo
// disconnettiamo a forza cosi' un nuovo client puo' subentrare subito,
// invece di restare bloccati a oltranza sul peer zombie.
#define ZOMBIE_WRITE_STREAK  20

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

// v6.0: server WebSocket per il terminale live (/term). Porta separata
// da 80/23. Pilotato SOLO da loop()/task principale, mai da
// bridgeTask - vedi il commento su WEBSOCKETS_TCP_TIMEOUT in testa al
// file per il perche'.
WebSocketsServer wsServer(81);

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

// v6.0: motivo dell'ultimo reset, letto una volta in setup(). Se il
// dispositivo si riavvia da solo in futuro, questo dice finalmente
// perche' (panic WDT, brownout, ecc.) invece di doverlo indovinare.
static const char* lastResetReason = "?";

static const char* resetReasonToStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_EXT:       return "reset esterno";
    case ESP_RST_SW:        return "riavvio software";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "watchdog interrupt";
    case ESP_RST_TASK_WDT:  return "watchdog task";
    case ESP_RST_WDT:       return "watchdog";
    case ESP_RST_DEEPSLEEP: return "wakeup da deep sleep";
    case ESP_RST_BROWNOUT:  return "brownout (alimentazione)";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "sconosciuto";
  }
}

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

// v6.0: contatori di write consecutive andate a vuoto (0 byte
// consegnati) verso il client, usati per rilevare connessioni zombie.
// A file-scope cosi' si possono azzerare al momento della connect.
static uint16_t telnetZeroWriteStreak = 0;
static uint16_t btZeroWriteStreak     = 0;

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

// v6.0: stop cooperativo del bridgeTask. Prima si chiamava vTaskDelete()
// da un'altra task: se bridgeTask veniva ucciso mentre era DENTRO una
// write() di lwIP (telnetClient.write) restava a rischio un lock interno
// di lwIP mai rilasciato, con conseguente hang permanente della prima
// chiamata WiFi successiva (es. WiFi.disconnect() nello shutdown per
// batteria critica). Ora si segnala lo stop e si aspetta che il task
// esca da solo a inizio ciclo (fuori da qualunque chiamata di rete),
// per poi autocancellarsi.
static volatile bool  bridgeStopRequested = false;

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
//  SCROLLBACK (v6.0): ultimi byte grezzi di output del router, per la
//  pagina /term. Buffer circolare a sola sovrascrittura (non e' una
//  coda da consumare come txQueue/rxQueue): un produttore (bridgeTask,
//  stesso punto dove drainSerialToRxQueue() gia' fa rxQueuePush() dei
//  byte UART grezzi), due lettori indipendenti senza stato condiviso
//  da sincronizzare:
//    1) snapshot on-demand quando si apre /term (task principale)
//    2) invio del delta live via WebSocket da loop() (vedi wsLastSentHead)
//  scrollTotal e' il conteggio totale di byte MAI scritti (monotono):
//  serve per sapere, finche' il buffer non si e' ancora riempito una
//  prima volta, da dove iniziano i dati validi.
//  Nota: una lettura puo' in rarissimi casi sovrapporsi a una scrittura
//  di bridgeTask (nessun lock, stesso spirito "best effort" gia' usato
//  altrove nel file per letture cross-task di scalari tipo battPercent).
//  Nel peggiore dei casi un carattere isolato "sporco" in una pagina di
//  solo testo, mai un crash: compromesso deliberato.
// ──────────────────────────────────────────────────────────────────
#define SCROLLBACK_SIZE 8192
static uint8_t           scrollback[SCROLLBACK_SIZE];
static volatile uint32_t scrollTotal = 0;   // byte totali scritti da sempre

static void scrollbackPush(const uint8_t* data, int len) {
  uint32_t t = scrollTotal;
  for (int i = 0; i < len; i++) {
    scrollback[t % SCROLLBACK_SIZE] = data[i];
    t++;
  }
  scrollTotal = t;
}

// ──────────────────────────────────────────────────────────────────
//  WS INPUT STAGING (v6.0): il terminale WebSocket gira interamente
//  da loop()/task principale (mai da bridgeTask, vedi il commento sul
//  define WEBSOCKETS_TCP_TIMEOUT in testa al file). Per non rendere
//  txQueue multi-produttore (oggi ci scrive solo bridgeTask, via
//  handleTelnet()/handleBluetooth()), i comandi digitati nel terminale
//  web vengono prima appoggiati qui dal callback WS (loop()), poi
//  bridgeTask li preleva e li spinge in txQueue nel suo giro normale
//  (handleWsInput()) - stesso identico idioma single-producer/
//  single-consumer di txQueue/rxQueue, solo un anello in piu'.
// ──────────────────────────────────────────────────────────────────
#define WS_IN_QUEUE_SIZE 256
static uint8_t           wsInQueue[WS_IN_QUEUE_SIZE];
static volatile uint16_t wsInHead = 0;
static volatile uint16_t wsInTail = 0;

static void wsInQueuePush(const uint8_t* data, int len) {
  uint16_t h = wsInHead;
  for (int i = 0; i < len; i++) {
    uint16_t next = (h + 1) % WS_IN_QUEUE_SIZE;
    if (next == wsInTail) break;   // pieno: comando troppo lungo/non letto, scarta l'eccesso
    wsInQueue[h] = data[i];
    h = next;
  }
  wsInHead = h;
}

static int wsInQueuePop(uint8_t* out, int maxLen) {
  int n = 0;
  uint16_t t = wsInTail;
  while (n < maxLen && t != wsInHead) {
    out[n++] = wsInQueue[t];
    t = (t + 1) % WS_IN_QUEUE_SIZE;
  }
  wsInTail = t;
  return n;
}

// ──────────────────────────────────────────────────────────────────
//  STORICO CONNESSIONI (v6.0): ultime CONN_HISTORY_SIZE connessioni
//  telnet/BT, ring a scrittura circolare - le voci piu' vecchie
//  vengono sovrascritte da sole appena il buffer si riempie, cosi'
//  la memoria occupata resta sempre fissa (~800 byte) e non cresce
//  mai, indipendentemente da quante connessioni avvengono nel tempo.
//  Produttore unico: bridgeTask (edge di connect/disconnect gia'
//  rilevati in handleTelnet()/handleBluetooth()). Consumatore:
//  handleRoot() (task principale), sola lettura per la visualizzazione.
// ──────────────────────────────────────────────────────────────────
#define CONN_HISTORY_SIZE 20
#define CONN_CH_TELNET 0
#define CONN_CH_BT     1
struct ConnHistoryEntry {
  uint8_t       channel;           // 0 = Telnet, 1 = Bluetooth
  unsigned long connectedAtMs;     // millis() alla connessione
  unsigned long disconnectedAtMs;  // 0 finche' e' ancora attiva
  IPAddress     remoteIp;          // solo telnet; BT -> 0.0.0.0
};
static ConnHistoryEntry connHistory[CONN_HISTORY_SIZE];
static uint8_t connHistoryNext  = 0;   // prossimo slot da scrivere (si avvolge)
static uint8_t connHistoryCount = 0;   // voci valide finora (satura a CONN_HISTORY_SIZE)

static void connHistoryAdd(uint8_t channel, IPAddress ip) {
  ConnHistoryEntry &e = connHistory[connHistoryNext];
  e.channel          = channel;
  e.connectedAtMs     = millis();
  e.disconnectedAtMs = 0;
  e.remoteIp          = ip;
  connHistoryNext = (connHistoryNext + 1) % CONN_HISTORY_SIZE;
  if (connHistoryCount < CONN_HISTORY_SIZE) connHistoryCount++;
}

// Segna come chiusa la voce aperta piu' recente per il canale dato
// (scansione lineare all'indietro su al massimo CONN_HISTORY_SIZE
// voci, costo trascurabile). Se non trova nulla (storico gia'
// sovrascritto) non fa nulla: non e' un errore, e' atteso.
static void connHistoryClose(uint8_t channel) {
  for (uint8_t i = 0; i < connHistoryCount; i++) {
    uint8_t idx = (connHistoryNext - 1 - i + CONN_HISTORY_SIZE * 2) % CONN_HISTORY_SIZE;
    ConnHistoryEntry &e = connHistory[idx];
    if (e.channel == channel && e.disconnectedAtMs == 0) {
      e.disconnectedAtMs = millis();
      return;
    }
  }
}

// ──────────────────────────────────────────────────────────────────
//  FORWARD DECL
// ──────────────────────────────────────────────────────────────────
void ledActTrigger();
static void saveBootModeAndReboot(uint8_t newMode);
static void stopBridgeTaskSafely();
static void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
static void handleWsInput();

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

  stopBridgeTaskSafely();

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

  stopBridgeTaskSafely();

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
  // v6.0: lettura UART unificata in un solo punto (prima ogni ramo
  // sotto leggeva per conto suo in un buffer "junk" separato solo per
  // scartare). Cosi' lo scrollback (sotto) cattura SEMPRE l'output
  // grezzo del router, indipendentemente da chi e' collegato in quel
  // momento - e' un buffer a parte pensato apposta per "cosa e'
  // successo mentre non ero collegato" (pagina /term), quindi non deve
  // dipendere dalla stessa logica di scarto di rxQueue qui sotto.
  static uint8_t buf[RX_STAGING_SIZE];
  int n = ConsoleSerial.readBytes(buf, min(avail, (int)sizeof(buf)));
  if (n <= 0) return;

  scrollbackPush(buf, n);

  // v5.0: se non c'e' alcun client connesso (per la modalita' corrente)
  // i dati UART vengono scartati subito (restano comunque in
  // scrollback, vedi sopra). Cosi':
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
  if (!hasClient) return;

  // DROP CONTROL UNIVERSALE (BT e WIFI): se la coda e' oltre il 75%
  // piena significa che il client non sta riuscendo a smaltire
  // l'output del router. Invece di accumulare backlog di secondi,
  // SCARTIAMO i nuovi byte verso rxQueue per liberare il buffer
  // hardware. Console realtime > integrita' del flusso.
  // Su WIFI succede con output massicci (show run, debug) e WiFi
  // instabile; su BT succede su Windows SPP che e' lento.
  // *** QUESTO E' UN FEATURE DELIBERATO PER LA FLUIDITA' ***
  if (rxQueueUsed() > RX_QUEUE_DROP_HIGH) return;

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
      telnetZeroWriteStreak = 0;
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
      // v6.0: se per ZOMBIE_WRITE_STREAK cicli di fila non passa
      // NESSUN byte, il client e' morto (WiFi caduto senza FIN/RST):
      // forziamo la disconnect cosi' non restiamo bloccati a
      // oltranza su un peer che TCP considera ancora "connesso".
      if (w == 0) {
        if (++telnetZeroWriteStreak >= ZOMBIE_WRITE_STREAK) {
          telnetClient.stop();
          telnetZeroWriteStreak = 0;
        }
      } else {
        telnetZeroWriteStreak = 0;
      }
      vTaskDelay(pdMS_TO_TICKS(1));   // pacing: cede CPU
    }

  } else { // MODE_BT
    if (!SerialBT.hasClient()) {
      btZeroWriteStreak = 0;
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
      // v6.0: stesso rilevatore zombie del ramo telnet. Un link SPP
      // morto (fuori portata, stack Windows impallato) puo' lasciare
      // hasClient()==true mentre la write() non consegna piu' nulla:
      // dopo la soglia forziamo la disconnect.
      if (w == 0) {
        if (++btZeroWriteStreak >= ZOMBIE_WRITE_STREAK) {
          SerialBT.disconnect();
          btZeroWriteStreak = 0;
        }
      } else {
        btZeroWriteStreak = 0;
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
    tnLastCR             = false;
    telnetZeroWriteStreak = 0;       // v6.0: reset rilevatore zombie sulla disconnect
    connHistoryClose(CONN_CH_TELNET); // v6.0: se c'era una sessione aperta, la chiude nello storico

    WiFiClient newClient = telnetServer.accept();  // v6.0: available() e' deprecato, stessa identica funzione
    if (newClient && newClient.connected()) {
      telnetClient = newClient;
      telnetClient.setNoDelay(true);
      connHistoryAdd(CONN_CH_TELNET, telnetClient.remoteIP());  // v6.0: storico connessioni

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
  // v6.0: storico connessioni. BluetoothSerial non offre un evento di
  // connect/disconnect comodo da usare qui senza introdurre un secondo
  // produttore concorrente (il callback SPP gira dal task interno
  // dello stack Bluedroid) - si rileva il fronte per polling su
  // hasClient(), stesso identico pattern gia' usato per il debounce
  // del pulsante GPIO32.
  static bool btWasConnected = false;
  bool btNowConnected = SerialBT.hasClient();
  if (btNowConnected != btWasConnected) {
    if (btNowConnected) connHistoryAdd(CONN_CH_BT, IPAddress(0, 0, 0, 0));
    else                connHistoryClose(CONN_CH_BT);
    btWasConnected = btNowConnected;
  }

  if (!btNowConnected) return;
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

// v6.0: preleva i comandi digitati nel terminale WebSocket (appoggiati
// in wsInQueue dal callback onWsEvent(), che gira in loop() - vedi
// commento su WEBSOCKETS_TCP_TIMEOUT in testa al file) e li spinge in
// txQueue esattamente come handleTelnet()/handleBluetooth() qui sopra,
// cosi' txQueue resta scritta solo da bridgeTask (unico produttore).
static void handleWsInput() {
  uint8_t buf[MAX_TELNET_PER_LOOP];
  int n = wsInQueuePop(buf, sizeof(buf));
  if (n <= 0) return;
  ledActTrigger();
  int pushed = txQueuePush(buf, n);
  if (pushed < n) {
    Serial.printf("[WARN] WS ring full, persi %d/%d byte\n", n - pushed, n);
  }
}

// v6.0: callback del terminale WebSocket. Gira SEMPRE dentro
// wsServer.loop(), chiamato da loop() (task principale) - MAI da
// bridgeTask, vedi il commento su WEBSOCKETS_TCP_TIMEOUT in testa al
// file. Il testo ricevuto viene solo appoggiato in wsInQueue: e'
// bridgeTask, nel suo giro normale (handleWsInput() sopra), a
// spingerlo in txQueue, per non rendere txQueue multi-produttore.
static void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT && length > 0) {
    wsInQueuePush(payload, (int)length);
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
    Serial.printf("[CFG] baud invalido in NVS (%u), forzo 9600\n", (unsigned)baudRate);
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

  // v6.0: safety net anche sul limite SUPERIORE. handleSave() rifiuta
  // gia' input troppo lunghi, ma se la NVS contenesse un valore fuori
  // dai limiti (config scritta da un firmware precedente, o NVS
  // corrotta) le stesse stringhe finiscono anche nell'HTML dinamico di
  // handleRoot(): tagliamo qui alla fonte, cosi' il budget del buffer
  // 'dyn' resta valido per costruzione invece che per sola fortuna.
  if (apSSID.length()        > 32) apSSID.remove(32);
  if (apPassword.length()     > 63) apPassword.remove(63);
  if (staSSID.length()        > 32) staSSID.remove(32);
  if (staPassword.length()    > 63) staPassword.remove(63);
  if (bluetoothName.length()  > 32) bluetoothName.remove(32);
  if (telnetPassword.length() > 63) telnetPassword.remove(63);
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
<div class="head"><h1>&#9889; Pi_Zaff System</h1><div class="sub">WIRELESS CONSOLE SERVER &middot; RS232 &middot; v6.0</div></div>
)HEAD";

static const char HTML_TAIL[] PROGMEM = R"TAIL(<div class="card">
<div class="title">Aggiornamento Firmware (OTA)</div>
<div class="fwbox"><input type="file" id="fw" accept=".bin" style="color:#9fb3c8"></div>
<button type="button" class="ota" onclick="ota()">&#9889; CARICA FIRMWARE</button>
<div id="st"></div></div>
<div class="card">
<a class="btn save" href="/term" style="color:#000">&#128187; APRI TERMINALE</a>
<a class="btn bt-mode" href="#" onclick="return post('/mode?to=bt','Passare in modalita BT? La GUI sara inaccessibile, premi il pulsante GPIO32 per tornare.')">PASSA A MODALITA' BT</a>
<a class="btn reboot" href="#" onclick="return post('/reboot','Riavviare?')">RIAVVIA</a>
</div>
<script>
function post(url,confirmMsg){
if(confirmMsg&&!confirm(confirmMsg))return false;
var x=new XMLHttpRequest();x.open('POST',url);
x.onload=function(){document.open();document.write(x.responseText);document.close();};
x.onerror=function(){alert('Errore connessione')};
x.send();
return false;}
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

// v6.0: pagina del terminale live (/term). Pagina statica separata
// (non passa dal buffer dyn[] di handleRoot(), gia' abbastanza denso):
// al caricamento scarica lo storico recente da /term-snapshot, poi
// riceve l'output live via WebSocket sulla porta 81. L'input e' a
// riga intera (invio = comando), non carattere per carattere: un vero
// terminale interattivo con editing sul prompt del router richiederebbe
// la stessa negoziazione ECHO di telnet, complessita' non necessaria
// per questo canale pensato come comodita' aggiuntiva, non sostituto
// del client telnet per sessioni intensive.
static const char HTML_TERM[] PROGMEM = R"TERM(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pi_Zaff Terminale</title>
<style>
body{background:#0f1117;color:#fff;font-family:Arial,sans-serif;margin:0;padding:15px;display:flex;flex-direction:column;height:100vh;box-sizing:border-box}
h1{margin:0 0 10px;color:#00d4ff;font-size:18px}
#out{flex:1;background:#0d1520;border-radius:8px;padding:12px;overflow-y:auto;white-space:pre-wrap;word-break:break-all;font-family:monospace;font-size:13px;margin-bottom:10px}
#row{display:flex;gap:8px}
#cmd{flex:1;padding:11px;border:none;border-radius:8px;background:#0d1520;color:#fff;font-family:monospace;font-size:14px;box-sizing:border-box}
#send{padding:11px 18px;border:none;border-radius:8px;background:#00d4ff;color:#000;font-weight:bold;cursor:pointer}
#st{font-size:12px;color:#5a7080;margin-bottom:8px}
a{color:#00d4ff}
</style></head><body>
<h1>&#9889; Pi_Zaff Terminale</h1>
<div id="st">connessione...</div>
<div id="out"></div>
<div id="row"><input id="cmd" type="text" autocomplete="off" placeholder="comando + invio"><button id="send">Invia</button></div>
<p><a href="/">&larr; Torna alla configurazione</a></p>
<script>
var out=document.getElementById('out');
var st=document.getElementById('st');
var cmd=document.getElementById('cmd');
function append(t){out.textContent+=t;out.scrollTop=out.scrollHeight;}
fetch('/term-snapshot').then(function(r){return r.text()}).then(append).catch(function(){});
var ws=new WebSocket('ws://'+location.hostname+':81/');
ws.onopen=function(){st.textContent='live';st.style.color='#00ff9d';};
ws.onclose=function(){st.textContent='disconnesso';st.style.color='#ff4455';};
ws.onerror=function(){st.textContent='errore connessione';st.style.color='#ff4455';};
ws.onmessage=function(e){append(e.data);};
function sendCmd(){
if(ws.readyState!==1)return;
ws.send(cmd.value+'\n');
cmd.value='';
}
document.getElementById('send').onclick=sendCmd;
cmd.addEventListener('keydown',function(e){if(e.key==='Enter')sendCmd();});
</script></body></html>
)TERM";

// v6.0: la sizeof(dyn)-off usata sotto e' size_t (unsigned): se off
// dovesse mai superare sizeof(dyn) (snprintf ritorna la lunghezza che
// AVREBBE scritto, non quella troncata), la sottrazione va in
// underflow e la snprintf successiva riceve una "size" enorme scrivendo
// oltre la fine di dyn (stack overflow). Questo helper la blocca a 0
// invece di farla avvolgere. Difesa in profondita': con gli attuali
// limiti di lunghezza dei campi (v. acceptArg/loadPrefs) dyn non
// dovrebbe mai riempirsi, ma il pattern va reso comunque sicuro.
static inline size_t htmlRemain(int off, size_t bufSize) {
  return (off < 0 || (size_t)off >= bufSize) ? 0 : (bufSize - (size_t)off);
}

// v6.0: RSSI del client collegato all'AP dell'ESP32 - il segnale
// rilevante per il tipo di blocchi da traffico pesante gia' risolti
// (il telefono/laptop vicino all'ESP32, non l'ESP32 verso il router
// di casa). Ritorna true e valorizza *rssi se c'e' almeno una
// stazione associata, false altrimenti. Lettura sincrona cheap sul
// driver: va bene chiamarla da handleRoot()/handleStatus() (task
// principale), MAI da bridgeTask.
static bool getApClientRssi(int8_t* rssi) {
  wifi_sta_list_t list;
  if (esp_wifi_ap_get_sta_list(&list) != ESP_OK || list.num <= 0) return false;
  *rssi = list.sta[0].rssi;
  return true;
}

// v6.0: formatta un intervallo in millis() come HH:MM:SS, per lo
// storico connessioni (timestamp relativi all'avvio: niente NTP/RTC,
// l'AP di solito non ha accesso a internet).
static void formatUptime(unsigned long ms, char* out, size_t outSize) {
  unsigned long s = ms / 1000UL;
  unsigned int  hh = s / 3600UL;
  unsigned int  mm = (s % 3600UL) / 60UL;
  unsigned int  ss = s % 60UL;
  snprintf(out, outSize, "%02u:%02u:%02u", hh, mm, ss);
}

static void handleRoot() {
  readBattery();

  const char* battClass = (battPercent > 50) ? "bok" : (battPercent > 20 ? "bw" : "bl");

  IPAddress ip    = WiFi.softAPIP();
  IPAddress staIp = (useSTA && WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : IPAddress(0,0,0,0);

  char dyn[2400];
  int off = 0;

  off += snprintf(dyn + off, htmlRemain(off, sizeof(dyn)),
    "<div class=\"card\"><div class=\"title\">Stato Sistema</div>"
    "<div class=\"battery %s\">&#128267; %d%%</div>"
    "<div class=\"info\">Modalita': <b>WiFi</b> (pulsante GPIO32 = passa a BT)</div>"
    "<div class=\"info\">AP IP: <b>%u.%u.%u.%u</b> (%s)</div>",
    battClass, battPercent,
    ip[0], ip[1], ip[2], ip[3], apSSID.c_str());

  if (useSTA && WiFi.status() == WL_CONNECTED) {
    off += snprintf(dyn + off, htmlRemain(off, sizeof(dyn)),
      "<div class=\"info\">STA IP: <b>%u.%u.%u.%u</b> (%s) &middot; RSSI: <b>%d dBm</b></div>",
      staIp[0], staIp[1], staIp[2], staIp[3], staSSID.c_str(), (int)WiFi.RSSI());
  }

  int8_t apRssi;
  if (getApClientRssi(&apRssi)) {
    off += snprintf(dyn + off, htmlRemain(off, sizeof(dyn)),
      "<div class=\"info\">RSSI client AP: <b>%d dBm</b></div>", (int)apRssi);
  }

  off += snprintf(dyn + off, htmlRemain(off, sizeof(dyn)),
    "<div class=\"info\">Baud: <b>%u</b></div>"
    "<div class=\"info\">Heap libero: <b>%u KB</b></div>"
    "<div class=\"info\">Ring TX: <b>%u/%u byte</b></div>"
    "<div class=\"info\">Ring RX: <b>%u/%u byte</b></div>"
    "<div class=\"info\">Uptime: <b>%lu s</b></div>"
    "<div class=\"info\">Ultimo reset: <b>%s</b></div>"
    "</div>",
    (unsigned)baudRate, (unsigned)(ESP.getFreeHeap() / 1024),
    (unsigned)txQueueUsed(), (unsigned)TX_QUEUE_SIZE,
    (unsigned)rxQueueUsed(), (unsigned)RX_QUEUE_SIZE,
    millis() / 1000UL, lastResetReason);

  off += snprintf(dyn + off, htmlRemain(off, sizeof(dyn)),
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
    off += snprintf(dyn + off, htmlRemain(off, sizeof(dyn)),
      "<option value=\"%u\"%s>%u</option>",
      (unsigned)VALID_BAUDS[i],
      (baudRate == VALID_BAUDS[i]) ? " selected" : "",
      (unsigned)VALID_BAUDS[i]);
  }

  off += snprintf(dyn + off, htmlRemain(off, sizeof(dyn)),
    "</select>"
    "<button class=\"save\" type=\"submit\">SALVA E RIAVVIA</button>"
    "</form></div>");

  // v6.0: card storico connessioni, buffer SEPARATO da dyn[] per non
  // intaccarne il margine (dyn resta esattamente com'era, gia'
  // verificato). Stesso pattern sicuro htmlRemain.
  char hist[2000];
  int hoff = 0;
  hoff += snprintf(hist + hoff, htmlRemain(hoff, sizeof(hist)),
    "<div class=\"card\"><div class=\"title\">Storico Connessioni</div>");

  if (connHistoryCount == 0) {
    hoff += snprintf(hist + hoff, htmlRemain(hoff, sizeof(hist)),
      "<div class=\"info\">Nessuna connessione registrata.</div>");
  } else {
    for (uint8_t i = 0; i < connHistoryCount; i++) {
      uint8_t idx = (connHistoryNext - 1 - i + CONN_HISTORY_SIZE * 2) % CONN_HISTORY_SIZE;
      ConnHistoryEntry &e = connHistory[idx];
      char connAt[16], dur[16], status[32];
      formatUptime(e.connectedAtMs, connAt, sizeof(connAt));
      if (e.disconnectedAtMs == 0) {
        snprintf(status, sizeof(status), "ancora attivo");
      } else {
        formatUptime(e.disconnectedAtMs - e.connectedAtMs, dur, sizeof(dur));
        snprintf(status, sizeof(status), "durata %s", dur);
      }
      if (e.channel == CONN_CH_TELNET) {
        hoff += snprintf(hist + hoff, htmlRemain(hoff, sizeof(hist)),
          "<div class=\"info\">Telnet &middot; %u.%u.%u.%u &middot; +%s &middot; %s</div>",
          e.remoteIp[0], e.remoteIp[1], e.remoteIp[2], e.remoteIp[3], connAt, status);
      } else {
        hoff += snprintf(hist + hoff, htmlRemain(hoff, sizeof(hist)),
          "<div class=\"info\">Bluetooth &middot; +%s &middot; %s</div>",
          connAt, status);
      }
    }
  }
  hoff += snprintf(hist + hoff, htmlRemain(hoff, sizeof(hist)), "</div>");

  // v6.0: off/hoff sono la lunghezza che snprintf AVREBBE scritto, non
  // necessariamente quella davvero presente in dyn/hist se htmlRemain
  // ha gia' azzerato lo spazio residuo. sendContent() sotto deve
  // mandare solo i byte realmente scritti, altrimenti leggerebbe oltre
  // la fine dell'array (over-read). Completa la stessa protezione di
  // htmlRemain sul lato lettura invece di lasciarla a meta'.
  if ((size_t)off  > sizeof(dyn))  off  = sizeof(dyn);
  if ((size_t)hoff > sizeof(hist)) hoff = sizeof(hist);

  size_t headLen = strlen_P(HTML_HEAD);
  size_t tailLen = strlen_P(HTML_TAIL);
  server.setContentLength(headLen + off + hoff + tailLen);
  server.sendHeader("Connection", "close");
  server.send(200, "text/html; charset=utf-8", "");
  server.sendContent_P(HTML_HEAD);
  server.sendContent(dyn, off);
  server.sendContent(hist, hoff);
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
    else Serial.printf("[SAVE] Baud invalido %u, mantengo %u\n", (unsigned)b, (unsigned)baudRate);
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

// v6.0: come /factory (v5.0), anche /reboot e /mode ora richiedono
// POST. Erano rimasti GET: un link o una <img> malevola su una pagina
// qualunque, aperta nello stesso browser mentre l'utente e' sulla GUI
// del dispositivo, poteva far ripartire o cambiare modalita' al
// dispositivo con un semplice click (o prefetch). Stesso identico
// risultato per l'utente legittimo (i pulsanti nella GUI restano
// uguali), cambia solo il verbo HTTP sotto il cofano.
static void sendMethodNotAllowedPost(const char* path) {
  char msg[128];
  snprintf(msg, sizeof(msg),
    "Method Not Allowed: %s richiede POST (v6.0).\n"
    "Usa: curl -X POST http://<ip>%s\n", path, path);
  server.send(405, "text/plain", msg);
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
  int8_t apRssi = 0;
  bool   hasApRssi = getApClientRssi(&apRssi);

  char buf[420];
  snprintf(buf, sizeof(buf),
    "{\"mode\":\"%s\",\"batt\":%d,\"volt\":%.2f,\"heap\":%u,"
    "\"txring\":%u,\"rxring\":%u,\"telnet\":%d,\"baud\":%u,\"uptime\":%lu,"
    "\"reset\":\"%s\",\"aprssi\":%d,\"starssi\":%d,"
    "\"ver\":\"6.0\"}",
    bootModeName(bootMode), battPercent, battVoltage,
    (unsigned)ESP.getFreeHeap(),
    (unsigned)txQueueUsed(), (unsigned)rxQueueUsed(),
    (telnetClient && telnetClient.connected()) ? 1 : 0,
    (unsigned)baudRate,
    millis() / 1000UL,
    lastResetReason,
    hasApRssi ? (int)apRssi : 0,
    (useSTA && WiFi.status() == WL_CONNECTED) ? (int)WiFi.RSSI() : 0);
  server.send(200, "application/json", buf);
}

// v6.0: snapshot testuale dello scrollback per il caricamento iniziale
// della pagina /term (l'aggiornamento live dopo il caricamento arriva
// via WebSocket, vedi pumpWsTerminal()). Streaming a chunk come
// handleRoot(), stesso pattern gia' in uso nel file.
static void handleTermSnapshot() {
  uint32_t total = scrollTotal;
  uint32_t from  = (total > SCROLLBACK_SIZE) ? (total - SCROLLBACK_SIZE) : 0;

  server.setContentLength(total - from);
  server.send(200, "text/plain; charset=utf-8", "");

  static uint8_t chunk[512];
  while (from < total) {
    size_t n = 0;
    while (from < total && n < sizeof(chunk)) {
      chunk[n++] = scrollback[from % SCROLLBACK_SIZE];
      from++;
    }
    server.sendContent((const char*)chunk, n);
  }
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
  // v6.0: /reboot e /mode, come /factory, accettano SOLO POST.
  server.on("/reboot",  HTTP_POST, handleReboot);
  server.on("/reboot",  HTTP_GET,  []() { sendMethodNotAllowedPost("/reboot"); });
  server.on("/mode",    HTTP_POST, handleModeChange);
  server.on("/mode",    HTTP_GET,  []() { sendMethodNotAllowedPost("/mode"); });
  server.on("/ledtest", HTTP_GET,  handleLedTest);
  server.on("/status",  HTTP_GET,  handleStatus);
  // v6.0: terminale live (WebSocket sulla porta 81).
  server.on("/term",          HTTP_GET, []() { server.send_P(200, PSTR("text/html; charset=utf-8"), HTML_TERM); });
  server.on("/term-snapshot", HTTP_GET, handleTermSnapshot);
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

  // v6.0: mDNS, cosi' non serve ricordare l'IP dell'AP.
  if (MDNS.begin("pizaff")) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("telnet", "tcp", 23);
    Serial.println(F("[NET] mDNS attivo: pizaff.local"));
  } else {
    Serial.println(F("[NET] mDNS FAIL"));
  }

  // v6.0: terminale live. onEvent()/loop() girano SOLO da loop(),
  // mai da bridgeTask (vedi commento su WEBSOCKETS_TCP_TIMEOUT).
  wsServer.begin();
  wsServer.onEvent(onWsEvent);
  Serial.println(F("[NET] WebSocket terminale :81 (/term)"));
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
// v6.0: ferma bridgeTask in modo cooperativo prima di un reboot/deep
// sleep che tocchera' il subsystem di rete (WiFi.disconnect/SerialBT.end
// nello shutdown batteria, o il reboot per cambio modalita'). Segnala
// lo stop e aspetta che il task esca da solo a inizio ciclo, cosi' non
// viene MAI ucciso mentre e' dentro una write() di rete: eviterebbe un
// eventuale lock interno di lwIP orfano che bloccherebbe per sempre la
// prima chiamata WiFi successiva. Se per qualche motivo non esce entro
// 200ms (non dovrebbe mai succedere, il ciclo e' <=~100ms nel caso
// peggiore), la vTaskDelete resta come ultima spiaggia.
static void stopBridgeTaskSafely() {
  if (bridgeTaskHandle == NULL) return;

  bridgeStopRequested = true;
  unsigned long t0 = millis();
  while (bridgeTaskHandle != NULL && millis() - t0 < 200) {
    delay(2);
  }
  if (bridgeTaskHandle != NULL) {
    vTaskDelete(bridgeTaskHandle);
    bridgeTaskHandle = NULL;
  }
  bridgeStopRequested = false;
}

static void bridgeTask(void *param) {
  esp_task_wdt_add(NULL);
  Serial.printf("[BRIDGE] task partito su core %d\n", xPortGetCoreID());

  unsigned long lastBattWarnCheck = 0;

  for (;;) {
    esp_task_wdt_reset();

    if (bridgeStopRequested) break;   // v6.0: uscita pulita, mai a meta' write()

    unsigned long t0 = millis();

    if (!otaInProgress) {
      // 1) UART -> rxQueue (output del router verso il client).
      //    v5.0: scarta subito se non c'e' alcun client.
      drainSerialToRxQueue();

      // 2) Client -> txQueue (i comandi che l'utente digita).
      if (bootMode == MODE_WIFI) { handleTelnet(); handleWsInput(); }
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

  // v6.0: uscita cooperativa richiesta da stopBridgeTaskSafely(). Si
  // azzera l'handle PRIMA di autocancellarsi: da questo punto in poi
  // il chiamante vede bridgeTaskHandle == NULL e sa che il task non
  // tocchera' piu' nulla.
  esp_task_wdt_delete(NULL);
  bridgeTaskHandle = NULL;
  vTaskDelete(NULL);
}

// ──────────────────────────────────────────────────────────────────
//  SETUP
// ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(80);
  Serial.println(F("\n[Pi_Zaff] Console Server v6.0"));
  if (USB_DEBUG_MODE)
    Serial.println(F("** USB_DEBUG_MODE: batteria simulata 100% **"));

  // v6.0: motivo dell'ultimo reset, prima possibile nel boot.
  lastResetReason = resetReasonToStr(esp_reset_reason());
  Serial.printf("[BOOT] motivo ultimo reset: %s\n", lastResetReason);

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

// v6.0: manda ai client del terminale WebSocket solo i byte nuovi
// scritti in scrollback dall'ultima chiamata (mai l'intero buffer).
// Gira interamente da loop()/task principale, MAI da bridgeTask - vedi
// il commento su WEBSOCKETS_TCP_TIMEOUT in testa al file: e' proprio
// wsServer.loop()/broadcastTXT() la parte che puo' bloccare (fino al
// timeout ridotto a 100ms) se un client e' bloccato, e va tenuta
// isolata dal ponte seriale/telnet/BT realtime.
static void pumpWsTerminal() {
  wsServer.loop();

  static uint32_t wsLastSentHead = 0;
  uint32_t total = scrollTotal;
  if (total == wsLastSentHead) return;

  // Se e' passato piu' di un giro intero di buffer dall'ultimo invio
  // (client appena connesso, o rimasto indietro), non rincorriamo
  // tutta la storia perduta: ripartiamo dagli ultimi byte disponibili.
  uint32_t from = (total - wsLastSentHead > SCROLLBACK_SIZE) ? (total - SCROLLBACK_SIZE) : wsLastSentHead;

  static uint8_t chunk[256];
  while (from < total) {
    size_t n = 0;
    while (from < total && n < sizeof(chunk)) {
      chunk[n++] = scrollback[from % SCROLLBACK_SIZE];
      from++;
    }
    wsServer.broadcastTXT(chunk, n);
  }
  wsLastSentHead = total;
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
    pumpWsTerminal();
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
