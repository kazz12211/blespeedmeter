#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
  #include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"

#define FACTORYRESET_ENABLE      1
//#define MODE_LED_BEHAVIOUR          "MODE"

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ,BLUEFRUIT_SPI_RST);

int32_t speedmeterId;

void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

void readSpeed(int32_t charId, uint8_t data[], uint16_t len) {
  Serial.println("Read Characteristic");
  Serial.write(data, len);
  Serial.println();
  
  if(charId == speedmeterId) {
   StaticJsonBuffer<256> jsonBuffer;
   JsonObject& root = jsonBuffer.parseObject(data);
    if(!root.success()) {
      Serial.println("readSpeed: parseObject() failed");
    }
    double speed = root["speed"];
    Serial.print("SPEED: ");
    Serial.print(speed, 0);
    Serial.println("km/h");
  }
}

void readUart(char data[], uint16_t len) {
  Serial.println("Read UART");
  Serial.write(data, len);
  Serial.println();
  
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(data);
  if(!root.success()) {
    Serial.println("readUart: parseObject() failed");
  }
  double speed = root["speed"];
  Serial.print("SPEED: ");
  Serial.print(speed, 0);
  Serial.println("km/h");
  
}

void setup() {
  while(!Serial);
  delay(500);
  Serial.begin(9600);
  Serial.println("Bluefruit LE Micro Battery Monitor");
  Serial.print(F("Initializing Bluefruit LE Micro"));
  if(!ble.begin(VERBOSE_MODE)) {
    error(F("Couldn't find Bluefruit"));
  }
  Serial.print(F("OK"));
  
 if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
      error(F("Couldn't factory reset"));
    }
  }
  ble.echo(false);
  ble.info();
  ble.verbose(false);

  while(!ble.isConnected()) {
    delay(500);
  }

  ble.sendCommandCheckOK("AT+GAPDEVNAME=Speedmeter");
  ble.sendCommandCheckOK(F("AT+GATTADDSERVICE=UUID128=6E-40-00-01-B5-A3-F3-93-E0-A9-E5-0E-24-DC-CA-9F"));
  ble.sendCommandWithIntReply(F("AT+GATTADDCHAR=UUID128=6E-40-00-02-B5-A3-F3-93-E0-A9-E5-0E-24-DC-CA-9F, PROPERTIES=0x08, MIN_LEN=2, MAX_LEN=20, DATATYPE=string, DESCRIPTION=Speed,VALUE={speed:0}"), &speedmeterId);
  ble.sendCommandCheckOK( "AT+GAPSETADVDATA=02-01-06-05-02-0d-18-0a-18" );
  ble.reset();
  ble.setBleGattRxCallback(speedmeterId, readSpeed);
  ble.setBleUartRxCallback(readUart);
}

void loop() {
  ble.update(200);
}
