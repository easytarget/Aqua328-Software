// Physical pin and other hardware attribute settings

// Speaker is on PWM pin D10 
#define SPEAKER 10

// OneWire on pin D2
// #define ONE_WIRE_BUS 4         //  <---- BREADBOARD
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 9

// Switches
#define LIDSWITCH 7 // digital pin D7
#define USERSWITCH 8 // digital pin D8

// Lights
#define RED 5   // Red LED PWM on D5
#define GREEN 6 // Red LED PWM on D6
#define BLUE 3  // Red LED PWM on D3

// Fan 
// Comment out the line below to disable fan.
#define FAN 9            // Fan PWM on D9

// Misc:
// A4 and A5 are used for SDA and SCL on the I2C bus (default)
// D0 and D1 are used by serial and FTDI (default)
