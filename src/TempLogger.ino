/*
 * Project TempLogger
 * Description:
 * Author:
 * Date:
 */


#include "DS18.h"

// Initialize sensor object.
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
