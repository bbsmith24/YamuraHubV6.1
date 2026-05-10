// Compatibility header for WiFiNINA with Teensy
// PinStatus was added in Arduino 1.8.13 but may not be available in Teensy core

#ifndef PIN_STATUS_COMPAT_H
#define PIN_STATUS_COMPAT_H

// Define PinStatus if not already defined
#ifndef PinStatus
  #define PinStatus uint8_t
  #define LOW 0
  #define HIGH 1
#endif

// Define missing Arduino_SpiNINA constants for Teensy
#ifndef PINS_COUNT
  #define PINS_COUNT 40  // Teensy 4.0 has many GPIO pins, this is a conservative estimate
#endif

#ifndef NINA_GPIO0
  #define NINA_GPIO0 0
#endif

// SPI Speed configuration for WiFiNINA (typical values: 4MHz to 50MHz)
#ifndef SPIWIFI_SPEED
  #define SPIWIFI_SPEED 24000000  // 24 MHz SPI speed (safe for most WiFi modules)
#endif

#endif // PIN_STATUS_COMPAT_H
