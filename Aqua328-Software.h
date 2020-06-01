// Export our modified timer functions so we can use them in the TimerFreeTone.cpp

// Custom delay functions that adjust for the timescale
void myDelay(unsigned long d);

// PWM frequency (timer0) adjustment
extern int timeScale;  // Used to scale time values, defined in setup()
