// #include <ESP8266WiFi.h>
// #include <WiFiClient.h>
// #include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <FS.h>
// #include <ESPAsyncTCP.h>
// #include <ESPAsyncWebServer.h>

// communication to arduino
#define BAUDRATE 115200
#define SERIAL_CONFIG SERIAL_8N1
#define SERIAL_START_BYTE 0xFF
#define SERIAL_STOP_BYTE 0x00
#define SERIAL_MODE_TEXT 1
#define SERIAL_MODE_ANIM 2

// text modes
#define TEXT_STATIC 1
#define TEXT_SCROLL 2
#define TEXT_SCROLL_IF_LONG 3
#define TEXT_SCROLL_BOTH_LAYERS 4
#define TEXT_STATIC_BOTH_LAYERS 5
// anim modes
#define SOLID 32
#define WIPE 33
#define WIPE_DIAGONAL 34
#define SNAKE 35
#define BOX_OUTLINE 36
// special animations
#define PACMAN 64

#define PIN_MASTER_SLAVE_MODE D7
#define ARDUINO_MASTER 0
#define WIFI_MASTER 1

#define PIN_WIFI_ENABLED D6
#define WIFI_ENABLED 1
#define WIFI_DISABLED 0

#define PIN_WIFI_CONNECTED D5
#define WIFI_CONNECTED 1
#define WIFI_DISCONNECTED 0

const char *ssid = "EE-de2brd_EXT";
const char *password = "golf-drift-key";
const char *ARG_TEXT = "text";

ESP8266WebServer server(80);
// AsyncWebServer server(80);

bool isMaster = false;

void setup() {
    pinMode(PIN_MASTER_SLAVE_MODE, INPUT);
    pinMode(PIN_WIFI_ENABLED, OUTPUT);
    pinMode(PIN_WIFI_CONNECTED, OUTPUT);

    digitalWrite(PIN_WIFI_ENABLED, WIFI_ENABLED);
    digitalWrite(PIN_WIFI_CONNECTED, WIFI_DISCONNECTED);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.begin(BAUDRATE, SERIAL_CONFIG);
    WiFi.begin(ssid, password);

    // wait for wifi to connect
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        // Serial.print(".");
    }
    digitalWrite(LED_BUILTIN, HIGH);
    // Serial.println("");
    // Serial.println("IP address: ");
    // Serial.println(WiFi.localIP());

    SPIFFS.begin();

    server.serveStatic("/", SPIFFS, "/index.html");
    server.serveStatic("/style.css", SPIFFS, "/style.css");
    server.serveStatic("/scripts.js", SPIFFS, "/scripts.js");

    // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    //     request -> send(SPIFFS, "/index.html", "text/html");
    // });

    // server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    //     request -> send(SPIFFS, "/style.css", "text/css");
    // });

    // server.on("/scripts.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    //     request -> send(SPIFFS, "/scripts.js", "text/javascript");
    // });

    // server.on("/text", HTTP_POST, [](AsyncWebServerRequest *request) {
    //     digitalWrite(LED_BUILTIN, LOW);
    // });

    // server.on("/", handleRoot);
    server.on("/text", handleTextMode);
    server.on("/anim", handleAnimMode);

    server.begin();
    // Serial.println("Server started");

    // digitalWrite(PIN_WIFI_ENABLE, WIFI_ENABLED);

    digitalWrite(PIN_WIFI_CONNECTED, WIFI_CONNECTED);
    while (digitalRead(PIN_MASTER_SLAVE_MODE) != WIFI_MASTER) {
        delay(1);
    }
    isMaster = true;
}

void loop() {
    server.handleClient();

    uint8_t masterSlaveMode = digitalRead(PIN_MASTER_SLAVE_MODE);
    if (masterSlaveMode == WIFI_MASTER && isMaster == false) {
        isMaster = true;
        Serial.begin(BAUDRATE, SERIAL_CONFIG);
    } else if (masterSlaveMode != WIFI_MASTER && isMaster == true) {
        isMaster = false;
        Serial.end();
    }
}

void handleRoot() {
    // server.sendHeader("Location", "/text", true);
    // server.send(302, "text/plain", "");

    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
}

void handleTextMode() {
    // if (server.args() > 0) {
    //     if (server.arg(ARG_TEXT) && isMaster) {
    //         Serial.print(server.arg(ARG_TEXT));
    //     }
    // }

    // server.send(200, "text/html", textHTML);



    char textMode = server.arg("textmode")[0];
    uint8_t mode = textMode - '0';

    Serial.write(SERIAL_START_BYTE);
    Serial.write(SERIAL_MODE_TEXT);
    Serial.write(mode);
    Serial.print(server.arg("text-input"));
    Serial.write(SERIAL_STOP_BYTE);


    // String message;
    // message += server.arg("text-input") + '\n';
    // message += server.arg("textmode");

    // char message[2];
    // message[1] = '\0';

    // server.send(200, "text/plain", message);
}

void handleAnimMode() {
    Serial.write(SERIAL_START_BYTE);
    Serial.write(SERIAL_MODE_ANIM);

    char animMode = server.arg("animmode")[0];
    Serial.write(animMode);
    if (animMode == WIPE) {
        uint8_t dir = server.arg("dir")[0] - '0';
        Serial.write(dir);
    } else if (animMode == WIPE_DIAGONAL) {
        uint8_t dir = server.arg("dir")[0] - '0';
        if (dir == 1) dir = 10;
        Serial.write(dir);
    } else if (animMode == BOX_OUTLINE) {
        uint8_t layer = server.arg("layer")[0] - '0';
        Serial.write(layer);
    }
    Serial.write(SERIAL_STOP_BYTE);
}