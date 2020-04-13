uint8_t inByte = 0;

void setup() {
    pinMode(6, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(9600, SERIAL_8N1);

    while (!Serial) {

    }
}

void loop() {

    Serial.write(2);
    // digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    Serial.write(1);
    // digitalWrite(LED_BUILTIN, LOW);
    delay(1000);

    while (Serial.available() > 0) {
        inByte = Serial.read();

        // if (inByte == 2) {
        //     digitalWrite(LED_BUILTIN, HIGH);
        // } else if (inByte == 1) {
        //     digitalWrite(LED_BUILTIN, LOW);
        // }

        digitalWrite(LED_BUILTIN, HIGH);

        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
    }
}