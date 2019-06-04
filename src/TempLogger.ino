/*
 * Project TempLogger
 * Description: Reading Temperature from OneWire 18B20 and sending it to particle cloud. 
 * Author: Abdul Hannan Mustajab
 * Date:
 */


#include "DS18.h"

// Initialize sensor object.
DS18 sensor(D4);
char data[100];  // This is a big array - yet it is not used.
// You could define a smaller array here for your Temperature variable
char temperatureString[16];



void setup() {

pinMode(D3,OUTPUT);

// Setup Particle Variable

Particle.variable("temperature",String(sensor.celsius()));

}

void loop() {
// Reading data from the sensor.    
   if (sensor.read()) {
     //Sending the temperature in Celsius
    Particle.publish("temperature", String(sensor.celsius()), PRIVATE); 
    
    //Sending the temperature in  fahrenheit.
    Particle.publish("farenhiet",String(sensor.fahrenheit()),PRIVATE);

    // What if you constructed a more descriptive string for your Particle.variable here
    // Use snprintf and the temperatureString to construct a variable like: "50 degrees C"

    delay(5000);  // This is a blocking delay
    /* 
    You should avoid delays that block program execution.  
    Consider another approach taking advantage of Particle's waitUntil() function
    Particle services process requests each time it completes the main loop - your program should not block 
    the program flow through the main loop.  This gets more important as the code size grows.
    */
  }

}
