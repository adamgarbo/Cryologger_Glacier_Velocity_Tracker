/*
    Title:    Cryologger - Glacier Velocity Tracker (GVT) v2.0.4
    Date:     March 23, 2022
    Author:   Adam Garbo

    Components:
    - SparkFun Artemis Processor
    - SparkFun MicroMod Data Logging Carrier Board
    - SparkFun GPS-RTK-SMA Breakout - ZED-F9P (Qwiic)

    Comments:
    - Code is currently configured for short-term deployments during the 
    2022 Arctic Bay field season.

    Dependencies:
    - Apollo3 Core v1.2.3
    - SparkFun u-blox GNSS Arduino Library v2.2.7
    - SdFat v2.1.2
*/

// ----------------------------------------------------------------------------
// Libraries
// ----------------------------------------------------------------------------
#include <RTC.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h> // https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library
#include <SdFat.h>                                // https://github.com/greiman/SdFat
#include <SPI.h>
#include <WDT.h>
#include <Wire.h>

// -----------------------------------------------------------------------------
// Debugging macros
// -----------------------------------------------------------------------------
#define DEBUG       true  // Output debug messages to Serial Monitor
#define DEBUG_GNSS  false  // Output GNSS information to Serial Monitor

#if DEBUG
#define DEBUG_PRINT(x)            Serial.print(x)
#define DEBUG_PRINTLN(x)          Serial.println(x)
#define DEBUG_PRINT_DEC(x, y)     Serial.print(x, y)
#define DEBUG_PRINTLN_DEC(x, y)   Serial.println(x, y)
#define DEBUG_WRITE(x)            Serial.write(x)

#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT_DEC(x, y)
#define DEBUG_PRINTLN_DEC(x, y)
#define DEBUG_WRITE(x)
#endif

// ----------------------------------------------------------------------------
// Pin definitions
// ----------------------------------------------------------------------------
#define PIN_PWC_POWER     33  // G1
#define PIN_QWIIC_POWER   34  // G2 
#define PIN_SD_CS         41  // CS

// ----------------------------------------------------------------------------
// Object instantiations
// ----------------------------------------------------------------------------
APM3_RTC          rtc;
APM3_WDT          wdt;
SdFs              sd;
FsFile            logFile;
FsFile            debugFile;
SFE_UBLOX_GNSS    gnss;       // I2C address: 0x42

// ----------------------------------------------------------------------------
// User defined logging/sleeping variables
// ----------------------------------------------------------------------------

// Logging mode
byte          loggingMode           = 2;    // 1 = daily, 2 = rolling

// Daily alarm
byte          loggingStartTime      = 19;   // Logging start hour (UTC)
byte          loggingStopTime       = 22;   // Logging end hour (UTC)

// Rolling alarm
byte          loggingAlarmMinutes   = 30;    // Rolling minutes alarm
byte          loggingAlarmHours     = 0;    // Rolling hours alarm
byte          sleepAlarmMinutes     = 30;    // Rolling minutes alarm
byte          sleepAlarmHours       = 0;    // Rolling hours alarm

// Alarm modes
byte          loggingAlarmMode      = 5;    // Logging alarm mode
byte          sleepAlarmMode        = 5;    // Sleep alarm mode
byte          initialAlarmMode      = 6;    // Initial alarm mode

// ----------------------------------------------------------------------------
// Global variable declarations
// ----------------------------------------------------------------------------
const int     sdWriteSize         = 512;          // Write data to SD in blocks of 512 bytes
const int     fileBufferSize      = 16384;        // Allocate 16 KB RAM for UBX message storage
volatile bool alarmFlag           = false;        // Flag for alarm interrupt service routine
volatile bool wdtFlag             = false;        // Flag for watchdog timer interrupt service routine
volatile int  wdtCounter          = 0;            // Counter for watchdog timer interrupts
volatile int  wdtCounterMax       = 0;            // Counter for max watchdog timer interrupts
bool          gnssConfigFlag      = true;         // Flag to indicate whether to configure the u-blox module
bool          rtcSyncFlag         = false;        // Flag to indicate if RTC has been synced with GNSS
char          logFileName[30]     = "";           // Log file name
char          debugFileName[20]   = "gvt_0_debug.csv";  // Debug log file name
unsigned int  debugCounter        = 0;            // Counter to track number of recorded debug messages
unsigned int  gnssTimeout         = 5;            // Timeout for GNSS signal acquisition (minutes)
unsigned int  maxBufferBytes      = 0;            // Maximum value of file buffer
unsigned long previousMillis      = 0;            // Global millis() timer
unsigned long bytesWritten        = 0;            // Counter for tracking bytes written to microSD
unsigned long syncFailCounter     = 0;            // microSD logfile synchronize failure counter
unsigned long writeFailCounter    = 0;            // microSD logfile write failure counter
unsigned long closeFailCounter    = 0;            // microSD logfile close failure counter
long          rtcDrift            = 0;            // Counter for drift of RTC
float         voltage             = 0.0;          // Battery voltage

// ----------------------------------------------------------------------------
// Unions/structures
// ----------------------------------------------------------------------------

// Union to store online/offline states
struct struct_online
{
  bool microSd  = false;
  bool gnss     = false;
  bool logGnss  = false;
  bool logDebug = false;
} online;

// Union to store loop timers
struct struct_timer
{
  unsigned long wdt;
  unsigned long rtc;
  unsigned long microSd;
  unsigned long voltage;
  unsigned long sensors;
  unsigned long gnss;
  unsigned long syncRtc;
  unsigned long logDebug;
  unsigned long logGnss;
} timer;

// ----------------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------------
void setup()
{
  // Set ADC to 14-bits
  analogReadResolution(14);
  
  // Pin assignments
  pinMode(PIN_QWIIC_POWER, OUTPUT);
  pinMode(PIN_PWC_POWER, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  qwiicPowerOn();         // Enable power to Qwiic connector
  peripheralPowerOn();    // Enable power to peripherials

  Wire.begin();           // Initalize I2C
  Wire.setPullups(0);     // Disable internal I2C pull-ups to help reduce bus errors
  Wire.setClock(400000);  // Set I2C clock speed to 400 kHz
  SPI.begin();            // Initialize SPI

#if DEBUG
  Serial.begin(115200);   // Open Serial port
  //while (!Serial);        // Wait for user to open Serial Monitor
  blinkLed(4, 1000);      // Delay to allow user to open Serial Monitor
#endif

  printLine();
  DEBUG_PRINTLN("Cryologger - Glacier Velocity Measurement Test Unit");
  printLine();

  printDateTime();      // Print RTC's current date and time
  readVoltage();        // Read battery voltage
  //DEBUG_PRINT("Battery voltage: "); DEBUG_PRINTLN(voltage);
  
  // Print logging configuration
  printLoggingSettings();

  // Configure devices
  configureWdt();         // Configure and start Watchdog Timer (WDT)
  configureGnss();        // Configure u-blox GNSS
  syncRtc();              // Acquire GNSS fix and sync RTC with GNSS
  configureSd();          // Configure microSD
  createDebugFile();      // Create debug log file
  setInitialAlarm();      // Configure RTC and set initial alarm

  DEBUG_PRINT("Info: Datetime "); printDateTime();
  DEBUG_PRINT("Info: Initial alarm "); printAlarm();

  // Blink LED to indicate completion of setup
  blinkLed(10, 100);
}

// ----------------------------------------------------------------------------
// Loop
// ----------------------------------------------------------------------------
void loop()
{
  // Check if alarm flag is set
  if (alarmFlag)
  {
    DEBUG_PRINT("Info: Alarm trigger "); printDateTime();

    // Configure logging
    readRtc();            // Get the RTC's alarm date and time
    setLoggingAlarm();    // Set logging alarm
    getLogFileName();     // Get timestamped log file name

    readVoltage();

    // Configure devices
    qwiicPowerOn();       // Enable power to Qwiic connector
    peripheralPowerOn();  // Enable power to peripherals
    configureSd();        // Configure microSD
    configureGnss();      // Configure u-blox GNSS
    syncRtc();            // Synchronize RTC

    // Log data
    logGnss();            // Log u-blox GNSS data
    logDebug();           // Log system debug information
    setSleepAlarm();      // Set sleep alarm

    printTimers();        // Log timers to debug file
  }

  // Check for watchdog interrupt
  if (wdtFlag)
  {
    petDog(); // Restart watchdog timer
  }

  // Blink LED
  blinkLed(1, 100);

  // Enter deep sleep
  goToSleep();
}

// ----------------------------------------------------------------------------
// Interupt Service Routines (ISR)
// ----------------------------------------------------------------------------

// Interrupt handler for the RTC
extern "C" void am_rtc_isr(void)
{
  // Clear the RTC alarm interrupt
  //rtc.clearInterrupt();
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);

  // Set alarm flag
  alarmFlag = true;
}

// Interrupt handler for the watchdog timer
extern "C" void am_watchdog_isr(void)
{
  // Clear the watchdog interrupt
  wdt.clear();

  // Perform system reset after 10 watchdog interrupts (should not occur)
  if (wdtCounter < 10)
  {
    wdt.restart(); // Restart the watchdog timer
  }

  wdtFlag = true; // Set the watchdog flag
  wdtCounter++; // Increment watchdog interrupt counter

  if (wdtCounter > wdtCounterMax)
  {
    wdtCounterMax = wdtCounter;
  }
}
