#ifndef WIFI_SECRETS_H
#define WIFI_SECRETS_H

// Compiled-in defaults for WiFi + FTP.
//
// These are PLACEHOLDERS - put your real credentials in a /config.ini on the SD
// card, whose matching fields override these at boot (see Config.h /
// LoadConfigFromSD). This keeps real secrets out of committed source. Without a
// config.ini the device will not connect until you replace these placeholders.

// WiFi credentials
#define DEFAULT_SSID       "YOUR_SSID"
#define DEFAULT_PASSWORD   "YOUR_WIFI_PASSWORD"

// FTP configuration
#define DEFAULT_FTP_SERVER "192.168.4.2"
#define DEFAULT_FTP_USER   "YOUR_FTP_USER"
#define DEFAULT_FTP_PASS   "YOUR_FTP_PASSWORD"
#define DEFAULT_FTP_PATH   "/"
#define DEFAULT_FTP_PORT   21

#endif // WIFI_SECRETS_H
