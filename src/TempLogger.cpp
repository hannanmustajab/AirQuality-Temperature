#include "application.h"
#line 1 "/Users/abdulhannanmustajab/Desktop/Projects/IoT/Particle/tempLogger/TempLogger/src/TempLogger.ino"
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
#line 12 "/Users/abdulhannanmustajab/Desktop/Projects/IoT/Particle/tempLogger/TempLogger/src/TempLogger.ino"
DS18 sensor(D8);

// You could define a smaller array here for your Temperature variable
char temperatureString[16];

int updateRate = 5000; // Define Update Rate
unsigned long tstamp = millis();  // Initialize timestamp
//unsigned long time = millis();





void setup() {

  //pinMode(D3,OUTPUT);   // Why are you defining this pin - don't see it being used

  Particle.variable("celsius",temperatureString);// Setup Particle Variable
  Serial.begin(); 

}




void loop() {
// Reading data from the sensor.    
   if (sensor.read()) {

      //snprintf(temperatureString , 16, "%0.2lf Degrees C",sensor.celsius());
      snprintf(temperatureString, sizeof(temperatureString), "%3.1f Degrees C", sensor.celsius());  // Ensures you get the size right and prevent memory overflow
      Serial.print("sensor.celsius()");
   }

    /*  
    You should avoid delays that block program execution.  
    Consider another approach taking advantage of Particle's waitUntil() function
    Particle services process requests each time it completes the main loop - your program should not block 
    the program flow through the main loop.  This gets more important as the code size grows.
     */
    //waitFor(sensor.read,5000);  // This will not rate limit as sensor.read will test true much faster than 1/sec. In fact, you are here in the code because it is true
   // waitUntil(PublishDelayFunction);
    //Particle.publish("farenhiet",String(sensor.fahrenheit()),PRIVATE);  
  }

/*void PublishDelayFunction(long tstamp,int updateRate)
{
  if (millis() <=unsigned ( updateRate + tstamp ))
  {
    return 0;
  }
    else 
    {
      tstamp = millis();
      return 1;
    }
  
}
*/
// Hint, what if there were a function that would return "true" only if 1 second had passed since last publish
/*  Key elements would be: 
    Certaintly in execution - use waitUntil() not waitFor()
    a constant defined at the top of the sketch setting the update rate
    a constant defined at the top that would hold the timestamp of the last publish
    a function that would check to see if millis() was more than that timestamp plus that rate
    call that function in the waitUntil statement.

    Take a stab at this and then I will help if you are still stuck.  Again, this is something you will use a lot
*/


