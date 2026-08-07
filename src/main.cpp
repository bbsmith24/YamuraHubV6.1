//
// YamuraLog Hub
// Teensy 4.0/CAN FD
// Brian B. Smith
// Yamura Motors LLC
// brianbsmith.com
// February 2022
//
// 05/2026 
// replaced open source FTP client with custom implementation to 
// support large file transfers and directory listing. Also eliminated
// versioning issues with Arduino libraries by implementing FTP client 
// functionality directly in the main codebase. Uses ESP FTP display board
// with built in microSD card slot for WiFi and file storage
// 
// Requires FlexCAN_T4 for Teensy 4.0/4.1 CAN 
// can1 and can2 are CAN2.0B
// can3 is CAN FD
//
//  MIT License
//  Copyright (c) 2022 Brian B Smith - info@brianbsmith.com
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
// 

#include "main.h"        // Contains pin definitions
#include "Config.h"      // Effective WiFi/FTP settings + driver list (config.ini)

// Global FTP client instance
FTPClient ftpClient;
/// TFT display
TFT_eSPI tftDisplay = TFT_eSPI();
// int screenSize[4];
// int8_t fontHeight = 12;
// int fontWidth;
// int textPosition[2] = { 5, 12 };
// int screenRotation = 1;

// menu instance
TFTMenu tftMenu;

FlexCAN_T4FD<CAN3, RX_SIZE_256, TX_SIZE_16> canFD;  // can3 port
// using 11 bit Identifiers
//          1
// 01234567890
// -------      7 bit node type   (0x00 to 0x3F)
//        ----  4 bit node number (0x00 to 0x0F)
// hub gets lowest possible number so it has bus priority
// YamuraLog device IDs (all use standard 11 bit IDs)
// HUB       0000001 0000      (only one allowed) 0x10  (this node)
// Control   0000010 0000      (only one allowed) 0x20
// A2D       0000011 0000-1111                    0x30-0x3F
// Accel     0000100 0000      (only one allowed) 0x40
// GPS       0000101 0000      (only one allowed) 0x50
// Tire      0000110 0000-1111                    0x60-0x6F
// Shock     0000111 0000-1111                    0x70-0x7F
// WheelSpd  0001000 0000-1111                    0x80-0x8F
// RPM       0001001 0000      (only one allowed) 0x90
// CAN       0001010 0000      (only one allowed) 0xA0
#define CANID 0x010
bool timeValid = false;

File targetFile;
///
///
///
void setup() 
{
    Serial.begin(115200);
    // Initialize menu buttons
    tftMenu.bannerString = "Yamura Motors LLC Data Logger";
    tftMenu.buttons[0].buttonPin = BUTTON_0;
    tftMenu.buttons[0].buttonName = "SELECT";
    tftMenu.buttons[1].buttonPin = BUTTON_1;        
    tftMenu.buttons[1].buttonName = "UP";
    tftMenu.buttons[2].buttonPin = BUTTON_2;
    tftMenu.buttons[2].buttonName = "DOWN";
    for (int btnIdx = 0; btnIdx < TFTMenu::buttonCount; btnIdx++)
    {

        tftMenu.buttons[btnIdx].buttonState = HIGH;
        tftMenu.buttons[btnIdx].lastButtonState = HIGH;
        tftMenu.buttons[btnIdx].lastDebounceTime = millis();
        tftMenu.buttons[btnIdx].buttonChanged = false;
        pinMode(tftMenu.buttons[btnIdx].buttonPin, INPUT_PULLUP);
    }
    // Set TFT display pointer in menu instance
    tftMenu.tftDisplay = &tftDisplay;
    // draw live upload progress on the TFT during sends
    ftpClient.progressCallback = UploadProgressTFT;
    /// Define main menu choices
    TFTMenu::MenuChoice mainMenuChoices[6];
    mainMenuChoices[0].description = "Start Logging";
    mainMenuChoices[0].result = START_LOGGING;
    mainMenuChoices[1].description = "Select Driver";
    mainMenuChoices[1].result = SELECT_DRIVER;
    mainMenuChoices[2].description = "Send File";
    mainMenuChoices[2].result = SEND_FILE;
    mainMenuChoices[3].description = "Get File";
    mainMenuChoices[3].result = GET_FILE;
    mainMenuChoices[4].description = "Settings";
    mainMenuChoices[4].result = CHANGE_SETTINGS;

    // Wait for Serial connection
    unsigned long startTime = millis();
    while (!Serial && millis() - startTime < 5000) {
        delay(10);
    }
    #ifdef DEBUG_VERBOSE
    Serial.println("\n=====================================================");
    Serial.println(  "* Teensy FTP Client with AirLift WiFi - C++ Version *");
    Serial.println(  "=====================================================\n");
    #endif
    // Configure WiFi control pins
    // CS high (not selected)
    pinMode(SPIWIFI_SS, OUTPUT);
    digitalWrite(SPIWIFI_SS, HIGH);
    // Reset pin high (normal operation)
    pinMode(ESP32_RESETN, OUTPUT);
    digitalWrite(ESP32_RESETN, HIGH);
    // Interrupt pin as input
    pinMode(SPIWIFI_ACK, INPUT);  
    #ifdef DEBUG_VERBOSE
    Serial.println();
    Serial.println("SPI Pins");
    Serial.println("========");
    Serial.print("SPI SDO(MOSI) pin: ");
    Serial.println(WIFI_SPI_MOSI_PIN);
    Serial.print("SPI SDI(MISO) pin: ");
    Serial.println(WIFI_SPI_MISO_PIN);
    Serial.print("SPI SCK pin:       ");
    Serial.println(WIFI_SPI_SCK_PIN);

    Serial.println();
    Serial.println("Airlift pins");
    Serial.println("============");
    Serial.print("CS pin:    ");
    Serial.println(SPIWIFI_SS);
    Serial.print("RST pin:   ");
    Serial.println(ESP32_RESETN);
    Serial.print("IRQ pin:   ");
    Serial.println(SPIWIFI_ACK);
    Serial.print("GPIO0 pin: ");
    Serial.println(ESP32_GPIO0);    

    Serial.println();
    Serial.println("TFT pins");
    Serial.println("========");
    Serial.print("TFT_CS pin: ");
    Serial.println(TFT_CS);    
    Serial.print("TFT_DC pin: ");
    Serial.println(TFT_DC);    
    Serial.print("TFT_RST pin: ");
    Serial.println(TFT_RST);    

    Serial.println();
    Serial.println("SD pins");
    Serial.println("=======");
    Serial.print("CS pin: ");
    Serial.println(SD_CS);
    Serial.println();

    Serial.println();
    Serial.println("LED indicator pins");
    Serial.println("=======");
    Serial.print("RECORDING LED pin: ");
    Serial.println(RECORDING_LED);
    Serial.print("GPS STATUS LED pin: ");
    Serial.println(GPS_STATUS_LED);
    Serial.println();
    #endif
    // Initialize indicator LEDs
    pinMode(RECORDING_LED, OUTPUT); 
    pinMode(GPS_STATUS_LED, OUTPUT);

    #ifdef DEBUG_VERBOSE
    Serial.println("Blinking indicator LEDs to show system is starting up...");
    for(int idx = 0; idx < 10; idx++)
    {
        digitalWrite(RECORDING_LED, LOW);
        digitalWrite(GPS_STATUS_LED, HIGH);
        delay(100);
        digitalWrite(RECORDING_LED, HIGH);
        digitalWrite(GPS_STATUS_LED, LOW);
        delay(100);
    }
    #endif
    digitalWrite(RECORDING_LED, LOW);
    digitalWrite(GPS_STATUS_LED, LOW);

    // Initialize SPI for WiFi module
    #ifdef DEBUG_VERBOSE
    Serial.println("Initializing SPI for WiFi module...");
    #endif
    // WiFiNINA requires SPI mode 0, MSB first, and specific clock speed
    // Teensy 4.1 runs at 600MHz, so SPI clock divisor must be much higher
    // WiFiNINA specs: max 8MHz SPI clock, but slower is more reliable
    //SPI_CLOCK_DIV128 = 600MHz / 128 = 4.6875MHz (very conservative)
    SPI.begin();
    SPI.setClockDivider(SPI_CLOCK_DIV128);  // 600MHz / 128 = 4.6875MHz (very conservative for AirLift)
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);

    // Try to set WiFi pins for the AirLift module
    WiFi.setPins(SPIWIFI_SS, SPIWIFI_ACK, ESP32_RESETN, ESP32_GPIO0);

    // Initialize WiFi module
    // Note: WiFi.setPins() is not available in Teensy's WiFiNINA library
    // PIN configuration is already handled above through pinMode() and digitalWrite()
    #ifdef DEBUG_VERBOSE
    Serial.println("WiFi module pins configured, waiting for module to initialize...");
    #endif
    delay(500);  // Give WiFi module time to initialize
 
    // Check if WiFi module is present
    int wifiStatus = WiFi.status();
    #ifdef DEBUG_VERBOSE
    Serial.print("WiFi module status: ");
    Serial.println(wifiStatus);
    #endif
    if (wifiStatus == WL_NO_SHIELD || 
        wifiStatus == WL_NO_MODULE) 
    {
        #ifdef DEBUG_VERBOSE
        Serial.println("FAILED");
        if(wifiStatus == WL_NO_SHIELD) 
        {
            Serial.println("ERROR: No WiFi shield found!");
        }
        else 
        {
            Serial.println("ERROR: No WiFi module found!");
        }
        // Try to reset the WiFi module
        Serial.println("Attempting to reset WiFi module...");
        #endif
        digitalWrite(ESP32_RESETN, LOW);
        delay(100);
        digitalWrite(ESP32_RESETN, HIGH);
        delay(1000);
        
        wifiStatus = WiFi.status();

        #ifdef DEBUG_VERBOSE
        Serial.print("WiFi module status after reset: ");
        Serial.println(wifiStatus);
        #endif

        if (wifiStatus == WL_NO_SHIELD || wifiStatus == WL_NO_MODULE) {
            while (true) 
            {
                delay(1000);
            }
        }
    }

    #ifdef DEBUG_VERBOSE
    Serial.println("OK - WiFi module detected");
    #endif
    String fv = WiFi.firmwareVersion();
    // check for the WiFi module:
    #ifdef DEBUG_VERBOSE
    sprintf(outStr, "Airlift OK - firmware version %s", fv.c_str());
    Serial.println(outStr);
    if (fv < "1.0.0")
    {
      Serial.println("Please upgrade the Airlift firmware");
    }
    else
    {
      Serial.println("Airlift firmware version is up to date");
    }
    Serial.println("==================================");
    #endif
    
    // Initialize SD card
    #ifdef DEBUG_VERBOSE
    Serial.print("Initializing SD card...");
    #endif
    if (!SD.begin(SD_CS)) 
    {
        #ifdef DEBUG_VERBOSE
        Serial.println(" FAILED");
        Serial.println("ERROR: SD card initialization failed");
        #endif
        while(true)
        {
            delay(1000);
        }
    }
    #ifdef DEBUG_VERBOSE
    Serial.println("SD card initialized successfully");
    root = SD.open("/");
    ListFiles(root);
    root.close();
    #endif
    // Register the callback sd file date/time callback function
    SdFile::dateTimeCallback(DateTimeProvider);

    // Load /config.ini if present - overrides the WiFi/FTP defaults from
    // WiFiSecrets.h and populates the driver list.
    LoadConfigFromSD("/config.ini");
    ftpClient.ftpListenerPort = FTP_PORT;  // config default; Settings menu can still change it
    // Initialize TFT display
    tftMenu.TFTInitialization();

    // CAN-FD setup
    canFD.begin();
    CANFD_timings_t canfdConfig;
    canfdConfig.clock = CLK_24MHz;
    canfdConfig.baudrate = ARB_BAUD;
    canfdConfig.baudrateFD = FD_BAUD;
    canfdConfig.propdelay = 190;
    canfdConfig.bus_length = 1;
    canfdConfig.sample = 75;
    canFD.setRegions(64);
    canFD.setBaudRateAdvanced(canfdConfig, 1, 1);
    // receive MBs start at 0
    canFD.setMB(MB0, RX, STD);  // control
    canFD.setMB(MB1, RX, STD);  // A2D node(s)
    canFD.setMB(MB2, RX, STD);  // IMU node
    canFD.setMB(MB3, RX, STD);  // GPS node
    canFD.setMB(MB4, RX, STD);  // IR tire tenp node(s)
    canFD.setMB(MB5, RX, STD);  // shock position node(s) fast A2D
    canFD.setMB(MB6, RX, STD);  // wheel speed counter node(s)
    canFD.setMB(MB7, RX, STD);
    canFD.setMB(MB8, RX, STD);
    //canFD.setMB(MB9,RX,STD);
    //canFD.setMB(MB10,RX,STD);
    //canFD.setMB(MB11,RX,STD);
    // transmit mailboxes - needed?
    //canFD.setMB(MB9,TX,STD);
    //canFD.setMB(MB13,TX,STD);
    //canFD.setMBFilter(REJECT_ALL);
    canFD.enableMBInterrupts();

    // mailbox 0 - messages from control box
    canFD.setMBFilterRange(MB0, 0x20, 0x20);
    canFD.enhanceFilter(MB0);
    canFD.onReceive(MB0, CAN_ControlMessage);

    // mailbox 1 - messages from A2D nodes
    canFD.setMBFilterRange(MB1, 0x30, 0x3F);
    canFD.enhanceFilter(MB1);
    canFD.onReceive(MB1, CAN_A2DMessage);

    // mailbox 2 - messages from accelerometer node
    canFD.setMBFilterRange(MB2, 0x40, 0x4F);
    canFD.enhanceFilter(MB2);
    canFD.onReceive(MB2, CAN_AccelMessage);

    // mailbox 3 - messages from gps node
    canFD.setMBFilterRange(MB3, 0x50, 0x5F);
    canFD.enhanceFilter(MB3);
    canFD.onReceive(MB3, CAN_GPSMessage);

    // mailbox 4 - messages from IR tire temp nodes
    canFD.setMBFilterRange(MB4, 0x60, 0x6F);
    canFD.enhanceFilter(MB4);
    canFD.onReceive(MB4, CAN_TireIRMessage);

    // mailbox 5 - messages from shock position nodes
    canFD.setMBFilterRange(MB5, 0x70, 0x7F);
    canFD.enhanceFilter(MB5);
    canFD.onReceive(MB5, CAN_ShockMessage);

    // mailbox 6 - messages from wheel speed nodes
    canFD.setMBFilterRange(MB6, 0x80, 0x8F);
    canFD.enhanceFilter(MB6);
    canFD.onReceive(MB6, CAN_WheelSpeedMessage);

    // mailbox 7 - messages from wheel speed nodes
    canFD.setMBFilterRange(MB7, 0x90, 0x9F);
    canFD.enhanceFilter(MB7);
    canFD.onReceive(MB7, CAN_RPMMessage);

    // mailbox 8 - messages from wheel speed nodes
    canFD.setMBFilterRange(MB8, 0xA0, 0xAF);
    canFD.enhanceFilter(MB8);
    canFD.onReceive(MB8, CAN_CANMessage);

    #ifdef DEBUG_VERBOSE
    sprintf(outStr, "CAN bus ready");
    #endif
    tftMenu.DrawString(outStr, tftMenu.textPosition[0], tftMenu.textPosition[1], GFXFF);
    delay(1000);

    // CAN send timer
    #ifdef DEBUG_VERBOSE
    Serial.println("Start timer in setup");
    sprintf(outStr, "Start Heartbeat timer");
    //tftDisplay.drawString(outStr, tftMenu.textPosition[0], tftMenu.textPosition[1], GFXFF);
    //tftMenu.textPosition[1] += tftMenu.fontHeight;
    //
    #endif
    delay(1000);
    bool timerOK = timer.begin(SendHeartbeat, TIMER_1HZ);  // Send heartbeat frame every 1000ms in setup
    if (!timerOK)
    {
        #ifdef DEBUG_VERBOSE
        Serial.println("Error starting SendHeartbeat timer");
        #endif
        sprintf(outStr, "Error starting Heartbeat timer");
        tftMenu.DrawString(outStr, tftMenu.textPosition[0], tftMenu.textPosition[1], GFXFF);
        delay(1000);
    }
    #ifdef DEBUG_VERBOSE
    Serial.println("Error starting SendHeartbeat timer");
    #endif
    sprintf(outStr, "Heartbeat timer started");
     tftMenu.textPosition[1] += tftMenu.fontHeight;
    tftMenu.DrawString(outStr, tftMenu.textPosition[0], tftMenu.textPosition[1], GFXFF);
    canFD.mailboxStatus();

    sprintf(sendLogFileName, "-");
}
///
///
///
void loop() 
{
  #ifdef DEBUG_EXTRA_VERBOSE
  Serial.print("loop() current deviceState: ");
  Serial.println(deviceState);
  #endif
  switch (deviceState)
  {
    case DISPLAY_MENU:
      MainMenu();
      break;
    case SELECT_DRIVER:
      SelectDriverMenu();
      deviceState = DISPLAY_MENU;
      break;
    case START_LOGGING:
      StartLogging();
      deviceState = DISPLAY_MENU;
      break;
    case SEND_LAST_FILE:
      SendFile(sendLogFileName);
      deviceState = DISPLAY_MENU;
      break;
    case SEND_FILE:
      SendFileMenu();
      deviceState = DISPLAY_MENU;
      break;
    case GET_FILE:
      GetFileMenu();
      deviceState = DISPLAY_MENU;
      break;
    case CHANGE_SETTINGS:
      ChangeSettingsMenu();
      break;
    case DELETE_LOG_FILES:
      Serial.println("DeleteLogFiles() - deleting log files");  
      root = SD.open("/");
      DeleteLogFiles(root);
      deviceState = DISPLAY_MENU;
      break;
    case DELETE_ALL_FILES:
      Serial.println("DeleteAllFiles() - deleting all files");  
      root = SD.open("/");
      DeleteAllFiles(root);
      deviceState = DISPLAY_MENU;
      break;
    case LIST_FILES:
      ListFilesMenu();  // shows files on the TFT; returns to the settings menu
      break;
    case SELECT_FTP_PORT:
      Serial.println("SelectFtpPort() - selecting FTP port");
      SelectFtpPort();
      deviceState = DISPLAY_MENU;
      break;
    case SELECT_DEBUG:
      SelectDebugDisplay();
      deviceState = DISPLAY_MENU;
      break;
    default:
      break;
  }
}
//
//
//
void MainMenu()
{
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("MainMenu() - displaying main menu");
    #endif
    sprintf(outStr, "Send Last File (%s)", sendLogFileName);
    int menuCount = 6;
    TFTMenu::MenuChoice mainMenuChoices[6];
    mainMenuChoices[0].description = "Start Logging";
    mainMenuChoices[0].result = START_LOGGING;
    //
    mainMenuChoices[1].description = outStr;
    mainMenuChoices[1].result = SEND_LAST_FILE;
    //
    mainMenuChoices[2].description = "Select Driver";
    mainMenuChoices[2].result = SELECT_DRIVER;
    //
    mainMenuChoices[3].description = "Send File";
    mainMenuChoices[3].result = SEND_FILE;
    //
    mainMenuChoices[4].description = "Get File";
    mainMenuChoices[4].result = GET_FILE;
    //
    mainMenuChoices[5].description = "Settings";
    mainMenuChoices[5].result = CHANGE_SETTINGS;
    deviceState = tftMenu.MenuSelect(12, mainMenuChoices, menuCount, START_LOGGING);
}
//
//
//
void SelectDriverMenu()
{
    #ifdef DEBUG_VERBOSE
    Serial.println("SelectDriverMenu() - displaying select driver menu");
    #endif

    if (driverCount <= 0)
    {
        tftMenu.NotImplementedScreen("No drivers in config.ini");
        deviceState = DISPLAY_MENU;
        return;
    }

    // build a selectable menu from the driver list (same pattern as SelectLocalFile)
    TFTMenu::MenuChoice *driverMenu = (TFTMenu::MenuChoice*)calloc(driverCount, sizeof(TFTMenu::MenuChoice));
    if (!driverMenu)
    {
        tftMenu.NotImplementedScreen("Out of memory building list");
        deviceState = DISPLAY_MENU;
        return;
    }
    for (int i = 0; i < driverCount; i++)
    {
        driverMenu[i].description = driverNames[i];
        driverMenu[i].result = i;
    }

    int selectedIdx = tftMenu.MenuSelect(12, driverMenu, driverCount, 0);
    strncpy(currentDriver, driverMenu[selectedIdx].description.c_str(), CFG_DRIVER_LEN - 1);
    currentDriver[CFG_DRIVER_LEN - 1] = '\0';
    free(driverMenu);

    #ifdef DEBUG_VERBOSE
    Serial.print("Selected driver: ");
    Serial.println(currentDriver);
    #endif

    deviceState = DISPLAY_MENU;
}
//
// Filesystem-safe log filename prefix built from the selected driver name:
// keeps only letters/digits/'-'/'_' (dropping spaces, dots and anything else the
// menu parser or FAT dislikes) and caps the length so the full
// "/<prefix>NNN.yl5" name fits sendLogFileName / the SD library. Falls back to
// "sdLog" when no driver is chosen or the name has no usable characters.
//
const char* LogFilePrefix()
{
    // Leave room for '/', up to a 5-digit index, ".yl5" and the null terminator.
    static const size_t maxPrefix = sizeof(sendLogFileName) - 1 - 5 - 4 - 1;
    static char prefix[sizeof(sendLogFileName)];
    size_t n = 0;
    for (const char* p = currentDriver; *p != '\0' && n < maxPrefix; p++)
    {
        char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_')
        {
            prefix[n++] = c;
        }
    }
    prefix[n] = '\0';
    if (n == 0)
    {
        strcpy(prefix, "sdLog");
    }
    return prefix;
}
//
//
//
void StartLogging()
{
    #ifdef DEBUG_VERBOSE
    Serial.println("Stop heartbeat to start logging");
    #endif
    timer.end();  // stop heartbeat before logging starts
    int fileIdx = GetNextLogFileIdx();
    sprintf(sendLogFileName, "/%s%03d.yl5", LogFilePrefix(), fileIdx);
    #ifdef DEBUG_VERBOSE
    Serial.printf("Opening log file %s for writing\n", sendLogFileName);
    #endif

    digitalWrite(RECORDING_LED, HIGH);  // turn on recording LED

    tftDisplay.fillScreen((uint16_t)~TFT_GREEN);
    tftDisplay.setRotation(1);
    tftMenu.DisplayBanner();
    tftDisplay.setFreeFont(FSS12);
    int16_t fontHeight = tftDisplay.fontHeight(GFXFF);
    int16_t textPosition[2];
    textPosition[0] = tftMenu.textPosition[0];
    textPosition[1] = tftMenu.fontHeight * 2;
    tftDisplay.setTextColor((uint16_t)~TFT_BLACK, (uint16_t)~TFT_GREEN);
    sprintf(outStr, "START LOGGING at %lu", millis());
    tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
    sprintf(outStr, "%s", sendLogFileName);
    tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
    //
    targetFile = SD.open(sendLogFileName, FILE_WRITE);
    if (!targetFile || targetFile.isDirectory())
    {
        #ifdef DEBUG_VERBOSE
        Serial.printf("Failed to open file %s for writing\n", sendLogFileName);
        #endif
        sprintf(outStr, "Logging Failed %s", sendLogFileName);
        tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
        textPosition[1] += fontHeight;
        logData = false;
        return;
    }
    #ifdef DEBUG_VERBOSE
    Serial.printf("Start logging to %s at %dms\n", sendLogFileName, millis());
    #endif
    logData = true;
    while (true)
    {
        canFD.events();
        currentMillis = millis();
        tftMenu.CheckButton(currentMillis, 0);
        if (tftMenu.buttons[0].buttonReleased)
        {
            StopLogging();
            tftMenu.buttons[0].buttonReleased = false;
            break;
        }
    }
    deviceState = DISPLAY_MENU;

    #ifdef DEBUG_VERBOSE
    Serial.println("Restart heartbeat timer in StartLogging");
    #endif
    timer.begin(SendHeartbeat, TIMER_1HZ);  // restart heartbeat after logging ends
}
//
//
// upload progress callback (registered on ftpClient) - draws the live progress
// line in place at progressY. fillRect clears the line first so it overwrites
// cleanly (proportional fonts don't paint a background over old text).
//
void UploadProgressTFT(size_t sent, size_t total)
{
    int pct = total ? (int)((sent * 100) / total) : 0;
    sprintf(outStr, "%d%%  %u / %u", pct, (unsigned)sent, (unsigned)total);
    tftDisplay.fillRect(0, progressY, tftDisplay.width(), tftMenu.fontHeight + 2, TFT_WHITE);
    tftDisplay.drawString(outStr, 0, progressY, GFXFF);
}
//
//
void SendFileMenu()
{
    char fileNameToSend[64];
    // get the file name to send from the user through the TFT menu
    SelectLocalFile(fileNameToSend);
    SendFile(fileNameToSend);
}
//
//
//
void SendFile(char* fileNameToSend)
{
    char fileNameToSendSize[64];
    String fileNameStrToSend = sendLogFileName;
    int spaceIdx = fileNameStrToSend.indexOf(".");
    if(!strcmp(fileNameToSend, "-"))
    {
      sprintf(outStr, "No recent file - record first");
      tftMenu.NotImplementedScreen(outStr);
      deviceState = DISPLAY_MENU;
      return;
    }

    // stop heartbeat before file upload to FTP server
    #ifdef DEBUG_VERBOSE
    Serial.println("Stop heartbeat timer to send file to FTP server");
    #endif
    timer.end();  

    // clean up the file name - split on space between name and size
    if(spaceIdx > -1)
    {
      spaceIdx = fileNameStrToSend.indexOf(" ", spaceIdx);
      strcpy(fileNameToSend, fileNameStrToSend.substring(0, spaceIdx).c_str());
      strcpy(fileNameToSendSize, fileNameStrToSend.substring(spaceIdx).c_str());
    }
    bool sendResult = false;
    int attemptCount = 0;
    // Keep WiFi associated across retries within this send - re-associating per
    // attempt is the slow part; a data-connection drop just needs a fresh try.
    ftpClient.keepWiFiAlive = true;
    while(!sendResult)
    {
      #ifdef DEBUG_VERBOSE
      sprintf(outStr, "%s >>>> %s", fileNameToSend, fileNameToSendSize);
      Serial.println(outStr);
      #endif
      // display sending file message on TFT
      tftDisplay.fillScreen(TFT_WHITE);
      tftMenu.DisplayBanner();
      tftMenu.SetFont(12);
      int textPosition[2];
      textPosition[0] = 0;
      textPosition[1] = tftMenu.fontHeight;
      sprintf(outStr, "Sending %s...(%d)", fileNameToSend, attemptCount + 1);
      sprintf(sendLogFileName, "%s", fileNameToSend );
      tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
      textPosition[1] += tftMenu.fontHeight;
      // when debug display is on, show the WiFi + FTP credentials in use;
      // otherwise just the file name is shown
      if (debugDisplay)
      {
        sprintf(outStr, "WiFi %s  pass %s", SSID, PASSWORD);
        tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
        textPosition[1] += tftMenu.fontHeight;
        sprintf(outStr, "FTP user %s  pass %s", FTP_USER, FTP_PASS);
        tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
        textPosition[1] += tftMenu.fontHeight;
        sprintf(outStr, "FTP port %d", ftpClient.ftpListenerPort);
        tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
        textPosition[1] += tftMenu.fontHeight;
      }
      // reserve this line for the live upload progress (overwritten in place by
      // UploadProgressTFT during the transfer); Sent/Result go below it
      progressY = textPosition[1];
      textPosition[1] += tftMenu.fontHeight;
      // send the file to the FTP server
      char statusStr[256];
      sendResult = ftpClient.UploadFileFromSDtoFTPServer(fileNameToSend, fileNameToSend, statusStr);
      // display result on TFT
      sprintf(outStr, "Sent %s", fileNameToSend);
      tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);

      textPosition[1] += tftMenu.fontHeight;
      sprintf(outStr, "Result %s", sendResult ? "OK" : "ERROR");
      tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);

      textPosition[1] += tftMenu.fontHeight;
      sprintf(outStr, " %s",statusStr);
      tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);

      if(attemptCount >= FTP_SEND_MAX_ATTEMPTS - 1)
      {
        textPosition[1] += tftMenu.fontHeight;
        tftDisplay.drawString("Press any button to continue", textPosition[0], textPosition[1], GFXFF);
        textPosition[1] += tftMenu.fontHeight;
        tftMenu.WaitForAnyButton();
        break;
      }

      // On failure, hold the error briefly so it's readable, then retry. Retries
      // are fast now (WiFi stays up), so keep the pause short.
      if(!sendResult)
      {
        #ifdef DEBUG_VERBOSE
        Serial.print("Send attempt ");
        Serial.print(attemptCount + 1);
        Serial.print(" failed: ");
        Serial.println(statusStr);
        #endif
        delay(1000);
      }
      attemptCount++;
    }
    // Done retrying (success or gave up): drop WiFi now.
    ftpClient.keepWiFiAlive = false;
    ftpClient.FTPDisconnect();
    deviceState = DISPLAY_MENU;

    #ifdef DEBUG_VERBOSE
    Serial.println("Restart heartbeat timer in SendFileMenu");
    #endif
    timer.begin(SendHeartbeat, TIMER_1HZ);  // restart heartbeat after file send
}
//
//
//
void GetFileMenu()
{
    #ifdef DEBUG_VERBOSE
    Serial.println("GetFileMenu() - request file list from FTP server");
    #endif

    // stop heartbeat during FTP activity (mirrors SendFile)
    timer.end();

    const int MAX_REMOTE_FILES = 64;
    String remoteNames[MAX_REMOTE_FILES];
    char statusStr[256];

    // let the user know we are reaching out to the server (this re-associates
    // WiFi and can take a few seconds)
    tftDisplay.fillScreen(TFT_WHITE);
    tftMenu.DisplayBanner();
    tftMenu.SetFont(12);
    tftDisplay.drawString("Getting file list...", 0, tftMenu.fontHeight, GFXFF);

    int fileCount = ftpClient.GetFTPServerFileList("/", remoteNames, MAX_REMOTE_FILES, statusStr);
    if (fileCount <= 0)
    {
        // statusStr holds "No files on server" or the error text
        tftMenu.NotImplementedScreen(statusStr);
        deviceState = DISPLAY_MENU;
        timer.begin(SendHeartbeat, TIMER_1HZ);
        return;
    }

    // build a selectable menu from the server file list (same pattern as
    // SelectLocalFile)
    TFTMenu::MenuChoice *filesMenu = (TFTMenu::MenuChoice*)calloc(fileCount, sizeof(TFTMenu::MenuChoice));
    if (!filesMenu)
    {
        tftMenu.NotImplementedScreen("Out of memory building list");
        deviceState = DISPLAY_MENU;
        timer.begin(SendHeartbeat, TIMER_1HZ);
        return;
    }
    for (int i = 0; i < fileCount; i++)
    {
        filesMenu[i].description = remoteNames[i];
        filesMenu[i].result = i;
    }

    int selectedIdx = tftMenu.MenuSelect(12, filesMenu, fileCount, 0);
    char remoteFile[96];
    strncpy(remoteFile, filesMenu[selectedIdx].description.c_str(), sizeof(remoteFile) - 1);
    remoteFile[sizeof(remoteFile) - 1] = '\0';
    free(filesMenu);

    // fetch to the top level of the microSD file system
    char localPath[128];
    sprintf(localPath, "/%s", remoteFile);

    bool getResult = false;
    int attemptCount = 0;
    while (!getResult)
    {
        tftDisplay.fillScreen(TFT_WHITE);
        tftMenu.DisplayBanner();
        tftMenu.SetFont(12);
        int textPosition[2];
        textPosition[0] = 0;
        textPosition[1] = tftMenu.fontHeight;
        sprintf(outStr, "Getting %s...(%d)", remoteFile, attemptCount + 1);
        tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);

        getResult = ftpClient.GetFileFromFTPServer(remoteFile, localPath, statusStr);

        textPosition[1] += tftMenu.fontHeight;
        sprintf(outStr, "Result %s", getResult ? "OK" : "ERROR");
        tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
        textPosition[1] += tftMenu.fontHeight;
        sprintf(outStr, " %s", statusStr);
        tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);

        if (getResult)
        {
            delay(2000);  // hold the success message briefly
            break;
        }

        if (attemptCount > 5)
        {
            textPosition[1] += tftMenu.fontHeight;
            tftDisplay.drawString("Press any button to continue", textPosition[0], textPosition[1], GFXFF);
            tftMenu.WaitForAnyButton();
            break;
        }

        // hold the error long enough to read before retrying (also a backoff)
        #ifdef DEBUG_VERBOSE
        Serial.print("Get attempt ");
        Serial.print(attemptCount + 1);
        Serial.print(" failed: ");
        Serial.println(statusStr);
        #endif
        delay(3000);
        attemptCount++;
    }

    deviceState = DISPLAY_MENU;

    #ifdef DEBUG_VERBOSE
    Serial.println("Restart heartbeat timer in GetFileMenu");
    #endif
    timer.begin(SendHeartbeat, TIMER_1HZ);  // restart heartbeat after file get
}
//
//
//
void ChangeSettingsMenu()
{
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("ChangeSettingsMenu() - displaying settings menu");
    #endif
    int menuCount = 6;
    TFTMenu::MenuChoice settingsMenuChoices[6];
    settingsMenuChoices[0].description = "Delete log files";
    settingsMenuChoices[0].result = DELETE_LOG_FILES;
    settingsMenuChoices[1].description = "Delete all files";
    settingsMenuChoices[1].result = DELETE_ALL_FILES;
    settingsMenuChoices[2].description = "List files";
    settingsMenuChoices[2].result = LIST_FILES;
    settingsMenuChoices[3].description = "FTP Port";
    settingsMenuChoices[3].result = SELECT_FTP_PORT;
    settingsMenuChoices[4].description = "Debug display";
    settingsMenuChoices[4].result = SELECT_DEBUG;
    settingsMenuChoices[5].description = "Exit";
    settingsMenuChoices[5].result = DISPLAY_MENU;
    deviceState = tftMenu.MenuSelect(12, settingsMenuChoices, menuCount, DELETE_LOG_FILES);
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.print("ChangeSettingsMenu() - selected menu item: ");
    Serial.println(deviceState); 
    #endif
}
//
// send a timestamp heartbeat
//
void SendHeartbeat() 
{
  //while (canFD.events() > 0) 
  //{}
  CANFD_message_t msg;
  msg.len = 5;
  msg.id = CANID;
  msg.seq = 1;
  timeStampData.timestamp = millis();
  #ifdef DEBUG_EXTRA_VERBOSE
  Serial.printf("%d\tHeartbeat\n", timeStampData.timestamp);
  #endif
  msg.buf[0] = 1;
  for (uint8_t i = 1; i < msg.len; i++) 
  {
    msg.buf[i] = timeStampData.byteBuffer[i - 1];
  }
  canFD.write(msg);  // write to can3 mailbox 12
}
//
// CAN mailbox message handlers
//
// control box message
//
void CAN_ControlMessage(const CANFD_message_t &msg) 
{
  #ifdef DEBUG_EXTRA_VERBOSE
  Serial.println("CAN_ControlMessage");
  char bufferCat[512];
  sprintf(outStr, "%ld\t0x%02X\tControl\tlen %d\t", millis(),
          (int)msg.id,
          msg.len);
  if (msg.buf[0] == 1) 
  {
    if (msg.buf[1] == 1) 
    {
      sprintf(outStr, "\t%s", "LOG START");
      strcat(outStr, bufferCat);
    } 
    else if (msg.buf[1] == 0) 
    {
      sprintf(bufferCat, "\t%s", "LOG STOP");
      strcat(outStr, bufferCat);
    } 
    else 
    {
      sprintf(bufferCat, "\t%s", "UNKNOWN");
      strcat(outStr, bufferCat);
    }
  } 
  else 
  {
    sprintf(bufferCat, "\t%s", "UNKNOWN");
    strcat(outStr, bufferCat);
  }
  Serial.println(outStr);
  #endif

  if (msg.buf[0] == 1) 
  {
    if (msg.buf[1] == 1) 
    {
      StartLogging();
      logData = true;
    } 
    else if (msg.buf[1] == 0) 
    {
      logData = false;
      StopLogging();
    }
  }
}
//
// A2D node message
//
void CAN_A2DMessage(const CANFD_message_t &msg) 
{
  // copy data to buffer
  memcpy(adData.dataBuffer, msg.buf, AD_SIZE);

  #ifdef DEBUG_EXTRA_VERBOSE
  sprintf(outStr, "%ld\t0x%02X\tAnalog\t%04d\t%04d\t%04d\t%04d\t%04d\t%04d\t%04d\t%04d Digital %d%d%d%d%d%d%d%d",
          adData.dataStructure.timestamp,
          (int)msg.id,
          adData.dataStructure.analogVal[0],
          adData.dataStructure.analogVal[1],
          adData.dataStructure.analogVal[2],
          adData.dataStructure.analogVal[3],
          adData.dataStructure.analogVal[4],
          adData.dataStructure.analogVal[5],
          adData.dataStructure.analogVal[6],
          adData.dataStructure.analogVal[7],
          (adData.dataStructure.digitalVal & 1) == 0 ? 0 : 1,
          (adData.dataStructure.digitalVal & 2) == 0 ? 0 : 1,
          (adData.dataStructure.digitalVal & 4) == 0 ? 0 : 1,
          (adData.dataStructure.digitalVal & 8) == 0 ? 0 : 1,
          (adData.dataStructure.digitalVal & 16) == 0 ? 0 : 1,
          (adData.dataStructure.digitalVal & 32) == 0 ? 0 : 1,
          (adData.dataStructure.digitalVal & 64) == 0 ? 0 : 1,
          (adData.dataStructure.digitalVal & 128) == 0 ? 0 : 1);
  Serial.println(outStr);
  #endif
    if (!logData) 
  {
    return;
  }
  targetFile.write((const uint8_t)msg.id);
  targetFile.write((const uint8_t *)adData.dataBuffer, AD_SIZE);
}
//
// Accelerometer node message
//
void CAN_AccelMessage(const CANFD_message_t &msg) 
{
  // copy data to buffer
  memcpy(imuData.dataBuffer, msg.buf, IMU_SIZE);
  #ifdef DEBUG_EXTRA_VERBOSE
  sprintf(outStr, "%ld\t0x%02X\tIMU\t%0.4f\t%0.4f\t%0.4f",
          imuData.dataStructure.timestamp,
          (int)msg.id,
          imuData.dataStructure.accel[0],
          imuData.dataStructure.accel[1],
          imuData.dataStructure.accel[2]);
  Serial.println(outStr);
  #endif
  if (!logData) 
  {
    return;
  }
  targetFile.write((const uint8_t)msg.id);
  targetFile.write((const uint8_t *)imuData.dataBuffer, IMU_SIZE);
}
//
// GPS node message
//
void CAN_GPSMessage(const CANFD_message_t &msg) 
{
  // copy data to buffer
  memcpy(gpsData.dataBuffer, msg.buf, GPS_SIZE);
  gpsSIV = (int)gpsData.dataStructure.SIV;
  #ifdef DEBUG_EXTRA_VERBOSE
  sprintf(outStr, "%ld\t0x%02X\tGPS\t%lu\t%d/%d/%d %d:%d:%d\tLAT\t%0.7F\tLONG\t%0.7F\tCOURSE\t%0.5F\tSIV\t%d",
          millis(),
          (int)msg.id,
          gpsData.dataStructure.timestamp,
          gpsData.dataStructure.gpsDay,
          gpsData.dataStructure.gpsMonth,
          gpsData.dataStructure.gpsYear,
          gpsData.dataStructure.gpsHour,
          gpsData.dataStructure.gpsMinute,
          gpsData.dataStructure.gpsSecond,
          (float)gpsData.dataStructure.latitude / 10000000.0F,
          (float)gpsData.dataStructure.longitude / 10000000.0F,
          gpsData.dataStructure.course / 100000.0F,
          gpsData.dataStructure.SIV);
  Serial.println(outStr);
  #endif
  // if not logging, check for valid RTC time. if not valid, set it
  if (!logData) 
  {
    ShowGPSStatus(gpsStatus, gpsSIV);
    if (!timeValid)
    {
      // GPS time is UTC. Convert to a time_t, add the configured timezone offset
      // (local = UTC + offset), then set the system clock to local time so SD
      // file timestamps (via DateTimeProvider) are local.
      tmElements_t tm;
      tm.Year   = (gpsData.dataStructure.gpsYear > 99)
                    ? (gpsData.dataStructure.gpsYear - 1970)   // full year e.g. 2025
                    : (gpsData.dataStructure.gpsYear + 30);    // 2-digit year e.g. 25
      tm.Month  = gpsData.dataStructure.gpsMonth;
      tm.Day    = gpsData.dataStructure.gpsDay;
      tm.Hour   = gpsData.dataStructure.gpsHour;
      tm.Minute = gpsData.dataStructure.gpsMinute;
      tm.Second = gpsData.dataStructure.gpsSecond;
      time_t local = makeTime(tm) + UTC_OFFSET_SECONDS;
      setTime(local);
      //#ifdef DEBUG_VERBOSE
      sprintf(outStr, "Update system date/time from GPS (UTC%+ldh): %02d/%02d/%04d %02d:%02d:%02d",
              UTC_OFFSET_SECONDS / 3600,
              month(local), day(local), year(local),
              hour(local), minute(local), second(local));
      Serial.println(outStr);
      //#endif
      timeValid = true;
    }
    return;
  }
  targetFile.write((const uint8_t)msg.id);
  targetFile.write((const uint8_t *)gpsData.dataBuffer, GPS_SIZE);
}
//
// IR tire temp node message
//
void CAN_TireIRMessage(const CANFD_message_t &msg) 
{
  // copy data to buffer
  memcpy(irData.dataBuffer, msg.buf, IR_SIZE);
  #ifdef DEBUG_EXTRA_VERBOSE
  sprintf(outStr, "%lu\t0x%08luX\tIR\t", irData.dataStructure.timestamp, msg.id);
  Serial.print(outStr);
  for (int idx = 0; idx < 8; idx++) 
  {
    Serial.print(irData.dataStructure.tempVal[idx]);
    Serial.print("\t");
  }
  Serial.println();
  #endif
  if (!logData) 
  {
    return;
  }
  targetFile.write((const uint8_t)msg.id);
  targetFile.write((const uint8_t *)irData.dataBuffer, IR_SIZE);
}
//
// Shock position message
//
void CAN_ShockMessage(const CANFD_message_t &msg) 
{
  #ifdef DEBUG_EXTRA_VERBOSE
  Serial.print(millis());
  Serial.print("\tShock message not implemented");
  Serial.println();
  #endif
  if (!logData) 
  {
    return;
  }
}
//
// Wheel speed message
//
void CAN_WheelSpeedMessage(const CANFD_message_t &msg) 
{
  // copy data to buffer
  memcpy(speedData.dataBuffer, msg.buf, SPEED_SIZE);

  #ifdef DEBUG_EXTRA_VERBOSE
  sprintf(outStr, "%lu\t0x%08luX\tSPD\t%05lu", speedData.dataStructure.timestamp,
          msg.id,
          speedData.dataStructure.speedVal);
  Serial.println(outStr);
  #endif
  if (!logData) 
  {
    return;
  }
  targetFile.write((const uint8_t)msg.id);
  targetFile.write((const uint8_t *)speedData.dataBuffer, SPEED_SIZE);
}
//
// RPM message
//
void CAN_RPMMessage(const CANFD_message_t &msg) 
{
  // copy data to buffer
  #ifdef DEBUG_EXTRA_VERBOSE
  Serial.print(millis());
  Serial.print("\tRPM not implemented");
  Serial.println();
  #endif
}
//
// CAN (transfer) message
//
void CAN_CANMessage(const CANFD_message_t &msg) 
{
  #ifdef DEBUG_EXTRA_VERBOSE
  Serial.print(millis());
  Serial.print("\tCANxfer not implemented");
  Serial.println();
  #endif
  if (!logData) 
  {
    return;
  }
}
//
//
//
void ShowGPSStatus(bool gpsActive, int gpsSIV) 
{
  #ifdef DEBUG_VERBOSE
   Serial.printf("ShowGPSStatus() SIV %02d\n", gpsSIV);
  #endif

  if(gpsSIV <= 0)
  {
    digitalWrite(GPS_STATUS_LED, LOW);
  }
  else
  {
    digitalWrite(GPS_STATUS_LED, HIGH); 
  }
}
//
// get next log file index from files on uSD
// assumes all files are at the / level, does not search subfolders
//
int GetNextLogFileIdx() 
{
  int logIdx = 0;
  File dir = SD.open("/");
  while (true) 
  {
    File entry = dir.openNextFile();
    // done
    if (!entry) 
    {
      break;
    }
    // skip subfolders
    else if (entry.isDirectory()) 
    {
      continue;
    }
    String fileName = entry.name();
    // Count this driver's existing logs (or "sdLog" files if no driver chosen)
    // so the next index continues that driver's sequence.
    if (fileName.startsWith(LogFilePrefix()))
    {
      logIdx++;
    }
  }
  return logIdx;
}
///
///
///
void StopLogging()
{
    // flush and close file
    targetFile.flush();
    targetFile.close();
    digitalWrite(RECORDING_LED, LOW);  // turn off recording LED
    sprintf(outStr, "Logging stopped");
    tftMenu.DrawString(outStr, tftMenu.textPosition[0], tftMenu.textPosition[1], GFXFF);
    logData = false;
    #ifdef DEBUG_VERBOSE
    Serial.printf("End logging  at %dms to file %s (%0.3f KB)\n", millis(), sendLogFileName, (float)targetFile.size() / 1000.0);
    #endif
    tftDisplay.fillScreen((uint16_t)~TFT_RED);
    tftDisplay.setRotation(1);
    tftMenu.DisplayBanner();
    tftDisplay.setFreeFont(FSS12);
    int16_t fontHeight = tftDisplay.fontHeight(GFXFF);
    int16_t textPosition[2];
    textPosition[0] = tftMenu.textPosition[0];
    textPosition[1] = tftMenu.fontHeight * 2;
    tftDisplay.setTextColor((uint16_t)~TFT_BLACK, (uint16_t)~TFT_RED);
    sprintf(outStr, "STOP LOGGING at %lu", millis());
    tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
    sprintf(outStr, "%s (%0.3f KB)", sendLogFileName, (float)targetFile.size() / 1000.0);
    tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
    //
    targetFile.close();

    delay(10000);  // wait 10 seconds before returning to menu
    //tftDisplay.drawString("Press any button to continue", textPosition[0], textPosition[1], GFXFF);
    //textPosition[1] += fontHeight;
    //tftMenu.WaitForAnyButton();
    deviceState = DISPLAY_MENU;
}
//
// select file from screen
// list files on screen, use buttons to move selection up, down, and select
// return file name selected
// could add dive into subdirectories, return to parent directory (later)
//
void SelectLocalFile(char *selectedFile) 
{
    root = SD.open("/");
    int fileCount = CountFiles(root);
    #ifdef DEBUG_VERBOSE
    Serial.print("files in / ");
    Serial.println(fileCount);
    Serial.println("Create menu...");
    #endif
    TFTMenu::MenuChoice *filesMenu = (TFTMenu::MenuChoice*)calloc(fileCount, sizeof(TFTMenu::MenuChoice));
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("Load names to menu...");
    #endif
    root = SD.open("/");
    LoadLocalFileMenu(root, filesMenu);
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.println("Run menu...");
    #endif
    int selectedFileIdx = tftMenu.MenuSelect(12, filesMenu, fileCount, 0);
    #ifdef DEBUG_EXTRA_VERBOSE
    Serial.print("Returned index:\t");
    Serial.println(selectedFileIdx);
    Serial.print("Returned selection:\t");
    Serial.println(filesMenu[selectedFileIdx].description);
    #endif  
    int spaceIdx = filesMenu[selectedFileIdx].description.indexOf(".");
    spaceIdx = filesMenu[selectedFileIdx].description.indexOf(" ", spaceIdx);
    strcpy(sendLogFileName, filesMenu[selectedFileIdx].description.substring(0, spaceIdx).c_str());
    strcpy(selectedFile, sendLogFileName);
    free(filesMenu);
    #ifdef DEBUG_VERBOSE
    Serial.print("File name to send:\t>>>>>");
    Serial.print(selectedFile);
    Serial.println("<<<<<");
    #endif
}
//
//
//
int CountFiles(File dir) 
{
  int fileCount = 0;
  while (true) 
  {
    File entry = dir.openNextFile();
    if (!entry) 
    {
      break;
    }
    if (entry.isDirectory()) 
    {
      continue;
    } 
    else 
    {
      fileCount++;
    }
    entry.close();
  }
  return fileCount;
}
//
//
//
void ListFiles(File dir) 
{
  Serial.println("===== list files ======");
  while (true) 
  {
    File entry = dir.openNextFile();
    if (!entry) 
    {
      break;
    }
    if (entry.isDirectory()) 
    {
      Serial.print("DIR : ");
      Serial.println(entry.name());

      continue;
    } 
    else 
    {
      Serial.print("FILE: "); 
      Serial.println(entry.name());
    }
    entry.close();
  }
  Serial.println("=======================");
}
//
//
//
void DeleteLogFiles(File dir)
{
  while (true) 
  {
    File entry = dir.openNextFile();
    if (!entry) 
    {
      break;
    }
    String fileName = entry.name();
    if (entry.isDirectory()) 
    {
      Serial.print("DIR (skip) : ");
      Serial.println(entry.name());
      continue;
    } 
    else if((fileName.endsWith(".yl5")) ||
            (fileName.endsWith(".ylg")))
    {
      Serial.print("DELETE FILE: "); 
      Serial.println(fileName);
      SD.remove(entry.name());
    }
    entry.close();
  }
  Serial.println("===== remaining files =====");
  dir.rewindDirectory();
  ListFiles(dir);
}
//
//
//
void DeleteAllFiles(File dir)
{
  while (true) 
  {
    File entry = dir.openNextFile();
    if (!entry) 
    {
      break;
    }
    String fileName = entry.name();
    if (entry.isDirectory()) 
    {
      Serial.print("DIR (skip) : ");
      Serial.println(entry.name());
      continue;
    } 
    else
    {
      Serial.print("DELETE FILE: "); 
      Serial.println(fileName);
      SD.remove(entry.name());
    }
    entry.close();
  }
  Serial.println("===== remaining files =====");
  dir.rewindDirectory();
  ListFiles(dir);
}
//
//
//
void LoadLocalFileMenu(File dir, TFTMenu::MenuChoice *filesMenu) 
{
  int fileIdx = 0;
  while (true) 
  {
    File entry = dir.openNextFile();
    if (!entry) 
    {
      break;
    }
    if (entry.isDirectory()) 
    {
      continue;
    } 
    else 
    {
      sprintf(outStr, "%s %0.3fKB", entry.name(), (float)entry.size() / 1000.0);
      filesMenu[fileIdx].description = outStr;  //entry.name();
      filesMenu[fileIdx].result = fileIdx;
      #ifdef DEBUG_VERBOSE
      Serial.print("Menu entry ");
      Serial.print(fileIdx);
      Serial.print(" ");
      Serial.print(filesMenu[fileIdx].description);
      Serial.print(" ");
      Serial.println(filesMenu[fileIdx].description);
      #endif
      fileIdx++;
    }
    entry.close();
  }
}
//
// select FTP port
//
void SelectFtpPort()
{
    TFTMenu::MenuChoice ftpPortMenuChoices[3];
    ftpPortMenuChoices[0].description = "Port 21";
    ftpPortMenuChoices[0].result = 21;  
    ftpPortMenuChoices[1].description = "Port 2121";
    ftpPortMenuChoices[1].result = 2121;
    ftpPortMenuChoices[2].description = "Port 2021";
    ftpPortMenuChoices[2].result = 2021;
    int selectedPort = tftMenu.MenuSelect(12, ftpPortMenuChoices, 3, 0);
    if (selectedPort != ftpClient.ftpListenerPort)
    {
        ftpClient.ftpListenerPort = selectedPort;
        // persist the change so it survives a power cycle
        SaveFtpPortToConfig("/config.ini", selectedPort);
    }
}
//
// turn the debug display (WiFi/FTP credentials on the Sending screen) on or off
//
void SelectDebugDisplay()
{
    TFTMenu::MenuChoice debugMenuChoices[2];
    debugMenuChoices[0].description = "Debug display OFF";
    debugMenuChoices[0].result = 0;
    debugMenuChoices[1].description = "Debug display ON";
    debugMenuChoices[1].result = 1;
    debugDisplay = (tftMenu.MenuSelect(12, debugMenuChoices, 2, debugDisplay ? 1 : 0) != 0);
    #ifdef DEBUG_VERBOSE
    Serial.print("Debug display ");
    Serial.println(debugDisplay ? "ON" : "OFF");
    #endif
}
//
// list SD files on the TFT as a menu; the selection is informational only -
// any choice just returns to the settings menu
//
void ListFilesMenu()
{
    root = SD.open("/");
    int fileCount = CountFiles(root);
    if (fileCount <= 0)
    {
        tftMenu.NotImplementedScreen("No files on SD card");
        deviceState = CHANGE_SETTINGS;
        return;
    }

    TFTMenu::MenuChoice *filesMenu = (TFTMenu::MenuChoice*)calloc(fileCount, sizeof(TFTMenu::MenuChoice));
    if (!filesMenu)
    {
        tftMenu.NotImplementedScreen("Out of memory building list");
        deviceState = CHANGE_SETTINGS;
        return;
    }
    root = SD.open("/");
    LoadLocalFileMenu(root, filesMenu);

    tftMenu.MenuSelect(12, filesMenu, fileCount, 0);  // selection ignored
    free(filesMenu);

    deviceState = CHANGE_SETTINGS;  // back to the settings menu
}
//
//
//
void DateTimeProvider(uint16_t* date, uint16_t* time) 
{
  #ifdef DEBUG_VERBOSE
  Serial.printf("DateTimeProvider() - %02d/%02d/%04d %02d:%02d:%02d\n", month(), day(), year(), hour(), minute(), second());
  #endif
  // Return canonical FAT date and time format
  *date = FAT_DATE(year(), month(), day());
  *time = FAT_TIME(hour(), minute(), second());
}