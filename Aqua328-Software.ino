#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "musicnotes.h"

// 1602 display
LiquidCrystal_I2C lcd(0x3F,16,2);  // set the LCD address to 0x3F for a 16 chars and 2 line display

//  Some custom glyphs for the display
static byte onGlyph[8] = { 0b00100,0b01110,0b11111,0b10101,0b00100,0b00100,0b00100,0b00100 };
static byte offGlyph[8] = { 0b00100,0b00100,0b00100,0b00100,0b10101,0b11111,0b01110,0b00100 };
static byte degreesGlyph[8] = { 0b00110,0b01001,0b01001,0b00110,0b00000,0b00000,0b00000,0b00000 };

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 9

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Assign address manually. 
DeviceAddress tankThermometer  = { 0x10, 0x38, 0x4E, 0x2A, 0x03, 0x08, 0x00, 0x05 };

// Sound Setup
// change this to make the sounds slower or faster
int tempo = 180;
// change this to whichever pin you want to use
int buzzer = 10;

// Switches
#define LIDSWITCH 7 // digital pin D7
#define USERSWITCH 8 // digital pin  

// Lights
#define RED 5   // Red LED PWM on Digital 5
#define GREEN 6 // Red LED PWM on Digital 6
#define BLUE 3  // Red LED PWM on Digital 3

byte redVal = 0;   // current red value
byte greenVal = 0; // current green value
byte blueVal = 0;  // current blue value

int fastFadeRate = 30; // Rate (millis per step) for maintenance on/off cycle
int slowFadeRate = 1500; // Rate (millis per step) for day on/off cycle
bool serialTemps = false; // Show temperature on serial port
 

// Fan
#define FAN 9 // Fan PWM on Digital 9

int timeScale; // Used to scale time readings when we adjust Timer0 multiplier

void setup()
{
  // Serial
  Serial.begin(19200);
  
  // OneWire temp sensors
  sensors.begin();

  // General IO Pins
  pinMode(LIDSWITCH,INPUT_PULLUP);  // Lid switch to input & pull-up
  pinMode(USERSWITCH,INPUT_PULLUP); // User button to input & pull-up
  pinMode(RED,OUTPUT);              // PWM, RED led channel
  pinMode(GREEN,OUTPUT);            // PWM, GREEN led channel
  pinMode(BLUE,OUTPUT);             // PWM, BLUE led channel
  pinMode(FAN,OUTPUT);              // PWM, Fan
  analogWrite(RED,redVal);          // Set default
  analogWrite(GREEN,greenVal);      // Set default
  analogWrite(BLUE,blueVal);        // Set default
  analogWrite(FAN,125);             // Defaulted to 50%
  
    // PWM Prescaler to set PWM base frequency; avoids fan noise (whine) and make LED's smooth
  TCCR0B = TCCR0B & B11111000 | B00000010; // Timer0 (D5,D6) ~7.8 KHz
  timeScale = 8; // Adjusting Timer0 makes millis() and delay() run faster.
  TCCR1B = TCCR1B & B11111000 | B00000001; // Timer1 (D9,D10) ~31.4 KHz
  TCCR2B = TCCR2B & B11111000 | B00000001; // Timer2 (D3,D11) ~31.4 KHz

  // LCD
  lcd.init(); // Start lcd up
  lcd.noCursor(); // do not display cursor
  lcd.backlight(); // light up
  lcd.createChar(0, onGlyph);
  lcd.createChar(1, offGlyph);
  lcd.createChar(2, degreesGlyph); 
  lcd.setCursor(0,0); 
  lcd.print("Aqua328");    // welcome text.
  lcd.setCursor(0,1);
  lcd.print(F("Off")); // Start 'Off'


  // Greetings on serial port
  Serial.println("Welcome to Aqua328");
  Serial.println("https://github.com/easytarget/Aqua328-Software");

  sensors.requestTemperatures(); // Send initial command to get temperatures
  // pause on splash screen while temps being gathered
  mydelay(3000);
}

void mydelay(unsigned long d) {
  delay(d*timeScale);
}

unsigned long mymillis() {
  return (millis()/timeScale);
}

char getpress(int waitTime) {
  unsigned long start = mymillis();
  while(!Serial.available() && (start+waitTime) <= mymillis()) {
    mydelay(10);
  }
  char c = Serial.read();
  return c;
}

void lightsontune() {
  // Adapted from the 'Nokia Ringtone' sketch found at:
  // https://github.com/robsoncouto/arduino-songs
  int melody[] = {
    // Nokia Ringtone 
    // Score available at https://musescore.com/user/29944637/scores/5266155
    NOTE_E5, 8, NOTE_D5, 8, NOTE_FS4, 4, NOTE_GS4, 4, 
    NOTE_CS5, 8, NOTE_B4, 8, NOTE_D4, 4, NOTE_E4, 4, 
    NOTE_B4, 8, NOTE_A4, 8, NOTE_CS4, 4, NOTE_E4, 4,
    NOTE_A4, 2, 
  };
  int notes = sizeof(melody) / sizeof(melody[0]) / 2;
  int wholenote = (60000 * 4) / tempo;
  int divider = 0, noteDuration = 0;

  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {
    divider = melody[thisNote + 1];
    if (divider > 0) {
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; // increases the duration in half for dotted notes
    }
    // we only play the note for 90% of the duration, leaving 10% as a pause
    tone(buzzer, melody[thisNote], noteDuration * 0.9);
    mydelay(noteDuration);
    noTone(buzzer);
    lcd.print('.'); //animate on the start screen
  }

}
void lightsofftune() {
  // Adapted from the 'Star trek Intro' sketch found at:
  // https://github.com/robsoncouto/arduino-songs
  int melody[] = {
  // Star Trek Intro
  // Score available at https://musescore.com/user/10768291/scores/4594271
    NOTE_D4, -8, NOTE_G4, 16, NOTE_C5, -4, 
    NOTE_B4, 8, NOTE_G4, -16, NOTE_E4, -16, NOTE_A4, -16,
    NOTE_D5, 2,
  };
  int notes = sizeof(melody) / sizeof(melody[0]) / 2;
  int wholenote = (60000 * 4) / tempo;
  int divider = 0, noteDuration = 0;

  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {
    divider = melody[thisNote + 1];
    if (divider > 0) {
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; // increases the duration in half for dotted notes
    }
    // we only play the note for 90% of the duration, leaving 10% as a pause
    tone(buzzer, melody[thisNote], noteDuration * 0.9);
    mydelay(noteDuration);
    noTone(buzzer);
    lcd.print('.'); //animate on the start screen
  }
}

void cycleOn(int del) {
  Serial.println(F("Coming On: "));  
  lcd.setCursor(0,1);
  lcd.write(byte(0)); // 'up' sysmbol we defined in setup()
  lcd.print(F("     "));
  Serial.println(F("Red: "));
  for (int a = 0; a <= 255; a++) {
    analogWrite(RED,a);
    redVal=a;
    mydelay(del);
    showTemp();
  }
  lcd.setCursor(1,1);
  lcd.write(byte(0)); // 'up' sysmbol we defined in setup()
  Serial.println(F("Green: "));
  for (int a = 0; a <= 255; a++) {
    analogWrite(GREEN,a);
    greenVal=a;
    mydelay(del);
    showTemp();
  }
  lcd.setCursor(2,1);
  lcd.write(byte(0)); // 'up' sysmbol we defined in setup()
  Serial.println(F("Blue: "));
  for (int a = 0; a <= 255; a++) {
    analogWrite(BLUE,a);
    blueVal=a;
    mydelay(del);
    showTemp();
  }
  Serial.println(F("On!"));
  lcd.setCursor(0,1);
  lcd.print(F("On      "));
}

void cycleOff(int del) {
  Serial.println(F("Going Off: "));  
  lcd.setCursor(0,1);
  lcd.write(byte(1)); // 'down' sysmbol we defined in setup()
  lcd.print(F("     "));
  Serial.println(F("Red: "));
  for (int a = 255; a >= 0; a--) {
    analogWrite(RED,a);
    redVal=a;
    mydelay(del);
    showTemp();
  }
  lcd.setCursor(1,1);
  lcd.write(byte(1)); // 'down' sysmbol we defined in setup()
  Serial.println(F("Green: "));
  for (int a = 255; a >= 0; a--) {
    analogWrite(GREEN,a);
    greenVal=a;
    mydelay(del);
    showTemp();
  }
  lcd.setCursor(2,1);
  lcd.write(byte(1)); // 'down' sysmbol we defined in setup()
  Serial.println(F("Blue: "));
  for (int a = 255; a >= 0; a--) {
    analogWrite(BLUE,a);
    mydelay(del);
    blueVal=a;
    showTemp();
  }
  Serial.println(F("Off!"));
  lcd.setCursor(0,1);
  lcd.print(F("Off     "));
}

void showTemp() {
  // Get the current reading
  float tankTemp = sensors.getTempC(tankThermometer);
  // Request a new temperature reading
  sensors.requestTemperatures(); 

  // Display the results
  lcd.setCursor(11,1);
  if (tankTemp != -127) {
    lcd.print(tankTemp,1);
    lcd.write(byte(2));    // 'degrees' sysmbol we defined in setup()
    lcd.print(F("  "));
  } else {
    lcd.print(F("Error"));
  }

  if (serialTemps) {
    // Put a loop here to write this once per minute
    Serial.print(F("Tank: "));
    Serial.print(tankTemp);
    Serial.print(char(194));
    Serial.print(char(176));
    Serial.print(F("C: Time: "));
    Serial.print((float)mymillis()/1000,2);
    Serial.print(F(": R: "));
    Serial.print(redVal);
    Serial.print(F(": G: "));
    Serial.print(greenVal);
    Serial.print(F(": B: "));
    Serial.print(blueVal);
    Serial.println();
  }  
}

// Main Loop

int lightStatus = 0; // 0=off, 1=On
bool buttonHit = false; // set to true when the button is hit

void loop()
{  
  lcd.setCursor(0,0);
  if (!digitalRead(USERSWITCH)) { // switches are inverted..
    lcd.print(F("Button! "));
    Serial.print(F("Button: ")); 
    buttonHit = true; 
    }
  else if (!digitalRead(LIDSWITCH)) { // switches are inverted..
    lcd.print(F("Lid! "));
    Serial.print(F("Lid: "));
    }
  else {
    // Restore display and check to see if the button has been pressed
    lcd.print(F("Aqua328         "));
    if (buttonHit) {
      buttonHit=false;
      if (lightStatus == 0) {
        // turn on
        cycleOn(slowFadeRate);
        lightStatus = 1;
      } else {
        // turn off 
        cycleOff(slowFadeRate);
        lightStatus = 0;
      }
    }
  }

  // Hold for one second here, processing serial and buttons
  char ret = getpress(500);

  // Show the water temperature.
  showTemp();
  
  // And loop back to the button/temp display
}
