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
//#define DEBUG_VERBOSE
//#define DEBUG_EXTRA_VERBOSE

#include "main.h"        // Contains pin definitions

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
bool timeValid = true;

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
    Serial.println("\n====================================================");
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
    #endif
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
//   // Connect to WiFi
//   Serial.print("Connecting to WiFi network '");
//   Serial.print(SSID);
//   Serial.print("'... ");
//
//    int status = WiFi.begin(SSID, PASSWORD);
//    if (status != WL_CONNECTED) {
//        Serial.println("FAILED");
//        Serial.print("ERROR: WiFi connection failed, status = ");
//        Serial.println(status);
//        while (true) {
//            delay(1000);
//        }
//    }
//
//    Serial.println("OK");
//    Serial.print("Connected to: ");
//    Serial.println(SSID);
//    Serial.print("IP address: ");
//    Serial.println(WiFi.localIP());
//    Serial.println();
//
//    // Initialize SD card
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
    #endif
    // Initialize TFT display
    tftMenu.TFTInitialization();

//    // Connect to FTP server
//    Serial.print("Connecting to FTP server '");
//    Serial.print(FTP_SERVER);
//    Serial.println("'...");
//
//    if (!ftpClient.FTPConnect(FTP_SERVER, 21, FTP_USER, FTP_PASS)) {
//        Serial.println("ERROR: FTP connection failed!");
//        while (true) {
//            delay(1000);
//        }
//    }
//
//    Serial.println("\n--- Testing FTP Operations ---\n");
//
//    // Test 1: List remote directory
//    Serial.println("[1] Listing remote directory...");
//    if (!ftpClient.ListFTPServerDirectory(FTP_PATH)) 
//    {
//        Serial.println("WARNING: Remote directory listing failed");
//    }
//    Serial.println();
//
//    // Test 1a: List local directory
//    Serial.println("[1a] Listing local directory...");
//     if (ftpClient.ListLocalDirectory("/"))
//     {
//        Serial.println("WARNING: Local directory listing failed");
//     }
//    Serial.println();
//
//    // Test 2: Upload file
//    Serial.println("[2] Uploading file...");
//    size_t contentSize = strlen(UPLOAD_FILE_CONTENT);
//    if (!ftpClient.UploadFileToFTPServer(UPLOAD_FILE_NAME, 
//                              reinterpret_cast<const uint8_t*>(UPLOAD_FILE_CONTENT), 
//                              contentSize)) {
//        Serial.println("WARNING: File upload failed");
//    } else {
//        Serial.println("File upload successful");
//    }
//    Serial.println();
//
//    // Test 3: Download file
//    Serial.println("[3] Downloading file to SD card...");
//    if (!ftpClient.DownloadFileFromFTPServerToSD(DOWNLOAD_FILE_NAME, DOWNLOAD_FILE_PATH)) {
//        Serial.println("WARNING: File download failed");
//    } else {
//        Serial.println("File download complete and stored to SD card");
//    }
//    Serial.println();
//
//    // test 4: upload file from SD card (if SD card support is implemented and available)
//    Serial.println("[4] Uploading file from SD card...");    
//    ftpClient.UploadFileFromSDtoFTPServer(SD_UPLOAD_FILE_NAME, SD_UPLOAD_FILE_PATH);
//    Serial.println("SD card file upload attempted (check FTP server for result)");  
//
//
//    // Disconnect from FTP server
//    Serial.println("--- Disconnecting from FTP server ---");
//    ftpClient.FTPDisconnect();

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
    // progress bar for file transfer
    //xferProgressBar.CreateProgressBar(2, screenSize[1] - 60, screenSize[0] - 2, screenSize[1] - 40, TFT_WHITE, TFT_BLUE, TFT_BLACK, screenRotation, true, &tftDisplay);
    // Serial.println("\n==========================================");
    // Serial.println(  "* FTP demo complete                      *");
    // Serial.println(  "* CAN FD setup complete                  *");
    // Serial.println(  "* Entering idle loop.                    *");
    // Serial.println(  "==========================================\n");
}
///
///
///
void loop() 
{
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
      deviceState = DISPLAY_MENU;
      break;
    default:
      break;
  }
}
// //
// // Set the TFT screen rotation and update global variables accordingly
// //
// void SetScreenRotation(int rotVal) 
// {
//     Serial.println("SetScreenRotation() - Setting screen rotation...");
//     tftDisplay.setRotation(rotVal);
//     screenRotation = rotVal;
//     if ((rotVal == 0) || (rotVal == 2)) 
//     {
//         screenSize[0] = TFT_WIDTH;
//         screenSize[1] = TFT_HEIGHT;
//     } 
//     else if ((rotVal == 1) || (rotVal == 3)) 
//     {
//         screenSize[0] = TFT_HEIGHT;
//         screenSize[1] = TFT_WIDTH;
//     }
//     sprintf(outStr, "Rotation\t%d", rotVal);
//     Serial.println(outStr);
//     sprintf(outStr, "Screen size\tW %d\tH %d", screenSize[0], screenSize[1]);
//     Serial.println(outStr);
// }
//
// draw the Yamura banner at bottom of screen
//
// void YamuraBanner() 
// {
//     Serial.println("YamuraBanner() - drawing banner on TFT display");
//     tftDisplay.setTextColor((uint16_t)~TFT_BLACK, (uint16_t)~TFT_YELLOW);
//     SetFont(9);
//     int xPos = tftDisplay.width() / 2;
//     int yPos = tftDisplay.height() - fontHeight / 2;
//     tftDisplay.setTextDatum(BC_DATUM);
//     tftDisplay.drawString("Yamura Motors LLC Data Logger", xPos, yPos, GFXFF);  // Print the font name onto the TFT screen
//     tftDisplay.setTextDatum(TL_DATUM);
//     tftDisplay.setFreeFont(FSS12);
//     fontHeight = tftDisplay.fontHeight(GFXFF);
//     tftDisplay.setTextColor(TFT_BLACK, TFT_WHITE);
// }
//
// set font for TFT display, update fontHeight used for vertical stepdown by line
//
// void SetFont(int fontSize)
// {
//   switch (fontSize) 
//   {
//     case 9:
//       tftDisplay.setFreeFont(FSS9);
//       break;
//     case 12:
//       tftDisplay.setFreeFont(FSS12);
//       break;
//     case 18:
//       tftDisplay.setFreeFont(FSS18);
//       break;
//     case 24:
//       tftDisplay.setFreeFont(FSS24);  // was FSS18
//       break;
//     default:
//       tftDisplay.setFreeFont(FSS12);
//       break;
//   }
//   tftDisplay.setTextDatum(TL_DATUM);
//   fontHeight = tftDisplay.fontHeight(GFXFF);
//   fontWidth = tftDisplay.textWidth("X");
// }
//
//
//
void MainMenu()
{
    #ifdef DEBUG_VERBOSE
    Serial.println("MainMenu() - displaying main menu");
    #endif
    int menuCount = 5;
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
    tftMenu.NotImplementedScreen("Select driver not implemented");
    deviceState = DISPLAY_MENU;
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
    sprintf(sdLogFileName, "/sdLog%03d.yl5", fileIdx);
    #ifdef DEBUG_VERBOSE
    Serial.printf("Opening log file %s for writing\n", sdLogFileName);
    #endif

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
    sprintf(outStr, "%s", sdLogFileName);
    tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
    //
    //targetFile = SD.open(sdLogFileName, O_WRITE | O_CREAT);
    targetFile = SD.open(sdLogFileName, FILE_WRITE);
    if (!targetFile || targetFile.isDirectory())
    {
        #ifdef DEBUG_VERBOSE
        Serial.printf("Failed to open file %s for writing\n", sdLogFileName);
        #endif
        sprintf(outStr, "Logging Failed %s", sdLogFileName);
        tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
        textPosition[1] += fontHeight;
        logData = false;
        return;
    }
    #ifdef DEBUG_VERBOSE
    Serial.printf("Start logging to %s at %dms\n", sdLogFileName, millis());
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
//
void SendFileMenu()
{
    #ifdef DEBUG_VERBOSE
    Serial.println("Stop heartbeat timer to send file to FTP server");
    #endif
    timer.end();  // stop heartbeat before file upload to FTP server
    char fileNameToSend[64];
    char fileNameToSendSize[64];
    // get the file name to send from the user through the TFT menu
    SelectLocalFile(fileNameToSend);
    // clean up the file name - split on space between name and size
    String fileNameStrToSend = fileNameToSend;
    int spaceIdx = fileNameStrToSend.indexOf(".");
    spaceIdx = fileNameStrToSend.indexOf(" ", spaceIdx);
    strcpy(fileNameToSend, fileNameStrToSend.substring(0, spaceIdx).c_str());
    strcpy(fileNameToSendSize, fileNameStrToSend.substring(spaceIdx).c_str());
    #ifdef DEBUG_VERBOSE
    sprintf(outStr, "%s >>>> %s", fileNameToSend, fileNameToSendSize);
    Serial.println(outStr);
    #endif
    // display sending file message on TFT
    tftDisplay.fillScreen(TFT_WHITE);
    tftMenu.DisplayBanner();
    tftMenu.SetFont(12);
    int textPosition[2];
    textPosition[0] = tftMenu.textPosition[0];
    textPosition[1] = tftMenu.fontHeight;
    bool sendResult = ftpClient.UploadFileFromSDtoFTPServer(fileNameToSend, fileNameToSend);

    tftDisplay.fillScreen(TFT_WHITE);
    tftMenu.DisplayBanner ();
    tftMenu.SetFont(12);
    textPosition[1] += tftMenu.fontHeight;
    sprintf(outStr, "Sent %s with result %s", fileNameToSend, sendResult ? "OK" : "ERROR");
    tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += tftMenu.fontHeight;
    tftDisplay.drawString("Press any button to continue", textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += tftMenu.fontHeight;

    tftMenu.WaitForAnyButton();
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
    Serial.println("GetFileMenu() - displaying get file menu");
    #endif
    tftMenu.NotImplementedScreen("Get file not implemented");
    deviceState = DISPLAY_MENU;
}
//
//
//
void ChangeSettingsMenu()
{
    #ifdef DEBUG_VERBOSE
    Serial.println("ChangeSettingsMenu() - displaying change settings menu");
    #endif
    tftMenu.NotImplementedScreen("Change settings not implemented");
    deviceState = DISPLAY_MENU;
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
  #ifdef DEBUG_VERBOSE
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
    // if (!timeValid) 
    // {
    //   //setTime(gpsData.dataStructure.gpsHour, gpsData.dataStructure.gpsMinute, gpsData.dataStructure.gpsSecond, gpsData.dataStructure.gpsDay, gpsData.dataStructure.gpsMonth, gpsData.dataStructure.gpsYear);
    //   #ifdef DEBUG_VERBOSE
    //   sprintf(outStr, "Update system date/time from GPS: %02d/%02d/%04d %02d:%02d:%02d UTC", month(),
    //           day(),
    //           year(),
    //           hour(),
    //           minute(),
    //           second());
    //   Serial.println(outStr);
    //   #endif
    // }
    timeValid = true;
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
    // char messageStr[32];
    //#ifdef DEBUG_VERBOSE
    // Serial.println("ShowGPSStatus() - displaying GPS status on TFT display");
    //#endif
    // sprintf(messageStr, "SIV %02d", gpsSIV);
  
    // if (gpsSIV == 0) 
    // {
    //   tftDisplay.setTextColor((uint16_t)~TFT_BLACK, (uint16_t)~TFT_RED);
    // } 
    // else if (gpsSIV < 4) 
    // {
    //   tftDisplay.setTextColor((uint16_t)~TFT_BLACK, (uint16_t)~TFT_ORANGE);
    // } 
    // else 
    // {
    //   tftDisplay.setTextColor((uint16_t)~TFT_BLACK, (uint16_t)~TFT_GREEN);
    // }
    // //tftMenu.SetFont(9);
    // int xPos = tftMenu.screenSize[0];
    // int yPos = tftMenu.screenSize[1] - tftMenu.fontHeight / 2;
    // //tftMenu.DrawString(messageStr, xPos, yPos, GFXFF);
    
    // tftDisplay.setTextDatum(BR_DATUM);
    // tftDisplay.drawString(messageStr, xPos, yPos, GFXFF);
    // tftDisplay.setTextDatum(TL_DATUM);
    // tftDisplay.setFreeFont(FSS12);
    // //tftMenu.fontHeight = tftDisplay.fontHeight(GFXFF);
    // tftDisplay.setTextColor(TFT_BLACK, TFT_WHITE);
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
    if (fileName.startsWith("sdLog", 0)) 
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
    sprintf(outStr, "Logging stopped");
    tftMenu.DrawString(outStr, tftMenu.textPosition[0], tftMenu.textPosition[1], GFXFF);
    logData = false;
    #ifdef DEBUG_VERBOSE
    Serial.printf("End logging  at %dms to file %s (%0.3f KB)\n", millis(), sdLogFileName, (float)targetFile.size() / 1000.0);
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
    sprintf(outStr, "%s (%0.3f KB)", sdLogFileName, (float)targetFile.size() / 1000.0);
    tftDisplay.drawString(outStr, textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
    //
    targetFile.close();

    tftDisplay.drawString("Press any button to continue", textPosition[0], textPosition[1], GFXFF);
    textPosition[1] += fontHeight;
    tftMenu.WaitForAnyButton();
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
    File root = SD.open("/");
    int fileCount = CountFiles(root);
    #ifdef DEBUG_VERBOSE
    Serial.print("files in / ");
    Serial.println(fileCount);
    Serial.println("Create menu...");
    #endif
    TFTMenu::MenuChoice *filesMenu = (TFTMenu::MenuChoice*)calloc(fileCount, sizeof(TFTMenu::MenuChoice));
    #ifdef DEBUG_VERBOSE
    Serial.println("Load names to menu...");
    #endif
    root = SD.open("/");
    LoadLocalFileMenu(root, filesMenu);
    Serial.println("Run menu...");
    int selectedFileIdx = tftMenu.MenuSelect(12, filesMenu, fileCount, 0);
    #ifdef DEBUG_VERBOSE
    Serial.print("Returned index:\t");
    Serial.println(selectedFileIdx);
    Serial.print("Returned selection:\t");
    Serial.println(filesMenu[selectedFileIdx].description);
    #endif  
    int spaceIdx = filesMenu[selectedFileIdx].description.indexOf(".");
    spaceIdx = filesMenu[selectedFileIdx].description.indexOf(" ", spaceIdx);
    strcpy(selectedFile, filesMenu[selectedFileIdx].description.substring(0, spaceIdx).c_str());
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