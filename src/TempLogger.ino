/*
 * Project TempLogger
 * Description: Reading Temperature from OneWire 18B20 and sending it to particle cloud. 
 * Author: Abdul Hannan Mustajab
 * Date:
 */


#include "DS18.h"

// Initialize sensor object.
DS18 sensor(D4);
char data[100];



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

    delay(5000);
  }

}
