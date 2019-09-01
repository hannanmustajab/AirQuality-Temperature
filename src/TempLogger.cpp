/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "application.h"
#line 1 "/Users/chipmc/Documents/Maker/Particle/Projects/AirQuality-Temperature/src/TempLogger.ino"
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
// v1.14 - Fixed adaptive sampling 

/*
Comments for this fix
- No static needed for global variables

*/

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
bool sendNow(String Command);
#line 55 "/Users/chipmc/Documents/Maker/Particle/Projects/AirQuality-Temperature/src/TempLogger.ino"
const char releaseNumber[6] = "1.14"; // Displays the release on the menu

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
State state = INITIALIZATION_STATE;                                               // Initialize the state variable
State oldState = INITIALIZATION_STATE;                                            //Initialize the oldState Variable

// Variables Related To Particle Mobile Application Reporting
char signalString[16];                                                            // Used to communicate Wireless RSSI and Description
char temperatureString[16];                                                       // Temperature string for Reporting
char batteryString[16];                                                           // Battery value for reporting

// Variables Related To Program Control and Timing
const int rapidSamplePeriodMinutes = 5;                                           // How often will we sample when there is a major change in temperature
const int normalSamplePeriodMinutes = 10;                                         // How often do we normally sample
unsigned int sampleRate = normalSamplePeriodMinutes;                              // We start out sampling at the normal rate
float tempChangeThreshold = 1.5;                                                  // What is the difference in temp needed to trigger rapid sampling
const unsigned long webhookTimeout = 45000;                                       // Timeperiod to wait for a response from Ubidots before going to error State.
unsigned long webhookTimeStamp = 0;                                               // Start timer for webhook timeout
unsigned long resetStartTimeStamp = 0;                                            // Start the clock on Reset
const int resetDelayTime = 30000;                                                 // How long to we wait before we reset in the error state
bool inTransit = false;                                                           // This variable is used to check if the data is inTransit to Ubidots or not. If inTransit is false, Then data is succesfully sent.

// Variables releated to the sensors
bool verboseMode = true;                                                          // Variable VerboseMode.
float temperatureInC = 0;                                                         // Current Temp Reading global variable
float voltage;                                                                    // Voltage level of the LiPo battery - 3.6-4.2V range



void setup()
{
  pinMode(D3,INPUT);

  // This part receives Response using Particle.subscribe() and tells the response received from Ubidots.
  char responseTopic[125];
  String deviceID = System.deviceID();                                            // Multiple Particle devices share the same hook - keeps things straight
  deviceID.toCharArray(responseTopic, 125);
  Particle.subscribe(responseTopic, UbidotsHandler, MY_DEVICES);                  // Subscribe to the integration response event

  // Particle Functions.
  Particle.function("verboseMode", SetVerboseMode);                               // Added Particle Function For VerboseMode.
  Particle.function("GetReading", sendNow);                                 // This function will force it to get a reading and set the refresh rate to 15mins.

  // Particle Variables
  Particle.variable("celsius", temperatureString);                                // Setup Particle Variable
  Particle.variable("Release", releaseNumber);                                    // So we can see what release is running from the console
  Particle.variable("Signal", signalString);                                      // Particle variables that enable monitoring using the mobile app
  Particle.variable("Battery", batteryString);                                    // Battery level in V as the Argon does not have a fuel cell

  getTemperature();

  if (verboseMode)
    Particle.publish("State", "IDLE", PRIVATE);

  state = IDLE_STATE;                                                             // If we made it this far, we are ready to go to IDLE in the main loop
}

void loop()
{
  switch (state)  {                                                               // In the main loop, all code execution must take place in a defined state
    case IDLE_STATE:                                                              // IDLE State.
    {
      if (verboseMode && oldState != state) transitionState();                    // If verboseMode is on and state is changed, Then publish the state transition.
      static unsigned long TimePassed = 0;

      if ((Time.minute() - TimePassed >= sampleRate) || Time.minute()== 0) {     // Sample time or the top of the hour
        state = MEASURING_STATE;
        TimePassed = Time.minute();
      }
    } break;

    case MEASURING_STATE:                                                         // Measuring State.
      if (verboseMode && oldState != state) transitionState();                    // If verboseMode is on and state is changed, Then publish the state transition.
      if (getMeasurements()) state = REPORTING_DETERMINATION;                     // Get the measurements and move to reporting determination
      else  {
        resetStartTimeStamp = millis();
        state = ERROR_STATE;                                                      // If we fail to get the measurements we need - go to error state
      }
    break;

    case REPORTING_DETERMINATION:                                                 // Reporting determination state.
    {
      if (verboseMode && oldState != state) transitionState();                    // If verboseMode is on and state is changed, Then publish the state transition.
      static int currentHourlyPeriod = 0;                                         // keep track of when the hour changes
      static float lastTemperatureInC = 0;

      // Four possible outcomes: 1) Top of the hour - report, 2) Big change in Temp - report and move to rapid sampling, 3) small change in Temp - report and normal sampling, 4) No change in temp - back to Idle
      if (Time.hour() != currentHourlyPeriod) {                                   // Case 1 - If it is a new hour - report
        if (verboseMode) {
          waitUntil(PublishDelayFunction);
          Particle.publish("State", "New Hour- Reporting", PRIVATE);              // Report for diagnotics
        }
        currentHourlyPeriod = Time.hour();
        state = REPORTING_STATE;
        break;                                                                    // Leave this case and move on
      }
      if (abs(temperatureInC - lastTemperatureInC) >= tempChangeThreshold) {      // Case 2 - Big change in Temp - report and move to rapid sampling
        if (verboseMode) {
          waitUntil(PublishDelayFunction);
          Particle.publish("State", "Big Change - Rapid & Reporting", PRIVATE);   // Report for diagnostics
        }
        lastTemperatureInC = temperatureInC;
        state = REPORTING_STATE;                                                  
        sampleRate = rapidSamplePeriodMinutes;                                    // Move to rapid sampling
        break;
      }
      else if (temperatureInC != lastTemperatureInC) {                            // Case 3 - smal change in Temp - report and normal sampling
        if (verboseMode) {
          waitUntil(PublishDelayFunction);
          Particle.publish("State", "Change - Reporting", PRIVATE);               // Report for diagnostics
        }
        lastTemperatureInC = temperatureInC;
        state = REPORTING_STATE;
        sampleRate = normalSamplePeriodMinutes;                                   // Small but non-zero change - move to normal sampling
        break;  
      }
      else {                                                                      // Case 4 - No change in temp - go back to idle
        if (verboseMode) {
          waitUntil(PublishDelayFunction);
          Particle.publish("State", "No Change - Idle", PRIVATE);                 // Report for diagnostics
        }
        state = IDLE_STATE;                                                      
        sampleRate = normalSamplePeriodMinutes;                                   // Small but non-zero change - move to normal sampling
      }
    } break;

    case REPORTING_STATE:
      if (verboseMode && oldState != state) transitionState();                    // If verboseMode is on and state is changed, Then publish the state transition.

      if (Time.hour() == 12) Particle.syncTime();                                 // SET CLOCK EACH DAY AT 12 NOON.

      if (verboseMode) {
        waitUntil(PublishDelayFunction);
        Particle.publish("Temperature", temperatureString, PRIVATE);
      }
      sendUBIDots();
      state = RESPONSE_WAIT;
      break;

    case RESPONSE_WAIT:
      if (verboseMode && oldState != state) transitionState();                    // If verboseMode is on and state is changed, Then publish the state transition.

      if (!inTransit) state = IDLE_STATE;                                         // This checks for the response from UBIDOTS. 

      if (millis() - webhookTimeStamp > webhookTimeout) {                         // If device does not respond in 45 Seconds, Then Reset it.
        Particle.publish("spark/device/session/end", "", PRIVATE); 
        resetStartTimeStamp = millis();                                           // Start the reset clock
        state = ERROR_STATE;                                                      // Send to the error state - webhook failed
      }
      break;

    case ERROR_STATE: // This state RESETS the devices.
      if (verboseMode && oldState != state) transitionState();                    // If verboseMode is on and state is changed, Then publish the state transition.
      if (millis() - resetStartTimeStamp >= resetDelayTime) {
        waitUntil(PublishDelayFunction);
        Particle.publish("Error", "Resetting in 30 seconds", PRIVATE);            // Reset the device and hope that fixes it
        delay(2000);                                                              // Get the message out before resetting
        System.reset();
      }
      break;
  }
}

// Function to create a delay in the publish time
bool PublishDelayFunction() {
  static unsigned long tstamp = 0;
  if (millis() - tstamp <= 1000)                                                  // Particle limits webhooks and publishes to once every second
    return 0;
  else {
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
  getSignalStrength();                                                            // Get Signal Strength
  getBatteryCharge();                                                             // Get Battery Charge Percentage
  if (getTemperature()) return 1;                                                 // Read Temperature from Sensor
  else return 0;                                                                  // Less than 1 degree difference detected
}


bool getTemperature() {                                                           // Function to get temperature value from DS18B20.
  if (sensor.read()) {
    temperatureInC = sensor.celsius();
    snprintf(temperatureString, sizeof(temperatureString), "%3.1f Degrees C", temperatureInC);
    return 1;
  }
  else return 0;
}


bool SetVerboseMode(String command) {                                             // Function to Toggle VerboseMode.
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
  else return 0;
}

void sendUBIDots()                                                                // Function that sends the JSON payload to Ubidots
{
  char data[256];
  snprintf(data, sizeof(data), "{\"Temperature\":%3.1f, \"Battery\":%3.1f}", temperatureInC, voltage);
  Particle.publish("Air-Quality-Hook", data, PRIVATE);
  webhookTimeStamp = millis();
  inTransit = true;
}

void UbidotsHandler(const char *event, const char *data)                          // Looks at the response from Ubidots - Will reset Photon if no successful response
{
  // Response Template: "{{hourly.0.status_code}}"
  if (!data) {                                                                    // First check to see if there is any data
    Particle.publish("Ubidots Hook", "No Data", PRIVATE);
    return;
  }
  int responseCode = atoi(data);                                                  // Response is only a single number thanks to Template
  if ((responseCode == 200) || (responseCode == 201))
  {
    Particle.publish("State", "Response Received", PRIVATE);
    inTransit = false;                                                            // Data has been received
  }
  else
    Particle.publish("Ubidots Hook", data, PRIVATE);                              // Publish the response code
}

void transitionState(void) {                                                      // This function publishes change of state.
  waitUntil(PublishDelayFunction);
  char stateTransitionString[64];                                                 // Declare a String to show state transition.
  snprintf(stateTransitionString, sizeof(stateTransitionString), "Transition: %s to %s", stateNames[oldState], stateNames[state]);
  oldState = state;
  Particle.publish("State", stateTransitionString, PRIVATE);
}


bool sendNow(String Command)                                                      // This command lets you force a reporting cycle
{
  if (Command == "1") {
    state = REPORTING_STATE;                                                      // Set the state to reporting
    waitUntil(PublishDelayFunction);  
    Particle.publish("Function", "Command accepted - sending now",PRIVATE);               // Acknowledge receipt
    return 1;
  }
  else if (Command == "0") {                                                      // No action required
    return 1;
  }
  return 0;
}
