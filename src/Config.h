#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Runtime configuration for the YamuraLog hub.
//
// The effective WiFi/FTP settings below are seeded from the compiled defaults in
// WiFiSecrets.h and then, if a /config.ini file exists on the SD card, overridden
// per-field by that file at boot. The [drivers] section of the same file fills
// the driver list used by SelectDriverMenu.
//
// config.ini format (sections and keys are case-insensitive):
//
//   [wifi]
//   ssid=YamuraLog
//   password=PeteAron
//
//   [ftp]
//   server=192.168.4.2
//   user=ftpuser
//   pass=ZoeyDora48375
//   path=/
//   port=21
//
//   [time]
//   utc_offset=-4      ; local = UTC + offset, in hours (EDT -4, PST -8, IST 5.5)
//
//   [drivers]
//   Pete
//   Aron
//   Zoey
//
// Lines starting with '#' or ';' are comments. Under [drivers], each non-empty
// line is one driver name.

#define CFG_STR_LEN     64   // capacity of each WiFi/FTP field (incl. null)
#define CFG_MAX_DRIVERS 24   // maximum drivers read from config.ini
#define CFG_DRIVER_LEN  32   // capacity of each driver name (incl. null)

// Effective settings (defaults from WiFiSecrets.h, overridden by config.ini).
// Named to match prior use in FTPClient.cpp.
extern char SSID[CFG_STR_LEN];
extern char PASSWORD[CFG_STR_LEN];
extern char FTP_SERVER[CFG_STR_LEN];
extern char FTP_USER[CFG_STR_LEN];
extern char FTP_PASS[CFG_STR_LEN];
extern char FTP_PATH[CFG_STR_LEN];
extern int  FTP_PORT;

// Offset of local time from UTC, in SECONDS (local = UTC + offset). GPS time is
// UTC; this is added when setting the system clock so SD file timestamps are
// local. Default 0. Set via [time] utc_offset (hours) in config.ini.
extern long UTC_OFFSET_SECONDS;

// Driver list loaded from the [drivers] section, and the current selection.
extern char driverNames[CFG_MAX_DRIVERS][CFG_DRIVER_LEN];
extern int  driverCount;
extern char currentDriver[CFG_DRIVER_LEN];

// Load /config.ini (or another path) from the SD card, overriding the defaults
// above and populating the driver list. Must be called after SD.begin().
// Returns true if the file was found and read, false if it was absent/unreadable
// (in which case the compiled defaults remain in effect).
bool LoadConfigFromSD(const char* path);

// Persist the FTP port back to config.ini, creating the file/[ftp] section if
// needed and preserving all other lines (including comments). Also updates the
// in-memory FTP_PORT. Returns true on success.
bool SaveFtpPortToConfig(const char* path, int port);
// Persist the selected driver back to config.ini, creating the file/[selected driver] section if
// needed and preserving all other lines (including comments). Also updates the
// in-memory currentDriver. Returns true on success.
bool SaveSelectedDriverToConfig(const char* path, char* driver);
#endif // CONFIG_H
