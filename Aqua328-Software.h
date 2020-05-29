// Export our modified timer functions so we can use them in the TimerFreeTone.cpp

// Custom delay function that adjusts for the timescale
void mydelay(unsigned long d);
void mydelayMicroseconds(unsigned long d);
// Custom millis function that adjusts for the timescale
unsigned long mymillis();
