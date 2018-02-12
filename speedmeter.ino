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



#define USE_TFT 1

#ifdef USE_TFT
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#else
#include "SO1602A.h"
#endif

#define FACTORYRESET_ENABLE      1
//#define MODE_LED_BEHAVIOUR          "MODE"

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ,BLUEFRUIT_SPI_RST);
#ifndef USE_TFT
SO1602A lcd(0x3c, 127);
#else
#define TFT_CS     10
#define TFT_RST    9  // you can also connect this to the Arduino reset
                      // in which case, set this #define pin to 0!
#define TFT_DC     8
#define TFT_SCLK 13   // set these to be whatever pins you like!
#define TFT_MOSI 11   // set these to be whatever pins you like!
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC,TFT_MOSI, TFT_SCLK,  TFT_RST);
#endif

int32_t speedmeterId;
double lastSpeed = 0;

void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

void showStatus(char status[]) {
  #ifndef USE_TFT
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(status);
  #else
  tft.fillRect(10, 10, 150, 12, ST7735_BLACK);
  tft.setCursor(10, 10);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextWrap(true);
  tft.print(status);
  #endif
}

void displaySpeed(double value) {
  #ifndef USE_TFT
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(value, 0);
  lcd.print("km/h");
  #else
  tft.fillRect(40, 40, 70, 32, ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextWrap(true);
  if(value >= 10 && value < 100) {
    tft.setCursor(64, 40);
  } else if(value >= 100) {
    tft.setCursor(40, 40);
  } else {
    tft.setCursor(88, 40);
  }
  tft.setTextSize(4);
  tft.print(value, 0);
  tft.setCursor(112, 60);
  tft.setTextSize(1);
  tft.print("km/h");
  #endif
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
    if(lastSpeed != speed) {
      displaySpeed(speed);
    }
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
  displaySpeed(speed);
  
}

void connected() {
  showStatus("CONNECTED");
  displaySpeed(0);
}

void disconnected() {
  showStatus("WAITING...");
}

void setup() {
  //while(!Serial);
  delay(500);
  Serial.begin(9600);
  Serial.println("Bluefruit LE Micro Battery Monitor");
  Serial.print(F("Initializing Bluefruit LE Micro"));

  #ifndef USE_TFT
  lcd.begin(16, 2, LCD_5x8DOTS);
  lcd.setCursor(0, 0);
  lcd.print("INITIALIZING...");
  #else
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);
  showStatus("INITIALIZING...");
  #endif
  
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

/*
  while(!ble.isConnected()) {
    delay(500);
  }
*/
  ble.sendCommandCheckOK("AT+GAPDEVNAME=Speedmeter");
  ble.sendCommandCheckOK(F("AT+GATTADDSERVICE=UUID128=6E-40-00-01-B5-A3-F3-93-E0-A9-E5-0E-24-DC-CA-9F"));
  ble.sendCommandWithIntReply(F("AT+GATTADDCHAR=UUID128=6E-40-00-02-B5-A3-F3-93-E0-A9-E5-0E-24-DC-CA-9F, PROPERTIES=0x08, MIN_LEN=2, MAX_LEN=20, DATATYPE=string, DESCRIPTION=Speed,VALUE={speed:0}"), &speedmeterId);
  ble.sendCommandCheckOK( "AT+GAPSETADVDATA=02-01-06-05-02-0d-18-0a-18" );
  ble.reset();
  ble.setConnectCallback(connected);
  ble.setDisconnectCallback(disconnected);
  ble.setBleGattRxCallback(speedmeterId, readSpeed);
  ble.setBleUartRxCallback(readUart);

  disconnected(); 
}

void loop() {
  ble.update(200);
}
