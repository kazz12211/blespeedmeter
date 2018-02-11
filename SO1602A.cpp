#include "SO1602A.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <Wire.h>

// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set: 
//    DL = 1; 8-bit interface data 
//    N = 0; 1-line display 
//    F = 0; 5x8 dot character font 
// 3. Display on/off control: 
//    D = 0; Display off 
//    C = 0; Cursor off 
//    B = 0; Blinking off 
// 4. Entry mode set: 
//    I/D = 1; Increment by 1 
//    S = 0; No shift 
//
// Note, however, that resetting the Arduino doesn't reset the LCD, so we
// can't assume that its in that state when a sketch starts (and the
// SO1602A constructor is called).

/// This library has been modified for I2C based LCD by Straberry Linux in Japan.
// The controller is ST7032i.
//  21 Aug. 2012, Noriaki Mitsunaga

const static uint8_t I2C_ADDR_AKIZUKI = 0x50;
const static uint8_t I2C_ADDR_GROVE_RGB = 0x3e;
const static uint8_t I2C_ADDR_STRAWBERRY = 0x3e;
const static uint8_t LCD_UNDEF = 0;

SO1602A::SO1602A(uint8_t addr, uint8_t contrast)
{
  I2C_ADDR = addr; // Akizuki denshi (OLED, SO1602A series)
  I2C_RS = 0x40;
  _contrast = contrast;
  lcd_type = LCD_UNDEF;
  wire = &Wire;
  init();
}

void SO1602A::init()
{
  _displayfunction = 0;
  //  begin(16, 1);  
}

void SO1602A::setWire(TwoWire *w)
{
  wire = w;
}

void SO1602A::begin(uint8_t cols, uint8_t lines, uint8_t dotsize) {
  if (lines > 1) {
    _displayfunction |= LCD_2LINE;
  }
  _numlines = lines;
  _currline = 0;

  // for some 1 line displays you can select a 10 pixel high font
  if ((dotsize != 0) && (lines == 1)) {
    _displayfunction |= LCD_5x10DOTS;
  }

  wire->begin();

  // SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
  // according to datasheet, we need at least 40ms after power rises above 2.7V
  // before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
  delay(50); 
 
    delay(100);
    command(0x01); // clear display
    delay(20);
    command(0x02); // return home
    delay(2);
    command(0x0f); // display on
    delay(2);
    command(0x01); // clear display
    delay(20);

    command(0x2a);
    command(0x79);
    command(0x81);
    command(_contrast);
    command(0x78);
    delay(100);

  // Function set (set so as to be compatible with parallel LCD)
  command(0b00110000 | _displayfunction);  

  // turn the display on with no cursor or blinking default
  _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;  
  display();

  // clear it off
  clear();

  // Initialize to default text direction (for romance languages)
  _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
  // set the entry mode
  command(LCD_ENTRYMODESET | _displaymode);

}

/********** high level commands, for the user! */
void SO1602A::clear()
{
  command(LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
  delay(20);  // this command takes a long time!
}

void SO1602A::home()
{
  command(LCD_RETURNHOME);  // set cursor position to zero
  delay(20);  // this command takes a long time!
}

void SO1602A::setCursor(uint8_t col, uint8_t row)
{
  if (I2C_ADDR == I2C_ADDR_AKIZUKI || I2C_ADDR == I2C_ADDR_STRAWBERRY) {
    int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
    if ( row >= _numlines ) {
      row = _numlines-1;    // we count rows starting w/0
    }
  
    command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
  } else {
    int row_offsets[] = {0x00, 0x20};

    command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
  }
}

// Turn the display on/off (quickly)
void SO1602A::noDisplay() {
  _displaycontrol &= ~LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void SO1602A::display() {
  _displaycontrol |= LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turns the underline cursor on/off
void SO1602A::noCursor() {
  _displaycontrol &= ~LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void SO1602A::cursor() {
  _displaycontrol |= LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turn on and off the blinking cursor
void SO1602A::noBlink() {
  _displaycontrol &= ~LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void SO1602A::blink() {
  _displaycontrol |= LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// These commands scroll the display without changing the RAM
void SO1602A::scrollDisplayLeft(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void SO1602A::scrollDisplayRight(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void SO1602A::leftToRight(void) {
  _displaymode |= LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This is for text that flows Right to Left
void SO1602A::rightToLeft(void) {
  _displaymode &= ~LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'right justify' text from the cursor
void SO1602A::autoscroll(void) {
  _displaymode |= LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'left justify' text from the cursor
void SO1602A::noAutoscroll(void) {
  _displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void SO1602A::createChar(uint8_t location, uint8_t charmap[]) {
  location &= 0x7; // we only have 8 locations 0-7
  command(LCD_SETCGRAMADDR | (location << 3));
  for (int i=0; i<8; i++) {
    write(charmap[i]);
  }
}

/*********** mid level commands, for sending data/cmds */

inline void SO1602A::command(uint8_t value) {
  send(value, LOW);
}

inline size_t SO1602A::write(uint8_t value) {
  send(value, HIGH);
  return 1; // assume sucess
}

/************ low level data pushing commands **********/

// write either command or data, with automatic 4/8-bit selection
void SO1602A::send(uint8_t value, uint8_t mode) {

  wire->beginTransmission(I2C_ADDR);
  if (mode == LOW)
    wire->write((uint8_t)0x0);   // Co = 0, RS = 0    // Co: continue
  else
    wire->write(I2C_RS);  // Co = 0, RS = 1
  wire->write(value);
  wire->endTransmission();
}


