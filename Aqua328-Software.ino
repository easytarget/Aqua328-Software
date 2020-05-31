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
//
//  ToDo; lid sensor that dims lights and alerts with a tune.
//        dimming the LCD via the spare (D11) pwm pin.
//        much better state machine for the main loop

#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Pins.h"
#include "Glyphs.h"
#include "TimerFreeTone.h"
#include "Speaker.h"

// 1602 display
LiquidCrystal_I2C lcd(0x3F,16,2);  // set the LCD address to 0x3F, which is common for 1602 lcd display

// Setup a OneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our OneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Dallas OneWire temperature Sensor
DeviceAddress tankThermometer;  // we will search for the address during setup
bool tempSensor=false; 
float currentTemp = -127; // last successful reading (-127 == no sensor or other error)

// Sounds
Speaker loudSpeaker(SPEAKER_PIN);

// State Machine
// Basic light cycle with fast modes, can add lit open states etc later.
enum activityStates {Off, TurnOn, FastOn, On, TurnOff, FastOff};

// Note: I try to keep all values below as dynamic variables (not defines) so that adding eeprom and
//       user setting of values (eg serial, jog dial, wifi, etc) becomes easier to implement later

// Lights
byte blueVal = 0;
byte redVal = 0;
byte greenVal = 0;
enum activityStates ledState = Off; // State, start as 'Off'


// Timing
unsigned long changeTime  =  1800000; // Full LED change (off-on or on-off) in ms (30 mins)
unsigned long cycleTime   = 27000000; // daytime lights on period in ms (7 hrs 30 mins)
// TODO unsigned long pulseTime  =  1800000; // Gap between fan pulses in ms (20 mins) (4x slower when off)
unsigned int readTime     = 333; // delay between temperature readings in ms

// Fan 
// Comment out the fan pin definition in pins.h to disable fan.
byte fanVal = 0;          // Default to Off
float fanMinTemp = 26.5;  // min speed at this temp
float fanMaxTemp = 27.5;  // max speed at this temp
byte fanMinSpeed = 96;    // min PWM value
byte fanMaxSpeed = 255;   // max PWM value

// User Interface
int buttonDelay = 200;      // Button hold down delay (debounce and anti-tap)
byte lcdOn  = 255;          // lcd backlight when lights on
byte lcdOff = 32;           // lcd backlight when lights off

// PWM frequency (timer0) adjustment
int timeScale;  // Used to scale time readings when we adjust Timer0 multiplier

// Logging
unsigned long logInterval = 60000;  // log once per minute
bool serialLog = false;         // Show temperature on serial port

/*
 * Setup and initialisation
 */

void setup()
{
  // General IO Pins
  pinMode(LIDSWITCH,INPUT_PULLUP);  // Lid switch to input & pull-up
  pinMode(USERSWITCH,INPUT_PULLUP); // User button to input & pull-up
  pinMode(RED,OUTPUT);              // PWM: RED led channel
  pinMode(GREEN,OUTPUT);            // PWM: GREEN led channel
  pinMode(BLUE,OUTPUT);             // PWM: BLUE led channel
  analogWrite(RED,redVal);          // Set default
  analogWrite(GREEN,greenVal);      // Set default
  analogWrite(BLUE,blueVal);        // Set default
  pinMode(BACKLIGHT,OUTPUT);        // PWM: LCD Backlight led channel
  analogWrite(BACKLIGHT,0);         // Set default
  #ifdef FAN
    pinMode(FAN,OUTPUT);            // PWM: Fan
    analogWrite(FAN,fanVal);        // Set default
  #endif

  // Serial
  Serial.begin(57600);

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
  lcd.createChar(3, fanGlyph);  // Fan symbol
  lcd.createChar(4, fan1Glyph); // Fan level 1
  lcd.createChar(5, fan2Glyph); // fan level 2
  lcd.createChar(6, fan3Glyph); // fan level 3

  lcd.setCursor(0,0);    // Say Hello
  lcd.print("Aqua328");
  lcd.setCursor(0,1);
  lcd.print(F("Welcome!"));

  // OneWire temp sensors
  tempSensor = firstOneWireDevice(); // true if a sensor is found
  if (tempSensor) {
    sensors.begin();
    sensors.requestTemperatures(); // Send initial command to get temperatures
  }

  for (int lcdVal=0; lcdVal < lcdOn; lcdVal++) {
    analogWrite(BACKLIGHT,lcdVal);
    mydelay(1);
  }

  // play a tune while first sensor reading is taken etc.
  splashtune();
  mydelay(333);

  // Go into initial 'off' state
  setLED(0);
  for (int lcdVal=lcdOn; lcdVal > lcdOff; lcdVal--) {
    analogWrite(BACKLIGHT,lcdVal);
    mydelay(1);
  }
}

/* 
 *  Utility Functions
 */

// Custom millisecond delay() function that adjusts for the timescale
void mydelay(unsigned long d) {
  delay(d*timeScale);
}

// Custom microsecond delay() function that adjusts for the timescale
void mydelayMicroseconds(unsigned long d) {
  delayMicroseconds(d*timeScale);
}

// Custom millis() function that adjusts for the timescale
unsigned long mymillis() {
  return (millis()/timeScale);
}

// Search the oneWire interface for first sensor. If found, set the device address
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
  oneWire.reset_search();  // stop searching
  return true;
}

// Wait for specified time while checking for serial input, acts as main delay in the loop
char getSerial(unsigned int waitTime) {
  unsigned long start = mymillis();
  while(!Serial.available() && (mymillis() - start <= waitTime)) {
    mydelay(1);
  }
  char c = Serial.read();
  return c;
}

// Get a temperature reading and displays it.
float readTemp() {
  float tankTemp = -127;
  if (tempSensor) {
    // Get the current reading
    tankTemp = sensors.getTempC(tankThermometer);
    // Request a new temperature reading
    sensors.requestTemperatures();
  }
  return tankTemp;
}

// Display temperature reading
void showTemp(float temp) {
  // Display the temperature
  lcd.setCursor(11,1);
  if (temp != -127) {
    lcd.print(temp,1);
    lcd.write(byte(2));    // 'degrees' sysmbol we defined in setup()
    lcd.print(F("  "));
  } else {
    lcd.print(F("Error"));
  }
}


// Take supplied temperature and maps that to the fan PWM pin as needed
byte setFan(float temp) {
  byte fan = 0;
  // use temperature to calculate PWM value proportionally within the min and max range
  #ifdef FAN
    if (temp == -127) fan = 0;  // sensor failure
    else {
      // We have a fan; process the temperature
      float newVal = fanMinSpeed + (temp - fanMinTemp) * ((fanMaxSpeed-fanMinSpeed)/(fanMaxTemp-fanMinTemp));
      if (newVal < fanMinSpeed) fan = 0;  // fully off 
      else if (newVal > fanMaxSpeed) fan = fanMaxSpeed;  // fully on
      else fan = floor(newVal);  // in-between values
    }
    analogWrite(FAN,fan);
  #endif
  // Show fan icons, rather crude logic and maths but hey! it works ;-)
  int fanSpeedStep = (fanMaxSpeed - fanMinSpeed) / 4;
  lcd.setCursor(14,0);
  if (fan >= fanMinSpeed + fanSpeedStep*3) {
    lcd.write(byte(3)); // 'fan'
    lcd.write(byte(6)); // level 4
  } 
  else if (fan >= fanMinSpeed + fanSpeedStep*2) {
    lcd.write(byte(3)); // 'fan'
    lcd.write(byte(5)); // level 3
  }
  else if (fan >= fanMinSpeed + fanSpeedStep) {
    lcd.write(byte(3)); // 'fan'
    lcd.write(byte(4)); // level 2
  }
  else if (fan >= fanMinSpeed) {
    lcd.write(byte(3)); // 'fan'
    lcd.print(' '); // level 1, no symbol
  }
  else {
    lcd.print(F("  ")); // We are off, or no fan
  }
  return fan;
}

// Serial logging 
void logState() {
  Serial.print(F("Tank: "));
  if (!tempSensor) Serial.print(F("No Sensor: "));
  else if (currentTemp == -127) Serial.print(F("Sensor Error: "));
  else {
    Serial.print(currentTemp,2);
    Serial.print(char(194));
    Serial.print(char(176));
    Serial.print(F("C"));
  }
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
  //Serial.print(F(": Countdown: "));
  //Serial.print((float)mymillis()/1000,0);
  Serial.println();
}

/*
 * Feedback
 */

void splashtune() {
  // 'Hurrah!' Tune from the PlayMelody examples
  unsigned int Length      = 6;
  unsigned int Melody[]    = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5, NOTE_G4, NOTE_C5};
  unsigned int Durations[] = {8      , 8      , 8      , 4      , 8      , 4      };
  loudSpeaker.playMelody(Length, Melody, Durations); 
}

void lightsontune() {
  // Adapted from the 'windowsXP startup' score found at:
  // https://pianoletternotes.blogspot.com/2017/10/windows-xp-startup-and-shutdown-sounds.html
  unsigned int Length      = 8;
  unsigned int Melody[]    = {NOTE_D6, NOTE_NN, NOTE_D5, NOTE_A5, NOTE_G5, NOTE_D5, NOTE_D6, NOTE_A5};
  unsigned int Durations[] = {4      , 8      , 8      , 4      , 4      , 4      , 4      , 4      };
  loudSpeaker.playMelody(Length, Melody, Durations); 
}

void lightsofftune() {
  // Adapted from the 'windowsXP shutdown' score found at:
  // https://pianoletternotes.blogspot.com/2017/10/windows-xp-startup-and-shutdown-sounds.html
  unsigned int Length      = 4;
  unsigned int Melody[]    = {NOTE_G6, NOTE_D6, NOTE_G5, NOTE_A5};
  unsigned int Durations[] = {6,       6,       6,       6};
  loudSpeaker.playMelody(Length, Melody, Durations); 
}

// Long beeps
void buttonBeep(int beeps) {
  for (int i=1; i <= beeps; i++) {
    TimerFreeTone(SPEAKER_PIN, NOTE_D4, 120);
    mydelay(250);
  }
}

// Short beeps
void notifyBeep(int beeps) {
  for (int i=1; i <= beeps; i++) {
    TimerFreeTone(SPEAKER_PIN, NOTE_B4, 70);
    mydelay(200);
  }
}

// Announce turn on
void cycleOn() {
  Serial.println(F("Lights: Turning On"));
  lightsontune();
  for (int lcdVal=lcdOff; lcdVal < lcdOn; lcdVal++) {
    analogWrite(BACKLIGHT,lcdVal);
    mydelay(1);
  }
}

// Announce turn off
void cycleOff() {
  Serial.println(F("Lights: Turning Off"));
  lightsofftune();
  for (int lcdVal=lcdOn; lcdVal > lcdOff; lcdVal--) {
    analogWrite(BACKLIGHT,lcdVal);
    mydelay(1);
  }
}

// LED level setting and feedback
void setLED(int level) {
  byte pInd = '?';
  if ((ledState == TurnOn) || (ledState == FastOn)) pInd = 0; // up
  if ((ledState == TurnOff) || (ledState == FastOff)) pInd = 1; // down
  lcd.setCursor(0,1);
  if (level == 0) {
    redVal = 0;
    greenVal = 0;
    blueVal = 0;
    lcd.print(F("Off"));
    Serial.println(F("Lights: Off"));
  } else if (level < 256) {
    redVal = level;
    greenVal = 0;
    blueVal = 0;
    lcd.write(pInd);
  } else if (level < 512) {
    redVal = 255;
    greenVal = level - 256;
    blueVal = 0;
    lcd.write(pInd);
    lcd.write(pInd);
 } else if (level < 767) {
    redVal = 255;
    greenVal = 255;
    blueVal = level - 512;
    lcd.write(pInd);
    lcd.write(pInd);
    lcd.write(pInd);
  } else {
    redVal = 255;
    greenVal = 255;
    blueVal = 255;
    lcd.print(F("On"));
    Serial.println(F("Lights: On"));
  }
  lcd.print(F("      "));
  analogWrite(RED,redVal);
  analogWrite(GREEN,greenVal);
  analogWrite(BLUE,blueVal);
}

/*
 * Main Control loop
 * states are: {Off, TurnOn, FastOn, On, TurnOff, FastOff}
 */

int loopDelay = 50;                     // Basic 'tempo' of the state machine loop
int ledLevel = 0;                       // Total level (R+G+B) 
int ledTotal = 767;                     // Maximum brightness 
int slowStep = changeTime / ledTotal;   // Millis per step in slow change mode
unsigned long lastChange = mymillis();  // last time we changed light
unsigned long buttonStart = 0;          // positive press timer for button
unsigned long lastRead = mymillis();    // temperature read timer
//TODO unsigned long lastPulse = 0;          // fan pulse timer
unsigned long lastLog = 0;              // log timer

void loop() {
  unsigned long loopStart = mymillis();  // Note when we started.

  // Show the water temperature.
  if (tempSensor && ((mymillis() - lastRead) > readTime)) {
    // Get the temperature (and display at same time)
    currentTemp = readTemp();
    lastRead = mymillis();
    showTemp(currentTemp);
    fanVal = setFan(currentTemp);
  }

  // LEDs
  int newLevel = ledLevel;
  switch (ledState) {
    case Off: newLevel = 0; break;
    case TurnOn: if (mymillis() - lastChange > slowStep) newLevel++; break;
    case FastOn: newLevel+= 10; break;
    case On: newLevel = ledTotal; break;
    case TurnOff: if (mymillis() - lastChange > slowStep) newLevel--; break;
    case FastOff: newLevel-= 10; break;
  }
  newLevel = constrain(newLevel, 0, ledTotal);
  // Set new level only if it is changed
  if (newLevel != ledLevel) {
    lastChange = mymillis();
    ledLevel = newLevel;
    setLED(ledLevel);
  }
  // Change state when limits are reached
  if ((newLevel == ledTotal) && (ledState != On) && (ledState != TurnOff)) {
    ledState = On;
    notifyBeep(2);
  } 
  if ((newLevel == 0) && (ledState != Off) && (ledState != TurnOn)) {
    ledState = Off;
    notifyBeep(2);
  }

  // test if button or lid has been activated
  bool button = false;
  lcd.setCursor(0,0);
  if (!digitalRead(USERSWITCH)) { // switches are inverted..
    lcd.print(F("Button! "));
    Serial.println(F("Button: pressed"));
    while(!digitalRead(USERSWITCH)) mydelay(1); // simple hold
    button = true;
  }
  else if (!digitalRead(LIDSWITCH)) { // switches are inverted..
    lcd.print(F("Lid! "));
    Serial.println(F("Lid: opened"));
    // Do more here once lid switch hardware is in place; 
    // Eg: notify and dim lights, set feeding timer, etc..
  }

  // Display
  lcd.setCursor(0,0);
  lcd.print(F("Aqua328       "));

  if (serialLog) {
    if ((mymillis() - lastLog) >= logInterval) {  // more than a minute
      logState();
      lastLog = mymillis();
    }
  }

  // Primary loop delay here, processing serial
  switch ( getSerial(loopDelay) ) {
    case 'l': serialLog = true; Serial.println(F("Log: On")); break;
    case 'q': serialLog = false; Serial.println(F("Log: Off")); break;
    case 'i': logState(); break;
    case 'b': button = true; break;  // Serial simulation of button
  }

    // Process button actions according to state
  if (button) {
    switch (ledState) {
      case Off: ledState = TurnOn; cycleOn(); break;
      case TurnOn: ledState = FastOn; buttonBeep(1); Serial.println(F("Speedup")); break;
      case On: ledState = TurnOff; cycleOff(); break;
      case TurnOff: ledState = FastOff; buttonBeep(1); Serial.println(F("Speedup")); break;
    }
  }
}  // Fin
