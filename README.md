# Pi_Zaff — Wireless Console Server (Console ESP32)

Server di console **seriale over WiFi / Bluetooth** su ESP32.

Colleghi l'ESP32 alla porta **console RS-232** di un router, switch, firewall,
UPS o qualsiasi apparato con console seriale, e da quel momento ci accedi senza
filo:

- **via WiFi** — client Telnet (porta 23) *oppure* terminale live nel browser
- **via Bluetooth** — porta COM virtuale (SPP), come un normale adattatore
  USB-seriale ma wireless

Alimentazione a **batteria 18650**, 4 LED di stato, un solo pulsante. Nessun PC
sempre acceso attaccato all'apparato per leggerne la console.

---

## Contenuto del repository

| Percorso | Cosa contiene |
|---|---|
| `Pi_Zaff_v5_0_STABLE/` | Firmware **corrente** (banner interno: v6.0) — Telnet + GUI web + terminale WebSocket + storico connessioni + mDNS + OTA |
| `Pi_Zaff_v5_0_ROLLBACK/` | Firmware **v5.0**, versione di ripiego: stesso hardware e pinout, ma senza terminale WebSocket / storico / mDNS. Da usare solo se la v6.0 dà problemi |
| `PiZaff_Schema_Collegamenti.html` | Schema dei collegamenti (apri nel browser) |
| `PiZaff_Schema_LED.html` | Legenda dei 4 LED |
| `PiZaff_Case.scad` / `ESPCLI.3mf` | Case per stampa 3D (OpenSCAD + progetto slicer) |

---

## Hardware

- **ESP32 classico** (WROOM-32 / DevKit) — serve il **Bluetooth classico (SPP)**,
  quindi **non** ESP32-S3/C3/C6 che ne sono privi
- **MAX3232** (adattatore livelli TTL ↔ RS-232) sulla UART2 verso l'apparato
- Batteria **18650** con partitore di tensione verso GPIO34
- 4 LED + resistenze
- 1 pulsante (tra GPIO32 e GND)

### Pinout

```
LED_ACT  = GPIO25      flash sul passaggio di dati
LED_WIFI = GPIO26      modalità WiFi: lampeggia (nessun client) / fisso (client connesso)
LED_BT   = GPIO27      modalità BT:   lampeggia (nessun client) / fisso (client connesso)
LED_BATT = GPIO33      lampeggio di warning batteria

UART2 verso MAX3232:   RX = GPIO16   TX = GPIO17
Batteria 18650 (part.): GPIO34
Pulsante modalità:      GPIO32  → GND (pull-up interno)
```

Schema completo in `PiZaff_Schema_Collegamenti.html`, legenda LED in
`PiZaff_Schema_LED.html`.

---

## Compilazione e flash (Arduino IDE)

Sono sketch `.ino`, si compilano con **Arduino IDE** (o arduino-cli).

1. **Boards Manager** → installa *esp32 by Espressif Systems*.
2. **Tools → Board** → `ESP32 Dev Module`.
3. **Tools → Partition Scheme** → **`Minimal SPIFFS (1.9MB APP with OTA)`**
   (o comunque uno schema **con OTA**: il firmware include `Update.h` per
   l'aggiornamento via web).
4. Solo per `Pi_Zaff_v5_0_STABLE` (v6.0): installa la libreria
   **`WebSockets` di Markus Sattler (arduinoWebSockets / Links2004)** dal
   Library Manager. La versione ROLLBACK non richiede librerie esterne.
5. Apri `Pi_Zaff_v5_0_STABLE/Pi_Zaff_v5_0_STABLE.ino`, seleziona la porta,
   **Upload**.
6. Monitor seriale a **115200** baud per vedere il log di boot.

Aggiornamenti successivi si possono fare **via web** (OTA, vedi sotto) senza
ricollegare l'USB.

---

## Primo collegamento

### Le due modalità

Il dispositivo parte in **una sola** modalità alla volta, scelta col pulsante:

| Modalità | Cosa attiva | Heap libera |
|---|---|---|
| **MODE_WIFI** (default) | Access Point + eventuale STA + Telnet + GUI web + terminale WebSocket + mDNS + OTA | ~140 KB |
| **MODE_BT** | Solo Bluetooth SPP (COM virtuale Windows/Android). WiFi completamente spento. Nessuna GUI | ~100 KB |

**Pulsante GPIO32:**

- **Pressione lunga (> 1.5 s)** a dispositivo acceso → cambia modalità e
  riavvia
- **Tenuto premuto all'accensione** → recovery: forza **MODE_WIFI** (via
  d'uscita se ci si è chiusi fuori in BT)

### In WiFi

1. Connettiti all'access point:
   - SSID **`ConsoleESP32`** · password **`12345678`**
2. Apri **http://192.168.4.1** oppure **http://pizaff.local**
3. Dalla **GUI web**:
   - imposta il **baud rate** della console dell'apparato (9600 default)
   - opzionale: SSID/password della **tua rete** (STA) per raggiungere il
     dispositivo senza doverti agganciare al suo AP
   - opzionale: **password Telnet** (`console` di default) e se richiederla
   - **`/term`** → terminale live nel browser (WebSocket su porta 81, con
     scrollback recente in RAM da 8 KB)
   - home page → stato: RSSI client AP e link STA, motivo dell'ultimo reset,
     **storico ultime 20 connessioni** Telnet/BT
4. In alternativa alla GUI: client **Telnet** su `192.168.4.1` **porta 23**.

### In Bluetooth

1. Pressione lunga sul pulsante per passare a MODE_BT (si riavvia).
2. Accoppia il dispositivo **`ConsoleESP32`** da Windows/Android.
3. Apri la **porta COM virtuale** (SPP) con PuTTY / minicom / screen ai baud
   impostati.

### Baud rate supportati

`9600` · `19200` · `38400` · `57600` · `115200`
(impostati dalla GUI web e salvati in NVS; persistono al riavvio).

---

## Endpoint web (MODE_WIFI)

| Metodo | Path | Funzione |
|---|---|---|
| GET  | `/` | Home page: stato, storico connessioni, RSSI, reset reason |
| POST | `/save` | Salva configurazione (SSID/pass AP+STA, baud, telnet, nome BT) |
| GET  | `/status` | Stato in forma compatta |
| GET  | `/term` | Terminale live nel browser (WebSocket porta 81) |
| GET  | `/term-snapshot` | Snapshot dello scrollback corrente |
| GET  | `/ledtest` | Accende tutti i LED per qualche secondo |
| POST | `/reboot` | Riavvio |
| POST | `/mode` | Cambia modalità WiFi/BT |
| POST | `/update` | **OTA**: carica un nuovo `.bin` compilato |
| POST | `/factory` | **Reset di fabbrica** (cancella la config in NVS) |

`/reboot`, `/mode`, `/factory` e `/update` richiedono **POST** (un semplice link
GET non deve poter riavviare o resettare il dispositivo). Da terminale:

```bash
curl -X POST http://192.168.4.1/factory      # reset config
curl -X POST http://192.168.4.1/reboot       # riavvio
```

---

## LED di stato

| LED | Significato |
|---|---|
| **LED_ACT** (25) | Flash a ogni passaggio di dati sulla console |
| **LED_WIFI** (26) | MODE_WIFI: lampeggia = nessun client, fisso = client connesso. Spento in MODE_BT |
| **LED_BT** (27) | MODE_BT: lampeggia = nessun client, fisso = client connesso. Spento in MODE_WIFI |
| **LED_BATT** (33) | Lampeggio di warning: più veloce man mano che la batteria scende (soglie 15% / 10% / 5%) |

A batteria **critica** il dispositivo si spegne in modo pulito per non
corrompere la flash.

---

## Come funziona (in breve)

- Un **task dedicato** (`bridgeTask`, core 1) fa da ponte fra la UART e il
  client di rete/BT, con due **ring buffer** separati (client→UART e
  UART→client) senza stato condiviso fra i due lettori.
- Se **nessun client** è connesso, i byte in arrivo dalla console vengono
  scartati subito: niente backlog stantìo consegnato al prossimo che si
  collega.
- Protezioni contro i **client "zombie"** (link caduto senza FIN/RST, tipico
  quando il WiFi sparisce durante un output pesante): dopo N `write()` di fila
  che non consegnano nemmeno un byte, la connessione viene chiusa a forza,
  invece di restare impiantati sul peer morto.
- Il terminale WebSocket gira **solo da `loop()`**, mai dal task ponte, per non
  reintrodurre blocchi nel percorso critico.
- **Watchdog** hardware a 30 s.

Il changelog dettagliato (bug-fix e scelte di design versione per versione) è in
testa a `Pi_Zaff_v5_0_STABLE/Pi_Zaff_v5_0_STABLE.ino`.

---

## Reset di fabbrica

`POST /factory` dalla GUI/curl: cancella la config in NVS e riavvia. Tornano i
default (`ConsoleESP32` / `12345678`, baud 9600, telnet `console`, MODE_WIFI).

---

## Licenza

Copyright (C) 2026 Pietro Zaffarano.

Questo progetto è distribuito con licenza **GNU GPL v3.0**: puoi usarlo,
studiarlo, modificarlo e ridistribuirlo, ma ogni derivato che distribuisci
deve restare open source con la stessa licenza — non può essere chiuso in un
prodotto proprietario né essere fatto passare per opera di qualcun altro.
Testo completo in [LICENSE](LICENSE).
