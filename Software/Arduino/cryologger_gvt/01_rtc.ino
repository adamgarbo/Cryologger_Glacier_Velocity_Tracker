// Configure the real-time clock (RTC)
void configureRtc()
{
  // Debugging only: Manually set the RTC date and time
  //rtc.setTime(23, 57, 30, 0, 25, 6, 23); // hour, minutes, seconds, hundredths, day, month, year
}

// Set initial RTC alarm(s)
void setInitialAlarm()
{
  // Alarm modes:
  // 0: Alarm interrupt disabled
  // 1: Alarm match hundredths, seconds, minutes, hour, day, month  (every year)
  // 2: Alarm match hundredths, seconds, minutes, hours, day        (every month)
  // 3: Alarm match hundredths, seconds, minutes, hours, weekday    (every week)
  // 4: Alarm match hundredths, seconds, minutes, hours             (every day)
  // 5: Alarm match hundredths, seconds, minutes                    (every hour)
  // 6: Alarm match hundredths, seconds                             (every minute)
  // 7: Alarm match hundredths                                      (every second)

  normalOperationMode = operationMode;

  // Re-check each alarm cycle if it's summer
  if (isSummer())
  {
    operationMode = 3; // Continuous logging mode
  }
  else
  {
    operationMode = normalOperationMode; // Default to normal user-selected mode
  }

  // Check for operation mode
  if (operationMode == 1) // Daily mode
  {
    // Set alarm mode
    alarmModeInitial = 4; // Alarm match hundredths, seconds, minutes, hours

    // Set initial alarm for specified hour
    rtc.setAlarm(alarmStartHour % 24, alarmStartMinute % 60, 0, 0, 0, 0);

    // Set the alarm mode
    rtc.setAlarmMode(alarmModeInitial);

    // Clear flag
    alarmFlag = false;
  }
  else if (operationMode == 2) // Rolling mode
  {
    // Set alarm mode
    alarmModeInitial = 5;  // Alarm match hundredths, seconds, minutes

    // Set initial alarm for rollover of next hour
    rtc.setAlarm(0, 0, 0, 0, 0, 0);

    // Set the alarm mode
    rtc.setAlarmMode(alarmModeInitial);

    // Clear flag
    alarmFlag = false;
  }
  else if (operationMode == 3) // Continuous mode
  {
    // Set alarm mode
    alarmModeInitial = 4; // Alarm match hundredths, seconds, minutes, hours

    // Set initial alarm for rollover of next day (00:00:00 UTC)
    rtc.setAlarm(0, 0, 0, 0, 0, 0);

    // Set the alarm mode
    rtc.setAlarmMode(alarmModeInitial);

    // Set flag
    alarmFlag = true;
  }

  // Attach alarm interrupt
  rtc.attachInterrupt();

  // Clear the RTC alarm interrupt
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);

  DEBUG_PRINT(F("Debug - Initial alarm mode: "));  DEBUG_PRINTLN(alarmModeInitial);
}

// Set logging alarm(s)
void setAwakeAlarm()
{
  // Clear the RTC alarm interrupt
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);

  // Re-check each alarm cycle if it's summer
  if (isSummer())
  {
    operationMode = 3; // Continuous logging mode
  }
  else
  {
    operationMode = normalOperationMode; // Default to normal user-selected mode
  }
  
  // Check for logging mode
  if (operationMode == 1) // Daily mode
  {
    DEBUG_PRINTLN(F("Info - Setting daily RTC logging alarm"));

    // Set alarm mode
    alarmModeLogging = 4; // Alarm match hundredths, seconds, minutes, hours

    // Set daily RTC alarm
    rtc.setAlarm(alarmStopHour, alarmStopMinute, 0, 0, 0, 0);

    // Set RTC alarm mode
    rtc.setAlarmMode(alarmModeLogging); // Alarm match on hundredths, seconds,  minutes, hours
  }
  else if (operationMode == 2) // Rolling mode
  {
    DEBUG_PRINTLN(F("Info - Setting daily RTC logging alarm"));

    // Adjust alarm mode based on duration of rolling alarm
    if (alarmAwakeHours > 0)
    {
      alarmModeLogging = 4; // Alarm match on hundredths, seconds, minutes, hours
    }
    else
    {
      alarmModeLogging = 5; // Alarm match on hundredths, seconds, minutes
    }
    // Set rolling RTC alarm
    rtc.setAlarm((rtc.hour + alarmAwakeHours) % 24, (rtc.minute + alarmAwakeMinutes) % 60, 0, 0, rtc.dayOfMonth, rtc.month);

    // Set RTC alarm mode
    rtc.setAlarmMode(alarmModeLogging);
  }
  else if (operationMode == 3) // Continuous mode
  {
    DEBUG_PRINTLN(F("Info - Continuous logging enabled"));
  }

  // Clear alarm flag
  alarmFlag = false;

  //DEBUG_PRINT(F("Debug - Logging alarm mode: "));  DEBUG_PRINTLN(alarmModeLogging);

  // Print the next RTC alarm date and time
  DEBUG_PRINT("Info - Logging until "); printAlarm();
}

// Set sleep alarm(s)
void setSleepAlarm()
{
  // Clear RTC alarm interrupt
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);

  // Check for logging mode
  if (operationMode == 1) // Daily mode
  {
    DEBUG_PRINTLN(F("Info - Setting daily RTC sleep alarm"));

    // Set alarm mode
    alarmModeSleep = 4; // Alarm match hundredths, seconds, minutes, hours

    // Set daily alarm
    rtc.setAlarm(alarmStartHour, alarmStartMinute, 0, 0, 0, 0);

    // Set RTC alarm mode
    rtc.setAlarmMode(alarmModeSleep); // Alarm match hundredths, seconds, minute, hours

    // Clear alarm flag
    alarmFlag = false;
  }
  else if (operationMode == 2) // Rolling mode
  {
    DEBUG_PRINTLN(F("Info - Setting rolling RTC alarm"));

    // Set rolling RTC alarm
    rtc.setAlarm((rtc.hour + alarmSleepHours) % 24, (rtc.minute + alarmSleepMinutes) % 60, 0, 0, rtc.dayOfMonth, rtc.month);

    // Adjust alarm mode based on duration of rolling alarm
    if (alarmSleepHours > 0)
    {
      alarmModeSleep = 4; // Alarm match on hundredths, seconds, minutes, hours
    }
    else
    {
      alarmModeSleep = 5; // Alarm match on hundredths, seconds, minutes
    }

    // Set RTC alarm mode
    rtc.setAlarmMode(alarmModeSleep);

    // Clear alarm flag
    alarmFlag = false;
  }
  else if (operationMode == 3) // Continuous mode
  {
    DEBUG_PRINTLN(F("Info - Continuous logging enabled. No sleep alarm set."));
    return;
  }

  // Print the next RTC alarm date and time
  DEBUG_PRINT("Info - Current time "); printDateTime();
  DEBUG_PRINT("Info - Sleeping until "); printAlarm();
}

// Read the RTC
void readRtc()
{
  // Start the loop timer
  unsigned long loopStartTime = micros();

  // Get the RTC's current date and time
  rtc.getTime();

  //DEBUG_PRINT("Info - Unix epoch time "); DEBUG_PRINTLN(rtc.getEpoch());

  // Stop the loop timer
  timer.rtc = micros() - loopStartTime;
}

// Get the RTC's date and time and store it in a buffer
void getDateTime()
{
  rtc.getTime(); // Get the RTC's date and time
  sprintf(dateTimeBuffer, "20%02d-%02d-%02d %02d:%02d:%02d",
          rtc.year, rtc.month, rtc.dayOfMonth,
          rtc.hour, rtc.minute, rtc.seconds, rtc.hundredths);
}

// Print the RTC's date and time
void printDateTime()
{
  rtc.getTime(); // Get the RTC's date and time
  sprintf(dateTimeBuffer, "20%02d-%02d-%02d %02d:%02d:%02d",
          rtc.year, rtc.month, rtc.dayOfMonth,
          rtc.hour, rtc.minute, rtc.seconds, rtc.hundredths);
  DEBUG_PRINTLN(dateTimeBuffer);
}

// Print the RTC's alarm
void printAlarm()
{
  rtc.getAlarm(); // Get the RTC's alarm date and time
  char alarmBuffer[25];
  sprintf(alarmBuffer, "20%02d-%02d-%02d %02d:%02d:%02d",
          rtc.year, rtc.alarmMonth, rtc.alarmDayOfMonth,
          rtc.alarmHour, rtc.alarmMinute, rtc.alarmSeconds, rtc.alarmHundredths);
  DEBUG_PRINTLN(alarmBuffer);
}

// Check RTC datetime
void checkDate()
{
  rtc.getTime(); // Get the RTC's date and time
  if (firstTimeFlag)
  {
    
    dateCurrent = rtc.dayOfMonth;
  }
  dateNew = rtc.dayOfMonth;
  Serial.print("Current date: "); Serial.print(dateCurrent);
  Serial.print(" New date: "); Serial.println(dateNew);
}

// Check for summer logging season
bool isSummer()
{
  rtc.getTime(); // Ensure RTC has the latest date and time

  // If the start and end are in the same month, check if the day falls within the range
  if (alarmSummerStartMonth == alarmSummerEndMonth)
  {
    return (rtc.month == alarmSummerStartMonth && 
            rtc.dayOfMonth >= alarmSummerStartDay && 
            rtc.dayOfMonth <= alarmSummerEndDay);
  }

  // General case: check month and day conditions
  return (rtc.month > alarmSummerStartMonth && rtc.month < alarmSummerEndMonth) ||
         (rtc.month == alarmSummerStartMonth && rtc.dayOfMonth >= alarmSummerStartDay) ||
         (rtc.month == alarmSummerEndMonth && rtc.dayOfMonth <= alarmSummerEndDay);
}

