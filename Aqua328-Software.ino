#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "musicnotes.h"

LiquidCrystal_I2C lcd(0x3F,16,2);  // set the LCD address to 0x3F for a 16 chars and 2 line display

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 9

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
//DeviceAddress insideThermometer, outsideThermometer;

// Assign address manually. The addresses below will need to be changed
// to valid device addresses on your bus. Device address can be retrieved
// by using either oneWire.search(deviceAddress) or individually via
// sensors.getAddress(deviceAddress, index)
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

// Fan
#define FAN 9 // Fan PWM on Digital 9

void setup()
{
  // Serial first
  Serial.begin(19200);
  // OneWire temp sensors
  sensors.begin();

  // Turn Speaker Tansistor OFF.

  // General IO Pins
  pinMode(LIDSWITCH,INPUT_PULLUP);  // Lid switch to input & pull-up
  pinMode(USERSWITCH,INPUT_PULLUP); // User button to input & pull-up
  pinMode(RED,OUTPUT);              // PWM, RED led channel
  pinMode(GREEN,OUTPUT);            // PWM, GREEN led channel
  pinMode(BLUE,OUTPUT);             // PWM, BLUE led channel
  pinMode(FAN,OUTPUT);              // PWM, Fan
  analogWrite(RED,0);               // Ensure defaulted to Off
  analogWrite(GREEN,0);             // Ensure defaulted to Off
  analogWrite(BLUE,0);              // Ensure defaulted to Off
  analogWrite(FAN,0);               // Ensure defaulted to Off

  lcd.init(); // Start lcd up
  lcd.noCursor(); // do not display cursor
  lcd.backlight(); // light up
  lcd.setCursor(0,0); // home
  lcd.print("Aqua328"); // welcome text.
  lcd.setCursor(1,1); // home
//  lcd.noBacklight();
  Serial.println("Welcome to Aqua328");
  Serial.println("https://github.com/easytarget/Aqua328-Software");

  sensors.requestTemperatures(); // Send initial command to get temperatures
  // pause on splash screen while temps being gathered
  delay(3000);
}

char getpress() {
  // Serial.print("."); // debug
  unsigned long start = millis();
  while(!Serial.available() && (start+1000) <= millis()) {
    delay(10);
  }
  char c = Serial.read();
  //Serial.print("'");
  //Serial.print(c);
  //Serial.println("' received.");
  // 
  //delay(10); 
  //while(Serial.available())
  //{
  //  Serial.read();
  //  delay(10);
  //}
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
    delay(noteDuration);
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
    delay(noteDuration);
    noTone(buzzer);
    lcd.print('.'); //animate on the start screen
  }
}

void cycleOn() {
  Serial.println(F("Coming On:"));  
  lcd.setCursor(0,1);
  lcd.print(F("Starting"));
  Serial.print(F("Red"));
  for (int a = 0; a <= 255; a++) {
    analogWrite(RED,a);
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.print(F("Green"));
  for (int a = 0; a <= 255; a++) {
    analogWrite(GREEN,a);
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.print(F("Blue"));
  for (int a = 0; a <= 255; a++) {
    analogWrite(BLUE,a);
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.println(F("On!"));
  lcd.setCursor(0,1);
  lcd.print(F("On      "));
}

void cycleOff() {
  Serial.println(F("Going Off:"));  
  lcd.setCursor(0,1);
  lcd.print(F("Stopping"));
  Serial.print(F("Red"));
  for (int a = 255; a >= 0; a--) {
    analogWrite(RED,a);
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.print(F("Green"));
  for (int a = 255; a >= 0; a--) {
    analogWrite(GREEN,a);
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.print(F("Blue"));
  for (int a = 255; a >= 0; a--) {
    analogWrite(BLUE,a);
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.println(F("Off!"));
  lcd.setCursor(0,1);
  lcd.print(F("Off     "));
}

int lightStatus = 0; // 0=off, 1=On
bool buttonHit = false; // set to true when the button is hit

void loop()
{  
  // Request a temperature reading
  sensors.requestTemperatures(); // Send initial command to get temperatures
  // Hold for one second here, processing serial and buttons
  char ret = getpress();
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
        cycleOn();
        lightStatus = 1;
      } else {
        // turn off 
        cycleOff();
        lightStatus = 0;
      }
    }
  }

  float tankTemp = sensors.getTempC(tankThermometer);
  lcd.setCursor(11,1);
  lcd.print(tankTemp,1);
  lcd.print((char)223);

  Serial.print(F("Tank: "));
  Serial.print(tankTemp);
  Serial.print(char(194));
  Serial.print(char(176));
  Serial.println(F("C"));

  // And loop back to the button/temp display
}
