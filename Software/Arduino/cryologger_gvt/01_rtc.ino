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
  rtc.setTime(23, 57, 30, 0, 31, 8, 25);  // Format is hour, minutes, seconds, hundredths, day, month, year
}

// Set the initial RTC alarm.
//
// Determines the initial alarm configuration based on the selected operation mode.
void setInitialAlarm() {
  normalOperationMode = operationMode;  // Store the current operation mode

  checkOperationMode();  // Evaluate if summer mode should be enabled

  switch (operationMode) {
    case DAILY:
      DEBUG_PRINTLN(F("[RTC] Info: Setting initial daily logging alarm."));
      alarmModeInitial = 4;
      rtc.setAlarm(alarmStartHour, alarmStartMinute, 0, 0, rtc.dayOfMonth, rtc.month);
      break;

    case ROLLING:
      DEBUG_PRINTLN(F("[RTC] Info: Setting initial rolling logging alarm."));
      alarmModeInitial = 5;
      rtc.setAlarm(0, 0, 0, 0, rtc.dayOfMonth, rtc.month);
      break;

    case CONTINUOUS:
      rtc.setAlarm(0, 0, 0, 0, 0, 0);
      DEBUG_PRINTLN(F("[RTC] Info: Continuous logging mode active. No alarm set."));
      alarmModeInitial = 4;
      alarmFlag = true;  // Start logging immediately
      break;
  }

  rtc.setAlarmMode(alarmModeInitial);
  rtc.attachInterrupt();
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);

  DEBUG_PRINT(F("[RTC] Info: Initial alarm mode = "));
  DEBUG_PRINTLN(alarmModeInitial);
}

// Set the RTC logging alarm.
//
// Ensures normal logging behavior while properly transitioning to summer mode if applicable.
void setAwakeAlarm() {
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);  // Clear any pending RTC alarm interrupts

  checkOperationMode();  // Determine the correct operation mode

  switch (operationMode) {
    case DAILY:
      // If today is the last day before summer mode and logging is finished, do NOT set another awake alarm.
      if (summerMode && isLastDayBeforeSummer() && rtc.hour >= alarmStopHour && rtc.minute >= alarmStopMinute) {
        DEBUG_PRINTLN(F("[RTC] Info: Last logging session complete. No new logging alarms before summer mode."));
        return;  // Let `setSleepAlarm()` handle the 00:00 wake-up alarm.
      }

      // Otherwise, set the normal daily logging alarm
      DEBUG_PRINTLN(F("[RTC] Info: Setting daily logging alarm."));
      alarmModeLogging = 4;
      rtc.setAlarm(alarmStopHour, alarmStopMinute, 0, 0, rtc.dayOfMonth, rtc.month);
      break;

    case ROLLING:
      DEBUG_PRINTLN(F("[RTC] Info: Setting rolling logging alarm."));
      alarmModeLogging = (alarmAwakeHours > 0) ? 4 : 5;
      rtc.setAlarm((rtc.hour + alarmAwakeHours) % 24,
                   (rtc.minute + alarmAwakeMinutes) % 60,
                   0, 0, rtc.dayOfMonth, rtc.month);
      break;

    case CONTINUOUS:
      DEBUG_PRINTLN(F("[RTC] Info: Continuous logging mode active. No new logging alarms required."));
      break;
  }

  rtc.setAlarmMode(alarmModeLogging);
  alarmFlag = false;

  DEBUG_PRINT(F("[RTC] Info: Logging until "));
  printAlarm();
}

// Set the RTC sleep alarm.
//
// Ensures normal sleep cycles while handling the summer mode transition correctly.
void setSleepAlarm() {
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);  // Clear any pending RTC alarm interrupts
  checkOperationMode();                      // Ensure we evaluate the correct logging mode

  switch (operationMode) {
    case DAILY:
      // If today is the last day before summer mode and logging is finished, set alarm for 00:00
      if (summerMode && isLastDayBeforeSummer() && rtc.hour >= alarmStopHour && rtc.minute >= alarmStopMinute) {
        rtc.setAlarm(0, 0, 0, 0, alarmSummerStartDay, alarmSummerStartMonth);
        DEBUG_PRINTLN(F("[RTC] Info: Last sleep before summer mode. Sleep alarm set for 00:00 transition."));
        alarmModeSleep = 4;
      } else {
        // Otherwise, set the normal daily sleep alarm
        rtc.setAlarm(alarmStartHour, alarmStartMinute, 0, 0, rtc.dayOfMonth, rtc.month);
        DEBUG_PRINTLN(F("[RTC] Info: Setting normal daily sleep alarm."));
        alarmModeSleep = 4;
      }
      break;

    case ROLLING:
      rtc.setAlarm((rtc.hour + alarmSleepHours) % 24,
                   (rtc.minute + alarmSleepMinutes) % 60,
                   0, 0, rtc.dayOfMonth, rtc.month);
      alarmModeSleep = (alarmSleepHours > 0) ? 4 : 5;
      DEBUG_PRINTLN(F("[RTC] Info: Setting rolling sleep alarm."));
      break;

    case CONTINUOUS:
      DEBUG_PRINTLN(F("[RTC] Info: Continuous logging mode active. No sleep alarm required."));
      break;
  }

  rtc.setAlarmMode(alarmModeSleep);
  alarmFlag = false;
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

  DEBUG_PRINT(F("[RTC] Info: Current date: "));
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

  DEBUG_PRINT(F("[RTC] Info: Current date = "));
  DEBUG_PRINTLN(currentMD);

  DEBUG_PRINT(F("[RTC] Info: Summer logging period = "));
  DEBUG_PRINT(startMD);
  DEBUG_PRINT(F("-"));
  DEBUG_PRINTLN(endMD);

  return (currentMD >= startMD && currentMD <= endMD);
}

// Check if today is the last day before summer logging starts.
bool isLastDayBeforeSummer() {
  rtc.getTime();
  if (alarmSummerStartDay == 1) {
    return (rtc.dayOfMonth == getLastDayOfMonth(rtc.month, rtc.year) && rtc.month == (alarmSummerStartMonth - 1));
  }
  return (rtc.dayOfMonth == (alarmSummerStartDay - 1) && rtc.month == alarmSummerStartMonth);
}
// Check and update the operation mode.
//
// This function determines the appropriate operation mode but does **not** set any alarms.
// Alarm scheduling is handled separately in `setAwakeAlarm()`.
void checkOperationMode() {
  DEBUG_PRINTLN(F("[RTC] Info: Checking operation mode..."));
  rtc.getTime();

  // If summer mode is enabled and the current date is within the summer period,
  // activate continuous logging mode.
  if (summerMode && isSummer()) {
    operationMode = CONTINUOUS;
    DEBUG_PRINTLN(F("[RTC] Info: Summer logging period detected. Switching to continuous logging mode."));
    return;
  }

  // Otherwise, keep the normal operation mode.
  operationMode = normalOperationMode;
  DEBUG_PRINT(F("[RTC] Info: Normal operation mode remains active: "));
  DEBUG_PRINTLN(operationMode);
}

// Returns the last day of a given month (handles leap years)
int getLastDayOfMonth(int month, int year) {
  if (month == 2) return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  return (month == 4 || month == 6 || month == 9 || month == 11) ? 30 : 31;
}
