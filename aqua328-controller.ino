// Octoprint-Pal

/* HARDWARE:
DS18B20 Temperature Sensors on 1 wire
  Connections:
  DS18B20 Pinout (Left to Right, pins down, flat side toward you)
  - Left   = Ground
  - Center = Signal (Pin 2):  (with 3.3K to 4.7K resistor to +5 or 3.3 )
  - Right  = +5 or +3.3 V
  */

// I need some libraries
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Device Numbers
#define LCD_ADDRESS 0x3F
#define ONE_WIRE_BUS_PIN 3

#define SPEED 2


// I2C LCD display
LiquidCrystal_I2C lcd(LCD_ADDRESS, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

// oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS_PIN);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Assign the addresses of your 1-Wire temp sensors.
// See the tutorial on how to obtain these addresses:
// http://www.hacktronics.com/Tutorials/arduino-1-wire-address-finder.html
DeviceAddress Probe01 = {0x10,0xD8,0x24,0x2B,0x03,0x08,0x00,0x0F};

// local variables 
int c=1; // simple counter
bool alarM=false; // Knock-Knock

void setup()
{
  // LED-pin as output
  pinMode(13, OUTPUT);
  // Initialize the Temperature measurement library
  sensors.begin();
  sensors.requestTemperatures();
  // set the resolution to 10 bit (Can be 9 to 12 bits .. lower is faster)
  sensors.setResolution(Probe01, 10);
  // and start a measurement cycle
  sensors.requestTemperatures();
  // Display
  lcd.begin(16,2);

  // Splash
  for (int a=10;a>=0;a--) {
      lcd.setCursor(a,0);
      lcd.print("Owen! ");
      delay(200);
  }
  printTemps();
  delay(666);
  lcd.setCursor(2,1);
  lcd.print("Lets begin.    ");
  delay(2000);
  lcd.clear();
  lcd.home();
}
  
void loop()
{
  lcd.setCursor(0,1);
  lcd.print("#");
  lcd.print(c);
  c++;
  lcd.setCursor(0,0);
  lcd.print("Lamp On in : ");
  for (int a=SPEED;a>0;a--) {
      lcd.setCursor(14,0);
      lcd.print(a);
      lcd.print(" ");
      printTemps();
      if (alarM) a=SPEED/10;
      delay(1000);
  }
  lcd.setCursor(0,0);
  lcd.print("Lamp On         ");
  digitalWrite(13, HIGH);
  delay(500);
  
  lcd.setCursor(0,0);
  lcd.print("Laser On in : ");
  for (int a=SPEED;a>0;a--) {
      lcd.setCursor(14,0);
      lcd.print(a);
      lcd.print(" ");
      printTemps();
      if (alarM) a=SPEED/10;
      delay(1000);
  }
  lcd.setCursor(0,0);
  lcd.print("Laser On        ");
  delay(500);
  
  lcd.setCursor(0,0);
  lcd.print("LED On in : ");
  for (int a=SPEED;a>0;a--) {
      lcd.setCursor(14,0);
      lcd.print(a);
      lcd.print(" ");
      printTemps();
      if (alarM) a=SPEED/10;
      delay(1000);
  }
  lcd.setCursor(0,0);
  lcd.print("LED On          ");
  delay(500);
  
  lcd.setCursor(0,0);
  lcd.print("Lamp Off in : ");
  for (int a=SPEED;a>0;a--) {
      lcd.setCursor(14,0);
      lcd.print(a);
      lcd.print(" ");
      printTemps();
      if (alarM) a=SPEED/10;
      delay(1000);
  }
  lcd.setCursor(0,0);
  lcd.print("Lamp Off        ");
  digitalWrite(13, LOW);
  delay(500);
  
  lcd.setCursor(0,0);
  lcd.print("Laser Off in : ");
  for (int a=SPEED;a>0;a--) {
      lcd.setCursor(14,0);
      lcd.print(a);
      lcd.print(" ");
      printTemps();
      if (alarM) a=SPEED/10;
      delay(1000);
  }
  lcd.setCursor(0,0);
  lcd.print("Laser Off       ");
  delay(500);
  
  lcd.setCursor(0,0);
  lcd.print("LED Off in : ");
  for (int a=SPEED;a>0;a--) {
      lcd.setCursor(14,0);
      lcd.print(a);
      lcd.print(" ");
      printTemps();
      if (alarM) a=SPEED/10;
      delay(1000);
  }
  lcd.setCursor(0,0);
  lcd.print("LED Off         ");
}

void printTemps()
{
  float tempC = sensors.getTempC(Probe01);
  lcd.setCursor(5,1);
  lcd.print("          ");
  lcd.setCursor(5,1);
  if (tempC == -127.00) 
  {
    lcd.print("T: N/A");
  } 
  else
  {
    lcd.print("T: ");
    lcd.print(tempC);
  }
  sensors.requestTemperatures();
  if (tempC >= 30) {
    alarM=true;
  }
  else {
    alarM=false;
  }
}

