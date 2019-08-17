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
      Good 1) Add more comments in your code - helps in sharing with others and remembering why you did something 6 months from now
      Not Complete 2) Need to report every hour - even if the temperature has not changed.  
      Good 3) Next, we need to complete the reporting loop to Ubidots.  You will get a response when you send a webhook to Ubidots.
          - Check that this repose is "201" which is defined using the response template in your WebHook
          - Add a function that reads this response and published a message (if in verboseMode) that the data was received by Ubidots
          - Add a new state (RESPONSE_WAIT) that will look for the response from Ubidots and timeout if 45 seconds pass - going to an ERROR_STATE
          - Add a new state (ERROR_STATE) which will reset the Argon after 30 seconds
      Good 4) In reporting state, why two If conditionals?
      Good 5) In response wait state, where is the state transition message?
      Not Complete 6) In ERROR state, publish that resetting in 30 secs, then delay 30 secs and reset the device.
      Good 7) Add a Particle.function() that will enable you to force a measurement then change the measuring frequency to 15 mins.
      8) Adaptive sampling - I have an idea that could be interesting.  I have not yet implemented this on my sensors so, something new
      Adaptive sampling rate ( we will do this on temp for now).   Here is the PublishDelayFunction
        - Have a base rate of sampling - say every 15 minutes
        - Only report to Ubidots if there is a change greater than x
        - However, if there is a change of y  (where y > x) then we start sampling ever 5 minutes 
        - We only report to Ubidots if there is a chance greater than x
        - Once we have a period where the change is z (z < x), we go back to sampling every 15 minutes
        - Even if there is no change, we report at least every hour
      Think of this use case.  If we are sampling air quality, imagine sampling at a slower rate but, when there is a sudden change, 
      such as during rush hour, we take more frequent samples and show to less frequent samples when there is less change.
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
// v1.12 - Cleaned up the code a small bit and added new tasks
// v1.13 - Rewritten the Forced Reading Function.

void setup();
void loop();
bool PublishDelayFunction();
void getSignalStrength();
void getBatteryCharge();
bool getMeasurements();
bool getTemperature();
bool SetVerboseMode(String command);
void sendUBIDots();
void UbidotsHandler(const char *event, const char *data);
void transitionState(void);
bool forcedReading(String Command);
bool adaptiveMode();
#line 48 "/Users/abdulhannanmustajab/Desktop/Projects/IoT/Particle/tempLogger/TempLogger/src/TempLogger.ino"
const char releaseNumber[6] = "1.13"; // Displays the release on the menu

#include "DS18.h" // Include the OneWire library

// Initialize modules here
DS18 sensor(D3); // Initialize the temperature sensor object

// State Machine Variables
enum State
{
  INITIALIZATION_STATE,
  IDLE_STATE,
  MEASURING_STATE,
  REPORTING_DETERMINATION,
  REPORTING_STATE,
  RESPONSE_WAIT,
  ERROR_STATE
};
// These are the allowed states in the main loop
char stateNames[7][44] = {"Initial State", "Idle State", "Measuring", "Reporting Determination", "Reporting", "Response Wait", "Error Wait"};
State state = INITIALIZATION_STATE;    // Initialize the state variable
State oldState = INITIALIZATION_STATE; //Initialize the oldState Variable

// Variables Related To Particle Mobile Application Reporting
char signalString[16];      // Used to communicate Wireless RSSI and Description
char temperatureString[16]; // Temperature string for Reporting
char batteryString[16];     // Battery value for reporting

// Variables Related To Update and Refresh Rates.
unsigned long updateRate = 5000;            // Define Update Rate
static unsigned long refreshRate = 1;       // Time period for IDLE state.
static unsigned long publishTime = 0;       // Timestamp for Ubidots publish
const unsigned long webhookTimeout = 45000; // Timeperiod to wait for a response from Ubidots before going to error State.
unsigned long webhookTimeStamp = 0;         // Webhooks timestamp.
signed long publishTimeHour = 0;
static unsigned long forcedReadingRate = 1;   // Interval for the data in Forced Reading Mode.
static unsigned long adaptiveReadingRate = 5; // Interval for adaptive measuring state.

// static unsigned long baseChangeTemp = 1;     // Temperature change for the base sampling rate
// static unsigned long limitingChangeTemp = 2; // Limiting Temperature change for the sampling rate

// Variables releated to the sensors

bool verboseMode = true;  // Variable VerboseMode.
float temperatureInC = 0; // Current Temp Reading global variable
float voltage;            // Voltage level of the LiPo battery - 3.6-4.2V range
bool inTransit = false;   // This variable is used to check if the data is inTransit to Ubidots or not. If inTransit is false, Then data is succesfully sent.
bool forcedMode = false;  //Forced value mode.
bool adaptiveModeOn;      // Variable to check if Adaptive mode is on or off.

void setup()
{
  // This part receives Response using Particle.subscribe() and tells the response received from Ubidots.

  char responseTopic[125];
  String deviceID = System.deviceID(); // Multiple Electrons share the same hook - keeps things straight
  deviceID.toCharArray(responseTopic, 125);
  Particle.subscribe(responseTopic, UbidotsHandler, MY_DEVICES); // Subscribe to the integration response event

  // Particle Functions.

  Particle.function("verboseMode", SetVerboseMode); // Added Particle Function For VerboseMode.
  Particle.function("GetReading", forcedReading);   // This function will force it to get a reading and set the refresh rate to 15mins.

  // Particle Variables

  Particle.variable("celsius", temperatureString); // Setup Particle Variable
  Particle.variable("Release", releaseNumber);     // So we can see what release is running from the console
  Particle.variable("Signal", signalString);       // Particle variables that enable monitoring using the mobile app
  Particle.variable("Battery", batteryString);     // Battery level in V as the Argon does not have a fuel cell

  getTemperature();
  adaptiveMode();

  if (verboseMode)
    Particle.publish("State", "IDLE", PRIVATE);

  state = IDLE_STATE; // If we made it this far, we are ready to go to IDLE in the main loop
}

void loop()
{

  switch (state) // In the main loop, all code execution must take place in a defined state

  {
  case IDLE_STATE: // IDLE State.
    if (verboseMode && oldState != state)
      transitionState(); // If verboseMode is on and state is changed, Then publish the state transition.
    static unsigned long TimePassed = 0;
    if (Time.minute() - TimePassed >= refreshRate)
    {
      state = MEASURING_STATE;
      TimePassed = Time.minute();
    }
    break;

  case MEASURING_STATE:

    if (verboseMode && oldState != state)
      transitionState(); // If verboseMode is on and state is changed, Then publish the state transition.

    // Measuring State.
    getMeasurements(); // Get Measurements and Move to Reporting State if there is a change
    state = REPORTING_STATE;

    if (verboseMode)
    {
      waitUntil(PublishDelayFunction);
      Particle.publish("State", "Change detected - Reporting Determination ", PRIVATE);
    }

    else
    {
      state = IDLE_STATE;
      if (verboseMode)
      {
        waitUntil(PublishDelayFunction);
        Particle.publish("State", "No change - Idle", PRIVATE);
      }
    }
    break;

  case REPORTING_DETERMINATION: // Reporting determination state.
    static unsigned long adaptiveValueTimePassed = 0;

    if (verboseMode && oldState != state)
      transitionState(); // If verboseMode is on and state is changed, Then publish the state transition.

    if (Time.hour() != publishTimeHour) // Check if 60 minutes or 1 hr has passed.
    {
      state = REPORTING_STATE;
      if (verboseMode)
      {
        waitUntil(PublishDelayFunction);
        Particle.publish("State", "Time Passed - Reporting", PRIVATE); //Tells us that One Hour has passed.
      }
    }

    // AdaptiveMode
    if ((adaptiveModeOn) && (Time.minute() - adaptiveValueTimePassed > adaptiveReadingRate)) // Checks if adaptiveMode is ON and 5 minutes have passed from the last value.
    {
      state = REPORTING_STATE;
      adaptiveValueTimePassed = Time.minute();

      if (verboseMode)
      {
        waitUntil(PublishDelayFunction);
        Particle.publish("ADAPTIVE ON", "Next value in 5 minutes", PRIVATE);
      }
    }

    else
    {
      state = IDLE_STATE;
      if (verboseMode)
      {
        waitUntil(PublishDelayFunction);
        Particle.publish("State", "No change - Idle", PRIVATE);
      }
    }

    break;

  case REPORTING_STATE:
    if (verboseMode && oldState != state)
      transitionState(); // If verboseMode is on and state is changed, Then publish the state transition.

    if (Time.hour() == 12)
      Particle.syncTime(); // SET CLOCK EACH DAY AT 12 NOON.
    if (verboseMode)
    {
      waitUntil(PublishDelayFunction);
      Particle.publish("Temperature", temperatureString, PRIVATE);
      Particle.publish("State", "Waiting RESPONSE", PRIVATE);
    }
    sendUBIDots();
    state = RESPONSE_WAIT;

    break;

  case RESPONSE_WAIT:
    if (verboseMode && oldState != state)
      transitionState(); // If verboseMode is on and state is changed, Then publish the state transition.

    if (!inTransit)
    {
      Particle.publish("STATE", "Data Received, Going to IDLE"); // If data is not inTransit, Then data was sent succesfully, Hence go to Idle State.
      state = IDLE_STATE;
    } // This checks for the response from UBIDOTS.

    if (millis() - webhookTimeStamp > webhookTimeout)
    {                                                            // If device does not respond in 45 Seconds, Then Reset it.
      Particle.publish("spark/device/session/end", "", PRIVATE); //
      state = ERROR_STATE;                                       // time out
    }
    break;

  case ERROR_STATE: // This state RESETS the devices.
    if (verboseMode && oldState != state)
      transitionState(); // If verboseMode is on and state is changed, Then publish the state transition.
    Particle.publish("STATE", "RESETTING IN 30 SEC. ", PRIVATE);
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
  getSignalStrength(); // Get Signal Strength
  getBatteryCharge();  // Get Battery Charge Percentage
  if (getTemperature())
    return 1; // Read Temperature from Sensor
  else
    return 0; // Less than 1 degree difference detected
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
  if (abs(temperatureInC - lastTemperatureInC) >= 1)
  {
    lastTemperatureInC = temperatureInC;
    return 1;
  }
  else
    return 0;
}

// Function to Toggle VerboseMode.
bool SetVerboseMode(String command)
{

  if (command == "1" && verboseMode == false)
  {
    verboseMode = true;
    waitUntil(PublishDelayFunction);
    Particle.publish("Mode", "Verbose Mode Started.", PRIVATE);
    return 1;
  }

  if (command == "1" && verboseMode == true)
  {
    waitUntil(PublishDelayFunction);
    Particle.publish("Mode", "Verbose Mode Already ON.", PRIVATE);
    return 0;
  }

  if (command == "0" && verboseMode == true)
  {
    verboseMode = false;
    waitUntil(PublishDelayFunction);
    Particle.publish("Mode", "Verbose Mode Stopped.", PRIVATE);
    return 1;
  }

  if (command == "0" && verboseMode == false)
  {
    waitUntil(PublishDelayFunction);
    Particle.publish("Mode", "Verbose Mode already OFF.", PRIVATE);
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
  snprintf(data, sizeof(data), "{\"Temperature\":%3.1f, \"Battery\":%3.1f}", temperatureInC, voltage);
  Particle.publish("Air-Quality-Hook", data, PRIVATE);
  publishTime = Time.minute();
  publishTimeHour = Time.hour();
  webhookTimeStamp = millis();
  inTransit = true;
}

void UbidotsHandler(const char *event, const char *data) // Looks at the response from Ubidots - Will reset Photon if no successful response
{
  // Response Template: "{{hourly.0.status_code}}"
  if (!data)
  { // First check to see if there is any data
    Particle.publish("Ubidots Hook", "No Data", PRIVATE);
    return;
  }
  int responseCode = atoi(data); // Response is only a single number thanks to Template
  if ((responseCode == 200) || (responseCode == 201))
  {
    Particle.publish("State", "Response Received", PRIVATE);
    inTransit = false; // Data has been received
  }
  else
    Particle.publish("Ubidots Hook", data, PRIVATE); // Publish the response code
}

void transitionState(void)
{                                 // This function publishes change of state.
  char stateTransitionString[64]; // Declare a String to show state transition.
  snprintf(stateTransitionString, sizeof(stateTransitionString), "Transition: %s to %s", stateNames[oldState], stateNames[state]);
  oldState = state;
  Particle.publish("State", stateTransitionString, PRIVATE);
}

/* The forced Reading function takes in two arguments,
 If it is "1" then it turns the 
 forcedMode to true and sets a rate of 5 mins.  */
bool forcedReading(String Command)
{

  if (Command == "1")
  {
    state = REPORTING_STATE;
    forcedReadingRate = 5;
    forcedMode = true;
    Particle.publish("STATE", "Getting Value, Next Reading in 15 Mins.");
    return 1;
  }

  else if (Command == "0")
  {
    forcedMode = false;
    return 0;
  }
  return 0;
}

// Adaptive Mode Function

bool adaptiveMode()
{
  if ((Time.hour() == 5) || (Time.hour() == 6) || (Time.hour() == 0) || (Time.hour() == 1))
  {
    adaptiveModeOn = true;
    return 1;
  }

  else
  {
    adaptiveModeOn = false;
    return 0;
  }
}