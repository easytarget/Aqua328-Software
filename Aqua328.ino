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
DeviceAddress insideThermometer, outsideThermometer;

// Assign address manually. The addresses below will need to be changed
// to valid device addresses on your bus. Device address can be retrieved
// by using either oneWire.search(deviceAddress) or individually via
// sensors.getAddress(deviceAddress, index)
DeviceAddress tankThermometer = { 0x10, 0xD8, 0x24, 0x2B, 0x03, 0x08, 0x00, 0x0F };
DeviceAddress gapThermometer  = { 0x10, 0x38, 0x4E, 0x2A, 0x03, 0x08, 0x00, 0x05 };

// Sound Setup
// change this to make the sounds slower or faster
int tempo = 180;
// change this to whichever pin you want to use
int buzzer = 10;



void setup()
{
  Serial.begin(9600);
  lcd.init(); // Start lcd up
  lcd.noCursor(); // do not display cursor
  lcd.setCursor(1,0); // home
  lcd.print("Power On"); // welcome text.
  lcd.setCursor(1,1); // home
  lcd.backlight();
  startuptune();
  lcd.noBacklight();
  Serial.println("Type Stuff:");
}

char getpress() {
  Serial.print("> ");
  while(!Serial.available()) {
    delay(10);
  }
  char c = Serial.read();
  Serial.print("'");
  Serial.print(c);
  Serial.println("' received.");
  // 
  delay(10); 
  while(Serial.available())
  {
    Serial.read();
    delay(10);
  }
  return c;
}

void startuptune() {
  
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

void loop()
{
  // Print a message to the LCD.
  lcd.clear();
  lcd.setCursor(1,0);
  lcd.print("Aqua328");
  delay(1000);
  lcd.setCursor(1,1);
  lcd.print("Demo");
  char ret = getpress();
  lcd.noBacklight();
  lcd.clear();
  lcd.setCursor(1,0);
  lcd.print("Serial:");
  lcd.setCursor(8,0);
  lcd.print(ret);
  lcd.setCursor(1,1);
  lcd.backlight();
  delay(250);
  startuptune();
}
