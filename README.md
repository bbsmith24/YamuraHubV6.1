# FTPClientTeensy

This project is a Teensy 4.1 FTP client example using the Adafruit AirLift WiFi shield.
It connects to a WiFi network, opens an FTP control session, lists a directory, and downloads a file.

## Hardware

- Teensy 4.1
- Adafruit AirLift shield (WINC1500-based)
- USB cable for programming
- Optional: serial monitor for output

## Required Libraries

Install these libraries in Arduino IDE or PlatformIO:

- `WiFiNINA`
- `SPI`

If you use Arduino IDE, install the `WiFiNINA` library from Library Manager.

## Usage

1. Open `FTPClientTeensy.ino`.
2. Replace the WiFi credentials:
   - `YOUR_SSID`
   - `YOUR_WIFI_PASSWORD`
3. Replace the FTP server details:
   - `ftp.example.com`
   - `anonymous` / `guest`
   - `readme.txt`
4. Adjust AirLift pin definitions if needed:
   - `WLAN_CS`, `WLAN_IRQ`, `WLAN_RST`
5. Build and upload to Teensy 4.1.
6. Open Serial Monitor at `115200` baud.

## Notes

- This example uses passive FTP (`PASV`).
- Output prints directory listings and file contents to Serial.
- Change `downloadFileName` to download a different file from the FTP server.

## PlatformIO

A `platformio.ini` file is included for Teensy 4.1 builds.

### Build command

```sh
platformio run
```

### Upload command

```sh
platformio run --target upload
```
