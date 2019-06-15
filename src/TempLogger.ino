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
// v1.04 - Added verbose and Measurements Function.
// v1.05 - Added Particle Function For VerboseMode and Setup the IDLE State.
// v1.06 - Added comments for moving IDLE to Time not millis() 

const char releaseNumber[6] = "1.06"; // Displays the release on the menu ****  this is not a production release ****

#include "DS18.h"

// Initialize modules here
DS18 sensor(D3); // Initialize sensor object

// State Machine Variables
enum State
{
  INITIALIZATION_STATE,
  IDLE_STATE,
  MEASURING_STATE,
  REPORTING_STATE
};
State state = INITIALIZATION_STATE;

// Variables Related To Particle Mobile Application Reporting
char signalString[16];      // Used to communicate Wireless RSSI and Description
char temperatureString[16]; // Temperature string for Reporting
char batteryString[16];     // Battery value for reporting

unsigned long updateRate = 5000; // Define Update Rate
static unsigned long refreshRate = 1;




void setup()
{
  getTemperature();
  Particle.variable("celsius", temperatureString); // Setup Particle Variable
  Particle.variable("Release", releaseNumber);
  Particle.variable("Signal", signalString); // Particle variables that enable monitoring using the mobile app
  Particle.variable("Battery", batteryString);
  Particle.function("verboseMode", verboseMode);  // Added Particle Function For VerboseMode. 
  
  state = IDLE_STATE;
  
}

void loop()
{

  switch (state)
  {
  case IDLE_STATE:
    //System.sleep(D3,CHANGE);  // We are not ready to put the Particle to sleep yet
    // Idle state should be where the Particle spends its time waiting to do something
    // Bring back the code you had before that checks to see if 5 minutes have passed
    // Once they have, change the state to MEASURING_STATE

    static unsigned long TimePassed = 0;        // If you define a variable in a case - then you need to enclose that case in brackets to define scope 
    if (Time.minute() - TimePassed >= refreshRate ) {
    state = MEASURING_STATE;
    TimePassed = Time.minute();                     // This will work - but only if we never put the device to sleep
    }                                           // Try defining the interval using Time functions as the interval will almost 
                                                // always be minutes if not hours.  Also, millis() stops counting when you sleep
    break;

  case MEASURING_STATE: // Excellent, you nailed this state

    getMeasurements();

    state = REPORTING_STATE;
    break;

  case REPORTING_STATE: // Remember that you are in a finite state machine - you know exactly what the Electron
                        // has done up to this point.  You don't have to waitUntil here because there is no issues with rate limiteing
                        // Otherwise, you nailed this state as well
    if (verboseMode) Particle.publish("Temperature", temperatureString, PRIVATE);
    state = IDLE_STATE;
    break;
  }
}

// Function to create a delay in the publish time
bool PublishDelayFunction()
{
  static unsigned long tstamp = 0; // Static variables are defined once and retain their value
  if (millis() - tstamp <= updateRate)
    return 0;
  else
  {
    tstamp = millis();
    return 1;
  }
}


/* Restructure the main loop to put this code into states
  Use a Switch case statement to control program flow based on the state
  Idle state is where the device waits for the 5 second to pass between samples
  Measuring state where you update the temperatureString
  Reporting state where you publish the results
  Then back to the idle state for the next samples
  Give it a shot and let me know if you get stuck.  Your main loop should only be the Switch case statement on "state"
  */

void getSignalStrength()
{
  WiFiSignal sig = WiFi.RSSI();

  float rssi = sig.getStrength();

  snprintf(signalString, sizeof(signalString), "%.0f%%", rssi);
}

void getBatteryCharge()
{
  float voltage = analogRead(BATT) * 0.0011224;

  snprintf(batteryString, sizeof(batteryString), "%3.1f V", voltage);
}

void getTemperature()
{
  if (sensor.read())
  {
    snprintf(temperatureString, sizeof(temperatureString), "%3.1f Degrees C", sensor.celsius()); 
  }
  
}

void getMeasurements()
{

  getSignalStrength(); // Get Signal Strength

  getBatteryCharge(); // Get Battery Charge Percentage

  getTemperature(); // Read Temperature from Sensor.
  
}

bool verboseMode(String toggleSensor)
{
  if (toggleSensor == "on")
  {
    
    return 1;
  }
  else 
  {
    return 0;
  }
}

// Particle functions accept a string and must retun a boolean.  
// Extract the value you want (1 or 0) and discard the rest.  If you don't get a valid input return a 0
// If you can make this work, you can change the inputs to "on" and "off" easily.  Use the function to set
// the value of a boolean verboseMode and then put a conditional before every Particle.publish().