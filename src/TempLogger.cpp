#include "application.h"
#line 1 "/Users/abdulhannanmustajab/Desktop/Projects/IoT/Particle/tempLogger/TempLogger/src/TempLogger.ino"
/*
 * Project TempLogger
 * Description:
 * Author:
 * Date:
 */


#include "DS18.h"

// Initialize sensor object.
void setup();
void loop();
#line 12 "/Users/abdulhannanmustajab/Desktop/Projects/IoT/Particle/tempLogger/TempLogger/src/TempLogger.ino"
DS18 sensor(D4);



// setup() runs once, when the device is first turned on.
void setup() {
  // Put initialization like pinMode and begin functions here.

}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  // The core of your code will likely live here.
   
   if (sensor.read()) {
    Particle.publish("temperature", String(sensor.celsius()), PRIVATE);
    Particle.publish("farenhiet",String(sensor.fahrenheit()),PRIVATE);
  }

}
