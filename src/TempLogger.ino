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
      4) In reporting state, why two If conditionals?
      5) In response wait state, where is the state transition message?
      6) In ERROR state, publish that resetting in 30 secs, then delay 30 secs and reset the device.
      7) Add a Particle.function() that will enable you to force a measurement then change the measuring frequency to 15 mins.
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
// v1.10 - Updated comment formatting and fixed the Ubidots reporting logic for temp change
// v1.11 - Added UbiDots Response Handler


const char releaseNumber[6] = "1.10";                       // Displays the release on the menu 

#include "DS18.h"                                           // Include the OneWire library

// Initialize modules here
DS18 sensor(D3);                                            // Initialize the temperature sensor object


// State Machine Variables
enum State                                              
{
  INITIALIZATION_STATE,
  IDLE_STATE,
  MEASURING_STATE,
  REPORTING_STATE,
  RESPONSE_WAIT,
  ERROR_STATE
};                                                          // These are the allowed states in the main loop
State state = INITIALIZATION_STATE;                         // Initialize the state variable

// Variables Related To Particle Mobile Application Reporting
char signalString[16];                                      // Used to communicate Wireless RSSI and Description
char temperatureString[16];                                 // Temperature string for Reporting
char batteryString[16];                                     // Battery value for reporting

// Variables Related To Update and Refresh Rates. 
unsigned long updateRate = 5000;                            // Define Update Rate
static unsigned long refreshRate = 1;                       // Time period for IDLE state. 
static unsigned long publishTime =0;                        // Timestamp for Ubidots publish
const unsigned long webhookTimeout = 45000;                 // Timeperiod to wait for a response from Ubidots before going to error State.
unsigned long webhookTimeStamp = 0;                         // Webhooks timestamp.
unsigned long publishTimeHour=0;

// Variables releated to the sensors 

bool verboseMode;                                            // Variable VerboseMode. 
float temperatureInC=0;                                     // Current Temp Reading global variable
float voltage;                                              // Voltage level of the LiPo battery - 3.6-4.2V range
bool inTransit = false;


void setup()
{
  // This part receives Response using Particle.subscribe() and tells the response received from Ubidots. 

  char responseTopic[125];
  String deviceID = System.deviceID();                            // Multiple Electrons share the same hook - keeps things straight
  deviceID.toCharArray(responseTopic,125);
  Particle.subscribe(responseTopic, UbidotsHandler, MY_DEVICES);  // Subscribe to the integration response event


  getTemperature();
  Particle.variable("celsius", temperatureString);          // Setup Particle Variable
  Particle.variable("Release", releaseNumber);              // So we can see what release is running from the console
  Particle.variable("Signal", signalString);                // Particle variables that enable monitoring using the mobile app
  Particle.variable("Battery", batteryString);              // Battery level in V as the Argon does not have a fuel cell
  Particle.function("verboseMode", SetVerboseMode);         // Added Particle Function For VerboseMode. 
  if (verboseMode) Particle.publish("State","IDLE", PRIVATE);
  
  state = IDLE_STATE;                                       // If we made it this far, we are ready to go to IDLE in the main loop
}

void loop()
{
  
  switch (state)                                            // In the main loop, all code execution must take place in a defined state
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

  case MEASURING_STATE:                                    // Measuring State. 
    if (getMeasurements()) {                               // Get Measurements and Move to Reporting State if there is a change
      state = REPORTING_STATE;
       if(verboseMode)
       {
          waitUntil(PublishDelayFunction);
          Particle.publish("State","Change detected - Reporting",PRIVATE);
        } 
    }
    
    else if(Time.hour() != publishTimeHour)        // Check if 60 minutes or 1 hr has passed. 
    {                      
      state = REPORTING_STATE;
        if(verboseMode){
          waitUntil(PublishDelayFunction);
          Particle.publish("State","Time Passed - Reporting",PRIVATE);      //Tells us that One Hour has passed. 
        } 
    }
    else {
      state = IDLE_STATE;
      if(verboseMode){
        waitUntil(PublishDelayFunction);
        Particle.publish("State","No change - Idle",PRIVATE);         
      } 
    }
    break;

  case REPORTING_STATE: //
    if (Time.hour() == 12) Particle.syncTime();                             // SET CLOCK EACH DAY AT 12 NOON.
    if(verboseMode)
    { 
      Particle.publish("Temperature", temperatureString, PRIVATE); 
    } 
    if (verboseMode)
    {
      waitUntil(PublishDelayFunction);
      Particle.publish("State","Waiting RESPONSE",PRIVATE);
    }
    sendUBIDots();
    state = RESPONSE_WAIT;
    
  

      break;

  case RESPONSE_WAIT:     
  
    if (!inTransit){
        
     state = IDLE_STATE;
    }                                                                     // This checks for the response from UBIDOTS. 
    if(millis() - webhookTimeStamp > webhookTimeout ){
      Particle.publish("spark/device/session/end", "", PRIVATE);          //
      state = ERROR_STATE;                                                // time out
    }
  break;

  case ERROR_STATE:                                                       // This state RESETS the devices. 
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
  voltage = analogRead(BATT) * 0.0011224;
  snprintf(batteryString, sizeof(batteryString), "%3.1f V", voltage);
}


bool getMeasurements()
{
  getSignalStrength();                                      // Get Signal Strength
  getBatteryCharge();                                       // Get Battery Charge Percentage
  if (getTemperature()) return 1;                           // Read Temperature from Sensor
  else return 0;                                            // Less than 1 degree difference detected
}

// Function to get temperature value from DS18B20. 
bool getTemperature()
{
  static float lastTemperatureInC = 0; 
  if (sensor.read())
  { 
    temperatureInC = sensor.celsius();
    snprintf(temperatureString, sizeof(temperatureString), "%3.1f Degrees C", temperatureInC); 
    
  }
  if (abs(temperatureInC - lastTemperatureInC) >= 1) {
    lastTemperatureInC = temperatureInC;
    return 1;
  }
  else return 0;
}

// Function to Toggle VerboseMode. 
bool SetVerboseMode(String command)
{

  if(command == "1" && verboseMode == false)
  {
    verboseMode = true;
    waitUntil(PublishDelayFunction);
    Particle.publish("Mode","Verbose Mode Started.", PRIVATE);
    return 1;
  }

   if(command == "1" && verboseMode == true)
  {
    waitUntil(PublishDelayFunction);
    Particle.publish("Mode","Verbose Mode Already ON.", PRIVATE);
    return 0;
  }

   if (command == "0" && verboseMode == true)
  {
    verboseMode = false;
    waitUntil(PublishDelayFunction);
    Particle.publish("Mode","Verbose Mode Stopped.", PRIVATE);
    return 1;
  }
  
   if (command == "0" && verboseMode == false)
  {
    waitUntil(PublishDelayFunction);
    Particle.publish("Mode","Verbose Mode already OFF.", PRIVATE);
    return 0;
  }

  else 
  {
    return 0;
  }

}


void sendUBIDots()
{
  char data[256];
  snprintf(data,sizeof(data),"{\"Temperature\":%3.1f, \"Battery\":%3.1f}",temperatureInC, voltage);
  Particle.publish("Air-Quality-Hook",data,PRIVATE);
  publishTime = Time.minute();
  publishTimeHour = Time.hour();
  webhookTimeStamp = millis();
  inTransit = true;

}


void UbidotsHandler(const char *event, const char *data)  // Looks at the response from Ubidots - Will reset Photon if no successful response
{
  // Response Template: "{{hourly.0.status_code}}"
  if (!data) {                                            // First check to see if there is any data
    Particle.publish("Ubidots Hook", "No Data",PRIVATE);
    return;
  }
  int responseCode = atoi(data);                          // Response is only a single number thanks to Template
  if ((responseCode == 200) || (responseCode == 201))
  {
    Particle.publish("State","Response Received",PRIVATE);
    inTransit = false;                                 // Data has been received
  }
  else Particle.publish("Ubidots Hook", data,PRIVATE);             // Publish the response code
}