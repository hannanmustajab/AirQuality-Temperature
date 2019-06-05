#include "application.h"
#line 1 "/Users/abdulhannanmustajab/Desktop/Projects/IoT/Particle/tempLogger/TempLogger/src/TempLogger.ino"
/*
 * Project TempLogger
 * Description: Reading Temperature from OneWire 18B20 and sending it to particle cloud. 
 * Author: Abdul Hannan Mustajab
 * Date:
 */


#include "DS18.h"

// Initialize sensor object.
void setup();
void loop();
#line 12 "/Users/abdulhannanmustajab/Desktop/Projects/IoT/Particle/tempLogger/TempLogger/src/TempLogger.ino"
DS18 sensor(D8);

// You could define a smaller array here for your Temperature variable
char temperatureString[16];
char temp[16];




void setup() {

pinMode(D3,OUTPUT);

// Setup Particle Variable

Particle.variable("celsius",String(sensor.celsius()));

}

void loop() {
// Reading data from the sensor.    
   if (sensor.read()) {
     //Sending the temperature in Celsius
    
     // Created temperature String using snprintf. 
      snprintf(temperatureString , 16, "%lf Degrees C",sensor.celsius());
      strcpy(temperatureString,temp);
   
   // Particle.publish("temperature", temp, PRIVATE); 

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
