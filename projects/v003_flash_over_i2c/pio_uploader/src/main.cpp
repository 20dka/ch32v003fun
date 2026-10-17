#include <Arduino.h>

#include <Wire.h>

#define SDA_PIN D1
#define SCL_PIN D2
const int16_t I2C_SLAVE = 69;

void i2c_send(uint8_t addr, uint8_t offset, uint8_t *buf, uint8_t len)
{
    Wire.beginTransmission(addr);
    Wire.write(offset);
    Wire.write(buf, len);
    Wire.endTransmission();
}

bool i2c_receive(uint8_t addr, uint8_t offset, uint8_t *buf, uint8_t len)
{
    //printf("reading register #%d\n", offset);
    Wire.beginTransmission(addr);
    Wire.write(offset);
    Wire.endTransmission();
    Wire.requestFrom(addr, len);
    Wire.endTransmission();
    return Wire.readBytes(buf, len) == len;
}

void setup() {
    Serial.begin(9600);
    delay(2000); // if the wire library is initialized too soon, it corrupts listening slaves
    Wire.begin(SDA_PIN, SCL_PIN);

}

void loop() {

    uint8_t buf[] = {0xDE, 0xAD, 0xBE, 0xEF};

    i2c_send(0x69, 0xFC, buf, 4);
    printf("\n");
    delay(1000);
}
