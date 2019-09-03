/*
 * Project TempLogger
 * Description: Reading Temperature from OneWire 18B20 and sending it to particle cloud. 
 * Author: Abdul Hannan Mustajab
 * Date:
 */

/* 
      ***  Next Steps ***
      - Implement sleep 
      - Store some persisten variables in EEPROM
      - Add the CCS811 Sensor
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
// v1.15 - Added multiple tries for sensor read
// v1.16 - Added LowPowerMode

const char releaseNumber[6] = "1.16"; // Displays the release on the menu

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
  ERROR_STATE,
  NAPPING_STATE
};
// These are the allowed states in the main loop
char stateNames[8][44] = {"Initial State", "Idle State", "Measuring", "Reporting Determination", "Reporting", "Response Wait", "Error Wait", "Napping State"};
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
int currentHourlyPeriod = 0;                                                      // keep track of when the hour changes


// Variables releated to the sensors
bool verboseMode = false;                                                         // Variable VerboseMode.
float temperatureInC = 0;                                                         // Current Temp Reading global variable
float voltage;                                                                    // Voltage level of the LiPo battery - 3.6-4.2V range
bool lowPowerModeOn = false;                                                      // Variable to check the status of lowPowerMode. 
 

void setup()
{
  // This part receives Response using Particle.subscribe() and tells the response received from Ubidots.
  char responseTopic[125];
  String deviceID = System.deviceID();                                            // Multiple Particle devices share the same hook - keeps things straight
  deviceID.toCharArray(responseTopic, 125);
  Particle.subscribe(responseTopic, UbidotsHandler, MY_DEVICES);                  // Subscribe to the integration response event

  // Particle Functions.
  Particle.function("verboseMode", SetVerboseMode);                               // Added Particle Function For VerboseMode.
  Particle.function("Get-Reading", senseNow);                                     // This function will force it to get a reading and set the refresh rate to 15mins.
  Particle.function("Send-Report", sendNow);                                      // This function will force it to get a reading and set the refresh rate to 15mins.
  Particle.function("Low-Power-Mode", LowPowerMode);                              // This function will send the device to low power mode or napping.  
  
  // Particle Variables
  Particle.variable("Temperature", temperatureString);                            // Setup Particle Variable
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

      if (lowPowerModeOn) state = NAPPING_STATE;                                 // If lowPowerMode is turned on, It will move to the napping state. 

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
          Particle.publish("State", "Change detected - Reporting", PRIVATE);      // Report for diagnostics
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

      if (!inTransit) {
        state = IDLE_STATE;                                                       // This checks for the response from UBIDOTS. 
        if (!verboseMode) {                                                       // Abbreviated messaging for non-verbose mode
          waitUntil(PublishDelayFunction);
          Particle.publish("State", "Data Sent / Response Received", PRIVATE);    // Lets everyone know data was send successfully
        }
      } 

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

    case NAPPING_STATE: // This state puts the device to sleep mode
      if (verboseMode && oldState != state) transitionState();                    // If verboseMode is on and state is changed, Then publish the state transition.
      Particle.publish("Napping", "5 Minutes of Nap");
        System.sleep(30);  
        

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
  char data[32];
  for (int i=1; i <= 10; i++) {
    if (sensor.read()) {
      temperatureInC = sensor.celsius();
      snprintf(temperatureString, sizeof(temperatureString), "%3.1f Degrees C", temperatureInC);
      return 1;
    }
    Particle.process();                                                           // This could tie up the Argon making it unresponsive to Particle commands
    snprintf(data,sizeof(data),"Sensor Read Failed, attempt %i",i);
    waitUntil(PublishDelayFunction);                                              // Use this function to slow the reading of the sensor
    if (verboseMode) Particle.publish("Sensing",data,PRIVATE);                    // Send messages so we can see if sensor is mesbehaving
  }
  return 0;
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
    if (verboseMode) {
      waitUntil(PublishDelayFunction);
      Particle.publish("Ubidots Hook", "No Data", PRIVATE);
    }
    return;
  }
  int responseCode = atoi(data);                                                  // Response is only a single number thanks to Template
  if ((responseCode == 200) || (responseCode == 201))
  {
    if (verboseMode) {
      waitUntil(PublishDelayFunction);
      Particle.publish("State", "Response Received", PRIVATE);
    }
    inTransit = false;                                                            // Data has been received
  }
  else if (verboseMode) {
    waitUntil(PublishDelayFunction);      
    Particle.publish("Ubidots Hook", data, PRIVATE);                              // Publish the response code
  }
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
    Particle.publish("Function", "Command accepted - reporting now",PRIVATE);     // Acknowledge receipt
    return 1;
  }
  else if (Command == "0") {                                                      // No action required
    return 1;
  }
  return 0;
}

bool senseNow(String Command)                                                      // This command lets you force a reporting cycle
{
  if (Command == "1") {
    state = MEASURING_STATE;                                                      // Set the state to reporting
    waitUntil(PublishDelayFunction);  
    Particle.publish("Function", "Command accepted - sensing now",PRIVATE);       // Acknowledge receipt
    return 1;
  }
  else if (Command == "0") {                                                      // No action required
    return 1;
  }
  return 0;
}

bool LowPowerMode(String Command)
{
  if (Command == "1")
  {
    lowPowerModeOn = true;                                                         // This sets the lowPowerModeOn to true 
    return 1;
  }
  else if (Command == "0")
  {
    lowPowerModeOn = false;
    return 1;
  }
  else return 0;
}
