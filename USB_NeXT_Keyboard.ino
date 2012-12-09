// NeXT non-ADB Keyboard to USB converter
// This will take an older NeXT keyboard, talk to it, and turn the keycodes into a USB keyboard
// Requires an Arduino Micro for the USB portion - but could be ported to another micro fairly easily
// Written by Limor Fried / Adafruit Industries
// Released under BSD license - thanks NetBSD! :)
//
// Timing reference thanks to http://m0115.web.fc2.com/
// Pinouts thanks to http://www.68k.org/~degs/nextkeyboard.html
// Keycodes from http://ftp.netbsd.org/pub/NetBSD/NetBSD-release-6/src/sys/arch/next68k/dev/

#include "wsksymdef.h"
#include "nextkeyboard.h"

// the timing per bit, 50microseconds
#define TIMING 50

// pick which pins you want to use
#define KEYBOARDOUT 3
#define KEYBOARDIN 2

// comment to speed things up, uncomment for help!
//#define DEBUG 

// speed up reads by caching the 'raw' pin ports
volatile uint8_t *misoportreg;
uint8_t misopin;
// our little macro
#define readkbd() ((*misoportreg) & misopin)

// debugging/activity LED
#define LED 13

// special command for setting LEDs
void setLEDs(bool leftLED, bool rightLED) {
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING *9);
  digitalWrite(KEYBOARDOUT, HIGH);  
  delayMicroseconds(TIMING *3);
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING);

  if (leftLED)
      digitalWrite(KEYBOARDOUT, HIGH);
  else 
      digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING);

  if (rightLED)
      digitalWrite(KEYBOARDOUT, HIGH);
  else 
      digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING);
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING *7);
  digitalWrite(KEYBOARDOUT, HIGH);
}

void query() {
  // query the keyboard for data
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING *5);
  digitalWrite(KEYBOARDOUT, HIGH);  
  delayMicroseconds(TIMING );  
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING *3);
  digitalWrite(KEYBOARDOUT, HIGH); 
}

void nextreset() {
  // reset the keyboard
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING);
  digitalWrite(KEYBOARDOUT, HIGH);  
  delayMicroseconds(TIMING*4);  
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING);
  digitalWrite(KEYBOARDOUT, HIGH);
  delayMicroseconds(TIMING*6);
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING*10);  
  digitalWrite(KEYBOARDOUT, HIGH);
}


void setup() {
  // set up pin directions
  pinMode(KEYBOARDOUT, OUTPUT);
  pinMode(KEYBOARDIN, INPUT);
  pinMode(LED, OUTPUT);
  
  misoportreg = portInputRegister(digitalPinToPort(KEYBOARDIN));
  misopin = digitalPinToBitMask(KEYBOARDIN);
  
  // according to http://cfile7.uf.tistory.com/image/14448E464F410BF22380BB
  query();
  delay(5);
  nextreset();
  delay(8);
  
  query();
  delay(5);
  nextreset();
  delay(8);
  
  Keyboard.begin();

#ifdef DEBUG
  while (!Serial)
  Serial.begin(57600);
  Serial.println("NeXT");
#endif
}

uint32_t getresponse() {
  // bitbang timing, read 22 bits 50 microseconds apart
  cli();
  while ( readkbd() );
  delayMicroseconds(TIMING/2);
  uint32_t data = 0;
  for (uint8_t i=0; i < 22; i++) {
      if (readkbd())
        data |= ((uint32_t)1 << i);
      delayMicroseconds(TIMING);
  }
  sei();
  return data;

}

void loop() {
  digitalWrite(LED, LOW);
  delay(20);
  uint32_t resp;
  query();
  resp = getresponse();

  // check for a 'idle' response, we'll do nothing
  if (resp == 0x200600) return;
  
  // turn on the LED when we get real resposes!
  digitalWrite(LED, HIGH);

  // keycode is the lower 7 bits
  uint8_t keycode = resp & 0xFF;
  keycode /= 2;
  
#ifdef DEBUG
  Serial.print('['); Serial.print(resp, HEX);  Serial.print("] ");
  Serial.print("keycode: "); Serial.print(keycode);
#endif

  if (keycode == 0) { 
    // modifiers! you can remap these here, 
    // but I suggest doing it in the OS instead
    if (resp & 0x1000)
      Keyboard.press(KEY_LEFT_GUI);
    else 
      Keyboard.release(KEY_LEFT_GUI);

    if (resp & 0x2000) {
      Keyboard.press(KEY_LEFT_SHIFT);
    } else { 
      Keyboard.release(KEY_LEFT_SHIFT);
    }
    if (resp & 0x4000) {
      Keyboard.press(KEY_RIGHT_SHIFT);
    } else {
      Keyboard.release(KEY_RIGHT_SHIFT);
    }
    // turn on shift LEDs if shift is held down
    if (resp & 0x6000)
      setLEDs(true, true);
    else
      setLEDs(false, false);
      
    if (resp & 0x8000)
      Keyboard.press(KEY_LEFT_CTRL);
    else 
      Keyboard.release(KEY_LEFT_CTRL);
      
    if (resp & 0x10000)
      Keyboard.press(KEY_RIGHT_CTRL);
    else 
      Keyboard.release(KEY_RIGHT_CTRL);

    if (resp & 0x20000)
      Keyboard.press(KEY_LEFT_ALT);
    else 
      Keyboard.release(KEY_LEFT_ALT);
    if (resp & 0x40000)
      Keyboard.press(KEY_RIGHT_ALT);
    else 
      Keyboard.release(KEY_RIGHT_ALT);

    return;
  }
  
  for (int i = 0; i< 100; i++) {
    if (nextkbd_keydesc_us[i*3] == keycode) {
      char ascii = nextkbd_keydesc_us[i*3+1];

#ifdef DEBUG
      Serial.print("--> ");      Serial.print(ascii);
#endif

      int code;
      switch (keycode) {
        case 73: code = KEY_ESC; break;
        case 13: code = KEY_RETURN; break;
        case 42: code = KEY_RETURN; break;
        case 27: code = KEY_BACKSPACE; break;
        case 22: code = KEY_UP_ARROW; break;
        case 15: code = KEY_DOWN_ARROW; break;
        case 16: code = KEY_RIGHT_ARROW; break;
        case 9: code = KEY_LEFT_ARROW; break;
        // remap the 'lower volume' key to Delete (its where youd expect it)
        case 2: code = KEY_DELETE; break;
        
        default: code = ascii;
      }
      if ((resp & 0xF00) == 0x400) {  // down press
#ifdef DEBUG
        Serial.println(" v ");
#endif
        Keyboard.press(code);
        break;
      }
      if ((resp & 0xF00) == 0x500) {
        Keyboard.release(code);
#ifdef DEBUG
        Serial.println(" ^ ");
#endif
        break;
      }
    }
  }
}

