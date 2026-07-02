## YamuraLog Hub
Control for YamuraLog sensor network. Start/Stop logging, save log file to microSD, handle FTP communications with data viewer

## Hardware
- Teensy 4.0 (could also run on a Teensy 4.1 and use internal micorSD card)
- Waveshare SN65HVD230 CAN tranciever
- Hosyond 4" 320x480 display (with built in microSD card slot)
- Adafruit Airlift

## Required Libraries
This is a Visual Code/Platform IO project, I don't recommend using Arduino IDE for this, although the code did start there.
Install these libraries:

- `WiFiNINA - Adafruit fork`
- `SPI`
- 'TFT_eSPI'
- 'FlexCan_T4'
- 'ArduinoJson'

## Usage

1. Open `WiFiSecrets.h`.
2. Replace the WiFi credentials:
   - `YOUR_SSID`
   - `YOUR_WIFI_PASSWORD`
3. Replace the FTP server details:
   - `FTP_SERVER`
   - `FTP_USER`
   - `FTP_PASS`
   - 'FTP_PATH'
4. Adjust pin definitions in main.h as needed:
5. Build and upload to Teensy 4.0.
6. For full debug messages, uncomment #define DEBUG_EXTRA_VERBOSE. Use #define DEBUG_VERBOSE for more serious(?) messages
7. Open Serial Monitor at `115200` baud.

## Notes

- To log anything, you need at least 1 YamuraLog node (GPS, IMU, A/D), appropriate CAN cables and terminators.

## PlatformIO

A `platformio.ini` file is included for Teensy 4.0 builds.