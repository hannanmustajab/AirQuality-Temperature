#include "application.h"
#line 1 "/Users/chipmc/Documents/Maker/Particle/Projects/AirQuality-Temperature/src/TempLogger.ino"
/*
 * Project TempLogger
 * Description: Reading Temperature from OneWire 18B20 and sending it to particle cloud. 
 * Author: Abdul Hannan Mustajab
 * Date:
 */

// v1.00 - Got the metering working, tested with sensor - viable code
// v1.01 - Added release level to variables
// v1.02 - Moved pin to D6 and started to add finite state machine structure
// v1.03 - Added measurements for WiFi signal

// Version Number.
// #define VERSION "1.00"      - Don't use common names in #define
// #define SOFTWARERELEASENUMBER "1.00"               // Keep track of release numbers
void setup();
void loop();
bool PublishDelayFunction();
void getSignalStrength();
void getBatteryCharge();
#line 16 "/Users/chipmc/Documents/Maker/Particle/Projects/AirQuality-Temperature/src/TempLogger.ino"
const char releaseNumber[6] = "1.03";               // Displays the release on the menu ****  this is not a production release ****


#include "DS18.h"

// Initialize modules here
DS18 sensor(D3);                       // Initialize sensor object

// State Machine Variables
enum State { INITIALIZATION_STATE, IDLE_STATE, MEASURING_STATE, REPORTING_STATE };
State state = INITIALIZATION_STATE;


// Variables Related To Particle Mobile Application Reporting
char signalString[16];                     // Used to communicate Wireless RSSI and Description
char temperatureString[16];                                         // Temperature string for Reporting
char batteryString[16];                                             // Battery value for reporting

unsigned long updateRate = 5000; // Define Update Rate


void setup() {
  Particle.variable("celsius",temperatureString);// Setup Particle Variable
  Particle.variable("Release",releaseNumber);
  Particle.variable("Signal", signalString);                      // Particle variables that enable monitoring using the mobile app
  Particle.variable("Battery", batteryString);

  state= IDLE_STATE;
}


void loop() {
// Reading data from the sensor.    
   if (sensor.read()) {
      snprintf(temperatureString, sizeof(temperatureString), "%3.1f Degrees C", sensor.celsius());  // Ensures you get the size right and prevent memory overflow2
   }
   getSignalStrength();
   getBatteryCharge();
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

  /* Restructure the main loop to put this code into states
  Use a Switch case statement to control program flow based on the state
  Idle state is where the device waits for the 5 second to pass between samples
  Measuring state where you update the temperatureString
  Reporting state where you publish the results
  Then back to the idle state for the next samples
  Give it a shot and let me know if you get stuck.  Your main loop should only be the Switch case statement on "state"
  */
}

void getSignalStrength()
{
  WiFiSignal sig = WiFi.RSSI();
 
  float rssi = sig.getStrength();

  snprintf(signalString,sizeof(signalString), "%.0f%%", rssi);
}

void getBatteryCharge() 
{
  float voltage = analogRead(BATT) * 0.0011224;
  
  snprintf(batteryString, sizeof(batteryString), "%3.1f V", voltage);
}
