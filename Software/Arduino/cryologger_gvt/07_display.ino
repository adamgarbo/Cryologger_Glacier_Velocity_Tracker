// Configure OLED display
void configureOled()
{
  // Enable internal I2C pull-ups
  enablePullups();

  // Initalize the OLED device and related graphics system
  if (!oled.begin())
  {
    online.oled = false;
    DEBUG_PRINTLN("Warning - OLED failed to initialize. Reattempting...");

    // Delay between initialization attempts
    myDelay(2000);

    if (!oled.begin())
    {
      online.oled = false;
      DEBUG_PRINTLN("Warning - OLED failed to initialize.");
    }
    else
    {
      lineTest();
      online.oled = true;
    }
  }
  else
  {
    lineTest();
    online.oled = true;
  }

  // Disable internal I2C pull-ups
  disablePullups();
}

// Reset OLED display after sleep/power cycle
void resetOled()
{
#if OLED
  myDelay(4000);
  online.oled = true;
  enablePullups();
  oled.reset(1);
#endif
}

void displayWelcome()
{
  if (online.oled)
  {
    enablePullups(); // Enable internal I2C pull-ups
    oled.erase();
    oled.setCursor(0, 0);
    oled.print("Cryologger GVT ");
    //oled.print(SERIAL);
    oled.setCursor(0, 10);
    oled.print(dateTimeBuffer);
    oled.setCursor(0, 20);
    oled.print("Voltage:");
    oled.setCursor(54, 20);
    oled.print(readBattery(), 2);
    oled.display();
    disablePullups();
    myDelay(4000);
  }
}

void displayInitialize(char *device)
{
  if (online.oled)
  {
    enablePullups();
    char displayBuffer[24];
    sprintf(displayBuffer, "Initialize %s...", device);
    oled.erase();
    oled.text(0, 0, displayBuffer);
    oled.display();
    disablePullups();
  }
}

void displaySuccess()
{
  if (online.oled)
  {
    enablePullups();
    //oled.erase();
    oled.text(0, 10, "Success!");
    oled.display();
    disablePullups();
    myDelay(2000);
  }
}

void displayFailure()
{
  if (online.oled)
  {
    enablePullups(); // Enable internal I2C pull-ups
    oled.text(0, 10, "Failed!");
    oled.display();
    disablePullups();
  }
}

void displayReattempt()
{
  if (online.oled)
  {
    enablePullups();
    oled.text(0, 10, "Failed! Reattempting...");
    oled.display();
    disablePullups();
  }
}

void displaySetupComplete()
{
  if (online.oled)
  {
    enablePullups();
    oled.erase();
    oled.text(0, 0, "Setup complete!");
    oled.display();
    disablePullups();
    myDelay(2000);
  }
}

void displayLoggingMode()
{
  if (online.oled)
  {
    enablePullups();
    oled.erase();
    oled.setCursor(0, 0);
    oled.print("Log Mode: ");
    if (operationMode == 1)
    {
      char displayBuffer1[32];
      char displayBuffer2[32];
      sprintf(displayBuffer1, "Start time: %d:%d", alarmStartHour, alarmStartMinute);
      sprintf(displayBuffer2, "End time: %d:%d", alarmStopHour, alarmStopMinute);
      oled.print("Daily");
      oled.text(0, 10, displayBuffer1);
      oled.text(0, 20, displayBuffer2);
    }
    else if (operationMode == 2)
    {
      char displayBuffer1[32];
      char displayBuffer2[32];
      sprintf(displayBuffer1, "Log: %02d hrs %02d min", alarmAwakeHours, alarmAwakeMinutes);
      sprintf(displayBuffer2, "Sleep: %02d hrs %02d min", alarmSleepHours, alarmSleepMinutes);

      oled.print("Rolling");
      oled.text(0, 10, displayBuffer1);
      oled.text(0, 20, displayBuffer2);
    }
    else if (operationMode == 3)
    {
      oled.print("Continuous");
    }
    else
    {
      oled.print("Not specified!");
    }
    oled.display();
    disablePullups();
    myDelay(8000);
  }
}

void displayRtcSyncStatus()
{
  if (online.oled)
  {
    enablePullups();
    oled.erase();
    oled.setCursor(0, 0);
    oled.print("Syncing RTC/GNSS...");
    oled.setCursor(0, 10);
    oled.print("Sat:");
    oled.setCursor(35, 10);
    oled.print(gnss.getSIV());
    oled.setCursor(55, 10);
    oled.print("Fix:");
    oled.setCursor(90, 10);
    oled.print(gnss.getFixType());
    oled.setCursor(0, 20);
    oled.print("Date:");
    oled.setCursor(35, 20);
    oled.print(gnss.getConfirmedDate());
    oled.setCursor(55, 20);
    oled.print("Time:");
    oled.setCursor(90, 20);
    oled.print(gnss.getConfirmedTime());
    oled.display();
    disablePullups();
  }
}

void displayRtcFailure()
{
  if (online.oled)
  {
    enablePullups();
    oled.erase();
    oled.text(0, 0, "Warning - RTC sync failed!");
    oled.display();
    disablePullups();
    myDelay(2000);
  }
}

void displayRtcOffset(long drift)
{
  if (online.oled && firstTimeFlag)
  {
    // Get current date and time
    getDateTime();
    enablePullups();
    oled.erase();
    oled.setCursor(0, 0);
    oled.print("RTC sync success!");
    oled.setCursor(0, 10);
    oled.print(dateTimeBuffer);
    oled.display();
    myDelay(4000);
    oled.erase();
    oled.setCursor(0, 0);
    oled.print("RTC drift (s): ");
    oled.setCursor(0, 10);
    oled.print(drift);
    oled.display();
    disablePullups();
    myDelay(2000);
  }
}

void displayErrorMicrosd1()
{
  enablePullups();
  oled.erase();
  oled.text(0, 0, "Error: microSD");
  oled.text(0, 10, "failed to initialize!");
  oled.text(0, 20, "Reattempting...");
  oled.display();
  disablePullups();
  myDelay(4000);
}

void displayErrorMicrosd2()
{
  enablePullups();
  oled.erase();
  oled.text(0, 0, "Error: microSD");
  oled.text(0, 10, "second attempt failed!");
  oled.display();
  disablePullups();
  myDelay(4000);
}

void displayScreen1()
{
  if (online.oled)
  {
    enablePullups();
    char displayBuffer1[32];
    char displayBuffer2[32];
    sprintf(displayBuffer1, "File size: %d", (bytesWritten / 1024));
    sprintf(displayBuffer2, "Max buffer: %d", maxBufferBytes);

    oled.erase();
    oled.text(0, 0, logFileName);
    oled.text(0, 10, displayBuffer1);
    oled.text(0, 20, displayBuffer2);
    oled.display();
    disablePullups();
  }
}

void displayScreen2()
{
  if (online.oled)
  {
    // Get current date and time
    getDateTime();

    enablePullups();
    oled.erase();
    oled.setCursor(0, 0);
    oled.print(dateTimeBuffer);
    oled.setCursor(0, 10);
    oled.print("Voltage:");
    oled.setCursor(54, 10);
    oled.print(readBattery(), 2);
    //oled.setCursor(90, 10);
    //oled.print(reading);
    oled.setCursor(0, 20);
    oled.print("Duration:");
    oled.setCursor(60, 20);
    oled.print((rtc.getEpoch() - logStartTime));
    oled.display();
    disablePullups();
  }
}

void displayDeepSleep()
{
  if (online.oled)
  {
    enablePullups();
    oled.erase();
    oled.text(0, 0, "Entering deep sleep...");
    oled.display();
    disablePullups();
    myDelay(4000);
  }
}

void displayOff()
{
  if (online.oled)
  {
    oled.displayPower(1);
  }
}

void displayOn()
{
  if (online.oled)
  {
    oled.displayPower(0);
  }
}
void lineTest(void)
{
  int width = oled.getWidth();
  int height = oled.getHeight();

  for (int i = 0; i < width; i += 6)
  {
    oled.line(0, 0, i, height - 1);
    oled.display();
  }
  myDelay(2000);
  oled.erase();
  for (int i = width - 1; i >= 0; i -= 6)
  {
    oled.line(width - 1, 0, i, height - 1);
    oled.display();
  }
  myDelay(1000);
}
