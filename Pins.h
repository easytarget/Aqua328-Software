// Physical pin and other hardware attribute settings

// Speaker is on PWM pin D10 
#define SPEAKER_PIN 10

// OneWire on pin D2
//#define ONE_WIRE_BUS 4    //  <---- BREADBOARD (AdaFruit Trinket + ftdi)
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 11

// Switches
//#define LIDSWITCH 12  //  <---- BREADBOARD (AdaFruit Trinket + ftdi)
#define LIDSWITCH 7    // digital pin D7, pullup enabled, -1 to disable
#define USERSWITCH 8   // digital pin D8, pullup enabled
#define LIDOPEN HIGH   // Pin in this state when Lid is open 
#define USERPRESS LOW  // Pin in this state when user presses the button

// Lights
#define RED 5   // Red LED PWM on D5
#define GREEN 6 // Red LED PWM on D6
#define BLUE 3  // Red LED PWM on D3

// Fan 
// Comment out the line below to disable fan.
#define FAN 9          // Fan PWM on D9

// Misc:
#define BACKLIGHT 11   // Display Backlight PWM on D11

// A4 and A5 are used for SDA and SCL on the I2C bus (default)
// D0 and D1 are used by serial and FTDI (default)
