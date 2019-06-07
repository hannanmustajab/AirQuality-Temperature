#include "application.h"
#line 1 "/Users/chipmc/Documents/Maker/Particle/Projects/AirQuality-Temperature/src/TempLogger.ino"
/*
 * Project TempLogger
 * Description: Reading Temperature from OneWire 18B20 and sending it to particle cloud. 
 * Author: Abdul Hannan Mustajab
 * Date:
 */


#include "DS18.h"

// Initialize sensor object
void setup();
void loop();
bool PublishDelayFunction();
#line 12 "/Users/chipmc/Documents/Maker/Particle/Projects/AirQuality-Temperature/src/TempLogger.ino"
DS18 sensor(D4);

// You could define a smaller array here for your Temperature variable
char temperatureString[16];

unsigned long updateRate = 5000; // Define Update Rate


void setup() {
  Particle.variable("celsius",temperatureString);// Setup Particle Variable
  // Serial.begin();   // you don't need this line with Particle even if you are using Serial.print()
}


void loop() {
// Reading data from the sensor.    
   if (sensor.read()) {
      snprintf(temperatureString, sizeof(temperatureString), "%3.1f Degrees C", sensor.celsius());  // Ensures you get the size right and prevent memory overflow2
   }
   waitUntil(PublishDelayFunction);
   Particle.publish("Temperature",temperatureString,PRIVATE);  
  }

// Function to create a delay in the publish time 
bool PublishDelayFunction()
{
  static unsigned long tstamp = 0;                      // Static variables are defined once and retain their value
  if (millis() - tstamp  <= updateRate) return 0;
  else 
  {
    tstamp = millis();
    return 1;
  }
}

