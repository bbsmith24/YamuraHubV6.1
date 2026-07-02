#include <Arduino.h>
//#include "WiFiSecrets.h"  // Contains SSID and PASSWORD definitions
#include <SPI.h>
#include <SD.h>
#include <WiFiNINA.h>
#include "FTPClient.h"
#include <TFT_eSPI.h>     // https://github.com/Bodmer/TFT_eSPI Graphics and font library for ST7735 driver chip
#include "Free_Fonts.h"  // Include the header file attached to this sketch
#include "TFTMenu.h"    // Menuing system for TFT display and buttons
#include <FlexCAN_T4.h>  // CAN-FD https://github.com/tonton81/FlexCAN_T4

// WiFi SPI pin configuration for Teensy 4.1 with WiFiNINA shield
#define WIFI_SPI_MOSI_PIN  11  // SPI MOSI used by WiFi module
#define WIFI_SPI_MISO_PIN  12  // SPI MISO used by WiFi module
#define WIFI_SPI_SCK_PIN   13  // SPI SCK used by WiFi module
#define SPIWIFI_SS          5  // Airlift CS pin on Teensy (reverted to working pin)
#define ESP32_RESETN        6  // Airlift reset pin on Teensy
#define SPIWIFI_ACK         9  // Airlift busy/IRQ pin on Teensy
#define ESP32_GPIO0        -1  // GPIO0 pin not used in this wiring
#define SD_CS               3  // SD card CS pin

// TFT display pin configuration for Teensy 4.0 with ST7735 display
#define TFT_MOSI 11   // and microSD MOSI
#define TFT_MISO 12   // and microSD MISO
#define TFT_SCLK 13   // and microSD SCLK
#define TFT_CS   15   // TFT chip select control pin
#define TFT_DC    2   // Data Command control pin
#define TFT_RST   4   // Reset pin (could connect to RST pin)

// Button pin definitions
#define BUTTON_0 16
#define BUTTON_1 17
#define BUTTON_2 18

// main menu
#define DISPLAY_MENU 0
#define SELECT_DRIVER 1
#define START_LOGGING 2
#define SEND_FILE 3
#define GET_FILE 4
#define CHANGE_SETTINGS 5
#define DELETE_LOG_FILES 6
#define DELETE_ALL_FILES 7
#define LIST_FILES 8
#define SEND_LAST_FILE 9

// current state of logger
int deviceState = 0;
bool logData = false;
char sdLogFileName[20];
bool gpsStatus = false;
int gpsSIV = 0;
char outStr[512];
unsigned long currentMillis = 0;
// CAN setup
#define ARB_BAUD 500000
#define FD_BAUD  500000
//
// sample freq defines (value is sample rate in ms)
//
#define SAMPLE_1000Hz    1
#define SAMPLE_500Hz     2
#define SAMPLE_250Hz     4
#define SAMPLE_200Hz     5
#define SAMPLE_125Hz     8
#define SAMPLE_100Hz    10
#define SAMPLE_50Hz     20
#define SAMPLE_25Hz     40
#define SAMPLE_20Hz     50
#define SAMPLE_10Hz    100
#define SAMPLE_1Hz    1000

#define TIMER_1000000HZ 1
#define TIMER_100000HZ 10
#define TIMER_10000HZ 100
#define TIMER_1000HZ 1000
#define TIMER_100HZ 10000
#define TIMER_10HZ 100000
#define TIMER_1HZ 1000000
//
// timestamp structure (used by all)
//
union TimeStamp
{
  uint32_t timestamp;
  uint8_t byteBuffer[4];
} timeStampData;

//
// 0x10 HUB structures
// no structure, sends current millis value as heartbeat at regular intervals

//
// 0x20 CONTROL structures
// no structure, sends 1 to start, 0 to stop logging

//
// 0x30 A2D structures
//
#define AD_SIZE 21
struct __attribute__((packed)) AD_v2_Structure
{
  unsigned long timestamp;   // 1*4 =  4 bytes
  uint8_t digitalVal;        // 1 =    1 bytes (max 8 digital channels, 1 bit/channel)
  uint16_t analogVal[8];     // 8*2 = 16 bytes (max 8 analog channels)
                             // ------------
                             //       21 total bytes
};
union AD_v2_Data
{
  AD_v2_Structure dataStructure;
  uint8_t dataBuffer[AD_SIZE];
} adData;
//
// IMU structures
//
#define IMU_SIZE 16
struct __attribute__((packed)) IMU_Structure
{
  unsigned long timestamp;   // 1*4 =  4 bytes
  float accel[3];            // 3*4 = 12 bytes
                             // --------------
                             // 16 total bytes
};
union IMU_Data
{
  IMU_Structure dataStructure;
  uint8_t dataBuffer[IMU_SIZE];
} imuData;

//
// GPS structures
//
#define GPS_SIZE 28
struct __attribute__((packed)) GPS_Structure
{
  uint32_t timestamp;        // 4 bytes
  uint16_t gpsYear;          // 2
  uint8_t gpsMonth;          // 1
  uint8_t gpsDay;            // 1
  uint8_t gpsHour;           // 1
  uint8_t gpsMinute;         // 1
  uint8_t gpsSecond;         // 1
  int32_t latitude;          // 4
  int32_t longitude;         // 4
  int32_t course;            // 4
  int32_t speed;             // 4
  uint8_t SIV;               // 1
};
union GPS_Data
{
  GPS_Structure dataStructure;
  uint8_t dataBuffer[GPS_SIZE];
} gpsData;

//
// Tire temp structures
//
#define IR_SIZE 64
struct __attribute__((packed)) IR_Structure
{
  unsigned long timestamp;   //  1*4 =  4 bytes
  uint8_t tempVal[60];       // 60*1 = 60 bytes
                             // ------------
                             // 64 total bytes
};
union IR_Data
{
  IR_Structure dataStructure;
  uint8_t dataBuffer[IR_SIZE];
} irData;

//
// Shock structures
// not defined yet

//
// WheelSpd structures
//
#define SPEED_SIZE 8
struct __attribute__((packed)) SPEED_Structure
{
  unsigned long timestamp;   // 1*4 =  4 bytes
  unsigned long speedVal;    // 1*4 =  4 bytes
                             // ------------
                             //        8 total bytes
};
union SPEED_Data
{
  SPEED_Structure dataStructure;
  uint8_t dataBuffer[SPEED_SIZE];
} speedData;

IntervalTimer timer;
IntervalTimer timer2;
//
// 0x90 RPM structures
// not defined yet

//
// 0xA0 CAN structures
// not defined yet

//#endif
File root;
// function prototypes
void MainMenu();
void SelectDriverMenu();
void StartLogging();
void SelectLogFile();
int CountFiles(File dir);
void LoadLocalFileMenu(File dir, TFTMenu::MenuChoice *filesMenu);
void StopLogging();
void SendFileMenu();
void GetFileMenu();
void ChangeSettingsMenu();
void SendHeartbeat();
void CAN_ControlMessage(const CANFD_message_t &msg);
void CAN_A2DMessage(const CANFD_message_t &msg);
void CAN_AccelMessage(const CANFD_message_t &msg);
void CAN_GPSMessage(const CANFD_message_t &msg);
void CAN_TireIRMessage(const CANFD_message_t &msg);
void CAN_ShockMessage(const CANFD_message_t &msg);
void CAN_WheelSpeedMessage(const CANFD_message_t &msg);
void CAN_RPMMessage(const CANFD_message_t &msg);
void CAN_CANMessage(const CANFD_message_t &msg);
void ShowGPSStatus(bool gpsActive, int gpsSIV);
int  GetNextLogFileIdx();
void SelectLocalFile(char *selectedFile);
void SendFileMenu();
void ListFiles(File dir);
void DeleteAllFiles(File dir);
void DeleteLogFiles(File dir);
void SendFile(char* fileNameToSend);