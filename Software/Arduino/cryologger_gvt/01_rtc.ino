/*
  RTC Module

  This module configures the real-time clock (RTC), sets alarms, and manages 
  timekeeping. It handles scheduled logging, sleep cycles, and periodic 
  synchronization with GNSS timestamps.

  ----------------------------------------------------------------------------
  Alarm Modes:
  ----------------------------------------------------------------------------
  0: Alarm interrupt disabled
  1: Alarm match hundredths, seconds, minutes, hour, day, month  (every year)
  2: Alarm match hundredths, seconds, minutes, hours, day        (every month)
  3: Alarm match hundredths, seconds, minutes, hours, weekday    (every week)
  4: Alarm match hundredths, seconds, minutes, hours             (every day)
  5: Alarm match hundredths, seconds, minutes                    (every hour)
  6: Alarm match hundredths, seconds                             (every minute)
  7: Alarm match hundredths                                      (every second)
*/

// Configure the real-time clock (RTC)
//
// Initializes the RTC and sets the date/time manually for debugging purposes.
void configureRtc() {
  
  rtc.setTime(23, 48, 30, 0, 31, 5, 25);  // Manually set the RTC date/time for debugging
}

// Set the initial RTC alarm.
//
// Determines the initial alarm configuration based on the selected operation mode.
// This function also verifies whether summer mode is active.
void setInitialAlarm() {
  // Store the current operation mode
  normalOperationMode = operationMode;

  // Check and update the operation mode (e.g., enable summer mode if applicable)
  checkOperationMode();

  // Configure the RTC alarm based on the operation mode using switch-case
  switch (operationMode) {
    case DAILY:
      alarmModeInitial = 4;
      rtc.setAlarm(alarmStartHour % 24, alarmStartMinute % 60, 0, 0, 0, 0);
      break;

    case ROLLING:
      alarmModeInitial = 5;
      rtc.setAlarm(0, 0, 0, 0, 0, 0);
      break;

    case CONTINUOUS:
      alarmModeInitial = 4;
      rtc.setAlarm(0, 0, 0, 0, 0, 0);
      alarmFlag = true;  // Enable logging immediately
      break;
  }

  // Apply the configured alarm mode
  rtc.setAlarmMode(alarmModeInitial);
  rtc.attachInterrupt();
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);

  DEBUG_PRINT(F("[RTC] Info: Initial alarm mode: "));
  DEBUG_PRINTLN(alarmModeInitial);
}

// Set the RTC logging alarm.
//
// Defines when the next logging session should occur based on the current
// operation mode. Adjusts alarm behavior for summer logging.
void setAwakeAlarm() {
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);  // Clear any pending RTC alarm interrupts

  checkOperationMode();  // Re-evaluate operation mode for potential summer logging adjustments

  switch (operationMode) {
    case DAILY:
      DEBUG_PRINTLN(F("[RTC] Info: Setting daily logging alarm."));
      alarmModeLogging = 4;
      rtc.setAlarm(alarmStopHour, alarmStopMinute, 0, 0, 0, 0);
      break;

    case ROLLING:
      DEBUG_PRINTLN(F("[RTC] Info: Setting rolling logging alarm."));
      alarmModeLogging = (alarmAwakeHours > 0) ? 4 : 5;
      rtc.setAlarm((rtc.hour + alarmAwakeHours) % 24,
                   (rtc.minute + alarmAwakeMinutes) % 60,
                   0, 0, rtc.dayOfMonth, rtc.month);
      break;

    case CONTINUOUS:
      DEBUG_PRINTLN(F("[RTC] Info: Continuous logging enabled."));
      break;
  }

  rtc.setAlarmMode(alarmModeLogging);
  alarmFlag = false;

  DEBUG_PRINT(F("[RTC] Info: Logging until "));
  printAlarm();
}

// Set the RTC sleep alarm.
//
// Defines when the device should wake from sleep based on the logging mode.
// Ensures that the system conserves power efficiently between logging cycles.
void setSleepAlarm() {
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);  // Clear any pending RTC alarm interrupts

  checkOperationMode();  // Re-evaluate operation mode for potential summer logging adjustments

  switch (operationMode) {
    case DAILY:
      DEBUG_PRINTLN(F("[RTC] Info: Setting daily sleep alarm."));
      alarmModeSleep = 4;
      rtc.setAlarm(alarmStartHour, alarmStartMinute, 0, 0, 0, 0);
      break;

    case ROLLING:
      DEBUG_PRINTLN(F("[RTC] Info: Setting rolling sleep alarm."));
      rtc.setAlarm((rtc.hour + alarmSleepHours) % 24,
                   (rtc.minute + alarmSleepMinutes) % 60,
                   0, 0, rtc.dayOfMonth, rtc.month);
      alarmModeSleep = (alarmSleepHours > 0) ? 4 : 5;
      break;

    case CONTINUOUS:
      DEBUG_PRINTLN(F("[RTC] Info: Continuous logging enabled. No sleep alarm set."));
      return;
  }

  rtc.setAlarmMode(alarmModeSleep);

  DEBUG_PRINT(F("[RTC] Info: Sleeping until "));
  printAlarm();
}

// Read the RTC time.
//
// Retrieves the current time from the RTC and records the time taken
// for profiling purposes.
void readRtc() {
  unsigned long loopStartTime = micros();
  rtc.getTime();
  timer.rtc = micros() - loopStartTime;
}

// Get RTC date and time.
//
// Reads the RTC's date and time and stores it in a buffer.
void getDateTime() {
  rtc.getTime();
  sprintf(dateTimeBuffer, "20%02d-%02d-%02d %02d:%02d:%02d",
          rtc.year, rtc.month, rtc.dayOfMonth,
          rtc.hour, rtc.minute, rtc.seconds);
}

// Print the RTC date and time.
//
// Formats the RTC's date and time into a readable string for debugging output.
void printDateTime() {
  rtc.getTime();
  sprintf(dateTimeBuffer, "20%02d-%02d-%02d %02d:%02d:%02d.%02d",
          rtc.year, rtc.month, rtc.dayOfMonth,
          rtc.hour, rtc.minute, rtc.seconds, rtc.hundredths);
  DEBUG_PRINTLN(dateTimeBuffer);
}

// Print the scheduled RTC alarm.
//
// Retrieves and prints the configured alarm time in a readable format.
void printAlarm() {
  rtc.getAlarm();
  char alarmBuffer[30];
  sprintf(alarmBuffer, "20%02d-%02d-%02d %02d:%02d:%02d.%02d",
          rtc.year, rtc.alarmMonth, rtc.alarmDayOfMonth,
          rtc.alarmHour, rtc.alarmMinute, rtc.alarmSeconds, rtc.alarmHundredths);
  DEBUG_PRINTLN(alarmBuffer);
}

// Check the RTC date.
//
// Compares the stored date with the RTC’s current date to detect daily changes.
void checkDate() {
  rtc.getTime();  // Retrieve the current RTC date and time

  if (firstTimeFlag) {
    dateCurrent = rtc.dayOfMonth;
  }

  dateNew = rtc.dayOfMonth;

  DEBUG_PRINT(F("[RTC] Current date: "));
  DEBUG_PRINT(dateCurrent);
  DEBUG_PRINT(F(" New date: "));
  DEBUG_PRINTLN(dateNew);
}

// Determine if it is currently summer.
//
// Checks whether the current date falls within the defined summer logging period.
bool isSummer() {
  rtc.getTime();  // Retrieve the current RTC date

  int currentMD = (rtc.month * 100) + rtc.dayOfMonth;
  int startMD = (alarmSummerStartMonth * 100) + alarmSummerStartDay;
  int endMD = (alarmSummerEndMonth * 100) + alarmSummerEndDay;
  if (summerMode) {
    DEBUG_PRINT(F("[RTC] Info: Summer period Start "));
    DEBUG_PRINT(startMD);
    DEBUG_PRINT(F(" | End "));
    DEBUG_PRINTLN(endMD);
  }
  return (currentMD >= startMD && currentMD <= endMD);
}

// Check and update the operation mode.
//
// Evaluates if summer logging mode should be activated and adjusts
// the operation mode accordingly.
void checkOperationMode() {
  DEBUG_PRINTLN(F("[RTC] Info: Checking operation mode..."));
  if (summerMode && isSummer()) {
    operationMode = CONTINUOUS;
    DEBUG_PRINT(F("[RTC] Info: Summer logging mode enabled: "));
    DEBUG_PRINTLN(operationMode);
  } else {
    operationMode = normalOperationMode;
    DEBUG_PRINT(F("[RTC] Info: Normal operation mode enabled: "));
    DEBUG_PRINTLN(operationMode);
  }
}
