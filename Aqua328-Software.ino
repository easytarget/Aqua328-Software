// Aquarium Light controller for an attended aquarium (no clock..)
// Based on ATMega328P processor
//
// Following a 'WakeUp' button press (when the staff are opening the shop up) it will:
//   - Slowly fade the lights up, Red, then Green/White, then Blue.
//   - 8 Hours Later it will slowly fade the lights down in the same order.
//
// It continually monitors and reports the temperature on the display, the light 
// and fan status is shown there too.
// 
// The fan briefly cycles during the day to refresh the ait under the tank lid
// As the temperature rises between set limits the fan speed increases to provide cooling in summer.
//
// Additional button presses turn the lights on and off manually, speed the fade cycles up.
//
// Finally cheesy tunes entertain and delight; beeps feedback when the button is pressed

// ToDo; lid sensor that dims lights and alerts with a tune.
//       dimming the LCD via the spare (D11) pwm pin.

#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "pins.h"
#include "musicnotes.h"
#include "glyphs.h"

// 1602 display
LiquidCrystal_I2C lcd(0x3F,16,2);  // set the LCD address to 0x3F for a 16 chars and 2 line display

// Setup a OneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our OneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Dallas OneWire temperature Sensor
DeviceAddress tankThermometer;
bool tempSensor=false;
float currentTemp = -127; // last successful reading (-127 == no sensor or other error)

// Sounds
// change this to make the sounds slower or faster
int tempo = 180;

// Lights
byte redVal = 0;   // current red value
byte greenVal = 0; // current green value
byte blueVal = 0;  // current blue value
int fastFadeRate = 30; // Rate (millis per step) for maintenance on/off cycle
int slowFadeRate = 1500; // Rate (millis per step) for day on/off cycle 

// Fan 
// Comment out the fan pin definition in pins.h to disable fan.
byte fanVal = 0;         // Default to Off
float fanMinTemp = 27.0; // min speed at this temp
float fanMaxTemp = 28.5; // max speed at this temp
byte fanMinSpeed = 128;  // min PWM value
byte fanMaxSpeed = 255;  // max PWM value

// PWM adjustment
int timeScale; // Used to scale time readings when we adjust Timer0 multiplier

// Debug
bool serialTemps = false; // Show temperature on serial port (lots of output)

/*
 * SETUP
 */

void setup()
{
  // General IO Pins
  pinMode(LIDSWITCH,INPUT_PULLUP);  // Lid switch to input & pull-up
  pinMode(USERSWITCH,INPUT_PULLUP); // User button to input & pull-up
  pinMode(RED,OUTPUT);              // PWM, RED led channel
  pinMode(GREEN,OUTPUT);            // PWM, GREEN led channel
  pinMode(BLUE,OUTPUT);             // PWM, BLUE led channel
  analogWrite(RED,redVal);          // Set default
  analogWrite(GREEN,greenVal);      // Set default
  analogWrite(BLUE,blueVal);        // Set default
  #ifdef FAN
    pinMode(FAN,OUTPUT);              // PWM, Fan
    analogWrite(FAN,fanVal);          // Set default
  #endif

  // Serial
  Serial.begin(19200);

  // Greetings on serial port
  Serial.println();
  Serial.println(F("Welcome to Aqua328"));
  Serial.println(F("https://github.com/easytarget/Aqua328-Software"));
  Serial.println(F("Commands: l - logging, q - quiet, i - status, b - button"));

  // PWM Prescaler to set PWM base frequency; avoids fan noise (whine) and make LED's smooth
  TCCR0B = TCCR0B & B11111000 | B00000010; // Timer0 (D5,D6) ~7.8 KHz
  timeScale = 8; // Adjusting Timer0 makes millis() and delay() run faster.
  TCCR1B = TCCR1B & B11111000 | B00000001; // Timer1 (D9,D10) ~31.4 KHz
  TCCR2B = TCCR2B & B11111000 | B00000001; // Timer2 (D3,D11) ~31.4 KHz

  // Display
  lcd.init();                   // Start lcd up
  lcd.noCursor();               // no cursor
  lcd.backlight();              // lights on
  lcd.createChar(0, onGlyph);   // Define custom glyphs for up, down, degrees and fan settings.
  lcd.createChar(1, offGlyph);
  lcd.createChar(2, degreesGlyph);
  lcd.createChar(3, fanGlyph);  // Fan on/off
  lcd.createChar(4, fan1Glyph); // Fan level 1
  lcd.createChar(5, fan2Glyph); // fan level 2
  lcd.createChar(6, fan3Glyph); // fan level 3

  lcd.setCursor(0,0);    // Say Hello
  lcd.print("Aqua328");
  lcd.setCursor(0,1);
  lcd.print(F("Welcome!"));

  // OneWire temp sensors
  tempSensor = firstOneWireDevice(); // true if a sensorr is found
  if (tempSensor) {
    sensors.begin();
    sensors.requestTemperatures(); // Send initial command to get temperatures
  }

  // pause and play a tune while temps being gathered
  splashtune();
  lcd.setCursor(0,1);
  lcd.print(F("Off     ")); // Start with lights 'Off'
}

// Custom delay function that adjusts for the timescale
void mydelay(unsigned long d) {
  delay(d*timeScale);
}

// Custom delay function that adjusts for the timescale
unsigned long mymillis() {
  return (millis()/timeScale);
}

/* 
 *  FUNCTIONS
 */

bool firstOneWireDevice(void) {
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  
  if ( oneWire.search(addr))  {
    Serial.print(F("Found OneWire device with address: "));
    for( i = 0; i < 8; i++) {
      Serial.print("0x");
      if (addr[i] < 16) {
        Serial.print('0');
      }
      Serial.print(addr[i], HEX);
      if (i < 7) {
        Serial.print(F(", "));
      }
      tankThermometer[i] = addr[i];
    }
    Serial.println();
    if ( OneWire::crc8( addr, 7) != addr[7]) {
        Serial.print(F("Invalid OneWire CRC! No temperature functions available"));
        return false;
    }
  }
  else {
   Serial.print(F("No OneWire devices found: No temperature functions available"));
   return false;
  }
  oneWire.reset_search();
  return true;
}

char getpress(int waitTime) {
  unsigned long start = mymillis();
  while(!Serial.available() && (start+waitTime) <= mymillis()) {
    mydelay(10);
  }
  char c = Serial.read();
  return c;
}

float showTemp() {
  if (tempSensor) {
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
    return tankTemp;
  }
  else return -127;
}

byte setFan(float temp) {
  // takes current temperature and maps that to a fan speed value (percentage)
  // uses that percentage to set the fan PWM within the min and max pwm value range
  // Returns 0-4 indicating off, 25, 50, 75, 100% of the speed (for display) respectively

  #ifndef FAN
    // No fan, no need to process this, returning zero will suppress display icons
    return 0;
  #else
    // We have a fan; process the temperature
/*
byte fanMinSpeed = 128;  // min PWM value
byte fanMaxSpeed = 255;  // max PWM value*/
    byte fan;
    float newVal = fanMinSpeed + (temp - fanMinTemp) * ((fanMaxSpeed-fanMinSpeed)/(fanMaxTemp-fanMinTemp));
    if (newVal < fanMinSpeed) fan = 0; // fully off 
    else if (newVal > fanMaxSpeed) fan = fanMaxSpeed; // fullspeed
    else fan = floor(newVal);
  
    analogWrite(FAN,fan);

    return fan;
  #endif
}

/*
 * SOUNDS
 */

void lightsofftune() {
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
    tone(SPEAKER, melody[thisNote], noteDuration * 0.9);
    mydelay(noteDuration);
    noTone(SPEAKER);
  }
}

void lightsontune() {
  int melody[] = {
    // Pacman
    // Score available at https://musescore.com/user/85429/scores/107109
    NOTE_B4, 16, NOTE_B5, 16, NOTE_FS5, 16, NOTE_DS5, 16, //1
    NOTE_B5, 32, NOTE_FS5, -16, NOTE_DS5, 8, NOTE_C5, 16,
    NOTE_C6, 16, NOTE_G6, 16, NOTE_E6, 16, NOTE_C6, 32, NOTE_G6, -16, NOTE_E6, 8,
  
    NOTE_B4, 16,  NOTE_B5, 16,  NOTE_FS5, 16,   NOTE_DS5, 16,  NOTE_B5, 32,  //2
    NOTE_FS5, -16, NOTE_DS5, 8,  NOTE_DS5, 32, NOTE_E5, 32,  NOTE_F5, 32,
    NOTE_F5, 32,  NOTE_FS5, 32,  NOTE_G5, 32,  NOTE_G5, 32, NOTE_GS5, 32,  NOTE_A5, 16, NOTE_B5, 8
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
    tone(SPEAKER, melody[thisNote], noteDuration * 0.9);
    mydelay(noteDuration);
    noTone(SPEAKER);
  }
}

void splashtune() {
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
    tone(SPEAKER, melody[thisNote], noteDuration * 0.9);
    mydelay(noteDuration);
    noTone(SPEAKER);
  }
}

void buttonbeep(int beeps) {
  for (int i=1; i <= beeps; i++) {
    tone(SPEAKER, NOTE_D4, 150);
    mydelay(250);
  }
  noTone(SPEAKER);
}

void notifybeep(int beeps) {
  for (int i=1; i <= beeps; i++) {
    tone(SPEAKER, NOTE_B4, 100);
    mydelay(200);
  }
  noTone(SPEAKER);
}

void logState() {
  Serial.print(F("Tank: "));
  if (!tempSensor) Serial.print(F("No Sensor: "));
  else if (currentTemp == -127) Serial.print(F("Sensor Error: "));
  else {
    Serial.print(currentTemp,2);
    Serial.print(char(194));
    Serial.print(char(176));
    Serial.print(F("C: "));
  }
  Serial.print(F("Time: "));
  Serial.print((float)mymillis()/1000,0);
  Serial.print(F(": R: "));
  Serial.print(redVal);
  Serial.print(F(": G: "));
  Serial.print(greenVal);
  Serial.print(F(": B: "));
  Serial.print(blueVal);
  #ifdef FAN
    Serial.print(F(": Fan: "));
    Serial.print(fanVal);
  #endif
  Serial.println();
}

/*
 * LOOPS
 */
 
void cycleOn(int del1, int del2) {
  Serial.println(F("Lights: Turning On"));
  lcd.setCursor(0,1);
  lcd.print(F("     "));
  lightsontune();
  lcd.setCursor(0,1);
  lcd.write(byte(0)); // 'up' sysmbol we defined in setup()
  lcd.print(F("     "));
  Serial.println(F("Red: Fading Up"));
  for (int a=0; a <= 255; a++) {
    analogWrite(RED,a);
    redVal=a;
    mydelay(del1);
    if (!digitalRead(USERSWITCH)) {
      Serial.println(F("Speedup:"));
      buttonbeep(1);
      while(!digitalRead(USERSWITCH)) mydelay(1);
      del1=del2;
    }
    showTemp();
  }
  lcd.setCursor(1,1);
  lcd.write(byte(0)); // 'up' sysmbol we defined in setup()
  Serial.println(F("Green: Fading Up"));
  for (int a=0; a <= 255; a++) {
    analogWrite(GREEN,a);
    greenVal=a;
    mydelay(del1);
    if (!digitalRead(USERSWITCH)) {
      Serial.println(F("Speedup:"));
      buttonbeep(1);
      while(!digitalRead(USERSWITCH)) mydelay(1);
      del1=del2;
    }
    showTemp();
  }
  lcd.setCursor(2,1);
  lcd.write(byte(0)); // 'up' sysmbol we defined in setup()
  Serial.println(F("Blue: Fading Up"));
  for (int a=0; a <= 255; a++) {
    analogWrite(BLUE,a);
    blueVal=a;
    mydelay(del1);
    if (!digitalRead(USERSWITCH)) {
      Serial.println(F("Speedup:"));
      buttonbeep(1);
      while(!digitalRead(USERSWITCH)) mydelay(1);
      del1=del2;
    }
    showTemp();
  }

  // Notify lights now on.
  notifybeep(2);
  lcd.setCursor(0,1);
  lcd.print(F("On      "));
  Serial.println(F("Lights: On"));
}

void cycleOff(int del1, int del2) {
  Serial.println(F("Lights: Turning Off"));
  lightsofftune();
  lcd.setCursor(0,1);
  lcd.write(byte(1)); // 'down' sysmbol we defined in setup()
  lcd.print(F("     "));
  Serial.println(F("Red: Fading Down"));
  for (int a=255; a >= 0; a--) {
    analogWrite(RED,a);
    redVal=a;
    mydelay(del1);
    if (!digitalRead(USERSWITCH)) {
      Serial.println(F("Speedup:"));
      buttonbeep(1);
      while(!digitalRead(USERSWITCH)) mydelay(1);
      del1=del2;
    }
    showTemp();
  }
  lcd.setCursor(1,1);
  lcd.write(byte(1)); // 'down' sysmbol we defined in setup()
  Serial.println(F("Green: Fading Down"));
  for (int a=255; a >= 0; a--) {
    analogWrite(GREEN,a);
    greenVal=a;
    mydelay(del1);
    if (!digitalRead(USERSWITCH)) {
      Serial.println(F("Speedup:"));
      buttonbeep(1);
      while(!digitalRead(USERSWITCH)) mydelay(1);
      del1=del2;
    }
    showTemp();
  }
  lcd.setCursor(2,1);
  lcd.write(byte(1)); // 'down' sysmbol we defined in setup()
  Serial.println(F("Blue: Fading Down"));
  for (int a=255; a >= 0; a--) {
    analogWrite(BLUE,a);
    mydelay(del1);
    if (!digitalRead(USERSWITCH)) {
      Serial.println(F("Speedup:"));
      buttonbeep(1);
      while(!digitalRead(USERSWITCH)) mydelay(1);
      del1=del2;
    }
    blueVal=a;
    showTemp();
  }

  // Notify lights now off.
  notifybeep(2);
  lcd.setCursor(0,1);
  lcd.print(F("Off     "));
  Serial.println(F("Lights: Off"));
}

/*
 * Main Control loop
 */

int lightStatus = 0; // 0=off, 1=On
bool buttonHit = false; // set to true when the button is hit

void loop()
{
  lcd.setCursor(0,0);
  if (!digitalRead(USERSWITCH)) { // switches are inverted..
    lcd.print(F("Button! "));
    Serial.println(F("Button: pressed"));
    while(!digitalRead(USERSWITCH)) mydelay(1); // debounce
    buttonHit = true;
    }
  else if (!digitalRead(LIDSWITCH)) { // switches are inverted..
    lcd.print(F("Lid! "));
    Serial.println(F("Lid: opened"));
    // Do more here once lid switch hardware is in place; 
    // Eg: notify and dim lights
    }
  else {
    // Restore display and check to see if the button has been pressed
    lcd.print(F("Aqua328       "));
    if (buttonHit) {
      buttonHit=false;
      if (lightStatus == 0) {
        // turn on
        cycleOn(slowFadeRate,fastFadeRate);
        lightStatus = 1;
      } else {
        // turn off 
        cycleOff(slowFadeRate,fastFadeRate);
        lightStatus = 0;
      }
    }
  }

  // Hold for one second here, processing serial and buttons
  switch ( getpress(500) ) {
    case 'l': serialTemps = true; Serial.println(F("Logging: On")); break;
    case 'q': serialTemps = false; Serial.println(F("Logging: Off")); break;
    case 'i': logState(); break;
    case 'b': buttonHit = true; break;
  }

  // Show the water temperature.
  if (tempSensor) {
    // Get the temperature (and display at same time)
    currentTemp = showTemp(); 
    
    // Fan
    fanVal = setFan(currentTemp);
    int fanSpeedStep = (fanMaxSpeed - fanMinSpeed) / 4;
    lcd.setCursor(14,0);
    if (fanVal >= fanMinSpeed + fanSpeedStep*3) {
      lcd.write(byte(3)); // 'fan'
      lcd.write(byte(6)); // level 4
    } 
    else if (fanVal >= fanMinSpeed + fanSpeedStep*2) {
      lcd.write(byte(3)); // 'fan'
      lcd.write(byte(5)); // level 3
    }
    else if (fanVal >= fanMinSpeed + fanSpeedStep) {
      lcd.write(byte(3)); // 'fan'
      lcd.write(byte(4)); // level 2
    }
    else if (fanVal >= fanMinSpeed) {
      lcd.write(byte(3)); // 'fan'
      lcd.print(' '); // level 1, no symbol
    }
    else {
      lcd.print(F("  ")); // No fan display since we are off
    }
  }

  if (serialTemps) {
  // Put a loop here to only log once per minute/second
    logState();
  }

  // And loop back to the button/temp display
}
