#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// communication to arduino
#define BAUDRATE 115200
#define SERIAL_CONFIG SERIAL_8N1

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

bool isMaster = false;

const PROGMEM char indexHTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
    <head>
        <title>Hello world</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
    </head>
    <body>
        <form action="/">
            Display text: <input type="text" name="text">
            <input type="submit" value="Submit">
        </form>
    </body>
</html>
)rawliteral";

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

    server.on("/", handleRoot);

    server.begin();
    // Serial.println("Server started");

    // digitalWrite(PIN_WIFI_ENABLE, WIFI_ENABLED);

    digitalWrite(PIN_WIFI_CONNECTED, WIFI_CONNECTED);
    while (digitalRead(PIN_MASTER_SLAVE_MODE) != WIFI_MASTER) {
        delay(1);
    }
    isMaster = true;

    // Serial.print("A");
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
    if (server.args() > 0) {
        if (server.arg(ARG_TEXT) && isMaster) {
            Serial.print(server.arg(ARG_TEXT));
        }
    }

    server.send(200, "text/html", indexHTML);
    int ledState = 0;
    for (int i = 0; i < 10; i++) {
        digitalWrite(LED_BUILTIN, ledState);
        ledState = !ledState;
        delay(100);
    }
}