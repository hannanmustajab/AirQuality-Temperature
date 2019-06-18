/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "application.h"
#line 1 "/Users/abdulhannanmustajab/Desktop/Projects/IoT/Particle/tempLogger/TempLogger/src/TempLogger.ino"
/*
 * Project TempLogger
 * Description: Reading Temperature from OneWire 18B20 and sending it to particle cloud. 
 * Author: Abdul Hannan Mustajab
 * Date:
 */

/* 
      ***  Next Steps ***
      1) Add more comments in your code - helps in sharing with others and remembering why you did something 6 months from now
      2) Need to report every hour - even if the temperature has not changed.  
      3) Next, we need to complete the reporting loop to Ubidots.  You will get a response when you send a webhook to Ubidots.
          - Check that this repose is "201" which is defined using the response template in your WebHook
          - Add a function that reads this response and published a message (if in verboseMode) that the data was received by Ubidots
          - Add a new state (RESPONSE_WAIT) that will look for the response from Ubidots and timeout if 45 seconds pass - going to an ERROR_STATE
          - Add a new state (ERROR_STATE) which will reset the Argon after 30 seconds
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


void setup();
void loop();
bool PublishDelayFunction();
void getSignalStrength();
void getBatteryCharge();
bool getTemperature();
void getMeasurements();
void sendUBIDots();
#line 33 "/Users/abdulhannanmustajab/Desktop/Projects/IoT/Particle/tempLogger/TempLogger/src/TempLogger.ino"
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
  REPORTING_STATE,
  RESPONSE_WAIT,
  ERROR_STATE

};
State state = INITIALIZATION_STATE;

// Variables Related To Particle Mobile Application Reporting

char signalString[16];                                                      // Used to communicate Wireless RSSI and Description
char temperatureString[16];                                                 // Temperature string for Reporting
char batteryString[16];                                                     // Battery value for reporting

// Variables Related To Update and Refresh Rates. 

unsigned long updateRate = 5000;                                            // Define Update Rate
static unsigned long refreshRate = 1;                                       // Time period for IDLE state. 


bool SetVerboseMode(String command);                                        // Function to Set verbose mode.     *** This is not needed with Particle
bool verboseMode=true;                                                     // Variable VerboseMode. 

// Section for sensor specific variables and function protoypes. 

float temperatureInC = 0;                                                    // Current temperature in C. 
float lastTemperatureInC = 0;                                                // Last published temperature Value in C. 

// Setup Particle Variables and Functions here. 

void setup()
{
  getTemperature();                                                          //
  Particle.variable("celsius", temperatureString);                           // Setup Particle Variable
  Particle.variable("Release", releaseNumber);
  Particle.variable("Signal", signalString);                                 // Particle variables that enable monitoring using the mobile app
  Particle.variable("Battery", batteryString);
  Particle.function("verboseMode", SetVerboseMode);                          // Added Particle Function For VerboseMode. 
  
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
      Particle.publish("State","IDLE",PRIVATE);
      }              
    }
  break;

  case MEASURING_STATE:                                                         // Measuring State. 
    
    if(verboseMode)
      {
      Particle.publish("State","MEASURING",PRIVATE);
      } 
    
    getMeasurements();                                                          // Get Measurements and Move to Reporting State. 
    if(abs(lastTemperatureInC-temperatureInC) >= 1) state = REPORTING_STATE;
     state = IDLE_STATE;
     
    break;

  case REPORTING_STATE: //
  if(verboseMode){
      waitUntil(PublishDelayFunction);
      Particle.publish("State","REPORTING",PRIVATE);
      Particle.publish("Temperature", temperatureString, PRIVATE); 
    } 
    sendUBIDots();
   
    state = IDLE_STATE;
    
    break;

  case RESPONSE_WAIT: // This checks for the response from UBIDOTS. 
   break;

  case ERROR_STATE: // This state RESETS the devices. 
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
bool getTemperature()
{
  lastTemperatureInC = temperatureInC; 
  
  if (sensor.read())
  { 
    temperatureInC = sensor.celsius();
    snprintf(temperatureString, sizeof(temperatureString), "%3.1f Degrees C", temperatureInC); 
  }
  return 1;
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

void sendUBIDots(){
 
  if(sensor.read()){
     char data[256];
    snprintf(data,sizeof(data),"{\"Temperature\":%3.1f, \"Battery\":%3.1f}",temperatureInC, batteryString);
    Particle.publish("Air-Quality-Hook",data,PRIVATE);
    
    }
   
    }