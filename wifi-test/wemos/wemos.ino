// #include <SoftwareSerial.h>

// SoftwareSerial swSerial(D1, D2); // RX, TX

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    pinMode(D1, OUTPUT);
    pinMode(D7, INPUT);
    pinMode(D8, OUTPUT);

    Serial.begin(9600, SERIAL_8N1);
    Serial.swap();
    delay(500);
    Serial.set_tx(1);

    while (!Serial) {
        digitalWrite(LED_BUILTIN, HIGH);

        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
    }

    // swSerial.begin(115200);

}

void loop() {
    // Serial.write(2);
    // digitalWrite(LED_BUILTIN, LOW);
    // delay(1000);
    // Serial.write(1);
    // digitalWrite(LED_BUILTIN, HIGH);
    // delay(1000);
    // uint8_t value = digitalRead(D0);

    // digitalWrite(D3, value);

    digitalWrite(D1, LOW);
    delay(100);
    digitalWrite(D1, HIGH);
    delay(200);

    while (Serial.available() > 0) {
        uint8_t in = Serial.read();

        Serial.write(in);

        // if (in == 2) {
        //     digitalWrite(D1, HIGH);
        // } else if (in == 1) {
        //     digitalWrite(D1, LOW);
        // }
        digitalWrite(LED_BUILTIN, LOW);

        delay(100);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
    }
}