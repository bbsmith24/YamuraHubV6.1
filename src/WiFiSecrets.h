#ifndef WIFI_SECRETS_H
#define WIFI_SECRETS_H

// Compiled-in defaults for WiFi + FTP.
//
// These are used as-is unless a /config.ini file is present on the SD card, in
// which case matching fields there override these values at boot. See Config.h
// and LoadConfigFromSD(). Keeping credentials in /config.ini (on the card) lets
// you keep real secrets out of committed source.

// WiFi credentials
#define DEFAULT_SSID       "YamuraLog"
#define DEFAULT_PASSWORD   "PeteAron"

// FTP configuration
#define DEFAULT_FTP_SERVER "192.168.4.2"
#define DEFAULT_FTP_USER   "ftpuser"
#define DEFAULT_FTP_PASS   "ZoeyDora48375"
#define DEFAULT_FTP_PATH   "/"
#define DEFAULT_FTP_PORT   21

#endif // WIFI_SECRETS_H
