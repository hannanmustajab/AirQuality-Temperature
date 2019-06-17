/*
 * Project TempLogger
 * Description: Reading Temperature from OneWire 18B20 and sending it to particle cloud. 
 * Author: Abdul Hannan Mustajab
 * Date:
 */

/* 
      ***  Next Steps ***
      1) Clean up your logic on the set verboseMode - what if the input is neither "on" nor "off"
      2) clean up the extra comments and make sure you have commented all the remaining code
      3) Initalize the verbose mode as the current approach is undefined - suggest verboseMode = false;
      4) Add a new function that will publish (if in verboseMode) the state transition respecting Particle rate limits
      5) Add a webhook that will publish the temperature and battery level to Ubidots via a Webhook.
          - Excellent tutoral here: http://help.ubidots.com/articles/513304-connect-your-particle-device-to-ubidots-using-particle-webhooks
      6) Add a check that only sends the webhook if one of these are true: top of the hour OR temperature value has changed
 */



// v1.00 - Got the metering working, tested with sensor - viable code
// v1.01 - Added release level to variables
// v1.02 - Moved pin to D6 and started to add finite state machine structure
// v1.03 - Added measurements for WiFi signal
// v1.04 - Added verbose and Measurements Function.
// v1.05 - Added Particle Function For VerboseMode and Setup the IDLE State.
// v1.06 - Added comments for moving IDLE to Time not millis() 
// v1.07 - Made changes for IDLE and VerboseMode. 
// v1.08 - Fixed Verbose Mode, Cleaned Comments and Added State Transition Monitoring . 
// v1.09 - Added UBIDots handler and Setup WebHooks.


const char releaseNumber[6] = "1.08"; // Displays the release on the menu 

#include "DS18.h"             // Include the OneWire library




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

// Variables Related To Update and Refresh Rates. 

unsigned long updateRate = 5000; // Define Update Rate
static unsigned long refreshRate = 1; // Time period for IDLE state. 


bool SetVerboseMode(String command); // Function to Set verbose mode. 
bool verboseMode=false; // Variable VerboseMode. 

float temperatureHook; // Current Temp Reading. 
float lastPublishValue; // LastPublished Reading.



// Setup Particle Variables and Functions here. 

void setup()
{
  getTemperature();
 
temperatureHook = 0; // Current Temp Reading. 
lastPublishValue =1; // LastPublished Reading.

  Particle.variable("celsius", temperatureString); // Setup Particle Variable
  Particle.variable("Release", releaseNumber);
  Particle.variable("Signal", signalString); // Particle variables that enable monitoring using the mobile app
  Particle.variable("Battery", batteryString);
  Particle.function("verboseMode", SetVerboseMode);  // Added Particle Function For VerboseMode. 
  
  if (verboseMode) Particle.publish("State","IDLE", PRIVATE);
  state = IDLE_STATE;
  
  
}

void loop()
{
  
 
  switch (state)
  {
  case IDLE_STATE: // IDLE State.
    
    static unsigned long TimePassed = 0;        
    if (Time.minute() - TimePassed >= refreshRate ) 
    {
    state = MEASURING_STATE;
    TimePassed = Time.minute();     
    if(verboseMode){
      waitUntil(PublishDelayFunction);
      Particle.publish("State","MEASURING",PRIVATE);
      }              
    }
  break;

  case MEASURING_STATE: // Measuring State. 
  
    getMeasurements(); // Get Measurements and Move to Reporting State. 
  // if (abs(temperatureHook - lastPublishValue) > 1) state = REPORTING_STATE;

    state = REPORTING_STATE;
     if(verboseMode){
      waitUntil(PublishDelayFunction);
      Particle.publish("State","REPORTING",PRIVATE);
      
    } 
    break;

  case REPORTING_STATE: //
    if (verboseMode) Particle.publish("Temperature", temperatureString, PRIVATE); 
    if (abs(temperatureHook - lastPublishValue) > 1) UBIDotsHandler();
    
  
   if(verboseMode){
      waitUntil(PublishDelayFunction);
      Particle.publish("State","IDLE",PRIVATE);
      
    } 
    state = IDLE_STATE;
    
    break;
  }
}

// Function to create a delay in the publish time
bool PublishDelayFunction()
{
  static unsigned long tstamp = 0; 
  if (millis() - tstamp <= updateRate)
    return 0;
  else
  {
    tstamp = millis();
    return 1;
  }
}

// Functions for mobile app reporting. 

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

// Function to get temperature value from DS18B20. 
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

// Function to Toggle VerboseMode. 

bool SetVerboseMode(String command)
{

  if(command == "1")
  {
    verboseMode = true;
    waitUntil(PublishDelayFunction);
    Particle.publish("Mode","Verbose Mode Started.", PRIVATE);
    return 1;
  }
  else if (command == "0"){
    verboseMode = false;
    waitUntil(PublishDelayFunction);
    Particle.publish("Mode","Verbose Mode Stopped.", PRIVATE);
    return 1;
    }
    else {
      return 0;
    }
}

bool UBIDotsHandler(){
 
  
    char data[256];
    temperatureHook = sensor.celsius();
    snprintf(data,sizeof(data),"{\"Temperature\":%3.1f, \"Battery\":%3.1f}",temperatureHook, batteryString);
    Particle.publish("Air-Quality-Hook",data,PRIVATE);
    lastPublishValue = temperatureHook;
    return 1;
    }