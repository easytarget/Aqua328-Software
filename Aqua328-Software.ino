/* 
 Aquarium Light controller for an attended tropical/cold water aquarium 
 Based on ATMega328P processor

 Following a 'WakeUp' button press (when the staff are opening the shop up) it will:
   - Slowly fade the lights up, Red, then Green/White, then Blue.
   - 8 Hours Later it will slowly fade the lights down in the same order.

 It continually monitors and reports the temperature on the display, the light, lid
 and fan status is shown there too.

 Further button presses speed the fade cycles up, and provide a manual lights off function.

 The fan briefly cycles during the day to refresh the air under the tank lid.
 As the temperature rises between set limits the fan speed increases to provide
 cooling in summer.  If the Temperature continues to rise the lights are faded 
 back down to assist cooling.

 Pressing and long holding the button during the timed phase will decrease the time left, 
 allowing you to 'run down' the timer if needed (eg after powering off for maintenance)

 When the lid is opened a alert tune is played, if the button is pressed while the lid is
 open the tank enters maintenance mode, the lights are dimmed and the fan disabled if
 necesscary, but the status and timer are preserved.
 
 If the lid has not been opened for 24 hrs a feeding reminder activates; a 'Please Feed' 
 message is displayed and a beep is played every 5 minutes. After 48 hours the message 
 changes to 'Feed me Now' and we have 3 beeps every 3 minutes. Opening the lid to feed
 the fish clears this.

 Both the Lid and Fan functionality is configurable, and can be disabled if not used.

 Finally: Cheesy tunes entertain and delight (ymmv); beeps feedback when the button is pressed.
*/


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
DeviceAddress tankThermometer;  // Not defined here; we will search for the address during setup
bool tempSensor=false; 
float currentTemp = -127; // last successful reading (-127 == no sensor or other error)

// Sounds
Speaker loudSpeaker(SPEAKER_PIN);

// LED State Machine
// Basic light cycle with fast modes.
enum activityStates {Off, TurnOn, FastOn, On, TurnOff, FastOff};
int loopTime = 100; // Minimum loop time (ms)

// Lights
byte blueVal = 0;
byte redVal = 0;
byte greenVal = 0;
enum activityStates ledState = Off;  // State, start 'Off'
unsigned long lastChange = millis(); // record whenever we change the desired led level

// Timing
unsigned long changeTime     =  1800000; // Full LED change (off->on or on->off) (30m)
unsigned long dayCycleTime   = 27000000; // daytime lights on period (7h30m)
unsigned long onPulseTime    =   900000; // Gap between lights-on fan pulses (15m)
unsigned long offPulseTime   =  3600000; // Gap between lights-off fan pulses (1h)
unsigned long pulseTime      =    30000; // Fan pulse length (30s)
unsigned int readTime        =      333; // delay between temperature readings (1/3s)

// Fan 
// Note: Comment out the fan pin definition in pins.h to disable fan.
byte fanVal = 0;          // Default to Off
float fanMinTemp = 27;    // min speed at this temp
float fanMaxTemp = 29;    // max speed at this temp
byte fanMinSpeed = 96;    // min fan PWM value (also; pulse speed)
byte fanMaxSpeed = 255;   // max fan PWM value

// Temperature based Light Dimmer
float ledDimTemp = 28;  // dim down above this tempreature
float ledOffTemp = 30;  // lighting off above this temp

// Temperature Alarm
float lowAlarmTemp =  22.5;           // Low temperature trigger level
float highAlarmTemp = 30.5;           // High temperature trigger level
unsigned long alarmInterval = 300000; // Alarm Interval (5 mins)
int alarmBeeps = 5;                   // How many beeps to make at interval

// User Interface
// Note: Set the lid pin to -1 in pins.h to disable lid functionality
#define BSIZE 13                          // the upper banner size (12 chars + null)
long buttonDelay               = 1000;    // button hold down delay (1s)
unsigned long buttonStart      = 0;       // timer for button
byte lcdOn                     = 255;     // lcd backlight setting when lights on
byte lcdOff                    = 32;      // lcd backlight setting when lights off
bool maintenance               = false;   // maintenance mode (supresses fan and lights)
unsigned long lidBeepInterval  = 120000;  // gap between lid open beeps (2 mins)
unsigned long lidOpenQuietTime = 30000;   // lid open sound repeat holdoff (30s)

// PWM frequency (timer0) adjustment
int timeScale;  // Used to scale time values, defined in setup()

// Feeding Reminders
unsigned long lidStart            = 0;          // timer for lid open
unsigned long lastLid             = 0;          // timer for feeding reminders
unsigned long feedReminder        = 86400000;   // prompt for food time (24hrs)
unsigned long feedNowReminder     = 172800000;  // alert for food time (48hrs)
unsigned long feedBeepInterval    = 300000;     // Gap between feed beeps (5 mins)
unsigned long feedNowBeepInterval = 180000;     // Gap between feed now beeps (3 mins)

// Serial Logging
unsigned long logInterval = 60000;  // log once per minute
bool serialLog = true;              // logging enabled by default

/*
 * Setup and initialisation
 */

void setup()
{
  /* Uncomment these for faster debug on breadboard:
  feedReminder        = 60000; 
  feedNowReminder     = 120000;
  feedBeepInterval    = 10000;
  feedNowBeepInterval = 5000;
  logInterval = 10000;
  lidBeepInterval = 3000; */


  // General IO Pins
  if (LIDSWITCH != -1) pinMode(LIDSWITCH,INPUT_PULLUP);  // Lid switch to input & pull-up
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
  Serial.print(F("Commands: v - logging, i - status, b - button"));
  if (LIDSWITCH != -1)  Serial.print(F(", l - lid toggle"));
  Serial.println();
  Serial.println();

  // PWM Prescaler to set PWM base frequency; avoids fan noise (whine) and LED flicker
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
  lcd.createChar(5, fan2Glyph); // Fan level 2
  lcd.createChar(6, fan3Glyph); // Fan level 3
  lcd.createChar(7, fanPulseGlyph); // Fan pulsing

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
    myDelay(1);
  }

  // play a tune while first sensor reading is taken etc.
  splashtune();

  // Go into initial 'off' state
  setLED(0,255);
  for (int lcdVal=lcdOn; lcdVal > lcdOff; lcdVal--) {
    analogWrite(BACKLIGHT,lcdVal);
    myDelay(1);
  }
  myDelay(333);
  currentTemp = readTemp();
  logState();
}

/* 
 *  Utility Functions
 */

// Custom millisecond delay() function that adjusts for the timescale
void myDelay(unsigned long d) {
  delay(d*timeScale);
}

// Search the oneWire interface for first sensor. If found, set the device address
bool firstOneWireDevice(void) {
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  
  if ( oneWire.search(addr))  {
    logTime();
    Serial.print(F("Found OneWire device with address: "));
    for( i = 0; i < 8; i++) {
      if (addr[i] < 16) {
        Serial.print('0');
      }
      Serial.print(addr[i], HEX);
      if (i < 7) {
        Serial.print(F(":"));
      }
      tankThermometer[i] = addr[i];
    }
    Serial.println();
    if ( OneWire::crc8( addr, 7) != addr[7]) {
        logTime();
        Serial.print(F("Invalid OneWire CRC! No temperature functions available"));
        return false;
    }
  }
  else {
   logTime();
   Serial.print(F("No OneWire devices found: No temperature functions available"));
   return false;
  }
  oneWire.reset_search();  // stop searching
  return true;
}

// Get a temperature reading and display it.
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


// Take supplied temperature and map that to the fan PWM
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
    if (!maintenance) analogWrite(FAN,fan); 
    if ((fan == 0) && (fanVal > 0)) {
      logTime();
      Serial.println(F("Fan: Stopped"));
    } else if ((fan > 0) && (fanVal == 0)) {
      logTime();
      Serial.println(F("Fan: Started"));
    } 
  #endif
  // Show fan icons if running, rather crude logic and maths but hey! it works ;-)
  int fanSpeedStep = (fanMaxSpeed - fanMinSpeed) / 4;
  lcd.setCursor(14,0);
  if (fan >= fanMinSpeed + fanSpeedStep*3) {
    lcd.write(byte(3)); // 'fan' glyph
    lcd.write(byte(6)); // level 4 glyph
  } 
  else if (fan >= fanMinSpeed + fanSpeedStep*2) {
    lcd.write(byte(3)); // 'fan' glyph
    lcd.write(byte(5)); // level 3 glyph
  }
  else if (fan >= fanMinSpeed + fanSpeedStep) {
    lcd.write(byte(3)); // 'fan' glyph
    lcd.write(byte(4)); // level 2 glyph
  }
  else if (fan >= fanMinSpeed) {
    lcd.write(byte(3)); // 'fan' glyph
    lcd.print(' '); // level 1, no glyph
  }
  else {
    lcd.print(F("  ")); // We are off, or no fan
  }
  return fan;
}

// Serial log timestamp
void logTime() {
  Serial.print(F("["));
  unsigned long l = floor(millis()/1000/timeScale);
  int s = l%60;
  int m = int(floor(l / 60)) % 60;
  int h = int(floor(l/3600)) % 24;
  if (h < 10) Serial.print('0');
  Serial.print(h);
  Serial.print(':');
  if (m < 10) Serial.print('0');
  Serial.print(m);
  Serial.print(':');
  if (s < 10) Serial.print('0');
  Serial.print(s);
  Serial.print(F("] "));
}

// Serial log the system values
void logState() {
  logTime();
  Serial.print(F("Log: Tank: "));
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
  if (maintenance) Serial.print(F(": Maintenance")); 
  #ifdef FAN
    else {
      Serial.print(F(": Fan: "));
      Serial.print(fanVal);
    }
  #endif
  if (lidStart != 0) {
    Serial.print(F(": Lid: "));
    Serial.print(int(ceil((millis()-lidStart)/1000/timeScale)));
  }
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

void lidtune() {
  // Adapted from the first few notes of 'the lion sleeps tonight' score found at:
  // https://pianoletternotes.blogspot.com/2019/02/in-jungle-mighty-jungle.html
  unsigned int Length      = 4;
  unsigned int Melody[]    = {NOTE_F4, NOTE_G4, NOTE_A4, NOTE_G4};
  unsigned int Durations[] = {6,       6,       8,       6};
  loudSpeaker.playMelody(Length, Melody, Durations); 
}

// Long beeps
void buttonBeep(int beeps) {
  for (int i=1; i <= beeps; i++) {
    TimerFreeTone(SPEAKER_PIN, NOTE_D4, 120);
    myDelay(250);
  }
}

// Short beeps
void notifyBeep(int beeps) {
  for (int i=1; i <= beeps; i++) {
    TimerFreeTone(SPEAKER_PIN, NOTE_B4, 70);
    myDelay(200);
  }
}

// Announce turn on
void cycleOn() {
  logTime();
  Serial.println(F("Lights: Turning On"));
  for (int lcdVal=lcdOff; lcdVal < lcdOn; lcdVal++) {
    analogWrite(BACKLIGHT,lcdVal);
    myDelay(1);
  }
  lightsontune();
}

// Announce turn off
void cycleOff() {
  logTime();
  Serial.println(F("Lights: Turning Off"));
  lightsofftune();
  for (int lcdVal=lcdOn; lcdVal > lcdOff; lcdVal--) {
    analogWrite(BACKLIGHT,lcdVal);
    myDelay(1);
  }
}

// LED level setting and feedback
// dimmer is a max value applied to all channels evenly
unsigned int lastLevel;
byte lastDimmer;

bool setLED(unsigned int level, byte dimmer) {
  level = constrain(level,0,767); 
  dimmer = constrain(dimmer,0,255);

  if ((level != lastLevel) || (dimmer != lastDimmer)) {
    lcd.setCursor(0,1);
    char dir = ' ';  // no Glyph
    if ((ledState == TurnOn) || (ledState == FastOn)) dir = char(0); // up Glyph
    if ((ledState == TurnOff) || (ledState == FastOff)) dir = char(1); // down Glyph
    if (level == 0) {
      redVal = 0;
      greenVal = 0;
      blueVal = 0;
      lcd.print(F("Off"));
      if (lastLevel != level) { // log when the main loop hits limits
        logTime();
        Serial.println(F("Lights: Off"));
      }
    } else if (level < 256) {
      redVal = min(level, dimmer);
      greenVal = 0;
      blueVal = 0;
      lcd.write(dir);
    } else if (level < 512) {
      redVal = dimmer;
      greenVal = min(level - 256, dimmer);
      blueVal = 0;
      lcd.write(dir);
      lcd.write(dir);
   } else if (level < 767) {
      redVal = dimmer;
      greenVal = dimmer;
      blueVal = min(level - 512, dimmer);
      lcd.write(dir);
      lcd.write(dir);
      lcd.write(dir);
    } else {
      redVal = dimmer;
      greenVal = dimmer;
      blueVal = dimmer;
      if (dimmer == 255 ) lcd.print(F("On"));
      else lcd.print(F("Dim"));
      if (lastLevel != level) {  // log when the main loop hits limits
        logTime();
        Serial.println(F("Lights: On"));
      }
    }
    lcd.print(F("      "));
    analogWrite(RED,redVal);
    analogWrite(GREEN,greenVal);
    analogWrite(BLUE,blueVal);
    lastLevel = level;
    lastDimmer = dimmer; 
    return(true); // levels changed
  } else {
    return(false); // nothing changed
  }
}

// Display the countdown
void countDown() {
  char num [4];
  long l = ((lastChange+(dayCycleTime*timeScale))-millis());
  l = constrain(l,0,dayCycleTime*timeScale);
  l = floor(l/1000/timeScale);
  int s = l%60;
  int m = int(floor(l/60))%60;
  int h = int(floor(l/3600))%100;
  lcd.setCursor(0,1);
  sprintf (num, "%u", h);
  lcd.print(num);
  lcd.print(':');
  sprintf (num, "%02u", m);
  lcd.print(num);
  lcd.print(':');
  sprintf (num, "%02u", s);
  lcd.print(num);
  lcd.print(' ');
}

/*
 * Main Control loop
 * states are: {Off, TurnOn, FastOn, On, TurnOff, FastOff}
 */

// Primary light level is a linear value, covering the 3 channels with 256 values/channel
// Value is applied to Red, then Green/white then Blue, creating a sequenced fade up/down
unsigned int ledLevel          = 0;                      // Desired LED level (R+G+B) 
unsigned int ledTotal          = 767;                    // Maximum brightness (R+G+B)
byte maintDimmer               = 255;                    // Override lights in maintenance
int slowStep                   = changeTime / ledTotal;  // Millis per step in On/Off change mode 
int fastStep                   = 10;                     // Steps per loop cycle in fast On/Off modes
char bannerMem[BSIZE]          = "          ";           // Only update banner when needed
bool lidOverride               = false;                  // Used for serial lid open emulation
bool longButton                = false;                  // records if last button press was 'long'
unsigned long lastRead         = millis();               // temperature read timer
unsigned long lastPulse        = millis();               // fan pulse timer
unsigned long lastLog          = millis();               // log timer, preloaded
unsigned long lastLidReminder  = millis();               // lid reminder timer
unsigned long lastFeedReminder = millis();               // feeding reminder timer
unsigned long lastAlarm        = millis()-(alarmInterval*timeScale); // alarm timer

void loop() {
  unsigned long loopStart = millis();  // Note when we started.
  char banner[BSIZE] = "Aqua328     ";
  bool button = false;  
  
  // Read the water temperature and set fan appropriately
  if (tempSensor && ((millis() - lastRead) > (readTime * timeScale))) {
    // Get the temperature and display it
    currentTemp = readTemp();
    lastRead = millis();
    showTemp(currentTemp);
    fanVal = setFan(currentTemp);
  }

  // Fan pulses when not over temp
  if (fanVal == 0) {
    unsigned long thisPulse = onPulseTime;
    if (ledState == Off) thisPulse = offPulseTime;
    if ((millis() - lastPulse) > ((thisPulse + pulseTime) * timeScale)) {
      if (!maintenance) analogWrite(FAN, 0);
      lcd.setCursor(15,0);
      lcd.write(' ');
      lastPulse = millis();
      logTime();
      Serial.println(F("Fan: Pulsed"));
    }
    else if ((millis() - lastPulse ) > (thisPulse * timeScale)) {
      if (!maintenance) analogWrite(FAN, fanMinSpeed);
      lcd.setCursor(15,0);
      lcd.write(byte(7));
    } 
  }
  else lastPulse = millis();  // holds pulses off when fan is otherwise on

  // LEDs
  int newLevel = ledLevel;
  switch (ledState) {
    case Off: newLevel = 0; break;
    case TurnOn: if (millis()-lastChange > (slowStep*timeScale)) newLevel++; break; 
    case FastOn: newLevel+= fastStep; break;
    case On: {
              newLevel = ledTotal;
              if (maintDimmer == 255) countDown();  // Show remaining ON time
              // Turn off when we time out
              if (millis()-lastChange > (dayCycleTime*timeScale)) {
                logTime();
                Serial.println(F("Auto: Turn Off"));
                ledState = TurnOff; 
                cycleOff(); 
              }  
            } break;
    case TurnOff: if (millis() - lastChange > (slowStep * timeScale)) newLevel--; break;
    case FastOff: newLevel-= fastStep; break;
  }

  newLevel = constrain(newLevel, 0, ledTotal);
  
  // reset timer when main level has changed
  if (newLevel != ledLevel) {
    lastChange = millis();
    ledLevel = newLevel;
  }

  // Maintenance Dimmer 
  if (maintenance) maintDimmer = max(maintDimmer-10,0); 
  else maintDimmer = min(maintDimmer+10,255);

  // Temperature Dimmer 
  const float dimRate = 255 / (ledOffTemp-ledDimTemp);
  byte tempDimmer = constrain(255 - ((currentTemp - ledDimTemp) * dimRate),0,255);
  if (tempDimmer < 255) strcpy(banner,"Cooling     ");

  // Set the LEDs
  setLED(ledLevel,min(tempDimmer,maintDimmer));

  // Change state when limits are reached
  if ((ledLevel == ledTotal) && (ledState != On) && (ledState != TurnOff)) {
    ledState = On;
    notifyBeep(2);
  } 
  if ((ledLevel == 0) && (ledState != Off) && (ledState != TurnOn)) {
    ledState = Off;
    notifyBeep(2);
  }

  // Serial control
  if (Serial.available()) {
    switch (Serial.read()) {
      case 'v': {
                  serialLog = !serialLog; 
                  if (serialLog) {
                    logTime();
                    Serial.println(F("Log: On"));
                    lastLog = millis() - (logInterval * timeScale);
                  } else {
                    logTime();
                    Serial.println(F("Log: Off"));
                  }
                } break;
      case 'i': logState(); break;
      case 'b': button = true; 
                if (Serial.read() == 'b') {
                  logTime();
                  Serial.println(F("Button: Long Serial"));
                  longButton = true;
                } else {
                  logTime();
                  Serial.println(F("Button: Serial"));
                  longButton = false;
                }
                break;
      case 'l': {
                  lidOverride = !lidOverride;
                  break;
                }
    }
    while (Serial.available()) Serial.read(); // chomp serial buffer
  }

  // lid
  if (LIDSWITCH != -1) {
    if ((digitalRead(LIDSWITCH) == LIDOPEN) || lidOverride) {
      if (!maintenance) strcpy(banner, "Lid Open    ");
      else strcpy(banner, "Maintenance ");
      if (lidStart == 0) {
        logTime();
        Serial.println(F("Lid: Opened"));
        lidStart = millis();
        if(millis() - lastLid > (lidOpenQuietTime * timeScale)) {
          // dont play lid open tune when lid only closed for a few seconds..
          lidtune();
        }
        lastLidReminder = millis();
      }
      // timestamp used for feeding reminders
      lastLid = millis();
      // Lid reminder
      if (millis() - lastLidReminder > (lidBeepInterval * timeScale)) {
        logTime();
        Serial.println(F("Lid Reminder"));
        buttonBeep(1);
        lastLidReminder = millis();
      }
    } else {
      if (lidStart > 0) {
        logTime();
        Serial.println(F("Lid: Closed"));
        maintenance=false;
        lidStart = 0;
      } 
    }

    // Feeding Reminder
    if (millis() - lastLid > (feedReminder * timeScale)) {
      if (millis() - lastLid < (feedNowReminder * timeScale)) {
        strcpy(banner, "Please Feed ");
        if (millis() - lastFeedReminder > (feedBeepInterval * timeScale)) {
          logTime();
          Serial.println(F("Feed Reminder"));
          notifyBeep(1);
          lastFeedReminder = millis();
        }
      } else {
        strcpy(banner, "Feed Us Now ");
        if (millis() - lastFeedReminder > (feedNowBeepInterval * timeScale)) {
          logTime();
          Serial.println(F("Feed Now Reminder"));
          notifyBeep(3);
          lastFeedReminder = millis();
        }
      }
    }
  }

  // test if button been activated for a specified time
  if (digitalRead(USERSWITCH) == USERPRESS) {
    // Button pressed, show default message and start timer if needed
    strcpy(banner, "Button      ");
    if (buttonStart == 0)
      buttonStart = millis();

    // Feedback during button press
    if ((millis() - buttonStart) > (buttonDelay * timeScale)) {
      if (lidStart == 0) {
        switch (ledState) {  // feedback
         case Off:     strcpy(banner, "Lights On   "); break;
         case TurnOn:  strcpy(banner, "Fast On     "); break;
         case On:      strcpy(banner, "Already On! "); break;
         case TurnOff: strcpy(banner, "Fast Off    "); break;
        }
      }
      else {
        strcpy(banner, "Maintenance ");
      }
    }

    // Process long button if appropriate,
    if (((millis() - buttonStart) > (3 * buttonDelay * timeScale)) && !maintenance) {
      if (ledState == On) {  // reduce 'on' timer when pressed.
        lastChange = lastChange - (120000 * timeScale); // Serial.println(lastChange);
        strcpy(banner, "Speedup     ");
      } 
      else if (ledState == Off ) {
        strcpy(banner, "Happy Easter");
      }
      longButton = true;
    }
    else {
      // not yet a long press
      longButton = false;
    }
  }
  else if (buttonStart != 0) { 
    // Button not pressed, but timer running: set the flags for button release
    if ((millis() - buttonStart) > (buttonDelay * timeScale)) {  
      logTime();
      button = true;
      if (longButton) Serial.println(F("Button: Long Pressed"));
      else Serial.println(F("Button: Pressed"));
    }
    // And always reset the timer
    buttonStart = 0;
  }

  // Process button actions according to state
  if (button) {
    if (lidStart == 0) {
      // Main Button Actions only when lid closed
      switch (ledState) {
        case Off:      if (!longButton) {
                         ledState = TurnOn; 
                         cycleOn(); 
                       } else {
                         splashtune();
                         logTime();
                         Serial.println(F("Easter Egg Uncovered"));
                       }
                       break;
        case TurnOn:   ledState = FastOn; 
                       buttonBeep(1); 
                       logTime(); 
                       Serial.println(F("Lights: Fast On")); 
                       break;
        case On:       if (!longButton) {
                         logTime();
                         Serial.println(F("Button Pressed in 'On' State"));
                       } else if (!maintenance) {
                         logTime();
                         Serial.println(F("CountDown Reduced"));
                       }
                       break;
        case TurnOff:  ledState = FastOff; 
                       buttonBeep(1); 
                       logTime(); 
                       Serial.println(F("Lights: Fast Off")); 
                       break;
        }
    }
    else {
      // If lid open the only button action is to start maintenance mode
      logTime(); 
      Serial.println(F("Maintenance"));
      #ifdef FAN
        analogWrite(FAN,0); // force fan off during maintenance
      #endif
      maintenance = true;
    }
  }

  // Temperature Alarms 
  if (currentTemp <= lowAlarmTemp) {    
    strcpy(banner, "! TOO COLD !");
    strcpy(banner, "please check");
    strcpy(banner, " heaters    ");
  } else if (currentTemp >= highAlarmTemp) {
    strcpy(banner, "! TOO HOT ! ");
    strcpy(banner, " please add ");
    strcpy(banner, " Cold Water ");
  }
  if ((currentTemp <= lowAlarmTemp) || (currentTemp >= highAlarmTemp)) {
    if ((millis() - lastAlarm) >= (alarmInterval * timeScale)) { 
      notifyBeep(alarmBeeps);
      lastAlarm = millis();
    }
  }

  // Update banner as needed
  if (strcmp(banner,bannerMem) != 0) {
    lcd.setCursor(0,0);
    lcd.print(banner);
    strcpy(bannerMem,banner);
  }

  // Serial Logging
  if (serialLog) {
    if ((millis() - lastLog) >= (logInterval * timeScale)) {
      logState();
      lastLog = millis();
    }
  }

  // Loop complete; wait until looptime has been exceeded
  while ((millis() - loopStart) <= (loopTime * timeScale)) {
    myDelay(1);
  }
}  // Fin
