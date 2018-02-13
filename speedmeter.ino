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
#define USE_PDQ 0

#if USE_TFT
  #if USE_PDQ
    #include <PDQ_GFX.h>
    #include "PDQ_ST7735_config.h"
    #include <PDQ_ST7735.h>
  #else
    #include <Adafruit_GFX.h>
    #include "Adafruit_ST7735_config.h"
    #include <Adafruit_ST7735.h>
  #endif
#else
  #include "SO1602A.h"
#endif

#define FACTORYRESET_ENABLE      1
//#define MODE_LED_BEHAVIOUR          "MODE"

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ,BLUEFRUIT_SPI_RST);
#if USE_TFT
  #if USE_PDQ
    PDQ_ST7735 tft;
  #else
    Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
  #endif
#else
  SO1602A lcd(0x3c, 127);
#endif

int32_t speedmeterId;
double lastSpeed = 0;

void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

void showStatus(char status[]) {
  Serial.println(status);
  #if USE_TFT
    tft.fillRect(10, 10, 150, 12, ST7735_BLACK);
    tft.setCursor(10, 10);
    tft.setTextColor(ST7735_WHITE);
    tft.setTextWrap(true);
    tft.print(status);
  #else
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(status);
  #endif
}

void displaySpeed(double value) {
  #if USE_TFT
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
  #else
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(value, 0);
    lcd.print("km/h");
  #endif
}

double parseSpeed(uint8_t data[], uint16_t len) {
  Serial.write(data, len);
  Serial.println();
  
  StaticJsonBuffer<64> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(data);
  if(!root.success()) {
    Serial.println("parseObject() failed");
    return 0;
  }
  double speed = root["speed"];
  Serial.print("SPEED: ");
  Serial.print(speed, 0);
  Serial.println();
  return speed;
}

void readSpeed(int32_t charId, uint8_t data[], uint16_t len) {
  Serial.println("Read Characteristic");
   
  if(charId == speedmeterId) {
    double speed = parseSpeed(data, len);
    if(lastSpeed != speed) {
      displaySpeed(speed);
      lastSpeed = speed;
    }
  }
}

void readUart(char data[], uint16_t len) {
  Serial.println("Read UART");
  
  double speed = parseSpeed(data, len);
  if(lastSpeed != speed) {
    displaySpeed(speed);
  }
}

void connected() {
  showStatus("CONNECTED");
  displaySpeed(0);
}

void disconnected() {
  showStatus("WAITING...");
  displaySpeed(0);
}

void initDisplay() {
  #if USE_TFT
    #if USE_PDQ
      #if defined(ST7735_RST_PIN)
        FastPin<ST7735_RST_PIN>::setOutput();
        FastPin<ST7735_RST_PIN>::hi();
        FastPin<ST7735_RST_PIN>::lo();
        delay(1);
        FastPin(ST7735_RST_PIN>::hi();
      #endif
      tft.begin();
    #else
      tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab
    #endif
    tft.setRotation(1);
    tft.fillScreen(ST7735_BLACK);
  #else
    lcd.begin(16, 2, LCD_5x8DOTS);
    lcd.setCursor(0, 0);
    lcd.print("INITIALIZING...");
  #endif

}

void setup() {
  //while(!Serial);
  delay(500);
  Serial.begin(9600);
  Serial.println(F("Initializing Bluefruit LE Micro"));

  initDisplay();
  showStatus("INITIALIZING...");
 
  if(!ble.begin(VERBOSE_MODE)) {
    error(F("Couldn't find Bluefruit"));
  }
  Serial.println(F("OK"));
  
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
