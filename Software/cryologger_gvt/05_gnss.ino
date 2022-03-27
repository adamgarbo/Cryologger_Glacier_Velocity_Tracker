// Configure u-blox GNSS
void configureGnss()
{
  // Start loop timer
  unsigned long loopStartTime = millis();

  // Check if u-blox has been initialized
  if (!online.gnss)
  {
    // Disable internal I2C pull-ups to help reduce bus errors
    //Wire.setPullups(0);

    // Uncomment  line to enable GNSS debug messages on Serial
    //gnss.enableDebugging();

    // Disable the "7F" check in checkUbloxI2C as RAWX data can legitimately contain 0x7F
    gnss.disableUBX7Fcheck();

    // Allocate sufficient RAM to store RAWX messages (>2 KB)
    gnss.setFileBufferSize(fileBufferSize); // Must be called before gnss.begin()

    // Display OLED message
    displayInitialize("GNSS");

    // Initialize u-blox GNSS
    if (!gnss.begin(Wire))
    {
      DEBUG_PRINTLN("Warning: u-blox failed to initialize. Reattempting...");

      // Delay between initialization attempts
      myDelay(2000);

      if (!gnss.begin(Wire))
      {
        DEBUG_PRINTLN("Warning: u-blox failed to initialize! Please check wiring.");
        online.gnss = false;
        logDebug(); // Log system debug information

        // Display message to OLED
        displayFailure();

        while (1)
        {
          // Force WDT to reset system
          blinkLed(3, 250);
          myDelay(2000);
        }
      }
      else
      {
        online.gnss = true;
        DEBUG_PRINTLN("Info: u-blox initialized.");
        // Display OLED message
        displaySuccess();
      }
    }
    else
    {
      online.gnss = true;
      DEBUG_PRINTLN("Info: u-blox initialized.");
      // Display OLED message
      displaySuccess();
    }

    // Configure communication interfaces and satellite signals only if program is running for the first time
    if (gnssConfigFlag)
    {

      // Configure communciation interfaces
      bool setValueSuccess = true;
      setValueSuccess &= gnss.newCfgValset8(UBLOX_CFG_I2C_ENABLED, 1);    // Enable I2C
      setValueSuccess &= gnss.addCfgValset8(UBLOX_CFG_SPI_ENABLED, 0);    // Disable SPI
      setValueSuccess &= gnss.addCfgValset8(UBLOX_CFG_UART1_ENABLED, 0);  // Disable UART1
      setValueSuccess &= gnss.addCfgValset8(UBLOX_CFG_UART2_ENABLED, 0);  // Disable UART2
      setValueSuccess &= gnss.sendCfgValset8(UBLOX_CFG_USB_ENABLED, 0);   // Disable USB
      if (!setValueSuccess)
      {
        DEBUG_PRINTLN("Warning: Communication interfaces not configured!");
      }

      // Configure satellite signals
      setValueSuccess = true;
      setValueSuccess &= gnss.newCfgValset8(UBLOX_CFG_SIGNAL_GPS_ENA, 1);   // Enable GPS
      setValueSuccess &= gnss.addCfgValset8(UBLOX_CFG_SIGNAL_GLO_ENA, 1);   // Enable GLONASS
      setValueSuccess &= gnss.addCfgValset8(UBLOX_CFG_SIGNAL_GAL_ENA, 0);   // Disable Galileo
      setValueSuccess &= gnss.addCfgValset8(UBLOX_CFG_SIGNAL_BDS_ENA, 0);   // Disable BeiDou
      setValueSuccess &= gnss.sendCfgValset8(UBLOX_CFG_SIGNAL_QZSS_ENA, 0); // Disable QZSS
      myDelay(2000);

      if (!setValueSuccess)
      {
        DEBUG_PRINTLN("Warning: Satellite signals not configured!");
      }
      gnssConfigFlag = false; // Clear flag

      // Print current GNSS settings
      printGnssSettings();
    }

    // Configure u-blox GNSS
    gnss.setI2COutput(COM_TYPE_UBX);                  // Set the I2C port to output UBX only (disable NMEA)
    gnss.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT);  // Save communications port settings to flash and BBR
    gnss.setNavigationFrequency(1);                   // Produce one navigation solution per second
    gnss.setAutoPVT(true);                            // Enable automatic NAV-PVT messages
    gnss.setAutoRXMSFRBX(true, false);                // Enable automatic RXM-SFRBX messages
    gnss.setAutoRXMRAWX(true, false);                 // Enable automatic RXM-RAWX messages
    gnss.logRXMSFRBX();                               // Enable RXM-SFRBX data logging
    gnss.logRXMRAWX();                                // Enable RXM-RAWX data logging
  }
  else
  {
    return;
  }
  // Stop the loop timer
  timer.gnss = millis() - loopStartTime;
}

// Acquire valid GNSS fix and sync RTC
void syncRtc()
{
  // Start loop timer
  unsigned long loopStartTime = millis();

  // Check if u-blox GNSS initialized successfully
  if (online.gnss)
  {
    // Clear flag
    rtcSyncFlag = false;

    DEBUG_PRINTLN("Info: Attempting to sync RTC with GNSS...");
    // Display OLED message(s)
    displayRtcSync();

    // Attempt to acquire a valid GNSS position fix for up to 5 minutes
    while (!rtcSyncFlag && millis() - loopStartTime < gnssTimeout * 10UL * 1000UL)
    {
      petDog(); // Reset WDT

      // Check for UBX-NAV-PVT messages
      if (gnss.getPVT())
      {
        // Blink LED
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

        bool dateValidFlag = gnss.getConfirmedDate();
        bool timeValidFlag = gnss.getConfirmedTime();
        byte fixType = gnss.getFixType();

#if DEBUG_GNSS
        char gnssBuffer[100];
        sprintf(gnssBuffer, "%04u-%02d-%02d %02d:%02d:%02d.%03d,%ld,%ld,%d,%d,%d,%d,%d",
                gnss.getYear(), gnss.getMonth(), gnss.getDay(),
                gnss.getHour(), gnss.getMinute(), gnss.getSecond(), gnss.getMillisecond(),
                gnss.getLatitude(), gnss.getLongitude(), gnss.getSIV(),
                gnss.getPDOP(), gnss.getFixType(),
                dateValidFlag, timeValidFlag);
        DEBUG_PRINTLN(gnssBuffer);
#endif

        // Check if date and time are valid and synchronize RTC with GNSS
        if (fixType == 3 && dateValidFlag && timeValidFlag)
        {
          unsigned long rtcEpoch = rtc.getEpoch();        // Get RTC epoch time
          unsigned long gnssEpoch = gnss.getUnixEpoch();  // Get GNSS epoch time
          rtc.setEpoch(gnssEpoch);                        // Set RTC date and time
          rtcDrift = gnssEpoch - rtcEpoch;                // Calculate RTC drift
          rtcSyncFlag = true;                             // Set flag

          // Update logfile timestamp if more than 30 seconds of drift
          if (abs(rtcDrift) > 30)
          {
            DEBUG_PRINTLN("Info: Updating logfile timestamp");
            rtc.getTime(); // Get the RTC's date and time
            getLogFileName(); // Update logfile timestamp
          }

          DEBUG_PRINT("Info: RTC drift: "); DEBUG_PRINTLN(rtcDrift);
          DEBUG_PRINT("Info: RTC time synced to "); printDateTime();

          // Display OLED message(s)
          displaySuccess();
          displayRtcOffset(rtcDrift);
          //blinkLed(5, 1000);
        }
      }
    }
    if (!rtcSyncFlag)
    {
      DEBUG_PRINTLN("Warning: Unable to sync RTC!");
      // Display OLED message(s)
      displayFailure();
      //blinkLed(10, 500);
    }
  }
  else
  {
    DEBUG_PRINTLN("Warning: GNSS offline!");
    rtcSyncFlag = false; // Clear flag
  }

  // Stop the loop timer
  timer.syncRtc = millis() - loopStartTime;
}

// Create timestamped log file name
void getLogFileName()
{
  sprintf(logFileName, "GVT_0_20%02d%02d%02d_%02d%02d%02d.ubx",
          rtc.year, rtc.month, rtc.dayOfMonth,
          rtc.hour, rtc.minute, rtc.seconds);
}

// Log UBX-RXM-RAWX/SFRBX data
void logGnss()
{
  // Start loop timer
  unsigned long loopStartTime = millis();

  bool screen = 0;

  // Record logging start time
  logStartTime = rtc.getEpoch();

  // Check if microSD and u-blox GNSS initialized successfully
  if (online.microSd && online.gnss)
  {
    // Create a new log file and open for writing
    // O_CREAT  - Create the file if it does not exist
    // O_APPEND - Seek to the end of the file prior to each write
    // O_WRITE  - Open the file for writing
    if (!logFile.open(logFileName, O_CREAT | O_APPEND | O_WRITE))
    {
      DEBUG_PRINT("Warning: Failed to create log file"); DEBUG_PRINTLN(logFileName);
      return;
    }
    else
    {
      online.logGnss = true;
      DEBUG_PRINT("Info: Created log file "); DEBUG_PRINTLN(logFileName);
    }

    // Update file create timestamp
    updateFileCreate(&logFile);

    // Reset counters
    bytesWritten, writeFailCounter, syncFailCounter, closeFailCounter = 0;

    gnss.clearFileBuffer();         // Clear file buffer
    gnss.clearMaxFileBufferAvail(); // Reset max file buffer size

    DEBUG_PRINTLN("Info: Starting logging...");

    // Log data until logging alarm triggers
    while (!alarmFlag)
    {
      // Reset watchdog
      petDog();

      // Check for the arrival of new data and process it
      gnss.checkUblox();

      // Check if sdWriteSize bytes are waiting in the buffer
      while (gnss.fileBufferAvailable() >= sdWriteSize)
      {
        // Reset WDT
        petDog();

        // Turn on LED during SD writes
        digitalWrite(LED_BUILTIN, HIGH);

        // Create buffer to store data during writes to SD card
        uint8_t myBuffer[sdWriteSize];

        // Extract exactly sdWriteSize bytes from the UBX file buffer and put them into myBuffer
        gnss.extractFileBufferData((uint8_t *)&myBuffer, sdWriteSize);

        // Write exactly sdWriteSize bytes from myBuffer to the ubxDataFile on the SD card
        if (!logFile.write(myBuffer, sdWriteSize))
        {
          DEBUG_PRINTLN("Warning: Failed to write to log file!");
          writeFailCounter++; // Count number of failed writes to microSD
        }

        // Update bytesWritten
        bytesWritten += sdWriteSize;

        // If SD writing is slow or there is a lot of data to write, keep checking for the arrival of new data
        gnss.checkUblox(); // Check for the arrival of new data and process it

        // Turn off LED
        digitalWrite(LED_BUILTIN, LOW);
      }

      // Periodically print number of bytes written
      if (millis() - previousMillis > 5000)
      {
        // Sync the log file
        if (!logFile.sync())
        {
          DEBUG_PRINTLN("Warning: Failed to sync log file!");
          syncFailCounter++; // Count number of failed file syncs
        }

        // Print number of bytes written to SD card
        DEBUG_PRINT(bytesWritten); DEBUG_PRINT(" bytes written. ");

        // Get max file buffer size
        maxBufferBytes = gnss.getMaxFileBufferAvail();
        DEBUG_PRINT("Max buffer: "); DEBUG_PRINTLN(maxBufferBytes);

        // Warn if fileBufferSize was more than 80% full
        if (maxBufferBytes > ((fileBufferSize / 5) * 4))
        {
          DEBUG_PRINTLN("Warning: File buffer >80 % full. Data loss may have occurrred.");
        }

        // Display information to OLED display
        if (!screen)
        {
          screen = true;
          displayScreen1();
        }
        else
        {
          screen = false;
          //displayOff();
          displayScreen2();
        }

        previousMillis = millis(); // Update previousMillis
      }
    }

    // Check for bytes remaining in file buffer
    uint16_t remainingBytes = gnss.fileBufferAvailable();

    while (remainingBytes > 0)
    {
      // Reset WDT
      petDog();

      // Turn on LED during SD writes
      digitalWrite(LED_BUILTIN, HIGH);

      // Create buffer to store data during writes to SD card
      uint8_t myBuffer[sdWriteSize];

      // Write the remaining bytes to SD card sdWriteSize bytes at a time
      uint16_t bytesToWrite = remainingBytes;
      if (bytesToWrite > sdWriteSize)
      {
        bytesToWrite = sdWriteSize;
      }

      // Extract bytesToWrite bytes from the UBX file buffer and put them into myBuffer
      gnss.extractFileBufferData((uint8_t *)&myBuffer, bytesToWrite);

      // Write bytesToWrite bytes from myBuffer to the ubxDataFile on the SD card
      logFile.write(myBuffer, bytesToWrite);

      bytesWritten += bytesToWrite; // Update bytesWritten
      remainingBytes -= bytesToWrite; // Decrement remainingBytes

      // Turn off LED
      digitalWrite(LED_BUILTIN, LOW);
    }

    // Print total number of bytes written to SD card
    DEBUG_PRINT("Info: Total bytes written is "); DEBUG_PRINTLN(bytesWritten);

    // Sync the log file
    if (!logFile.sync())
    {
      DEBUG_PRINTLN("Warning: Failed to sync log file!");
      syncFailCounter++; // Count number of failed file syncs
    }

    // Update file access timestamps
    updateFileAccess(&logFile);

    // Close the log file
    if (!logFile.close())
    {
      DEBUG_PRINTLN("Warning: Failed to close log file!");
      closeFailCounter++; // Count number of failed file closes
    }

    // Enable internal I2C pull-ups
    //Wire.setPullups(1);

    // Free RAM allocated for file storage and PVT processing
    //gnss.end();
  }
  else
  {
    online.logGnss = false;
  }

  // Stop the loop timer
  timer.logGnss = millis() - loopStartTime;
}
