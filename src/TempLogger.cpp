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
// v1.17 - Moved to deviceOS@1.4.0 and implemented low power fixes 
// v1.18 - Added some variables to EEPROM. 
// v1.19 - Fixed some issues with the memory map

void setup();
void loop();
bool takeMeasurements();
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
bool senseNow(String Command);
bool LowPowerMode(String Command);
#line 36 "/Users/abdulhannanmustajab/Desktop/Projects/IoT/Particle/tempLogger/TempLogger/src/TempLogger.ino"
const char releaseNumber[6] = "1.17"; // Displays the release on the menu

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

// Constants that will influence the timing of the Program - collected here to make it easier to edit them
const unsigned long webhookTimeout = 45000;                                       // Timeperiod to wait for a response from Ubidots before going to error State.
const int resetDelayTime = 30000;                                                 // How long to we wait before we reset in the error state
const int rapidSamplePeriodSeconds = 5*60;                                        // How often will we sample when there is a major change in temperature
const int normalSamplePeriodSeconds = 10*60;                                      // How often do we normally sample
const unsigned long stayAwakeShort = 5000;                                        // When we want to stay awake a short while after taking a sample measurements (make longer in development / shorter when deployed)
const unsigned long stayAwakeLong = 90000;                                        // We will stay awake longer at the hour  - makes it easier to update code  

// Variables Related To Program Control
int sampleRate = normalSamplePeriodSeconds;                                       // We start out sampling at the normal rate
unsigned long stayAwake;                                                          // Compare to tell us when to nap
float tempChangeThreshold = 1.5;                                                  // What is the difference in temp needed to trigger rapid sampling
unsigned long webhookTimeStamp = 0;                                               // Start timer for webhook timeout
unsigned long resetStartTimeStamp = 0;                                            // Start the clock on Reset
unsigned long stayAWakeTimeStamp;                                                 // How long till I can take a nap?
bool inTransit = false;                                                           // This variable is used to check if the data is inTransit to Ubidots or not. If inTransit is false, Then data is succesfully sent.
int currentHourlyPeriod = 0;                                                      // keep track of when the hour changes
bool lowPowerModeOn = true;                                                       // Important to set when the device is running on solar / battery power
int resetCount;                                                                   // Counts the number of times the Argon has had a pin reset

// Variables releated to the sensors
bool verboseMode = false;                                                         // Variable VerboseMode.
float temperatureInC = 0;                                                         // Current Temp Reading global variable
float voltage;                                                                    // Voltage level of the LiPo battery - 3.6-4.2V range

// Time Period and reporting Related Variables
time_t currentCountTime;                            // Global time vairable
byte currentMinutePeriod;                           // control timing when using 5-min samp intervals


// Define the memory map - note can be EEPROM or FRAM
namespace MEM_MAP {                                 // Moved to namespace instead of #define to limit scope
  enum Addresses {
    versionAddr           = 0x00,                    // Where we store the memory map version number - 8 Bits
    alertCountAddr        = 0x01,                    // Where we store our current alert count - 8 Bits
    resetCountAddr        = 0x02,                     // This is where we keep track of how often the Argon was reset - 8 Bits
    currentCountsTimeAddr = 0x03,                    // Time of last report - 32 bits
    sensorData1Object     = 0x07                     // The first data object - where we start writing data
   };
};

// Keypad struct for mapping buttons, notes, note values, LED array index, and default color
struct sensor_data_struct {                         // Here we define the structure for collecting and storing data from the sensors
  bool validData;
  unsigned long timeStamp;
  float batteryVoltage;
  float temperatureInC;
};

sensor_data_struct sensor_data;


#define MEMORYMAPVERSION 2                          // Lets us know if we need to reinitialize the memory map


void setup()
{
 Serial.begin(9600);
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

  if (MEMORYMAPVERSION != EEPROM.read(MEM_MAP::versionAddr)) {                    // Check to see if the memory map is the right version
      EEPROM.put(MEM_MAP::versionAddr,MEMORYMAPVERSION);
    for (int i=1; i < 100; i++) {
      EEPROM.put(i,0);                                                            // Zero out the memory - new map or new device
    }
  }

  resetCount = EEPROM.read(MEM_MAP::resetCountAddr);                              // Retrive system recount data from FRAM
  
  getTemperature();
  takeMeasurements();
  

  stayAwake = stayAwakeLong;                                                      // Stay awake longer on startup - helps with recovery for deployed devices
  stayAWakeTimeStamp = millis();                                                  // Reset the timestamp here as the connection sequence could take a while



  state = IDLE_STATE;                                                             // If we made it this far, we are ready to go to IDLE in the main loop
  if (verboseMode && oldState != state) transitionState();                        // If verboseMode is on and state is changed, Then publish the state transition.
}

void loop()
{  
  switch (state)  {                                                               // In the main loop, all code execution must take place in a defined state
    case IDLE_STATE:                                                              // IDLE State.
    {
      if (verboseMode && oldState != state) transitionState();                    // If verboseMode is on and state is changed, Then publish the state transition.
      static int TimePassed = 0;
      
      if (lowPowerModeOn && (millis() - stayAWakeTimeStamp >= stayAwake)) state = NAPPING_STATE;    // If lowPowerMode is turned on, It will move to the napping state. 
                                     
      if ((Time.minute() - TimePassed >= sampleRate/60) || Time.hour() != currentHourlyPeriod ) {     // Sample time or the top of the hour
          state = MEASURING_STATE;
          TimePassed = Time.minute();
      }
    } break;

    case MEASURING_STATE:                                                         // Measuring State.
      if (verboseMode && oldState != state) transitionState();                    // If verboseMode is on and state is changed, Then publish the state transition.
      currentHourlyPeriod = Time.hour();
      if (getMeasurements()) {
        takeMeasurements();
        state = REPORTING_DETERMINATION;                     // Get the measurements and move to reporting determination
      }
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
        stayAwake = stayAwakeLong;                                                // Stay awake longer at the hour - helps if you need to publish updates to deployed devices
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
        sampleRate = rapidSamplePeriodSeconds;                                    // Move to rapid sampling
        break;
      }
      else if (temperatureInC != lastTemperatureInC) {                            // Case 3 - smal change in Temp - report and normal sampling
        if (verboseMode) {
          waitUntil(PublishDelayFunction);
          Particle.publish("State", "Change detected - Reporting", PRIVATE);      // Report for diagnostics
        }
        lastTemperatureInC = temperatureInC;
        state = REPORTING_STATE;
        sampleRate = normalSamplePeriodSeconds;                                   // Small but non-zero change - move to normal sampling
        break;  
      }

      else {                                                                      // Case 4 - No change in temp - go back to idle
        if (verboseMode) {
          waitUntil(PublishDelayFunction);
          Particle.publish("State", "No Change - Idle", PRIVATE);                 // Report for diagnostics
        }
        state = IDLE_STATE;                                                      
        sampleRate = normalSamplePeriodSeconds;                                   // Small but non-zero change - move to normal sampling
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

    case NAPPING_STATE: { // This state puts the device to sleep mode
      char data[64];
      if (verboseMode && oldState != state) transitionState();                    // If verboseMode is on and state is changed, Then publish the state transition.

      stayAwake = stayAwakeShort;                                                 // Don't need to wake for long when we are just sampling
      int wakeInSeconds = constrain(sampleRate - Time.now() % sampleRate, 1, sampleRate); // Calculate the seconds to the next sample
      
      if (Particle.connected()) {
        snprintf(data,sizeof(data),"Going to take a %i second nap", wakeInSeconds);
        waitUntil(PublishDelayFunction);
        Particle.publish("Napping", data, PRIVATE);
      }
      
      System.sleep(D8, RISING, wakeInSeconds);                                    // This is a light sleep but all we can do until we put an external clock in
      Particle.connect();                                                         // We need to connect and transmit data each time - can move to sample and hold in the future
      Particle.publish("WokeUp","From Sleep",PRIVATE);
      stayAWakeTimeStamp = millis();                                              // Start the clock on how long we are awake
      state = IDLE_STATE;
    } break; 
  }
}


bool takeMeasurements() {
  
  // Mocked up here for the call - need to replace with your real readings
  int reportCycle;                                                    // Where are we in the sense and report cycle
  currentCountTime = Time.now();
  int currentMinutes = Time.minute();                                // So we only have to check once
  switch (currentMinutes) {
    case 15:
      reportCycle = 0;                                                // This is the first of the sample-only periods
      break;  
    case 30:
      reportCycle = 1;                                                // This is the second of the sample-only periods
      break; 
    case 45:
      reportCycle = 2;                                                // This is the third of the sample-only periods
      break; 
    case 0:
      reportCycle = 3;                                                // This is the fourth of the sample-only periods
      break; 
    default:
      reportCycle = 3;  
      break;                                                          // just in case
  }
  

  // Only gets marked true if we get all the measurements
  sensor_data.validData = false;

  // SoilMoisture Measurements here
  sensor_data.temperatureInC = temperatureInC;
  Serial.println(temperatureInC);
  Serial.println("TEMPERATURE IN C");

  snprintf(temperatureString,sizeof(temperatureString), "%4.1f %%", sensor_data.temperatureInC);
  
  Serial.println(sensor_data.temperatureInC);
  Serial.println("Temperature from takeMeasurements Function");


  // Get battery voltage level
  sensor_data.batteryVoltage = analogRead(BATT) * 0.0011224;                   // Voltage level of battery
  snprintf(batteryString, sizeof(batteryString), "%4.1f %%", sensor_data.batteryVoltage);
  



  // Indicate that this is a valid data array and store it
  sensor_data.validData = true;
  sensor_data.timeStamp = Time.now();
  EEPROM.put(7 + 100*reportCycle,sensor_data);                              // Current object is 72 bytes long - leaving some room for expansion

  return 1;                                                             // Done, measurements take and the data array is stored as an obeect in EEPROM                                         
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
  char data[512];

  for (int i = 0; i < 4; i++) {
    sensor_data = EEPROM.get(7 + i*100,sensor_data);                  // This spacing of the objects - 100 - must match what we put in the takeMeasurements() function
    snprintf(data, sizeof(data), "{\"Temperature\":%3.1f, \"Battery\":%3.1f}", sensor_data.temperatureInC, sensor_data.batteryVoltage);
    Particle.publish("Air-Quality-Hook", data, PRIVATE);
    waitUntil(PublishDelayFunction);                                  // Space out the sends
  }
  currentCountTime = Time.now();
  EEPROM.write(MEM_MAP::currentCountsTimeAddr, currentCountTime);
  webhookTimeStamp = millis();
  currentHourlyPeriod = Time.hour();
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
    inTransit = false;    
    EEPROM.write(MEM_MAP::currentCountsTimeAddr,Time.now());          // Record the last successful Webhook Response
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