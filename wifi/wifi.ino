// #include <ESP8266WiFi.h>
// #include <WiFiClient.h>
// #include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <FS.h>
// #include <ESPAsyncTCP.h>
// #include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// communication to arduino
#define BAUDRATE 115200
#define SERIAL_CONFIG SERIAL_8N1
#define SERIAL_START_BYTE 0xFF
#define SERIAL_STOP_BYTE 0x00

// post server codes
#define POST_MODE_OFF '0'
#define POST_MODE_TEXT '1'
#define POST_MODE_ANIM '2'
#define POST_MODE_DATETIME '3'

#define POST_TEXTMODE_STATIC '1'
#define POST_TEXTMODE_SCROLL '2'
#define POST_TEXTMODE_SCROLL_IF_LONG '3'
#define POST_TEXTMODE_SCROLL_BOTH_LAYERS '4'
#define POST_TEXTMODE_STATIC_BOTH_LAYERS '5'

#define POST_ANIMMODE_WIPE '1'
#define POST_ANIMMODE_WIPE_DIAGONAL '2'
#define POST_ANIMMODE_SNAKE '3'
#define POST_ANIMMODE_BOX_OUTLINE '4'
#define POST_ANIMMODE_PACMAN '5'

#define POST_DIR_L_TO_R '1'
#define POST_DIR_R_TO_L '2'
#define POST_DIR_T_TO_B '3'
#define POST_DIR_B_TO_T '4'

#define POST_LAYER_FRONT '0'
#define POST_LAYER_BACK '1'
#define POST_LAYER_BOTH '2'

// serial codes
#define SERIAL_MODE_OFF 0
#define SERIAL_MODE_TEXT 1
#define SERIAL_MODE_ANIM 2
#define SERIAL_MODE_DATETIME 3
// text modes
#define SERIAL_TEXT_STATIC 1
#define SERIAL_TEXT_SCROLL 2
#define SERIAL_TEXT_SCROLL_IF_LONG 3
#define SERIAL_TEXT_SCROLL_BOTH_LAYERS 4
#define SERIAL_TEXT_STATIC_BOTH_LAYERS 5
// anim modes
#define SERIAL_SOLID 32
#define SERIAL_WIPE 33
#define SERIAL_WIPE_DIAGONAL 34
#define SERIAL_SNAKE 35
#define SERIAL_BOX_OUTLINE 36
// special animations
#define SERIAL_PACMAN 64

#define SERIAL_DIR_L_TO_R 0b000001
#define SERIAL_DIR_R_TO_L 0b000010
#define SERIAL_DIR_T_TO_B 0b000100
#define SERIAL_DIR_B_TO_T 0b001000

#define SERIAL_LAYER_FRONT 0
#define SERIAL_LAYER_BACK 1
#define SERIAL_LAYER_BOTH 2

#define PIN_MASTER_SLAVE_MODE D7
#define ARDUINO_MASTER 0
#define WIFI_MASTER 1

#define PIN_WIFI_ENABLED D6
#define WIFI_ENABLED 1
#define WIFI_DISABLED 0

#define PIN_WIFI_CONNECTED D5
#define WIFI_CONNECTED 1
#define WIFI_DISCONNECTED 0

// datetime
#define UTC_OFFSET_SECONDS 3600
#define DATETIME_UPDATE_MILLIS 10000

const char *ssid = "EE-de2brd_EXT";
const char *password = "golf-drift-key";

ESP8266WebServer server(80);
// AsyncWebServer server(80);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", UTC_OFFSET_SECONDS);

// default to datetime when power on
bool isDatetimeMode = true;
uint32_t lastDatetimeUpdate;
uint8_t lastDatetimeMins;

bool isMaster = false;

char text[64];

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

    server.on("/update", handleEffectsUpdate);

    server.begin();
    // Serial.println("Server started");

    // digitalWrite(PIN_WIFI_ENABLE, WIFI_ENABLED);

    digitalWrite(PIN_WIFI_CONNECTED, WIFI_CONNECTED);
    while (digitalRead(PIN_MASTER_SLAVE_MODE) != WIFI_MASTER) {
        delay(1);
    }
    isMaster = true;

    timeClient.begin();
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

    if (isDatetimeMode) {
        timeClient.update();
        if (timeClient.getMinutes() != lastDatetimeMins) {
            sendSerialDatetimePacket();
            lastDatetimeMins = timeClient.getMinutes();
        }
    }
}

void handleEffectsUpdate() {
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.write(SERIAL_START_BYTE);

    char mode = server.arg("mode")[0];

    if (mode == POST_MODE_OFF) {
        isDatetimeMode = false;
        Serial.write(SERIAL_MODE_OFF);
    } else if (mode == POST_MODE_TEXT) {
        sendUpdateText();
    } else if (mode == POST_MODE_ANIM) {
        sendUpdateAnim();
    } else if (mode == POST_MODE_DATETIME) {
        sendUpdateDatetime();
    }

    Serial.write(SERIAL_STOP_BYTE);
}

void sendUpdateText() {
    isDatetimeMode = false;
    Serial.write(SERIAL_MODE_TEXT);

    char textMode = server.arg("textmode")[0];
    switch (textMode) {
        case POST_TEXTMODE_STATIC:
            Serial.write(SERIAL_TEXT_STATIC);
            break;
        case POST_TEXTMODE_SCROLL:  
            Serial.write(SERIAL_TEXT_SCROLL);
            break;
        case POST_TEXTMODE_SCROLL_IF_LONG:
            Serial.write(SERIAL_TEXT_SCROLL_IF_LONG);
            break;
        case POST_TEXTMODE_SCROLL_BOTH_LAYERS:
            Serial.write(SERIAL_TEXT_SCROLL_BOTH_LAYERS);
            break;
        case POST_TEXTMODE_STATIC_BOTH_LAYERS:
            Serial.write(SERIAL_TEXT_STATIC_BOTH_LAYERS);
            break;
    }

    Serial.print(server.arg("text-input"));
}

void sendUpdateAnim() {
    isDatetimeMode = false;
    Serial.write(SERIAL_MODE_ANIM);

    char animMode = server.arg("animmode")[0];

    if (animMode == POST_ANIMMODE_PACMAN) {
        sendUpdatePacman();
    } else if (animMode == POST_ANIMMODE_WIPE) {
        sendUpdateWipe();
    } else if (animMode == POST_ANIMMODE_WIPE_DIAGONAL) {
        sendUpdateWipeDiagonal();
    } else if (animMode == POST_ANIMMODE_BOX_OUTLINE) {
        sendUpdateBoxOutline();
    }
}

void sendUpdatePacman() {
    Serial.write(SERIAL_PACMAN);
}

void sendUpdateWipe() {
    Serial.write(SERIAL_WIPE);

    char dir = server.arg("dir")[0];

    switch(dir) {
        case POST_DIR_L_TO_R:
            Serial.write(SERIAL_DIR_L_TO_R);
            break;
        case POST_DIR_R_TO_L:
            Serial.write(SERIAL_DIR_R_TO_L);
            break;
        case POST_DIR_T_TO_B:
            Serial.write(SERIAL_DIR_T_TO_B);
            break;
        case POST_DIR_B_TO_T:
            Serial.write(SERIAL_DIR_B_TO_T);
            break;
    }
}

void sendUpdateWipeDiagonal() {
    Serial.write(SERIAL_WIPE_DIAGONAL);

    char dirH = server.arg("dirH")[0];
    char dirV = server.arg("dirV")[0];

    uint8_t diagonalDir = 0;

    if (dirH == POST_DIR_L_TO_R) {
        diagonalDir |= SERIAL_DIR_L_TO_R;
    } else {
        diagonalDir |= SERIAL_DIR_R_TO_L;
    }

    if (dirV == POST_DIR_T_TO_B) {
        diagonalDir |= SERIAL_DIR_T_TO_B;
    } else {
        diagonalDir |= SERIAL_DIR_B_TO_T;
    }

    Serial.write(diagonalDir);
}

void sendUpdateBoxOutline() {
    Serial.write(SERIAL_BOX_OUTLINE);

    char layer = server.arg("layer")[0];

    switch (layer) {
        case POST_LAYER_FRONT:
            Serial.write(SERIAL_LAYER_FRONT);
            break;
        case POST_LAYER_BACK:
            Serial.write(SERIAL_LAYER_BACK);
            break;
        case POST_LAYER_BOTH:
            Serial.write(SERIAL_LAYER_BOTH);
            break;
    }
}

void sendUpdateDatetime() {
    isDatetimeMode = true;
    timeClient.update();
    Serial.write(SERIAL_MODE_DATETIME);
    Serial.write(timeClient.getHours());
    Serial.write(timeClient.getMinutes());
    Serial.write(timeClient.getDay());
}

// hours, mins, day of week
void sendSerialDatetimePacket() {
    Serial.write(SERIAL_START_BYTE);
    Serial.write(SERIAL_MODE_DATETIME);
    Serial.write(timeClient.getHours());
    Serial.write(timeClient.getMinutes());
    Serial.write(timeClient.getDay());
    Serial.write(SERIAL_STOP_BYTE);
}