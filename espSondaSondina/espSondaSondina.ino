/**
 * @file esp8266_firmware.cpp
 * @brief Firmware per ESP8266 che agisce come gateway WiFi per un STM32.
 * @author Riccardo Bonagura, con revisione e refactoring di Copilot
 * @version 5.5 (Build Stabile - Correzioni Critiche)
 *
 * @details
 * - Risolto problema di troncamento seriale durante l'init con Serial.flush().
 * - Riattivata connessione HTTPS stabile per le API Gemini con setInsecure().
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <time.h>
#include <Hash.h>

// --- CONFIGURAZIONE ---
const char* ap_ssid = "PlantPen";
const char* ap_password = "password123";
const char* GEMINI_API_KEY = ""; // Inserisci qui la tua chiave API Gemini (non committare mai la chiave reale)
#define SERIAL_BAUD_RATE 115200
#define STM32_RESPONSE_TIMEOUT 15000
#define WIFI_CONNECT_TIMEOUT 30000

// --- STATO GLOBALE ---
bool masterSimulationEnabled = false;

// --- STRUTTURE DATI ---
struct WiFiCredentials { char ssid[33]; char password[65]; };

// --- OGGETTI GLOBALI ---
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);
WiFiClientSecure client;

// --- STATO E CACHE ---
String serialBuffer = "";
String cachedInitData = "{}";
volatile bool waitingForStmResponse = false;
volatile bool stmResponseReceived = false;
String expectedResponseType = "";
String stmResponsePayload = "";

// --- PROTOTIPI FUNZIONI ---
void sendSerialCommand(const String& command);
bool handleFileRead(String path);
void processSerialMessage(String msg);
bool waitForStmResponse(const String& commandToSend, const String& responseType, unsigned long timeout);
String getAiAdvice(const String& plantType, const JsonObject& measurementData);
void saveCredentials(const char* ssid, const char* password);
void loadCredentials(WiFiCredentials& creds);
void handleInit();
void handleAddPlant();
void handleStartMeasurement();
void handleGetHistory();
void handleGetAiAdvice();
void handleWiFiScan();
void handleWiFiConnect();
void handleWiFiStatus();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void checkSerial();
String escapeJsonString(const String& input);
time_t iso8601ToUnix(const char* iso8601);

/**
 * @brief Converte una stringa in formato ISO 8601 in un timestamp UNIX.
 */
time_t iso8601ToUnix(const char* iso8601) {
  struct tm t;
  if (sscanf(iso8601, "%d-%d-%dT%d:%d:%d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) {
    return 0;
  }
  t.tm_year -= 1900;
  t.tm_mon -= 1;
  setenv("TZ", "UTC", 1);
  tzset();
  return mktime(&t);
}

// =============================================================================
// --- SETUP ---
// =============================================================================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  
  EEPROM.begin(sizeof(WiFiCredentials));

  if (!LittleFS.begin()) {
    return;
  }

  WiFiCredentials creds;
  loadCredentials(creds);
  bool connected = false;
  if (strlen(creds.ssid) > 0) {
    WiFi.begin(creds.ssid, creds.password);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
      delay(500);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      time_t now = time(nullptr);
      while (now < 8 * 3600 * 2) {
        delay(500);
        now = time(nullptr);
      }
    } else {
      WiFi.disconnect(true); 
    }
  }

  if (!connected) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
  }

  MDNS.begin("plantpen");

  // Routing delle API HTTP
  server.on("/", HTTP_GET, [](){ handleFileRead("/index.html"); });
  server.on("/sw.js", HTTP_GET, [](){ handleFileRead("/sw.js"); });
  server.on("/manifest.json", HTTP_GET, [](){ handleFileRead("/manifest.json"); });
  server.on("/simulator", HTTP_GET, [](){ handleFileRead("/simulator.html"); });
  server.on("/init", HTTP_POST, handleInit);
  server.on("/add_plant", HTTP_POST, handleAddPlant);
  server.on("/start_measurement", HTTP_POST, handleStartMeasurement);
  server.on("/measurements", HTTP_GET, handleGetHistory);
  server.on("/get_ai_advice", HTTP_POST, handleGetAiAdvice);
  server.on("/wifi_scan", HTTP_GET, handleWiFiScan);
  server.on("/wifi_connect", HTTP_POST, handleWiFiConnect);
  server.on("/wifi_status", HTTP_GET, handleWiFiStatus);
  
  server.on("/toggle_master_simulation", HTTP_GET, []() {
    masterSimulationEnabled = server.arg("enable") == "true";
    server.send(200, "text/plain", String("Simulazione Master: ") + (masterSimulationEnabled ? "ON" : "OFF"));
  });
  server.on("/toggle_serial_debug", HTTP_GET, []() {
    server.send(200, "text/plain", String("Debug Seriale rimosso dal firmware."));
  });

  server.onNotFound([](){ if (!handleFileRead(server.uri())) server.send(404, "text/plain", "404: Not Found"); });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// =============================================================================
// --- LOOP PRINCIPALE ---
// =============================================================================
void loop() {
  webSocket.loop();
  server.handleClient();
  MDNS.update();
  if (!waitingForStmResponse) {
    checkSerial();
  }
}

// =============================================================================
// --- GESTIONE API HTTP ---
// =============================================================================

void handleInit() {
  if (server.method() != HTTP_POST) return server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
  
  if (masterSimulationEnabled) {
    const char* fakeInitData = "{\"type\":\"INIT_DATA\",\"plantTypes\":[{\"id\":1,\"name\":\"Cactus (Sim)\"},{\"id\":2,\"name\":\"Pomodoro (Sim)\"}],\"registeredPlants\":[{\"plantId\":101,\"typeId\":2,\"name\":\"Pomodoro Balcone (Sim)\",\"notes\":\"Esposizione sud-est\"}]}";
    server.send(200, "application/json", fakeInitData);
    return;
  }

  StaticJsonDocument<256> doc;
  deserializeJson(doc, server.arg("plain"));
  
  time_t timestamp = 0;
  if (doc["timestamp"].is<const char*>()) {
    const char* timestampString = doc["timestamp"];
    timestamp = iso8601ToUnix(timestampString);
    if (timestamp == 0) {
      return server.send(400, "application/json", "{\"error\":\"Formato timestamp stringa non valido\"}");
    }
  } else if (doc["timestamp"].is<unsigned long>()) {
    timestamp = doc["timestamp"].as<unsigned long>();
  } else {
    return server.send(400, "application/json", "{\"error\":\"Timestamp mancante o di tipo non supportato\"}");
  }

  StaticJsonDocument<256> rtcCmd;
  rtcCmd["cmd"] = "SET_RTC";
  rtcCmd["timestamp"] = timestamp;
  String rtcCommandString;
  serializeJson(rtcCmd, rtcCommandString);
  sendSerialCommand(rtcCommandString);

  // *** LA CORREZIONE ***
  // Serial.flush() attende che il buffer di trasmissione seriale sia vuoto.
  // Questo garantisce che il primo comando sia stato completamente inviato
  // prima di procedere con il secondo.
  Serial.flush();
  delay(50); // Un piccolo delay extra per sicurezza

  String getInitCommand = "{\"cmd\":\"GET_INIT_DATA\"}";
  if (waitForStmResponse(getInitCommand, "INIT_DATA", STM32_RESPONSE_TIMEOUT)) {
    cachedInitData = stmResponsePayload;
    server.send(200, "application/json", cachedInitData);
  } else {
    server.send(504, "application/json", "{\"error\":\"Timeout dall'STM32 per INIT_DATA\"}");
  }
}

void handleAddPlant() {
  if (server.method() != HTTP_POST) return server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
  
  String body = server.arg("plain");
  StaticJsonDocument<512> receivedDoc;
  deserializeJson(receivedDoc, body);

  StaticJsonDocument<512> commandDoc;
  commandDoc["cmd"] = "ADD_PLANT";
  commandDoc["idTipo"] = receivedDoc["typeId"];
  commandDoc["nomePianta"] = receivedDoc["newPlantName"];
  
  String command;
  serializeJson(commandDoc, command);
  
  if (waitForStmResponse(command, "ACK", STM32_RESPONSE_TIMEOUT)) {
    server.send(200, "application/json", stmResponsePayload);
  } else {
    server.send(504, "application/json", "{\"error\":\"Timeout dall'STM32 per ADD_PLANT\"}");
  }
}

void handleStartMeasurement() {
  if (server.method() != HTTP_POST) return server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
  
  String body = server.arg("plain");
  StaticJsonDocument<256> receivedDoc;
  deserializeJson(receivedDoc, body);

  StaticJsonDocument<256> commandDoc;
  commandDoc["cmd"] = "START_MEAS";
  commandDoc["idPianta"] = receivedDoc["plantId"];

  String command;
  serializeJson(commandDoc, command);
  
  sendSerialCommand(command);
  server.send(202, "application/json", "{\"status\":\"Misurazione avviata\"}");
}

void handleGetHistory() {
  if (!server.hasArg("plantId")) return server.send(400, "application/json", "{\"error\":\"plantId mancante\"}");
  
  StaticJsonDocument<256> doc;
  doc["cmd"] = "GET_HISTORY";
  doc["idPianta"] = server.arg("plantId").toInt();

  String command;
  serializeJson(doc, command);

  if (waitForStmResponse(command, "HISTORY_RESULTS", STM32_RESPONSE_TIMEOUT)) {
    server.send(200, "application/json", stmResponsePayload);
  } else {
    server.send(504, "application/json", "{\"error\":\"Timeout dall'STM32 per GET_HISTORY\"}");
  }
}

void handleGetAiAdvice() {
  if (WiFi.status() != WL_CONNECTED) return server.send(200, "application/json", "{\"advice\":null, \"error\":\"Nessuna connessione a Internet\"}");
  if (strcmp(GEMINI_API_KEY, "YOUR_GEMINI_API_KEY_HERE") == 0) return server.send(200, "application/json", "{\"advice\":null, \"error\":\"Chiave API Gemini non configurata\"}");
  String body = server.arg("plain");
  StaticJsonDocument<1024> doc;
  deserializeJson(doc, body);
  String plantType = doc["plantType"];
  JsonObject measurementData = doc["measurement"];
  String advice = getAiAdvice(plantType, measurementData);
  StaticJsonDocument<1024> responseDoc;
  if (advice.length() > 0) responseDoc["advice"] = advice;
  else { responseDoc["advice"] = nullptr; responseDoc["error"] = "Impossibile ottenere consiglio dall'AI"; }
  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

String getAiAdvice(const String& plantType, const JsonObject& data) {
  String prompt = "Sei un esperto giardiniere. Una pianta di tipo '" + plantType + "' ";
  prompt += "presenta i seguenti valori: ";
  prompt += String(data["temperatura_valore"].as<float>(), 1) + "°C, ";
  prompt += String(data["luce_valore"].as<int>()) + " lux, ";
  prompt += String(data["umidita_interna_valore"].as<float>(), 1) + "% umidità suolo. ";
  prompt += "Basandoti su questi dati, fornisci un breve e utile consiglio di massimo 30 parole in italiano.";
  StaticJsonDocument<1024> reqBody;
  JsonArray contents = reqBody.createNestedArray("contents");
  JsonObject content_item = contents.createNestedObject();
  JsonArray parts = content_item.createNestedArray("parts");
  JsonObject part_item = parts.createNestedObject();
  part_item["text"] = prompt;
  String requestBodyString;
  serializeJson(reqBody, requestBodyString);
  
  // *** LA CORREZIONE ***
  // Ripristina setInsecure() per la massima compatibilità con ESP8266
  client.setInsecure();
  
  HTTPClient http;
  String apiUrl = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash-latest:generateContent?key=" + String(GEMINI_API_KEY);
  
  if (http.begin(client, apiUrl)) {
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(requestBodyString);

    if (httpCode > 0) {
        String payload = http.getString();
        if (httpCode == HTTP_CODE_OK) {
            DynamicJsonDocument resDoc(2048);
            if (deserializeJson(resDoc, payload)) { http.end(); return ""; }
            if (resDoc.containsKey("promptFeedback")) { http.end(); return ""; }
            if (resDoc["candidates"][0]["content"]["parts"][0]["text"].is<const char*>()) {
                String advice = resDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
                http.end(); return advice;
            }
        }
    }
    http.end();
  }
  return "";
}

// --- IL RESTO DEL FILE RIMANE INVARIATO ---

void handleWiFiScan() {
  int n = WiFi.scanNetworks();
  StaticJsonDocument<1024> doc;
  JsonArray networks = doc.createNestedArray("networks");
  for (int i = 0; i < n; ++i) {
    JsonObject network = networks.createNestedObject();
    network["ssid"] = WiFi.SSID(i);
    network["rssi"] = WiFi.RSSI(i);
    network["encryption"] = WiFi.encryptionType(i);
  }
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleWiFiConnect() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  if (ssid.length() == 0) return server.send(400, "text/plain", "SSID non può essere vuoto");
  saveCredentials(ssid.c_str(), password.c_str());
  server.send(200, "text/plain", "Credenziali salvate. Riavvio per connettere...");
  delay(1000);
  ESP.restart();
}

void handleWiFiStatus() {
  StaticJsonDocument<256> doc;
  doc["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "STA";
  doc["connected"] = (WiFi.status() == WL_CONNECTED);
  doc["ssid"] = WiFi.isConnected() ? WiFi.SSID() : (WiFi.getMode() == WIFI_AP ? String(ap_ssid) : "");
  doc["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void saveCredentials(const char* ssid, const char* password) {
  WiFiCredentials creds;
  strncpy(creds.ssid, ssid, sizeof(creds.ssid) - 1);
  creds.ssid[sizeof(creds.ssid) - 1] = '\0';
  strncpy(creds.password, password, sizeof(creds.password) - 1);
  creds.password[sizeof(creds.password) - 1] = '\0';
  EEPROM.put(0, creds);
  EEPROM.commit();
}

void loadCredentials(WiFiCredentials& creds) {
  EEPROM.get(0, creds);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: break;
    case WStype_CONNECTED: break;
    case WStype_TEXT: {
      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, payload, length)) return;
      const char* action = doc["action"];
      if (action && strcmp(action, "SIMULATE_STM_RESPONSE") == 0) {
        if (masterSimulationEnabled) processSerialMessage(doc["payload"]);
      }
      break;
    }
    default: break;
  }
}

void sendSerialCommand(const String& command) {
  Serial.println(command);
  Serial.flush();
  String logMsg = "{\"type\":\"LOG\",\"source\":\"ESP_TX\",\"data\":\"" + escapeJsonString(command) + "\"}";
  webSocket.broadcastTXT(logMsg);
}

void processSerialMessage(String msg) {
  if (msg.length() == 0) return;
  String logMsg = "{\"type\":\"LOG\",\"source\":\"STM32_RX\",\"data\":\"" + escapeJsonString(msg) + "\"}";
  webSocket.broadcastTXT(logMsg);
  
  StaticJsonDocument<8192> doc;
  if (deserializeJson(doc, msg)) {
    return;
  }
  
  const char* type = doc["type"];
  if (!type) return;

  if (waitingForStmResponse && expectedResponseType.equals(type)) {
    stmResponsePayload = msg;
    stmResponseReceived = true;
  }

  String typeStr = String(type);
  if (typeStr == "MEAS_RESULTS" || typeStr == "ERR") {
    webSocket.broadcastTXT(msg);
  }
}

void checkSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      if(serialBuffer.length() > 0) {
        processSerialMessage(serialBuffer);
      }
      serialBuffer = "";
    } else if (c >= 32) {
      serialBuffer += c;
    }
  }
}

String escapeJsonString(const String& input) {
  String output = "";
  output.reserve(input.length());
  for (size_t i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c == '"' || c == '\\') {
      output += '\\';
    }
    output += c;
  }
  return output;
}

bool waitForStmResponse(const String& commandToSend, const String& responseType, unsigned long timeout) {
    expectedResponseType = responseType;
    waitingForStmResponse = true;
    stmResponseReceived = false;
    stmResponsePayload = "";
    
    sendSerialCommand(commandToSend);
    
    unsigned long startTime = millis();
    while (millis() - startTime < timeout) {
        checkSerial();
        webSocket.loop();
        server.handleClient();
        if (stmResponseReceived) {
            break;
        }
        yield();
    }
    
    waitingForStmResponse = false;
    return stmResponseReceived;
}

bool handleFileRead(String path) {
  String contentType = "text/plain";
  if (path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";
  else if (path.endsWith(".json")) contentType = "application/json";
  else if (path.endsWith(".webmanifest")) contentType = "application/manifest+json";
  else if (path.endsWith(".ico")) contentType = "image/x-icon";
  
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

